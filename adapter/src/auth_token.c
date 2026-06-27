/* ── auth_token.c ─────────────────────────────────────────────────────
 * C-side authentication: token interception at adapter layer.
 * DAY38-12: C 侧 token 拦截 + 基本格式校验
 * ──────────────────────────────────────────────────────────────────── */

#include "bs/adapter/auth_token.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

BsAuthContext* bs_auth_context_create(void)
{
    BsAuthContext* ctx = (BsAuthContext*)calloc(1, sizeof(BsAuthContext));
    if (!ctx) return NULL;
    ctx->authenticated = 0;
    ctx->expires_at    = 0;
    ctx->error_message[0] = '\0';
    return ctx;
}

void bs_auth_context_destroy(BsAuthContext* ctx)
{
    if (!ctx) return;
    if (ctx->token)     free((void*)ctx->token);
    if (ctx->user_id)   free((void*)ctx->user_id);
    if (ctx->session_id) free((void*)ctx->session_id);
    free(ctx);
}

int bs_auth_token_set(BsAuthContext* ctx, const char* token)
{
    if (!ctx || !token || token[0] == '\0') {
        if (ctx) snprintf(ctx->error_message, sizeof(ctx->error_message), "token 不能为空");
        return -1;
    }

    /* Basic format check: token should be reasonable length */
    size_t len = strlen(token);
    if (len < 8 || len > 512) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "token 长度无效 (%zu 字节，需 8-512)", len);
        if (ctx) ctx->authenticated = 0;
        return -2;
    }

    if (ctx->token) free((void*)ctx->token);
    ctx->token = strdup(token);

    if (!ctx->user_id) ctx->user_id = strdup("default_user");
    if (!ctx->session_id) ctx->session_id = strdup("session_1");

    ctx->authenticated = 1;
    return 0;
}

int bs_auth_token_verify(const BsAuthContext* ctx, const char* expected)
{
    if (!ctx || !ctx->token || !expected) return 0;
    return (strcmp(ctx->token, expected) == 0) ? 1 : 0;
}

int bs_auth_is_valid(const BsAuthContext* ctx)
{
    if (!ctx) return 0;
    if (!ctx->authenticated) return 0;
    if (ctx->expires_at > 0) {
        time_t now = time(NULL);
        if ((uint64_t)now > ctx->expires_at) return 0;
    }
    return 1;
}

int bs_auth_is_expired(const BsAuthContext* ctx)
{
    if (!ctx || ctx->expires_at == 0) return 0;
    time_t now = time(NULL);
    return ((uint64_t)now > ctx->expires_at) ? 1 : 0;
}

void bs_auth_reset(BsAuthContext* ctx)
{
    if (!ctx) return;
    if (ctx->token) { free((void*)ctx->token); ctx->token = NULL; }
    ctx->authenticated = 0;
    ctx->expires_at    = 0;
    ctx->error_message[0] = '\0';
}
