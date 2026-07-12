/**
 * biz_bootstrap — BlessStar 业务系统注册 & 门禁链搭建工具
 *
 * 根据 ADR 方案C（弹性扩展）实现：
 *   1. 扫描 biz-registry/ 目录
 *   2. 加载 manifest.json → 注册字段 + 注册业务系统
 *   3. 加载 gate_rule_def.json → 编译门禁规则 → 搭建门禁链
 *   4. 验证并打印摘要
 *
 * 编译方式 (在 BlessStar 项目根目录):
 *   cmake -S . -B build
 *   cmake --build build --target biz_bootstrap
 *
 * 运行方式:
 *   ./build/bin/biz_bootstrap [business_dir]
 */

#include "gate_rule_loader.h"

#include "bs/adapter/business/manifest.h"
#include "bs/adapter/business/manifest_loader.h"
#include "bs/adapter/business/registry.h"
#include "bs/adapter/business/scanner.h"
#include "bs/adapter/business/version_check.h"

#include "bs/app/sdk/config_declare.h"

#include "bs/kernel/gate_chain/gate_chain_types.h"
#include "bs/kernel/gate_chain/gate_evaluator.h"
#include "bs/kernel/gate_chain/gate_factory.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

/* ─── Helpers ──────────────────────────────────────────────────────────── */

static std::string current_sdk_version() {
    return "1.0.0"; /* matches manifest sdk_version ">=1.0.0 <2.0.0" */
}

/* Map manifest field type string to bs_field_type_t */
static bs_field_type_t map_field_type(const char* type_str) {
    if (!type_str) return BS_TYPE_STRING;
    if (strcmp(type_str, "string")  == 0 ||
        strcmp(type_str, "STR")     == 0 ||
        strcmp(type_str, "STRING")  == 0) return BS_TYPE_STRING;
    if (strcmp(type_str, "integer") == 0 ||
        strcmp(type_str, "I32")     == 0 ||
        strcmp(type_str, "INT32")   == 0) return BS_TYPE_INT32;
    if (strcmp(type_str, "I64")     == 0 ||
        strcmp(type_str, "INT64")   == 0) return BS_TYPE_INT64;
    if (strcmp(type_str, "float")   == 0 ||
        strcmp(type_str, "F64")     == 0 ||
        strcmp(type_str, "FLOAT64") == 0) return BS_TYPE_DOUBLE;
    if (strcmp(type_str, "bool")    == 0 ||
        strcmp(type_str, "BOOL")    == 0) return BS_TYPE_BOOL;
    return BS_TYPE_STRING;
}

/* ─── Step 1: Register fields from manifest ──────────────────────────────── */

static int register_fields_from_manifest(const bs_manifest_t* manifest) {
    if (!manifest || !manifest->fields || manifest->fields_count == 0) {
        std::cout << "  No fields to register\n";
        return 0;
    }

    /* Build bs_field_decl_t array */
    std::vector<bs_field_decl_t> decls;
    decls.reserve(manifest->fields_count);

    for (size_t i = 0; i < manifest->fields_count; i++) {
        bs_manifest_field_t* mf = &manifest->fields[i];
        bs_field_decl_t fd;
        fd.key         = mf->key;
        fd.type        = map_field_type(mf->type);
        fd.default_str = mf->default_str;
        fd.description = mf->description;
        fd.required    = mf->required != 0;
        decls.push_back(fd);

        std::cout << "    Field: " << mf->key
                  << " (type=" << mf->type
                  << ", default=" << mf->default_str
                  << ", required=" << (mf->required ? "true" : "false")
                  << ")\n";
    }

    /* Register all fields at once */
    int rc = bs_config_declare(decls.data(), decls.size());
    if (rc == 0) {
        std::cout << "  Registered " << decls.size() << " fields via bs_config_declare()\n";
    } else {
        std::cerr << "  [ERROR] bs_config_declare() failed with rc=" << rc << "\n";
        return -1;
    }

    return 0;
}

/* ─── Step 2: Register business system ──────────────────────────────────── */

static int register_biz_system(const bs_manifest_t* manifest) {
    if (!manifest) return -1;

    /* Version check */
    std::string sdk_ver = current_sdk_version();
    int compat = bs_version_compatible(sdk_ver.c_str(), manifest->sdk_version);
    if (compat != 0) {
        std::cerr << "  [ERROR] SDK version " << sdk_ver
                  << " is NOT compatible with manifest requirement: "
                  << manifest->sdk_version << "\n";
        return -1;
    }
    std::cout << "  SDK version " << sdk_ver << " compatible with requirement: "
              << manifest->sdk_version << "\n";

    /* Register via biz_registry */
    int rc = bs_biz_registry_register(manifest);
    if (rc == 0) {
        std::cout << "  Registered biz: " << manifest->biz_id
                  << " (" << manifest->display_name << ")\n";
    } else if (rc == -1) {
        std::cout << "  Biz already registered: " << manifest->biz_id << "\n";
    } else {
        std::cerr << "  [ERROR] biz_registry_register() failed with rc=" << rc << "\n";
        return -1;
    }

    return 0;
}

/* ─── Step 3: Build gate chain from rule defs ────────────────────────────── */

static int build_gates(const std::string& biz_dir, const std::string& biz_id) {
    std::string gate_file = biz_dir + "/gate_rule_def.json";

    /* Load rules */
    auto rules = load_gate_rules(gate_file);
    if (rules.empty()) {
        /* Check if file exists */
        std::ifstream test(gate_file);
        if (!test) {
            std::cout << "  No gate_rule_def.json found (skipping gate build)\n";
            return 0;
        }
        std::cout << "  0 gate rules loaded (all disabled or empty?)\n";
        return 0;
    }

    std::cout << "  Loaded " << rules.size() << " gate rules from gate_rule_def.json\n";

    /* Create gate chain */
    bs_gate_chain_t* chain = bs_gate_chain_create();
    if (!chain) {
        std::cerr << "  [ERROR] Failed to create gate chain\n";
        return -1;
    }

    /* Build gate chain from rules */
    int rc = build_gate_chain(chain, rules, biz_id);
    if (rc != 0) {
        std::cerr << "  [WARN] Gate chain build had errors (partial build may still be usable)\n";
    }

    /* Print summary */
    print_gate_chain_summary(chain, biz_id);

    /* Print rule detail by sub_category */
    int threshold_count = 0, alert_count = 0, approval_count = 0, enum_check_count = 0;
    for (const auto& r : rules) {
        if (r.sub_category == "threshold")  threshold_count++;
        else if (r.sub_category == "alert") alert_count++;
        else if (r.sub_category == "approval") approval_count++;
        else if (r.sub_category == "enum_check") enum_check_count++;
    }
    std::cout << "  Rule breakdown: threshold=" << threshold_count
              << " alert=" << alert_count
              << " approval=" << approval_count
              << " enum_check=" << enum_check_count << "\n";

    /* ── Verification: run a sample evaluation ── */
    if (rules.size() > 0) {
        const GateRuleDef& first = rules[0];
        bs_gate_eval_context_t eval_ctx;
        eval_ctx.field_key = first.field_key.c_str();
        eval_ctx.field_value = first.value.c_str();
        eval_ctx.user_data = nullptr;

        bs_gate_eval_result_t eval_result;
        int eval_rc = bs_gate_evaluator_evaluate(chain, &eval_ctx, &eval_result);
        if (eval_rc == 0) {
            std::cout << "  [VERIFY] Gate evaluation for " << first.field_key
                      << " = " << first.value
                      << " -> " << (eval_result.passed ? "PASS" : "FAIL")
                      << " (" << (eval_result.error_message ? eval_result.error_message : "") << ")\n";
            bs_gate_eval_result_free(&eval_result);
        } else {
            std::cout << "  [VERIFY] Gate evaluation skipped (no evaluator or chain empty)\n";
        }
    }

    /* Don't free the chain — in production it stays resident.
     * For the CLI tool, we free it since we're exiting. */
    bs_gate_chain_free(chain);
    return 0;
}

/* ─── Process a single business system ───────────────────────────────────── */

static int process_biz_dir(const std::string& biz_dir, const std::string& biz_id) {
    std::string manifest_path = biz_dir + "/manifest.json";

    std::cout << "\n═══════════════════════════════════════════════════════════\n";
    std::cout << "  Processing business: " << biz_id << "\n";
    std::cout << "  Directory: " << biz_dir << "\n";
    std::cout << "═══════════════════════════════════════════════════════════\n";

    /* Load manifest */
    bs_manifest_t manifest;
    int rc = bs_manifest_load_from_file(manifest_path.c_str(), &manifest);
    if (rc != 0) {
        std::cerr << "  [ERROR] Failed to load manifest: " << manifest_path
                  << " (rc=" << rc << ")\n";
        return -1;
    }

    std::cout << "  Manifest loaded: " << manifest.biz_id
              << " v" << manifest.sdk_version << "\n";

    /* Step 1: Register fields */
    if (register_fields_from_manifest(&manifest) != 0) {
        bs_manifest_destroy(&manifest);
        return -1;
    }

    /* Step 2: Register biz system */
    if (register_biz_system(&manifest) != 0) {
        bs_manifest_destroy(&manifest);
        return -1;
    }

    bs_manifest_destroy(&manifest);

    /* Step 3: Build gate chain */
    if (build_gates(biz_dir, biz_id) != 0) {
        std::cerr << "  [WARN] Gate chain build had issues (continuing)\n";
    }

    return 0;
}

/* ─── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char** argv) {
    std::cout << "\n"
              << "╔══════════════════════════════════════════════════════╗\n"
              << "║   BlessStar 业务系统 Bootstrap — 注册 & 门禁链搭建    ║\n"
              << "╚══════════════════════════════════════════════════════╝\n"
              << "\n";

    /* Determine business directory */
    std::string base_dir;
    if (argc > 1) {
        base_dir = argv[1];
    } else {
        /* Default: look for biz-registry/ in project root */
        base_dir = "biz-registry";
    }

    std::cout << "Base directory: " << base_dir << "\n";

    /* Check if base_dir exists */
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(base_dir.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        std::cerr << "[ERROR] Directory not found: " << base_dir << "\n";
        return 1;
    }
#else
    struct stat st;
    if (stat(base_dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        std::cerr << "[ERROR] Directory not found: " << base_dir << "\n";
        return 1;
    }
#endif

    /* Scan for business systems */
    bs_manifest_t* manifests = nullptr;
    size_t manifest_count = 0;

    int scan_rc = bs_biz_scanner_scan(base_dir.c_str(), &manifests, &manifest_count);
    if (scan_rc != 0) {
        std::cerr << "[ERROR] Scanner failed with rc=" << scan_rc << "\n";
        return 1;
    }

    if (manifest_count == 0) {
        std::cout << "\nNo business systems found in " << base_dir << "/\n";
        std::cout << "Expected structure: " << base_dir << "/<biz_id>/manifest.json\n";
        bs_biz_scanner_free_result(manifests, manifest_count);
        return 0;
    }

    std::cout << "\nFound " << manifest_count << " business system(s):\n";
    int success_count = 0;

    for (size_t i = 0; i < manifest_count; i++) {
        std::string biz_dir = base_dir + "/" + manifests[i].biz_id;
        int rc = process_biz_dir(biz_dir, manifests[i].biz_id);
        if (rc == 0) success_count++;
    }

    bs_biz_scanner_free_result(manifests, manifest_count);

    /* Print final summary */
    std::cout << "\n═══════════════════════════════════════════════════════════\n";
    std::cout << "  Bootstrap Complete: " << success_count << "/" << manifest_count
              << " business systems registered successfully\n";
    std::cout << "═══════════════════════════════════════════════════════════\n";

    /* Print all registered biz systems */
    char** ids = nullptr;
    size_t reg_count = bs_biz_registry_list(&ids);
    std::cout << "\nRegistered business systems (" << reg_count << "):\n";
    for (size_t i = 0; i < reg_count; i++) {
        const bs_manifest_t* m = bs_biz_registry_lookup(ids[i]);
        std::cout << "  " << (i + 1) << ". " << ids[i];
        if (m) std::cout << " (" << m->display_name << " - " << m->fields_count << " fields)";
        std::cout << "\n";
    }
    bs_biz_registry_free_list(ids, reg_count);

    /* Print schema summary */
    char* schema_json = nullptr;
    size_t schema_len = 0;
    if (bs_config_declare_get_schema_json(&schema_json, &schema_len) == 0 && schema_json) {
        std::cout << "\nSchema JSON: " << schema_len << " bytes ("
                  << std::count(schema_json, schema_json + schema_len, '"') / 2
                  << " registered fields)\n";
        std::free(schema_json);
    }

    std::cout << "\nDone.\n";
    return success_count == (int)manifest_count ? 0 : 1;
}
