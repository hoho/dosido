#include "libiojsInternals.h"


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
        //if (jsCtx->freeCallback) {
        //    jsCtx->freeCallback(jsCtx->jsCallback);
        //}
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
