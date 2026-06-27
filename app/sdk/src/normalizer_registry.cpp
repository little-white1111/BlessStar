#include "bs/app/sdk/normalizer_plugin.h"

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

namespace
{

// ── 全局注册表 ────────────────────────────────────────────────────────
// key: vendor_id, value: 归一化函数指针
using RegistryMap = std::unordered_map<std::string, BsNormalizerFn>;

RegistryMap& registry()
{
    static RegistryMap inst;
    return inst;
}

std::mutex& registry_mutex()
{
    static std::mutex mtx;
    return mtx;
}

} // anonymous namespace

/* ── C ABI 实现 ────────────────────────────────────────────────────── */

int bs_normalizer_register(const char* vendor_id, BsNormalizerFn fn)
{
    if (!vendor_id || vendor_id[0] == '\0' || !fn)
        return -1;

    std::lock_guard<std::mutex> lock(registry_mutex());
    registry()[std::string(vendor_id)] = fn;
    return 0;
}

int bs_normalizer_normalize(const char* vendor_id,
                             const char* input_json,
                             const char* extra_json,
                             char** out_json,
                             size_t* out_len)
{
    if (!vendor_id || vendor_id[0] == '\0' || !input_json || !out_json || !out_len)
        return -1;

    BsNormalizerFn fn = nullptr;
    {
        std::lock_guard<std::mutex> lock(registry_mutex());
        auto it = registry().find(std::string(vendor_id));
        if (it == registry().end())
            return -1; // vendor_id 未注册
        fn = it->second;
    }

    // 调用插件函数
    return fn(vendor_id, input_json, extra_json, out_json, out_len);
}

int bs_normalizer_unregister(const char* vendor_id)
{
    if (!vendor_id || vendor_id[0] == '\0')
        return -1;

    std::lock_guard<std::mutex> lock(registry_mutex());
    auto it = registry().find(std::string(vendor_id));
    if (it == registry().end())
        return -1;
    registry().erase(it);
    return 0;
}

/* ── C ABI wrapper for editor bridge ────────────────────────────── */

extern "C" {

char* bs_normalizer_normalize_c(const char* vendor_id,
                                 const char* input_json,
                                 const char* extra_json)
{
    if (!vendor_id || !input_json)
        return nullptr;

    char* out_json = nullptr;
    size_t out_len = 0;
    int rc = bs_normalizer_normalize(vendor_id, input_json, extra_json,
                                      &out_json, &out_len);
    if (rc != 0 || !out_json)
        return nullptr;

    // bs_normalizer_normalize 分配了 out_json（malloc），直接返回
    return out_json;
}

} // extern "C"
