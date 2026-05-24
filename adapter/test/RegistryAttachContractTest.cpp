/**
 * Attach/regression contract tests (part I): registry rules on implemented APIs.
 * Labels: unit;registry;attach
 */

#include "bs/kernel/registry/path_registry.h"
#include "bs/kernel/registry/registry_facade.h"

#include "bs/adapter/registry_bootstrap.h"

#include <cstdio>
#include <cstring>

#define REQUIRE(cond)                                                                              \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
        {                                                                                          \
            std::fprintf(stderr, "FAIL %s:%d: (%s)\n", __FILE__, __LINE__, #cond);                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int test_bind_without_declaration(PathRegistry* reg)
{
    REQUIRE(bs_path_registry_advance_phase(reg, BS_REGISTRY_PHASE_P1) == BS_REGISTRY_OK);
    int stub = 1;
    REQUIRE(bs_path_registry_bind_instance(reg, "/kernel/ir/no_decl", &stub) ==
            BS_REGISTRY_ERR_NO_DECLARATION);
    return 0;
}

static int test_hub_undeclared_resolve(RegistryFacade* facade)
{
    REQUIRE(bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P1) == BS_REGISTRY_OK);
    REQUIRE(bs_registry_facade_register_hub_mapping(facade, "kernel.ir.ghost", "/kernel/ir/ghost",
                                                    0) == BS_REGISTRY_OK);
    Binding b{};
    REQUIRE(bs_registry_facade_resolve(facade, "kernel.ir.ghost", &b) == BS_REGISTRY_ERR_NOT_FOUND);
    REQUIRE(bs_registry_facade_resolve(facade, "/kernel/ir/ghost", &b) ==
            BS_REGISTRY_ERR_NOT_FOUND);
    return 0;
}

static int test_not_found(RegistryFacade* facade)
{
    REQUIRE(bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P1) == BS_REGISTRY_OK);
    Binding b{};
    REQUIRE(bs_registry_facade_resolve(facade, "/kernel/ir/missing", &b) ==
            BS_REGISTRY_ERR_NOT_FOUND);
    REQUIRE(bs_registry_facade_resolve(facade, "kernel.ir.missing", &b) ==
            BS_REGISTRY_ERR_NOT_FOUND);
    return 0;
}

static int test_adapter_manifest_required(RegistryFacade* facade)
{
    REQUIRE(bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P1) == BS_REGISTRY_OK);
    REQUIRE(bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P2) == BS_REGISTRY_OK);

    PathEntry plugin{};
    plugin.source          = BS_PATH_ENTRY_PLUGIN;
    plugin.manifest_ref    = "";
    plugin.type_constraint = nullptr;
    REQUIRE(bs_registry_facade_register_declaration(facade, "/adapter/plugin/no_manifest",
                                                    &plugin) == BS_REGISTRY_ERR_MANIFEST);
    return 0;
}

static int test_list_subtree_depth(PathRegistry* reg)
{
    REQUIRE(bs_path_registry_advance_phase(reg, BS_REGISTRY_PHASE_P1) == BS_REGISTRY_OK);

    PathEntry builtin{};
    builtin.source          = BS_PATH_ENTRY_BUILTIN;
    builtin.manifest_ref    = "builtin";
    builtin.type_constraint = nullptr;

    REQUIRE(bs_path_registry_register_declaration(reg, "/kernel/a", &builtin) == BS_REGISTRY_OK);
    REQUIRE(bs_path_registry_register_declaration(reg, "/kernel/a/b", &builtin) == BS_REGISTRY_OK);
    REQUIRE(bs_path_registry_register_declaration(reg, "/kernel/a/b/c", &builtin) ==
            BS_REGISTRY_OK);

    char  buf0[BS_REGISTRY_MAX_PATH];
    char  buf1[BS_REGISTRY_MAX_PATH];
    char  buf2[BS_REGISTRY_MAX_PATH];
    char* rows[]    = {buf0, buf1, buf2};
    int   out_count = 0;

    REQUIRE(bs_path_registry_list_subtree(reg, "/kernel/a", 2, rows, 3, &out_count) ==
            BS_REGISTRY_OK);
    REQUIRE(out_count == 2);

    out_count = 0;
    REQUIRE(bs_path_registry_list_subtree(reg, "/kernel/a", 3, rows, 3, &out_count) ==
            BS_REGISTRY_ERR_INVALID_ARG);
    return 0;
}

static int test_duplicate_decl_and_bind(PathRegistry* reg)
{
    REQUIRE(bs_path_registry_advance_phase(reg, BS_REGISTRY_PHASE_P1) == BS_REGISTRY_OK);

    PathEntry builtin{};
    builtin.source          = BS_PATH_ENTRY_BUILTIN;
    builtin.manifest_ref    = "builtin";
    builtin.type_constraint = nullptr;

    REQUIRE(bs_path_registry_register_declaration(reg, "/kernel/dup", &builtin) == BS_REGISTRY_OK);
    REQUIRE(bs_path_registry_register_declaration(reg, "/kernel/dup", &builtin) ==
            BS_REGISTRY_ERR_ALREADY_EXISTS);

    int a = 1;
    REQUIRE(bs_path_registry_bind_instance(reg, "/kernel/dup", &a) == BS_REGISTRY_OK);
    int b = 2;
    REQUIRE(bs_path_registry_bind_instance(reg, "/kernel/dup", &b) ==
            BS_REGISTRY_ERR_ALREADY_EXISTS);
    return 0;
}

static int test_adapter_blocked_in_phase_p1(RegistryFacade* facade)
{
    REQUIRE(bs_adapter_registry_bootstrap_begin(facade) == 0);
    REQUIRE(bs_registry_facade_current_phase(facade) == BS_REGISTRY_PHASE_P1);

    PathEntry plugin{};
    plugin.source          = BS_PATH_ENTRY_PLUGIN;
    plugin.manifest_ref    = "m";
    plugin.type_constraint = nullptr;
    REQUIRE(bs_registry_facade_register_declaration(facade, "/adapter/plugin/early", &plugin) ==
            BS_REGISTRY_ERR_PHASE);
    return 0;
}

static int test_advance_phase_monotonic(RegistryFacade* facade)
{
    REQUIRE(bs_registry_facade_current_phase(facade) == BS_REGISTRY_PHASE_P0);
    REQUIRE(bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P2) ==
            BS_REGISTRY_ERR_PHASE);
    REQUIRE(bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P1) == BS_REGISTRY_OK);
    REQUIRE(bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P2) == BS_REGISTRY_OK);
    REQUIRE(bs_registry_facade_advance_phase(facade, BS_REGISTRY_PHASE_P1) ==
            BS_REGISTRY_ERR_PHASE);
    return 0;
}

int main()
{
    int failures = 0;

    PathRegistry* reg_bind = bs_path_registry_create();
    if (!reg_bind || test_bind_without_declaration(reg_bind) != 0)
        ++failures;
    bs_path_registry_destroy(reg_bind);

    PathRegistry* reg_list = bs_path_registry_create();
    if (!reg_list || test_list_subtree_depth(reg_list) != 0)
        ++failures;
    bs_path_registry_destroy(reg_list);

    PathRegistry* reg_dup = bs_path_registry_create();
    if (!reg_dup || test_duplicate_decl_and_bind(reg_dup) != 0)
        ++failures;
    bs_path_registry_destroy(reg_dup);

    RegistryFacade* f_hub = bs_registry_facade_create();
    if (!f_hub || test_hub_undeclared_resolve(f_hub) != 0)
        ++failures;
    bs_registry_facade_destroy(f_hub);

    RegistryFacade* f_nf = bs_registry_facade_create();
    if (!f_nf || test_not_found(f_nf) != 0)
        ++failures;
    bs_registry_facade_destroy(f_nf);

    RegistryFacade* f_manifest = bs_registry_facade_create();
    if (!f_manifest || test_adapter_manifest_required(f_manifest) != 0)
        ++failures;
    bs_registry_facade_destroy(f_manifest);

    RegistryFacade* f_phase = bs_registry_facade_create();
    if (!f_phase || test_advance_phase_monotonic(f_phase) != 0)
        ++failures;
    bs_registry_facade_destroy(f_phase);

    RegistryFacade* f_p1 = bs_registry_facade_create();
    if (!f_p1 || test_adapter_blocked_in_phase_p1(f_p1) != 0)
        ++failures;
    bs_registry_facade_destroy(f_p1);

    bs_adapter_registry_shutdown_log();

    if (failures != 0)
    {
        std::fprintf(stderr, "RegistryAttachContractTest: %d case(s) failed\n", failures);
        return 1;
    }
    return 0;
}
