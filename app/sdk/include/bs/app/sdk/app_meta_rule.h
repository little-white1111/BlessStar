#ifndef BS_APP_SDK_APP_META_RULE_H
#define BS_APP_SDK_APP_META_RULE_H

/*
 * C-ST-7 contract block:
 * Thread safety: pure; conversion only.
 * Error semantics: none.
 */

#include <string>
#include "bs/adapter/parser/meta_rule.h"

namespace bs::app {

struct MetaRule {
    std::string instr_name; // "" = match all
    std::string key;
    BsMetaOp    op = BS_META_EQ;
    std::string value;
};

/** Convert C++ MetaRule to C BsMetaRule.
 *  dst fields point into src's internal storage; src must outlive dst usage.
 */
inline void to_c_rule(const MetaRule& src, BsMetaRule* dst) {
    dst->instr_name = src.instr_name.empty() ? nullptr : src.instr_name.c_str();
    dst->key        = src.key.empty() ? nullptr : src.key.c_str();
    dst->op         = src.op;
    // For EXISTS / NOT_EXISTS value may be ignored
    if (src.op == BS_META_EXISTS || src.op == BS_META_NOT_EXISTS) {
        dst->value = nullptr;
    } else {
        dst->value = src.value.empty() ? nullptr : src.value.c_str();
    }
}

} // namespace bs::app

#endif // BS_APP_SDK_APP_META_RULE_H