#ifndef BS_APP_SDK_APP_SESSION_H
#define BS_APP_SDK_APP_SESSION_H

/*
 * C-ST-7 contract block:
 * Thread safety: AppSession is not thread-safe; all access must be on the creating thread.
 * Error semantics: bool ok() indicates bootstrap success; nullptr returns on failure.
 * Platform notes: RAII wrapper around ctx_create -> bootstrap_begin -> freeze -> open_io -> open_store.
 */

#include "bs/kernel/io/io.h"
#include "bs/adapter/attach_context.h"

namespace bs::app
{

/**
 * RAII AppSession — one-line startup for real business code.
 *
 * Usage:
 *   bs::app::AppSession session("path/to/manifest");
 *   if (!session.ok()) { return; }
 *   ConfigReloadSession cs(session.ctx());
 *   ...
 *   // ~AppSession auto-calls teardown
 */
class AppSession
{
public:
    /** Bootstrap ctx, freeze registry, open IoFacade, and optionally open persist store. */
    explicit AppSession(const char* manifest_path = nullptr);

    /** Non-copyable. */
    AppSession(const AppSession&)            = delete;
    AppSession& operator=(const AppSession&) = delete;

    /** Move-allowed (transfers ownership). */
    AppSession(AppSession&& other) noexcept;
    AppSession& operator=(AppSession&& other) noexcept;

    ~AppSession();

    /** True if bootstrap succeeded. */
    bool ok() const { return ok_; }

    /** AttachContext pointer (valid while session is alive). */
    AttachContext* ctx() { return ctx_; }
    const AttachContext* ctx() const { return ctx_; }

    /** IoFacade pointer (valid while session is alive; may be null if not opened). */
    IoFacade* io() { return io_; }
    const IoFacade* io() const { return io_; }

private:
    void destroy();

    AttachContext* ctx_ = nullptr;
    IoFacade*      io_  = nullptr;
    bool           ok_  = false;
};

} // namespace bs::app

#endif // BS_APP_SDK_APP_SESSION_H
