#include "bs/db/mgmt/db_config_loader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace bs::db::mgmt {

static std::string trim(const std::string& s)
{
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string extract_value(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return "";
    pos = json.find_first_not_of(" \t\r\n", pos + 1);
    if (pos == std::string::npos) return "";
    if (json[pos] == '"') {
        auto end = json.find('"', pos + 1);
        if (end == std::string::npos) return "";
        return json.substr(pos + 1, end - pos - 1);
    }
    auto end = json.find_first_of(",}\r\n", pos);
    if (end == std::string::npos) return "";
    return trim(json.substr(pos, end - pos));
}

static DbDriverType parse_driver(const std::string& s)
{
    std::string lower;
    lower.resize(s.size());
    std::transform(s.begin(), s.end(), lower.begin(), ::tolower);
    if (lower == "sqlite") return DbDriverType::SQLite;
    if (lower == "mysql") return DbDriverType::MySQL;
    if (lower == "postgresql") return DbDriverType::PostgreSQL;
    return DbDriverType::Null;
}

DbMgrConfig DbConfigLoader::FromFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) return {};
    std::stringstream ss;
    ss << file.rdbuf();
    return FromJson(ss.str());
}

DbMgrConfig DbConfigLoader::FromJson(const std::string& json)
{
    DbMgrConfig cfg;
    std::string driver_str = extract_value(json, "driver");
    if (driver_str.empty()) {
        auto pos = json.find("\"db\"");
        if (pos != std::string::npos) {
            auto brace = json.find('{', pos);
            auto end = json.find('}', brace);
            if (brace != std::string::npos && end != std::string::npos) {
                std::string section = json.substr(brace, end - brace + 1);
                driver_str = extract_value(section, "driver");
                if (driver_str.empty()) driver_str = extract_value(section, "type");
                cfg.db_cfg.dsn = extract_value(section, "dsn");
                cfg.db_cfg.user = extract_value(section, "user");
                cfg.db_cfg.password = extract_value(section, "password");
                cfg.db_cfg.database = extract_value(section, "database");
                std::string ps = extract_value(section, "pool_size");
                if (!ps.empty()) cfg.pool_size = std::stoi(ps);
                std::string to = extract_value(section, "timeout_ms");
                if (!to.empty()) cfg.db_cfg.timeout_ms = std::stoi(to);
            }
        }
    }
    cfg.db_cfg.driver_type = parse_driver(driver_str);
    std::string hci = extract_value(json, "interval_ms");
    if (hci.empty()) {
        auto pos = json.find("\"health_check\"");
        if (pos != std::string::npos) {
            auto brace = json.find('{', pos);
            auto end = json.find('}', brace);
            if (brace != std::string::npos && end != std::string::npos) {
                std::string hc = json.substr(brace, end - brace + 1);
                hci = extract_value(hc, "interval_ms");
                std::string ar = extract_value(hc, "auto_reconnect");
                if (!ar.empty()) cfg.auto_reconnect = (ar == "true" || ar == "1");
                std::string mr = extract_value(hc, "max_retries");
                if (!mr.empty()) cfg.max_retries = std::stoi(mr);
            }
        }
    }
    if (!hci.empty()) cfg.health_check_interval_ms = std::stoi(hci);
    cfg.plugin_dir = extract_value(json, "plugin_dir");
    auto wpos = json.find("\"watcher\"");
    if (wpos != std::string::npos) {
        auto brace = json.find('{', wpos);
        auto end = json.find('}', brace);
        if (brace != std::string::npos && end != std::string::npos) {
            std::string ws = json.substr(brace, end - brace + 1);
            cfg.watcher_config_path = extract_value(ws, "config_path");
        }
    }
    return cfg;
}

} // namespace bs::db::mgmt
