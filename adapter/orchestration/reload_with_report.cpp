#include "bs/adapter/orchestration/reload_with_report.h"

int bs_adapter_attach_reload_batch_run_with_report(ReloadBatchController* ctrl, Report* report)
{
    if (!ctrl)
        return -1;
    bs_adapter_attach_reload_batch_set_report(ctrl, report);
    const int rc = bs_adapter_attach_reload_batch_run(ctrl);
    bs_adapter_attach_reload_batch_set_report(ctrl, nullptr);
    return rc;
}
