#ifndef BS_ADAPTER_PERSISTENCE_ATTACH_AUDIT_H
#define BS_ADAPTER_PERSISTENCE_ATTACH_AUDIT_H

/*
 * C-ST-7 contract block:
 * Thread safety: Audit records written from batch thread only.
 * Error semantics: Drops silently if report/store unset; see implementation.
 * Platform notes: Maps reload stages to Report entries for operators.
 */

#include "bs/adapter/persistence/attach_store.h"

struct Report;

#ifdef __cplusplus
extern "C"
{
#endif

    const char* bs_adapter_attach_persist_scheme_label(BsAttachScheme scheme);

    void bs_adapter_attach_persist_report_audit(struct Report* report, const char* stage,
                                                BsAttachScheme scheme, uint64_t batch_epoch,
                                                const char* uri, uint64_t revision_base,
                                                int abort_code, const char* detail);

    void bs_adapter_attach_persist_report_session_begin(struct Report* report,
                                                        BsAttachScheme scheme, uint64_t batch_epoch,
                                                        const char* uri, uint64_t revision_base);

    void bs_adapter_attach_persist_report_persist_ok(struct Report* report, BsAttachScheme scheme,
                                                     uint64_t batch_epoch, const char* uri,
                                                     uint64_t new_revision);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_PERSISTENCE_ATTACH_AUDIT_H */
