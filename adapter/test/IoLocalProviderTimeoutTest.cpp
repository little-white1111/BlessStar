#include "bs/adapter/io/local_file_provider.h"
#include "bs/kernel/io/io.h"

#include <cassert>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

int main()
{
    const fs::path path = fs::absolute("bs_io_timeout_test.txt");
    {
        std::ofstream out(path, std::ios::binary);
        out << "data";
    }

    LocalFileProvider* provider = bs_adapter_io_local_provider_create();
    IoProviderBinding* binding  = bs_adapter_io_local_provider_binding(provider);
    assert(binding != nullptr);

    std::string uri_path = path.string();
    for (char& c : uri_path)
    {
        if (c == '\\')
            c = '/';
    }
    const std::string uri = "file:///" + uri_path;

    IoReadResult result{};
    assert(binding->ops->read(binding->ctx, uri.c_str(), &result, 1024, 0) == BS_IO_ERR_TIMEOUT);
    assert(result.error_message != nullptr);
    bs_io_read_result_free(&result);

    IoReadResult ok{};
    assert(binding->ops->read(binding->ctx, uri.c_str(), &ok, 1024,
                              BS_IO_READ_TIMEOUT_MS_DEFAULT) == BS_IO_OK);
    bs_io_read_result_free(&ok);

    bs_adapter_io_local_provider_destroy(provider);
    fs::remove(path);
    return 0;
}
