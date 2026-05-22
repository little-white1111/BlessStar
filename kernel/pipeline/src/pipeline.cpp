#include "bs/kernel/ir/ir.h"
#include "bs/kernel/pipeline/pipeline.h"

#include <cstdlib>
#include <cstring>

struct Stage
{
    const char*      name;
    StageExecuteFunc execute;
    Stage*           next;
};

Pipeline* pipeline_create(void)
{
    Pipeline* pipeline = (Pipeline*)malloc(sizeof(Pipeline));
    if (!pipeline)
        return nullptr;

    pipeline->stages         = nullptr;
    pipeline->stage_count    = 0;
    pipeline->stage_capacity = 0;

    return pipeline;
}

void pipeline_destroy(Pipeline* pipeline)
{
    if (!pipeline)
        return;

    Stage* stage = pipeline->stages;
    while (stage)
    {
        Stage* next = stage->next;
        if (stage->name)
            free((void*)stage->name);
        free(stage);
        stage = next;
    }

    free(pipeline);
}

int pipeline_add_stage(Pipeline* pipeline, Stage* stage)
{
    if (!pipeline || !stage)
        return -1;

    Stage* new_stage = (Stage*)malloc(sizeof(Stage));
    if (!new_stage)
        return -2;

    new_stage->name    = stage->name ? strdup(stage->name) : nullptr;
    new_stage->execute = stage->execute;
    new_stage->next    = nullptr;

    if (!pipeline->stages)
    {
        pipeline->stages = new_stage;
    }
    else
    {
        Stage* curr = pipeline->stages;
        while (curr->next)
        {
            curr = curr->next;
        }
        curr->next = new_stage;
    }

    pipeline->stage_count++;
    return 0;
}

int pipeline_remove_stage(Pipeline* pipeline, const char* stage_name)
{
    if (!pipeline || !stage_name)
        return -1;

    Stage* curr = pipeline->stages;
    Stage* prev = nullptr;

    while (curr)
    {
        if (curr->name && strcmp(curr->name, stage_name) == 0)
        {
            if (prev)
            {
                prev->next = curr->next;
            }
            else
            {
                pipeline->stages = curr->next;
            }
            if (curr->name)
                free((void*)curr->name);
            free(curr);
            pipeline->stage_count--;
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }

    return -2;
}

Stage* pipeline_get_stage(const Pipeline* pipeline, const char* stage_name)
{
    if (!pipeline || !stage_name)
        return nullptr;

    Stage* curr = pipeline->stages;
    while (curr)
    {
        if (curr->name && strcmp(curr->name, stage_name) == 0)
        {
            return curr;
        }
        curr = curr->next;
    }

    return nullptr;
}

size_t pipeline_get_stage_count(const Pipeline* pipeline)
{
    if (!pipeline)
        return 0;
    return pipeline->stage_count;
}

int pipeline_execute(Pipeline* pipeline, const IRInstruction* input, Report** output)
{
    if (!pipeline || !input || !output)
        return -1;

    // Simple placeholder: pass through input
    *output = nullptr;
    return 0;
}

int pipeline_reset(Pipeline* pipeline)
{
    if (!pipeline)
        return -1;
    return 0;
}
