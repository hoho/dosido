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

#include <string>
#include <vector>


typedef enum {
    IOJS_CALL = 1,
    IOJS_CHUNK,
    IOJS_REQUEST_ERROR,
    IOJS_RESPONSE_ERROR,
    IOJS_FREE_CALLBACK,
    IOJS_EXIT
} iojsToJSCommandType;


typedef enum {
    BY_JS_INIT_DESTRUCTOR = 0,
    BY_JS_READ_REQUEST_BODY = 1,
    BY_JS_RESPONSE_HEADERS = 2,
    BY_JS_RESPONSE_BODY = 3,
    BY_JS_SUBREQUEST = 4
} iojsByJSCommandType;


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
    iojsContext  *jsCtx;
    int           index;
    void         *headers;
} iojsCallCmd;


typedef struct {
    IOJS_TO_JS_HEAD
    iojsContext  *jsCtx;
    void         *jsCallback;
    iojsString    chunk;
} iojsChunkCmd;


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
