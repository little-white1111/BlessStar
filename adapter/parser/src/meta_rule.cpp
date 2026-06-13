#include "bs/adapter/parser/meta_rule.h"

#include <cstdlib>
#include <cstring>

#if defined(_MSC_VER)
#  include <regex>
#else
#  include <regex.h>
#endif

/* ---------------------------------------------------------------------------
 * String helpers
 * --------------------------------------------------------------------------- */

/** True if s is NULL or empty. */
static int is_empty(const char* s)
{
    return !s || s[0] == '\0';
}

/** Safe strcmp that tolerates NULL. */
static int streq(const char* a, const char* b)
{
    if (!a || !b) return (a == b);
    return std::strcmp(a, b) == 0;
}

/** Substring search that tolerates NULL haystack. */
static int str_contains(const char* haystack, const char* needle)
{
    if (!haystack || !needle) return 0;
    return std::strstr(haystack, needle) != NULL;
}

/* ---------------------------------------------------------------------------
 * Regex helper (platform-independent)
 * ---------------------------------------------------------------------------
 * Returns 0 on match, non-zero on no-match or error.
 */

struct RegexHelper
{
#if defined(_MSC_VER)
    std::regex re;
    bool       valid;

    RegexHelper(const char* pattern)
        : re(pattern, std::regex::extended)
        , valid(true)
    {
        try { re = std::regex(pattern, std::regex::extended); }
        catch (...) { valid = false; }
    }

    int exec(const char* str) const
    {
        if (!valid || !str) return -1;
        return std::regex_match(str, re) ? 0 : -1;
    }
#else
    regex_t re;
    bool    valid;

    RegexHelper(const char* pattern)
        : valid(true)
    {
        int rc = regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB);
        if (rc != 0) valid = false;
    }

    ~RegexHelper()
    {
        if (valid) regfree(&re);
    }

    int exec(const char* str) const
    {
        if (!valid || !str) return -1;
        return regexec(&re, str, 0, NULL, 0);
    }
#endif
};

/* ---------------------------------------------------------------------------
 * Per-rule check against a single instruction
 * ---------------------------------------------------------------------------
 * Returns 0 if the instruction passes this rule, non-zero on failure.
 * On failure, if out_buf is non-NULL, writes a human-readable message.
 */

static int check_one_rule(const IRInstruction* instr,
                          const BsMetaRule* rule,
                          char* out_buf, size_t out_cap)
{
    const char* meta_val = bs_ir_instruction_get_metadata(instr, rule->key);

    /* EXISTS and NOT_EXISTS handled separately (no value needed). */
    switch (rule->op)
    {
    case BS_META_EXISTS:
        if (!meta_val)
        {
            if (out_buf && out_cap > 0)
            {
                int n = std::snprintf(out_buf, out_cap,
                                      "[%s/%s] metadata key \"%s\" should exist but not found",
                                      instr->type ? instr->type : "",
                                      instr->name ? instr->name : "",
                                      rule->key);
                if (n < 0) out_buf[0] = '\0';
            }
            return -1;
        }
        return 0;

    case BS_META_NOT_EXISTS:
        if (meta_val)
        {
            if (out_buf && out_cap > 0)
            {
                int n = std::snprintf(out_buf, out_cap,
                                      "[%s/%s] metadata key \"%s\" should not exist but found=\"%s\"",
                                      instr->type ? instr->type : "",
                                      instr->name ? instr->name : "",
                                      rule->key, meta_val);
                if (n < 0) out_buf[0] = '\0';
            }
            return -1;
        }
        return 0;

    default:
        break;
    }

    /* All remaining ops require the key to exist. */
    if (!meta_val)
    {
        if (out_buf && out_cap > 0)
        {
            int n = std::snprintf(out_buf, out_cap,
                                  "[%s/%s] metadata key \"%s\" not found (required by op=%d)",
                                  instr->type ? instr->type : "",
                                  instr->name ? instr->name : "",
                                  rule->key, (int)rule->op);
            if (n < 0) out_buf[0] = '\0';
        }
        return -1;
    }

    switch (rule->op)
    {
    case BS_META_EQ:
        if (!streq(meta_val, rule->value))
        {
            if (out_buf && out_cap > 0)
            {
                int n = std::snprintf(out_buf, out_cap,
                                      "[%s/%s] %s: eq expected \"%s\" got \"%s\"",
                                      instr->type ? instr->type : "",
                                      instr->name ? instr->name : "",
                                      rule->key, rule->value, meta_val);
                if (n < 0) out_buf[0] = '\0';
            }
            return -1;
        }
        return 0;

    case BS_META_NE:
        if (streq(meta_val, rule->value))
        {
            if (out_buf && out_cap > 0)
            {
                int n = std::snprintf(out_buf, out_cap,
                                      "[%s/%s] %s: ne unexpected value \"%s\"",
                                      instr->type ? instr->type : "",
                                      instr->name ? instr->name : "",
                                      rule->key, rule->value);
                if (n < 0) out_buf[0] = '\0';
            }
            return -1;
        }
        return 0;

    case BS_META_GT:
    {
        double mv = std::atof(meta_val);
        double rv = std::atof(rule->value);
        if (!(mv > rv))
        {
            if (out_buf && out_cap > 0)
            {
                int n = std::snprintf(out_buf, out_cap,
                                      "[%s/%s] %s: gt expected >%s got %s",
                                      instr->type ? instr->type : "",
                                      instr->name ? instr->name : "",
                                      rule->key, rule->value, meta_val);
                if (n < 0) out_buf[0] = '\0';
            }
            return -1;
        }
        return 0;
    }

    case BS_META_LT:
    {
        double mv = std::atof(meta_val);
        double rv = std::atof(rule->value);
        if (!(mv < rv))
        {
            if (out_buf && out_cap > 0)
            {
                int n = std::snprintf(out_buf, out_cap,
                                      "[%s/%s] %s: lt expected <%s got %s",
                                      instr->type ? instr->type : "",
                                      instr->name ? instr->name : "",
                                      rule->key, rule->value, meta_val);
                if (n < 0) out_buf[0] = '\0';
            }
            return -1;
        }
        return 0;
    }

    case BS_META_GE:
    {
        double mv = std::atof(meta_val);
        double rv = std::atof(rule->value);
        if (!(mv >= rv))
        {
            if (out_buf && out_cap > 0)
            {
                int n = std::snprintf(out_buf, out_cap,
                                      "[%s/%s] %s: ge expected >=%s got %s",
                                      instr->type ? instr->type : "",
                                      instr->name ? instr->name : "",
                                      rule->key, rule->value, meta_val);
                if (n < 0) out_buf[0] = '\0';
            }
            return -1;
        }
        return 0;
    }

    case BS_META_LE:
    {
        double mv = std::atof(meta_val);
        double rv = std::atof(rule->value);
        if (!(mv <= rv))
        {
            if (out_buf && out_cap > 0)
            {
                int n = std::snprintf(out_buf, out_cap,
                                      "[%s/%s] %s: le expected <=%s got %s",
                                      instr->type ? instr->type : "",
                                      instr->name ? instr->name : "",
                                      rule->key, rule->value, meta_val);
                if (n < 0) out_buf[0] = '\0';
            }
            return -1;
        }
        return 0;
    }

    case BS_META_REGEX:
    {
        RegexHelper rh(rule->value ? rule->value : "");
        if (!rh.valid)
        {
            if (out_buf && out_cap > 0)
            {
                int n = std::snprintf(out_buf, out_cap,
                                      "[%s/%s] %s: invalid regex pattern \"%s\"",
                                      instr->type ? instr->type : "",
                                      instr->name ? instr->name : "",
                                      rule->key, rule->value ? rule->value : "");
                if (n < 0) out_buf[0] = '\0';
            }
            return -1;
        }
        if (rh.exec(meta_val) != 0)
        {
            if (out_buf && out_cap > 0)
            {
                int n = std::snprintf(out_buf, out_cap,
                                      "[%s/%s] %s: regex \"%s\" does not match \"%s\"",
                                      instr->type ? instr->type : "",
                                      instr->name ? instr->name : "",
                                      rule->key, rule->value ? rule->value : "", meta_val);
                if (n < 0) out_buf[0] = '\0';
            }
            return -1;
        }
        return 0;
    }

    case BS_META_CONTAINS:
        if (!str_contains(meta_val, rule->value))
        {
            if (out_buf && out_cap > 0)
            {
                int n = std::snprintf(out_buf, out_cap,
                                      "[%s/%s] %s: expected to contain \"%s\" got \"%s\"",
                                      instr->type ? instr->type : "",
                                      instr->name ? instr->name : "",
                                      rule->key, rule->value ? rule->value : "", meta_val);
                if (n < 0) out_buf[0] = '\0';
            }
            return -1;
        }
        return 0;

    case BS_META_EXISTS:
    case BS_META_NOT_EXISTS:
        /* Already handled above */
        return 0;
    }

    /* Unknown op -- should not reach */
    if (out_buf && out_cap > 0)
    {
        int n = std::snprintf(out_buf, out_cap, "unknown meta op=%d", (int)rule->op);
        if (n < 0) out_buf[0] = '\0';
    }
    return -1;
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

size_t bs_meta_rule_check(const IRInstructionList* instructions,
                          const BsMetaRule* rules, size_t rule_count,
                          char* err, size_t err_cap)
{
    if (!instructions || !rules || rule_count == 0)
        return (size_t)-1;

    size_t n_instr = bs_ir_instruction_list_size(instructions);

    for (size_t ri = 0; ri < rule_count; ++ri)
    {
        const BsMetaRule* rule = &rules[ri];

        /* Iterate all instructions, optionally filtered by instr_name. */
        int found_any = 0;
        for (size_t ii = 0; ii < n_instr; ++ii)
        {
            const IRInstruction* instr = bs_ir_instruction_list_get(instructions, ii);
            if (!instr) continue;

            /* instr_name filter: if rule has a non-empty name, skip non-matching instructions. */
            if (!is_empty(rule->instr_name))
            {
                if (!instr->name || std::strcmp(instr->name, rule->instr_name) != 0)
                    continue;
            }

            found_any = 1;

            if (check_one_rule(instr, rule, err, err_cap) != 0)
                return ri;  /* First failure */
        }

        /* EXISTS rule: if no instruction matched the name filter and key does not exist,
         * that constitutes failure (the key was not found on any matching instruction). */
        if (!found_any && rule->op == BS_META_EXISTS)
        {
            if (err && err_cap > 0)
            {
                int n = std::snprintf(err, err_cap,
                                      "no instruction matching \"%s\" for EXISTS check on key \"%s\"",
                                      rule->instr_name ? rule->instr_name : "",
                                      rule->key ? rule->key : "");
                if (n < 0) err[0] = '\0';
            }
            return ri;
        }

        /* NOT_EXISTS rule: if no instruction matched the name filter, it vacuously passes. */
        /* All other ops with no matching instruction: vacuously pass (nothing to check). */
    }

    return (size_t)-1;
}
