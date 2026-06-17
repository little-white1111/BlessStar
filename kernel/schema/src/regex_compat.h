#ifndef BS_KERNEL_SCHEMA_REGEX_COMPAT_H
#define BS_KERNEL_SCHEMA_REGEX_COMPAT_H

/*
 * C++ wrapper around std::regex for cross-platform compatibility.
 */

#include <regex>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle (actually a std::regex*) */
typedef void* regex_t;
typedef size_t regmatch_t;

#define REG_EXTENDED 1
#define REG_NOSUB    2
#define REG_NOMATCH  1

static inline int regcomp(regex_t* preg, const char* pattern, int cflags)
{
    (void)cflags;
    try
    {
        *preg = new std::regex(pattern,
            std::regex::ECMAScript | std::regex::optimize);
        return 0;
    }
    catch (...)
    {
        *preg = nullptr;
        return -1;
    }
}

static inline int regexec(const regex_t* preg, const char* string,
                           size_t nmatch, regmatch_t* pmatch, int eflags)
{
    (void)nmatch;
    (void)pmatch;
    (void)eflags;
    if (!preg || !*preg) return REG_NOMATCH;
    const std::regex* re = static_cast<const std::regex*>(*preg);
    return std::regex_search(string, *re) ? 0 : REG_NOMATCH;
}

static inline void regfree(regex_t* preg)
{
    if (preg && *preg)
    {
        delete static_cast<std::regex*>(*preg);
        *preg = nullptr;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* BS_KERNEL_SCHEMA_REGEX_COMPAT_H */
