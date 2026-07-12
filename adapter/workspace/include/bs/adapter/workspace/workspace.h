#ifndef BS_ADAPTER_WORKSPACE_H
#define BS_ADAPTER_WORKSPACE_H

/*
 * C-ST-7 contract block:
 * Thread safety: one bs_workspace_t per thread; no concurrent access.
 * Error semantics: negative errno codes on error.
 * Platform notes: Windows uses CreateDirectoryA / GetFileAttributesA.
 *
 * ADR-全链路接通 — Project Workspace Service.
 * Workspace 数据主权：所有配置文件读写必须经过此 C ABI（不变量 #2）。
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#include <string>
#include <vector>

/* Forward declaration of StorageBackend vtable */
struct StorageBackend;

/**
 * SourceEntry — represents a registered source file in the workspace.
 * Defined here (not in workspace.cpp) so that workspace_build.cpp and
 * workspace_export.cpp can access ws->sources[] directly.
 */
struct SourceEntry {
    std::string rel_path;
    std::string format;  // "auto", "json", "yaml", "toml", "ini"
};

struct bs_workspace_t {
    std::string               project_root;
    std::vector<SourceEntry>  sources;
    StorageBackend*           storage;    /* ADR-workspace加固: 可插拔存储后端 */
};

extern "C"
{
#endif

    /** Opaque workspace handle (C view). */
    typedef struct bs_workspace_t bs_workspace_t;

    /** A history entry. */
    typedef struct bs_workspace_history_entry_t
    {
        uint64_t   timestamp;   /**< Unix timestamp in ms */
        char       src_name[256]; /**< Source file name */
        size_t     data_len;    /**< Size of stored data */
    } bs_workspace_history_entry_t;

    /* ─── Lifecycle ──────────────────────────────────────────────── */

    /**
     * Create a new workspace project at the given root path.
     * Creates .blessstar/ directory and workspace.yaml metadata.
     */
    bs_workspace_t* bs_workspace_create(const char* project_root);

    /**
     * Open an existing workspace project.
     * Reads .blessstar/workspace.yaml and loads source registry.
     */
    int bs_workspace_open(bs_workspace_t* ws);

    /* ─── Source management ──────────────────────────────────────── */

    /**
     * Register a source file in the workspace.
     * @param ws        Workspace handle.
     * @param rel_path  Path relative to project root (e.g. "src/dev.yaml").
     * @param format    Format hint ("auto", "json", "yaml", "toml", "ini"), or NULL for auto.
     */
    int bs_workspace_add_source(bs_workspace_t* ws, const char* rel_path,
                                 const char* format);

    int bs_workspace_remove_source(bs_workspace_t* ws, const char* rel_path);

    /* ─── Config read/write ───────────────────────────────────────── */

    /**
     * Read a source file through the workspace pipeline:
     *   detect → parse → validate → cache.
     * Returns v1 JSON bytes.
     */
    int bs_workspace_read(bs_workspace_t* ws, const char* src_name,
                          uint8_t** out_data, size_t* out_len);

    /**
     * Write v1 JSON data to a source file.
     * The data is converted back to the source's original format.
     */
    int bs_workspace_write(bs_workspace_t* ws, const char* src_name,
                           const uint8_t* data, size_t len);

    /* ─── Build & Export ─────────────────────────────────────────── */

    /**
     * Build all sources in the workspace, producing target-format output
     * in the dist/ directory. Runs gate validation on each source.
     * @param target_format Output format ("json", "yaml", "toml", "ini").
     */
    int bs_workspace_build(bs_workspace_t* ws, const char* target_format);

    /**
     * Export a single source to a specific format and output path.
     */
    int bs_workspace_export(bs_workspace_t* ws, const char* src_name,
                            const char* target_format, const char* output_path);

    /* ─── History ─────────────────────────────────────────────────── */

    int bs_workspace_list_history(bs_workspace_t* ws, const char* src_name,
                                   bs_workspace_history_entry_t* entries,
                                   size_t* count);

    int bs_workspace_rollback(bs_workspace_t* ws, const char* src_name,
                               uint64_t timestamp);

    /* ─── Metadata ────────────────────────────────────────────────── */

    const char* bs_workspace_get_project_root(bs_workspace_t* ws);
    int         bs_workspace_get_source_count(bs_workspace_t* ws);

    void bs_workspace_destroy(bs_workspace_t* ws);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_WORKSPACE_H */
