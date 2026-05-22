#include "bs/adapter/persistence/attach_audit.h"

#include "bs/kernel/common/bs_safe_format.h"
#include "bs/kernel/report/report.h"

const char* bs_attach_scheme_label(BsAttachScheme scheme)
{
    switch (scheme)
    {
    case BS_ATTACH_SCHEME_PER_PATH:
        return "per_path";
    case BS_ATTACH_SCHEME_PER_BATCH:
        return "per_batch";
    default:
        return "unset";
    }
}

static void format_audit_msg(char* msg, size_t cap, BsAttachScheme scheme, uint64_t batch_epoch,
                             const char* uri, uint64_t revision_base, int abort_code,
                             const char* detail)
{
    bs_safe_snprintf(msg, cap,
                     "scheme=%s batch_epoch=%llu uri=%s revision_base=%llu abort_code=%d detail=%s",
             bs_attach_scheme_label(scheme), (unsigned long long)batch_epoch,
             uri ? uri : "-", (unsigned long long)revision_base, abort_code,
             detail ? detail : "");
}

void bs_attach_report_audit(Report* report, const char* stage, BsAttachScheme scheme,
                            uint64_t batch_epoch, const char* uri, uint64_t revision_base,
                            int abort_code, const char* detail)
{
    if (!report || !stage)
        return;
    char msg[640];
    format_audit_msg(msg, sizeof(msg), scheme, batch_epoch, uri, revision_base, abort_code,
                     detail);
    report_add_error(report, stage, msg);
}

void bs_attach_report_session_begin(Report* report, BsAttachScheme scheme, uint64_t batch_epoch,
                                  const char* uri, uint64_t revision_base)
{
    if (!report)
        return;
    char msg[640];
    format_audit_msg(msg, sizeof(msg), scheme, batch_epoch, uri, revision_base, 0, "session_begin");
    report_add_info(report, "cache_attach", msg);
}

void bs_attach_report_persist_ok(Report* report, BsAttachScheme scheme, uint64_t batch_epoch,
                                 const char* uri, uint64_t new_revision)
{
    if (!report)
        return;
    char msg[640];
    bs_safe_snprintf(msg, sizeof(msg),
                     "scheme=%s batch_epoch=%llu uri=%s revision=%llu abort_code=0 detail=commit_ok",
             bs_attach_scheme_label(scheme), (unsigned long long)batch_epoch, uri ? uri : "-",
             (unsigned long long)new_revision);
    report_add_info(report, "persistent_commit", msg);
}
