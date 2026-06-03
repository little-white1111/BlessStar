#pragma once

#include <cstddef>
#include <string>

/** Valid BlessStar Config v1 JSON with serialized size >= target_bytes. */
inline std::string bs_day19_make_valid_v1_json(size_t target_bytes)
{
    std::string json = R"({"kernel_version":"0.4.0","adapter_version":"0.4.0","manual_requirements":[],"instructions":[)";
    int         idx  = 0;
    while (json.size() < target_bytes + 64u)
    {
        if (idx > 0)
            json += ',';
        json += R"({"type":"test","name":"p)" + std::to_string(idx) + R"(","metadata":{"pad":")";
        json += std::string(128, 'a');
        json += R"("}})";
        ++idx;
        if (idx > 100000)
            break;
    }
    json += "]}";
    return json;
}
