/* ── auth_token.h ─────────────────────────────────────────────────────
 * C-side authentication module header.
 * Intercepts tokens at the adapter layer via BsAuthContext.
 * DAY38-12: C 侧 token 拦截 + TS 侧 bcrypt 密码认证
 * ──────────────────────────────────────────────────────────────────── */

#ifndef BS_AUTH_TOKEN_H
#define BS_AUTH_TOKEN_H

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
extern "C" {
#else
#include <stddef.h>
#include <stdint.h>
#endif

/* ── Auth context (token-level interceptor) ────────────────────────── */
typedef struct BsAuthContext {
    const char* token;
    const char* user_id;
    const char* session_id;
    uint64_t    expires_at;   /* unix timestamp, 0 = no expiry */
    int         authenticated;
    char        error_message[256];
} BsAuthContext;

/* ── API ───────────────────────────────────────────────────────────── */

/** Create an empty auth context */
BsAuthContext* bs_auth_context_create(void);

/** Destroy an auth context */
void bs_auth_context_destroy(BsAuthContext* ctx);

/** Set the session token and validate basic format */
int bs_auth_token_set(BsAuthContext* ctx, const char* token);

/** Verify a token against a stored hash (simple equality for MVP) */
int bs_auth_token_verify(const BsAuthContext* ctx, const char* expected);

/** Check if the auth context is authenticated and not expired */
int bs_auth_is_valid(const BsAuthContext* ctx);

/** Check if token has expired */
int bs_auth_is_expired(const BsAuthContext* ctx);

/** Clear the auth context (logout) */
void bs_auth_reset(BsAuthContext* ctx);

#ifdef __cplusplus
}
#endif

#endif /* BS_AUTH_TOKEN_H */
