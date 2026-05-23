#ifndef BS_TEST_CONFIG_V1_BOUNDARY_BUILD_H
#define BS_TEST_CONFIG_V1_BOUNDARY_BUILD_H

#include <cstddef>

#include <string>

/** Build BlessStar Config JSON v1 bytes for day10 boundary tests (model C and variants). */

inline void bs_test_append_json_escaped_string(std::string& out, const std::string& raw)
{
    out.push_back('"');
    for (char c : raw)
    {
        if (c == '"' || c == '\\')
            out.push_back('\\');
        out.push_back(c);
    }
    out.push_back('"');
}

inline void bs_test_build_config_v1_model_c(std::string& out, size_t instruction_count,
                                            size_t metadata_keys_per_instr)
{
    out.clear();
    out.reserve(instruction_count * (80 + metadata_keys_per_instr * 24));
    out +=
        R"({"kernel_version":"0.4.0","adapter_version":"0.4.0","manual_requirements":[],"instructions":[)";
    for (size_t i = 0; i < instruction_count; ++i)
    {
        if (i > 0)
            out += ',';
        out += R"({"type":"test","name":")";
        out += "inst-";
        out += std::to_string(i);
        out += R"(","metadata":{)";
        for (size_t k = 0; k < metadata_keys_per_instr; ++k)
        {
            if (k > 0)
                out += ',';
            out += "\"k";
            out += std::to_string(k);
            out += "\":\"v";
            out += std::to_string(k);
            out += '"';
        }
        if (i == 0)
        {
            if (metadata_keys_per_instr > 0)
                out += ',';
            out += R"("subject_code":"1001.01","tax_rate":"13","amount":"100.50")";
        }
        out += "}}";
    }
    out += "]}";
}

/** Model A style: many instructions, minimal metadata (2 keys). */
inline void bs_test_build_config_v1_model_a_light(std::string& out, size_t instruction_count)
{
    bs_test_build_config_v1_model_c(out, instruction_count, 2);
}

inline std::string bs_test_build_single_instruction_metadata_value(size_t value_len)
{
    std::string out;
    out +=
        R"({"kernel_version":"0.4.0","adapter_version":"0.4.0","manual_requirements":[],"instructions":[{"type":"test","name":"long-val","metadata":{)";
    out += "\"payload\":";
    bs_test_append_json_escaped_string(out, std::string(value_len, 'a'));
    out += R"(}}]})";
    return out;
}

inline std::string bs_test_build_truncated_json()
{
    return R"({"kernel_version":"0.4.0","adapter_version":"0.4.0","instructions":[)";
}

#endif /* BS_TEST_CONFIG_V1_BOUNDARY_BUILD_H */
