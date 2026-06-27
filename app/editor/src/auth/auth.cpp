/* ── auth.cpp ─────────────────────────────────────────────────────
 * 第38天 ⑫ · TS 侧 Auth 认证逻辑（C 侧接口转发）
 *
 * 提供 bs_auth_is_valid() / bs_auth_token_verify() 两个函数，
 * 供 Electron addon 通过 napi-rs 绑定调用，实现 token 校验。
 * ───────────────────────────────────────────────────────────────── */

#include "auth.h"

#include <cstring>
#include <ctime>

extern "C" {

/// @brief 验证 auth token 是否有效（格式校验 + 过期检测）
/// @return 1=有效, 0=无效
int bs_auth_token_verify(const char *token) {
  if (!token || token[0] == '\0') return 0;

  // 格式校验：token 必须包含 user / token / expires 三个字段的 JSON
  // 简单校验：检查是否包含 "user"、"token"、"expires"
  const char *has_user = strstr(token, "\"user\"");
  const char *has_tok = strstr(token, "\"token\"");
  const char *has_exp = strstr(token, "\"expires\"");

  if (!has_user || !has_tok || !has_exp) return 0;

  // 过期检测：提取 expires 时间戳
  const char *exp_start = has_exp;
  // 跳过 "expires":
  while (*exp_start && *exp_start != ':') exp_start++;
  if (*exp_start == ':') exp_start++; // 跳过冒号

  // 跳过空白
  while (*exp_start == ' ' || *exp_start == '\t' || *exp_start == '\n')
    exp_start++;

  long long expires = 0;
  int n = 0;
  // 简单数字解析
  while (exp_start[n] >= '0' && exp_start[n] <= '9') {
    expires = expires * 10 + (exp_start[n] - '0');
    n++;
  }
  if (expires == 0) return 0; // 没有有效时间戳

  time_t now = time(nullptr);
  return (now < (time_t)expires) ? 1 : 0;
}

/// @brief 验证 token 并返回是否有效
/// @return 1=有效, 0=无效或未设置
int bs_auth_is_valid(const char *token) {
  return bs_auth_token_verify(token);
}
}
