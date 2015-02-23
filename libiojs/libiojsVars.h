/*
 * Copyright Marat Abdullin (https://github.com/hoho)
 */

#ifndef __LIBIOJSVARS_H__
#define __LIBIOJSVARS_H__

#include "node.h"
#include "libiojs.h"
#include "uv.h"
#include "node_natives.h"
#include "v8.h"


int                            iojsIncomingPipeFd[2] = {-1, -1};
int                            iojsOutgoingPipeFd[2] = {-1, -1};
uv_barrier_t                   iojsStartBlocker;
uv_thread_t                    iojsThreadId;
uv_poll_t                      iojsCommandPoll;

iojsJS                        *iojsScripts;
size_t                         iojsScriptsLen;
int                            iojsError;

v8::Persistent<v8::Array>      iojsLoadedScripts;


#endif /* __LIBIOJSVARS_H__ */
