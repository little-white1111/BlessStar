#include "bs/adapter/plugin/attach_manifest_yaml.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{

static void trim(std::string& s)
{
    while (!s.empty() &&
           (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
        ++i;
    if (i > 0)
        s.erase(0, i);
}

static const char* dup_cstr(const std::string& s, std::vector<std::string>& storage)
{
    storage.push_back(s);
    return storage.back().c_str();
}

struct ParseState
{
    AttachManifestPluginConfig* out_configs     = nullptr;
    int                         max_configs     = 0;
    int                         count           = 0;
    int                         cur             = -1;
    int                         in_depends_list = 0;
    std::vector<std::string>    strings;
};

static AttachManifestPluginConfig* current_plugin(ParseState* st)
{
    if (!st || st->cur < 0 || st->cur >= st->count)
        return nullptr;
    return &st->out_configs[st->cur];
}

static int start_plugin(ParseState* st, const std::string& id)
{
    if (!st || id.empty())
        return -1;
    if (st->count >= st->max_configs)
        return -1;
    for (int i = 0; i < st->count; ++i)
    {
        if (st->out_configs[i].manifest_id &&
            std::strcmp(st->out_configs[i].manifest_id, id.c_str()) == 0)
        {
            st->cur             = i;
            st->in_depends_list = 0;
            return 0;
        }
    }
    st->cur             = st->count;
    st->in_depends_list = 0;
    auto& cfg           = st->out_configs[st->count];
    cfg.manifest_id     = dup_cstr(id, st->strings);
    cfg.enabled         = 1;
    cfg.depends_count   = 0;
    ++st->count;
    return 0;
}

static int parse_line(ParseState* st, const std::string& raw)
{
    std::string line = raw;
    trim(line);
    if (line.empty() || line[0] == '#')
        return 0;

    const char* k_manifest = "manifest_id:";
    const auto  midx       = line.find(k_manifest);
    if (midx != std::string::npos)
    {
        std::string id = line.substr(midx + std::strlen(k_manifest));
        trim(id);
        if (!id.empty() && id[0] == ':')
        {
            id.erase(0, 1);
            trim(id);
        }
        if (id.empty())
            return -1;
        return start_plugin(st, id);
    }

    auto* cfg = current_plugin(st);
    if (!cfg)
        return 0;

    if (line.rfind("enabled:", 0) == 0)
    {
        st->in_depends_list = 0;
        std::string v       = line.substr(std::strlen("enabled:"));
        trim(v);
        if (v == "true")
            cfg->enabled = 1;
        else if (v == "false")
            cfg->enabled = 0;
        else
            return -1;
        return 0;
    }

    if (line.rfind("depends_on:", 0) == 0)
    {
        st->in_depends_list = 1;
        return 0;
    }

    if (st->in_depends_list && line.rfind("- ", 0) == 0)
    {
        std::string dep = line.substr(2);
        trim(dep);
        if (dep.empty() || cfg->depends_count >= 8)
            return -1;
        cfg->depends_on[cfg->depends_count++] = dup_cstr(dep, st->strings);
        return 0;
    }

    if (line.rfind("- ", 0) == 0)
        st->in_depends_list = 0;

    return 0;
}

} // namespace

int bs_adapter_attach_manifest_yaml_load(const char* path, AttachManifestPluginConfig* out_configs,
                                         int max_configs)
{
    if (!path || !out_configs || max_configs <= 0)
        return -1;

    FILE* f = std::fopen(path, "r");
    if (!f)
        return -1;

    ParseState st{};
    st.out_configs = out_configs;
    st.max_configs = max_configs;

    char buf[512];
    while (std::fgets(buf, sizeof(buf), f))
    {
        if (parse_line(&st, buf) != 0)
        {
            std::fclose(f);
            return -1;
        }
    }
    std::fclose(f);
    return st.count;
}
