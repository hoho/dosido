/*
 * Copyright Marat Abdullin (https://github.com/hoho)
 */

#ifndef __LIBIOJS_H__
#define __LIBIOJS_H__


#include <libiojsExports.h>


typedef enum {
    FROM_JS_INIT_CALLBACK = 1,
    FROM_JS_READ_REQUEST_BODY,
    FROM_JS_RESPONSE_HEADERS,
    FROM_JS_RESPONSE_BODY,
    FROM_JS_BEGIN_SUBREQUEST,
    FROM_JS_SUBREQUEST_DONE,
    FROM_JS_LOG
} iojsFromJSCommandType;


typedef struct {
    char        *filename;
    size_t       len;
    int          index;
} iojsJS;


typedef struct {
    iojsJS      *js;
    size_t       len;
} iojsJSArray;


// Should be the same to ngx_str_t structure.
typedef struct {
    size_t                  len;
    char                   *data;
} iojsString;


typedef int64_t (*iojsAtomicFetchAdd)(int64_t *value, int64_t add);
typedef void (*iojsFreeFunc)(void *data);
typedef void (*iojsLogger)(unsigned level, const char *fmt, ...);


typedef struct _iojsContext iojsContext;
struct _iojsContext {
    void                   *r;
    unsigned                refused:1;
    size_t                  wait;
    int64_t                 refCount;
    iojsAtomicFetchAdd      afa;
    void                   *_p; // To hold a persistent handle of destroy
                                // indicator.
    void                   *jsCallback;
    void                   *jsSubrequestCallback;
    iojsContext            *rootCtx;
};


typedef struct {
    iojsFromJSCommandType   type;
    iojsContext            *jsCtx;
    void                   *data;
    iojsFreeFunc            free;
    void                   *jsCallback;
} iojsFromJS;


typedef struct {
    iojsString      url;
    iojsString      method;
    iojsString      body;
    iojsString     *headers;
} iojsSubrequest;


// Should match with the level from ngx_log.h.
#define IOJS_LOG_STDERR            0
#define IOJS_LOG_EMERG             1
#define IOJS_LOG_ALERT             2
#define IOJS_LOG_CRIT              3
#define IOJS_LOG_ERR               4
#define IOJS_LOG_WARN              5
#define IOJS_LOG_NOTICE            6
#define IOJS_LOG_INFO              7
#define IOJS_LOG_DEBUG             8


#ifdef __cplusplus
extern "C" {
#endif


LIBIOJSPUBFUN int LIBIOJSCALL
        iojsStart                (iojsLogger logger,
                                  iojsJSArray *scripts, int *fd);

LIBIOJSPUBFUN iojsFromJS* LIBIOJSCALL
        iojsFromJSRecv           (void);
LIBIOJSPUBFUN void LIBIOJSCALL
        iojsFromJSFree           (iojsFromJS *cmd);

LIBIOJSPUBFUN iojsContext * LIBIOJSCALL
        iojsContextCreate        (void *r, iojsContext *rootCtx,
                                  iojsAtomicFetchAdd afa);
LIBIOJSPUBFUN void LIBIOJSCALL
        iojsContextAttemptFree   (iojsContext *jsCtx);

LIBIOJSPUBFUN int LIBIOJSCALL
        iojsCall                 (int index, iojsContext *jsCtx,
                                  iojsString *method, iojsString *uri,
                                  iojsString *httpProtocol,
                                  iojsString **headers, iojsString **params);
LIBIOJSPUBFUN int LIBIOJSCALL
        iojsChunk                (iojsContext *jsCtx, char *data, size_t len,
                                  unsigned last, unsigned sr);
LIBIOJSPUBFUN int LIBIOJSCALL
        iojsSubrequestHeaders    (iojsContext *jsCtx, int status,
                                  iojsString *statusMsg, iojsString **headers);

#ifdef __cplusplus
}
#endif

#endif /* __LIBIOJS_H__ */
