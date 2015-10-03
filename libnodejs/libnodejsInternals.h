/*
 * Copyright Marat Abdullin (https://github.com/hoho)
 */

#ifndef __LIBNODEJSINTERNALS_H__
#define __LIBNODEJSINTERNALS_H__

#include "node.h"
#include "v8.h"
#include "uv.h"
#include "libnodejs.h"

#include <string>
#include <vector>


typedef enum {
    TO_JS_CALL_LOADED_SCRIPT = 1,
    TO_JS_PUSH_CHUNK,
    TO_JS_REQUEST_ERROR,
    TO_JS_RESPONSE_ERROR,
    TO_JS_SUBREQUEST_HEADERS,
    TO_JS_FREE_CALLBACK
} nodejsToJSCommandType;


typedef enum {
    BY_JS_INIT_DESTRUCTOR    = 1,
    BY_JS_READ_REQUEST_BODY  = 2,
    BY_JS_RESPONSE_HEADERS   = 3,
    BY_JS_RESPONSE_BODY      = 4,
    BY_JS_BEGIN_SUBREQUEST   = 5,
    BY_JS_SUBREQUEST_DONE    = 6
} nodejsByJSCommandType;


typedef enum {
    TO_JS_CALLBACK_PUSH_CHUNK         = 7,
    TO_JS_CALLBACK_SUBREQUEST_HEADERS = 8,
    TO_JS_CALLBACK_REQUEST_ERROR      = 9,
    TO_JS_CALLBACK_RESPONSE_ERROR     = 10
} nodejsToJSCallbackCommandType;


typedef enum {
    SR_URL      = 2,
    SR_METHOD   = 3,
    SR_HEADERS  = 4,
    SR_BODY     = 5,
    SR_CALLBACK = 6
} nodejsSrArgument;


#define NODEJS_CHECK_OUT_OF_MEMORY(ptr)                                        \
    if (ptr == NULL) {                                                       \
        fprintf(stderr, "Out of memory\n");                                  \
        abort();                                                             \
    }


typedef struct _nodejsToJS {
    nodejsToJSCommandType   type;
    nodejsContext  *jsCtx;
} nodejsToJS;


typedef struct _nodejsCallCmd : _nodejsToJS {
    int             index;
    nodejsString    method;
    nodejsString    uri;
    nodejsString    httpProtocol;
    nodejsString  **headers;
    nodejsString  **params;
} nodejsCallCmd;


typedef struct nodejsChunkCmd : _nodejsToJS {
    nodejsString    chunk;
    unsigned        last:1;
    unsigned        sr:1;
} nodejsChunkCmd;


typedef struct nodejsSubrequestHeadersCmd : _nodejsToJS {
    int             status;
    nodejsString   *statusMessage;
    nodejsString  **headers;
} nodejsSubrequestHeadersCmd;


typedef struct _nodejsFreeCallbackCmd : _nodejsToJS {
    void  *cb;
} nodejsFreeCallbackCmd;


typedef v8::Local<v8::Value> (*ExecuteStringFunc)(node::Environment* env,
                                                  v8::Handle<v8::String> source,
                                                  v8::Handle<v8::String> filename);
typedef void (*ReportExceptionFunc)(node::Environment* env,
                                    const v8::TryCatch& try_catch);


int
        nodejsLoadScripts(node::Environment *env,
                          ExecuteStringFunc execute, ReportExceptionFunc report);
void
        nodejsUnloadScripts(void);

void
        nodejsInitLogger(nodejsLogger logger);


#endif /* __LIBNODEJSINTERNALS_H__ */
