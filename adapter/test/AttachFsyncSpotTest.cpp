/**
 * ATOM-WIN-3..6: spot-check tmp write + fsync + rename (Windows/POSIX).
 * Does not depend on WAL framing.
 */

#include <cassert>
#include <cstdio>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <string>

#include "attach_fsync.h"

namespace fs = std::filesystem;

static int write_tmp_rename_atomic(const char* final_path, const void* data, size_t len)
{
    const std::string tmp = std::string(final_path) + ".bs.tmp";
    FILE*             f   = fopen(tmp.c_str(), "wb");
    if (!f)
        return -1;
    if (len > 0 && fwrite(data, 1, len, f) != len)
    {
        fclose(f);
        std::remove(tmp.c_str());
        return -1;
    }
    if (bs_adapter_attach_persist_fsync_file(f) != 0)
    {
        fclose(f);
        std::remove(tmp.c_str());
        return -1;
    }
    fclose(f);
    (void)std::remove(final_path);
    if (std::rename(tmp.c_str(), final_path) != 0)
    {
        std::remove(tmp.c_str());
        return -1;
    }
    return 0;
}

int main()
{
    const fs::path tmp = fs::temp_directory_path() / "bs_fsync_spot";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    const fs::path final_p = tmp / "data.bin";
    const char*    payload = "spot-payload";
    assert(write_tmp_rename_atomic(final_p.string().c_str(), payload, std::strlen(payload)) == 0);
    assert(fs::exists(final_p));
    assert(!fs::exists(final_p.string() + ".bs.tmp"));

    {
        std::ifstream in(final_p, std::ios::binary);
        std::string   got((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();
        assert(got == payload);
    }

    const char* payload2 = "spot-payload-v2";
    assert(write_tmp_rename_atomic(final_p.string().c_str(), payload2, std::strlen(payload2)) == 0);
    {
        std::ifstream in2(final_p, std::ios::binary);
        std::string   got2((std::istreambuf_iterator<char>(in2)), std::istreambuf_iterator<char>());
        in2.close();
        assert(got2 == payload2);
    }

    fs::remove_all(tmp);
    return 0;
}
