#include "bs/kernel/io/io.h"

#include "bs/adapter/io/local_file_provider.h"

#include <cassert>
#include <cstring>

#include <filesystem>
#include <string>

#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

int main()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_io_local_provider"));
    const unsigned char      bom_utf8[] = {0xEF, 0xBB, 0xBF, 'h', 'i'};
    const fs::path             cfg_file   = tmp_guard.path / "local_test.txt";
    assert(bs_test_write_binary_file(cfg_file, bom_utf8, 5));

    LocalFileProvider* provider = bs_adapter_io_local_provider_create();
    assert(provider != nullptr);
    IoProviderBinding* binding = bs_adapter_io_local_provider_binding(provider);
    assert(binding != nullptr);

    const std::string uri = bs_test_path_to_file_uri(cfg_file);
    IoReadResult      result{};
    assert(binding->ops->read(binding->ctx, uri.c_str(), &result, BS_IO_MAX_READ_BYTES,
                              BS_IO_READ_TIMEOUT_MS_DEFAULT) == BS_IO_OK);
    assert(result.length == 5);
    assert(result.encoding_hint != nullptr);
    assert(std::strcmp(result.encoding_hint, "utf-8-bom") == 0);
    bs_io_read_result_free(&result);

    bs_adapter_io_local_provider_destroy(provider);
    return 0;
}
