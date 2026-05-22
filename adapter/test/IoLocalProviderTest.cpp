#include "bs/adapter/io/local_file_provider.h"
#include "bs/kernel/io/io.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static std::string make_temp_file(const char* name, const unsigned char* data, size_t len)
{
    const std::string path = name;
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(len));
    return path;
}

int main()
{
    const unsigned char bom_utf8[] = {0xEF, 0xBB, 0xBF, 'h', 'i'};
    const fs::path    abs_path = fs::absolute("bs_io_local_test.txt");
    const std::string path     = make_temp_file(abs_path.string().c_str(), bom_utf8, 5);

    LocalFileProvider* provider = bs_adapter_io_local_provider_create();
    assert(provider != nullptr);
    IoProviderBinding* binding = bs_adapter_io_local_provider_binding(provider);
    assert(binding != nullptr);

    std::string uri_path = path;
    for (char& c : uri_path)
    {
        if (c == '\\')
            c = '/';
    }
    const std::string uri = "file:///" + uri_path;
    IoReadResult        result{};
    assert(binding->ops->read(binding->ctx, uri.c_str(), &result, BS_IO_MAX_READ_BYTES,
                              BS_IO_READ_TIMEOUT_MS_DEFAULT) == BS_IO_OK);
    assert(result.length == 5);
    assert(result.encoding_hint != nullptr);
    assert(std::strcmp(result.encoding_hint, "utf-8-bom") == 0);
    bs_io_read_result_free(&result);

    bs_adapter_io_local_provider_destroy(provider);
    std::remove(path.c_str());
    return 0;
}
