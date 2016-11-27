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
    TO_JS_CALL_JS_MODULE = 1,
    TO_JS_PUSH_CHUNK,
    TO_JS_REQUEST_ERROR,
    TO_JS_RESPONSE_ERROR,
    TO_JS_SUBREQUEST_HEADERS,
    TO_JS_FREE_CALLBACK
} nodejsToJSCommandType;


typedef enum {
    BY_JS_ERROR              = 1,
    BY_JS_INIT_DESTRUCTOR    = 2,
    BY_JS_READ_REQUEST_BODY  = 3,
    BY_JS_RESPONSE_HEADERS   = 4,
    BY_JS_RESPONSE_BODY      = 5,
    BY_JS_BEGIN_SUBREQUEST   = 6,
    BY_JS_SUBREQUEST_DONE    = 7
} nodejsByJSCommandType;


typedef enum {
    TO_JS_CALLBACK_PUSH_CHUNK         = 101,
    TO_JS_CALLBACK_SUBREQUEST_HEADERS = 102,
    TO_JS_CALLBACK_REQUEST_ERROR      = 103,
    TO_JS_CALLBACK_RESPONSE_ERROR     = 104
} nodejsToJSCallbackCommandType;


typedef enum {
    SR_URL      = 2,
    SR_METHOD   = 3,
    SR_HEADERS  = 4,
    SR_BODY     = 5,
    SR_CALLBACK = 6
} nodejsSrArgument;


#define NODEJS_CHECK_OUT_OF_MEMORY(ptr)                                      \
    if (ptr == NULL) {                                                       \
        fprintf(stderr, "Out of memory\n");                                  \
        abort();                                                             \
    }


typedef struct _nodejsToJS {
    nodejsToJSCommandType   type;
    nodejsContext  *jsCtx;
} nodejsToJS;


typedef struct _nodejsCallCmd : _nodejsToJS {
    nodejsString    jsModulePath;
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


int
        nodejsStartPolling(node::Environment *env);
void
        nodejsStopPolling(void);

void
        nodejsInitLogger(nodejsLogger logger);


#endif /* __LIBNODEJSINTERNALS_H__ */
