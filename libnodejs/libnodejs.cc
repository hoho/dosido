#include "env.h"
#include "env-inl.h"
#include "node_natives.h"

#include "libnodejsInternals.h"
#include <fcntl.h>

#ifdef _WIN32

#define PIPE_FD_TYPE SOCKET

inline int nodejsPipe(PIPE_FD_TYPE pipefd[2])
{
    int                       err;
    PIPE_FD_TYPE              listenSocket;
    struct sockaddr_in        addr;
    socklen_t                 addrlen = sizeof(addr);
    PIPE_FD_TYPE              writeSocket;
    PIPE_FD_TYPE              readSocket;
    u_long                    set = 1;

    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;

    err = bind(listenSocket, (sockaddr*)&addr, sizeof(addr));
    if (err) return err;
    err = getsockname(listenSocket, (struct sockaddr*)&addr, &addrlen);
    if (err) return err;

    err = listen(listenSocket, SOMAXCONN);
    if (err) return err;

    writeSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    err = ioctlsocket(writeSocket, FIONBIO, &set);
    if (err) return err;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    connect(writeSocket, (sockaddr*)&addr, sizeof(addr));

    readSocket = accept(listenSocket, NULL, NULL);
    err = ioctlsocket(readSocket, FIONBIO, &set);
    if (err) return err;

    closesocket(listenSocket);

    pipefd[0] = readSocket;
    pipefd[1] = writeSocket;

    return 0;
}
inline ssize_t nodejsWrite(PIPE_FD_TYPE fd, const void *buf, size_t count)
{
    return send(fd, (const char *)buf, count, 0);
}
inline ssize_t nodejsRead(PIPE_FD_TYPE fd, void *buf, size_t count)
{
    return recv(fd, (char *)buf, count, 0);
}
inline int nodejsClose(PIPE_FD_TYPE fd)
{
    return closesocket(fd);
}

#else

#define PIPE_FD_TYPE int

#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>

inline int nodejsPipe(PIPE_FD_TYPE pipefd[2])
{
    int err = pipe(pipefd);
    if (err) return err;

    int set = 1;

    do err = ioctl(pipefd[0], FIONBIO, &set);
    while (err == -1 && errno == EINTR);
    if (err) return -errno;

    do err = ioctl(pipefd[1], FIONBIO, &set);
    while (err == -1 && errno == EINTR);
    if (err) return -errno;

    return err;
}
inline ssize_t nodejsWrite(PIPE_FD_TYPE fd, const void *buf, size_t count)
{
    return write(fd, buf, count);
}
inline ssize_t nodejsRead(PIPE_FD_TYPE fd, void *buf, size_t count)
{
    return read(fd, buf, count);
}
inline int nodejsClose(PIPE_FD_TYPE fd)
{
    return close(fd);
}

#endif


using node::Environment;
using node::libnodejs_libnodejs_native;
using node::OneByteString;
using node::Utf8Value;

using v8::Array;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Null;
using v8::Object;
using v8::Persistent;
using v8::String;
using v8::TryCatch;
using v8::Value;


static PIPE_FD_TYPE                   nodejsIncomingPipeFd[2] = {-1, -1};
static PIPE_FD_TYPE                   nodejsOutgoingPipeFd[2] = {-1, -1};
static uv_barrier_t                   nodejsStartBlocker;
static uv_thread_t                    nodejsThreadId;
static uv_poll_t                      nodejsCommandPoll;

static nodejsJSArray                  nodejsScripts;
static int                            nodejsError;

static v8::Persistent<v8::Array>      nodejsLoadedScripts;

static nodejsLogger                   nodejsLoggerFunc = NULL;

// To call from nginx thread.
static inline void
nodejsToJSSend(nodejsToJS *cmd)
{
    ssize_t sent = 0;
    ssize_t sz;
    while (sent < (ssize_t)sizeof(cmd)) {
        sz = nodejsWrite(nodejsIncomingPipeFd[1],
                         reinterpret_cast<char *>(&cmd) + sent,
                         sizeof(cmd) - sent);

        if (sz < 0) {
            nodejsLoggerFunc(NODEJS_LOG_ALERT,
                             "nodejsToJSSend fatal error: %d\n", errno);
            abort();
        }

        sent += sz;
    }
}


// To call from nodejs thread.
static ssize_t      nodejsCurToJSRecvLen = 0;
static nodejsToJS  *nodejsCurToJSRecvCmd;
static inline nodejsToJS*
nodejsToJSRecv(void)
{
    ssize_t sz = nodejsRead(
            nodejsIncomingPipeFd[0],
            reinterpret_cast<char *>(&nodejsCurToJSRecvCmd) +
                    nodejsCurToJSRecvLen,
            sizeof(nodejsToJS *) - nodejsCurToJSRecvLen
    );

    if (sz < 0) {
        nodejsLoggerFunc(NODEJS_LOG_ALERT,
                         "nodejsToJSRecv fatal error: %d\n", errno);
        abort();
    }

    if (sz > 0) {
        nodejsCurToJSRecvLen += sz;
        if (nodejsCurToJSRecvLen == sizeof(nodejsToJS *)) {
            nodejsCurToJSRecvLen = 0;
            return nodejsCurToJSRecvCmd;
        }
    }

    return NULL;
}


// To call from nodejs thread.
static inline void
nodejsFromJSSend(nodejsFromJS *cmd)
{
    ssize_t sent = 0;
    ssize_t sz;
    while (sent < (ssize_t)sizeof(cmd)) {
        sz = nodejsWrite(nodejsOutgoingPipeFd[1],
                         reinterpret_cast<char *>(&cmd) + sent,
                         sizeof(cmd) - sent);

        if (sz < 0) {
            nodejsLoggerFunc(NODEJS_LOG_ALERT,
                             "nodejsFromJSSend fatal error: %d\n", errno);
            abort();
        }

        sent += sz;
    }
}


// To call from nginx thread.
static ssize_t        nodejsCurFromJSRecvLen = 0;
static nodejsFromJS  *nodejsCurFromJSRecvCmd;
nodejsFromJS*
nodejsFromJSRecv(void)
{
    ssize_t sz = nodejsRead(
            nodejsOutgoingPipeFd[0],
            reinterpret_cast<char *>(&nodejsCurFromJSRecvCmd) +
                    nodejsCurFromJSRecvLen,
            sizeof(nodejsFromJS *) - nodejsCurFromJSRecvLen
    );

    if (sz < 0) {
        nodejsLoggerFunc(NODEJS_LOG_ALERT,
                         "nodejsFromJSRecv fatal error: %d\n", errno);
        abort();
    }

    if (sz > 0) {
        nodejsCurFromJSRecvLen += sz;
        if (nodejsCurFromJSRecvLen == sizeof(nodejsFromJS *)) {
            nodejsCurFromJSRecvLen = 0;
            return nodejsCurFromJSRecvCmd;
        }
    }

    return NULL;
}


// To call from nodejs thread.
static inline void
nodejsToJSFree(nodejsToJS *cmd)
{
    free(cmd);
}


// To call from nginx thread.
static inline void
nodejsFreeCallback(void *cb)
{
    nodejsFreeCallbackCmd  *cmd;

    cmd = reinterpret_cast<nodejsFreeCallbackCmd *>(
            malloc(sizeof(nodejsFreeCallbackCmd))
    );
    NODEJS_CHECK_OUT_OF_MEMORY(cmd);

    cmd->type = TO_JS_FREE_CALLBACK;
    cmd->cb = cb;

    nodejsToJSSend(cmd);
}


// To call from nginx thread.
void
nodejsFromJSFree(nodejsFromJS *cmd)
{
    if (cmd->free != NULL)
        cmd->free(cmd->data);

    if (cmd->jsCallback)
        nodejsFreeCallback(cmd->jsCallback);

    free(cmd);
}


static inline int
nodejsStartPolling(Environment *env, uv_poll_cb cb)
{
    int ret;

    ret = uv_poll_init_socket(env->event_loop(),
                              &nodejsCommandPoll, nodejsIncomingPipeFd[0]);
    if (ret)
        return ret;

    nodejsCommandPoll.data = env;
    ret = uv_poll_start(&nodejsCommandPoll, UV_READABLE, cb);

    return ret;
}


static inline void
nodejsStopPolling(void)
{
    uv_poll_stop(&nodejsCommandPoll);
}


static inline void
nodejsClosePipes(void)
{
    nodejsLoggerFunc(NODEJS_LOG_INFO, "Closing nodejs pipes");

    nodejsClose(nodejsIncomingPipeFd[0]);
    nodejsClose(nodejsIncomingPipeFd[1]);
    nodejsClose(nodejsOutgoingPipeFd[0]);
    nodejsClose(nodejsOutgoingPipeFd[1]);
}


static inline int
nodejsOpenPipes(void)
{
    int err;

    nodejsLoggerFunc(NODEJS_LOG_INFO, "Opening nodejs pipes");

    err = nodejsPipe(nodejsIncomingPipeFd);
    if (err) goto error;

    err = nodejsPipe(nodejsOutgoingPipeFd);
    if (err) goto error;

    nodejsLoggerFunc(NODEJS_LOG_INFO, "Opened nodejs pipes");

    return 0;

error:
    nodejsLoggerFunc(NODEJS_LOG_CRIT, "Failed to open nodejs pipes (%d)", err);
    nodejsClosePipes();
    return err;
}


static void
nodejsRunnerThread(void *arg)
{
    int argc = 1;
    char *argv[1] = {(char *)"dosido"};
    nodejsLoggerFunc(NODEJS_LOG_INFO, "Starting nodejs");
    nodejsInitLogger(nodejsLoggerFunc);
    node::Start(argc, argv);
    nodejsLoggerFunc(NODEJS_LOG_INFO, "Done nodejs");
    nodejsClosePipes();
}


int
nodejsStart(nodejsLogger logger, nodejsJSArray *scripts, int *fd)
{
    int err;

    nodejsLoggerFunc = logger;
    nodejsError = -1;

    err = nodejsOpenPipes();
    if (err)
        goto error;

    uv_barrier_init(&nodejsStartBlocker, 2);

    memcpy(&nodejsScripts, scripts, sizeof(nodejsScripts));

    nodejsLoggerFunc(NODEJS_LOG_INFO, "Starting nodejs runner thread");

    uv_thread_create(&nodejsThreadId, nodejsRunnerThread, NULL);

    if (uv_barrier_wait(&nodejsStartBlocker) > 0)
        uv_barrier_destroy(&nodejsStartBlocker);

    memset(&nodejsScripts, 0, sizeof(nodejsScripts));

    *fd = nodejsOutgoingPipeFd[0];

    return nodejsError;

error:
    nodejsClosePipes();
    return err;
}


static void
nodejsDestroyWeakCallback(const v8::WeakCallbackInfo<nodejsContext>& data)
{
    nodejsContext *jsCtx = data.GetParameter();

    Persistent<Object> *destroy = static_cast<Persistent<Object> *>(jsCtx->_p);
    destroy->ClearWeak();
    destroy->Reset();
    delete destroy;
    jsCtx->_p = destroy = nullptr;

    nodejsContextAttemptFree(jsCtx);
}


static inline void
nodejsFreePersistentFunction(void *fn)
{
    CHECK(fn != NULL);
    Persistent<Function> *f = static_cast<Persistent<Function> *>(fn);
    f->Reset();
    delete f;
}


static void
nodejsCallJSCallback(Environment *env, nodejsToJSCallbackCommandType what,
                     nodejsContext *jsCtx, void *cmd)
{
    HandleScope scope(env->isolate());

    Local<Value> args[3];
    Persistent<Function> *_f = NULL;
    Local<Function> f;
    unsigned isSubrequest;

    switch (what) {
        case TO_JS_CALLBACK_PUSH_CHUNK:
            isSubrequest = reinterpret_cast<nodejsChunkCmd *>(cmd)->sr;
            break;

        case TO_JS_CALLBACK_SUBREQUEST_HEADERS:
            isSubrequest = 1;
            break;

        case TO_JS_CALLBACK_REQUEST_ERROR:
            isSubrequest = 0;
            break;

        case TO_JS_CALLBACK_RESPONSE_ERROR:
            isSubrequest = 0;
            break;
    }

    if (!isSubrequest && jsCtx->jsCallback) {
        _f = static_cast<Persistent<Function> *>(jsCtx->jsCallback);
        f = Local<Function>::New(env->isolate(), *_f);
    } else if (isSubrequest && jsCtx->jsSubrequestCallback) {
        _f = static_cast<Persistent<Function> *>(jsCtx->jsSubrequestCallback);
        f = Local<Function>::New(env->isolate(), *_f);
    }

    if (_f == NULL) {
        return;
    }

    args[0] = Integer::New(env->isolate(), what);

    switch (what) {
        case TO_JS_CALLBACK_PUSH_CHUNK:
            {
                nodejsChunkCmd *c = reinterpret_cast<nodejsChunkCmd *>(cmd);

                if (c->chunk.len > 0) {
                    args[1] = String::NewFromUtf8(env->isolate(),
                                                  c->chunk.data,
                                                  String::kNormalString,
                                                  c->chunk.len);
                    // Call the callback in the context of itself.
                    MakeCallback(env, f, f, 2, args);
                }

                if (c->last) {
                    args[1] = Null(env->isolate());
                    MakeCallback(env, f, f, 2, args);
                }
            }
            break;

        case TO_JS_CALLBACK_SUBREQUEST_HEADERS:
            {
                Local<Object>                h = Object::New(env->isolate());
                Local<Object>                meta = Object::New(env->isolate());
                int                          i;
                nodejsSubrequestHeadersCmd  *shcmd = \
                        reinterpret_cast<nodejsSubrequestHeadersCmd *>(cmd);
                nodejsString               **headers = shcmd->headers;

                if (headers != NULL) {
                    for (i = 0; headers[i] != NULL; i += 2) {
                        h->Set(
                                String::NewFromUtf8(env->isolate(),
                                                    headers[i]->data,
                                                    String::kNormalString,
                                                    headers[i]->len),
                                String::NewFromUtf8(env->isolate(),
                                                    headers[i + 1]->data,
                                                    String::kNormalString,
                                                    headers[i + 1]->len)
                        );
                    }
                }

                meta->Set(0, Integer::New(env->isolate(), shcmd->status));
                meta->Set(1, String::NewFromUtf8(env->isolate(),
                                                 shcmd->statusMessage->data,
                                                 String::kNormalString,
                                                 shcmd->statusMessage->len));
                args[1] = meta;
                args[2] = h;

                MakeCallback(env, f, f, 3, args);
            }
            break;

        case TO_JS_CALLBACK_REQUEST_ERROR:
            break;

        case TO_JS_CALLBACK_RESPONSE_ERROR:
            break;
    }
}


static inline int
nodejsAggregateHeaders(Environment *env, Local<Object> headers,
                       std::vector<std::pair<std::string, std::string>> *ret)
{
    int sz = 0;

    Local<Array> names = headers->GetOwnPropertyNames();
    Local<String> strval;
    Local<String> val;
    uint32_t i;
    int keyLen;
    int valLen;

    // XXX: Headers aggregation should probably be done better and more
    //      efficient. Maybe someday.
    sz = 0;
    for (i = names->Length(); i--;) {
        strval = names->Get(i)->ToString();
        val = headers->Get(strval)->ToString();

        Utf8Value _key(env->isolate(), strval);
        Utf8Value _val(env->isolate(), val);

        keyLen = strval->Utf8Length();
        valLen = val->Utf8Length();

        ret->push_back(std::make_pair(
                std::string(*_key, keyLen),
                std::string(*_val, valLen)
        ));

        sz += sizeof(nodejsString) + sizeof(nodejsString) + keyLen + valLen;
    }

    return sz;
}


static inline void
nodejsHeadersToStringArray(std::vector<std::pair<std::string, std::string>> *h,
                           char *allocated, nodejsHeaders *ret)
{
    if (h->size()) {
        std::vector<std::pair<std::string, std::string>>::reverse_iterator it;
        uint32_t          i;
        size_t            sz;
        nodejsString     *harr = reinterpret_cast<nodejsString *>(allocated);

        allocated += sizeof(nodejsString) * h->size() * 2;
        ret->strings = harr;
        ret->len = h->size() * 2;

        i = 0;

        for (it = h->rbegin(); it != h->rend(); ++it) {
            sz = it->first.length();
            harr[i].len = sz;
            harr[i++].data = allocated;
            memcpy(allocated, it->first.c_str(), sz);
            allocated += sz;

            sz = it->second.length();
            harr[i].len = sz;
            harr[i++].data = allocated;
            memcpy(allocated, it->second.c_str(), sz);
            allocated += sz;
        }
    } else {
        ret->strings = NULL;
        ret->len = 0;
    }
}


static void
nodejsCallLoadedScriptCallback(const FunctionCallbackInfo<Value>& args)
{
    Environment *env = Environment::GetCurrent(args.GetIsolate());

    HandleScope scope(env->isolate());

    int64_t         _type = args[0]->IntegerValue();
    nodejsContext   *jsCtx =
            static_cast<nodejsContext *>(args[1].As<v8::External>()->Value());

    if (_type == 0) {
        // Destroy indicator. When JavaScript is finished, this object will be
        // garbage collected and PayloadWeakCallback() will cleanup the context.
        Local<Object> tmp = Object::New(env->isolate());
        Persistent<Object> *destroy = new Persistent<Object>(env->isolate(), tmp);

        destroy->SetWeak(jsCtx, nodejsDestroyWeakCallback,
                         v8::WeakCallbackType::kParameter);
        destroy->MarkIndependent();

        jsCtx->_p = destroy;

        args.GetReturnValue().Set(*destroy);
        return;
    }

    nodejsByJSCommandType type;
    Local<Value> arg = args[2];
    bool isstr;
    Local<String> strval;

    size_t sz = 0;
    type = static_cast<nodejsByJSCommandType>(_type);

    switch (type) {
        case BY_JS_INIT_DESTRUCTOR:
        case BY_JS_READ_REQUEST_BODY:
        case BY_JS_RESPONSE_HEADERS:
        case BY_JS_BEGIN_SUBREQUEST:
        case BY_JS_SUBREQUEST_DONE:
            sz = sizeof(nodejsFromJS);
            break;

        case BY_JS_RESPONSE_BODY:
            sz = sizeof(nodejsFromJS);
            isstr = arg->IsString();
            if (isstr) {
                strval = arg.As<String>();
                sz += sizeof(nodejsString) + strval->Utf8Length();
            }
            break;
    }

    if (sz == 0)
        return;

    nodejsFromJS *cmd = reinterpret_cast<nodejsFromJS *>(malloc(sz));
    NODEJS_CHECK_OUT_OF_MEMORY(cmd);
    memset(cmd, 0, sizeof(nodejsFromJS));

    cmd->jsCtx = jsCtx;

    switch (type) {
        case BY_JS_INIT_DESTRUCTOR:
            CHECK(arg->IsFunction());
            cmd->type = FROM_JS_INIT_CALLBACK;
            {
                Persistent<Function> *f = new Persistent<Function>(
                        env->isolate(),
                        Local<Function>::Cast(arg)
                );
                cmd->jsCallback = f;
            }
            break;

        case BY_JS_READ_REQUEST_BODY:
            cmd->type = FROM_JS_READ_REQUEST_BODY;
            break;

        case BY_JS_RESPONSE_HEADERS:
            cmd->type = FROM_JS_RESPONSE_HEADERS;
            {
                Local<Object>    meta = arg->ToObject();
                Local<Integer>   status = meta->Get(0)->ToInteger(); // statusCode.
                Local<String>    msg = meta-> Get(1)->ToString(); // statusMessage.
                Local<Object>    headers = args[3]->ToObject(); // headers.
                int              msgLen = msg->Utf8Length();
                std::vector<std::pair<std::string, std::string>> h;

                sz = nodejsAggregateHeaders(env, headers, &h) +
                     sizeof(nodejsHeaders) + msgLen;

                nodejsHeaders   *ret =
                        reinterpret_cast<nodejsHeaders *>(malloc(sz));
                NODEJS_CHECK_OUT_OF_MEMORY(ret);
                cmd->data = ret;
                cmd->free = free;

                nodejsHeadersToStringArray(&h,
                                           reinterpret_cast<char *>(&ret[1]),
                                           ret);
                ret->statusCode = status->IntegerValue();
                ret->statusMessage.len = msgLen;
                if (msgLen) {
                    Utf8Value _msg(env->isolate(), msg);
                    ret->statusMessage.data = \
                            reinterpret_cast<char *>(ret) + sz - msgLen;
                    memcpy(ret->statusMessage.data, *_msg, msgLen);
                }
            }
            break;

        case BY_JS_RESPONSE_BODY:
            cmd->type = FROM_JS_RESPONSE_BODY;

            if (isstr) {
                cmd->data = &cmd[1];
                nodejsString *s = reinterpret_cast<nodejsString *>(cmd->data);
                s->len = strval->Utf8Length();
                s->data = reinterpret_cast<char *>(&cmd[1]) + sizeof(nodejsString);
                Utf8Value chunk(env->isolate(), strval);
                memcpy(s->data, *chunk, s->len);
            } else {
                cmd->data = nullptr;
            }

            break;

        case BY_JS_BEGIN_SUBREQUEST:
            cmd->type = FROM_JS_BEGIN_SUBREQUEST;
            {
                Local<String> url = args[SR_URL]->ToString(); // url.
                Local<String> method = args[SR_METHOD]->ToString(); // method.
                Local<Object> headers = args[SR_HEADERS]->ToObject(); // headers.
                Local<String> body;

                CHECK(args[SR_CALLBACK]->IsFunction());

                int urlLen = url->Utf8Length();
                int methodLen = method->Utf8Length();
                int bodyLen;

                bool noBody = args[SR_BODY]->IsNull();

                if (noBody) {
                    bodyLen = 0;
                } else {
                    body = args[SR_BODY]->ToString();
                    bodyLen = body->Utf8Length();
                }

                std::vector<std::pair<std::string, std::string>> h;

                sz = nodejsAggregateHeaders(env, headers, &h) +
                     urlLen + methodLen + bodyLen + sizeof(nodejsSubrequest);

                nodejsSubrequest *sr = reinterpret_cast<nodejsSubrequest *>(malloc(sz));
                NODEJS_CHECK_OUT_OF_MEMORY(sr);
                cmd->data = sr;
                cmd->free = free;

                // sr callback
                Persistent<Function> *srCallback = new Persistent<Function>(
                        env->isolate(),
                        Local<Function>::Cast(args[SR_CALLBACK])
                );
                cmd->jsCallback = srCallback;

                char *ptr = reinterpret_cast<char *>(&sr[1]);

                sr->url.len = urlLen;
                if (urlLen) {
                    Utf8Value _url(env->isolate(), url);
                    sr->url.data = ptr;
                    memcpy(sr->url.data, *_url, urlLen);
                    ptr += urlLen;
                } else {
                    sr->url.data = nullptr;
                    sr->url.len = 0;
                }

                sr->method.len = methodLen;
                if (methodLen) {
                    Utf8Value _method(env->isolate(), method);
                    sr->method.data = ptr;
                    memcpy(sr->method.data, *_method, methodLen);
                    ptr += methodLen;
                } else {
                    sr->method.data = nullptr;
                    sr->method.len = 0;
                }

                sr->body.len = bodyLen;
                if (bodyLen) {
                    Utf8Value _body(env->isolate(), body);
                    sr->body.data = ptr;
                    memcpy(sr->body.data, *_body, bodyLen);
                    ptr += bodyLen;
                } else {
                    sr->body.data = nullptr;
                    sr->body.len = 0;
                }

                nodejsHeadersToStringArray(&h, ptr, &sr->headers);
            }
            break;

        case BY_JS_SUBREQUEST_DONE:
            cmd->type = FROM_JS_SUBREQUEST_DONE;
            break;

        default:
            // This should never happen.
            nodejsFromJSFree(cmd);
            return;
    }

    nodejsFromJSSend(cmd);
}


static inline void
nodejsCallLoadedScript(Environment *env, nodejsCallCmd *cmd)
{
    nodejsString        **headers = cmd->headers;
    nodejsString        **params = cmd->params;

    HandleScope handle_scope(env->isolate());

    Local<Array>          scripts = Local<Array>::New(env->isolate(),
                                                      nodejsLoadedScripts);

    Local<Object>         meta = Object::New(env->isolate());
    Local<Object>         h = Object::New(env->isolate());
    Local<Object>         p = Object::New(env->isolate());

    Local<Function>       callback = \
            env->NewFunctionTemplate(nodejsCallLoadedScriptCallback)->GetFunction();
    Local<v8::External>   payload = v8::External::New(env->isolate(), cmd->jsCtx);

    Local<Value>          args[5] = {meta, h, p, callback, payload};

    meta->Set(0, String::NewFromUtf8(env->isolate(),
              cmd->method.data,
              String::kNormalString,
              cmd->method.len));
    meta->Set(1, String::NewFromUtf8(env->isolate(),
              cmd->uri.data,
              String::kNormalString,
              cmd->uri.len));
    meta->Set(2, String::NewFromUtf8(env->isolate(),
              cmd->httpProtocol.data,
              String::kNormalString,
              cmd->httpProtocol.len));

    int i;

    if (headers != NULL) {
        for (i = 0; headers[i] != NULL; i += 2) {
            h->Set(
                    String::NewFromUtf8(env->isolate(),
                                        headers[i]->data,
                                        String::kNormalString,
                                        headers[i]->len),
                    String::NewFromUtf8(env->isolate(),
                                        headers[i + 1]->data,
                                        String::kNormalString,
                                        headers[i + 1]->len)
            );
        }
    }

    if (params != NULL) {
        for (i = 0; params[i] != NULL; i += 2) {
            p->Set(
                    String::NewFromUtf8(env->isolate(),
                                        params[i]->data,
                                        String::kNormalString,
                                        params[i]->len),
                    String::NewFromUtf8(env->isolate(),
                                        params[i + 1]->data,
                                        String::kNormalString,
                                        params[i + 1]->len)
            );
        }
    }

    MakeCallback(env, scripts, cmd->index, 5, args);
}


void
nodejsRunIncomingTask(uv_poll_t *handle, int status, int events)
{
    nodejsToJS *cmd;

    cmd = nodejsToJSRecv();

    if (cmd != NULL) {
        Environment *env = reinterpret_cast<Environment *>(handle->data);
        HandleScope scope(env->isolate());

        switch (cmd->type) {
            case TO_JS_CALL_LOADED_SCRIPT:
                nodejsCallLoadedScript(
                        env,
                        reinterpret_cast<nodejsCallCmd *>(cmd)
                );
                break;

            case TO_JS_PUSH_CHUNK:
                nodejsCallJSCallback(
                        env,
                        TO_JS_CALLBACK_PUSH_CHUNK,
                        cmd->jsCtx,
                        cmd
                );
                break;

            case TO_JS_REQUEST_ERROR:
                break;

            case TO_JS_RESPONSE_ERROR:
                break;

            case TO_JS_SUBREQUEST_HEADERS:
                nodejsCallJSCallback(
                        env,
                        TO_JS_CALLBACK_SUBREQUEST_HEADERS,
                        cmd->jsCtx,
                        cmd
                );
                break;

            case TO_JS_FREE_CALLBACK:
                nodejsFreePersistentFunction(
                        reinterpret_cast<nodejsFreeCallbackCmd *>(cmd)->cb
                );
                break;
        }

        nodejsToJSFree(cmd);
    }
}


int
nodejsLoadScripts(Environment *env,
                  ExecuteStringFunc execute, ReportExceptionFunc report)
{
    int ret = 0;

    HandleScope handle_scope(env->isolate());

    TryCatch try_catch(env->isolate());

    try_catch.SetVerbose(false);

    // Workaround to actually run _third_party_main.js.
    env->tick_callback_function()->Call(env->process_object(), 0, nullptr);

    Local<String> script_name = FIXED_ONE_BYTE_STRING(env->isolate(),
                                                      "libnodejs.js");
    Local<Value> f_value = execute(
        env,
        OneByteString(env->isolate(), libnodejs_libnodejs_native,
                                      sizeof(libnodejs_libnodejs_native) - 1),
        script_name
    );

    if (try_catch.HasCaught())  {
        report(env, try_catch);
        exit(10);
    }

    CHECK(f_value->IsFunction());
    Local<Function>   f = f_value.As<Function>();

    Local<String>     require_name = FIXED_ONE_BYTE_STRING(env->isolate(),
                                                       "_require");
    Local<Value>      require_value = env->process_object()->Get(require_name);
    CHECK(require_value->IsFunction());
    // Remove temporary `process._require` set up by _third_party_main.js.
    env->process_object()->Delete(require_name);
    Local<Function>   require = require_value.As<Function>();

    Local<Array>      scripts = Array::New(env->isolate(), nodejsScripts.len);

    size_t            i;
    nodejsJS         *script;

    for (i = 0; i < nodejsScripts.len; i++) {
        script = &nodejsScripts.js[i];
        scripts->Set(script->index,
                     OneByteString(env->isolate(),
                                   script->filename, script->len));
    }

    Local<Value> args[2] = {require, scripts};

    f_value = f->Call(f, 2, args);

    if (try_catch.HasCaught())  {
        report(env, try_catch);
        ret = -1;
        goto done;
    }

    CHECK(f_value->IsArray());
    nodejsLoadedScripts.Reset(env->isolate(), Local<Array>::Cast(f_value));

    ret = nodejsStartPolling(env, nodejsRunIncomingTask);

done:
    nodejsError = ret;

    if (uv_barrier_wait(&nodejsStartBlocker) > 0)
        uv_barrier_destroy(&nodejsStartBlocker);

    return ret;
}


void
nodejsUnloadScripts(void)
{
    nodejsStopPolling();

    if (!nodejsLoadedScripts.IsEmpty())
        nodejsLoadedScripts.Reset();
}


nodejsContext*
nodejsContextCreate(void *r, nodejsContext *rootCtx, nodejsAtomicFetchAdd afa)
{
    nodejsContext  *ret;

    ret = reinterpret_cast<nodejsContext *>(malloc(sizeof(nodejsContext)));
    if (ret == NULL)
        return ret;

    memset(ret, 0, sizeof(nodejsContext));

    ret->refCount = 1;
    ret->r = r;
    ret->afa = afa;
    ret->rootCtx = rootCtx == NULL ? ret : rootCtx;

    return ret;
}


void
nodejsContextAttemptFree(nodejsContext *jsCtx)
{
    if (jsCtx == NULL)
        return;

    int64_t refs = jsCtx->afa(&jsCtx->refCount, -1) - 1;

    if (refs <= 0) {
        if (jsCtx->jsCallback || jsCtx->jsSubrequestCallback) {
            if (nodejsThreadId == uv_thread_self()) {
                // It is safe to free the callback from nodejs thread,
                // this will mostly be the case, because nginx request is
                // usually completed and finalized before V8 garbage collects
                // request's data.
                if (jsCtx->jsCallback)
                    nodejsFreePersistentFunction(jsCtx->jsCallback);
                if (jsCtx->jsSubrequestCallback)
                    nodejsFreePersistentFunction(jsCtx->jsSubrequestCallback);
            } else {
                // Otherwise, we need to send free command to nodejs.
                if (jsCtx->jsCallback)
                    nodejsFreeCallback(jsCtx->jsCallback);
                if (jsCtx->jsSubrequestCallback)
                    nodejsFreeCallback(jsCtx->jsSubrequestCallback);
            }
        }
        free(jsCtx);
    }
}


int
nodejsCall(int index, nodejsContext *jsCtx, nodejsString *method,
           nodejsString *uri, nodejsString *httpProtocol,
           nodejsString **headers, nodejsString **params)
{
    jsCtx->afa(&jsCtx->refCount, 1);

    nodejsCallCmd  *cmd;

    cmd = reinterpret_cast<nodejsCallCmd *>(malloc(
            sizeof(nodejsCallCmd) + method->len + uri->len + httpProtocol->len
    ));
    NODEJS_CHECK_OUT_OF_MEMORY(cmd);

    cmd->type = TO_JS_CALL_LOADED_SCRIPT;
    cmd->index = index;
    cmd->jsCtx = jsCtx;

    cmd->method.len = method->len;
    cmd->method.data = reinterpret_cast<char *>(&cmd[1]);
    memcpy(cmd->method.data, method->data, method->len);

    cmd->uri.len = uri->len;
    cmd->uri.data = cmd->method.data + method->len;
    memcpy(cmd->uri.data, uri->data, uri->len);

    cmd->httpProtocol.len = httpProtocol->len;
    cmd->httpProtocol.data = cmd->uri.data + uri->len;
    memcpy(cmd->httpProtocol.data, httpProtocol->data, httpProtocol->len);

    cmd->headers = headers;
    cmd->params = params;

    nodejsToJSSend(cmd);

    return 0;
}


int
nodejsChunk(nodejsContext *jsCtx, char *data, size_t len,
            unsigned last, unsigned sr)
{
    nodejsChunkCmd *cmd;

    cmd = reinterpret_cast<nodejsChunkCmd *>(malloc(sizeof(nodejsChunkCmd) +
                                                    len));
    NODEJS_CHECK_OUT_OF_MEMORY(cmd);

    cmd->type = TO_JS_PUSH_CHUNK;
    cmd->jsCtx = jsCtx;
    cmd->chunk.len = len;
    cmd->chunk.data = (char *)&cmd[1];
    cmd->last = last;
    cmd->sr = sr;
    memcpy(cmd->chunk.data, data, len);

    nodejsToJSSend(cmd);

    return 0;
}


int
nodejsSubrequestHeaders(nodejsContext *jsCtx, int status,
                        nodejsString *statusMsg, nodejsString **headers)
{
    nodejsSubrequestHeadersCmd *cmd;

    cmd = reinterpret_cast<nodejsSubrequestHeadersCmd *>(
            malloc(sizeof(nodejsSubrequestHeadersCmd) +
                   sizeof(nodejsString) + statusMsg->len)
    );
    NODEJS_CHECK_OUT_OF_MEMORY(cmd);

    cmd->type = TO_JS_SUBREQUEST_HEADERS;
    cmd->jsCtx = jsCtx;
    cmd->status = status;
    cmd->statusMessage = reinterpret_cast<nodejsString *>(&cmd[1]);
    cmd->statusMessage->data = reinterpret_cast<char *>(&cmd->statusMessage[1]);
    cmd->statusMessage->len = statusMsg->len;
    memcpy(cmd->statusMessage->data, statusMsg->data, statusMsg->len);
    cmd->headers = headers;

    nodejsToJSSend(cmd);

    return 0;
}
