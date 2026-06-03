#ifndef BS_KERNEL_REPORT_REPORT_H
#define BS_KERNEL_REPORT_REPORT_H

/*
 * C-ST-7 contract block:
 * Thread safety: Not thread-safe; one Report per workflow/batch on a single thread.
 * Error semantics: void helpers no-op on NULL report; execute paths set REPORT_STATUS_*.
 * Platform notes: Batch reload audit trail; JSON via bs_report_to_json.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum ReportLevel
    {
        REPORT_LEVEL_DEBUG,
        REPORT_LEVEL_INFO,
        REPORT_LEVEL_WARN,
        REPORT_LEVEL_ERROR,
        REPORT_LEVEL_FATAL
    } ReportLevel;

    typedef enum ReportStatus
    {
        REPORT_STATUS_SUCCESS,
        REPORT_STATUS_PARTIAL,
        REPORT_STATUS_FAILED,
        REPORT_STATUS_TIMEOUT
    } ReportStatus;

    typedef struct ReportEntry ReportEntry;
    typedef struct Report      Report;

    struct ReportEntry
    {
        ReportLevel  level;
        const char*  message;
        const char*  stage_name;
        uint64_t     timestamp;
        ReportEntry* next;
    };

    struct Report
    {
        ReportStatus status;
        const char*  workflow_name;
        uint64_t     start_time;
        uint64_t     end_time;
        ReportEntry* entries;
        size_t       entry_count;
        const char*  error_message;
        const char*  next_target;
        const char*  next_action;
    };

    Report* bs_report_create(const char* workflow_name);
    void    bs_report_destroy(Report* report);

    void bs_report_add_entry(Report* report, ReportLevel level, const char* stage_name,
                             const char* message);
    void bs_report_add_debug(Report* report, const char* stage_name, const char* message);
    void bs_report_add_info(Report* report, const char* stage_name, const char* message);
    void bs_report_add_warn(Report* report, const char* stage_name, const char* message);
    void bs_report_add_error(Report* report, const char* stage_name, const char* message);
    void bs_report_add_fatal(Report* report, const char* stage_name, const char* message);

    void         bs_report_set_status(Report* report, ReportStatus status);
    ReportStatus bs_report_get_status(const Report* report);

    void        bs_report_set_error_message(Report* report, const char* message);
    const char* bs_report_get_error_message(const Report* report);

    void        bs_report_set_next_target(Report* report, const char* target);
    const char* bs_report_get_next_target(const Report* report);

    void        bs_report_set_next_action(Report* report, const char* action);
    const char* bs_report_get_next_action(const Report* report);

    void     bs_report_mark_start(Report* report);
    void     bs_report_mark_end(Report* report);
    uint64_t bs_report_get_duration(const Report* report);

    char* bs_report_to_string(const Report* report);
    char* bs_report_to_json(const Report* report);

#ifdef __cplusplus
}
#endif

#endif // BS_KERNEL_REPORT_REPORT_H
