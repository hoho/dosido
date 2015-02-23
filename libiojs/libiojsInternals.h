/*
 * Copyright Marat Abdullin (https://github.com/hoho)
 */

#ifndef __LIBIOJSINTERNALS_H__
#define __LIBIOJSINTERNALS_H__

#include "node.h"
#include "libiojs.h"
#include "uv.h"
#include "node_natives.h"
#include "v8.h"


typedef enum {
    IOJS_REQUEST_HEADERS = 1,
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
        iojsClosePipes   (void);


void
        iojsToJSSend     (iojsToJS *cmd);
void
        iojsFromJSSend   (iojsFromJS *cmd);
iojsToJS*
        iojsToJSRecv     (void);
void
        iojsToJSFree     (iojsToJS *cmd);

#endif /* __LIBIOJSINTERNALS_H__ */
