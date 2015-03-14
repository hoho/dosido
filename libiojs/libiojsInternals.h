/*
 * Copyright Marat Abdullin (https://github.com/hoho)
 */

#ifndef __LIBIOJSINTERNALS_H__
#define __LIBIOJSINTERNALS_H__

#include "node.h"
#include "v8.h"
#include "uv.h"
#include "libiojs.h"

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
    BY_JS_INIT_DESTRUCTOR   = 1,
    BY_JS_READ_REQUEST_BODY = 2,
    BY_JS_RESPONSE_HEADERS  = 3,
    BY_JS_RESPONSE_BODY     = 4,
    BY_JS_SUBREQUEST        = 5,
    BY_JS_SUBREQUEST_DONE   = 6
} iojsByJSCommandType;


typedef enum {
    TO_JS_CALLBACK_CHUNK              = 7,
    TO_JS_CALLBACK_SUBREQUEST_HEADERS = 8,
    TO_JS_CALLBACK_REQUEST_ERROR      = 9,
    TO_JS_CALLBACK_RESPONSE_ERROR     = 10
} iojsToJSCallbackCommandType;


typedef enum {
    SR_URL      = 2,
    SR_METHOD   = 3,
    SR_HEADERS  = 4,
    SR_BODY     = 5,
    SR_CALLBACK = 6
} iojsSrArgument;


#define IOJS_CHECK_OUT_OF_MEMORY(ptr)                                        \
    if (ptr == NULL) {                                                       \
        fprintf(stderr, "Out of memory\n");                                  \
        abort();                                                             \
    }


typedef struct _iojsToJS {
    iojsToJSCommandType   type;
} iojsToJS;


typedef struct _iojsCallCmd : _iojsToJS {
    iojsContext  *jsCtx;
    int           index;
    void         *headers;
} iojsCallCmd;


typedef struct iojsChunkCmd : _iojsToJS {
    iojsContext  *jsCtx;
    iojsString    chunk;
    unsigned      last:1;
    unsigned      sr:1;
} iojsChunkCmd;


typedef struct _iojsFreeCallbackCmd : _iojsToJS {
    void  *cb;
} iojsFreeCallbackCmd;


typedef v8::Local<v8::Value> (*ExecuteStringFunc)(node::Environment* env,
                                                  v8::Handle<v8::String> source,
                                                  v8::Handle<v8::String> filename);
typedef void (*ReportExceptionFunc)(node::Environment* env,
                                    const v8::TryCatch& try_catch);


int
        iojsLoadScripts(node::Environment *env,
                        ExecuteStringFunc execute, ReportExceptionFunc report);
void
        iojsUnloadScripts(void);


#endif /* __LIBIOJSINTERNALS_H__ */
