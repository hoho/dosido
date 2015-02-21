/*
 * Copyright Marat Abdullin (https://github.com/hoho)
 */

#ifndef __LIBIOJSINTERNALS_H__
#define __LIBIOJSINTERNALS_H__

#include <libiojs.h>

typedef enum {
    IOJS_ADD_SCRIPT = 1,
    IOJS_REQUEST_HEADERS,
    IOJS_REQUEST_BODY,
    IOJS_SUBREQUEST_RESPONSE_HEADERS,
    IOJS_SUBREQUEST_RESPONSE_DATA,
    IOJS_EXIT_JS
} iojsToJSCommandType;


#define IOJS_TO_JS_HEAD                                                      \
    iojsToJSCommandType   type;

#define IOJS_CHECK_OUT_OF_MEMORY(ptr)                                        \
    if (ptr == NULL) {                                                       \
        fprintf(stderr, "Out of memory\n");                                  \
        abort();                                                             \
    }

typedef struct {
    IOJS_TO_JS_HEAD
} iojsToJS;


typedef struct {
    IOJS_TO_JS_HEAD
    char  *filename;
    int    id;
} iojsCmdAddJS;


void
        iojsToJSSend     (iojsToJS *cmd);
void
        iojsFromJSSend   (iojsFromJS *cmd);
iojsToJS*
        iojsToJSRecv     (void);
void
        iojsToJSFree     (iojsToJS *cmd);

#endif /* __LIBIOJSINTERNALS_H__ */
