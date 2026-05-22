#include "bs/adapter/orchestration/reload_with_report.h"

int bs_reload_batch_run_with_report(ReloadBatchController* ctrl, Report* report)
{
    if (!ctrl)
        return -1;
    bs_reload_batch_controller_set_report(ctrl, report);
    const int rc = bs_reload_batch_run(ctrl);
    bs_reload_batch_controller_set_report(ctrl, nullptr);
    return rc;
}
