#include "bs/app/sdk/app_session.h"

#include "bs/adapter/registry_bootstrap.h"

namespace bs::app
{

AppSession::AppSession(const char* manifest_path)
{
    ctx_ = bs_adapter_attach_ctx_create();
    if (!ctx_)
        return;

    if (bs_adapter_registry_bootstrap_begin_ctx(ctx_) != 0)
    {
        destroy();
        return;
    }

    if (bs_adapter_registry_bootstrap_freeze_ctx(ctx_) != 0)
    {
        destroy();
        return;
    }

    RegistryFacade* facade = bs_adapter_attach_ctx_registry(ctx_);
    if (facade)
    {
        io_ = bs_io_facade_create(facade);
    }

    if (manifest_path && manifest_path[0] != '\0')
    {
        bs_adapter_attach_ctx_open_persist_store(ctx_, manifest_path);
    }

    ok_ = true;
}

AppSession::AppSession(AppSession&& other) noexcept
    : ctx_(other.ctx_)
    , io_(other.io_)
    , ok_(other.ok_)
{
    other.ctx_ = nullptr;
    other.io_  = nullptr;
    other.ok_  = false;
}

AppSession& AppSession::operator=(AppSession&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        ctx_        = other.ctx_;
        io_         = other.io_;
        ok_         = other.ok_;
        other.ctx_  = nullptr;
        other.io_   = nullptr;
        other.ok_   = false;
    }
    return *this;
}

AppSession::~AppSession()
{
    destroy();
}

void AppSession::destroy()
{
    if (io_)
    {
        bs_io_facade_destroy(io_);
        io_ = nullptr;
    }
    if (ctx_)
    {
        bs_adapter_attach_ctx_destroy(ctx_);
        ctx_ = nullptr;
    }
    ok_ = false;
}

} // namespace bs::app
