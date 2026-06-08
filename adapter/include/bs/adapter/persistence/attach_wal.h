#ifndef BS_ADAPTER_PERSISTENCE_ATTACH_WAL_H
#define BS_ADAPTER_PERSISTENCE_ATTACH_WAL_H

/*
 * C-ST-7 contract block:
 * Thread safety: WAL append is serialized by attach_store caller.
 * Error semantics: Non-zero on checksum or fsync failure.
 * Platform notes: Write-ahead log segments for crash recovery (MVP subset).
 */

#include "bs/adapter/persistence/attach_store.h"

#include <stddef.h>
#include <stdint.h>

#if defined(BS_TESTING)
#include <stdio.h>
#endif

#ifndef BS_ATTACH_WAL_MAX_RECORD_BYTES
/** ATOM-WAL-DBG-3: Any record len above cap is corruption. */
#define BS_ATTACH_WAL_MAX_RECORD_BYTES (64u * 1024u)
#endif

#ifndef BS_ATTACH_WAL_PURGE_KEEP_EPOCHS
/** ATOM-PURGE-11: retain history segments with epoch > manifest_epoch - K. */
#define BS_ATTACH_WAL_PURGE_KEEP_EPOCHS (2u)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct BsAttachWal BsAttachWal;

    typedef struct BsAttachWalEntry
    {
        const char* uri;
        const char* staging_path;
        uint64_t    expected_rev;
        uint64_t    new_rev;
        uint32_t    payload_checksum;
    } BsAttachWalEntry;

    /** REC-A'-7 / T-REC-6: per_batch reload FSM phase markers (WAL only). */
    typedef enum BsAttachWalRecoverPhase
    {
        BS_ATTACH_WAL_PHASE_STAGE   = 1,
        BS_ATTACH_WAL_PHASE_GATE    = 2,
        BS_ATTACH_WAL_PHASE_EXEC    = 3,
        BS_ATTACH_WAL_PHASE_PERSIST = 4,
        BS_ATTACH_WAL_PHASE_COMMIT  = 5
    } BsAttachWalRecoverPhase;

    BsAttachWal* bs_adapter_attach_persist_wal_open(const char* wal_path);
    void         bs_adapter_attach_persist_wal_close(BsAttachWal* wal);

    /** Append batch intent and fsync (ATOM-XIV-3). */
    int bs_adapter_attach_persist_wal_append_batch(BsAttachWal* wal, uint64_t epoch,
                                                   const BsAttachWalEntry* entries, size_t count);

    /** Record batch completion after manifest flip. */
    int bs_adapter_attach_persist_wal_mark_committed(BsAttachWal* wal, uint64_t epoch);

    /**
     * Append PHASE_MARK { batch_epoch, phase, uri_set_hash } for per_batch crash recovery.
     * No-op when @p wal is NULL (in-memory store).
     */
    int bs_adapter_attach_persist_wal_append_phase_mark(BsAttachWal* wal, uint64_t batch_epoch,
                                                        BsAttachWalRecoverPhase phase,
                                                        uint32_t                  uri_set_hash);

    /** 1 if the last recover_unfinished detected EXEC-or-later orphan rollback (REC-A'-7). */
    int bs_adapter_attach_persist_wal_had_exec_rollback(const BsAttachWal* wal,
                                                          uint64_t*          epoch_out);

    /**
     * Remove orphan staging files for batches with epoch > @p manifest_epoch that lack commit.
     * @return BS_ATTACH_OK or BS_ATTACH_ERR_IO.
     */
    int bs_adapter_attach_persist_wal_recover_unfinished(BsAttachWal* wal, uint64_t manifest_epoch);

    /** P3: purge history segments `active.wal.e{N}` when safe (see ATOM-PURGE-9..12). */
    int bs_adapter_attach_persist_wal_purge_old_segments(const char* active_wal_path,
                                                         uint64_t    manifest_epoch);

#if defined(BS_TESTING)
    /**
     * Debug-only dump for framing WAL (H2).
     *
     * @param wal WAL handle (path).
     * @param epoch_filter If non-zero, dump only records related to this epoch (best-effort).
     * @param from_offset Start reading at this byte offset.
     * @param max_records Max number of records to dump.
     * @param out Output stream.
     */
    int bs_adapter_attach_persist_wal_dump(BsAttachWal* wal, uint64_t epoch_filter,
                                           uint64_t from_offset, size_t max_records, FILE* out);

    /** Count records of @p type in the active WAL segment (testing / AG-REC-SCHEME-1). */
    int bs_adapter_attach_persist_wal_count_record_type(BsAttachWal* wal, uint16_t type,
                                                        size_t* out_count);
#endif

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PERSISTENCE_ATTACH_WAL_H */
