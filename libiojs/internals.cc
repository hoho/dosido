#include "libiojsInternals.h"


iojsContext*
iojsContextCreate(void *r, void *ctx, iojsAtomicFetchAdd afa)
{
    iojsContext  *ret;

    ret = (iojsContext*)malloc(sizeof(iojsContext));
    if (ret == NULL)
        return ret;

    ret->refCount = 2;
    ret->r = r;
    ret->refused = 0;
    ret->afa = afa;

    return ret;
}


void
iojsContextAttemptFree(iojsContext *context)
{
    if (context == NULL)
        return;

    int refs = context->afa(context->refCount, -1);

    if (refs <= 0) {
        free(context);
    }
}
