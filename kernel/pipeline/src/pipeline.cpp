#include "bs/kernel/ir/ir.h"
#include "bs/kernel/pipeline/Stage.h"
#include "bs/kernel/pipeline/pipeline.h"
#include "bs/kernel/report/report.h"

#include <cstdlib>
#include <cstring>

Pipeline* bs_pipeline_create(void)
{
    Pipeline* pipeline = (Pipeline*)malloc(sizeof(Pipeline));
    if (!pipeline)
        return nullptr;

    pipeline->stages      = nullptr;
    pipeline->stage_count = 0;
    return pipeline;
}

void bs_pipeline_destroy(Pipeline* pipeline)
{
    if (!pipeline)
        return;

    Stage* stage = pipeline->stages;
    while (stage)
    {
        Stage* next = stage->next;
        bs_stage_destroy(stage);
        stage = next;
    }

    free(pipeline);
}

static void pipeline_append_stage(Pipeline* pipeline, Stage* stage)
{
    if (!pipeline->stages)
    {
        pipeline->stages = stage;
    }
    else
    {
        Stage* tail = pipeline->stages;
        while (tail->next)
            tail = tail->next;
        tail->next = stage;
    }
    pipeline->stage_count++;
}

int bs_pipeline_add_stage(Pipeline* pipeline, Stage* stage)
{
    if (!pipeline || !stage || !stage->name)
        return -1;

    Stage* owned = bs_stage_create(stage->name, stage->execute);
    if (!owned)
        return -2;

    owned->cleanup = stage->cleanup;
    owned->context = stage->context;
    pipeline_append_stage(pipeline, owned);
    return 0;
}

int bs_pipeline_remove_stage(Pipeline* pipeline, const char* stage_name)
{
    if (!pipeline || !stage_name)
        return -1;

    Stage* prev = nullptr;
    Stage* curr = pipeline->stages;
    while (curr)
    {
        if (curr->name && strcmp(curr->name, stage_name) == 0)
        {
            if (prev)
                prev->next = curr->next;
            else
                pipeline->stages = curr->next;
            bs_stage_destroy(curr);
            if (pipeline->stage_count > 0)
                pipeline->stage_count--;
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }

    return -2;
}

Stage* bs_pipeline_get_stage(const Pipeline* pipeline, const char* stage_name)
{
    if (!pipeline || !stage_name)
        return nullptr;

    for (Stage* curr = pipeline->stages; curr; curr = curr->next)
    {
        if (curr->name && strcmp(curr->name, stage_name) == 0)
            return curr;
    }

    return nullptr;
}

size_t bs_pipeline_get_stage_count(const Pipeline* pipeline)
{
    return pipeline ? pipeline->stage_count : 0;
}

int bs_pipeline_execute(Pipeline* pipeline, const IRInstruction* input, Report** output)
{
    if (!pipeline || !input || !output)
        return -1;

    *output = bs_report_create("pipeline_execution");
    if (!*output)
        return -1;

    bs_report_mark_start(*output);

    const IRInstruction* current_input = input;
    IRInstruction*       stage_output  = nullptr;

    for (Stage* stage = pipeline->stages; stage; stage = stage->next)
    {
        if (!bs_stage_is_ready(stage))
        {
            bs_report_add_warn(*output, stage->name, "Stage not ready, skipping");
            bs_stage_set_state(stage, STAGE_STATE_SKIPPED);
            continue;
        }

        bs_stage_set_state(stage, STAGE_STATE_RUNNING);
        bs_report_add_info(*output, stage->name, "Executing stage");

        const int result = bs_stage_execute(stage, current_input, &stage_output);

        if (result != 0)
        {
            bs_stage_set_state(stage, STAGE_STATE_FAILED);
            bs_report_add_error(*output, stage->name, "Stage execution failed");
            bs_report_set_status(*output, REPORT_STATUS_FAILED);
            bs_report_mark_end(*output);
            return -1;
        }

        bs_stage_set_state(stage, STAGE_STATE_SUCCESS);
        bs_report_add_info(*output, stage->name, "Stage completed successfully");

        if (stage_output)
            current_input = stage_output;
    }

    bs_report_set_status(*output, REPORT_STATUS_SUCCESS);
    bs_report_mark_end(*output);
    return 0;
}

int bs_pipeline_reset(Pipeline* pipeline)
{
    if (!pipeline)
        return -1;

    for (Stage* stage = pipeline->stages; stage; stage = stage->next)
        bs_stage_set_state(stage, STAGE_STATE_IDLE);

    return 0;
}
