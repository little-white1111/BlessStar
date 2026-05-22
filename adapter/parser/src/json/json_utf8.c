#include "bs/adapter/parser/json_utf8.h"

int bs_json_utf8_codepoint_valid(unsigned int cp)
{
    if (cp > 0x10FFFFu)
        return 0;
    if (cp >= 0xD800u && cp <= 0xDFFFu)
        return 0;
    return 1;
}

int bs_json_utf8_decode_advance(const char** p, const char* end, unsigned int* out_cp)
{
    if (!p || !*p || !end || *p >= end || !out_cp)
        return 0;

    const unsigned char c0 = (unsigned char)(**p);
    unsigned int        cp = 0;
    size_t              adv = 0;

    if (c0 < 0x80u)
    {
        cp  = c0;
        adv = 1;
    }
    else if ((c0 & 0xE0u) == 0xC0u)
    {
        if ((size_t)(end - *p) < 2)
            return 0;
        const unsigned char c1 = (unsigned char)(*p)[1];
        if ((c1 & 0xC0u) != 0x80u)
            return 0;
        cp = ((unsigned int)(c0 & 0x1Fu) << 6) | (unsigned int)(c1 & 0x3Fu);
        if (cp < 0x80u)
            return 0;
        adv = 2;
    }
    else if ((c0 & 0xF0u) == 0xE0u)
    {
        if ((size_t)(end - *p) < 3)
            return 0;
        const unsigned char c1 = (unsigned char)(*p)[1];
        const unsigned char c2 = (unsigned char)(*p)[2];
        if ((c1 & 0xC0u) != 0x80u || (c2 & 0xC0u) != 0x80u)
            return 0;
        cp = ((unsigned int)(c0 & 0x0Fu) << 12) | ((unsigned int)(c1 & 0x3Fu) << 6) |
             (unsigned int)(c2 & 0x3Fu);
        if (cp < 0x800u)
            return 0;
        adv = 3;
    }
    else if ((c0 & 0xF8u) == 0xF0u)
    {
        if ((size_t)(end - *p) < 4)
            return 0;
        const unsigned char c1 = (unsigned char)(*p)[1];
        const unsigned char c2 = (unsigned char)(*p)[2];
        const unsigned char c3 = (unsigned char)(*p)[3];
        if ((c1 & 0xC0u) != 0x80u || (c2 & 0xC0u) != 0x80u || (c3 & 0xC0u) != 0x80u)
            return 0;
        cp = ((unsigned int)(c0 & 0x07u) << 18) | ((unsigned int)(c1 & 0x3Fu) << 12) |
             ((unsigned int)(c2 & 0x3Fu) << 6) | (unsigned int)(c3 & 0x3Fu);
        if (cp < 0x10000u)
            return 0;
        adv = 4;
    }
    else
        return 0;

    if (!bs_json_utf8_codepoint_valid(cp))
        return 0;

    *out_cp = cp;
    *p += adv;
    return 1;
}
