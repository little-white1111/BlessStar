#include "bs/kernel/report/report.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include <vector>

static void test_Report_NullInput()
{
    bs_report_add_debug(nullptr, nullptr, nullptr);
    bs_report_add_info(nullptr, nullptr, nullptr);
    bs_report_add_warn(nullptr, nullptr, nullptr);
    bs_report_add_error(nullptr, nullptr, nullptr);
    bs_report_add_fatal(nullptr, nullptr, nullptr);
    printf("test_Report_NullInput: PASS\n");
}

static void test_Report_EmptyWorkflow()
{
    Report* report = bs_report_create("");
    assert(report != nullptr);
    bs_report_destroy(report);
    printf("test_Report_EmptyWorkflow: PASS\n");
}

static void test_Report_LargeEntryCount()
{
    Report*   report = bs_report_create("test");
    const int N      = 10000;

    for (int i = 0; i < N; i++)
    {
        bs_report_add_info(report, "stage", "message");
    }

    assert(report->entry_count == N);
    bs_report_destroy(report);
    printf("test_Report_LargeEntryCount: PASS\n");
}

static void test_Report_LongMessage()
{
    // Avoid ~1MiB on stack (default Windows thread stack is ~1MiB and overflows).
    std::vector<char> long_msg(1024 * 1024);
    memset(long_msg.data(), 'x', long_msg.size() - 1);
    long_msg[long_msg.size() - 1] = '\0';

    Report* report = bs_report_create("test");
    bs_report_add_info(report, "stage", long_msg.data());
    bs_report_destroy(report);
    printf("test_Report_LongMessage: PASS\n");
}

static void test_Report_AllStatusTransitions()
{
    Report* report = bs_report_create("test");

    bs_report_set_status(report, REPORT_STATUS_SUCCESS);
    assert(report->status == REPORT_STATUS_SUCCESS);

    bs_report_set_status(report, REPORT_STATUS_PARTIAL);
    assert(report->status == REPORT_STATUS_PARTIAL);

    bs_report_set_status(report, REPORT_STATUS_TIMEOUT);
    assert(report->status == REPORT_STATUS_TIMEOUT);

    bs_report_set_status(report, REPORT_STATUS_FAILED);
    assert(report->status == REPORT_STATUS_FAILED);

    bs_report_destroy(report);
    printf("test_Report_AllStatusTransitions: PASS\n");
}

static void test_Report_EmptyNextTarget()
{
    Report* report = bs_report_create("test");
    bs_report_set_next_target(report, "");
    bs_report_set_next_action(report, "");
    bs_report_destroy(report);
    printf("test_Report_EmptyNextTarget: PASS\n");
}

int main()
{
    printf("=== Report Boundary Tests ===\n");
    test_Report_NullInput();
    test_Report_EmptyWorkflow();
    test_Report_LargeEntryCount();
    test_Report_LongMessage();
    test_Report_AllStatusTransitions();
    test_Report_EmptyNextTarget();
    printf("=== All Report Boundary Tests PASSED! ===\n");
    return 0;
}
