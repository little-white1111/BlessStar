#include "bs/kernel/io/io.h"

#include "bs/adapter/io/local_file_provider.h"

#include <cassert>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <string>

#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

static std::string write_file(const fs::path& p, const void* data, size_t len)
{
    std::ofstream out(p, std::ios::binary);
    out.write(static_cast<const char*>(data), static_cast<std::streamsize>(len));
    return p.string();
}

static std::string file_uri(const std::string& path)
{
    return bs_test_path_to_file_uri(fs::path(path));
}

int main()
{
    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_io_local_boundary"));
    const fs::path           base = tmp_guard.path;

    LocalFileProvider* provider = bs_adapter_io_local_provider_create();
    IoProviderBinding* binding  = bs_adapter_io_local_provider_binding(provider);
    assert(binding && binding->ops && binding->ops->read && binding->ops->stat);

    /* not found */
    IoReadResult nf{};
    assert(binding->ops->read(binding->ctx, "file:///no/such/file.bin", &nf, 1024, 30000) ==
           BS_IO_ERR_NOT_FOUND);
    bs_io_read_result_free(&nf);

    /* utf-16-le BOM */
    const unsigned char u16[]    = {0xFF, 0xFE, 'a'};
    const std::string   u16_path = write_file(base / "u16.txt", u16, sizeof(u16));
    IoReadResult        u16r{};
    assert(binding->ops->read(binding->ctx, file_uri(u16_path).c_str(), &u16r, BS_IO_MAX_READ_BYTES,
                              BS_IO_READ_TIMEOUT_MS_DEFAULT) == BS_IO_OK);
    assert(u16r.encoding_hint && std::strcmp(u16r.encoding_hint, "utf-16-le-bom") == 0);
    bs_io_read_result_free(&u16r);

    /* truncation via max_read */
    const char        payload[]  = "123456789";
    const std::string trunc_path = write_file(base / "trunc.txt", payload, sizeof(payload) - 1);
    IoReadResult      tr{};
    assert(binding->ops->read(binding->ctx, file_uri(trunc_path).c_str(), &tr, 4, 30000) ==
           BS_IO_OK);
    assert(tr.length == 4);
    assert(tr.truncated == 1);
    bs_io_read_result_free(&tr);

    /* stat */
    int64_t size   = 0;
    int     exists = 0;
    assert(binding->ops->stat(binding->ctx, file_uri(trunc_path).c_str(), &size, &exists) ==
           BS_IO_OK);
    assert(exists == 1);
    assert(size == static_cast<int64_t>(sizeof(payload) - 1));

    assert(binding->ops->stat(binding->ctx, "file:///missing", &size, &exists) == BS_IO_OK);
    assert(exists == 0);

    bs_adapter_io_local_provider_destroy(provider);
    return 0;
}
