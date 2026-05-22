#include "bs/kernel/ir/resolver.h"

#include <cstdlib>
#include <cstring>

extern "C" int bs_requirement_validate(const IRRequirementList* list)
{
    if (!list)
        return 0;
    for (IRRequirementEntry* e = list->head; e; e = e->next)
    {
        if (!e->instruction_type || e->instruction_type[0] == '\0')
            return 0;
    }
    return 1;
}

extern "C" int bs_requirement_check_compatibility(const char* /*kernel_version*/,
                                                  const char* adapter_version,
                                                  const char* min_adapter_version,
                                                  const char* max_adapter_version)
{
    if (!adapter_version)
        return 0;
    if (min_adapter_version && min_adapter_version[0] != '\0')
    {
        if (std::strcmp(adapter_version, min_adapter_version) < 0)
            return 0;
    }
    if (max_adapter_version && max_adapter_version[0] != '\0')
    {
        if (std::strcmp(adapter_version, max_adapter_version) > 0)
            return 0;
    }
    return 1;
}

static char* dup_cstr(const char* s)
{
    if (!s)
        return nullptr;
    const size_t n = std::strlen(s) + 1;
    char*        p = static_cast<char*>(std::malloc(n));
    if (p)
        std::memcpy(p, s, n);
    return p;
}

static IRRequirementEntry* clone_entry(const char* type)
{
    IRRequirementEntry* e = (IRRequirementEntry*)std::malloc(sizeof(IRRequirementEntry));
    if (!e)
        return nullptr;
    e->instruction_type = dup_cstr(type);
    e->next             = nullptr;
    return e;
}

static void append_unique(IRRequirementList* out, const char* type, int prefer_existing)
{
    for (IRRequirementEntry* e = out->head; e; e = e->next)
    {
        if (e->instruction_type && type && std::strcmp(e->instruction_type, type) == 0)
        {
            if (!prefer_existing)
            {
                /* replace value: for MVP skip duplicate merge semantics beyond first wins */
            }
            return;
        }
    }
    IRRequirementEntry* ne = clone_entry(type);
    if (!ne)
        return;
    ne->next  = out->head;
    out->head = ne;
    out->count++;
}

extern "C" IRRequirementList* bs_requirement_merge(const IRRequirementList* a,
                                                   const IRRequirementList* b, int priority_a_wins)
{
    IRRequirementList* out = (IRRequirementList*)std::calloc(1, sizeof(IRRequirementList));
    if (!out)
        return nullptr;
    const IRRequirementList* first  = priority_a_wins ? a : b;
    const IRRequirementList* second = priority_a_wins ? b : a;
    if (first)
    {
        for (IRRequirementEntry* e = first->head; e; e = e->next)
        {
            if (e->instruction_type)
                append_unique(out, e->instruction_type, 1);
        }
    }
    if (second)
    {
        for (IRRequirementEntry* e = second->head; e; e = e->next)
        {
            if (!e->instruction_type)
                continue;
            bool exists = false;
            for (IRRequirementEntry* x = out->head; x; x = x->next)
            {
                if (x->instruction_type &&
                    std::strcmp(x->instruction_type, e->instruction_type) == 0)
                {
                    exists = true;
                    break;
                }
            }
            if (!exists)
                append_unique(out, e->instruction_type, 1);
        }
    }
    return out;
}

extern "C" void bs_requirement_list_free(IRRequirementList* list)
{
    if (!list)
        return;
    IRRequirementEntry* e = list->head;
    while (e)
    {
        IRRequirementEntry* next = e->next;
        if (e->instruction_type)
            std::free((void*)e->instruction_type);
        std::free(e);
        e = next;
    }
    std::free(list);
}
