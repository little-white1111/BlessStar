#include "bs/app/sdk/db/config_raw_parse.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

// Helper: resolve script absolute path
static std::string resolve_script_path(const char* script_name) {
    // Priority 1: env BS_NORMALIZE_DIR
    const char* normalize_dir = getenv("BS_NORMALIZE_DIR");
    if (normalize_dir) {
        return std::string(normalize_dir) + "/" + script_name;
    }
    // Priority 2: try to find script relative to cwd (project root expected)
    // Use absolute path from cwd
    char cwd_buf[4096];
#if defined(_WIN32)
    if (GetCurrentDirectoryA(sizeof(cwd_buf), cwd_buf)) {
        return std::string(cwd_buf) + "\\tools\\normalize\\examples\\" + script_name;
    }
#else
    if (getcwd(cwd_buf, sizeof(cwd_buf))) {
        return std::string(cwd_buf) + "/tools/normalize/examples/" + script_name;
    }
#endif
    // fallback
    return std::string("tools/normalize/examples/") + script_name;
}

// Helper: check if file exists
static bool file_exists(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (f) { fclose(f); return true; }
    return false;
}

// Helper: run python script and capture stderr
static int run_script(const std::string& script_path,
                      const std::string& input_path,
                      const std::string& output_path,
                      std::string& err_msg) {
    // Build command: python script.py <input> <output> 2>&1
    std::string cmd =
#if defined(_WIN32)
        std::string("python \"") + script_path + "\" \"" + input_path + "\" \"" + output_path + "\" 2>&1";
#else
        std::string("\"") + script_path + "\" \"" + input_path + "\" \"" + output_path + "\" 2>&1";
#endif

    FILE* pipe = nullptr;
#if defined(_WIN32)
    pipe = _popen(cmd.c_str(), "r");
#else
    pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) {
        err_msg = "popen failed";
        return 2;
    }

    char buf[4096];
    std::string stderr_out;
    while (fgets(buf, sizeof(buf), pipe)) {
        stderr_out += buf;
    }

#if defined(_WIN32)
    int rc = _pclose(pipe);
#else
    int rc = pclose(pipe);
#endif

    if (rc != 0) {
        err_msg = stderr_out.empty()
            ? std::string("script exited with code ") + std::to_string(rc)
            : stderr_out;
        if (rc == 1 || rc == 2) return rc;
        return 1;
    }
    return 0;
}

// Helper: write data to temp file
static int write_temp_file(const std::string& path, const uint8_t* data, size_t len) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return -1;
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    return (written == len) ? 0 : -1;
}

// Helper: read temp file into buffer (caller free()s *out)
static int read_temp_file(const std::string& path, uint8_t** out, size_t* out_len) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { free(*out); *out = nullptr; *out_len = 0; return -1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return -1; }
    *out = (uint8_t*)malloc(sz);
    if (!*out) { fclose(f); return -1; }
    *out_len = (size_t)sz;
    fread(*out, 1, sz, f);
    fclose(f);
    return 0;
}

// Helper: generate temporary file path
static std::string temp_path(const char* prefix) {
#if defined(_WIN32)
    char buf[MAX_PATH];
    char tmpdir_buf[MAX_PATH];
    DWORD ret = GetTempPathA(MAX_PATH, tmpdir_buf);
    if (ret == 0 || ret > MAX_PATH) return "";
    // GetTempFileNameA creates a unique temp file (creates 0-byte file)
    UINT uRet = GetTempFileNameA(tmpdir_buf, prefix, 0, buf);
    if (uRet == 0) return "";
    // File created (0 bytes); we'll overwrite it later
    return std::string(buf);
#else
    std::string tmpl = std::string("/tmp/") + prefix + "_XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    int fd = mkstemp(buf.data());
    if (fd == -1) return "";
    close(fd);
    return std::string(buf.data());
#endif
}

// Main conversion pipeline:
//   raw bytes → temp file → python script → temp output → read back
static int raw_to_json_via_script(const uint8_t* data, size_t len,
                                  const char* script_name,
                                  uint8_t** out, size_t* out_len) {
    if (!data || !out || !out_len) return -1;
    *out = nullptr;
    *out_len = 0;

    // Resolve script path
    std::string script_path = resolve_script_path(script_name);
    if (!file_exists(script_path)) {
        fprintf(stderr, "config_raw_parse: script not found: %s\n", script_path.c_str());
        return -2;
    }

    // Temp files
    std::string tmp_in = temp_path("bs_raw_in");
    std::string tmp_out = temp_path("bs_raw_out");
    if (tmp_in.empty() || tmp_out.empty()) {
        fprintf(stderr, "config_raw_parse: failed to create temp path\n");
        return -2;
    }

    // Write input
    if (write_temp_file(tmp_in, data, len) != 0) {
        remove(tmp_in.c_str()); remove(tmp_out.c_str());
        return -2;
    }

    // Run script
    std::string err_msg;
    int rc = run_script(script_path, tmp_in, tmp_out, err_msg);
    if (rc != 0) {
        fprintf(stderr, "config_raw_parse: %s failed: %s\n",
                script_path.c_str(), err_msg.c_str());
        remove(tmp_in.c_str()); remove(tmp_out.c_str());
        return rc > 0 ? -rc : -1;
    }

    // Read output
    rc = read_temp_file(tmp_out, out, out_len);
    remove(tmp_in.c_str()); remove(tmp_out.c_str());
    if (rc != 0) {
        fprintf(stderr, "config_raw_parse: failed to read script output\n");
        return -2;
    }

    // Ensure null-termination for convenience
    if (*out && *out_len > 0) {
        *out = (uint8_t*)realloc(*out, *out_len + 1);
        if (*out) {
            (*out)[*out_len] = '\0';
        }
    }

    return 0;
}

// ── C ABI ──────────────────────────────────────────────────────────

int bs_config_raw_ini_to_json(const uint8_t* data, size_t len,
                              uint8_t** out, size_t* out_len) {
    return raw_to_json_via_script(data, len, "ini2v1.py", out, out_len);
}

int bs_config_raw_xml_to_json(const uint8_t* data, size_t len,
                              uint8_t** out, size_t* out_len) {
    (void)data; (void)len; (void)out; (void)out_len;
    return -1; // XML not implemented in MVP
}

int bs_config_raw_yaml_to_json(const uint8_t* data, size_t len,
                               uint8_t** out, size_t* out_len) {
    return raw_to_json_via_script(data, len, "yaml2v1.py", out, out_len);
}
