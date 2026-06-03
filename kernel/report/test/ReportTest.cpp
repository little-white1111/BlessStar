#include "bs/kernel/report/report.h"

#include <cassert>
#include <cstdio>
#include <cstring>

static void test_Report_CreateDestroy()
{
    Report* report = bs_report_create("test workflow");
    assert(report != nullptr);
    assert(report->status == REPORT_STATUS_SUCCESS);
    bs_report_destroy(report);
    printf("test_Report_CreateDestroy: PASS\n");
}

static void test_Report_Entries()
{
    Report* report = bs_report_create("test");

    bs_report_add_debug(report, "stage1", "debug message");
    bs_report_add_info(report, "stage1", "info message");
    bs_report_add_warn(report, "stage1", "warn message");
    bs_report_add_error(report, "stage1", "error message");
    bs_report_add_fatal(report, "stage1", "fatal message");

    assert(report->entry_count == 5);
    bs_report_destroy(report);
    printf("test_Report_Entries: PASS\n");
}

static void test_Report_Status()
{
    Report* report = bs_report_create("test");
    assert(bs_report_get_status(report) == REPORT_STATUS_SUCCESS);

    bs_report_set_status(report, REPORT_STATUS_FAILED);
    assert(bs_report_get_status(report) == REPORT_STATUS_FAILED);

    bs_report_destroy(report);
    printf("test_Report_Status: PASS\n");
}

static void test_Report_TargetAction()
{
    Report* report = bs_report_create("test");

    bs_report_set_next_target(report, "target1");
    bs_report_set_next_action(report, "action1");

    assert(strcmp(bs_report_get_next_target(report), "target1") == 0);
    assert(strcmp(bs_report_get_next_action(report), "action1") == 0);

    bs_report_destroy(report);
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
