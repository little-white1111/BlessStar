#ifndef BS_KERNEL_PIPELINE_STAGE_H
#define BS_KERNEL_PIPELINE_STAGE_H

/*
 * C-ST-7 contract block:
 * Thread safety: Stages are owned by their Pipeline list; not thread-safe.
 * Error semantics: bs_stage_execute returns -1 if execute fn missing; dependency gating via
 * is_ready. Platform notes: StageExecuteFunc transforms IRInstruction to IRInstruction*.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct IRInstruction IRInstruction;
    typedef struct Stage         Stage;

    typedef enum StageState
    {
        STAGE_STATE_IDLE,
        STAGE_STATE_RUNNING,
        STAGE_STATE_SUCCESS,
        STAGE_STATE_FAILED,
        STAGE_STATE_SKIPPED
    } StageState;

    typedef int (*StageExecuteFunc)(Stage* stage, const IRInstruction* input,
                                    IRInstruction** output);
    typedef void (*StageCleanupFunc)(Stage* stage);

    struct Stage
    {
        const char*      name;
        uint32_t         version;
        StageState       state;
        StageExecuteFunc execute;
        StageCleanupFunc cleanup;
        void*            context;
        Stage*           next;
        const char**     dependencies;
        size_t           dependency_count;
    };

    Stage* bs_stage_create(const char* name, StageExecuteFunc execute);
    void   bs_stage_destroy(Stage* stage);

    int  bs_stage_execute(Stage* stage, const IRInstruction* input, IRInstruction** output);
    void bs_stage_cleanup(Stage* stage);

    void  bs_stage_set_context(Stage* stage, void* context);
    void* bs_stage_get_context(const Stage* stage);

    void       bs_stage_set_state(Stage* stage, StageState state);
    StageState bs_stage_get_state(const Stage* stage);

    void         bs_stage_set_dependencies(Stage* stage, const char** deps, size_t count);
    const char** bs_stage_get_dependencies(const Stage* stage, size_t* count);

    int bs_stage_is_ready(const Stage* stage);

#ifdef __cplusplus
}
#endif

#endif // BS_KERNEL_PIPELINE_STAGE_H
