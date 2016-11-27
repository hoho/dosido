#include <stdio.h>
#include <stdarg.h>
#include "libnodejs.h"


static void
log(unsigned level, const char *fmt, ...)
{
    va_list  args;
    va_start(args, fmt);
    fprintf(stderr, "%d: ", level);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}


#ifdef _WIN32
#include <VersionHelpers.h>
#include <WinError.h>

int wmain(int argc, wchar_t *wargv[]) {
    int fd = -1;
    nodejsStart(log, &fd);
    nodejsStop();
}
#else
// UNIX
int main(int argc, char *argv[]) {
    int fd = -1;
    nodejsStart(log, &fd);
    nodejsStop();
}
#endif
