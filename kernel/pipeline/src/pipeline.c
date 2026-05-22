#include "bs/kernel/pipeline/Stage.h"
#include "bs/kernel/pipeline/pipeline.h"
#include "bs/kernel/report/report.h"

#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 8

Pipeline* pipeline_create(void)
{
    Pipeline* pipeline = (Pipeline*)malloc(sizeof(Pipeline));
    if (!pipeline)
        return NULL;

    pipeline->stages = (Stage**)malloc(sizeof(Stage*) * INITIAL_CAPACITY);
    if (!pipeline->stages)
    {
        free(pipeline);
        return NULL;
    }

    pipeline->stage_count    = 0;
    pipeline->stage_capacity = INITIAL_CAPACITY;

    return pipeline;
}

void pipeline_destroy(Pipeline* pipeline)
{
    if (!pipeline)
        return;

    for (size_t i = 0; i < pipeline->stage_count; i++)
    {
        stage_destroy(pipeline->stages[i]);
    }

    if (pipeline->stages)
    {
        free(pipeline->stages);
    }

    free(pipeline);
}

static int pipeline_resize(Pipeline* pipeline, size_t new_capacity)
{
    Stage** new_stages = (Stage**)realloc(pipeline->stages, sizeof(Stage*) * new_capacity);
    if (!new_stages)
        return -1;

    pipeline->stages         = new_stages;
    pipeline->stage_capacity = new_capacity;
    return 0;
}

int pipeline_add_stage(Pipeline* pipeline, Stage* stage)
{
    if (!pipeline || !stage)
        return -1;

    if (pipeline->stage_count >= pipeline->stage_capacity)
    {
        if (pipeline_resize(pipeline, pipeline->stage_capacity * 2) != 0)
        {
            return -1;
        }
    }

    pipeline->stages[pipeline->stage_count++] = stage;
    return 0;
}

int pipeline_remove_stage(Pipeline* pipeline, const char* stage_name)
{
    if (!pipeline || !stage_name)
        return -1;

    for (size_t i = 0; i < pipeline->stage_count; i++)
    {
        if (strcmp(pipeline->stages[i]->name, stage_name) == 0)
        {
            stage_destroy(pipeline->stages[i]);

            for (size_t j = i; j < pipeline->stage_count - 1; j++)
            {
                pipeline->stages[j] = pipeline->stages[j + 1];
            }

            pipeline->stage_count--;
            return 0;
        }
    }

    return -1;
}

Stage* pipeline_get_stage(const Pipeline* pipeline, const char* stage_name)
{
    if (!pipeline || !stage_name)
        return NULL;

    for (size_t i = 0; i < pipeline->stage_count; i++)
    {
        if (strcmp(pipeline->stages[i]->name, stage_name) == 0)
        {
            return pipeline->stages[i];
        }
    }

    return NULL;
}

size_t pipeline_get_stage_count(const Pipeline* pipeline)
{
    return pipeline ? pipeline->stage_count : 0;
}

int pipeline_execute(Pipeline* pipeline, const IRInstruction* input, Report** output)
{
    if (!pipeline || !input || !output)
        return -1;

    *output = report_create("pipeline_execution");
    if (!*output)
        return -1;

    report_mark_start(*output);

    const IRInstruction* current_input = input;
    IRInstruction*       stage_output  = NULL;

    for (size_t i = 0; i < pipeline->stage_count; i++)
    {
        Stage* stage = pipeline->stages[i];

        if (!stage_is_ready(stage))
        {
            report_add_warn(*output, stage->name, "Stage not ready, skipping");
            stage_set_state(stage, STAGE_STATE_SKIPPED);
            continue;
        }

        stage_set_state(stage, STAGE_STATE_RUNNING);
        report_add_info(*output, stage->name, "Executing stage");

        int result = stage_execute(stage, current_input, &stage_output);

        if (result != 0)
        {
            stage_set_state(stage, STAGE_STATE_FAILED);
            report_add_error(*output, stage->name, "Stage execution failed");
            report_set_status(*output, REPORT_STATUS_FAILED);
            report_mark_end(*output);
            return -1;
        }

        stage_set_state(stage, STAGE_STATE_SUCCESS);
        report_add_info(*output, stage->name, "Stage completed successfully");

        if (stage_output)
        {
            if (current_input != input)
            {
                // Free intermediate output
            }
            current_input = stage_output;
        }
    }

    report_set_status(*output, REPORT_STATUS_SUCCESS);
    report_mark_end(*output);

    return 0;
}

int pipeline_reset(Pipeline* pipeline)
{
    if (!pipeline)
        return -1;

    for (size_t i = 0; i < pipeline->stage_count; i++)
    {
        stage_set_state(pipeline->stages[i], STAGE_STATE_IDLE);
    }

    return 0;
}
