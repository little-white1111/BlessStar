#ifndef BS_KERNEL_PIPELINE_PIPELINE_H
#define BS_KERNEL_PIPELINE_PIPELINE_H

/*
 * C-ST-7 contract block:
 * Thread safety: Not thread-safe; external synchronization required.
 * Error semantics: Returns 0 on success; negative codes on stage failure; see report output.
 * Platform notes: Linked list of stages; uses bs_kernel_report_core for execution trace.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct IRInstruction IRInstruction;
    typedef struct Report        Report;
    typedef struct Stage         Stage;
    typedef struct Pipeline      Pipeline;

    typedef int (*StageExecuteFunc)(Stage* stage, const IRInstruction* input,
                                    IRInstruction** output);

    struct Pipeline
    {
        Stage* stages;
        size_t stage_count;
    };

    Pipeline* bs_pipeline_create(void);
    void      bs_pipeline_destroy(Pipeline* pipeline);

    int    bs_pipeline_add_stage(Pipeline* pipeline, Stage* stage);
    int    bs_pipeline_remove_stage(Pipeline* pipeline, const char* stage_name);
    Stage* bs_pipeline_get_stage(const Pipeline* pipeline, const char* stage_name);
    size_t bs_pipeline_get_stage_count(const Pipeline* pipeline);

    int bs_pipeline_execute(Pipeline* pipeline, const IRInstruction* input, Report** output);
    int bs_pipeline_reset(Pipeline* pipeline);

#ifdef __cplusplus
}
#endif

#endif // BS_KERNEL_PIPELINE_PIPELINE_H
