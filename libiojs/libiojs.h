/*
 * Copyright Marat Abdullin (https://github.com/hoho)
 */

#ifndef __LIBIOJS_H__
#define __LIBIOJS_H__


#include <libiojsExports.h>


typedef enum {
    IOJS_JS_CALLBACK = 0,
    IOJS_READ_REQUEST_BODY,
    IOJS_RESPONSE_HEADERS,
    IOJS_RESPONSE_BODY,
    IOJS_SUBREQUEST,
    IOJS_LOG,
    IOJS_EXIT_MAIN
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
                                  short last);

#ifdef __cplusplus
}
#endif

#endif /* __LIBIOJS_H__ */
