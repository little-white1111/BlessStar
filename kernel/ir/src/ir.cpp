#include "bs/kernel/ir/ir.h"

#include <cstdlib>
#include <cstring>

#include <new>
#include <vector>

struct IRInstructionListImpl
{
    std::vector<IRInstruction*> items;
};

IRInstruction* ir_instruction_create(const char* type, const char* name)
{
    IRInstruction* instr = (IRInstruction*)malloc(sizeof(IRInstruction));
    if (!instr)
        return nullptr;

    instr->type      = type ? strdup(type) : nullptr;
    instr->name      = name ? strdup(name) : nullptr;
    instr->metadata  = nullptr;
    instr->version   = 0;
    instr->timestamp = 0;

    return instr;
}

void ir_instruction_destroy(IRInstruction* instr)
{
    if (!instr)
        return;

    if (instr->type)
        free((void*)instr->type);
    if (instr->name)
        free((void*)instr->name);

    IRMetadata* meta = instr->metadata;
    while (meta)
    {
        IRMetadata* next = meta->next;
        if (meta->key)
            free((void*)meta->key);
        if (meta->value)
            free((void*)meta->value);
        free(meta);
        meta = next;
    }

    free(instr);
}

IRMetadata* ir_metadata_create(const char* key, const char* value)
{
    IRMetadata* meta = (IRMetadata*)malloc(sizeof(IRMetadata));
    if (!meta)
        return nullptr;

    meta->key   = key ? strdup(key) : nullptr;
    meta->value = value ? strdup(value) : nullptr;
    meta->next  = nullptr;

    return meta;
}

void ir_metadata_destroy(IRMetadata* meta)
{
    if (!meta)
        return;

    if (meta->key)
        free((void*)meta->key);
    if (meta->value)
        free((void*)meta->value);
    free(meta);
}

void ir_instruction_add_metadata(IRInstruction* instr, IRMetadata* meta)
{
    if (!instr || !meta)
        return;

    meta->next      = instr->metadata;
    instr->metadata = meta;
}

const char* ir_instruction_get_metadata(const IRInstruction* instr, const char* key)
{
    if (!instr || !key)
        return nullptr;

    IRMetadata* meta = instr->metadata;
    while (meta)
    {
        if (meta->key && strcmp(meta->key, key) == 0)
        {
            return meta->value;
        }
        meta = meta->next;
    }
    return nullptr;
}

IRInstructionList* ir_instruction_list_create(void)
{
    try
    {
        return reinterpret_cast<IRInstructionList*>(new IRInstructionListImpl());
    }
    catch (...)
    {
        return nullptr;
    }
}

void ir_instruction_list_destroy(IRInstructionList* list)
{
    if (!list)
        return;

    auto* impl = reinterpret_cast<IRInstructionListImpl*>(list);
    for (IRInstruction* instr : impl->items)
    {
        ir_instruction_destroy(instr);
    }
    delete impl;
}

int ir_instruction_list_add(IRInstructionList* list, IRInstruction* instr)
{
    if (!list || !instr)
        return -1;

    auto* impl = reinterpret_cast<IRInstructionListImpl*>(list);
    try
    {
        impl->items.push_back(instr);
        return 0;
    }
    catch (...)
    {
        return -2;
    }
}

size_t ir_instruction_list_size(const IRInstructionList* list)
{
    if (!list)
        return 0;

    const auto* impl = reinterpret_cast<const IRInstructionListImpl*>(list);
    return impl->items.size();
}

IRInstruction* ir_instruction_list_get(const IRInstructionList* list, size_t index)
{
    if (!list)
        return nullptr;

    const auto* impl = reinterpret_cast<const IRInstructionListImpl*>(list);
    if (index >= impl->items.size())
        return nullptr;

    return impl->items[index];
}
