#include "bs/kernel/report/report.h"

#include <cassert>
#include <cstdio>
#include <cstring>

static void test_Report_CreateDestroy()
{
    Report* report = report_create("test workflow");
    assert(report != nullptr);
    assert(report->status == REPORT_STATUS_SUCCESS);
    report_destroy(report);
    printf("test_Report_CreateDestroy: PASS\n");
}

static void test_Report_Entries()
{
    Report* report = report_create("test");

    report_add_debug(report, "stage1", "debug message");
    report_add_info(report, "stage1", "info message");
    report_add_warn(report, "stage1", "warn message");
    report_add_error(report, "stage1", "error message");
    report_add_fatal(report, "stage1", "fatal message");

    assert(report->entry_count == 5);
    report_destroy(report);
    printf("test_Report_Entries: PASS\n");
}

static void test_Report_Status()
{
    Report* report = report_create("test");
    assert(report_get_status(report) == REPORT_STATUS_SUCCESS);

    report_set_status(report, REPORT_STATUS_FAILED);
    assert(report_get_status(report) == REPORT_STATUS_FAILED);

    report_destroy(report);
    printf("test_Report_Status: PASS\n");
}

static void test_Report_TargetAction()
{
    Report* report = report_create("test");

    report_set_next_target(report, "target1");
    report_set_next_action(report, "action1");

    assert(strcmp(report_get_next_target(report), "target1") == 0);
    assert(strcmp(report_get_next_action(report), "action1") == 0);

    report_destroy(report);
    printf("test_Report_TargetAction: PASS\n");
}

int main()
{
    printf("=== Report Tests ===\n");
    test_Report_CreateDestroy();
    test_Report_Entries();
    test_Report_Status();
    test_Report_TargetAction();
    printf("=== All Report Tests PASSED! ===\n");
    return 0;
}
