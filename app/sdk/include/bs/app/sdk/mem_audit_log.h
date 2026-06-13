#ifndef BS_APP_SDK_MEM_AUDIT_LOG_H
#define BS_APP_SDK_MEM_AUDIT_LOG_H

/*
 * C-ST-7 contract block:
 * Thread safety: MemAuditLog is not thread-safe; use one instance per session thread.
 * Error semantics: Init/Record return false on I/O failure; GetLastError() provides detail.
 * Platform notes: Writes JSON manifest and binary snapshot files to an audit directory.
 *   Max 5 snapshots per key (MRU-evict).
 */

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace bs::app
{

/**
 * MEM audit log — write snapshots of in-memory config submissions to disk for audit trail.
 *
 * - manifest.json tracks per-key sequence numbers and snapshot count.
 * - Each record writes {key}.v{N}.bin (binary copy) and bumps seq.
 * - When snapshot_count exceeds max_snapshots_per_key (5), the oldest file is removed.
 */
class MemAuditLog
{
public:
    static constexpr unsigned kMaxSnapshotsPerKey = 5;

    MemAuditLog() = default;
    ~MemAuditLog();

    MemAuditLog(const MemAuditLog&)            = delete;
    MemAuditLog& operator=(const MemAuditLog&) = delete;

    /** Initialize with audit directory. Creates directory if needed. Returns false on I/O error. */
    bool Init(const char* audit_dir);

    /** True if Init() succeeded. */
    bool initialized() const { return initialized_; }

    /**
     * Write a snapshot record for @p key with @p data / @p len.
     * Bumps sequence number, writes .bin file, updates manifest, evicts oldest if >5.
     * Returns false on I/O failure.
     */
    bool Record(const char* key, const void* data, size_t len);

    /** Human-readable last error (for diagnostics). */
    const char* GetLastError() const { return last_error_.c_str(); }

private:
    bool LoadManifest();
    bool SaveManifest();
    bool RemoveSnapshot(const char* key, unsigned seq);

    std::string audit_dir_;
    std::string last_error_;
    bool        initialized_ = false;

    /** Per-key state loaded from / written to manifest. */
    struct KeyState
    {
        unsigned current_seq      = 0;
        unsigned snapshot_count   = 0;
    };
    // Inline map; small (typically 1-10 keys).
    // We use a sorted container for deterministic manifest output.
    // For simplicity, we use a vector and rebuild on each save.
    struct KeyEntry
    {
        std::string key;
        KeyState    state;
    };
    std::vector<KeyEntry> keys_;
};

} // namespace bs::app

#endif // BS_APP_SDK_MEM_AUDIT_LOG_H
