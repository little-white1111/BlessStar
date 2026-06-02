#include "bs/kernel/io/io.h"

#include "bs/adapter/io/local_file_provider.h"

#include <cassert>

#include <filesystem>
#include <fstream>
#include <string>

#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

int main()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_io_local_timeout"));
    const fs::path           cfg_file = tmp_guard.path / "timeout.txt";
    {
        std::ofstream out(cfg_file, std::ios::binary);
        out << "data";
    }

    LocalFileProvider* provider = bs_adapter_io_local_provider_create();
    IoProviderBinding* binding  = bs_adapter_io_local_provider_binding(provider);
    assert(binding != nullptr);

    const std::string uri = bs_test_path_to_file_uri(cfg_file);

    IoReadResult result{};
    assert(binding->ops->read(binding->ctx, uri.c_str(), &result, 1024, 0) == BS_IO_ERR_TIMEOUT);
    assert(result.error_message != nullptr);
    bs_io_read_result_free(&result);

    IoReadResult ok{};
    assert(binding->ops->read(binding->ctx, uri.c_str(), &ok, 1024,
                              BS_IO_READ_TIMEOUT_MS_DEFAULT) == BS_IO_OK);
    bs_io_read_result_free(&ok);

    bs_adapter_io_local_provider_destroy(provider);
    return 0;
}
