/*
 * Copyright (C) Marat Abdullin
 */

#define DDEBUG 1


#ifndef DDEBUG
#define DDEBUG 0
#endif

#include "ddebug.h"
#include "ngx_string.h"
#include "ngx_array.h"

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <unistd.h>
#include <libiojs.h>
#include <string.h>


typedef struct {
    ngx_str_t                  name;
    ngx_http_complex_value_t   value;
} ngx_http_iojs_param_t;


typedef struct {
    ngx_str_t                  js;
    int                        jsId;
    ngx_array_t               *params;       /* ngx_http_iojs_param_t */
    ngx_str_t                  root;
} ngx_http_iojs_loc_conf_t;


static ngx_connection_t          jsPipe;
static ngx_event_t               jsPipeEv;
static ngx_str_t                 jsArgs = ngx_null_string;
static ngx_array_t               jsLocations;


static ngx_int_t   ngx_http_iojs_init          (ngx_cycle_t *cycle);
static void        ngx_http_iojs_free          (ngx_cycle_t *cycle);
static ngx_int_t   ngx_http_iojs_preconf       (ngx_conf_t *cf);
static ngx_int_t   ngx_http_iojs_postconf      (ngx_conf_t *cf);
//static ngx_int_t   ngx_http_iojs_post_sr       (ngx_http_request_t *r,
//                                                void *data, ngx_int_t rc);

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
    { ngx_string("iojs_param"),
      NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF
                         | NGX_HTTP_LIF_CONF | NGX_CONF_TAKE2,
      ngx_http_iojs_param,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("iojs_root"),
      NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF
                         | NGX_HTTP_LIF_CONF | NGX_CONF_TAKE1,
      ngx_http_iojs_root,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("iojs_args"),
      NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE1,
      ngx_http_iojs_args,
      NGX_HTTP_MAIN_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("iojs"),
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
    //xrltContextPtr        xctx;
    void                 *xctx;
    size_t                id;
    ngx_http_iojs_ctx_t  *main_ctx;
    unsigned              headers_sent:1;
    unsigned              run_post_subrequest:1;
    unsigned              refused:1;
};


static void
ngx_http_iojs_cleanup_context(void *data)
{
    dd("context cleanup");
}


ngx_inline static ngx_http_iojs_ctx_t *
ngx_http_iojs_create_ctx(ngx_http_request_t *r, size_t id) {
    ngx_http_iojs_ctx_t       *ctx;
    ngx_http_iojs_loc_conf_t  *conf;
    //ngx_uint_t                 i, j;
    //ngx_http_iojs_param_t     *params;
    //char                     **iojsParams;

    conf = ngx_http_get_module_loc_conf(r, ngx_http_iojs_module);

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_iojs_ctx_t));
    if (ctx == NULL) {
        return NULL;
    }

    if (id == 0) {
        ngx_pool_cleanup_t  *cln;

        cln = ngx_pool_cleanup_add(r->pool, 0);
        if (cln == NULL) {
            return NULL;
        }

        dd("context creation");

        if (conf->params != NULL && conf->params->nelts > 0) {
            //params = conf->params->elts;

            /*iojsParams = ngx_palloc(
                r->pool, sizeof(char *) * (conf->params->nelts * 2 + 1)
            );

            j = 0;

            for (i = 0; i < conf->params->nelts; i++) {
                iojsParams[j++] = strndup(params[i].name.data,
                                          params[i].name.len);
                iojsParams[j++] = strndup(params[i].value.value.data,
                                          params[i].value.value.len);
            }

            iojsParams[j] = NULL;*/
        } else {
            //iojsParams = NULL;
        }

        //ctx->xctx = xrltContextCreate(conf->sheet, xrltparams);
        ctx->id = 0;
        ctx->main_ctx = ctx;

        if (ctx->xctx == NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "Failed to create iojs context");
        }

        cln->handler = ngx_http_iojs_cleanup_context;
        cln->data = ctx->xctx;
    } else {
        ngx_http_iojs_ctx_t  *main_ctx;

        main_ctx = ngx_http_get_module_ctx(r->main, ngx_http_iojs_module);

        if (main_ctx == NULL || main_ctx->xctx == NULL) {
            return NULL;
        }

        ctx->xctx = main_ctx->xctx;
        ctx->id = id;
        ctx->main_ctx = main_ctx;
    }

    return ctx;
}


static void
ngx_http_iojs_receive(ngx_event_t *ev)
{
    // Something is available to read.
    iojsFromJS *cmd;

    cmd = iojsFromJSRecv();
    if (cmd != NULL) {
        fprintf(stderr, "Recv from JS: %d\n", cmd->type);
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

        xlcf->jsId = i;

        script = &scripts[i];
        filename = &xlcf->js;
        script->filename = (char *)filename->data;
        script->len = filename->len;
        script->id = i;
    }

    rc = iojsStart(scripts, jsLocations.nelts, &fd);

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


static ngx_int_t
ngx_http_iojs_postconf(ngx_conf_t *cf)
{
    dd("postconfiguration");
    /*ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_iojs_header_filter;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_iojs_body_filter;*/

    return NGX_OK;
}


/*
static void
ngx_http_iojs_post_request_body(ngx_http_request_t *r)
{
    ngx_http_iojs_ctx_t  *ctx;
//    ngx_chain_t          *cl;
//    ngx_int_t             rc;
    //xrltString            s;

    if (r != r->main) { return; }

    ctx = ngx_http_get_module_ctx(r, ngx_http_iojs_module);

    dd("post request body (r: %p)", r);

    if (r->request_body->bufs == NULL) {
        s.data = NULL;
        s.len = 0;

        rc = ngx_http_xrlt_transform_body(r, ctx, 0, &s, TRUE, FALSE);

        if (rc == NGX_ERROR) {
            ngx_http_finalize_request(r, NGX_ERROR);

            return;
        }
    } else {
        for (cl = r->request_body->bufs; cl; cl = cl->next) {
            s.data = (char *)cl->buf->pos;
            s.len = cl->buf->last - cl->buf->pos;

            rc = ngx_http_xrlt_transform_body(r, ctx, 0, &s, cl->buf->last_buf,
                    FALSE);

            if (rc == NGX_ERROR) {
                ngx_http_finalize_request(r, NGX_ERROR);

                return;
            }

            cl->buf->pos = cl->buf->last;
            cl->buf->file_pos = cl->buf->file_last;
        }
    }
}    */


static void
ngx_http_iojs_cleanup_request(void *data)
{
    dd("request cleanup");
}


static ngx_int_t
ngx_http_iojs_handler(ngx_http_request_t *r) {
    ngx_http_iojs_ctx_t  *ctx;
    ngx_pool_cleanup_t   *cln;
    //ngx_int_t             rc;

    dd("begin (main: %p)", r->main);

    ctx = ngx_http_get_module_ctx(r, ngx_http_iojs_module);
    if (ctx == NULL) {
        ctx = ngx_http_iojs_create_ctx(r, 0);
        if (ctx == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        ngx_http_set_ctx(r, ctx, ngx_http_iojs_module);
    }

    r->headers_out.status = NGX_HTTP_OK;

    cln = ngx_pool_cleanup_add(r->pool, 0);
    if (cln == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    cln->handler = ngx_http_iojs_cleanup_request;
    cln->data = ctx;

    /*rc = ngx_http_iojs_transform_headers(r, ctx);
    if (rc == NGX_ERROR) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (rc == NGX_DONE) {
        return NGX_OK;
    }

    if (ctx->xctx->bodyData != NULL) {
        rc = ngx_http_read_client_request_body(
                r, ngx_http_xrlt_post_request_body
        );

        if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
            return rc;
        }

        if (ctx->xctx->cur & XRLT_STATUS_DONE) {
            ngx_http_xrlt_wev_handler(r);

            return NGX_OK;
        }
    } else {
        r->main->count++;
    }*/

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


