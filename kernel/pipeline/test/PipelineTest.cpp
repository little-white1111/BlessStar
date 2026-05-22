#include "bs/kernel/ir/ir.h"
#include "bs/kernel/pipeline/pipeline.h"

#include <cassert>
#include <cstdio>

static void test_Pipeline_CreateDestroy()
{
    Pipeline* pipeline = pipeline_create();
    assert(pipeline != nullptr);
    pipeline_destroy(pipeline);
    printf("test_Pipeline_CreateDestroy: PASS\n");
}

static void test_Pipeline_Stages()
{
    Pipeline* pipeline = pipeline_create();

    assert(pipeline_get_stage_count(pipeline) == 0);

    // Stage is an opaque type in header, we skip detailed test
    pipeline_destroy(pipeline);
    printf("test_Pipeline_Stages: PASS (skipped)\n");
}

int main()
{
    printf("=== Pipeline Tests ===\n");
    test_Pipeline_CreateDestroy();
    test_Pipeline_Stages();
    printf("=== All Pipeline Tests PASSED! ===\n");
    return 0;
}
