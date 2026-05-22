#ifndef BS_TEST_CONFIG_V1_AUDIT_BUILD_H
#define BS_TEST_CONFIG_V1_AUDIT_BUILD_H

#include "bs/adapter/parser/json_lexer.h"

#include <string>

inline std::string bs_test_build_audit_minimal_ok()
{
    return R"({"kernel_version":"0.4.0","adapter_version":"0.4.0","manual_requirements":[],"instructions":[{"type":"test","name":"n","metadata":{"note":"ok"}}]})";
}

/** Build instructions array with @p count minimal items. */
inline std::string bs_test_build_instructions_count(size_t count)
{
    std::string out =
        R"({"kernel_version":"0.4.0","adapter_version":"0.4.0","manual_requirements":[],"instructions":[)";
    for (size_t i = 0; i < count; ++i)
    {
        if (i > 0)
            out += ',';
        out += R"({"type":"t","name":")";
        out += std::to_string(i);
        out += R"(","metadata":{"k":"v"}})";
    }
    out += "]}";
    return out;
}

inline std::string bs_test_build_manual_requirements_count(size_t count)
{
    std::string out =
        R"({"kernel_version":"0.4.0","adapter_version":"0.4.0","manual_requirements":[)";
    for (size_t i = 0; i < count; ++i)
    {
        if (i > 0)
            out += ',';
        out += "\"m";
        out += std::to_string(i);
        out += '"';
    }
    out += R"(],"instructions":[{"type":"test","name":"n","metadata":{}}]})";
    return out;
}

#endif /* BS_TEST_CONFIG_V1_AUDIT_BUILD_H */
