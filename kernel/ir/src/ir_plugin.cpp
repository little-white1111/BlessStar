#include "bs/kernel/ir/ir_plugin.h"

#include <utility>
#include <vector>

namespace
{

std::vector<std::pair<BsIrInstructionVisitor, void*>> g_visitors;

} // namespace

extern "C" void bs_ir_plugin_register_visitor(BsIrInstructionVisitor visitor, void* user_data)
{
    if (!visitor)
        return;
    g_visitors.emplace_back(visitor, user_data);
}

extern "C" void bs_ir_plugin_unregister_visitor(BsIrInstructionVisitor visitor)
{
    for (auto it = g_visitors.begin(); it != g_visitors.end(); ++it)
    {
        if (it->first == visitor)
        {
            g_visitors.erase(it);
            return;
        }
    }
}

extern "C" int bs_ir_plugin_apply_visitors(IRInstructionList* list)
{
    if (!list)
        return 0;
    const size_t n = bs_ir_instruction_list_size(list);
    for (size_t i = 0; i < n; ++i)
    {
        IRInstruction* instr = bs_ir_instruction_list_get(list, i);
        if (!instr)
            return -1;
        for (const auto& slot : g_visitors)
        {
            if (slot.first)
            {
                const int rc = slot.first(instr, slot.second);
                if (rc != 0)
                    return rc;
            }
        }
    }
    return 0;
}
