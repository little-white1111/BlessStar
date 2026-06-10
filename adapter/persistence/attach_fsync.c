#include "bs/kernel/common/bs_wait_trace.h"

#include <stdio.h>

#include "attach_fsync.h"

#if defined(_WIN32)
#include <io.h>
#include <windows.h>

typedef struct FsyncPoolCtx
{
    HANDLE file;
    int    result;
} FsyncPoolCtx;

static VOID CALLBACK fsync_pool_callback(PTP_CALLBACK_INSTANCE instance, PVOID context,
                                         PTP_WORK work)
{
    (void)instance;
    (void)work;
    FsyncPoolCtx* ctx = (FsyncPoolCtx*)context;
    ctx->result       = FlushFileBuffers(ctx->file) ? 0 : -1;
}

static int fsync_os_handle_threadpool(HANDLE h)
{
    FsyncPoolCtx ctx;
    ctx.file   = h;
    ctx.result = -1;

    PTP_WORK work = CreateThreadpoolWork(fsync_pool_callback, &ctx, NULL);
    if (!work)
        return -1;
    SubmitThreadpoolWork(work);
    WaitForThreadpoolWorkCallbacks(work, FALSE);
    CloseThreadpoolWork(work);
    return ctx.result;
}
#else
#include <unistd.h>
#endif

int bs_adapter_attach_persist_fsync_os_handle(void* os_file_handle)
{
#if defined(_WIN32)
    HANDLE h = (HANDLE)os_file_handle;
    if (!h || h == INVALID_HANDLE_VALUE)
        return -1;
    return fsync_os_handle_threadpool(h);
#else
    const int fd = (int)(intptr_t)os_file_handle;
    if (fd < 0)
        return -1;
    return fsync(fd) == 0 ? 0 : -1;
#endif
}

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
    const int rc = bs_adapter_attach_persist_fsync_os_handle(h);
    if (io_t0 >= 0)
        bs_wait_trace_hang_end("persist_io:fsync", io_t0);
    return rc;
#else
    const int fd = fileno(f);
    if (fd < 0)
    {
        if (io_t0 >= 0)
            bs_wait_trace_hang_end("persist_io:fsync", io_t0);
        return -1;
    }
    const int rc = bs_adapter_attach_persist_fsync_os_handle((void*)(intptr_t)fd);
    if (io_t0 >= 0)
        bs_wait_trace_hang_end("persist_io:fsync", io_t0);
    return rc;
#endif
}
