#include "env.h"
#include "env-inl.h"
#include "node_natives.h"

#include "libiojsInternals.h"

#include <sys/types.h>
#include <sys/ioctl.h>


using node::Environment;
using node::libiojs_native;
using node::OneByteString;
using node::Utf8Value;

using v8::Array;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Local;
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
        sz = write(iojsIncomingPipeFd[1],
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
    ssize_t sz = read(
            iojsIncomingPipeFd[0],
            reinterpret_cast<char *>(&iojsCurToJSRecvCmd) + iojsCurToJSRecvLen,
            sizeof(iojsToJS *) - iojsCurToJSRecvLen
    );
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
        sz = write(iojsOutgoingPipeFd[1],
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
    ssize_t sz = read(
            iojsOutgoingPipeFd[0],
            reinterpret_cast<char *>(&iojsCurFromJSRecvCmd) + iojsCurFromJSRecvLen,
            sizeof(iojsFromJS *) - iojsCurFromJSRecvLen
    );

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
void
iojsFromJSFree(iojsFromJS *cmd)
{
    if (cmd->free != NULL)
        cmd->free(cmd->data);
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
        close(iojsIncomingPipeFd[0]);

    if (iojsIncomingPipeFd[1] != -1)
        close(iojsIncomingPipeFd[1]);

    if (iojsOutgoingPipeFd[0] != -1)
        close(iojsOutgoingPipeFd[0]);

    if (iojsOutgoingPipeFd[1] != -1)
        close(iojsOutgoingPipeFd[1]);
}


static inline int
iojsOpenPipes(void)
{
    int err;
    int r;
    int set = 1;

    err = pipe(iojsIncomingPipeFd);
    if (err) {
        err = -errno;
        goto error;
    }
    err = pipe(iojsOutgoingPipeFd);
    if (err) {
        err = -errno;
        goto error;
    }

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


// To call from nginx thread.
void
iojsStop(void)
{
    iojsToJS *cmd = reinterpret_cast<iojsToJS *>(malloc(sizeof(iojsToJS)));
    cmd->type = IOJS_EXIT;
    iojsToJSSend(cmd);
    uv_thread_join(&iojsThreadId);
}


static void
DestroyWeakCallback(const v8::WeakCallbackData<Object, iojsContext>& data)
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
iojsCallJSCallback(Environment *env, void *cb, int what, void *arg)
{

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

        destroy->SetWeak(jsCtx, DestroyWeakCallback);
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
        case BY_JS_SUBREQUEST:
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
    cmd->jsCtx = jsCtx;
    cmd->data = NULL;
    cmd->free = NULL;
    cmd->jsCallback = NULL;

    switch (type) {
        case BY_JS_INIT_DESTRUCTOR:
            CHECK(arg->IsFunction());
            cmd->type = IOJS_JS_CALLBACK;
            {
                Persistent<Function> *f = new Persistent<Function>(
                        env->isolate(),
                        Local<Function>::Cast(arg)
                );
                cmd->jsCallback = f;
            }
            break;

        case BY_JS_READ_REQUEST_BODY:
            cmd->type = IOJS_READ_REQUEST_BODY;
            break;

        case BY_JS_RESPONSE_HEADERS:
            cmd->type = IOJS_RESPONSE_HEADERS;
            break;

        case BY_JS_RESPONSE_BODY:
            cmd->type = IOJS_RESPONSE_BODY;

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

        case BY_JS_SUBREQUEST:
            cmd->type = IOJS_SUBREQUEST;
            {
                Local<String> url = args[2]->ToString(); // url.
                Local<String> method = args[3]->ToString(); // method.
                Local<Object> headers = args[4]->ToObject(); // headers.
                Local<String> body;

                CHECK(args[6]->IsFunction());

                int urlLen = url->Utf8Length();
                int methodLen = method->Utf8Length();
                int bodyLen;

                bool noBody = args[5]->IsNull();

                if (noBody) {
                    bodyLen = 0;
                } else {
                    body = args[5]->ToString();
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
                        Local<Function>::Cast(args[6])
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
                }

                sr->method.len = methodLen;
                if (methodLen) {
                    Utf8Value _method(env->isolate(), method);
                    sr->method.data = ptr;
                    memcpy(sr->method.data, *_method, methodLen);
                    ptr += methodLen;
                } else {
                    sr->method.data = nullptr;
                }

                sr->body.len = urlLen;
                if (bodyLen) {
                    Utf8Value _body(env->isolate(), body);
                    sr->body.data = ptr;
                    memcpy(sr->body.data, *_body, bodyLen);
                    ptr += bodyLen;
                } else {
                    sr->body.data = nullptr;
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

        default:
            // This should never happen.
            iojsFromJSFree(cmd);
            return;
    }

    iojsFromJSSend(cmd);
}


static inline void
iojsCallLoadedScript(Environment *env, int index, iojsContext *jsCtx)
{
    HandleScope handle_scope(env->isolate());

    Local<Array> scripts = Local<Array>::New(env->isolate(), iojsLoadedScripts);

    Local<Object> headers = Object::New(env->isolate());
    Local<Function> callback = \
            env->NewFunctionTemplate(iojsCallLoadedScriptCallback)->GetFunction();
    Local<v8::External> payload = v8::External::New(env->isolate(), jsCtx);

    Local<Value> args[3] = {headers, callback, payload};

    MakeCallback(env, scripts, index, 3, args);
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
            case IOJS_CALL:
                iojsCallLoadedScript(
                        env,
                        reinterpret_cast<iojsCallCmd *>(cmd)->index,
                        reinterpret_cast<iojsCallCmd *>(cmd)->jsCtx
                );
                break;

            case IOJS_CHUNK:
                {
                    iojsChunkCmd *c = reinterpret_cast<iojsChunkCmd *>(cmd);
                    iojsCallJSCallback(env, c->jsCallback, 5, &c->chunk);
                }
                break;

            case IOJS_REQUEST_ERROR:
                break;

            case IOJS_RESPONSE_ERROR:
                break;

            case IOJS_FREE_CALLBACK:
                break;

            case IOJS_EXIT:
                iojsStopPolling();
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
    if (!iojsLoadedScripts.IsEmpty())
        iojsLoadedScripts.Reset();
}



iojsContext*
iojsContextCreate(void *r, void *ctx, iojsAtomicFetchAdd afa)
{
    iojsContext  *ret;

    ret = reinterpret_cast<iojsContext *>(malloc(sizeof(iojsContext)));
    if (ret == NULL)
        return ret;

    ret->refCount = 1;
    ret->r = r;
    ret->refused = 0;
    ret->afa = afa;

    return ret;
}


void
iojsContextAttemptFree(iojsContext *jsCtx)
{
    if (jsCtx == NULL)
        return;

    int64_t refs = jsCtx->afa(&jsCtx->refCount, -1) - 1;

    if (refs <= 0) {
        if (jsCtx->jsCallback) {
            if (iojsThreadId == uv_thread_self())
                // It is safe to free the callback from iojs thread,
                // this will mostly be the case, because nginx request is
                // usually completed and finalized before V8 garbage collects
                // request's data.
                iojsFreePersistentFunction(jsCtx->jsCallback);
            else
                // Otherwise, we need to send free command to iojs.
                void; // TODO: Implement.
        }
        free(jsCtx);
    }
}


int
iojsCall(int index, iojsContext *jsCtx)
{
    jsCtx->afa(&jsCtx->refCount, 1);

    iojsCallCmd  *cmd;

    cmd = reinterpret_cast<iojsCallCmd *>(malloc(sizeof(iojsCallCmd)));
    IOJS_CHECK_OUT_OF_MEMORY(cmd);

    cmd->type = IOJS_CALL;
    cmd->jsCtx = jsCtx;
    cmd->index = index;

    iojsToJSSend((iojsToJS *)cmd);

    return 0;
}


int
iojsChunk(iojsContext *jsCtx, char *data, size_t len, short last)
{
    fprintf(stderr, "Chunkie\n");

    iojsChunkCmd *cmd;

    cmd = reinterpret_cast<iojsChunkCmd *>(malloc(sizeof(iojsChunkCmd) + len));
    IOJS_CHECK_OUT_OF_MEMORY(cmd);

    cmd->type = IOJS_CHUNK;
    cmd->jsCtx = jsCtx;
    cmd->jsCallback = jsCtx->jsCallback;
    cmd->chunk.len = len;
    cmd->chunk.data = (char *)&cmd[1];
    memcpy(cmd->chunk.data, data, len);

    iojsToJSSend((iojsToJS *)cmd);

    return 0;
}
