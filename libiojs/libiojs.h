/*
 * Copyright Marat Abdullin (https://github.com/hoho)
 */

#ifndef __LIBIOJS_H__
#define __LIBIOJS_H__


#include <libiojsExports.h>


typedef enum {
    IOJS_CHUNK = 1,
    IOJS_SUBREQUEST,
} iojsFromJSCommandType;


typedef struct {
    iojsFromJSCommandType   type;
    void                   *data;
} iojsFromJS;


#ifdef __cplusplus
extern "C" {
#endif


LIBIOJSPUBFUN int LIBIOJSCALL
        iojsStart                (int *fd);

LIBIOJSPUBFUN int LIBIOJSCALL
        iojsAddJS                (char *filename, int *id);
LIBIOJSPUBFUN void LIBIOJSCALL
        iojsWaitAddJS            (void);

LIBIOJSPUBFUN iojsFromJS* LIBIOJSCALL
        iojsRecvFromJS           (void);
LIBIOJSPUBFUN void LIBIOJSCALL
        iojsFromJSFree           (iojsFromJS *cmd);


#ifdef __cplusplus
}
#endif

#endif /* __LIBIOJS_H__ */
