/*
 * Copyright Marat Abdullin (https://github.com/hoho)
 */

#ifndef __LIBIOJSINTERNALS_H__
#define __LIBIOJSINTERNALS_H__

#include <libiojs.h>

typedef enum {
    IOJS_ADD_SCRIPT = 1,
    IOJS_CALL_SCRIPT,
} iojsToJSCommandType;


typedef struct {
    iojsToJSCommandType   type;
    void                 *data;
} iojsToJS;


typedef struct {
    char  *filename;
    int    id;
} iojsAddCommand;


int
        iojsSendToJS     (iojsToJS *cmd);
int
        iojsSendFromJS   (iojsFromJS *cmd);
iojsToJS*
        iojsRecvToJS     (void);
void
        iojsToJSFree     (iojsToJS *cmd);

#endif /* __LIBIOJSINTERNALS_H__ */
