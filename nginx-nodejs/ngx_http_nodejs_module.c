/*
 * Copyright (C) Marat Abdullin
 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <ngx_string.h>
#include <ngx_array.h>

#include <libnodejs.h>
#include <string.h>

#define DDEBUG 1
#include "ddebug.h"


typedef struct {
    ngx_str_t                  name;
    ngx_http_complex_value_t   value;
} ngx_http_nodejs_param_t;


typedef struct {
    ngx_str_t                  js;
    unsigned                   fulljs:1;
    ngx_array_t               *params;       /* ngx_http_nodejs_param_t */
    ngx_str_t                  root;
} ngx_http_nodejs_loc_conf_t;


static ngx_log_t                *jsLogger = NULL;
static ngx_connection_t          jsPipe;
static ngx_event_t               jsPipeEv;
static ngx_str_t                 jsArgs = ngx_null_string;


static ngx_int_t   ngx_http_nodejs_init          (ngx_cycle_t *cycle);
static void        ngx_http_nodejs_free          (ngx_cycle_t *cycle);
static ngx_int_t   ngx_http_nodejs_preconf       (ngx_conf_t *cf);
static ngx_int_t   ngx_http_nodejs_postconf      (ngx_conf_t *cf);

static char       *ngx_http_nodejs               (ngx_conf_t *cf,
                                                  ngx_command_t *cmd, void *conf);
static char       *ngx_http_nodejs_param         (ngx_conf_t *cf,
                                                  ngx_command_t *cmd, void *conf);
static char       *ngx_http_nodejs_root          (ngx_conf_t *cf,
                                                  ngx_command_t *cmd, void *conf);
static char       *ngx_http_nodejs_args          (ngx_conf_t *cf,
                                                  ngx_command_t *cmd, void *conf);

static void       *ngx_http_nodejs_create_conf   (ngx_conf_t *cf);
static char       *ngx_http_nodejs_merge_conf    (ngx_conf_t *cf, void *parent,
                                                  void *child);


ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
ngx_http_output_body_filter_pt    ngx_http_next_body_filter;


static ngx_command_t ngx_http_nodejs_commands[] = {
    { ngx_string("js_param"),
      NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF
                         | NGX_HTTP_LIF_CONF | NGX_CONF_TAKE2,
      ngx_http_nodejs_param,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("js_root"),
      NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF
                         | NGX_HTTP_LIF_CONF | NGX_CONF_TAKE1,
      ngx_http_nodejs_root,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("js_args"),
      NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
      ngx_http_nodejs_args,
      NGX_HTTP_MAIN_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("js_pass"),
      NGX_HTTP_LOC_CONF | NGX_HTTP_LIF_CONF | NGX_CONF_TAKE1,
      ngx_http_nodejs,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
    ngx_null_command
};


ngx_http_module_t ngx_http_nodejs_module_ctx = {
    ngx_http_nodejs_preconf,          /*  preconfiguration */
    ngx_http_nodejs_postconf,         /*  postconfiguration */

    NULL,                             /*  create main configuration */
    NULL,                             /*  init main configuration */

    NULL,                             /*  create server configuration */
    NULL,                             /*  merge server configuration */

    ngx_http_nodejs_create_conf,      /*  create location configuration */
    ngx_http_nodejs_merge_conf        /*  merge location configuration */
};


ngx_module_t ngx_http_nodejs_module = {
    NGX_MODULE_V1,
    &ngx_http_nodejs_module_ctx,  /*  module context */
    ngx_http_nodejs_commands,     /*  module directives */
    NGX_HTTP_MODULE,              /*  module type */
    NULL,                         /*  init master */
    NULL,                         /*  init module */
    ngx_http_nodejs_init,         /*  init process */
    NULL,                         /*  init thread */
    NULL,                         /*  exit thread */
    ngx_http_nodejs_free,         /*  exit process */
    NULL,                         /*  exit master */
    NGX_MODULE_V1_PADDING
};


typedef struct ngx_http_nodejs_ctx_s ngx_http_nodejs_ctx_t;
struct ngx_http_nodejs_ctx_s {
    nodejsContext        *js_ctx;
    unsigned              skip_filter:1;
    unsigned              headers_sent:1;
    unsigned              refused:1;
};


static int64_t ngx_atomic_fetch_add_wrap(int64_t *value, int64_t add)
{
    return ngx_atomic_fetch_add((ngx_atomic_int_t *)value, (ngx_atomic_int_t)add);
}


static void
ngx_http_nodejs_log(unsigned level, const char *fmt, ...)
{
    va_list  args;

    if (jsLogger != NULL && jsLogger->log_level >= level) {
        va_start(args, fmt);
        ngx_log_error_core(level, jsLogger, 0, fmt, args);
        va_end(args);
    }
}


static void
ngx_http_nodejs_cleanup_context(void *data)
{
    dd("context cleanup (%p)", data);
    nodejsContextAttemptFree((nodejsContext *)data);
}


ngx_inline static ngx_http_nodejs_ctx_t *
ngx_http_nodejs_create_ctx(ngx_http_request_t *r)
{
    ngx_http_nodejs_ctx_t       *ctx;
    ngx_http_nodejs_ctx_t       *main_ctx;
    nodejsContext               *js_ctx;
    ngx_pool_cleanup_t          *cln;

    main_ctx = ngx_http_get_module_ctx(r->main, ngx_http_nodejs_module);

    // ngx_pcalloc() does ngx_memzero() too.
    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_nodejs_ctx_t));

    if (ctx == NULL) {
        return NULL;
    }

    cln = ngx_pool_cleanup_add(r->pool, 0);
    if (cln == NULL) {
        return NULL;
    }

    dd("context creation");

    js_ctx = nodejsContextCreate(r,
                                 main_ctx == NULL ? NULL : main_ctx->js_ctx,
                                 ngx_atomic_fetch_add_wrap);
    if (js_ctx == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "Failed to create nodejs context");
        return NULL;
    }

    js_ctx->jsCallback = NULL;

    ctx->js_ctx = js_ctx;

    cln->handler = ngx_http_nodejs_cleanup_context;
    cln->data = js_ctx;

    if (main_ctx == NULL) {
        // This would be the new root js context, store it in the main request.
        ngx_http_set_ctx(r->main, ctx, ngx_http_nodejs_module);
    }

    return ctx;
}


ngx_inline static ngx_int_t
ngx_http_nodejs_send_chunk(ngx_http_request_t *r, char *data, size_t len,
                         unsigned last)
{
    ngx_buf_t                        *b;
    ngx_chain_t                       out;
    ngx_int_t                         rc;
    ngx_http_nodejs_ctx_t            *ctx;
    ngx_http_postponed_request_t     *pr;
    ngx_http_request_t               *cr;

    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t) + len);
    if (b == NULL) {
        ngx_http_finalize_request(r, NGX_ERROR);
        return NGX_ERROR;
    }

    if (len) {
        b->start = (u_char *)&b[1];
        (void)ngx_copy(b->start, data, len);
        b->pos = b->start;
        b->last = b->end = b->start + len;
        b->temporary = 1;
    } else{
        b->start = b->pos = b->last = b->end = NULL;
    }

    b->last_in_chain = 1;
    b->flush = 1;
    b->last_buf = last;

    out.buf = b;
    out.next = NULL;

    dd("Sending response chunk (len: %zd, last: %d)", len, last);

    ctx = ngx_http_get_module_ctx(r, ngx_http_nodejs_module);

    if (ctx == NULL || ctx->skip_filter ||
        (r->postponed == NULL && r->connection->data == r))
    {
        rc = ngx_http_output_filter(r, &out);
    } else {
        // We will completely eat this chunk in our body filter, avoid
        // postpone filter for it.
        pr = r->postponed;
        cr = r->connection->data;

        r->postponed = NULL;
        r->connection->data = r;

        rc = ngx_http_output_filter(r, &out);

        r->postponed = pr;
        r->connection->data = cr;
    }

    if (rc == NGX_ERROR) {
        ngx_http_finalize_request(r, NGX_ERROR);
        return NGX_ERROR;
    }

    return NGX_OK;
}


static void
ngx_http_nodejs_read_request_body(ngx_http_request_t *r)
{
    ngx_http_nodejs_ctx_t    *ctx;
    ngx_chain_t              *cl;
    ngx_int_t                 rc;

    ctx = ngx_http_get_module_ctx(r, ngx_http_nodejs_module);
    if (ctx == NULL) {
        ngx_http_finalize_request(r, NGX_ERROR);
        return;
    }

    dd("Read request body (r: %p)", r);

    if (r->request_body == NULL || r->request_body->bufs == NULL) {
        rc = nodejsChunk(ctx->js_ctx, NULL, 0, 1, 0);
        if (rc)
            goto error;
    } else {
        for (cl = r->request_body->bufs; cl; cl = cl->next) {
            rc = nodejsChunk(ctx->js_ctx,
                             (char *)cl->buf->pos,
                             cl->buf->last - cl->buf->pos,
                             cl->buf->last_buf,
                             0);
            if (rc)
                goto error;

            cl->buf->pos = cl->buf->last;
            cl->buf->file_pos = cl->buf->file_last;
        }
    }

    return;

error:
    ngx_http_finalize_request(r, NGX_ERROR);
}


ngx_inline static ngx_int_t
ngx_http_nodejs_set_response_meta(ngx_http_request_t *r,
                                  nodejsHeaders *headers)
{
    r->headers_out.status = (ngx_uint_t)headers->statusCode;

    if (headers->statusMessage.len > 0) {
        r->headers_out.status_line.len = headers->statusMessage.len;
        r->headers_out.status_line.data = \
                ngx_pstrdup(r->pool, (ngx_str_t *) &headers->statusMessage);
    }

    return NGX_OK;
}


ngx_inline static ngx_table_elt_t *
ngx_http_nodejs_find_header(ngx_list_t *list, ngx_str_t *key)
{
    ngx_table_elt_t             *h;
    ngx_list_part_t             *part;
    ngx_uint_t                   i;

    part = &list->part;
    h = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            h = part->elts;
            i = 0;
        }

        if (h[i].hash != 0 &&
            h[i].key.len == key->len &&
            ngx_strncasecmp(key->data, h[i].key.data, h[i].key.len) == 0)
        {
            dd("found out header %.*s", (int)h[i].key.len, h[i].key.data);
            return h;
        }
    }

    return NULL;
}


ngx_inline static ngx_int_t
ngx_http_nodejs_push_headers(ngx_pool_t *pool, ngx_list_t *list,
                             nodejsHeaders *headers)
{
    size_t            len = headers->len;
    nodejsString     *arr = headers->strings;
    size_t            i;
    ngx_table_elt_t  *h;

    for (i = 0; i < len; i += 2) {
        h = ngx_http_nodejs_find_header(list, (ngx_str_t *)&arr[i]);

        if (h == NULL) {
            h = ngx_list_push(list);
            if (h == NULL) {
                return NGX_ERROR;
            }
        }

        h->hash = 1;

        h->key.data = ngx_pstrdup(pool, (ngx_str_t *)&arr[i]);
        h->key.len = arr[i].len;

        h->value.data = ngx_pstrdup(pool, (ngx_str_t *)&arr[i + 1]);
        h->value.len = arr[i + 1].len;
    }

    return NGX_OK;
}


ngx_inline static ngx_int_t
ngx_http_nodejs_init_subrequest_headers(ngx_http_request_t *sr,
                                        nodejsHeaders *headers,
                                        off_t len)
{
    memset(&sr->headers_in, 0, sizeof(ngx_http_headers_in_t));

    sr->headers_in.content_length_n = len;

    if (ngx_list_init(&sr->headers_in.headers, sr->pool, 20,
                      sizeof(ngx_table_elt_t)) != NGX_OK)
    {
        return NGX_ERROR;
    }

    return ngx_http_nodejs_push_headers(sr->pool,
                                        &sr->headers_in.headers,
                                        headers);
}


static ngx_int_t
ngx_http_nodejs_subrequest_done(ngx_http_request_t *sr,
                                void *data, ngx_int_t rc)
{
    dd("post subrequest %ld (%p, %.*s)",
       rc, sr, (int)sr->uri.len, sr->uri.data);

    if (sr != sr->connection->data)
        sr->connection->data = sr;

    if (rc == NGX_OK) {
        if (nodejsChunk(((ngx_http_nodejs_ctx_t *)data)->js_ctx, NULL, 0, 1, 1))
            return NGX_ERROR;
    }

    return rc;
}


static ngx_int_t
ngx_http_nodejs_subrequest(ngx_http_request_t *r, nodejsFromJS *cmd)
{
    nodejsSubrequest               *data;
    ngx_http_nodejs_ctx_t          *sr_ctx;
    ngx_http_request_t             *sr;
    ngx_http_post_subrequest_t     *psr;
    ngx_http_request_body_t        *rb;
    ngx_buf_t                      *b;
    ngx_http_core_main_conf_t      *cmcf;
    ngx_str_t                       sr_uri;
    ngx_str_t                       args;
    ngx_uint_t                      flags = 0;
    ngx_str_t                       sr_body;
    ngx_http_postponed_request_t   *pr;

    data = (nodejsSubrequest *)cmd->data;
    if (data == NULL) {
        return NGX_ERROR;
    }

    psr = ngx_palloc(r->pool, sizeof(ngx_http_post_subrequest_t));
    if (psr == NULL) {
        return NGX_ERROR;
    }

    psr->handler = ngx_http_nodejs_subrequest_done;

    sr_uri.len = data->url.len;
    sr_uri.data = ngx_pstrdup(r->pool, (ngx_str_t *)&data->url);

    memset(&args, 0, sizeof(ngx_str_t));

    if (ngx_http_parse_unsafe_uri(r, &sr_uri, &args, &flags) != NGX_OK) {
        return NGX_ERROR;
    }

    if (data->body.len) {
        sr_body.len = data->body.len;
        sr_body.data = ngx_pstrdup(r->pool, (ngx_str_t *)&data->body);
    } else {
        memset(&sr_body, 0, sizeof(ngx_str_t));
    }

    if (ngx_http_subrequest(r->main, &sr_uri, &args, &sr, psr, 0) != NGX_OK) {
        return NGX_ERROR;
    }

    dd("subrequest (%p, %.*s)", sr, (int)sr->uri.len, sr->uri.data);

    if (sr->connection->data == sr) {
        sr->connection->data = r;
    }

    // Remove subrequest from postponed requests.
    // Subrequest is the last item of the chain or the only item of the chain.
    pr = r->main->postponed;

    while (pr != NULL && pr->next != NULL) {
        // Run through the chain.
        pr = pr->next;
    }

    if (pr != NULL) {
        if (pr->next != NULL)
            pr->next = NULL; // Last item of the chain.
        else
            r->main->postponed = NULL; // The only item of the chain.
    }

    // Don't inherit parent request variables. Content-Length is
    // cacheable in proxy module and we don't need Content-Length from
    // another subrequest.
    cmcf = ngx_http_get_module_main_conf(sr, ngx_http_core_module);

    sr->variables = ngx_pcalloc(sr->pool, cmcf->variables.nelts
                                * sizeof(ngx_http_variable_value_t));
    if (sr->variables == NULL) {
        return NGX_ERROR;
    }

    sr->method = NGX_HTTP_GET;

    if (ngx_http_nodejs_init_subrequest_headers(sr,
                                                &data->headers,
                                                sr_body.len) == NGX_ERROR)
    {
        return NGX_ERROR;
    }

    sr_ctx = ngx_http_nodejs_create_ctx(sr);

    if (sr_ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_http_set_ctx(sr, sr_ctx, ngx_http_nodejs_module);

    psr->data = sr_ctx;

    sr_ctx->js_ctx->jsSubrequestCallback = cmd->jsCallback;
    cmd->jsCallback = NULL;

    if (sr_body.len > 0) {
        rb = ngx_pcalloc(r->pool, sizeof(ngx_http_request_body_t));
        if (rb == NULL) {
            return NGX_ERROR;
        }

        b = ngx_calloc_buf(r->pool);
        if (b == NULL) {
            return NGX_ERROR;
        }

        rb->bufs = ngx_alloc_chain_link(r->pool);
        if (rb->bufs == NULL) {
            return NGX_ERROR;
        }

        b->temporary = 1;
        b->start = b->pos = sr_body.data;
        b->end = b->last = sr_body.data + sr_body.len;
        b->last_buf = 1;
        b->last_in_chain = 1;

        rb->bufs->buf = b;
        rb->bufs->next = NULL;
        rb->buf = b;

        sr->request_body = rb;
    } else {
        sr->request_body = NULL;
    }

    ngx_http_run_posted_requests(sr->connection);

    return NGX_OK;
}


static void
ngx_http_nodejs_receive(ngx_event_t *ev)
{
    // Something is available to read.
    nodejsFromJS   *cmd;

    cmd = nodejsFromJSRecv();
    if (cmd != NULL) {
        ngx_int_t              rc;
        nodejsContext         *js_ctx = cmd->jsCtx;
        ngx_http_request_t    *r = js_ctx == NULL ?
                NULL
                :
                (ngx_http_request_t *)js_ctx->r;

        switch (cmd->type) {
            case FROM_JS_ERROR:
                ngx_http_finalize_request(r, NGX_ERROR);
                return;

            case FROM_JS_INIT_CALLBACK:
                if (js_ctx == NULL || js_ctx->wait == 0 || js_ctx->jsCallback)
                    break;

                js_ctx->jsCallback = cmd->jsCallback;
                cmd->jsCallback = NULL;

                break;

            case FROM_JS_READ_REQUEST_BODY:
                if (js_ctx == NULL || js_ctx->wait == 0)
                    break;

                rc = ngx_http_read_client_request_body(
                    r, ngx_http_nodejs_read_request_body
                );

                if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
                    ngx_http_finalize_request(r, NGX_ERROR);
                    return;
                }

                break;

            case FROM_JS_RESPONSE_HEADERS:
                if (js_ctx == NULL || js_ctx->wait == 0)
                    break;

                dd("Sending response headers");

                rc = ngx_http_nodejs_set_response_meta(r, cmd->data);

                if (rc == NGX_OK) {
                    rc = ngx_http_nodejs_push_headers(r->pool,
                                                      &r->headers_out.headers,
                                                      cmd->data);
                }

                if (rc != NGX_OK || (ngx_http_send_header(r) != NGX_OK)) {
                    ngx_http_finalize_request(r, NGX_ERROR);
                    return;
                }

                break;

            case FROM_JS_RESPONSE_BODY:
                if (js_ctx == NULL || js_ctx->wait == 0)
                    break;

                if (cmd->data) {
                    nodejsString  *s = (nodejsString *)cmd->data;
                    ngx_http_nodejs_send_chunk(r, s->data, s->len, 0);
                } else {
                    // Got the end of response mark.
                    // This one is for r->main->count++ in ngx_http_nodejs_handler.
                    r->main->count--;

                    js_ctx->wait--;
                    js_ctx->rootCtx->wait--;

                    if (r != r->main && js_ctx->wait == 0) {
                        // Finalize the request.
                        ngx_http_nodejs_send_chunk(r, NULL, 0, 1);
                        r->connection->data = r;
                        ngx_http_finalize_request(r, NGX_OK);
                    }

                    if (r->main->count == 1 && js_ctx->rootCtx->wait == 0) {
                        // No subrequests left, finalize the main request.
                        ngx_http_nodejs_send_chunk(r->main, NULL, 0, 1);
                        ngx_http_finalize_request(r->main, NGX_DONE);
                    }
                }

                break;

            case FROM_JS_BEGIN_SUBREQUEST:
                if (js_ctx == NULL || js_ctx->wait == 0)
                    break;

                if (ngx_http_nodejs_subrequest(r, cmd) == NGX_ERROR) {
                    ngx_http_finalize_request(r, NGX_ERROR);
                    return;
                }

                js_ctx->wait++;
                js_ctx->rootCtx->wait++;

                break;

            case FROM_JS_SUBREQUEST_DONE:
                if (js_ctx == NULL || js_ctx->wait == 0)
                    break;

                js_ctx->wait--;
                js_ctx->rootCtx->wait--;

                if (r != r->main && js_ctx->wait == 0) {
                    // Finalize the request.
                    ngx_http_nodejs_send_chunk(r, NULL, 0, 1);
                    r->connection->data = r;
                    ngx_http_finalize_request(r, NGX_OK);
                }

                if (r->main->count == 1 && js_ctx->rootCtx->wait == 0) {
                    // No subrequests left, finalize the main request.
                    ngx_http_nodejs_send_chunk(r->main, NULL, 0, 1);
                    ngx_http_finalize_request(r->main, NGX_DONE);
                }

                break;

            case FROM_JS_LOG:
                break;
        }

        nodejsFromJSFree(cmd);
    }
}


static ngx_int_t
ngx_http_nodejs_init(ngx_cycle_t *cycle)
{
    dd("start");

    int  rc;
    int  fd = -1;

    jsLogger = cycle->log;

    rc = nodejsStart(ngx_http_nodejs_log, &fd);

    if (rc) {
        ngx_log_error(NGX_LOG_ERR, cycle->log, 0,
                      "Failed to start nodejs (%d)", rc);
        return NGX_ERROR;
    }

    dd("started");

    memset(&jsPipe, 0, sizeof(jsPipe));
    memset(&jsPipeEv, 0, sizeof(jsPipeEv));

    jsPipe.number = ngx_atomic_fetch_add(ngx_connection_counter, 1);
    jsPipe.fd = fd;
    jsPipe.pool = cycle->pool;
    jsPipe.log = cycle->log;
    jsPipe.write = jsPipe.read = &jsPipeEv;

    jsPipeEv.data = &jsPipe;
    jsPipeEv.handler = ngx_http_nodejs_receive;
    jsPipeEv.log = cycle->log;
    jsPipeEv.index = NGX_INVALID_INDEX;

    dd("add pipe handler");
    ngx_add_event(&jsPipeEv, NGX_READ_EVENT, 0);

    return NGX_OK;
}


static void
ngx_http_nodejs_free(ngx_cycle_t *cycle)
{
    dd("stop");
    jsLogger = NULL;
    //nodejsFree();
}


static ngx_int_t
ngx_http_nodejs_preconf(ngx_conf_t *cf)
{
    dd("preconfiguration");
    nodejsNextScriptsVersion();
    return NGX_OK;
}


ngx_inline static ngx_int_t
ngx_http_nodejs_process_headers(ngx_http_request_t *r, ngx_http_nodejs_ctx_t *ctx)
{
    return NGX_OK;
}


ngx_inline static ngx_int_t
ngx_http_nodejs_aggregate_headers(ngx_http_request_t *r, unsigned out,
                                ngx_str_t ***ret)
{
    ngx_uint_t                 len;
    ngx_str_t                **headers;
    ngx_uint_t                 i;
    ngx_uint_t                 j;
    ngx_list_part_t           *part;
    ngx_table_elt_t           *header;

    if (out) {
        len = r->headers_out.headers.nalloc;
        part = &r->headers_out.headers.part;
    } else {
        len = r->headers_in.headers.nalloc;
        part = &r->headers_in.headers.part;
    }

    if (len > 0) {
        // Allocate memory for a NULL-terminated list of pointers to names
        // and values and for the values.
        headers = ngx_palloc(r->pool,
                sizeof(ngx_str_t *) * (len * 2 + 1));

        if (headers == NULL)
            return NGX_ERROR;

        j = 0;

        header = part->elts;

        for (i = 0; /* void */; i++) {

            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }

                part = part->next;
                header = part->elts;
                i = 0;
            }

            headers[j++] = &header[i].key;
            headers[j++] = &header[i].value;
        }

        // NULL-terminate.
        headers[j] = NULL;
    } else {
        headers = NULL;
    }

    *ret = headers;

    return NGX_OK;
}


static ngx_int_t
ngx_http_nodejs_header_filter(ngx_http_request_t *r)
{
    ngx_http_nodejs_ctx_t       *ctx;
    ngx_str_t                  **headers;
    ngx_int_t                    rc;
    ngx_http_request_t          *mr;

    ctx = ngx_http_get_module_ctx(r, ngx_http_nodejs_module);

    // Temporary dirty hack to force ngx_http_headers_filter() to run for a
    // subrequest.
    // TODO: Copy-paste ngx_http_headers_filter() behaviour here instead.
    mr = r->main;
    r->main = r;
    rc = ngx_http_next_header_filter(r);
    r->main = mr;

    if (ctx != NULL && !ctx->skip_filter) {
        dd("header filter (%p, %.*s)", r, (int)r->uri.len, r->uri.data);

        if (ngx_http_nodejs_aggregate_headers(r, 1, &headers) != NGX_OK)
            return NGX_ERROR;

        if (nodejsSubrequestHeaders(ctx->js_ctx, r->headers_out.status,
                                    (nodejsString *)&r->headers_out.status_line,
                                    (nodejsString **)headers))
            return NGX_ERROR;
    }

    return rc;
}


static ngx_int_t
ngx_http_nodejs_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_http_nodejs_ctx_t  *ctx;
    ngx_chain_t            *cl;
    ngx_int_t               rc;

    ctx = ngx_http_get_module_ctx(r, ngx_http_nodejs_module);

    if (ctx == NULL || ctx->skip_filter) {
        return ngx_http_next_body_filter(r, in);
    }

    // It's a subrequest response body. Eating it up and passing to nodejs.
    for (cl = in; cl; cl = cl->next) {
        if (!ctx->refused && cl->buf->pos != NULL) {
            rc = nodejsChunk(ctx->js_ctx,
                             (char *)cl->buf->pos,
                             cl->buf->last - cl->buf->pos,
                             0,
                             1);
            if (rc)
                return NGX_ERROR;
        }

        cl->buf->pos = cl->buf->last;
        cl->buf->file_pos = cl->buf->file_last;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_nodejs_postconf(ngx_conf_t *cf)
{
    dd("postconfiguration");

    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_nodejs_header_filter;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_nodejs_body_filter;

    return NGX_OK;
}


static void
ngx_http_nodejs_cleanup_request(void *data)
{
    dd("request cleanup");
}


static ngx_int_t
ngx_http_nodejs_handler(ngx_http_request_t *r) {
    ngx_http_nodejs_ctx_t       *ctx;
    ngx_http_nodejs_loc_conf_t  *conf;
    ngx_pool_cleanup_t          *cln;
    ngx_int_t                    rc;
    ngx_http_nodejs_param_t     *params;
    ngx_uint_t                   len;
    ngx_uint_t                   i;
    ngx_uint_t                   j;
    ngx_str_t                   *param;
    ngx_str_t                  **nodejs_params;
    ngx_str_t                  **nodejs_headers;

    dd("begin (r: %p, main: %p)", r, r->main);

    conf = ngx_http_get_module_loc_conf(r, ngx_http_nodejs_module);
    if (conf == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_nodejs_module);

    if (ctx == NULL) {
        ctx = ngx_http_nodejs_create_ctx(r);
        if (ctx == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        ctx->skip_filter = 1;

        ngx_http_set_ctx(r, ctx, ngx_http_nodejs_module);
    } else {
        ctx->skip_filter = 0;
    }

    if (!conf->fulljs) {
        if (conf->root.len) {
            rc = ngx_get_full_name(r->pool, &conf->root, &conf->js);

            if (rc != NGX_OK) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
        }

        conf->fulljs = 1;
    }

    len = conf->params != NULL ? conf->params->nelts : 0;

    if (len > 0) {
        // Allocate memory for a NULL-terminated list of pointers to names
        // and values and for the values.
        nodejs_params = ngx_palloc(r->pool,
                                   sizeof(ngx_str_t *) * (len * 2 + 1) +
                                   sizeof(ngx_str_t) * len);

        if (nodejs_params == NULL)
            return NGX_HTTP_INTERNAL_SERVER_ERROR;

        params = conf->params->elts;
        param = (ngx_str_t *)&nodejs_params[len * 2 + 1];
        j = 0;

        for (i = 0; i < len; i++) {
            if (ngx_http_complex_value(r, &params[i].value, param) != NGX_OK)
                return NGX_HTTP_INTERNAL_SERVER_ERROR;

            // ngx_http_complex_value gives zero char in the end sometimes.
            if (param->len > 0 && param->data[param->len - 1] == 0)
                param->len--;

            nodejs_params[j++] = &params[i].name;
            nodejs_params[j++] = param;

            param = (ngx_str_t *)(((char *)param) + sizeof(ngx_str_t));
        }

        // NULL-terminate.
        nodejs_params[j] = NULL;
    } else {
        nodejs_params = NULL;
    }

    if (ngx_http_nodejs_aggregate_headers(r, 0, &nodejs_headers) != NGX_OK)
        return NGX_HTTP_INTERNAL_SERVER_ERROR;

    r->headers_out.status = NGX_HTTP_OK;

    cln = ngx_pool_cleanup_add(r->pool, 0);
    if (cln == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    cln->handler = ngx_http_nodejs_cleanup_request;
    cln->data = ctx;

    rc = ngx_http_nodejs_process_headers(r, ctx);
    if (rc == NGX_ERROR) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    r->main->count++;
    ctx->js_ctx->wait++;
    ctx->js_ctx->rootCtx->wait++;

    rc = nodejsCall((nodejsString *)&conf->js, ctx->js_ctx,
                    (nodejsString *)&r->method_name,
                    (nodejsString *)&r->unparsed_uri,
                    (nodejsString *)&r->http_protocol,
                    (nodejsString **)nodejs_headers,
                    (nodejsString **)nodejs_params);
    if (rc) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    return NGX_DONE;
}


static char *
ngx_http_nodejs(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_nodejs_loc_conf_t    *xlcf = conf;
    ngx_http_core_loc_conf_t      *clcf;
    ngx_str_t                     *value;

    value = cf->args->elts;

    if (xlcf->js.data) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "Duplicate nodejs instruction");
        return NGX_CONF_ERROR;
    }

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    if (clcf == NULL) {
        return NGX_CONF_ERROR;
    }

    clcf->handler = ngx_http_nodejs_handler;

    xlcf->js.data = ngx_pstrdup(cf->pool, &value[1]);
    xlcf->js.len = (&value[1])->len;
    xlcf->fulljs = 0;

    if (xlcf->js.data == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_nodejs_param(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_nodejs_loc_conf_t          *xlcf = conf;

    ngx_http_nodejs_param_t             *param;
    ngx_http_compile_complex_value_t     ccv;
    ngx_str_t                           *value;

    value = cf->args->elts;

    if (xlcf->params == NULL) {
        xlcf->params = ngx_array_create(cf->pool, 2,
                                        sizeof(ngx_http_nodejs_param_t));
        if (xlcf->params == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    param = ngx_array_push(xlcf->params);
    if (param == NULL) {
        return NGX_CONF_ERROR;
    }

    param->name.data = ngx_pstrdup(cf->pool, &value[1]);
    if (param->name.data == NULL) {
        return NGX_CONF_ERROR;
    }

    param->name.len = value[1].len;

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[2];
    ccv.complex_value = &param->value;
    ccv.zero = 1;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static void *
ngx_http_nodejs_create_conf(ngx_conf_t *cf)
{
    ngx_http_nodejs_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_nodejs_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    return conf;
}


static char *
ngx_http_nodejs_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_nodejs_loc_conf_t  *prev = parent;
    ngx_http_nodejs_loc_conf_t  *conf = child;
    ngx_http_nodejs_param_t     *prevparams, *params, *param;
    ngx_uint_t                   i, j;
    int                          add;

    if (conf->root.len == 0) {
        conf->root.data = prev->root.data;
        conf->root.len = prev->root.len;
    }

    if (conf->root.len == 0) {
        conf->root.data = cf->cycle->prefix.data;
        conf->root.len = cf->cycle->prefix.len;
    }

    if (conf->params == NULL) {
        conf->params = prev->params;
    } else if (prev->params != NULL) {
        prevparams = prev->params->elts;
        params = conf->params->elts;

        for (i = 0; i < prev->params->nelts; i++) {
            add = 1;

            for (j = 0; j < conf->params->nelts; j++) {
                if (prevparams[i].name.len != params[j].name.len ||
                    ngx_strncmp(prevparams[i].name.data, params[j].name.data,
                                params[j].name.len) == 0)
                {
                    add = 0;
                    break;
                }
            }

            if (add) {
                param = ngx_array_push(conf->params);
                if (param == NULL) {
                    return NGX_CONF_ERROR;
                }

                param->name = prevparams[i].name;
                param->value = prevparams[i].value;
            }
        }
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_nodejs_root(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_nodejs_loc_conf_t        *xlcf = conf;
    ngx_str_t                         *value;

    value = cf->args->elts;

    xlcf->root.data = ngx_palloc(cf->pool, (&value[1])->len + 1);
    xlcf->root.len = (&value[1])->len;
    ngx_memcpy(xlcf->root.data, (&value[1])->data, xlcf->root.len);

    if (xlcf->root.len && (xlcf->root.data[xlcf->root.len - 1] != '/')) {
        xlcf->root.data[xlcf->root.len++] = '/';
    }

    dd("setting root to `%.*s`", (int)xlcf->root.len, xlcf->root.data);

    return NGX_CONF_OK;
}


static char *
ngx_http_nodejs_args(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                         *value;

    value = cf->args->elts;

    jsArgs.data = ngx_pstrdup(cf->pool, &value[1]);
    jsArgs.len = (&value[1])->len;

    dd("setting arguments to `%s`", jsArgs.data);

    return NGX_CONF_OK;
}
