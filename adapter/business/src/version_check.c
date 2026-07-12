#include "bs/adapter/business/version_check.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

/* ─── Semver 版本号解析 ────────────────────────────────────────────── */

typedef struct {
    unsigned int major;
    unsigned int minor;
    unsigned int patch;
} bs_version_t;

static int parse_version(const char* s, bs_version_t* v) {
    if (!s || !v) return -1;
    /* skip whitespace */
    while (*s && isspace((unsigned char)*s)) s++;
    int n;
    unsigned int ma = 0, mi = 0, pa = 0;
    n = sscanf(s, "%u.%u.%u", &ma, &mi, &pa);
    if (n < 1) return -1;
    v->major = ma;
    v->minor = (n >= 2) ? mi : 0;
    v->patch = (n >= 3) ? pa : 0;
    return 0;
}

static int version_cmp(const bs_version_t* a, const bs_version_t* b) {
    if (a->major != b->major) return (a->major < b->major) ? -1 : 1;
    if (a->minor != b->minor) return (a->minor < b->minor) ? -1 : 1;
    if (a->patch != b->patch) return (a->patch < b->patch) ? -1 : 1;
    return 0;
}

/* ─── Range 解析与匹配 ─────────────────────────────────────────────── */

/*
 * 支持的 range 语法:
 *   ">=1.0.0"
 *   ">=1.0.0 <2.0.0"
 *   ">1.0.0"
 *   "<2.0.0"
 *   "=1.0.0"
 *   "1.0.0 - 2.0.0"   (等价于 >=1.0.0 <=2.0.0)
 *   "^1.2.3"           (兼容性范围: >=1.2.3 <2.0.0)
 *   "~1.2.3"           (近似范围: >=1.2.3 <1.3.0)
 */

static int match_ge(const bs_version_t* ver, const bs_version_t* bound) {
    return version_cmp(ver, bound) >= 0;
}
static int match_gt(const bs_version_t* ver, const bs_version_t* bound) {
    return version_cmp(ver, bound) > 0;
}
static int match_le(const bs_version_t* ver, const bs_version_t* bound) {
    return version_cmp(ver, bound) <= 0;
}
static int match_lt(const bs_version_t* ver, const bs_version_t* bound) {
    return version_cmp(ver, bound) < 0;
}
static int match_eq(const bs_version_t* ver, const bs_version_t* bound) {
    return version_cmp(ver, bound) == 0;
}

static const char* skip_spaces(const char* s) {
    while (s && *s && isspace((unsigned char)*s)) s++;
    return s;
}

int bs_version_compatible(const char* sdk_version, const char* required_range) {
    if (!sdk_version || !required_range) return -2;

    bs_version_t ver;
    if (parse_version(sdk_version, &ver) != 0) return -2;

    const char* p = skip_spaces(required_range);
    if (!*p) return -2;

    int result = 1; /* default: compatible (if no constraint at all) */

    while (*p) {
        char op[4] = {0};
        bs_version_t bound;

        /* 读取操作符 */
        if (*p == '>') {
            if (*(p+1) == '=') { op[0] = '>'; op[1] = '='; op[2] = 0; p += 2; }
            else { op[0] = '>'; op[1] = 0; p += 1; }
        } else if (*p == '<') {
            if (*(p+1) == '=') { op[0] = '<'; op[1] = '='; op[2] = 0; p += 2; }
            else { op[0] = '<'; op[1] = 0; p += 1; }
        } else if (*p == '=') {
            op[0] = '='; op[1] = 0; p += 1;
        } else if (*p == '^') {
            op[0] = '^'; op[1] = 0; p += 1;
        } else if (*p == '~') {
            op[0] = '~'; op[1] = 0; p += 1;
        } else if (*p == '-') {
            /* "X - Y" 语法: 需要先解析低版本 */
            /* 回退: 用低版本边界 */
            return -2; /* 暂不支持单独 "-" 起始, 只用 "lo - hi" 双条件 */
        } else {
            /* 可能是显式版本号 (无操作符) 或 "-" 分隔 */
            if (isdigit((unsigned char)*p)) {
                /* 无操作符版本: 尝试解析, 当作 `=` */
                if (parse_version(p, &bound) == 0) {
                    if (!match_eq(&ver, &bound)) return -1;
                    /* 跳过版本号 */
                    while (*p && (isdigit((unsigned char)*p) || *p == '.')) p++;
                    p = skip_spaces(p);
                    continue;
                }
            }
            return -2;
        }

        p = skip_spaces(p);

        /* 读取版本号 */
        if (!isdigit((unsigned char)*p)) {
            /* 可能是 "lo - hi" 中的连字符 */
            if (*p == '-') {
                return -2; /* 暂不支持 */
            }
            return -2;
        }

        if (parse_version(p, &bound) != 0) return -2;

        /* 跳过版本号 */
        while (*p && (isdigit((unsigned char)*p) || *p == '.')) p++;

        /* 应用约束 */
        int match = 0;
        switch (op[0]) {
            case '>': match = (op[1] == '=') ? match_ge(&ver, &bound) : match_gt(&ver, &bound); break;
            case '<': match = (op[1] == '=') ? match_le(&ver, &bound) : match_lt(&ver, &bound); break;
            case '=': match = match_eq(&ver, &bound); break;
            case '^':
                /* ^1.2.3 => >=1.2.3 <2.0.0 */
                if (match_lt(&ver, &bound)) { match = 0; break; }
                bound.major = bound.major + 1;
                bound.minor = 0; bound.patch = 0;
                match = match_lt(&ver, &bound);
                break;
            case '~':
                /* ~1.2.3 => >=1.2.3 <1.3.0 */
                if (match_lt(&ver, &bound)) { match = 0; break; }
                bound.minor = bound.minor + 1;
                bound.patch = 0;
                match = match_lt(&ver, &bound);
                break;
            default: return -2;
        }
        if (!match) return -1;

        p = skip_spaces(p);
    }

    return 0;
}
