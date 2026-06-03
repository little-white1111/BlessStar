#include "bs/kernel/pipeline/Stage.h"

#include <stdlib.h>
#include <string.h>

Stage* bs_stage_create(const char* name, StageExecuteFunc execute)
{
    if (!name)
        return NULL;

    Stage* stage = (Stage*)malloc(sizeof(Stage));
    if (!stage)
        return NULL;

    stage->name             = strdup(name);
    stage->version          = 1;
    stage->state            = STAGE_STATE_IDLE;
    stage->execute          = execute;
    stage->cleanup          = NULL;
    stage->context          = NULL;
    stage->next             = NULL;
    stage->dependencies     = NULL;
    stage->dependency_count = 0;

    return stage;
}

void bs_stage_destroy(Stage* stage)
{
    if (!stage)
        return;

    if (stage->name)
        free((void*)stage->name);
    if (stage->dependencies)
    {
        for (size_t i = 0; i < stage->dependency_count; i++)
        {
            free((void*)stage->dependencies[i]);
        }
        free(stage->dependencies);
    }

    if (stage->cleanup && stage->context)
    {
        stage->cleanup(stage);
    }

    free(stage);
}

int bs_stage_execute(Stage* stage, const IRInstruction* input, IRInstruction** output)
{
    if (!stage || !stage->execute)
        return -1;

    return stage->execute(stage, input, output);
}

void bs_stage_cleanup(Stage* stage)
{
    if (!stage || !stage->cleanup)
        return;
    stage->cleanup(stage);
}

void bs_stage_set_context(Stage* stage, void* context)
{
    if (!stage)
        return;
    stage->context = context;
}

void* bs_stage_get_context(const Stage* stage)
{
    return stage ? stage->context : NULL;
}

void bs_stage_set_state(Stage* stage, StageState state)
{
    if (!stage)
        return;
    stage->state = state;
}

StageState bs_stage_get_state(const Stage* stage)
{
    return stage ? stage->state : STAGE_STATE_IDLE;
}

void bs_stage_set_dependencies(Stage* stage, const char** deps, size_t count)
{
    if (!stage)
        return;

    // Free existing dependencies
    if (stage->dependencies)
    {
        for (size_t i = 0; i < stage->dependency_count; i++)
        {
            free((void*)stage->dependencies[i]);
        }
        free(stage->dependencies);
    }

    if (count == 0)
    {
        stage->dependencies     = NULL;
        stage->dependency_count = 0;
        return;
    }

    stage->dependencies = (const char**)malloc(sizeof(char*) * count);
    if (!stage->dependencies)
        return;

    for (size_t i = 0; i < count; i++)
    {
        stage->dependencies[i] = strdup(deps[i]);
    }

    stage->dependency_count = count;
}

const char** bs_stage_get_dependencies(const Stage* stage, size_t* count)
{
    if (!stage || !count)
        return NULL;
    *count = stage->dependency_count;
    return stage->dependencies;
}

int bs_stage_is_ready(const Stage* stage)
{
    if (!stage)
        return 0;

    // If no dependencies, always ready (when idle)
    if (stage->dependency_count == 0)
    {
        return stage->state == STAGE_STATE_IDLE;
    }

    return 0;
}
