#include "bs/app/sdk/mem_audit_log.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace bs::app
{

MemAuditLog::~MemAuditLog()
{
    // No explicit cleanup needed; files persist on disk.
}

bool MemAuditLog::Init(const char* audit_dir)
{
    if (!audit_dir || audit_dir[0] == '\0')
    {
        last_error_ = "null or empty audit directory";
        return false;
    }

    audit_dir_ = audit_dir;

    // Create directory if it does not exist
    std::error_code ec;
    if (!fs::create_directories(audit_dir_, ec) && ec)
    {
        last_error_ = std::string("failed to create audit directory: ") + ec.message();
        return false;
    }

    // Load existing manifest if present
    LoadManifest();
    initialized_ = true;
    return true;
}

bool MemAuditLog::LoadManifest()
{
    const fs::path manifest_path = fs::path(audit_dir_) / "manifest.json";
    std::ifstream ifs(manifest_path);
    if (!ifs.is_open())
    {
        // No existing manifest; start fresh
        return true;
    }

    // Simple line-based JSON parsing (no JSON library dependency).
    // Expected format:
    // { "version": 1, "max_snapshots_per_key": 5, "keys": { ... } }
    std::stringstream buf;
    buf << ifs.rdbuf();
    std::string content = buf.str();
    if (content.empty())
        return true;

    // Find "keys": { section
    auto keys_pos = content.find("\"keys\"");
    if (keys_pos == std::string::npos)
        return true;

    auto obj_start = content.find('{', keys_pos);
    if (obj_start == std::string::npos)
        return true;

    // Parse key entries of form: "keyname": { "current_seq": N, ... }
    auto obj_end = content.rfind('}');
    if (obj_end == std::string::npos || obj_end <= obj_start)
        return true;

    std::string keys_body = content.substr(obj_start + 1, obj_end - obj_start - 1);

    // Find each quoted key
    size_t pos = 0;
    while ((pos = keys_body.find('\"', pos)) != std::string::npos)
    {
        size_t key_start = pos + 1;
        size_t key_end   = keys_body.find('\"', key_start);
        if (key_end == std::string::npos)
            break;
        std::string key = keys_body.substr(key_start, key_end - key_start);

        // Find "{ after ":"
        size_t val_start = keys_body.find('{', key_end);
        if (val_start == std::string::npos)
            break;

        size_t val_end = keys_body.find('}', val_start);
        if (val_end == std::string::npos)
            break;

        std::string val_json = keys_body.substr(val_start + 1, val_end - val_start - 1);

        // Parse current_seq
        unsigned current_seq = 0;
        auto seq_pos = val_json.find("current_seq");
        if (seq_pos != std::string::npos)
        {
            auto colon = val_json.find(':', seq_pos);
            if (colon != std::string::npos)
                current_seq = static_cast<unsigned>(std::strtoul(val_json.c_str() + colon + 1, nullptr, 10));
        }

        // Parse snapshot_count
        unsigned snap_count = 0;
        auto sc_pos = val_json.find("snapshot_count");
        if (sc_pos != std::string::npos)
        {
            auto colon = val_json.find(':', sc_pos);
            if (colon != std::string::npos)
                snap_count = static_cast<unsigned>(std::strtoul(val_json.c_str() + colon + 1, nullptr, 10));
        }

        KeyEntry entry;
        entry.key        = key;
        entry.state.current_seq    = current_seq;
        entry.state.snapshot_count = snap_count;
        keys_.push_back(std::move(entry));

        pos = val_end + 1;
    }

    return true;
}

bool MemAuditLog::SaveManifest()
{
    if (audit_dir_.empty())
        return false;

    const fs::path manifest_path = fs::path(audit_dir_) / "manifest.json";
    std::ofstream ofs(manifest_path);
    if (!ofs.is_open())
    {
        last_error_ = "failed to write manifest.json";
        return false;
    }

    ofs << "{\n";
    ofs << "  \"version\": 1,\n";
    ofs << "  \"max_snapshots_per_key\": " << kMaxSnapshotsPerKey << ",\n";
    ofs << "  \"keys\": {\n";

    for (size_t i = 0; i < keys_.size(); ++i)
    {
        // Get last submit time string
        std::time_t now = std::time(nullptr);
        char time_str[64] = {};
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", std::gmtime(&now));

        ofs << "    \"" << keys_[i].key << "\": {\n";
        ofs << "      \"current_seq\": " << keys_[i].state.current_seq << ",\n";
        ofs << "      \"last_submit_time\": \"" << time_str << "\",\n";
        ofs << "      \"snapshot_count\": " << keys_[i].state.snapshot_count << "\n";
        ofs << "    }";
        if (i + 1 < keys_.size())
            ofs << ",";
        ofs << "\n";
    }

    ofs << "  }\n";
    ofs << "}\n";
    ofs.close();

    if (ofs.fail())
    {
        last_error_ = "error writing manifest.json";
        return false;
    }
    return true;
}

bool MemAuditLog::Record(const char* key, const void* data, size_t len)
{
    if (!initialized_ || !key || !data || len == 0)
    {
        last_error_ = "not initialized or invalid args";
        return false;
    }

    // Find or create key entry
    KeyEntry* entry = nullptr;
    for (auto& k : keys_)
    {
        if (k.key == key)
        {
            entry = &k;
            break;
        }
    }
    if (!entry)
    {
        KeyEntry ne;
        ne.key = key;
        keys_.push_back(std::move(ne));
        entry = &keys_.back();
    }

    // Bump sequence
    entry->state.current_seq++;
    entry->state.snapshot_count++;

    // Write snapshot file
    {
        char filename[1024];
        std::snprintf(filename, sizeof(filename), "%s.v%u.bin", key, entry->state.current_seq);
        fs::path snap_path = fs::path(audit_dir_) / filename;
        std::FILE* f = std::fopen(snap_path.string().c_str(), "wb");
        if (!f)
        {
            // Rollback count
            entry->state.snapshot_count--;
            entry->state.current_seq--;
            last_error_ = std::string("failed to write snapshot: ") + filename;
            return false;
        }
        std::fwrite(data, 1, len, f);
        std::fclose(f);
    }

    // Evict oldest if over limit
    if (entry->state.snapshot_count > kMaxSnapshotsPerKey)
    {
        unsigned oldest_seq = entry->state.current_seq - entry->state.snapshot_count + 1;
        RemoveSnapshot(key, oldest_seq);
        entry->state.snapshot_count--;
    }

    // Save updated manifest
    if (!SaveManifest())
        return false;

    return true;
}

bool MemAuditLog::RemoveSnapshot(const char* key, unsigned seq)
{
    char filename[1024];
    std::snprintf(filename, sizeof(filename), "%s.v%u.bin", key, seq);
    fs::path snap_path = fs::path(audit_dir_) / filename;
    std::error_code ec;
    fs::remove(snap_path, ec);
    return !ec;
}

} // namespace bs::app
