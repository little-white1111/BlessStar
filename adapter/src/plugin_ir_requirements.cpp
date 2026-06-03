#include "bs/kernel/ir/requirements.h"

#include "bs/adapter/plugin/plugin_ir_requirements.h"

#include <cstdio>
#include <cstring>

#include <string>

namespace
{

static int builtin_has_type(const char* type)
{
    const KernelBuiltinRequirements* builtin = bs_kernel_get_builtin_requirements();
    if (!builtin || !type || type[0] == '\0')
        return 0;
    for (IRRequirementEntry* e = builtin->requirements.head; e; e = e->next)
    {
        if (e->instruction_type && std::strcmp(e->instruction_type, type) == 0)
            return 1;
    }
    return 0;
}

static void trim_inplace(std::string& s)
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

static int validate_line(const std::string& raw)
{
    std::string line = raw;
    trim_inplace(line);
    if (line.empty() || line[0] == '#')
        return 0;
    if (line.rfind("instruction_types:", 0) == 0)
        return 0;
    if (line[0] == '-')
    {
        line.erase(0, 1);
        trim_inplace(line);
    }
    if (line.empty())
        return 0;
    return builtin_has_type(line.c_str()) ? 0 : -1;
}

} // namespace

int bs_adapter_plugin_validate_ir_requirements_ref(const char* ref_path)
{
    if (!ref_path || ref_path[0] == '\0')
        return -1;

    FILE* f = std::fopen(ref_path, "r");
    if (!f)
        return -1;

    char buf[256];
    while (std::fgets(buf, sizeof(buf), f))
    {
        if (validate_line(buf) != 0)
        {
            std::fclose(f);
            return -1;
        }
    }
    std::fclose(f);
    return 0;
}
