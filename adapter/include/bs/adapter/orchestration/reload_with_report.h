#ifndef BS_ADAPTER_ORCHESTRATION_RELOAD_WITH_REPORT_H
#define BS_ADAPTER_ORCHESTRATION_RELOAD_WITH_REPORT_H

#include "bs/kernel/report/report.h"

#include "bs/adapter/orchestration/reload_batch_controller.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Run reload batch; failed paths add Report entries (stage io_read / ir_gate).
     * Does not write state. Caller owns @p report.
     */
    int bs_reload_batch_run_with_report(ReloadBatchController* ctrl, Report* report);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_ORCHESTRATION_RELOAD_WITH_REPORT_H */
