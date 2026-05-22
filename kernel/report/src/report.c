#include "bs/kernel/report/report.h"
#include "bs/kernel/common/bs_safe_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

Report* report_create(const char* workflow_name)
{
    Report* report = (Report*)malloc(sizeof(Report));
    if (!report)
        return NULL;

    report->status        = REPORT_STATUS_SUCCESS;
    report->workflow_name = workflow_name ? strdup(workflow_name) : strdup("unknown");
    report->start_time    = 0;
    report->end_time      = 0;
    report->entries       = NULL;
    report->entry_count   = 0;
    report->error_message = NULL;
    report->next_target   = NULL;
    report->next_action   = NULL;

    return report;
}

void report_destroy(Report* report)
{
    if (!report)
        return;

    if (report->workflow_name)
        free((void*)report->workflow_name);
    if (report->error_message)
        free((void*)report->error_message);
    if (report->next_target)
        free((void*)report->next_target);
    if (report->next_action)
        free((void*)report->next_action);

    ReportEntry* entry = report->entries;
    while (entry)
    {
        ReportEntry* next = entry->next;
        if (entry->message)
            free((void*)entry->message);
        if (entry->stage_name)
            free((void*)entry->stage_name);
        free(entry);
        entry = next;
    }

    free(report);
}

static void report_add_entry_internal(Report* report, ReportLevel level, const char* stage_name,
                                      const char* message)
{
    if (!report || !message)
        return;

    ReportEntry* entry = (ReportEntry*)malloc(sizeof(ReportEntry));
    if (!entry)
        return;

    entry->level      = level;
    entry->message    = strdup(message);
    entry->stage_name = stage_name ? strdup(stage_name) : strdup("global");
    entry->timestamp  = (uint64_t)time(NULL);
    entry->next       = NULL;

    if (!report->entries)
    {
        report->entries = entry;
    }
    else
    {
        ReportEntry* last = report->entries;
        while (last->next)
        {
            last = last->next;
        }
        last->next = entry;
    }

    report->entry_count++;
}

void report_add_entry(Report* report, ReportLevel level, const char* stage_name,
                      const char* message)
{
    report_add_entry_internal(report, level, stage_name, message);
}

void report_add_debug(Report* report, const char* stage_name, const char* message)
{
    report_add_entry_internal(report, REPORT_LEVEL_DEBUG, stage_name, message);
}

void report_add_info(Report* report, const char* stage_name, const char* message)
{
    report_add_entry_internal(report, REPORT_LEVEL_INFO, stage_name, message);
}

void report_add_warn(Report* report, const char* stage_name, const char* message)
{
    report_add_entry_internal(report, REPORT_LEVEL_WARN, stage_name, message);
}

void report_add_error(Report* report, const char* stage_name, const char* message)
{
    report_add_entry_internal(report, REPORT_LEVEL_ERROR, stage_name, message);
}

void report_add_fatal(Report* report, const char* stage_name, const char* message)
{
    report_add_entry_internal(report, REPORT_LEVEL_FATAL, stage_name, message);
}

void report_set_status(Report* report, ReportStatus status)
{
    if (!report)
        return;
    report->status = status;
}

ReportStatus report_get_status(const Report* report)
{
    return report ? report->status : REPORT_STATUS_FAILED;
}

void report_set_error_message(Report* report, const char* message)
{
    if (!report)
        return;
    if (report->error_message)
        free((void*)report->error_message);
    report->error_message = message ? strdup(message) : NULL;
}

const char* report_get_error_message(const Report* report)
{
    return report ? report->error_message : NULL;
}

void report_set_next_target(Report* report, const char* target)
{
    if (!report)
        return;
    if (report->next_target)
        free((void*)report->next_target);
    report->next_target = target ? strdup(target) : NULL;
}

const char* report_get_next_target(const Report* report)
{
    return report ? report->next_target : NULL;
}

void report_set_next_action(Report* report, const char* action)
{
    if (!report)
        return;
    if (report->next_action)
        free((void*)report->next_action);
    report->next_action = action ? strdup(action) : NULL;
}

const char* report_get_next_action(const Report* report)
{
    return report ? report->next_action : NULL;
}

void report_mark_start(Report* report)
{
    if (!report)
        return;
    report->start_time = (uint64_t)time(NULL);
}

void report_mark_end(Report* report)
{
    if (!report)
        return;
    report->end_time = (uint64_t)time(NULL);
}

uint64_t report_get_duration(const Report* report)
{
    if (!report)
        return 0;
    if (report->end_time == 0)
        return (uint64_t)time(NULL) - report->start_time;
    return report->end_time - report->start_time;
}

char* report_to_string(const Report* report)
{
    if (!report)
        return NULL;

    const char* status_str;
    switch (report->status)
    {
    case REPORT_STATUS_SUCCESS:
        status_str = "SUCCESS";
        break;
    case REPORT_STATUS_PARTIAL:
        status_str = "PARTIAL";
        break;
    case REPORT_STATUS_FAILED:
        status_str = "FAILED";
        break;
    case REPORT_STATUS_TIMEOUT:
        status_str = "TIMEOUT";
        break;
    default:
        status_str = "UNKNOWN";
    }

    size_t size   = 1024 + (report->entry_count * 512);
    char*  result = (char*)malloc(size);
    if (!result)
        return NULL;

    bs_safe_snprintf(result, size, "Report: %s\nStatus: %s\nDuration: %llu ms\n", report->workflow_name,
             status_str, (unsigned long long)report_get_duration(report));

    if (report->entries)
    {
        ReportEntry* entry = report->entries;
        while (entry)
        {
            const char* level_str;
            switch (entry->level)
            {
            case REPORT_LEVEL_DEBUG:
                level_str = "[DEBUG]";
                break;
            case REPORT_LEVEL_INFO:
                level_str = "[INFO]";
                break;
            case REPORT_LEVEL_WARN:
                level_str = "[WARN]";
                break;
            case REPORT_LEVEL_ERROR:
                level_str = "[ERROR]";
                break;
            case REPORT_LEVEL_FATAL:
                level_str = "[FATAL]";
                break;
            default:
                level_str = "[UNKNOWN]";
            }

            strncat(result, level_str, size - strlen(result) - 1);
            strncat(result, " [", size - strlen(result) - 1);
            strncat(result, entry->stage_name, size - strlen(result) - 1);
            strncat(result, "] ", size - strlen(result) - 1);
            strncat(result, entry->message, size - strlen(result) - 1);
            strncat(result, "\n", size - strlen(result) - 1);

            entry = entry->next;
        }
    }

    return result;
}

char* report_to_json(const Report* report)
{
    if (!report)
        return NULL;

    const char* status_str;
    switch (report->status)
    {
    case REPORT_STATUS_SUCCESS:
        status_str = "success";
        break;
    case REPORT_STATUS_PARTIAL:
        status_str = "partial";
        break;
    case REPORT_STATUS_FAILED:
        status_str = "failed";
        break;
    case REPORT_STATUS_TIMEOUT:
        status_str = "timeout";
        break;
    default:
        status_str = "unknown";
    }

    size_t size   = 2048 + (report->entry_count * 1024);
    char*  result = (char*)malloc(size);
    if (!result)
        return NULL;

    bs_safe_snprintf(result, size,
             "{\"workflow_name\":\"%s\",\"status\":\"%s\",\"duration_ms\":%llu,\"entries\":[",
             report->workflow_name, status_str, (unsigned long long)report_get_duration(report));

    if (report->entries)
    {
        ReportEntry* entry = report->entries;
        int          first = 1;

        while (entry)
        {
            if (!first)
                strncat(result, ",", size - strlen(result) - 1);
            first = 0;

            const char* level_str;
            switch (entry->level)
            {
            case REPORT_LEVEL_DEBUG:
                level_str = "debug";
                break;
            case REPORT_LEVEL_INFO:
                level_str = "info";
                break;
            case REPORT_LEVEL_WARN:
                level_str = "warn";
                break;
            case REPORT_LEVEL_ERROR:
                level_str = "error";
                break;
            case REPORT_LEVEL_FATAL:
                level_str = "fatal";
                break;
            default:
                level_str = "unknown";
            }

            char entry_json[1024];
            bs_safe_snprintf(
                entry_json, sizeof(entry_json),
                "{\"level\":\"%s\",\"stage_name\":\"%s\",\"message\":\"%s\",\"timestamp\":%llu}",
                level_str, entry->stage_name, entry->message, (unsigned long long)entry->timestamp);

            strncat(result, entry_json, size - strlen(result) - 1);
            entry = entry->next;
        }
    }

    strncat(result, "]", size - strlen(result) - 1);

    if (report->error_message)
    {
        char error_json[512];
        bs_safe_snprintf(error_json, sizeof(error_json), ",\"error_message\":\"%s\"",
                 report->error_message);
        strncat(result, error_json, size - strlen(result) - 1);
    }

    if (report->next_target)
    {
        char target_json[512];
        bs_safe_snprintf(target_json, sizeof(target_json), ",\"next_target\":\"%s\"", report->next_target);
        strncat(result, target_json, size - strlen(result) - 1);
    }

    if (report->next_action)
    {
        char action_json[512];
        bs_safe_snprintf(action_json, sizeof(action_json), ",\"next_action\":\"%s\"", report->next_action);
        strncat(result, action_json, size - strlen(result) - 1);
    }

    strncat(result, "}", size - strlen(result) - 1);

    return result;
}
