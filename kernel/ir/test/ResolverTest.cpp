#include "bs/kernel/ir/requirements.h"
#include "bs/kernel/ir/resolver.h"

#include <cassert>
#include <cstring>

int main()
{
    assert(bs_requirement_validate(nullptr) == 0);

    IRRequirementList empty{};
    assert(bs_requirement_validate(&empty) == 1);

    IRRequirementEntry e1{};
    e1.instruction_type = "";
    e1.next             = nullptr;
    IRRequirementList bad{};
    bad.head  = &e1;
    bad.count = 1;
    assert(bs_requirement_validate(&bad) == 0);

    assert(bs_requirement_check_compatibility("k", "1.0.0", "0.9.0", "2.0.0") == 1);
    assert(bs_requirement_check_compatibility("k", "0.1.0", "0.9.0", "2.0.0") == 0);
    assert(bs_requirement_check_compatibility("k", "9.0.0", "0.9.0", "2.0.0") == 0);

    IRRequirementEntry a1{};
    a1.instruction_type = "x";
    a1.next             = nullptr;
    IRRequirementList a{};
    a.head  = &a1;
    a.count = 1;

    IRRequirementEntry b1{};
    b1.instruction_type = "y";
    b1.next             = nullptr;
    IRRequirementList b{};
    b.head  = &b1;
    b.count = 1;

    IRRequirementList* m = bs_requirement_merge(&a, &b, 1);
    assert(m != nullptr);
    assert(m->count == 2u);
    bs_requirement_list_free(m);

    IRRequirementEntry dup{};
    dup.instruction_type = "type";
    dup.next             = nullptr;
    IRRequirementList manual{};
    manual.head                        = &dup;
    manual.count                       = 1;
    const KernelBuiltinRequirements* k = bs_kernel_get_builtin_requirements();
    assert(k != nullptr);
    IRRequirementList* merged = bs_requirement_merge(&k->requirements, &manual, 1);
    assert(merged != nullptr);
    bs_requirement_list_free(merged);

    return 0;
}
