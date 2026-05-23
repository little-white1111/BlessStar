#include "bs/kernel/io/io.h"
#include "bs/kernel/registry/registry_facade.h"

#include "bs/adapter/registry_bootstrap.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

int main()
{
    const std::string path = fs::absolute("bs_io_registry_phase.txt").string();
    {
        std::ofstream out(path, std::ios::binary);
        out << "freeze-read";
    }

    RegistryFacade* reg = bs_registry_facade_create();
    assert(reg != nullptr);
    assert(bs_adapter_registry_bootstrap_begin(reg) == 0);
    assert(bs_registry_facade_advance_phase(reg, BS_REGISTRY_PHASE_P2) == BS_REGISTRY_OK);
    assert(bs_adapter_registry_bootstrap_freeze(reg) == 0);

    PathEntry late{};
    late.source          = BS_PATH_ENTRY_PLUGIN;
    late.manifest_ref    = "late";
    late.type_constraint = nullptr;
    assert(bs_registry_facade_register_declaration(reg, "/adapter/io/late", &late) ==
           BS_REGISTRY_ERR_FROZEN);
    assert(bs_registry_facade_bind_instance(reg, "/adapter/io/late", &late) ==
           BS_REGISTRY_ERR_FROZEN);

    IoFacade* io = bs_io_facade_create(reg);
    assert(io != nullptr);

    std::string uri_path = path;
    for (char& c : uri_path)
    {
        if (c == '\\')
            c = '/';
    }
    const std::string uri = "file:///" + uri_path;
    IoReadResult      result{};
    assert(bs_io_facade_read(io, uri.c_str(), &result) == BS_IO_OK);
    assert(result.length == 11);
    assert(result.data != nullptr);
    assert(std::memcmp(result.data, "freeze-read", 11) == 0);
    bs_io_read_result_free(&result);

    bs_io_facade_destroy(io);
    bs_registry_facade_destroy(reg);
    std::remove(path.c_str());
    return 0;
}
