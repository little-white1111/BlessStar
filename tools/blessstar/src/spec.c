#include "blessstar/contract_parser.h"
#include "blessstar/contract_validator.h"
#include "blessstar/spec_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

/* Forward declarations */
static void print_usage(void);
static int cmd_check(int argc, char* argv[]);
static int cmd_generate(int argc, char* argv[]);
static const char* find_exec_dir(const char* argv0, char* buf, size_t buf_size);
static int file_exists(const char* path);

/**
 * Print usage information.
 */
static void print_usage(void) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  blessstar spec check <contract.yaml>\n");
    fprintf(stderr, "  blessstar spec generate <contract.yaml> --lang=<lang> [--manifest]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  check      Parse and validate a contract YAML file\n");
    fprintf(stderr, "  generate   Parse, validate, and generate implementation guide\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --lang=<lang>   Target language (default: c)\n");
    fprintf(stderr, "  --manifest      Also render manifest.json.mustache\n");
}

/**
 * Find the directory containing the executable.
 */
static const char* find_exec_dir(const char* argv0, char* buf, size_t buf_size) {
    if (!argv0 || !buf || buf_size == 0) return NULL;

    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", argv0);

#ifdef _WIN32
    /* On Windows, try to get the full path */
    char full_path[1024];
    if (_fullpath(full_path, tmp, sizeof(full_path))) {
        snprintf(tmp, sizeof(tmp), "%s", full_path);
    }
#endif

    /* Find last path separator */
    char* last_sep = strrchr(tmp, PATH_SEPARATOR);
#ifdef _WIN32
    /* Also check forward slash (MSYS/MinGW) */
    if (!last_sep) last_sep = strrchr(tmp, '/');
#endif
    if (!last_sep) {
        /* No directory component, use current directory */
        snprintf(buf, buf_size, ".");
    } else {
        *last_sep = '\0';
        snprintf(buf, buf_size, "%s", tmp);
    }

    return buf;
}

/**
 * Check if a file exists.
 */
static int file_exists(const char* path) {
    if (!path) return 0;
    FILE* fp = fopen(path, "rb");
    if (fp) {
        fclose(fp);
        return 1;
    }
    return 0;
}

/**
 * Handle "blessstar spec check <contract.yaml>"
 */
static int cmd_check(int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Error: missing contract.yaml path\n");
        print_usage();
        return 1;
    }

    const char* yaml_path = argv[3];
    bs_contract_t contract;
    
    if (bs_contract_parse_file(yaml_path, &contract) != 0) {
        fprintf(stderr, "Error: failed to parse contract file: %s\n", yaml_path);
        return 1;
    }

    char error_buf[4096] = {0};
    int errors = bs_contract_validate(&contract, error_buf, sizeof(error_buf));

    if (errors > 0) {
        fprintf(stderr, "Validation failed with %d error(s):\n%s", errors, error_buf);
        bs_contract_destroy(&contract);
        return 1;
    }

    printf("Contract is valid: biz_id=%s, fields=%zu, gates=%zu, effects=%zu\n",
           contract.biz_id, contract.fields_count, contract.gates_count, contract.effects_count);

    bs_contract_destroy(&contract);
    return 0;
}

/**
 * Handle "blessstar spec generate <contract.yaml> --lang=<lang>"
 */
static int cmd_generate(int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Error: missing contract.yaml path\n");
        print_usage();
        return 1;
    }

    const char* yaml_path = argv[3];
    const char* lang = "c";
    int render_manifest = 0;

    /* Parse optional flags from argv[4+] */
    for (int i = 4; i < argc; i++) {
        if (strncmp(argv[i], "--lang=", 7) == 0) {
            lang = argv[i] + 7;
        } else if (strcmp(argv[i], "--manifest") == 0) {
            render_manifest = 1;
        }
    }

    bs_contract_t contract;
    if (bs_contract_parse_file(yaml_path, &contract) != 0) {
        fprintf(stderr, "Error: failed to parse contract file: %s\n", yaml_path);
        return 1;
    }

    char error_buf[4096] = {0};
    int errors = bs_contract_validate(&contract, error_buf, sizeof(error_buf));
    if (errors > 0) {
        fprintf(stderr, "Validation failed with %d error(s):\n%s", errors, error_buf);
        bs_contract_destroy(&contract);
        return 1;
    }

    /* Find executable directory for template lookup */
    char exec_dir[1024];
    find_exec_dir(argv[0], exec_dir, sizeof(exec_dir));

    /* Determine template path — search order:
     * 1. Relative to executable directory (for installed/packaged builds)
     * 2. Relative to CWD (for development builds running from project root)
     */
    char template_path[1024];
    int template_found = 0;

    const char* search_dirs[] = {exec_dir, ".", "tools/blessstar"};
    for (int di = 0; di < 3 && !template_found; di++) {
        const char* base = search_dirs[di];

        if (strcmp(lang, "c") == 0) {
            /* For C, first try lang-specific, then fall back to generic */
            snprintf(template_path, sizeof(template_path),
                     "%s%ctemplates%cIMPLEMENTATION_GUIDE_%s.md.mustache",
                     base, PATH_SEPARATOR, PATH_SEPARATOR, lang);
            if (file_exists(template_path)) {
                template_found = 1;
            } else {
                snprintf(template_path, sizeof(template_path),
                         "%s%ctemplates%cIMPLEMENTATION_GUIDE.md.mustache",
                         base, PATH_SEPARATOR, PATH_SEPARATOR);
                if (file_exists(template_path)) {
                    template_found = 1;
                }
            }
        } else {
            /* Non-C language: try lang-specific template */
            snprintf(template_path, sizeof(template_path),
                     "%s%ctemplates%cIMPLEMENTATION_GUIDE_%s.md.mustache",
                     base, PATH_SEPARATOR, PATH_SEPARATOR, lang);
            if (file_exists(template_path)) {
                template_found = 1;
            }
        }
    }

    if (!template_found) {
        fprintf(stderr, "Error: template not found for lang='%s'\n", lang);
        bs_contract_destroy(&contract);
        return 1;
    }

    /* Render IMPLEMENTATION_GUIDE */
    if (bs_render_template_file(&contract, template_path, lang, stdout) != 0) {
        fprintf(stderr, "Error: failed to render template: %s\n", template_path);
        bs_contract_destroy(&contract);
        return 1;
    }

    /* Render manifest if requested */
    if (render_manifest) {
        char manifest_path[1024];
        snprintf(manifest_path, sizeof(manifest_path),
                 "%s%ctemplates%cmanifest.json.mustache",
                 exec_dir, PATH_SEPARATOR, PATH_SEPARATOR);

        if (file_exists(manifest_path)) {
            fprintf(stdout, "\n--- MANIFEST ---\n");
            if (bs_render_template_file(&contract, manifest_path, lang, stdout) != 0) {
                fprintf(stderr, "Error: failed to render manifest template\n");
                bs_contract_destroy(&contract);
                return 1;
            }
        } else {
            fprintf(stderr, "Warning: manifest template not found: %s\n", manifest_path);
        }
    }

    bs_contract_destroy(&contract);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Error: missing command\n");
        print_usage();
        return 1;
    }

    /* Parse subcommand */
    if (argc >= 2 && strcmp(argv[1], "spec") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: missing spec subcommand\n");
            print_usage();
            return 1;
        }

        if (strcmp(argv[2], "check") == 0) {
            return cmd_check(argc, argv);
        } else if (strcmp(argv[2], "generate") == 0) {
            return cmd_generate(argc, argv);
        } else {
            fprintf(stderr, "Error: unknown spec subcommand: %s\n", argv[2]);
            print_usage();
            return 1;
        }
    } else {
        fprintf(stderr, "Error: unknown command: %s\n", argv[1]);
        print_usage();
        return 1;
    }

    return 1;
}
