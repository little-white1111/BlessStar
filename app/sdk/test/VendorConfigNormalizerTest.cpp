#include "bs/kernel/common/bs_status.h"

#include "bs/adapter/parser/config_parse.h"

#include <cassert>
#include <cstdio>

#include <filesystem>
#include <fstream>

#include "bs/app/sdk/vendor_config_normalizer.h"
#include "bs/app/sdk/vendor_reload_facade.h"
#include "support/test_temp_dir.h"

namespace fs = std::filesystem;

static void write_text(const fs::path& path, const char* text)
{
    std::ofstream out(path, std::ios::binary);
    out << text;
}

int main()
{
    using namespace bs::app;

    const BsTestTempDirGuard tmp_guard(bs_test_unique_temp_dir("bs_vendor_config_normalizer"));

    const fs::path fixture = fs::path("app/sdk/test/fixtures/vendor_generic_business_good.json");
    const fs::path abs_fixture = fs::absolute(fixture);
    assert(fs::exists(abs_fixture));

    NormalizeResult result;
    assert(NormalizeVendorConfig(VendorFormat::GenericBusinessJson, abs_fixture.string(), &result));
    assert(result.ok);
    assert(!result.v1_bytes.empty());
    assert(result.source_vendor == "generic_business");
    assert(result.scenario == "expense_reimburse");

    BsConfigParseResult parse_out{};
    const BsStatus      st =
        bs_adapter_parser_parse_bytes(result.v1_bytes.data(), result.v1_bytes.size(), &parse_out);
    assert(bs_status_is_ok(st));
    bs_adapter_parser_result_destroy(&parse_out);

    const fs::path bad_file = tmp_guard.path / "vendor_bad.json";
    write_text(bad_file, "{ \"not\": \"v1\" }");
    NormalizeResult bad;
    assert(!NormalizeVendorConfig(VendorFormat::GenericBusinessJson, bad_file.string(), &bad));
    assert(!bad.ok);

    std::string uri;
    assert(NormalizeFileToTempUri(VendorFormat::GenericBusinessJson, abs_fixture.string(),
                                  tmp_guard.path.string(), &uri, &result));
    assert(uri.find("file:///") == 0);
    assert(fs::exists(tmp_guard.path / "bs_vendor_v1_normalized.json"));

    return 0;
}
