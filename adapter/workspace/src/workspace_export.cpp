/*
 * Workspace Export — export a single source to a specific format.
 */

#include "bs/adapter/parser/config_format/format_convert.h"
#include "bs/adapter/workspace/workspace.h"
#include "bs/adapter/workspace/storage_backend.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

int bs_workspace_export(bs_workspace_t* ws, const char* src_name,
                         const char* target_format, const char* output_path)
{
    if (!ws || !src_name || !target_format || !output_path) return -EINVAL;

    /* Find the source */
    int found = 0;
    for (size_t i = 0; i < ws->sources.size(); ++i) {
        if (ws->sources[i].rel_path == src_name) { found = 1; break; }
    }
    if (!found) return -ENOENT;

    /* Read source via StorageBackend */
    if (!ws->storage || !ws->storage->read) return -EIO;
    uint8_t* data = NULL;
    size_t size = 0;
    int rc = ws->storage->read(ws->storage, src_name, &data, &size);
    if (rc) return rc;

    /* Map format string */
    bs_format_t fmt;
    if      (strcmp(target_format, "json") == 0) fmt = BS_FORMAT_JSON;
    else if (strcmp(target_format, "yaml") == 0) fmt = BS_FORMAT_YAML;
    else if (strcmp(target_format, "toml") == 0) fmt = BS_FORMAT_TOML;
    else if (strcmp(target_format, "ini")  == 0) fmt = BS_FORMAT_INI;
    else { free(data); return -EINVAL; }

    /* Convert */
    uint8_t* out = NULL;
    size_t out_len = 0;
    rc = bs_format_convert(data, size + 1, fmt, &out, &out_len);
    free(data);

    if (rc) return rc;

    /* Write output */
    std::ofstream ofs(output_path, std::ios::binary);
    if (!ofs) { free(out); return -EIO; }
    ofs.write((const char*)out, out_len - 1);
    ofs.close();
    free(out);

    return 0;
}
