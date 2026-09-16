/* xoauth2 内部头。 */
#ifndef XOAUTH2_INTERNAL_H
#define XOAUTH2_INTERNAL_H

#include "../xoauth2.h"
#include "../xoauth2-xrt.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* 工具                                                                 */
/* ------------------------------------------------------------------ */
void xoauth2__error(int iCode, const char* sMessage);

/* isalnum URL-safe 字符或 -._~ */
bool xoauth2__url_safe(char c);

/* Base64URL 编码（无 padding） */
char* xoauth2__base64url(const void* pData, size_t iSize);

/* 释放预设持有的端点 URL 并清指针 */
void xoauth2__client_clear_owned(xoauth2client* pClient);

/* ------------------------------------------------------------------ */
/* 请求构造 / 响应解析（flow.c）                                         */
/* ------------------------------------------------------------------ */

/* token 响应 JSON → xoauth2token（错误字段优先） */
xoauth2token* xoauth2__parse_token_response(const char* sJson, size_t iSize);

/* authorization_code 交换请求体（xrtFree；含 AuthStyle 处理） */
char* xoauth2__build_token_request(xoauth2client* pClient, const char* sCode);

/* refresh_token 请求体（xrtFree） */
char* xoauth2__build_refresh_request(const xoauth2client* pClient,
                                     const char* sRefreshToken);

/* AuthStyle=BASIC 时返回 Authorization 头值 "Basic xxx"（xrtFree）；
 * 其他风格返回 NULL。id/secret 先按 RFC 6749 §2.3.1 URL 编码再拼接。 */
char* xoauth2__build_auth_header(const xoauth2client* pClient);

#endif
