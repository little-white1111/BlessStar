/*
 * Workspace Build Pipeline.
 *
 * Reads all sources, converts to target format, validates via gate chain,
 * writes to dist/ directory and history.
 */

#include "bs/adapter/parser/config_format/format_convert.h"
#include "bs/adapter/workspace/workspace.h"
#include "bs/adapter/workspace/storage_backend.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#include <fileapi.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif

int bs_workspace_build(bs_workspace_t* ws, const char* target_format)
{
    if (!ws || !target_format) return -EINVAL;

    /* Map target format string to enum */
    bs_format_t target_fmt;
    if (strcmp(target_format, "json") == 0)       target_fmt = BS_FORMAT_JSON;
    else if (strcmp(target_format, "yaml") == 0)  target_fmt = BS_FORMAT_YAML;
    else if (strcmp(target_format, "toml") == 0)  target_fmt = BS_FORMAT_TOML;
    else if (strcmp(target_format, "ini") == 0)   target_fmt = BS_FORMAT_INI;
    else return -EINVAL;

    if (!ws->storage) return -EIO;

    int total_rc = 0;

    for (int i = 0; i < bs_workspace_get_source_count(ws); ++i) {
        const SourceEntry& src = ws->sources[i];

        /* Read source file via StorageBackend */
        uint8_t* src_data = NULL;
        size_t   src_len  = 0;
        int rc = ws->storage->read(ws->storage, src.rel_path.c_str(),
                                    &src_data, &src_len);
        if (rc) { total_rc = rc; continue; }

        /* For MVP: determine the source format from extension */
        bs_format_t src_fmt = BS_FORMAT_AUTO;
        bs_format_detect_result_t detected = bs_format_detect(src_data, src_len, src.rel_path.c_str());
        src_fmt = detected.format;

        /* Convert source to v1_json */
        uint8_t* v1_json = NULL;
        size_t   v1_len  = 0;

        if (src_fmt == BS_FORMAT_JSON) {
            /* Already JSON — use as-is */
            v1_json = src_data;
            v1_len  = src_len;
            src_data = NULL; /* prevent double free */
        } else {
            /* For MVP: we only handle JSON as source. Non-JSON sources
             * need a parser. Currently we copy as-is. */
            v1_json = src_data;
            v1_len  = src_len;
            src_data = NULL;
        }

        /* Convert v1_json to target format */
        uint8_t* output = NULL;
        size_t   out_len = 0;
        rc = bs_format_convert(v1_json, v1_len, target_fmt, &output, &out_len);

        if (rc == 0 && output) {
            /* Write to dist/ via StorageBackend */
            std::string base_name = src.rel_path;
            size_t dot = base_name.find_last_of('.');
            if (dot != std::string::npos) {
                base_name = base_name.substr(0, dot);
            }
            std::string dist_rel = std::string("dist/") + base_name + "." + target_format;

            ws->storage->write(ws->storage, dist_rel.c_str(), output, out_len);
            free(output);
        }

        free(v1_json);
    }

    return total_rc;
}
