#include "bs/adapter/parser/config_v1_ast.h"
#include "bs/adapter/parser/json_parser.h"

#include "bs/kernel/common/bs_status.h"

#include <cassert>
#include <cstring>

static const char kMinimal[] = R"({
  "kernel_version": "0.4.0",
  "adapter_version": "0.4.0",
  "manual_requirements": [],
  "instructions": [
    {
      "type": "test",
      "name": "reload-smoke-1",
      "metadata": {
        "subject_code": "1001.01",
        "tax_rate": "13"
      }
    }
  ]
})";

int main()
{
    ConfigV1Ast* ast = nullptr;
    size_t       line = 0;
    size_t       col  = 0;
    BsStatus     st   = json_parse_config_v1(kMinimal, strlen(kMinimal), &ast, &line, &col);
    assert(bs_status_is_ok(st));
    assert(ast != nullptr);
    assert(strcmp(ast->kernel_version, "0.4.0") == 0);
    assert(ast->instructions_count == 1);
    assert(ast->instructions != nullptr);
    assert(strcmp(ast->instructions->type, "test") == 0);
    assert(ast->instructions->metadata != nullptr);
    config_v1_ast_destroy(ast);
    ast = nullptr;

    const char kBad[] = "{";
    st = json_parse_config_v1(kBad, strlen(kBad), &ast, &line, &col);
    assert(!bs_status_is_ok(st));
    assert(ast == nullptr);
    return 0;
}
