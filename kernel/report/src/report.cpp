#include "bs/kernel/common/bs_safe_format.h"
#include "bs/kernel/report/report.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

Report* bs_report_create(const char* workflow_name)
{
    Report* report = (Report*)malloc(sizeof(Report));
    if (!report)
        return nullptr;

    report->status        = REPORT_STATUS_SUCCESS;
    report->workflow_name = workflow_name ? strdup(workflow_name) : nullptr;
    report->start_time    = 0;
    report->end_time      = 0;
    report->entries       = nullptr;
    report->entry_count   = 0;
    report->error_message = nullptr;
    report->next_target   = nullptr;
    report->next_action   = nullptr;

    return report;
}

void bs_report_destroy(Report* report)
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

void bs_report_add_entry(Report* report, ReportLevel level, const char* stage_name,
                         const char* message)
{
    if (!report || !message)
        return;

    ReportEntry* new_entry = (ReportEntry*)malloc(sizeof(ReportEntry));
    if (!new_entry)
        return;

    new_entry->level      = level;
    new_entry->message    = message ? strdup(message) : nullptr;
    new_entry->stage_name = stage_name ? strdup(stage_name) : nullptr;
    new_entry->timestamp  = (uint64_t)time(nullptr);
    new_entry->next       = report->entries;
    report->entries       = new_entry;
    report->entry_count++;
}

void bs_report_add_debug(Report* report, const char* stage_name, const char* message)
{
    bs_report_add_entry(report, REPORT_LEVEL_DEBUG, stage_name, message);
}

void bs_report_add_info(Report* report, const char* stage_name, const char* message)
{
    bs_report_add_entry(report, REPORT_LEVEL_INFO, stage_name, message);
}

void bs_report_add_warn(Report* report, const char* stage_name, const char* message)
{
    bs_report_add_entry(report, REPORT_LEVEL_WARN, stage_name, message);
}

void bs_report_add_error(Report* report, const char* stage_name, const char* message)
{
    bs_report_add_entry(report, REPORT_LEVEL_ERROR, stage_name, message);
}

void bs_report_add_fatal(Report* report, const char* stage_name, const char* message)
{
    bs_report_add_entry(report, REPORT_LEVEL_FATAL, stage_name, message);
}

void bs_report_set_status(Report* report, ReportStatus status)
{
    if (report)
    {
        report->status = status;
    }
}

ReportStatus bs_report_get_status(const Report* report)
{
    if (!report)
        return REPORT_STATUS_FAILED;
    return report->status;
}

void bs_report_set_error_message(Report* report, const char* message)
{
    if (report)
    {
        if (report->error_message)
        {
            free((void*)report->error_message);
        }
        report->error_message = message ? strdup(message) : nullptr;
    }
}

const char* bs_report_get_error_message(const Report* report)
{
    if (!report)
        return nullptr;
    return report->error_message;
}

void bs_report_set_next_target(Report* report, const char* target)
{
    if (report)
    {
        if (report->next_target)
        {
            free((void*)report->next_target);
        }
        report->next_target = target ? strdup(target) : nullptr;
    }
}

const char* bs_report_get_next_target(const Report* report)
{
    if (!report)
        return nullptr;
    return report->next_target;
}

void bs_report_set_next_action(Report* report, const char* action)
{
    if (report)
    {
        if (report->next_action)
        {
            free((void*)report->next_action);
        }
        report->next_action = action ? strdup(action) : nullptr;
    }
}

const char* bs_report_get_next_action(const Report* report)
{
    if (!report)
        return nullptr;
    return report->next_action;
}

void bs_report_mark_start(Report* report)
{
    if (report)
    {
        report->start_time = (uint64_t)time(nullptr);
    }
}

void bs_report_mark_end(Report* report)
{
    if (report)
    {
        report->end_time = (uint64_t)time(nullptr);
    }
}

uint64_t bs_report_get_duration(const Report* report)
{
    if (!report)
        return 0;
    if (report->end_time == 0 || report->start_time >= report->end_time)
        return 0;
    return report->end_time - report->start_time;
}

char* bs_report_to_string(const Report* report)
{
    // Placeholder implementation
    if (!report)
        return nullptr;
    char* result = (char*)malloc(256);
    if (result)
    {
        strcpy(result, "Report string placeholder");
    }
    return result;
}

char* bs_report_to_json(const Report* report)
{
    if (!report)
        return nullptr;

    size_t cap = 256;
    char*  buf = (char*)malloc(cap);
    if (!buf)
        return nullptr;

    int written = bs_safe_snprintf(buf, cap, "{\"workflow\":\"%s\",\"entries\":[",
                                   report->workflow_name ? report->workflow_name : "");
    if (written < 0)
    {
        free(buf);
        return nullptr;
    }

    size_t len   = (size_t)written;
    int    first = 1;
    for (ReportEntry* e = report->entries; e; e = e->next)
    {
        const char* stage = e->stage_name ? e->stage_name : "";
        const char* msg   = e->message ? e->message : "";
        while (len + 128 > cap)
        {
            cap *= 2;
            char* grown = (char*)realloc(buf, cap);
            if (!grown)
            {
                free(buf);
                return nullptr;
            }
            buf = grown;
        }
        written = bs_safe_snprintf(buf + len, cap - len, "%s{\"stage\":\"%s\",\"message\":\"%s\"}",
                                   first ? "" : ",", stage, msg);
        first   = 0;
        if (written < 0)
        {
            free(buf);
            return nullptr;
        }
        len += (size_t)written;
    }

    while (len + 4 > cap)
    {
        cap *= 2;
        char* grown = (char*)realloc(buf, cap);
        if (!grown)
        {
            free(buf);
            return nullptr;
        }
        buf = grown;
    }
    bs_safe_snprintf(buf + len, cap - len, "]}");
    return buf;
}
