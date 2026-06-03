#include <fstream>

#include "bs/app/sdk/vendor_reload_facade.h"

namespace bs::app
{

bool NormalizeFileToV1Bytes(VendorFormat fmt, const std::string& vendor_file_path,
                            NormalizeResult* out)
{
    return NormalizeVendorConfig(fmt, vendor_file_path, out);
}

bool NormalizeFileToTempUri(VendorFormat fmt, const std::string& vendor_file_path,
                            const std::string& temp_dir, std::string* uri_out, NormalizeResult* out)
{
    if (!uri_out)
        return false;
    uri_out->clear();

    NormalizeResult  local;
    NormalizeResult* target = out ? out : &local;
    if (!NormalizeFileToV1Bytes(fmt, vendor_file_path, target) || !target->ok)
        return false;

    const std::string file_path = temp_dir + "/bs_vendor_v1_normalized.json";
    std::ofstream     file(file_path, std::ios::binary);
    if (!file)
    {
        target->ok    = false;
        target->error = "failed to write temp v1 file";
        return false;
    }
    file.write(reinterpret_cast<const char*>(target->v1_bytes.data()),
               static_cast<std::streamsize>(target->v1_bytes.size()));
    if (!file)
    {
        target->ok    = false;
        target->error = "failed to flush temp v1 file";
        return false;
    }

    std::string uri = "file:///" + file_path;
    for (char& c : uri)
    {
        if (c == '\\')
            c = '/';
    }
    *uri_out = uri;
    return true;
}

} // namespace bs::app
