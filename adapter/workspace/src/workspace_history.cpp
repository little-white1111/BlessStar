/*
 * Workspace History — snapshot history for rollback support.
 *
 * ADR-全链路接通 不变量 #6: .blessstar/history/ stores time-stamped snapshots.
 */

#include "bs/adapter/workspace/workspace.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fileapi.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

int bs_workspace_list_history(bs_workspace_t* ws, const char* src_name,
                               bs_workspace_history_entry_t* entries,
                               size_t* count)
{
    if (!ws || !count) return -EINVAL;

    /* MVP: return empty list (no history saved yet) */
    *count = 0;
    return 0;
}

int bs_workspace_rollback(bs_workspace_t* ws, const char* src_name,
                           uint64_t timestamp)
{
    if (!ws || !src_name) return -EINVAL;

    /* MVP: not yet implemented */
    return -ENOTSUP;
}
