#include "libiojsInternals.h"


iojsContext*
iojsContextCreate(void *r, void *ctx, iojsAtomicFetchAdd afa)
{
    iojsContext  *ret;

    ret = (iojsContext*)malloc(sizeof(iojsContext));
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

    int refs = jsCtx->afa(jsCtx->refCount, -1);

    if (refs <= 0) {
        free(jsCtx);
    }
}


int
iojsCall(int id, iojsContext *jsCtx)
{
    jsCtx->afa(jsCtx->refCount, 1);

    iojsCallData  *data;

    data = (iojsCallData *)malloc(sizeof(iojsCallData));
    IOJS_CHECK_OUT_OF_MEMORY(data);

    data->type = IOJS_CALL;
    data->id = id;

    iojsToJSSend((iojsToJS *)data);

    return 0;
}
