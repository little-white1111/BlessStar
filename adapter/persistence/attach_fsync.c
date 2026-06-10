#include <stdio.h>

#include "bs/kernel/common/bs_wait_trace.h"

#include "attach_fsync.h"

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

int bs_adapter_attach_persist_fsync_file(void* file_handle)
{
    const int io_t0 = bs_wait_trace_hang_begin("persist_io:fsync");
    FILE*     f     = (FILE*)file_handle;
    if (!f)
    {
        if (io_t0 >= 0)
            bs_wait_trace_hang_end("persist_io:fsync", io_t0);
        return -1;
    }
    if (fflush(f) != 0)
    {
        if (io_t0 >= 0)
            bs_wait_trace_hang_end("persist_io:fsync", io_t0);
        return -1;
    }
#if defined(_WIN32)
    const int fd = _fileno(f);
    if (fd < 0)
    {
        if (io_t0 >= 0)
            bs_wait_trace_hang_end("persist_io:fsync", io_t0);
        return -1;
    }
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE)
    {
        if (io_t0 >= 0)
            bs_wait_trace_hang_end("persist_io:fsync", io_t0);
        return -1;
    }
    if (!FlushFileBuffers(h))
    {
        if (io_t0 >= 0)
            bs_wait_trace_hang_end("persist_io:fsync", io_t0);
        return -1;
    }
    if (io_t0 >= 0)
        bs_wait_trace_hang_end("persist_io:fsync", io_t0);
    return 0;
#else
    const int fd = fileno(f);
    if (fd < 0)
    {
        if (io_t0 >= 0)
            bs_wait_trace_hang_end("persist_io:fsync", io_t0);
        return -1;
    }
    const int rc = fsync(fd) == 0 ? 0 : -1;
    if (io_t0 >= 0)
        bs_wait_trace_hang_end("persist_io:fsync", io_t0);
    return rc;
#endif
}
