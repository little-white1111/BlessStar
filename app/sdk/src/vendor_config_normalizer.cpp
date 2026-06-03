#include <fstream>
#include <sstream>

#include "bs/app/sdk/vendor_config_normalizer.h"

namespace bs::app
{
namespace
{

static void fail(NormalizeResult* out, const char* msg)
{
    if (!out)
        return;
    out->ok = false;
    out->v1_bytes.clear();
    out->error = msg;
}

static bool read_file(const std::string& path, std::string* text)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    *text = ss.str();
    return true;
}

static bool extract_balanced_object(const std::string& text, std::size_t start,
                                    std::string* object_out)
{
    if (start >= text.size() || text[start] != '{')
        return false;
    int depth = 0;
    for (std::size_t i = start; i < text.size(); ++i)
    {
        const char c = text[i];
        if (c == '{')
            ++depth;
        else if (c == '}')
        {
            --depth;
            if (depth == 0)
            {
                *object_out = text.substr(start, i - start + 1);
                return true;
            }
        }
    }
    return false;
}

static bool looks_like_v1_document(const std::string& json)
{
    return json.find("\"kernel_version\"") != std::string::npos &&
           json.find("\"instructions\"") != std::string::npos;
}

static bool extract_string_field(const std::string& json, const char* key, std::string* value_out)
{
    const std::string needle = std::string("\"") + key + "\"";
    const std::size_t pos    = json.find(needle);
    if (pos == std::string::npos)
        return false;
    std::size_t colon = json.find(':', pos + needle.size());
    if (colon == std::string::npos)
        return false;
    std::size_t q1 = json.find('"', colon + 1);
    if (q1 == std::string::npos)
        return false;
    std::size_t q2 = json.find('"', q1 + 1);
    if (q2 == std::string::npos)
        return false;
    *value_out = json.substr(q1 + 1, q2 - q1 - 1);
    return true;
}

static bool normalize_generic_business_json(const std::string& text, NormalizeResult* out)
{
    if (text.find("\"config\"") != std::string::npos)
    {
        const std::size_t cfg_key = text.find("\"config\"");
        std::size_t       brace   = text.find('{', cfg_key);
        if (brace == std::string::npos)
        {
            fail(out, "generic business json: missing config object");
            return false;
        }
        std::string v1_json;
        if (!extract_balanced_object(text, brace, &v1_json))
        {
            fail(out, "generic business json: unbalanced config object");
            return false;
        }
        if (!looks_like_v1_document(v1_json))
        {
            fail(out, "generic business json: config is not v1-shaped");
            return false;
        }
        out->v1_bytes.assign(v1_json.begin(), v1_json.end());
        extract_string_field(text, "source_vendor", &out->source_vendor);
        extract_string_field(text, "scenario", &out->scenario);
        if (out->source_vendor.empty())
            out->source_vendor = "generic_business";
        return true;
    }

    if (looks_like_v1_document(text))
    {
        out->v1_bytes.assign(text.begin(), text.end());
        out->source_vendor = "passthrough_v1";
        extract_string_field(text, "scenario", &out->scenario);
        return true;
    }

    fail(out, "generic business json: expected config wrapper or v1 document");
    return false;
}

} // namespace

bool NormalizeVendorConfig(VendorFormat fmt, const std::string& vendor_file_path,
                           NormalizeResult* out)
{
    if (!out)
        return false;
    *out = NormalizeResult{};

    std::string text;
    if (!read_file(vendor_file_path, &text) || text.empty())
    {
        fail(out, "failed to read vendor file");
        return false;
    }

    bool ok = false;
    switch (fmt)
    {
    case VendorFormat::GenericBusinessJson:
        ok = normalize_generic_business_json(text, out);
        break;
    default:
        fail(out, "unsupported vendor format");
        return false;
    }

    if (!ok)
        return false;

    out->ok = !out->v1_bytes.empty();
    if (!out->ok)
        out->error = "empty v1 output";
    return out->ok;
}

} // namespace bs::app
