/*
 * Copyright Marat Abdullin (https://github.com/hoho)
 */

#ifndef __LIBIOJS_H__
#define __LIBIOJS_H__


#include <libiojsExports.h>


typedef enum {
    IOJS_RESPONSE_HEADERS = 1,
    IOJS_RESPONSE_DATA,
    IOJS_SUBREQUEST_HEADERS,
    IOJS_SUBREQUEST_BODY,
    IOJS_LOG,
    IOJS_EXIT_MAIN
} iojsFromJSCommandType;


typedef struct {
    char        *filename;
    size_t       len;
    int          id;
} iojsJS;


typedef struct {
    iojsFromJSCommandType   type;
    void                   *data;
} iojsFromJS;


typedef int (*iojsAtomicFetchAdd)(int value, int add);


typedef struct {
    void                   *r;
    void                   *ctx;
    unsigned                refused:1;
    int                     refCount;
    iojsAtomicFetchAdd      afa;
} iojsContext;


#ifdef __cplusplus
extern "C" {
#endif


LIBIOJSPUBFUN int LIBIOJSCALL
        iojsStart                (iojsJS *scripts, size_t len, int *fd);
LIBIOJSPUBFUN void LIBIOJSCALL
        iojsStop                 (void);

LIBIOJSPUBFUN iojsFromJS* LIBIOJSCALL
        iojsFromJSRecv           (void);
LIBIOJSPUBFUN void LIBIOJSCALL
        iojsFromJSFree           (iojsFromJS *cmd);

LIBIOJSPUBFUN iojsContext * LIBIOJSCALL
        iojsContextCreate        (void *r, void *ctx, iojsAtomicFetchAdd afa);
LIBIOJSPUBFUN void LIBIOJSCALL
        iojsContextAttemptFree   (iojsContext *context);

LIBIOJSPUBFUN int LIBIOJSCALL
        iojsRequest              (int id);


#ifdef __cplusplus
}
#endif

#endif /* __LIBIOJS_H__ */
