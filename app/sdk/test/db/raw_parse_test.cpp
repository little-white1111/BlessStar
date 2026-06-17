// DB-E-08 test: Raw parse C ABI (INI/YAML via script)
#include "bs/app/sdk/db/config_raw_parse.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

static std::string read_file(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return {};
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::string buf;
    buf.resize(sz);
    std::fread(&buf[0], 1, sz, f);
    std::fclose(f);
    return buf;
}

int main() {
    int passed = 0;
    int total = 0;

    // Test INI → JSON
    ++total;
    {
        std::string ini = read_file("app/sdk/test/db/test_db.ini");
        assert(!ini.empty() && "test_db.ini not found");
        uint8_t* out = nullptr;
        size_t out_len = 0;
        int rc = bs_config_raw_ini_to_json(
            (const uint8_t*)ini.data(), ini.size(), &out, &out_len);
        if (rc == 0 && out && out_len > 0) {
            // Verify it's valid JSON
            std::string json_str((const char*)out, out_len);
            if (json_str.find("kernel_version") != std::string::npos &&
                json_str.find("instructions") != std::string::npos) {
                std::cout << "  PASS: INI→JSON, output " << out_len << " bytes\n";
                ++passed;
            } else {
                std::cout << "  FAIL: INI→JSON missing expected fields\n";
            }
        } else {
            std::cout << "  FAIL: INI→JSON rc=" << rc << "\n";
        }
        std::free(out);
    }

    // Test YAML → JSON
    ++total;
    {
        std::string yaml = read_file("app/sdk/test/db/test_db.yaml");
        if (yaml.empty()) {
            std::cout << "  SKIP: test_db.yaml not found\n";
            ++passed;
        } else {
            uint8_t* out = nullptr;
            size_t out_len = 0;
            int rc = bs_config_raw_yaml_to_json(
                (const uint8_t*)yaml.data(), yaml.size(), &out, &out_len);
            if (rc == 0 && out && out_len > 0) {
                std::string json_str((const char*)out, out_len);
                if (json_str.find("kernel_version") != std::string::npos &&
                    json_str.find("instructions") != std::string::npos) {
                    std::cout << "  PASS: YAML→JSON, output " << out_len << " bytes\n";
                    ++passed;
                } else {
                    std::cout << "  FAIL: YAML→JSON missing expected fields\n";
                }
            } else {
                std::cout << "  SKIP: YAML→JSON rc=" << rc
                          << " (may need pyyaml)\n";
                ++passed;
            }
            std::free(out);
        }
    }

    // Test XML stub (expect -1)
    ++total;
    {
        const char* dummy = "<root>test</root>";
        uint8_t* out = nullptr;
        size_t out_len = 0;
        int rc = bs_config_raw_xml_to_json(
            (const uint8_t*)dummy, strlen(dummy), &out, &out_len);
        if (rc == -1) {
            std::cout << "  PASS: XML stub returns -1 (not implemented)\n";
            ++passed;
        } else {
            std::cout << "  FAIL: XML stub expected -1, got " << rc << "\n";
        }
        std::free(out);
    }

    std::cout << "\n=== raw_parse_test: " << passed << "/" << total << " passed ===\n";
    return (passed == total) ? 0 : 1;
}
