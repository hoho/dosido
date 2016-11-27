/*
 * Copyright Marat Abdullin (https://github.com/hoho)
 */

#ifndef __LIBNODEJS_H__
#define __LIBNODEJS_H__

#include <stdlib.h>
#include <libnodejsExports.h>


typedef enum {
    FROM_JS_ERROR = 1,
    FROM_JS_INIT_CALLBACK,
    FROM_JS_READ_REQUEST_BODY,
    FROM_JS_RESPONSE_HEADERS,
    FROM_JS_RESPONSE_BODY,
    FROM_JS_BEGIN_SUBREQUEST,
    FROM_JS_SUBREQUEST_DONE,
    FROM_JS_LOG
} nodejsFromJSCommandType;


typedef struct {
    char        *filename;
    size_t       len;
    int          index;
} nodejsJS;


typedef struct {
    nodejsJS      *js;
    size_t       len;
} nodejsJSArray;


// Should be the same to ngx_str_t structure.
typedef struct {
    size_t                  len;
    char                   *data;
} nodejsString;


typedef int64_t (*nodejsAtomicFetchAdd)(int64_t *value, int64_t add);
typedef void (*nodejsFreeFunc)(void *data);
typedef void (*nodejsLogger)(unsigned level, const char *fmt, ...);


typedef struct _nodejsContext nodejsContext;
struct _nodejsContext {
    unsigned                  acceptedByJS:1;
    void                     *r;
    unsigned                  refused:1;
    size_t                    wait;
    int64_t                   refCount;
    nodejsAtomicFetchAdd      afa;
    void                     *_p; // To hold a persistent handle of destroy
                                  // indicator.
    void                     *jsCallback;
    void                     *jsSubrequestCallback;
    nodejsContext            *rootCtx;
};


typedef struct {
    nodejsFromJSCommandType   type;
    nodejsContext            *jsCtx;
    void                     *data;
    nodejsFreeFunc            free;
    void                     *jsCallback;
} nodejsFromJS;


typedef struct {
    nodejsString     *strings;
    unsigned long     len;
    int64_t           statusCode;
    nodejsString      statusMessage;
} nodejsHeaders;


typedef struct {
    nodejsString      url;
    nodejsString      method;
    nodejsString      body;
    nodejsHeaders     headers;
} nodejsSubrequest;


// Should match with the level from ngx_log.h.
#define NODEJS_LOG_STDERR            0
#define NODEJS_LOG_EMERG             1
#define NODEJS_LOG_ALERT             2
#define NODEJS_LOG_CRIT              3
#define NODEJS_LOG_ERR               4
#define NODEJS_LOG_WARN              5
#define NODEJS_LOG_NOTICE            6
#define NODEJS_LOG_INFO              7
#define NODEJS_LOG_DEBUG             8


#ifdef __cplusplus
extern "C" {
#endif


LIBNODEJSPUBFUN int LIBNODEJSCALL
        nodejsStart                (nodejsLogger logger, int *fd);
LIBNODEJSPUBFUN void LIBNODEJSCALL
        nodejsStop                 (void);
LIBNODEJSPUBFUN int LIBNODEJSCALL
        nodejsNextScriptsVersion   (void);

LIBNODEJSPUBFUN nodejsFromJS* LIBNODEJSCALL
        nodejsFromJSRecv           (void);
LIBNODEJSPUBFUN void LIBNODEJSCALL
        nodejsFromJSFree           (nodejsFromJS *cmd);

LIBNODEJSPUBFUN nodejsContext * LIBNODEJSCALL
        nodejsContextCreate        (void *r, nodejsContext *rootCtx,
                                    nodejsAtomicFetchAdd afa);
LIBNODEJSPUBFUN void LIBNODEJSCALL
        nodejsContextAttemptFree   (nodejsContext *jsCtx);

LIBNODEJSPUBFUN int LIBNODEJSCALL
        nodejsCall                 (nodejsString *module, nodejsContext *jsCtx,
                                    nodejsString *method, nodejsString *uri,
                                    nodejsString *httpProtocol,
                                    nodejsString **headers,
                                    nodejsString **params);
LIBNODEJSPUBFUN int LIBNODEJSCALL
        nodejsChunk                (nodejsContext *jsCtx, char *data,
                                    size_t len, unsigned last, unsigned sr);
LIBNODEJSPUBFUN int LIBNODEJSCALL
        nodejsSubrequestHeaders    (nodejsContext *jsCtx, int status,
                                    nodejsString *statusMsg,
                                    nodejsString **headers);

#ifdef __cplusplus
}
#endif

#endif /* __LIBNODEJS_H__ */
