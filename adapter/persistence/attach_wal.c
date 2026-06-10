#include "bs/adapter/persistence/attach_wal.h"
#include "bs/adapter/persistence/attach_watch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attach_crc32.h"
#include "attach_fsync.h"

/**
 * H2: binary record framing WAL.
 *
 * Record layout (little-endian fields):
 *   u32 magic
 *   u16 version
 *   u16 type
 *   u32 len
 *   u32 crc32  (over header_without_crc + payload)
 *   u8[len] payload
 *
 * Any framing error is treated as corruption. Per ATOM-REC-SAFE-2, corruption => no deletions.
 */

struct BsAttachWal
{
    char*    path;
    int      exec_rollback_detected;
    uint64_t exec_rollback_epoch;
};

enum
{
    BS_ATTACH_WAL_MAGIC   = 0x42535741u, /* 'A''W''S''B' (BlessStar WAL) */
    BS_ATTACH_WAL_VERSION = 1u
};

typedef enum BsAttachWalRecordType
{
    BS_ATTACH_WAL_REC_BATCH_BEGIN = 1,
    BS_ATTACH_WAL_REC_ENTRY       = 2,
    BS_ATTACH_WAL_REC_BATCH_END   = 3,
    BS_ATTACH_WAL_REC_COMMITTED   = 4,
    BS_ATTACH_WAL_REC_PHASE_MARK  = 5
} BsAttachWalRecordType;

#define BS_ATTACH_WAL_MAX_PHASE_TRACK 32u

typedef struct WalPhaseTrack
{
    uint64_t epoch;
    uint32_t max_phase;
} WalPhaseTrack;

static void wal_phase_track_update(WalPhaseTrack* tracks, size_t* track_count, uint64_t epoch,
                                   uint32_t phase)
{
    if (!tracks || !track_count || epoch == 0)
        return;
    for (size_t i = 0; i < *track_count; ++i)
    {
        if (tracks[i].epoch == epoch)
        {
            if (phase > tracks[i].max_phase)
                tracks[i].max_phase = phase;
            return;
        }
    }
    if (*track_count >= BS_ATTACH_WAL_MAX_PHASE_TRACK)
        return;
    tracks[*track_count].epoch     = epoch;
    tracks[*track_count].max_phase = phase;
    ++(*track_count);
}

static uint32_t wal_max_phase_for_epoch(const WalPhaseTrack* tracks, size_t track_count,
                                        uint64_t epoch)
{
    for (size_t i = 0; i < track_count; ++i)
    {
        if (tracks[i].epoch == epoch)
            return tracks[i].max_phase;
    }
    return 0;
}

static void wal_delete_staging_paths(char** staging_paths, uint32_t count)
{
    if (!staging_paths)
        return;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (staging_paths[i] && staging_paths[i][0] != '\0')
            (void)remove(staging_paths[i]);
    }
}

#ifndef BS_ATTACH_WAL_MAX_RECORD_BYTES
#define BS_ATTACH_WAL_MAX_RECORD_BYTES (64u * 1024u)
#endif

#if defined(BS_TESTING)
#define BS_ATTACH_WAL_DBGF(...) (fprintf(stderr, __VA_ARGS__))
#else
#define BS_ATTACH_WAL_DBGF(...) ((void)0)
#endif

static int wal_scan_last_committed_epoch(FILE* f, uint64_t* last_committed_out, int* corrupted_out);

static void write_u16_le(uint8_t* dst, uint16_t v)
{
    dst[0] = (uint8_t)(v & 0xFFu);
    dst[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void write_u32_le(uint8_t* dst, uint32_t v)
{
    dst[0] = (uint8_t)(v & 0xFFu);
    dst[1] = (uint8_t)((v >> 8) & 0xFFu);
    dst[2] = (uint8_t)((v >> 16) & 0xFFu);
    dst[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void write_u64_le(uint8_t* dst, uint64_t v)
{
    write_u32_le(dst, (uint32_t)(v & 0xFFFFFFFFu));
    write_u32_le(dst + 4, (uint32_t)((v >> 32) & 0xFFFFFFFFu));
}

static uint16_t read_u16_le(const uint8_t* src)
{
    return (uint16_t)src[0] | (uint16_t)((uint16_t)src[1] << 8);
}

static uint32_t read_u32_le(const uint8_t* src)
{
    return (uint32_t)src[0] | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

static uint64_t read_u64_le(const uint8_t* src)
{
    const uint64_t lo = (uint64_t)read_u32_le(src);
    const uint64_t hi = (uint64_t)read_u32_le(src + 4);
    return lo | (hi << 32);
}

static int wal_write_all(FILE* f, const void* buf, size_t n)
{
    if (n == 0)
        return BS_ATTACH_OK;
    if (fwrite(buf, 1, n, f) != n)
        return BS_ATTACH_ERR_IO;
    return BS_ATTACH_OK;
}

static int wal_write_record(FILE* f, uint16_t type, const uint8_t* payload, uint32_t len)
{
    if (!f)
        return BS_ATTACH_ERR_INVALID_ARG;
    if (len > (uint32_t)BS_ATTACH_WAL_MAX_RECORD_BYTES)
        return BS_ATTACH_ERR_LIMIT;

    uint8_t hdr_no_crc[4 + 2 + 2 + 4];
    write_u32_le(hdr_no_crc + 0, (uint32_t)BS_ATTACH_WAL_MAGIC);
    write_u16_le(hdr_no_crc + 4, (uint16_t)BS_ATTACH_WAL_VERSION);
    write_u16_le(hdr_no_crc + 6, (uint16_t)type);
    write_u32_le(hdr_no_crc + 8, (uint32_t)len);

    uint32_t crc = bs_adapter_attach_persist_crc32(hdr_no_crc, sizeof(hdr_no_crc));
    if (len > 0 && payload)
    {
        /* crc32 over header_without_crc + payload: just hash concatenation by hashing a temp */
        /* Since bs_adapter_attach_persist_crc32 isn't incremental, build one buffer when needed. */
        uint8_t* tmp = (uint8_t*)malloc(sizeof(hdr_no_crc) + len);
        if (!tmp)
            return BS_ATTACH_ERR_OOM;
        memcpy(tmp, hdr_no_crc, sizeof(hdr_no_crc));
        memcpy(tmp + sizeof(hdr_no_crc), payload, len);
        crc = bs_adapter_attach_persist_crc32(tmp, sizeof(hdr_no_crc) + len);
        free(tmp);
    }

    uint8_t hdr[4 + 2 + 2 + 4 + 4];
    memcpy(hdr, hdr_no_crc, sizeof(hdr_no_crc));
    write_u32_le(hdr + sizeof(hdr_no_crc), crc);

    int rc = wal_write_all(f, hdr, sizeof(hdr));
    if (rc != BS_ATTACH_OK)
        return rc;
    rc = wal_write_all(f, payload, (size_t)len);
    return rc;
}

BsAttachWal* bs_adapter_attach_persist_wal_open(const char* wal_path)
{
    if (!wal_path || wal_path[0] == '\0')
        return NULL;
    BsAttachWal* w = (BsAttachWal*)calloc(1, sizeof(BsAttachWal));
    if (!w)
        return NULL;
    const size_t n = strlen(wal_path) + 1;
    w->path        = (char*)malloc(n);
    if (!w->path)
    {
        free(w);
        return NULL;
    }
    memcpy(w->path, wal_path, n);
    w->exec_rollback_detected = 0;
    w->exec_rollback_epoch    = 0;
    return w;
}

void bs_adapter_attach_persist_wal_close(BsAttachWal* wal)
{
    if (!wal)
        return;
    free(wal->path);
    free(wal);
}

int bs_adapter_attach_persist_wal_append_batch(BsAttachWal* wal, uint64_t epoch,
                                               const BsAttachWalEntry* entries, size_t count)
{
    if (!wal || !wal->path || !entries || count == 0)
        return BS_ATTACH_ERR_INVALID_ARG;

    FILE* f = fopen(wal->path, "ab");
    if (!f)
    {
        return BS_ATTACH_ERR_IO;
    }

    if (count > 0xFFFFFFFFu)
    {
        fclose(f);
        return BS_ATTACH_ERR_LIMIT;
    }

    uint8_t begin_payload[8 + 4];
    write_u64_le(begin_payload + 0, epoch);
    write_u32_le(begin_payload + 8, (uint32_t)count);
    int rc = wal_write_record(f, (uint16_t)BS_ATTACH_WAL_REC_BATCH_BEGIN, begin_payload,
                              (uint32_t)sizeof(begin_payload));
    if (rc != BS_ATTACH_OK)
    {
        fclose(f);
        return rc;
    }

    uint32_t* entry_crcs = (uint32_t*)calloc(count, sizeof(uint32_t));
    if (!entry_crcs)
    {
        fclose(f);
        return BS_ATTACH_ERR_OOM;
    }

    for (size_t i = 0; i < count; ++i)
    {
        const char*  uri    = entries[i].uri ? entries[i].uri : "";
        const char*  path   = entries[i].staging_path ? entries[i].staging_path : "";
        const size_t uri_n  = strlen(uri);
        const size_t path_n = strlen(path);
        if (uri_n > (size_t)BS_ATTACH_WAL_MAX_RECORD_BYTES ||
            path_n > (size_t)BS_ATTACH_WAL_MAX_RECORD_BYTES)
        {
            free(entry_crcs);
            fclose(f);
            return BS_ATTACH_ERR_LIMIT;
        }

        const uint32_t payload_len = 4u + (uint32_t)uri_n + 4u + (uint32_t)path_n + 8u + 8u + 4u;
        if (payload_len > (uint32_t)BS_ATTACH_WAL_MAX_RECORD_BYTES)
        {
            free(entry_crcs);
            fclose(f);
            return BS_ATTACH_ERR_LIMIT;
        }

        uint8_t* payload = (uint8_t*)malloc(payload_len);
        if (!payload)
        {
            free(entry_crcs);
            fclose(f);
            return BS_ATTACH_ERR_OOM;
        }
        uint32_t off = 0;
        write_u32_le(payload + off, (uint32_t)uri_n);
        off += 4;
        memcpy(payload + off, uri, uri_n);
        off += (uint32_t)uri_n;
        write_u32_le(payload + off, (uint32_t)path_n);
        off += 4;
        memcpy(payload + off, path, path_n);
        off += (uint32_t)path_n;
        write_u64_le(payload + off, entries[i].expected_rev);
        off += 8;
        write_u64_le(payload + off, entries[i].new_rev);
        off += 8;
        write_u32_le(payload + off, entries[i].payload_checksum);
        off += 4;

        entry_crcs[i] = bs_adapter_attach_persist_crc32(payload, payload_len);
        rc = wal_write_record(f, (uint16_t)BS_ATTACH_WAL_REC_ENTRY, payload, payload_len);
        free(payload);
        if (rc != BS_ATTACH_OK)
        {
            free(entry_crcs);
            fclose(f);
            return rc;
        }
    }

    /* BATCH_END: epoch + entry_count + batch_hash */
    const uint32_t batch_hash =
        bs_adapter_attach_persist_crc32(entry_crcs, count * sizeof(uint32_t));
    free(entry_crcs);
    uint8_t end_payload[8 + 4 + 4];
    write_u64_le(end_payload + 0, epoch);
    write_u32_le(end_payload + 8, (uint32_t)count);
    write_u32_le(end_payload + 12, batch_hash);
    rc = wal_write_record(f, (uint16_t)BS_ATTACH_WAL_REC_BATCH_END, end_payload,
                          (uint32_t)sizeof(end_payload));
    if (rc != BS_ATTACH_OK)
    {
        fclose(f);
        return rc;
    }

    if (bs_adapter_attach_persist_fsync_file(f) != 0)
    {
        fclose(f);
        return BS_ATTACH_ERR_IO;
    }
    fclose(f);
    return BS_ATTACH_OK;
}

static int wal_build_segment_path(const char* active_path, uint64_t epoch, char* out, size_t cap)
{
    if (!active_path || !out || cap == 0)
        return BS_ATTACH_ERR_INVALID_ARG;
    char num[24];
    snprintf(num, sizeof(num), "%llu", (unsigned long long)epoch);
    const size_t need = strlen(active_path) + 2 + strlen(num) + 1;
    if (need > cap)
        return BS_ATTACH_ERR_LIMIT;
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif
    snprintf(out, cap, "%s.e%s", active_path, num);
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
    return BS_ATTACH_OK;
}

static int wal_file_nonempty(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f)
        return 0;
    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return 0;
    }
    const long sz = ftell(f);
    fclose(f);
    return sz > 0;
}

static int wal_rotate_active_segment(const char* active_path, uint64_t epoch)
{
    if (!active_path || active_path[0] == '\0')
        return BS_ATTACH_ERR_INVALID_ARG;
    if (!wal_file_nonempty(active_path))
        return BS_ATTACH_OK;

    char seg[4096];
    if (wal_build_segment_path(active_path, epoch, seg, sizeof(seg)) != BS_ATTACH_OK)
        return BS_ATTACH_ERR_LIMIT;
    (void)remove(seg);
    if (rename(active_path, seg) != 0)
        return BS_ATTACH_ERR_IO;
    return BS_ATTACH_OK;
}

int bs_adapter_attach_persist_wal_mark_committed(BsAttachWal* wal, uint64_t epoch)
{
    if (!wal || !wal->path)
        return BS_ATTACH_ERR_INVALID_ARG;

    FILE* f = fopen(wal->path, "ab");
    if (!f)
        return BS_ATTACH_ERR_IO;

    uint8_t payload[8];
    write_u64_le(payload, epoch);
    const int rc = wal_write_record(f, (uint16_t)BS_ATTACH_WAL_REC_COMMITTED, payload,
                                    (uint32_t)sizeof(payload));
    if (rc != BS_ATTACH_OK)
    {
        fclose(f);
        return rc;
    }
    if (bs_adapter_attach_persist_fsync_file(f) != 0)
    {
        fclose(f);
        return BS_ATTACH_ERR_IO;
    }
    fclose(f);

    const int rot = wal_rotate_active_segment(wal->path, epoch);
    return rot == BS_ATTACH_OK ? BS_ATTACH_OK : BS_ATTACH_ERR_IO;
}

int bs_adapter_attach_persist_wal_append_phase_mark(BsAttachWal* wal, uint64_t batch_epoch,
                                                    BsAttachWalRecoverPhase phase,
                                                    uint32_t                uri_set_hash)
{
    if (!wal || !wal->path)
        return BS_ATTACH_OK;
    if (batch_epoch == 0 || phase == 0)
        return BS_ATTACH_ERR_INVALID_ARG;

    FILE* f = fopen(wal->path, "ab");
    if (!f)
        return BS_ATTACH_ERR_IO;

    uint8_t payload[8 + 4 + 4];
    write_u64_le(payload + 0, batch_epoch);
    write_u32_le(payload + 8, (uint32_t)phase);
    write_u32_le(payload + 12, uri_set_hash);
    const int rc = wal_write_record(f, (uint16_t)BS_ATTACH_WAL_REC_PHASE_MARK, payload,
                                    (uint32_t)sizeof(payload));
    if (rc != BS_ATTACH_OK)
    {
        fclose(f);
        return rc;
    }
    if (bs_adapter_attach_persist_fsync_file(f) != 0)
    {
        fclose(f);
        return BS_ATTACH_ERR_IO;
    }
    fclose(f);
    return BS_ATTACH_OK;
}

int bs_adapter_attach_persist_wal_had_exec_rollback(const BsAttachWal* wal, uint64_t* epoch_out)
{
    if (!wal)
        return 0;
    if (epoch_out)
        *epoch_out = wal->exec_rollback_epoch;
    return wal->exec_rollback_detected ? 1 : 0;
}

static int wal_read_record_header(FILE* f, uint64_t* offset_io, uint16_t* type_out,
                                  uint32_t* len_out, uint32_t* crc_out, uint8_t* hdr_no_crc_out,
                                  size_t hdr_no_crc_cap)
{
    if (!f || !offset_io || !type_out || !len_out || !crc_out || !hdr_no_crc_out ||
        hdr_no_crc_cap < (4 + 2 + 2 + 4))
        return BS_ATTACH_ERR_INVALID_ARG;

    const uint64_t off = *offset_io;
    uint8_t        hdr[4 + 2 + 2 + 4 + 4];
    if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr))
        return BS_ATTACH_ERR_IO;

    const uint32_t magic   = read_u32_le(hdr + 0);
    const uint16_t version = read_u16_le(hdr + 4);
    const uint16_t type    = read_u16_le(hdr + 6);
    const uint32_t len     = read_u32_le(hdr + 8);
    const uint32_t crc     = read_u32_le(hdr + 12);

    if (magic != (uint32_t)BS_ATTACH_WAL_MAGIC || version != (uint16_t)BS_ATTACH_WAL_VERSION)
        return BS_ATTACH_ERR_IO;
    if (len > (uint32_t)BS_ATTACH_WAL_MAX_RECORD_BYTES)
        return BS_ATTACH_ERR_LIMIT;

    memcpy(hdr_no_crc_out, hdr, 4 + 2 + 2 + 4);
    (void)off;
    *type_out = type;
    *len_out  = len;
    *crc_out  = crc;
    *offset_io += (uint64_t)sizeof(hdr);
    return BS_ATTACH_OK;
}

static int wal_scan_last_committed_epoch(FILE* f, uint64_t* last_committed_out, int* corrupted_out)
{
    if (!f || !last_committed_out || !corrupted_out)
        return BS_ATTACH_ERR_INVALID_ARG;
    *last_committed_out = 0;
    *corrupted_out      = 0;

    uint64_t offset = 0;
    for (;;)
    {
        uint16_t       type = 0;
        uint32_t       len  = 0;
        uint32_t       crc  = 0;
        uint8_t        hdr_no_crc[4 + 2 + 2 + 4];
        const uint64_t record_start = offset;
#ifndef BS_TESTING
        (void)record_start;
#endif
        const int rh =
            wal_read_record_header(f, &offset, &type, &len, &crc, hdr_no_crc, sizeof(hdr_no_crc));
        if (rh != BS_ATTACH_OK)
        {
            if (feof(f))
                break;
            *corrupted_out = 1;
            BS_ATTACH_WAL_DBGF("wal_corrupt: offset=%llu header_read_failed rc=%d\n",
                               (unsigned long long)record_start, rh);
            return BS_ATTACH_OK;
        }

        uint8_t* payload = NULL;
        if (len > 0)
        {
            payload = (uint8_t*)malloc(len);
            if (!payload)
            {
                *corrupted_out = 1;
                return BS_ATTACH_OK;
            }
            if (fread(payload, 1, len, f) != len)
            {
                free(payload);
                *corrupted_out = 1;
                BS_ATTACH_WAL_DBGF("wal_corrupt: offset=%llu type=%u len=%u payload_short\n",
                                   (unsigned long long)record_start, (unsigned)type, (unsigned)len);
                return BS_ATTACH_OK;
            }
            offset += (uint64_t)len;
        }

        uint32_t calc = 0;
        if (len == 0)
            calc = bs_adapter_attach_persist_crc32(hdr_no_crc, sizeof(hdr_no_crc));
        else
        {
            uint8_t* tmp = (uint8_t*)malloc(sizeof(hdr_no_crc) + len);
            if (!tmp)
            {
                free(payload);
                *corrupted_out = 1;
                return BS_ATTACH_OK;
            }
            memcpy(tmp, hdr_no_crc, sizeof(hdr_no_crc));
            memcpy(tmp + sizeof(hdr_no_crc), payload, len);
            calc = bs_adapter_attach_persist_crc32(tmp, sizeof(hdr_no_crc) + len);
            free(tmp);
        }
        if (calc != crc)
        {
            free(payload);
            *corrupted_out = 1;
            BS_ATTACH_WAL_DBGF("wal_corrupt: offset=%llu type=%u len=%u crc_mismatch\n",
                               (unsigned long long)record_start, (unsigned)type, (unsigned)len);
            return BS_ATTACH_OK;
        }

        if (type == (uint16_t)BS_ATTACH_WAL_REC_COMMITTED && len == 8 && payload)
        {
            const uint64_t e = read_u64_le(payload);
            if (e > *last_committed_out)
                *last_committed_out = e;
        }
        free(payload);
    }

    return BS_ATTACH_OK;
}

static uint64_t wal_max_committed_across_segments(const char* active_path, uint64_t manifest_epoch,
                                                  int* corrupted_out)
{
    uint64_t max_c = 0;
    if (corrupted_out)
        *corrupted_out = 0;
    if (!active_path)
        return 0;

    FILE* f = fopen(active_path, "rb");
    if (f)
    {
        uint64_t lc  = 0;
        int      cor = 0;
        (void)wal_scan_last_committed_epoch(f, &lc, &cor);
        fclose(f);
        if (cor)
        {
            if (corrupted_out)
                *corrupted_out = 1;
            return 0;
        }
        if (lc > max_c)
            max_c = lc;
    }

    uint64_t       lo   = 1;
    const uint64_t keep = (uint64_t)BS_ATTACH_WAL_PURGE_KEEP_EPOCHS;
    if (manifest_epoch > keep)
        lo = manifest_epoch - keep;
    const uint64_t hi = manifest_epoch + 1;

    for (uint64_t e = lo; e <= hi; ++e)
    {
        char seg[4096];
        if (wal_build_segment_path(active_path, e, seg, sizeof(seg)) != BS_ATTACH_OK)
            continue;
        f = fopen(seg, "rb");
        if (!f)
            continue;
        uint64_t lc  = 0;
        int      cor = 0;
        (void)wal_scan_last_committed_epoch(f, &lc, &cor);
        fclose(f);
        if (cor)
        {
            if (corrupted_out)
                *corrupted_out = 1;
            return 0;
        }
        if (lc > max_c)
            max_c = lc;
    }

    return max_c;
}

static int wal_purge_segment_epoch_range(const char* active_wal_path, uint64_t start_epoch,
                                         uint64_t purge_through)
{
    for (uint64_t e = start_epoch; e <= purge_through; ++e)
    {
        char seg[4096];
        if (wal_build_segment_path(active_wal_path, e, seg, sizeof(seg)) != BS_ATTACH_OK)
            continue;

        FILE* f = fopen(seg, "rb");
        if (!f)
            continue;
        uint64_t lc  = 0;
        int      cor = 0;
        (void)wal_scan_last_committed_epoch(f, &lc, &cor);
        fclose(f);
        if (cor || lc != e)
            continue;
        (void)remove(seg);
    }

    return BS_ATTACH_OK;
}

int bs_adapter_attach_persist_wal_purge_old_segments(const char* active_wal_path,
                                                     uint64_t    manifest_epoch)
{
    return bs_adapter_attach_persist_wal_purge_old_segments_ex(active_wal_path, manifest_epoch,
                                                               NULL);
}

int bs_adapter_attach_persist_wal_purge_old_segments_ex(const char* active_wal_path,
                                                        uint64_t    manifest_epoch,
                                                        uint64_t*   inout_last_purged_through)
{
    if (!active_wal_path)
        return BS_ATTACH_ERR_INVALID_ARG;

    int corrupted = 0;
    (void)wal_max_committed_across_segments(active_wal_path, manifest_epoch, &corrupted);
    if (corrupted)
        return BS_ATTACH_OK;

    const uint64_t keep = (uint64_t)BS_ATTACH_WAL_PURGE_KEEP_EPOCHS;
    if (manifest_epoch <= keep)
        return BS_ATTACH_OK;

    const uint64_t purge_through = manifest_epoch - keep;
    uint64_t       start         = 1;
    if (inout_last_purged_through && *inout_last_purged_through > 0)
        start = *inout_last_purged_through + 1;
    if (start > purge_through)
        return BS_ATTACH_OK;

    const int rc = wal_purge_segment_epoch_range(active_wal_path, start, purge_through);
    if (rc == BS_ATTACH_OK && inout_last_purged_through)
        *inout_last_purged_through = purge_through;
    return rc;
}

int bs_adapter_attach_persist_wal_recover_unfinished(BsAttachWal* wal, uint64_t manifest_epoch)
{
    if (!wal || !wal->path)
        return BS_ATTACH_OK;

    wal->exec_rollback_detected = 0;
    wal->exec_rollback_epoch    = 0;

    int            corrupted = 0;
    const uint64_t last_committed =
        wal_max_committed_across_segments(wal->path, manifest_epoch, &corrupted);
    if (corrupted)
    {
        /* ATOM-REC-SAFE-2: corruption => conservative (no deletions). */
        BS_ATTACH_WAL_DBGF("wal_recover: corrupted=1 => conservative_no_delete\n");
        BsAttachWatchEvent ev;
        ev.epoch  = manifest_epoch;
        ev.uri    = "";
        ev.stage  = BS_ATTACH_WATCH_STAGE_RECOVER_CONSERVATIVE;
        ev.result = BS_ATTACH_WATCH_RESULT_FAIL;
        (void)bs_adapter_attach_persist_watch_publish(&ev);
        return BS_ATTACH_OK;
    }

    FILE* f = fopen(wal->path, "rb");
    if (!f)
        return BS_ATTACH_OK;

    uint64_t      current_batch_epoch = 0;
    uint32_t      current_batch_count = 0;
    uint32_t      current_entry_seen  = 0;
    uint32_t*     entry_crcs          = NULL;
    char**        staging_paths       = NULL;
    int           in_batch            = 0;
    WalPhaseTrack phase_tracks[BS_ATTACH_WAL_MAX_PHASE_TRACK];
    size_t        phase_track_count = 0;

    uint64_t offset = 0;
    for (;;)
    {
        uint16_t       type = 0;
        uint32_t       len  = 0;
        uint32_t       crc  = 0;
        uint8_t        hdr_no_crc[4 + 2 + 2 + 4];
        const uint64_t record_start = offset;
#ifndef BS_TESTING
        (void)record_start;
#endif
        const int rh =
            wal_read_record_header(f, &offset, &type, &len, &crc, hdr_no_crc, sizeof(hdr_no_crc));
        if (rh != BS_ATTACH_OK)
        {
            break;
        }

        uint8_t* payload = NULL;
        if (len > 0)
        {
            payload = (uint8_t*)malloc(len);
            if (!payload)
            {
                corrupted = 1;
                break;
            }
            if (fread(payload, 1, len, f) != len)
            {
                free(payload);
                corrupted = 1;
                BS_ATTACH_WAL_DBGF("wal_corrupt: offset=%llu type=%u len=%u payload_short\n",
                                   (unsigned long long)record_start, (unsigned)type, (unsigned)len);
                break;
            }
            offset += (uint64_t)len;
        }

        uint32_t calc = 0;
        if (len == 0)
            calc = bs_adapter_attach_persist_crc32(hdr_no_crc, sizeof(hdr_no_crc));
        else
        {
            uint8_t* tmp = (uint8_t*)malloc(sizeof(hdr_no_crc) + len);
            if (!tmp)
            {
                free(payload);
                corrupted = 1;
                break;
            }
            memcpy(tmp, hdr_no_crc, sizeof(hdr_no_crc));
            memcpy(tmp + sizeof(hdr_no_crc), payload, len);
            calc = bs_adapter_attach_persist_crc32(tmp, sizeof(hdr_no_crc) + len);
            free(tmp);
        }
        if (calc != crc)
        {
            free(payload);
            corrupted = 1;
            BS_ATTACH_WAL_DBGF("wal_corrupt: offset=%llu type=%u len=%u crc_mismatch\n",
                               (unsigned long long)record_start, (unsigned)type, (unsigned)len);
            break;
        }

        if (type == (uint16_t)BS_ATTACH_WAL_REC_BATCH_BEGIN && len == 12)
        {
            if (in_batch)
            {
                corrupted = 1;
                BS_ATTACH_WAL_DBGF("wal_corrupt: offset=%llu nested_batch_begin\n",
                                   (unsigned long long)record_start);
                free(payload);
                break;
            }
            current_batch_epoch = read_u64_le(payload + 0);
            current_batch_count = read_u32_le(payload + 8);
            current_entry_seen  = 0;
            entry_crcs          = (uint32_t*)calloc(current_batch_count, sizeof(uint32_t));
            staging_paths       = (char**)calloc(current_batch_count, sizeof(char*));
            if ((!entry_crcs && current_batch_count > 0) ||
                (!staging_paths && current_batch_count > 0))
            {
                corrupted = 1;
                BS_ATTACH_WAL_DBGF("wal_corrupt: offset=%llu oom_batch_alloc epoch=%llu count=%u\n",
                                   (unsigned long long)record_start,
                                   (unsigned long long)current_batch_epoch,
                                   (unsigned)current_batch_count);
                free(payload);
                break;
            }
            in_batch = 1;
        }
        else if (type == (uint16_t)BS_ATTACH_WAL_REC_ENTRY && in_batch)
        {
            if (current_entry_seen >= current_batch_count)
            {
                corrupted = 1;
                BS_ATTACH_WAL_DBGF("wal_corrupt: offset=%llu too_many_entries epoch=%llu\n",
                                   (unsigned long long)record_start,
                                   (unsigned long long)current_batch_epoch);
                free(payload);
                break;
            }
            entry_crcs[current_entry_seen] = bs_adapter_attach_persist_crc32(payload, len);

            /* parse staging_path from payload to delete later if needed */
            uint32_t off = 0;
            if (len < 4)
            {
                corrupted = 1;
                BS_ATTACH_WAL_DBGF("wal_corrupt: offset=%llu entry_too_short epoch=%llu len=%u\n",
                                   (unsigned long long)record_start,
                                   (unsigned long long)current_batch_epoch, (unsigned)len);
                free(payload);
                break;
            }
            const uint32_t uri_n = read_u32_le(payload + off);
            off += 4;
            if (off + uri_n + 4 > len)
            {
                corrupted = 1;
                BS_ATTACH_WAL_DBGF("wal_corrupt: offset=%llu entry_uri_overflow epoch=%llu\n",
                                   (unsigned long long)record_start,
                                   (unsigned long long)current_batch_epoch);
                free(payload);
                break;
            }
            off += uri_n;
            const uint32_t path_n = read_u32_le(payload + off);
            off += 4;
            if (off + path_n > len)
            {
                corrupted = 1;
                BS_ATTACH_WAL_DBGF("wal_corrupt: offset=%llu entry_path_overflow epoch=%llu\n",
                                   (unsigned long long)record_start,
                                   (unsigned long long)current_batch_epoch);
                free(payload);
                break;
            }
            staging_paths[current_entry_seen] = (char*)malloc((size_t)path_n + 1);
            if (!staging_paths[current_entry_seen])
            {
                corrupted = 1;
                BS_ATTACH_WAL_DBGF("wal_corrupt: offset=%llu oom_path epoch=%llu\n",
                                   (unsigned long long)record_start,
                                   (unsigned long long)current_batch_epoch);
                free(payload);
                break;
            }
            memcpy(staging_paths[current_entry_seen], payload + off, path_n);
            staging_paths[current_entry_seen][path_n] = '\0';
            current_entry_seen++;
        }
        else if (type == (uint16_t)BS_ATTACH_WAL_REC_BATCH_END && in_batch && len == 16)
        {
            const uint64_t end_epoch = read_u64_le(payload + 0);
            const uint32_t end_count = read_u32_le(payload + 8);
            const uint32_t end_hash  = read_u32_le(payload + 12);
            const uint32_t calc_hash =
                bs_adapter_attach_persist_crc32(entry_crcs, end_count * sizeof(uint32_t));
            if (end_epoch != current_batch_epoch || end_count != current_batch_count ||
                end_count != current_entry_seen || end_hash != calc_hash)
            {
                corrupted = 1;
                BS_ATTACH_WAL_DBGF(
                    "wal_corrupt: offset=%llu batch_end_mismatch epoch=%llu end_epoch=%llu "
                    "count=%u seen=%u\n",
                    (unsigned long long)record_start, (unsigned long long)current_batch_epoch,
                    (unsigned long long)end_epoch, (unsigned)end_count,
                    (unsigned)current_entry_seen);
            }
            else
            {
                const int orphan = (current_batch_epoch > last_committed) ||
                                   (current_batch_epoch > manifest_epoch);
                if (orphan)
                {
                    wal_delete_staging_paths(staging_paths, current_batch_count);
                    const uint32_t max_phase = wal_max_phase_for_epoch(
                        phase_tracks, phase_track_count, current_batch_epoch);
                    if (max_phase >= (uint32_t)BS_ATTACH_WAL_PHASE_EXEC)
                    {
                        wal->exec_rollback_detected = 1;
                        wal->exec_rollback_epoch    = current_batch_epoch;
                    }
                }
            }

            for (uint32_t i = 0; i < current_batch_count; ++i)
                free(staging_paths[i]);
            free(staging_paths);
            free(entry_crcs);
            staging_paths = NULL;
            entry_crcs    = NULL;
            in_batch      = 0;
        }
        else if (type == (uint16_t)BS_ATTACH_WAL_REC_PHASE_MARK && len == 16)
        {
            const uint64_t mark_epoch = read_u64_le(payload + 0);
            const uint32_t mark_phase = read_u32_le(payload + 8);
            wal_phase_track_update(phase_tracks, &phase_track_count, mark_epoch, mark_phase);
            if (mark_epoch > last_committed && mark_epoch > manifest_epoch &&
                mark_phase >= (uint32_t)BS_ATTACH_WAL_PHASE_EXEC)
            {
                wal->exec_rollback_detected = 1;
                wal->exec_rollback_epoch    = mark_epoch;
            }
        }

        free(payload);
        if (corrupted)
            break;
    }

    if (in_batch && !corrupted)
    {
        const int orphan =
            (current_batch_epoch > last_committed) || (current_batch_epoch > manifest_epoch);
        if (orphan)
        {
            wal_delete_staging_paths(staging_paths, current_batch_count);
            const uint32_t max_phase =
                wal_max_phase_for_epoch(phase_tracks, phase_track_count, current_batch_epoch);
            if (max_phase >= (uint32_t)BS_ATTACH_WAL_PHASE_EXEC)
            {
                wal->exec_rollback_detected = 1;
                wal->exec_rollback_epoch    = current_batch_epoch;
            }
        }
    }

    if (entry_crcs || staging_paths)
    {
        for (uint32_t i = 0; i < current_batch_count; ++i)
            free(staging_paths ? staging_paths[i] : NULL);
        free(staging_paths);
        free(entry_crcs);
    }

    fclose(f);
    return BS_ATTACH_OK;
}

#if defined(BS_TESTING)
int bs_adapter_attach_persist_wal_dump(BsAttachWal* wal, uint64_t epoch_filter,
                                       uint64_t from_offset, size_t max_records, FILE* out)
{
    if (!wal || !wal->path || !out)
        return BS_ATTACH_ERR_INVALID_ARG;

    FILE* f = fopen(wal->path, "rb");
    if (!f)
        return BS_ATTACH_OK;

    if (from_offset > 0)
    {
        if (fseek(f, (long)from_offset, SEEK_SET) != 0)
        {
            fclose(f);
            return BS_ATTACH_ERR_IO;
        }
    }

    uint64_t offset = from_offset;
    size_t   dumped = 0;
    while (dumped < max_records)
    {
        uint16_t       type = 0;
        uint32_t       len  = 0;
        uint32_t       crc  = 0;
        uint8_t        hdr_no_crc[4 + 2 + 2 + 4];
        const uint64_t record_start = offset;
        const int      rh =
            wal_read_record_header(f, &offset, &type, &len, &crc, hdr_no_crc, sizeof(hdr_no_crc));
        if (rh != BS_ATTACH_OK)
            break;

        uint8_t* payload = NULL;
        if (len > 0)
        {
            payload = (uint8_t*)malloc(len);
            if (!payload)
                break;
            if (fread(payload, 1, len, f) != len)
            {
                free(payload);
                break;
            }
            offset += (uint64_t)len;
        }

        uint32_t calc = 0;
        if (len == 0)
            calc = bs_adapter_attach_persist_crc32(hdr_no_crc, sizeof(hdr_no_crc));
        else
        {
            uint8_t* tmp = (uint8_t*)malloc(sizeof(hdr_no_crc) + len);
            if (!tmp)
            {
                free(payload);
                break;
            }
            memcpy(tmp, hdr_no_crc, sizeof(hdr_no_crc));
            memcpy(tmp + sizeof(hdr_no_crc), payload, len);
            calc = bs_adapter_attach_persist_crc32(tmp, sizeof(hdr_no_crc) + len);
            free(tmp);
        }

        int      has_epoch = 0;
        uint64_t epoch     = 0;
        if ((type == (uint16_t)BS_ATTACH_WAL_REC_BATCH_BEGIN && len >= 8) ||
            (type == (uint16_t)BS_ATTACH_WAL_REC_BATCH_END && len >= 8) ||
            (type == (uint16_t)BS_ATTACH_WAL_REC_COMMITTED && len == 8))
        {
            epoch     = read_u64_le(payload);
            has_epoch = 1;
        }

        if (epoch_filter == 0 || (has_epoch && epoch == epoch_filter))
        {
            fprintf(out, "offset=%llu type=%u len=%u crc_ok=%d%s%sepoch=%llu\n",
                    (unsigned long long)record_start, (unsigned)type, (unsigned)len,
                    (calc == crc) ? 1 : 0, has_epoch ? " " : "", has_epoch ? "" : "",
                    (unsigned long long)epoch);
            dumped++;
        }

        free(payload);
    }

    fclose(f);
    return BS_ATTACH_OK;
}

int bs_adapter_attach_persist_wal_count_record_type(BsAttachWal* wal, uint16_t type,
                                                    size_t* out_count)
{
    if (!wal || !wal->path || !out_count)
        return BS_ATTACH_ERR_INVALID_ARG;
    *out_count = 0;

    FILE* f = fopen(wal->path, "rb");
    if (!f)
        return BS_ATTACH_OK;

    uint64_t offset = 0;
    for (;;)
    {
        uint16_t type_read = 0;
        uint32_t len       = 0;
        uint32_t crc       = 0;
        uint8_t  hdr_no_crc[4 + 2 + 2 + 4];
        if (wal_read_record_header(f, &offset, &type_read, &len, &crc, hdr_no_crc,
                                   sizeof(hdr_no_crc)) != BS_ATTACH_OK)
            break;
        if (len > 0)
        {
            if (fseek(f, (long)len, SEEK_CUR) != 0)
                break;
            offset += (uint64_t)len;
        }
        if (type_read == type)
            ++(*out_count);
    }
    fclose(f);
    return BS_ATTACH_OK;
}
#endif
