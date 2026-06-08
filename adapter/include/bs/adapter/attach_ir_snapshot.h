#ifndef BS_ADAPTER_ATTACH_IR_SNAPSHOT_H
#define BS_ADAPTER_ATTACH_IR_SNAPSHOT_H

/*
 * C-ST-7 contract block:
 * Thread safety: store mutations require active AttachContext driver thread or external sync.
 * Error semantics: publish returns 0 handle on failure; pin/unpin are no-ops on invalid handle.
 * Platform notes: IR-SNAPSHOT-1/2/4A latest+pin; exec must not re-parse gated bytes.
 */

#include <stddef.h>
#include <stdint.h>

struct AttachContext;
struct IRInstructionList;

#ifdef __cplusplus
extern "C"
{
#endif

    /** Opaque snapshot handle (non-zero on success). */
    typedef uint64_t BsAttachIrSnapshotHandle;

    void bs_adapter_attach_ir_snapshot_init(AttachContext* ctx);
    void bs_adapter_attach_ir_snapshot_destroy(AttachContext* ctx);

    /**
     * Takes ownership of @p instructions (must not be NULL).
     * Retains latest per path; evicts unpinned older revisions (IR-SNAPSHOT-2).
     */
    BsAttachIrSnapshotHandle bs_adapter_attach_ir_snapshot_publish(AttachContext*     ctx,
                                                                   const char*        path,
                                                                   uint64_t           revision,
                                                                   IRInstructionList* instructions);

    void bs_adapter_attach_ir_snapshot_pin(AttachContext* ctx, BsAttachIrSnapshotHandle handle);
    void bs_adapter_attach_ir_snapshot_unpin(AttachContext* ctx, BsAttachIrSnapshotHandle handle);

    IRInstructionList* bs_adapter_attach_ir_snapshot_instructions(AttachContext*           ctx,
                                                                  BsAttachIrSnapshotHandle handle);

    /** Clear all unpinned snapshots (REC-A'-7 exec rollback). */
    void bs_adapter_attach_ir_snapshot_clear_all(AttachContext* ctx);

    /** Destroy one unpinned snapshot after successful exec consume (REC-G03-3b). */
    int bs_adapter_attach_ir_snapshot_remove(AttachContext* ctx, BsAttachIrSnapshotHandle handle);

    /** Entry count for recover/arch_gap tests. */
    size_t bs_adapter_attach_ir_snapshot_entry_count(AttachContext* ctx);

#ifdef __cplusplus
}
#endif

#endif /* BS_ADAPTER_ATTACH_IR_SNAPSHOT_H */
