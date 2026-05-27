#include <stdio.h>

#include "attach_fsync.h"

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

int bs_attach_fsync_file(void* file_handle)
{
    FILE* f = (FILE*)file_handle;
    if (!f)
        return -1;
    if (fflush(f) != 0)
        return -1;
#if defined(_WIN32)
    const int fd = _fileno(f);
    if (fd < 0)
        return -1;
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    if (!FlushFileBuffers(h))
        return -1;
    return 0;
#else
    const int fd = fileno(f);
    if (fd < 0)
        return -1;
    return fsync(fd) == 0 ? 0 : -1;
#endif
}
