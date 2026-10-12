#ifndef XHTTP_POLICY_HTTP_INTERNAL_H
#define XHTTP_POLICY_HTTP_INTERNAL_H

#include "xrt_internal.h"
#include <xrt/http_bridge.h>
#include <xrt/http_auth_policy.h>
#include <xrt/http_client_policy.h>

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION)
/* 验证 Digest 会话内部存储的输出别名。 */
bool __xrtHttpDigestSessionOutputValid(
 const xhttpdigestsession* pSession,
 const xhttpdigestexchange* pExchange,
 const void* pOutput, size_t iSize
);
#endif
#endif
