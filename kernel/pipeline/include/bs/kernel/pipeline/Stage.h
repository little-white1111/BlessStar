#ifndef BS_KERNEL_PIPELINE_STAGE_H
#define BS_KERNEL_PIPELINE_STAGE_H

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
        const char*      dependencies;
        size_t           dependency_count;
    };

    Stage* stage_create(const char* name, StageExecuteFunc execute);
    void   stage_destroy(Stage* stage);

    int  stage_execute(Stage* stage, const IRInstruction* input, IRInstruction** output);
    void stage_cleanup(Stage* stage);

    void  stage_set_context(Stage* stage, void* context);
    void* stage_get_context(const Stage* stage);

    void       stage_set_state(Stage* stage, StageState state);
    StageState stage_get_state(const Stage* stage);

    void         stage_set_dependencies(Stage* stage, const char** deps, size_t count);
    const char** stage_get_dependencies(const Stage* stage, size_t* count);

    int stage_is_ready(const Stage* stage);

#ifdef __cplusplus
}
#endif

#endif // BS_KERNEL_PIPELINE_STAGE_H
