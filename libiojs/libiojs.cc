#include "env.h"
#include "env-inl.h"
#include "node_natives.h"

#include "libiojsInternals.h"
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
inline int iojsPipe(int pipefd[2])
{
    return _pipe(pipefd, 65535, _O_BINARY);
}
inline ssize_t iojsWrite(int fd, const void *buf, size_t count)
{
    return _write(fd, buf, count);
}
inline ssize_t iojsRead(int fd, void *buf, size_t count)
{
    return _read(fd, buf, count);
}
inline int iojsClose(int fd)
{
    return _close(fd);
}
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
inline int iojsPipe(int pipefd[2])
{
    return pipe(pipefd);
}
inline ssize_t iojsWrite(int fd, const void *buf, size_t count)
{
    return write(fd, buf, count);
}
inline ssize_t iojsRead(int fd, void *buf, size_t count)
{
    return read(fd, buf, count);
}
inline int iojsClose(int fd)
{
    return close(fd);
}
#endif


using node::Environment;
using node::libiojs_native;
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


static int                            iojsIncomingPipeFd[2] = {-1, -1};
static int                            iojsOutgoingPipeFd[2] = {-1, -1};
static uv_barrier_t                   iojsStartBlocker;
static uv_thread_t                    iojsThreadId;
static uv_poll_t                      iojsCommandPoll;

static iojsJSArray                    iojsScripts;
static int                            iojsError;

static v8::Persistent<v8::Array>      iojsLoadedScripts;


// To call from nginx thread.
static inline void
iojsToJSSend(iojsToJS *cmd)
{
    ssize_t sent = 0;
    ssize_t sz;
    while (sent < (ssize_t)sizeof(cmd)) {
        sz = iojsWrite(iojsIncomingPipeFd[1],
                       reinterpret_cast<char *>(&cmd) + sent,
                       sizeof(cmd) - sent);

        if (sz < 0) {
            fprintf(stderr, "iojsToJSSend fatal error: %d\n", errno);
            abort();
        }

        sent += sz;
    }
}


// To call from iojs thread.
static ssize_t    iojsCurToJSRecvLen = 0;
static iojsToJS  *iojsCurToJSRecvCmd;
static inline iojsToJS*
iojsToJSRecv(void)
{
    ssize_t sz = iojsRead(
            iojsIncomingPipeFd[0],
            reinterpret_cast<char *>(&iojsCurToJSRecvCmd) + iojsCurToJSRecvLen,
            sizeof(iojsToJS *) - iojsCurToJSRecvLen
    );

    if (sz < 0) {
        fprintf(stderr, "iojsToJSRecv fatal error: %d\n", errno);
        abort();
    }

    if (sz > 0) {
        iojsCurToJSRecvLen += sz;
        if (iojsCurToJSRecvLen == sizeof(iojsToJS *)) {
            iojsCurToJSRecvLen = 0;
            return iojsCurToJSRecvCmd;
        }
    }

    return NULL;
}


// To call from iojs thread.
static inline void
iojsFromJSSend(iojsFromJS *cmd)
{
    ssize_t sent = 0;
    ssize_t sz;
    while (sent < (ssize_t)sizeof(cmd)) {
        sz = iojsWrite(iojsOutgoingPipeFd[1],
                       reinterpret_cast<char *>(&cmd) + sent,
                       sizeof(cmd) - sent);

        if (sz < 0) {
            fprintf(stderr, "iojsFromJSSend fatal error: %d\n", errno);
            abort();
        }

        sent += sz;
    }
}


// To call from nginx thread.
static ssize_t      iojsCurFromJSRecvLen = 0;
static iojsFromJS  *iojsCurFromJSRecvCmd;
iojsFromJS*
iojsFromJSRecv(void)
{
    ssize_t sz = iojsRead(
            iojsOutgoingPipeFd[0],
            reinterpret_cast<char *>(&iojsCurFromJSRecvCmd) + iojsCurFromJSRecvLen,
            sizeof(iojsFromJS *) - iojsCurFromJSRecvLen
    );

    if (sz < 0) {
        fprintf(stderr, "iojsFromJSRecv fatal error: %d\n", errno);
        abort();
    }

    if (sz > 0) {
        iojsCurFromJSRecvLen += sz;
        if (iojsCurFromJSRecvLen == sizeof(iojsFromJS *)) {
            iojsCurFromJSRecvLen = 0;
            return iojsCurFromJSRecvCmd;
        }
    }

    return NULL;
}


// To call from iojs thread.
static inline void
iojsToJSFree(iojsToJS *cmd)
{
    free(cmd);
}


// To call from nginx thread.
static inline void
iojsFreeCallback(void *cb)
{
    iojsFreeCallbackCmd  *cmd;

    cmd = reinterpret_cast<iojsFreeCallbackCmd *>(
            malloc(sizeof(iojsFreeCallbackCmd))
    );
    IOJS_CHECK_OUT_OF_MEMORY(cmd);

    cmd->type = TO_JS_FREE_CALLBACK;
    cmd->cb = cb;

    iojsToJSSend(cmd);
}


// To call from nginx thread.
void
iojsFromJSFree(iojsFromJS *cmd)
{
    if (cmd->free != NULL)
        cmd->free(cmd->data);

    if (cmd->jsCallback)
        iojsFreeCallback(cmd->jsCallback);

    free(cmd);
}


static inline int
iojsStartPolling(Environment *env, uv_poll_cb cb)
{
    int ret;

    ret = uv_poll_init(env->event_loop(),
                       &iojsCommandPoll, iojsIncomingPipeFd[0]);
    if (ret)
        return ret;

    iojsCommandPoll.data = env;
    ret = uv_poll_start(&iojsCommandPoll, UV_READABLE, cb);

    return ret;
}


static inline void
iojsStopPolling(void)
{
    uv_poll_stop(&iojsCommandPoll);
}


static inline void
iojsClosePipes(void)
{
    if (iojsIncomingPipeFd[0] != -1)
        iojsClose(iojsIncomingPipeFd[0]);

    if (iojsIncomingPipeFd[1] != -1)
        iojsClose(iojsIncomingPipeFd[1]);

    if (iojsOutgoingPipeFd[0] != -1)
        iojsClose(iojsOutgoingPipeFd[0]);

    if (iojsOutgoingPipeFd[1] != -1)
        iojsClose(iojsOutgoingPipeFd[1]);
}


static inline int
iojsOpenPipes(void)
{
    int err;
#ifndef _WIN32
    int r;
    int set = 1;
#endif

    err = iojsPipe(iojsIncomingPipeFd);
    if (err) {
        err = -errno;
        goto error;
    }
    err = iojsPipe(iojsOutgoingPipeFd);
    if (err) {
        err = -errno;
        goto error;
    }

#ifndef _WIN32
    do r = ioctl(iojsIncomingPipeFd[0], FIONBIO, &set);
    while (r == -1 && errno == EINTR);
    if (r) {
        err = -errno;
        goto error;
    }

    do r = ioctl(iojsIncomingPipeFd[1], FIONBIO, &set);
    while (r == -1 && errno == EINTR);
    if (r) {
        err = -errno;
        goto error;
    }

    do r = ioctl(iojsOutgoingPipeFd[0], FIONBIO, &set);
    while (r == -1 && errno == EINTR);
    if (r) {
        err = -errno;
        goto error;
    }

    do r = ioctl(iojsOutgoingPipeFd[1], FIONBIO, &set);
    while (r == -1 && errno == EINTR);
    if (r) {
        err = -errno;
        goto error;
    }
#endif

    return 0;

error:
    iojsClosePipes();
    return err;
}


static void
iojsRunnerThread(void *arg)
{
    int argc = 1;
    char *argv[1] = {(char *)"dosido"};
    node::Start(argc, argv);
    iojsClosePipes();
}


int
iojsStart(iojsJSArray *scripts, int *fd)
{
    int err;

    iojsError = -1;

    err = iojsOpenPipes();
    if (err)
        goto error;

    uv_barrier_init(&iojsStartBlocker, 2);

    memcpy(&iojsScripts, scripts, sizeof(iojsScripts));

    uv_thread_create(&iojsThreadId, iojsRunnerThread, NULL);

    if (uv_barrier_wait(&iojsStartBlocker) > 0)
        uv_barrier_destroy(&iojsStartBlocker);

    memset(&iojsScripts, 0, sizeof(iojsScripts));

    *fd = iojsOutgoingPipeFd[0];

    return iojsError;

error:
    iojsClosePipes();
    return err;
}


static void
iojsDestroyWeakCallback(const v8::WeakCallbackData<Object, iojsContext>& data)
{
    iojsContext *jsCtx = data.GetParameter();

    Persistent<Object> *destroy = static_cast<Persistent<Object> *>(jsCtx->_p);
    destroy->ClearWeak();
    destroy->Reset();
    delete destroy;
    jsCtx->_p = destroy = nullptr;

    iojsContextAttemptFree(jsCtx);
}


static inline void
iojsFreePersistentFunction(void *fn)
{
    CHECK(fn != NULL);
    Persistent<Function> *f = static_cast<Persistent<Function> *>(fn);
    f->Reset();
    delete f;
}


static void
iojsCallJSCallback(Environment *env, iojsToJSCallbackCommandType what,
                   iojsContext *jsCtx, void *cmd)
{
    HandleScope scope(env->isolate());

    Local<Value> args[3];
    Persistent<Function> *_f = NULL;
    Local<Function> f;
    unsigned isSubrequest;

    switch (what) {
        case TO_JS_CALLBACK_PUSH_CHUNK:
            isSubrequest = reinterpret_cast<iojsChunkCmd *>(cmd)->sr;
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
                iojsChunkCmd *c = reinterpret_cast<iojsChunkCmd *>(cmd);

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
                Local<Object>              h = Object::New(env->isolate());
                Local<Object>              meta = Object::New(env->isolate());
                int                        i;
                iojsSubrequestHeadersCmd  *shcmd = \
                        reinterpret_cast<iojsSubrequestHeadersCmd *>(cmd);
                iojsString               **headers = shcmd->headers;

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


static void
iojsCallLoadedScriptCallback(const FunctionCallbackInfo<Value>& args)
{
    Environment *env = Environment::GetCurrent(args.GetIsolate());

    HandleScope scope(env->isolate());

    int32_t _type = args[0]->ToInteger(env->isolate())->Int32Value();
    Local<v8::External> payload = Local<v8::External>::Cast(args[1]);
    iojsContext *jsCtx = reinterpret_cast<iojsContext *>(payload->Value());

    if (_type == 0) {
        // Destroy indicator. When JavaScript is finished, this object will be
        // garbage collected and PayloadWeakCallback() will cleanup the context.
        Local<Object> tmp = Object::New(env->isolate());
        Persistent<Object> *destroy = new Persistent<Object>(env->isolate(), tmp);

        destroy->SetWeak(jsCtx, iojsDestroyWeakCallback);
        destroy->MarkIndependent();

        jsCtx->_p = destroy;

        args.GetReturnValue().Set(*destroy);
        return;
    }

    iojsByJSCommandType type;
    Local<Value> arg = args[2];
    bool isstr;
    Local<String> strval;

    int sz = 0;
    type = static_cast<iojsByJSCommandType>(_type);

    switch (type) {
        case BY_JS_INIT_DESTRUCTOR:
        case BY_JS_READ_REQUEST_BODY:
        case BY_JS_RESPONSE_HEADERS:
        case BY_JS_BEGIN_SUBREQUEST:
        case BY_JS_SUBREQUEST_DONE:
            sz = sizeof(iojsFromJS);
            break;

        case BY_JS_RESPONSE_BODY:
            sz = sizeof(iojsFromJS);
            isstr = arg->IsString();
            if (isstr) {
                strval = arg->ToString();
                sz += sizeof(iojsString) + strval->Utf8Length();
            }
            break;
    }

    if (sz == 0)
        return;

    iojsFromJS *cmd = reinterpret_cast<iojsFromJS *>(malloc(sz));
    IOJS_CHECK_OUT_OF_MEMORY(cmd);
    memset(cmd, 0, sizeof(iojsFromJS));

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
            break;

        case BY_JS_RESPONSE_BODY:
            cmd->type = FROM_JS_RESPONSE_BODY;

            if (isstr) {
                cmd->data = &cmd[1];
                iojsString *s = reinterpret_cast<iojsString *>(cmd->data);
                s->len = strval->Utf8Length();
                s->data = reinterpret_cast<char *>(&cmd[1]) + sizeof(iojsString);
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
                std::vector<std::pair<std::string, std::string>>::reverse_iterator it;
                Local<Array> names = headers->GetOwnPropertyNames();
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

                    h.push_back(std::make_pair(
                            std::string(*_key, keyLen),
                            std::string(*_val, valLen)
                    ));

                    sz += sizeof(iojsString) + sizeof(iojsString) +
                          keyLen + valLen;
                }

                sz += urlLen + methodLen + bodyLen + sizeof(iojsSubrequest);

                iojsSubrequest *sr = reinterpret_cast<iojsSubrequest *>(malloc(sz));
                IOJS_CHECK_OUT_OF_MEMORY(sr);
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

                if (h.size()) {
                    iojsString *harr = reinterpret_cast<iojsString *>(ptr);
                    ptr += sizeof(iojsString) * h.size() * 2;
                    sr->headers = harr;

                    i = 0;

                    for (it = h.rbegin(); it != h.rend(); ++it) {
                        sz = it->first.length();
                        harr[i].len = sz;
                        harr[i++].data = ptr;
                        memcpy(ptr, it->first.c_str(), sz);
                        ptr += sz;

                        sz = it->second.length();
                        harr[i].len = sz;
                        harr[i++].data = ptr;
                        memcpy(ptr, it->second.c_str(), sz);
                        ptr += sz;
                    }
                }
            }
            break;

        case BY_JS_SUBREQUEST_DONE:
            cmd->type = FROM_JS_SUBREQUEST_DONE;
            break;

        default:
            // This should never happen.
            iojsFromJSFree(cmd);
            return;
    }

    iojsFromJSSend(cmd);
}


static inline void
iojsCallLoadedScript(Environment *env, iojsCallCmd *cmd)
{
    iojsString **headers = cmd->headers;
    iojsString **params = cmd->params;

    HandleScope handle_scope(env->isolate());

    Local<Array> scripts = Local<Array>::New(env->isolate(), iojsLoadedScripts);

    Local<Object> m = Object::New(env->isolate());
    Local<Object> h = Object::New(env->isolate());
    Local<Object> p = Object::New(env->isolate());

    Local<Function> callback = \
            env->NewFunctionTemplate(iojsCallLoadedScriptCallback)->GetFunction();
    Local<v8::External> payload = v8::External::New(env->isolate(), cmd->jsCtx);

    Local<Value> args[5] = {m, h, p, callback, payload};

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
iojsRunIncomingTask(uv_poll_t *handle, int status, int events)
{
    iojsToJS *cmd;

    cmd = iojsToJSRecv();

    if (cmd != NULL) {
        Environment *env = reinterpret_cast<Environment *>(handle->data);
        HandleScope scope(env->isolate());

        switch (cmd->type) {
            case TO_JS_CALL_LOADED_SCRIPT:
                iojsCallLoadedScript(env, reinterpret_cast<iojsCallCmd *>(cmd));
                break;

            case TO_JS_PUSH_CHUNK:
                iojsCallJSCallback(
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
                iojsCallJSCallback(
                        env,
                        TO_JS_CALLBACK_SUBREQUEST_HEADERS,
                        cmd->jsCtx,
                        cmd
                );
                break;

            case TO_JS_FREE_CALLBACK:
                iojsFreePersistentFunction(
                        reinterpret_cast<iojsFreeCallbackCmd *>(cmd)->cb
                );
                break;
        }

        iojsToJSFree(cmd);
    }
}


int
iojsLoadScripts(Environment *env,
                ExecuteStringFunc execute, ReportExceptionFunc report)
{
    int ret = 0;

    HandleScope handle_scope(env->isolate());

    TryCatch try_catch;

    try_catch.SetVerbose(false);

    Local<String> script_name = FIXED_ONE_BYTE_STRING(env->isolate(),
                                                      "libiojs.js");
    Local<Value> f_value = execute(
        env,
        OneByteString(env->isolate(), libiojs_native, sizeof(libiojs_native) - 1),
        script_name
    );

    if (try_catch.HasCaught())  {
        report(env, try_catch);
        exit(10);
    }

    CHECK(f_value->IsFunction());
    Local<Function> f = Local<Function>::Cast(f_value);

    Local<Object> global = env->context()->Global();
    Local<Array> scripts = Array::New(env->isolate(), iojsScripts.len);

    size_t   i;
    iojsJS  *script;

    for (i = 0; i < iojsScripts.len; i++) {
        script = &iojsScripts.js[i];
        scripts->Set(script->index,
                     OneByteString(env->isolate(),
                                   script->filename, script->len));
    }

    Local<Value> arg = scripts;
    f_value = f->Call(global, 1, &arg);

    if (try_catch.HasCaught())  {
        report(env, try_catch);
        ret = -1;
        goto done;
    }

    CHECK(f_value->IsArray());
    iojsLoadedScripts.Reset(env->isolate(), Local<Array>::Cast(f_value));

    if (iojsStartPolling(env, iojsRunIncomingTask))
        goto done;

    // Workaround to actually run _third_party_main.js.
    env->tick_callback_function()->Call(env->process_object(), 0, nullptr);

done:
    iojsError = ret;

    if (uv_barrier_wait(&iojsStartBlocker) > 0)
        uv_barrier_destroy(&iojsStartBlocker);

    return ret;
}


void
iojsUnloadScripts(void)
{
    iojsStopPolling();

    if (!iojsLoadedScripts.IsEmpty())
        iojsLoadedScripts.Reset();
}


iojsContext*
iojsContextCreate(void *r, iojsContext *rootCtx, iojsAtomicFetchAdd afa)
{
    iojsContext  *ret;

    ret = reinterpret_cast<iojsContext *>(malloc(sizeof(iojsContext)));
    if (ret == NULL)
        return ret;

    memset(ret, 0, sizeof(iojsContext));

    ret->refCount = 1;
    ret->r = r;
    ret->afa = afa;
    ret->rootCtx = rootCtx == NULL ? ret : rootCtx;

    return ret;
}


void
iojsContextAttemptFree(iojsContext *jsCtx)
{
    if (jsCtx == NULL)
        return;

    int64_t refs = jsCtx->afa(&jsCtx->refCount, -1) - 1;

    if (refs <= 0) {
        if (jsCtx->jsCallback || jsCtx->jsSubrequestCallback) {
            if (iojsThreadId == uv_thread_self()) {
                // It is safe to free the callback from iojs thread,
                // this will mostly be the case, because nginx request is
                // usually completed and finalized before V8 garbage collects
                // request's data.
                if (jsCtx->jsCallback)
                    iojsFreePersistentFunction(jsCtx->jsCallback);
                if (jsCtx->jsSubrequestCallback)
                    iojsFreePersistentFunction(jsCtx->jsSubrequestCallback);
            } else {
                // Otherwise, we need to send free command to iojs.
                if (jsCtx->jsCallback)
                    iojsFreeCallback(jsCtx->jsCallback);
                if (jsCtx->jsSubrequestCallback)
                    iojsFreeCallback(jsCtx->jsSubrequestCallback);
            }
        }
        free(jsCtx);
    }
}


int
iojsCall(int index, iojsContext *jsCtx, iojsString **headers,
         iojsString **params)
{
    jsCtx->afa(&jsCtx->refCount, 1);

    iojsCallCmd  *cmd;

    cmd = reinterpret_cast<iojsCallCmd *>(malloc(sizeof(iojsCallCmd)));
    IOJS_CHECK_OUT_OF_MEMORY(cmd);

    cmd->type = TO_JS_CALL_LOADED_SCRIPT;
    cmd->jsCtx = jsCtx;
    cmd->index = index;
    cmd->headers = headers;
    cmd->params = params;

    iojsToJSSend(cmd);

    return 0;
}


int
iojsChunk(iojsContext *jsCtx, char *data, size_t len,
          unsigned last, unsigned sr)
{
    iojsChunkCmd *cmd;

    cmd = reinterpret_cast<iojsChunkCmd *>(malloc(sizeof(iojsChunkCmd) + len));
    IOJS_CHECK_OUT_OF_MEMORY(cmd);

    cmd->type = TO_JS_PUSH_CHUNK;
    cmd->jsCtx = jsCtx;
    cmd->chunk.len = len;
    cmd->chunk.data = (char *)&cmd[1];
    cmd->last = last;
    cmd->sr = sr;
    memcpy(cmd->chunk.data, data, len);

    iojsToJSSend(cmd);

    return 0;
}


int
iojsSubrequestHeaders(iojsContext *jsCtx, int status, iojsString *statusMsg,
                      iojsString **headers)
{
    iojsSubrequestHeadersCmd *cmd;

    cmd = reinterpret_cast<iojsSubrequestHeadersCmd *>(
            malloc(sizeof(iojsSubrequestHeadersCmd) +
                   sizeof(iojsString) + statusMsg->len)
    );
    IOJS_CHECK_OUT_OF_MEMORY(cmd);

    cmd->type = TO_JS_SUBREQUEST_HEADERS;
    cmd->jsCtx = jsCtx;
    cmd->status = status;
    cmd->statusMessage = reinterpret_cast<iojsString *>(&cmd[1]);
    cmd->statusMessage->data = reinterpret_cast<char *>(&cmd->statusMessage[1]);
    cmd->statusMessage->len = statusMsg->len;
    memcpy(cmd->statusMessage->data, statusMsg->data, statusMsg->len);
    cmd->headers = headers;

    iojsToJSSend(cmd);

    return 0;
}
