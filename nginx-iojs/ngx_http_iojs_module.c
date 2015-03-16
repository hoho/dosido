/*
 * Copyright (C) Marat Abdullin
 */

#define DDEBUG 1


#ifndef DDEBUG
#define DDEBUG 0
#endif

#include "ddebug.h"

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <ngx_string.h>
#include <ngx_array.h>

#include <unistd.h>
#include <libiojs.h>
#include <string.h>


typedef struct {
    ngx_str_t                  name;
    ngx_http_complex_value_t   value;
} ngx_http_iojs_param_t;


typedef struct {
    ngx_str_t                  js;
    int                        js_index;
    ngx_array_t               *params;       /* ngx_http_iojs_param_t */
    ngx_str_t                  root;
} ngx_http_iojs_loc_conf_t;


static ngx_connection_t          jsPipe;
static ngx_event_t               jsPipeEv;
static ngx_str_t                 jsArgs = ngx_null_string;
static ngx_array_t               jsLocations;

static ngx_str_t                 ngx_http_iojs_content_length_key = \
                                     ngx_string("Content-Length");


static ngx_int_t   ngx_http_iojs_init          (ngx_cycle_t *cycle);
static void        ngx_http_iojs_free          (ngx_cycle_t *cycle);
static ngx_int_t   ngx_http_iojs_preconf       (ngx_conf_t *cf);
static ngx_int_t   ngx_http_iojs_postconf      (ngx_conf_t *cf);

static char       *ngx_http_iojs               (ngx_conf_t *cf,
                                                ngx_command_t *cmd, void *conf);
static char       *ngx_http_iojs_param         (ngx_conf_t *cf,
                                                ngx_command_t *cmd, void *conf);
static char       *ngx_http_iojs_root          (ngx_conf_t *cf,
                                                ngx_command_t *cmd, void *conf);
static char       *ngx_http_iojs_args          (ngx_conf_t *cf,
                                                ngx_command_t *cmd, void *conf);

static void       *ngx_http_iojs_create_conf   (ngx_conf_t *cf);
static char       *ngx_http_iojs_merge_conf    (ngx_conf_t *cf, void *parent,
                                                void *child);




ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
ngx_http_output_body_filter_pt    ngx_http_next_body_filter;


static ngx_command_t ngx_http_iojs_commands[] = {
    { ngx_string("js_param"),
      NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF
                         | NGX_HTTP_LIF_CONF | NGX_CONF_TAKE2,
      ngx_http_iojs_param,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("js_root"),
      NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF
                         | NGX_HTTP_LIF_CONF | NGX_CONF_TAKE1,
      ngx_http_iojs_root,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("js_args"),
      NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
      ngx_http_iojs_args,
      NGX_HTTP_MAIN_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("js_pass"),
      NGX_HTTP_LOC_CONF | NGX_HTTP_LIF_CONF | NGX_CONF_TAKE1,
      ngx_http_iojs,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
    ngx_null_command
};


ngx_http_module_t ngx_http_iojs_module_ctx = {
    ngx_http_iojs_preconf,            /*  preconfiguration */
    ngx_http_iojs_postconf,           /*  postconfiguration */

    NULL,                             /*  create main configuration */
    NULL,                             /*  init main configuration */

    NULL,                             /*  create server configuration */
    NULL,                             /*  merge server configuration */

    ngx_http_iojs_create_conf,        /*  create location configuration */
    ngx_http_iojs_merge_conf          /*  merge location configuration */
};


ngx_module_t ngx_http_iojs_module = {
    NGX_MODULE_V1,
    &ngx_http_iojs_module_ctx,  /*  module context */
    ngx_http_iojs_commands,     /*  module directives */
    NGX_HTTP_MODULE,            /*  module type */
    NULL,                       /*  init master */
    NULL,                       /*  init module */
    ngx_http_iojs_init,         /*  init process */
    NULL,                       /*  init thread */
    NULL,                       /*  exit thread */
    ngx_http_iojs_free,         /*  exit process */
    NULL,                       /*  exit master */
    NGX_MODULE_V1_PADDING
};


typedef struct ngx_http_iojs_ctx_s ngx_http_iojs_ctx_t;
struct ngx_http_iojs_ctx_s {
    iojsContext          *js_ctx;
    unsigned              skip_filter:1;
    unsigned              headers_sent:1;
    unsigned              refused:1;
};


static int64_t ngx_atomic_fetch_add_wrap(int64_t *value, int64_t add)
{
    return ngx_atomic_fetch_add(value, add);
}


static void
ngx_http_iojs_cleanup_context(void *data)
{
    dd("context cleanup (%p)", data);
    iojsContextAttemptFree((iojsContext *)data);
}


ngx_inline static ngx_http_iojs_ctx_t *
ngx_http_iojs_create_ctx(ngx_http_request_t *r)
{
    ngx_http_iojs_ctx_t       *ctx;
    ngx_http_iojs_ctx_t       *main_ctx;
    iojsContext               *js_ctx;
    ngx_pool_cleanup_t        *cln;

    main_ctx = ngx_http_get_module_ctx(r->main, ngx_http_iojs_module);

    // ngx_pcalloc() does ngx_memzero() too.
    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_iojs_ctx_t));

    if (ctx == NULL) {
        return NULL;
    }

    cln = ngx_pool_cleanup_add(r->pool, 0);
    if (cln == NULL) {
        return NULL;
    }

    dd("context creation");

    js_ctx = iojsContextCreate(r,
                               main_ctx == NULL ? NULL : main_ctx->js_ctx,
                               ngx_atomic_fetch_add_wrap);
    if (js_ctx == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "Failed to create iojs context");
        return NULL;
    }

    js_ctx->jsCallback = NULL;

    ctx->js_ctx = js_ctx;

    cln->handler = ngx_http_iojs_cleanup_context;
    cln->data = js_ctx;

    if (main_ctx == NULL) {
        // This would be the new root js context, store it in the main request.
        ngx_http_set_ctx(r->main, ctx, ngx_http_iojs_module);
    }

    return ctx;
}


ngx_inline static ngx_int_t
ngx_http_iojs_send_chunk(ngx_http_request_t *r, char *data, size_t len,
                         unsigned last)
{
    ngx_buf_t           *b;
    ngx_chain_t          out;
    ngx_int_t            rc;

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

    if (r != r->connection->data)
        r->connection->data = r;

    rc = ngx_http_output_filter(r, &out);

    if (rc == NGX_ERROR) {
        ngx_http_finalize_request(r, NGX_ERROR);
        return NGX_ERROR;
    }

    return NGX_OK;
}


static void
ngx_http_iojs_read_request_body(ngx_http_request_t *r)
{
    ngx_http_iojs_ctx_t  *ctx;
    ngx_chain_t          *cl;
    ngx_int_t             rc;
    iojsString            s;

    ctx = ngx_http_get_module_ctx(r, ngx_http_iojs_module);
    if (ctx == NULL) {
        ngx_http_finalize_request(r, NGX_ERROR);
        return;
    }

    dd("Read request body (r: %p)", r);

    if (r->request_body->bufs == NULL) {
        rc = iojsChunk(ctx->js_ctx, NULL, 0, 1, 0);
        if (rc)
            goto error;
    } else {
        for (cl = r->request_body->bufs; cl; cl = cl->next) {
            s.data = (char *)cl->buf->pos;
            s.len = cl->buf->last - cl->buf->pos;

            rc = iojsChunk(ctx->js_ctx,
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
ngx_http_iojs_init_subrequest_headers(ngx_http_request_t *sr, off_t len)
{
    ngx_table_elt_t  *h;
    u_char           *p;

    memset(&sr->headers_in, 0, sizeof(ngx_http_headers_in_t));

    sr->headers_in.content_length_n = len;

    if (ngx_list_init(&sr->headers_in.headers, sr->pool, 20,
                      sizeof(ngx_table_elt_t)) != NGX_OK)
    {
        return NGX_ERROR;
    }

    h = ngx_list_push(&sr->headers_in.headers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    h->key = ngx_http_iojs_content_length_key;
    h->lowcase_key = ngx_pnalloc(sr->pool, h->key.len);
    if (h->lowcase_key == NULL) {
        return NGX_ERROR;
    }

    ngx_strlow(h->lowcase_key, h->key.data, h->key.len);

    sr->headers_in.content_length = h;

    p = ngx_palloc(sr->pool, NGX_OFF_T_LEN);
    if (p == NULL) {
        return NGX_ERROR;
    }

    h->value.data = p;

    h->value.len = ngx_sprintf(h->value.data, "%O", len) - h->value.data;

    h->hash = ngx_hash(ngx_hash(ngx_hash(ngx_hash(ngx_hash(ngx_hash(ngx_hash(
        ngx_hash(ngx_hash(ngx_hash(ngx_hash(ngx_hash(
        ngx_hash('c', 'o'), 'n'), 't'), 'e'), 'n'), 't'), '-'), 'l'), 'e'),
        'n'), 'g'), 't'), 'h');

    return NGX_OK;
}


static ngx_int_t
ngx_http_iojs_subrequest_done(ngx_http_request_t *sr, void *data, ngx_int_t rc)
{
    dd("post subrequest %ld (%p, %.*s)", rc, sr, (int)sr->uri.len, sr->uri.data);

    if (sr != sr->connection->data)
        sr->connection->data = sr;

    if (rc == NGX_OK) {
        if (iojsChunk(((ngx_http_iojs_ctx_t *)data)->js_ctx, NULL, 0, 1, 1))
            return NGX_ERROR;
    }

    return rc;
}


static ngx_int_t
ngx_http_iojs_subrequest(ngx_http_request_t *r, iojsFromJS *cmd)
{
    iojsSubrequest              *data;
    ngx_http_iojs_ctx_t         *sr_ctx;
    ngx_http_request_t          *sr;
    ngx_http_post_subrequest_t  *psr;
    ngx_http_request_body_t     *rb;
    ngx_buf_t                   *b;
    ngx_http_core_main_conf_t   *cmcf;
    ngx_str_t                    sr_uri;
    ngx_str_t                    args;
    ngx_uint_t                   flags = 0;
    ngx_str_t                    sr_body;

    data = (iojsSubrequest *)cmd->data;
    if (data == NULL) {
        return NGX_ERROR;
    }

    psr = ngx_palloc(r->pool, sizeof(ngx_http_post_subrequest_t));
    if (psr == NULL) {
        return NGX_ERROR;
    }

    psr->handler = ngx_http_iojs_subrequest_done;

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

    if (ngx_http_iojs_init_subrequest_headers(sr, sr_body.len) == NGX_ERROR) {
        return NGX_ERROR;
    }

    sr_ctx = ngx_http_iojs_create_ctx(sr);

    if (sr_ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_http_set_ctx(sr, sr_ctx, ngx_http_iojs_module);

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
    }

    ngx_http_run_posted_requests(sr->connection);

    return NGX_OK;
}


static void
ngx_http_iojs_receive(ngx_event_t *ev)
{
    // Something is available to read.
    iojsFromJS *cmd;

    cmd = iojsFromJSRecv();
    if (cmd != NULL) {
        ngx_int_t            rc;
        iojsContext         *js_ctx = cmd->jsCtx;
        ngx_http_request_t  *r = js_ctx == NULL ?
                NULL
                :
                (ngx_http_request_t *)js_ctx->r;

        switch (cmd->type) {
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
                    r, ngx_http_iojs_read_request_body
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

                if (ngx_http_send_header(r) != NGX_OK) {
                    ngx_http_finalize_request(r, NGX_ERROR);
                    return;
                }

                break;

            case FROM_JS_RESPONSE_BODY:
                if (js_ctx == NULL || js_ctx->wait == 0)
                    break;

                if (cmd->data) {
                    iojsString  *s = (iojsString *)cmd->data;
                    ngx_http_iojs_send_chunk(r, s->data, s->len, 0);
                } else {
                    // Got end of the response mark.
                    // This one is for r->main->count++ in ngx_http_iojs_handler.
                    r->main->count--;

                    js_ctx->wait--;
                    js_ctx->rootCtx->wait--;

                    if (r != r->main && js_ctx->wait == 0) {
                        // Finalize the request.
                        ngx_http_iojs_send_chunk(r, NULL, 0, 1);
                        ngx_http_finalize_request(r, NGX_OK);
                    }

                    if (r->main->count == 1 && js_ctx->rootCtx->wait == 0) {
                        // No subrequests left, finalize the main request.
                        ngx_http_iojs_send_chunk(r->main, NULL, 0, 1);
                        ngx_http_finalize_request(r->main, NGX_DONE);
                    }
                }

                break;

            case FROM_JS_BEGIN_SUBREQUEST:
                if (js_ctx == NULL || js_ctx->wait == 0)
                    break;

                if (ngx_http_iojs_subrequest(r, cmd) == NGX_ERROR) {
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
                    ngx_http_iojs_send_chunk(r, NULL, 0, 1);
                    ngx_http_finalize_request(r, NGX_OK);
                }

                if (r->main->count == 1 && js_ctx->rootCtx->wait == 0) {
                    // No subrequests left, finalize the main request.
                    ngx_http_iojs_send_chunk(r->main, NULL, 0, 1);
                    ngx_http_finalize_request(r->main, NGX_DONE);
                }

                break;

            case FROM_JS_LOG:
                break;

            case FROM_JS_EXIT_MAIN:
                break;
        }

        iojsFromJSFree(cmd);
    }
}


static ngx_int_t
ngx_http_iojs_init(ngx_cycle_t *cycle)
{
    dd("start");

    ngx_uint_t                 i;
    ngx_http_iojs_loc_conf_t **pxlcf = jsLocations.elts;
    ngx_http_iojs_loc_conf_t  *xlcf;
    ngx_str_t                 *filename;
    iojsJS                    *scripts;
    iojsJS                    *script;
    int                       fd = -1;
    int                       rc;

    if (jsLocations.nelts <= 0) {
        dd("no scripts, do not start iojs");
        return NGX_OK;
    }

    scripts = (iojsJS *)ngx_palloc(cycle->pool,
                                   sizeof(iojsJS) * jsLocations.nelts);
    if (scripts == NULL) {
        return NGX_ERROR;
    }

    for (i = 0; i < jsLocations.nelts; i++) {
        xlcf = pxlcf[i];

        if (xlcf->root.len) {
            rc = ngx_get_full_name(cycle->pool, &xlcf->root, &xlcf->js);
        } else {
            rc = ngx_conf_full_name(cycle, &xlcf->js, 0);
        }

        if (rc != NGX_OK) {
            return NGX_ERROR;
        }

        dd("add script `%s`", (&xlcf->js)->data);

        xlcf->js_index = i;

        script = &scripts[i];
        filename = &xlcf->js;
        script->filename = (char *)filename->data;
        script->len = filename->len;
        script->index = i;
    }

    iojsJSArray s;
    s.js = scripts;
    s.len = jsLocations.nelts;
    rc = iojsStart(&s, &fd);

    ngx_array_destroy(&jsLocations);
    ngx_pfree(cycle->pool, scripts);

    if (rc) {
        ngx_log_error(NGX_LOG_ERR, cycle->log, 0,
                      "Failed to start iojs (%d)", rc);
        return NGX_ERROR;
    }

    dd("started");

    memset(&jsPipe, 0, sizeof(jsPipe));
    memset(&jsPipeEv, 0, sizeof(jsPipeEv));

    jsPipe.number = ngx_atomic_fetch_add(ngx_connection_counter, 1);
    jsPipe.fd = fd;
    jsPipe.pool = cycle->pool;
    jsPipe.log = cycle->log;
    jsPipe.write = &jsPipeEv;

    jsPipeEv.data = &jsPipe;
    jsPipeEv.handler = ngx_http_iojs_receive;
    jsPipeEv.log = cycle->log;

    dd("add pipe handler");
    ngx_add_event(&jsPipeEv, NGX_READ_EVENT, 0);

    return NGX_OK;
}


static void
ngx_http_iojs_free(ngx_cycle_t *cycle)
{
    dd("stop");
    //iojsFree();
}


static ngx_int_t
ngx_http_iojs_preconf(ngx_conf_t *cf)
{
    dd("preconfiguration");
    return ngx_array_init(&jsLocations, cf->pool, 1,
                          sizeof(ngx_http_iojs_loc_conf_t*));
}


ngx_inline static ngx_int_t
ngx_http_iojs_process_headers(ngx_http_request_t *r, ngx_http_iojs_ctx_t *ctx)
{
    return NGX_OK;
}


ngx_inline static ngx_int_t
ngx_http_iojs_aggregate_headers(ngx_http_request_t *r, unsigned sr,
                                ngx_str_t ***ret)
{
    ngx_uint_t                 len;
    ngx_str_t                **headers;
    ngx_uint_t                 i;
    ngx_uint_t                 j;
    ngx_list_part_t           *part;
    ngx_table_elt_t           *header;

    if (sr) {
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
ngx_http_iojs_header_filter(ngx_http_request_t *r)
{
    ngx_http_iojs_ctx_t         *ctx;
    ngx_str_t                  **headers;

    ctx = ngx_http_get_module_ctx(r, ngx_http_iojs_module);

    if (ctx == NULL || ctx->skip_filter) {
        return ngx_http_next_header_filter(r);
    }

    dd("header filter (%p, %.*s)", r, (int)r->uri.len, r->uri.data);

    if (ngx_http_iojs_aggregate_headers(r, 1, &headers) != NGX_OK)
        return NGX_ERROR;

    if (iojsSubrequestHeaders(ctx->js_ctx, (iojsString **)headers))
        return NGX_ERROR;

    return NGX_OK;
}


static ngx_int_t
ngx_http_iojs_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_http_iojs_ctx_t  *ctx;
    ngx_chain_t          *cl;
    ngx_int_t             rc;

    ctx = ngx_http_get_module_ctx(r, ngx_http_iojs_module);

    if (ctx == NULL || ctx->skip_filter) {
        return ngx_http_next_body_filter(r, in);
    }

    // It's a subrequest response body. Eating it up and passing to iojs.
    for (cl = in; cl; cl = cl->next) {
        if (!ctx->refused && cl->buf->pos != NULL) {
            rc = iojsChunk(ctx->js_ctx,
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
ngx_http_iojs_postconf(ngx_conf_t *cf)
{
    dd("postconfiguration");

    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_iojs_header_filter;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_iojs_body_filter;

    return NGX_OK;
}


static void
ngx_http_iojs_cleanup_request(void *data)
{
    dd("request cleanup");
}


static ngx_int_t
ngx_http_iojs_handler(ngx_http_request_t *r) {
    ngx_http_iojs_ctx_t       *ctx;
    ngx_http_iojs_loc_conf_t  *conf;
    ngx_pool_cleanup_t        *cln;
    ngx_int_t                  rc;
    ngx_http_iojs_param_t     *params;
    ngx_uint_t                 len;
    ngx_uint_t                 i;
    ngx_uint_t                 j;
    ngx_str_t                 *param;
    ngx_str_t                **iojs_params;
    ngx_str_t                **iojs_headers;

    dd("begin (r: %p, main: %p)", r, r->main);

    conf = ngx_http_get_module_loc_conf(r, ngx_http_iojs_module);
    if (conf == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_iojs_module);

    if (ctx == NULL) {
        ctx = ngx_http_iojs_create_ctx(r);
        if (ctx == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        ctx->skip_filter = 1;

        ngx_http_set_ctx(r, ctx, ngx_http_iojs_module);
    } else {
        ctx->skip_filter = 0;
    }

    len = conf->params != NULL ? conf->params->nelts : 0;

    if (len > 0) {
        // Allocate memory for a NULL-terminated list of pointers to names
        // and values and for the values.
        iojs_params = ngx_palloc(r->pool,
                                 sizeof(ngx_str_t *) * (len * 2 + 1) +
                                 sizeof(ngx_str_t) * len);

        if (iojs_params == NULL)
            return NGX_HTTP_INTERNAL_SERVER_ERROR;

        params = conf->params->elts;
        param = (ngx_str_t *)&iojs_params[len * 2 + 1];
        j = 0;

        for (i = 0; i < len; i++) {
            if (ngx_http_complex_value(r, &params[i].value, param) != NGX_OK)
                return NGX_HTTP_INTERNAL_SERVER_ERROR;

            // ngx_http_complex_value gives zero char in the end sometimes.
            if (param->len > 0 && param->data[param->len - 1] == 0)
                param->len--;

            iojs_params[j++] = &params[i].name;
            iojs_params[j++] = param;

            param = (ngx_str_t *)(((char *)param) + sizeof(ngx_str_t));
        }

        // NULL-terminate.
        iojs_params[j] = NULL;
    } else {
        iojs_params = NULL;
    }

    if (ngx_http_iojs_aggregate_headers(r, 0, &iojs_headers) != NGX_OK)
        return NGX_HTTP_INTERNAL_SERVER_ERROR;

    r->headers_out.status = NGX_HTTP_OK;

    cln = ngx_pool_cleanup_add(r->pool, 0);
    if (cln == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    cln->handler = ngx_http_iojs_cleanup_request;
    cln->data = ctx;

    rc = ngx_http_iojs_process_headers(r, ctx);
    if (rc == NGX_ERROR) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    r->main->count++;
    ctx->js_ctx->wait++;
    ctx->js_ctx->rootCtx->wait++;

    rc = iojsCall(conf->js_index, ctx->js_ctx,
                  (iojsString **)iojs_headers,
                  (iojsString **)iojs_params);
    if (rc) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    return NGX_DONE;
}


static char *
ngx_http_iojs(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_iojs_loc_conf_t  *xlcf = conf;
    ngx_http_core_loc_conf_t  *clcf;
    ngx_str_t                 *value;
    ngx_http_iojs_loc_conf_t **pxlcf;

    value = cf->args->elts;

    if (xlcf->js.data) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "Duplicate iojs instruction");
        return NGX_CONF_ERROR;
    }

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    if (clcf == NULL) {
        return NGX_CONF_ERROR;
    }

    clcf->handler = ngx_http_iojs_handler;

    xlcf->js.data = ngx_pstrdup(cf->pool, &value[1]);
    xlcf->js.len = (&value[1])->len;

    if (xlcf->js.data == NULL) {
        return NGX_CONF_ERROR;
    }

    pxlcf = ngx_array_push(&jsLocations);
    if (pxlcf == NULL) {
        return NGX_CONF_ERROR;
    }
    
    *pxlcf = xlcf;
    
    return NGX_CONF_OK;
}


static char *
ngx_http_iojs_param(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_iojs_loc_conf_t          *xlcf = conf;

    ngx_http_iojs_param_t             *param;
    ngx_http_compile_complex_value_t   ccv;
    ngx_str_t                         *value;

    value = cf->args->elts;

    if (xlcf->params == NULL) {
        xlcf->params = ngx_array_create(cf->pool, 2,
                                        sizeof(ngx_http_iojs_param_t));
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
ngx_http_iojs_create_conf(ngx_conf_t *cf)
{
    ngx_http_iojs_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_iojs_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    return conf;
}


static char *
ngx_http_iojs_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_iojs_loc_conf_t  *prev = parent;
    ngx_http_iojs_loc_conf_t  *conf = child;
    ngx_http_iojs_param_t     *prevparams, *params, *param;
    ngx_uint_t                 i, j;
    int                        add;

    if (conf->root.len == 0) {
        conf->root.data = prev->root.data;
        conf->root.len = prev->root.len;
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
ngx_http_iojs_root(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_iojs_loc_conf_t          *xlcf = conf;
    ngx_str_t                         *value;

    value = cf->args->elts;

    xlcf->root.data = ngx_pstrdup(cf->pool, &value[1]);
    xlcf->root.len = (&value[1])->len;

    dd("setting root to `%s`", xlcf->root.data);

    return NGX_CONF_OK;
}


static char *
ngx_http_iojs_args(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                         *value;

    value = cf->args->elts;

    jsArgs.data = ngx_pstrdup(cf->pool, &value[1]);
    jsArgs.len = (&value[1])->len;

    dd("setting arguments to `%s`", jsArgs.data);

    return NGX_CONF_OK;
}
