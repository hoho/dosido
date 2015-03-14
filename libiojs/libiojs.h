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
    FROM_JS_LOG,
    FROM_JS_EXIT_MAIN
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


typedef struct {
    void                   *r;
    unsigned                refused:1;
    unsigned                done:1;
    int64_t                 refCount;
    iojsAtomicFetchAdd      afa;
    void                   *_p; // To hold a persistent handle of destroy
                                // indicator.
    void                   *jsCallback;
    void                   *jsSubrequestCallback;
} iojsContext;


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


#ifdef __cplusplus
extern "C" {
#endif


LIBIOJSPUBFUN int LIBIOJSCALL
        iojsStart                (iojsJSArray *scripts, int *fd);
LIBIOJSPUBFUN void LIBIOJSCALL
        iojsStop                 (void);

LIBIOJSPUBFUN iojsFromJS* LIBIOJSCALL
        iojsFromJSRecv           (void);
LIBIOJSPUBFUN void LIBIOJSCALL
        iojsFromJSFree           (iojsFromJS *cmd);

LIBIOJSPUBFUN iojsContext * LIBIOJSCALL
        iojsContextCreate        (void *r, void *ctx, iojsAtomicFetchAdd afa);
LIBIOJSPUBFUN void LIBIOJSCALL
        iojsContextAttemptFree   (iojsContext *jsCtx);

LIBIOJSPUBFUN int LIBIOJSCALL
        iojsCall                 (int index, iojsContext *jsCtx);
LIBIOJSPUBFUN int LIBIOJSCALL
        iojsChunk                (iojsContext *jsCtx, char *data, size_t len,
                                  unsigned last, unsigned sr);

#ifdef __cplusplus
}
#endif

#endif /* __LIBIOJS_H__ */
