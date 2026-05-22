#include "bs/kernel/ir/requirements.h"

#include <cstring>

namespace
{

IRRequirementEntry kBuiltinTypes[] = {
    {"test", nullptr},  {"type1", nullptr}, {"type2", nullptr},
    {"type3", nullptr}, {"type", nullptr},
};

KernelBuiltinRequirements kBuiltin{};

void init_builtin_once()
{
    static bool done = false;
    if (done)
        return;
    done           = true;
    const size_t n = sizeof(kBuiltinTypes) / sizeof(kBuiltinTypes[0]);
    for (size_t i = 0; i + 1 < n; ++i)
    {
        kBuiltinTypes[i].next = &kBuiltinTypes[i + 1];
    }
    kBuiltinTypes[n - 1].next   = nullptr;
    kBuiltin.requirements.head  = &kBuiltinTypes[0];
    kBuiltin.requirements.count = n;
    std::strncpy(kBuiltin.kernel_version, "0.4.0", sizeof(kBuiltin.kernel_version) - 1);
    std::strncpy(kBuiltin.min_adapter_version, "0.4.0", sizeof(kBuiltin.min_adapter_version) - 1);
    std::strncpy(kBuiltin.max_adapter_version, "9.9.9", sizeof(kBuiltin.max_adapter_version) - 1);
    std::strncpy(kBuiltin.release_notes, "Day4 builtin requirement manifest (MVP).",
                 sizeof(kBuiltin.release_notes) - 1);
}

} // namespace

extern "C" const KernelBuiltinRequirements* kernel_get_builtin_requirements(void)
{
    init_builtin_once();
    return &kBuiltin;
}
