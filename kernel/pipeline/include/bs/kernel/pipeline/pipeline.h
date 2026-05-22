#ifndef BS_KERNEL_PIPELINE_PIPELINE_H
#define BS_KERNEL_PIPELINE_PIPELINE_H

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
        size_t stage_capacity;
    };

    Pipeline* pipeline_create(void);
    void      pipeline_destroy(Pipeline* pipeline);

    int    pipeline_add_stage(Pipeline* pipeline, Stage* stage);
    int    pipeline_remove_stage(Pipeline* pipeline, const char* stage_name);
    Stage* pipeline_get_stage(const Pipeline* pipeline, const char* stage_name);
    size_t pipeline_get_stage_count(const Pipeline* pipeline);

    int pipeline_execute(Pipeline* pipeline, const IRInstruction* input, Report** output);
    int pipeline_reset(Pipeline* pipeline);

#ifdef __cplusplus
}
#endif

#endif // BS_KERNEL_PIPELINE_PIPELINE_H
