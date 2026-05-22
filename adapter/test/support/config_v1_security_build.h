#ifndef BS_TEST_CONFIG_V1_SECURITY_BUILD_H
#define BS_TEST_CONFIG_V1_SECURITY_BUILD_H

#include <string>

/** Day11 security fixtures (no day10 1MiB / 4095 capacity matrix). */

inline std::string bs_test_build_security_minimal_ok()
{
    return R"({"kernel_version":"0.4.0","adapter_version":"0.4.0","manual_requirements":[],"instructions":[{"type":"test","name":"n","metadata":{"note":"ok"}}]})";
}

/** Append unknown root key "bomb" with nested objects (skipped by parser, counts toward depth). */
inline std::string bs_test_build_config_with_depth_bomb(size_t nested_object_depth)
{
    std::string out = bs_test_build_security_minimal_ok();
    if (out.empty() || out.back() != '}')
        return out;
    out.pop_back();
    out += R"(,"bomb":)";
    for (size_t i = 0; i < nested_object_depth; ++i)
        out += R"({"a":)";
    out += "null";
    for (size_t i = 0; i < nested_object_depth; ++i)
        out += '}';
    out += '}';
    return out;
}

inline std::string bs_test_build_invalid_utf8_surrogate_escape()
{
    return R"({"kernel_version":"0.4.0","adapter_version":"0.4.0","manual_requirements":[],"instructions":[{"type":"test","name":"n","metadata":{"bad":"\uD800"}}]})";
}

inline std::string bs_test_build_duplicate_metadata_amount()
{
    return R"({"kernel_version":"0.4.0","adapter_version":"0.4.0","manual_requirements":[],"instructions":[{"type":"test","name":"n","metadata":{"amount":"1","amount":"2"}}]})";
}

inline std::string bs_test_build_type_confusion_tax_rate_number()
{
    return R"({"kernel_version":"0.4.0","adapter_version":"0.4.0","manual_requirements":[],"instructions":[{"type":"test","name":"n","metadata":{"tax_rate":13}}]})";
}

inline std::string bs_test_build_injection_string_metadata()
{
    return R"({"kernel_version":"0.4.0","adapter_version":"0.4.0","manual_requirements":[],"instructions":[{"type":"test","name":"n","metadata":{"sql":"; DROP TABLE --"}}]})";
}

inline std::string bs_test_build_truncated_unclosed_string()
{
    return R"({"kernel_version":"0.4.0","adapter_version":"0.4.0","manual_requirements":[],"instructions":[{"type":"test","name":"n","metadata":{"x":")";
}

#endif /* BS_TEST_CONFIG_V1_SECURITY_BUILD_H */
