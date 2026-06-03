/**
 * Attach + IO integration (day 6): registry bootstrap -> IO providers -> freeze -> IoFacade.read.
 * Out of scope: full config collect/translate, state hot-reload, pipeline/report wiring.
 */

#include "bs/kernel/io/io.h"
#include "bs/kernel/ir/requirements.h"
#include "bs/kernel/registry/registry_facade.h"

#include "bs/adapter/attach_context.h"
#include "bs/adapter/registry_bootstrap.h"
#include "bs/adapter/requirement_filter.h"

#include <cassert>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <string>

#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

int main()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_io_attach_pipeline"));
    const fs::path           cfg_file = tmp_guard.path / "cfg.txt";
    {
        std::ofstream out(cfg_file, std::ios::binary);
        out << "attach-io-ok";
    }

    assert(bs_kernel_get_builtin_requirements() != nullptr);
    assert(bs_adapter_requirement_filter_validate_builtin() == 0);

    AttachContext* ctx = bs_adapter_attach_ctx_create();
    assert(ctx != nullptr);
    bs_adapter_attach_ctx_set_active(ctx);
    RegistryFacade* facade = bs_adapter_attach_ctx_registry(ctx);
    assert(facade != nullptr);

    assert(bs_adapter_registry_bootstrap_begin_ctx(ctx) == 0);
    assert(bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P2) == BS_REGISTRY_OK);
    assert(bs_adapter_registry_bootstrap_freeze_ctx(ctx) == 0);

    PathEntry late{};
    late.source          = BS_PATH_ENTRY_PLUGIN;
    late.manifest_ref    = "late";
    late.type_constraint = nullptr;
    assert(bs_registry_facade_register_declaration(facade, "/adapter/io/late", &late) ==
           BS_REGISTRY_ERR_FROZEN);

    Binding local{};
    assert(bs_registry_facade_resolve(facade, "/adapter/io/local", &local) == BS_REGISTRY_OK);

    IoFacade* io = bs_io_facade_create(facade);
    assert(io != nullptr);

    const std::string uri = bs_test_path_to_file_uri(cfg_file);

    IoReadResult result{};
    assert(bs_io_facade_read(io, uri.c_str(), &result) == BS_IO_OK);
    assert(result.length == 12);
    assert(std::memcmp(result.data, "attach-io-ok", 12) == 0);
    bs_io_read_result_free(&result);

    bs_io_facade_destroy(io);
    bs_adapter_registry_shutdown_log();
    bs_adapter_attach_ctx_destroy(ctx);
    return 0;
}
