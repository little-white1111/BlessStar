#include "bs/kernel/ir/requirements.h"

#include <cassert>
#include <cstring>

int main()
{
    const KernelBuiltinRequirements* k = bs_kernel_get_builtin_requirements();
    assert(k != nullptr);
    assert(k->requirements.head != nullptr);
    assert(k->requirements.count >= 1u);

    bool saw_test = false;
    for (IRRequirementEntry* e = k->requirements.head; e; e = e->next)
    {
        assert(e->instruction_type != nullptr);
        assert(e->instruction_type[0] != '\0');
        if (std::strcmp(e->instruction_type, "test") == 0)
            saw_test = true;
    }
    assert(saw_test);
    assert(std::strlen(k->kernel_version) > 0);
    return 0;
}
