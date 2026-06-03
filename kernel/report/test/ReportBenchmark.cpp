#include "bs/kernel/report/report.h"

#include <cstdio>
#include <ctime>

// Measure report creation/destroy N times
static void benchmark_ReportCreateDestroy(size_t N)
{
    clock_t start = clock();

    for (size_t i = 0; i < N; i++)
    {
        Report* report = bs_report_create("test");
        bs_report_destroy(report);
    }

    clock_t end = clock();
    double  ms  = 1000.0 * (end - start) / CLOCKS_PER_SEC;

    printf("benchmark_ReportCreateDestroy (N=%zu): %.3f ms\n", N, ms);
}

// Measure adding N log entries
static void benchmark_ReportAddEntries(size_t N)
{
    Report* report = bs_report_create("test");

    clock_t start = clock();

    for (size_t i = 0; i < N; i++)
    {
        bs_report_add_info(report, "stage", "log entry");
    }

    clock_t end = clock();
    double  ms  = 1000.0 * (end - start) / CLOCKS_PER_SEC;

    bs_report_destroy(report);

    printf("benchmark_ReportAddEntries (N=%zu): %.3f ms\n", N, ms);
}

// Measure mark_start/mark_end N times
static void benchmark_ReportTiming(size_t N)
{
    Report* report = bs_report_create("test");

    clock_t start = clock();

    for (size_t i = 0; i < N; i++)
    {
        bs_report_mark_start(report);
        bs_report_mark_end(report);
    }

    clock_t end = clock();
    double  ms  = 1000.0 * (end - start) / CLOCKS_PER_SEC;

    bs_report_destroy(report);

    printf("benchmark_ReportTiming (N=%zu): %.3f ms\n", N, ms);
}

int main()
{
    printf("=== Report Performance Benchmarks ===\n");

    benchmark_ReportCreateDestroy(10000);
    benchmark_ReportCreateDestroy(100000);

    benchmark_ReportAddEntries(1000);
    benchmark_ReportAddEntries(10000);

    benchmark_ReportTiming(10000);
    benchmark_ReportTiming(100000);

    printf("=== Report Benchmark Complete ===\n");
    return 0;
}
