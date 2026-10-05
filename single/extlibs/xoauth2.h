/*
 * MIT License
 *
 * Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */
/* Supply XRT and selected extension dependencies before this header. */
#if !defined(XRT_CORE_H)
#error "xoauth2 requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XOAUTH2_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XOAUTH2_IMPLEMENTATION) && \
	!defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
		#define _DARWIN_C_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif
#ifndef XOAUTH2_SINGLE_HEADER_H
#define XOAUTH2_SINGLE_HEADER_H
#define XOAUTH2_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xoauth2/include/xoauth2/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XOAUTH2_FEATURES_H
#define XOAUTH2_FEATURES_H

/* xoauth2 及其直接依赖。 */
#if defined(XOAUTH2_MODULE_ALL) || defined(XOAUTH2_MODULE_XOAUTH2)
#ifndef XOAUTH2_FEATURE_XOAUTH2
#define XOAUTH2_FEATURE_XOAUTH2
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#ifndef XRT_MODULE_RANDOM
#define XRT_MODULE_RANDOM
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#ifndef XRT_MODULE_JSON_READ
#define XRT_MODULE_JSON_READ
#endif
#ifndef XRT_MODULE_JSON_WRITE
#define XRT_MODULE_JSON_WRITE
#endif
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#ifndef XRT_MODULE_CRYPTO_CORE
#define XRT_MODULE_CRYPTO_CORE
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_NET_ENGINE
#define XRT_MODULE_NET_ENGINE
#endif
#ifndef XRT_MODULE_NET_RESOLVER
#define XRT_MODULE_NET_RESOLVER
#endif
#ifndef XRT_MODULE_NET_TCP_DIAL_SYNC
#define XRT_MODULE_NET_TCP_DIAL_SYNC
#endif
#ifndef XRT_MODULE_NET_TCP_FUTURE
#define XRT_MODULE_NET_TCP_FUTURE
#endif
#ifndef XRT_MODULE_TLS_STREAM_DIAL_FUTURE
#define XRT_MODULE_TLS_STREAM_DIAL_FUTURE
#endif
#ifndef XRT_MODULE_TLS_STREAM_FUTURE
#define XRT_MODULE_TLS_STREAM_FUTURE
#endif
#ifndef XRT_MODULE_TLS_CLIENT_VERIFY
#define XRT_MODULE_TLS_CLIENT_VERIFY
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE_SHA256
#define XRT_MODULE_TLS_SCHEDULE_SHA256
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE_SHA384
#define XRT_MODULE_TLS_SCHEDULE_SHA384
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE_X25519
#define XRT_MODULE_TLS_KEY_EXCHANGE_X25519
#endif
#ifndef XRT_MODULE_TLS_RECORD_AES
#define XRT_MODULE_TLS_RECORD_AES
#endif
#ifndef XRT_MODULE_HTTP1_BODY
#define XRT_MODULE_HTTP1_BODY
#endif
#ifndef XRT_MODULE_X509_STORE_SYSTEM
#define XRT_MODULE_X509_STORE_SYSTEM
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#endif

#endif /* XOAUTH2_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xoauth2/include/xoauth2/api.h */
/* ========================================================================== */

/*
	xoauth2 —— OAuth 2.0 (RFC 6749) 客户端扩展库。

	授权码流程 + PKCE (RFC 7636) + token 刷新 + provider 预设。
	ID token 验签由应用层组合 xjwt 完成（见 examples/oidc_login.c）。

	用法（GitHub 登录三步）：
		xoauth2client oauth = {0};
		xoauth2UseGithub(&oauth, id, secret, redirect);
		char* url = xoauth2BeginLogin(&oauth);          // → 重定向
		xoauth2token* tok = xoauth2CompleteLogin(&oauth, code, state);

	依赖闭包由 config/modules.json 声明；src/ 中各文件独立编译。
*/
#ifndef XOAUTH2_API_H
#define XOAUTH2_API_H


#if defined(XOAUTH2_FEATURE_XOAUTH2)

/* The selected product requires its complete declared dependency set. */
#if !defined(XRT_FEATURE_ATOMIC)
#error "xoauth2 requires atomic (XRT_FEATURE_ATOMIC)"
#endif
#if !defined(XRT_FEATURE_THREAD)
#error "xoauth2 requires thread (XRT_FEATURE_THREAD)"
#endif
#if !defined(XRT_FEATURE_STRING)
#error "xoauth2 requires string (XRT_FEATURE_STRING)"
#endif
#if !defined(XRT_FEATURE_TIME)
#error "xoauth2 requires time (XRT_FEATURE_TIME)"
#endif
#if !defined(XRT_FEATURE_RANDOM)
#error "xoauth2 requires random (XRT_FEATURE_RANDOM)"
#endif
#if !defined(XRT_FEATURE_RANDOM_SECURE)
#error "xoauth2 requires random_secure (XRT_FEATURE_RANDOM_SECURE)"
#endif
#if !defined(XRT_FEATURE_JSON_READ)
#error "xoauth2 requires json_read (XRT_FEATURE_JSON_READ)"
#endif
#if !defined(XRT_FEATURE_JSON_WRITE)
#error "xoauth2 requires json_write (XRT_FEATURE_JSON_WRITE)"
#endif
#if !defined(XRT_FEATURE_VALUE)
#error "xoauth2 requires value (XRT_FEATURE_VALUE)"
#endif
#if !defined(XRT_FEATURE_CODEC_BASE64)
#error "xoauth2 requires codec_base64 (XRT_FEATURE_CODEC_BASE64)"
#endif
#if !defined(XRT_FEATURE_CRYPTO_CORE)
#error "xoauth2 requires crypto_core (XRT_FEATURE_CRYPTO_CORE)"
#endif
#if !defined(XRT_FEATURE_CRYPTO_SHA256)
#error "xoauth2 requires crypto_sha256 (XRT_FEATURE_CRYPTO_SHA256)"
#endif
#if !defined(XRT_FEATURE_BUFFER)
#error "xoauth2 requires buffer (XRT_FEATURE_BUFFER)"
#endif
#if !defined(XRT_FEATURE_NET_ENGINE)
#error "xoauth2 requires net_engine (XRT_FEATURE_NET_ENGINE)"
#endif
#if !defined(XRT_FEATURE_NET_RESOLVER)
#error "xoauth2 requires net_resolver (XRT_FEATURE_NET_RESOLVER)"
#endif
#if !defined(XRT_FEATURE_NET_TCP_DIAL_SYNC)
#error "xoauth2 requires net_tcp_dial_sync (XRT_FEATURE_NET_TCP_DIAL_SYNC)"
#endif
#if !defined(XRT_FEATURE_NET_TCP_FUTURE)
#error "xoauth2 requires net_tcp_future (XRT_FEATURE_NET_TCP_FUTURE)"
#endif
#if !defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)
#error "xoauth2 requires tls_stream_dial_future (XRT_FEATURE_TLS_STREAM_DIAL_FUTURE)"
#endif
#if !defined(XRT_FEATURE_TLS_STREAM_FUTURE)
#error "xoauth2 requires tls_stream_future (XRT_FEATURE_TLS_STREAM_FUTURE)"
#endif
#if !defined(XRT_FEATURE_TLS_CLIENT_VERIFY)
#error "xoauth2 requires tls_client_verify (XRT_FEATURE_TLS_CLIENT_VERIFY)"
#endif
#if !defined(XRT_FEATURE_TLS_SCHEDULE_SHA256)
#error "xoauth2 requires tls_schedule_sha256 (XRT_FEATURE_TLS_SCHEDULE_SHA256)"
#endif
#if !defined(XRT_FEATURE_TLS_SCHEDULE_SHA384)
#error "xoauth2 requires tls_schedule_sha384 (XRT_FEATURE_TLS_SCHEDULE_SHA384)"
#endif
#if !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519)
#error "xoauth2 requires tls_key_exchange_x25519 (XRT_FEATURE_TLS_KEY_EXCHANGE_X25519)"
#endif
#if !defined(XRT_FEATURE_TLS_RECORD_AES)
#error "xoauth2 requires tls_record_aes (XRT_FEATURE_TLS_RECORD_AES)"
#endif
#if !defined(XRT_FEATURE_HTTP1_BODY)
#error "xoauth2 requires http1_body (XRT_FEATURE_HTTP1_BODY)"
#endif
#if !defined(XRT_FEATURE_X509_STORE_SYSTEM)
#error "xoauth2 requires x509_store_system (XRT_FEATURE_X509_STORE_SYSTEM)"
#endif
#if !defined(XRT_FEATURE_FUTURE)
#error "xoauth2 requires future (XRT_FEATURE_FUTURE)"
#endif


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* xrt 核心 JSON 值（前向声明，实现经 xrt.h） */
struct xvalue;
typedef struct xvalue xvalue;

#ifdef __cplusplus
extern "C" {
#endif

#define XOAUTH2_VERSION_MAJOR 2
#define XOAUTH2_VERSION_MINOR 0
#define XOAUTH2_VERSION_PATCH 0

/* ------------------------------------------------------------------
 * 错误码（xerror 域 "xrt.oauth2"）
 * ------------------------------------------------------------------ */
enum {
	XOAUTH2_ERROR_ARGUMENT       = 1,
	XOAUTH2_ERROR_STATE_MISMATCH = 2,   /* CSRF state 不匹配 */
	XOAUTH2_ERROR_TOKEN_ENDPOINT = 3,   /* token 端点 HTTP 失败 */
	XOAUTH2_ERROR_TOKEN_RESPONSE = 4,   /* 响应解析失败 */
	XOAUTH2_ERROR_TOKEN_DENIED   = 5,   /* provider 返回 error */
	XOAUTH2_ERROR_REFRESH        = 6,   /* 刷新失败 */
	XOAUTH2_ERROR_NETWORK        = 7,   /* TLS/TCP 连接失败 */
	XOAUTH2_ERROR_NONCE_MISMATCH = 8,   /* OIDC nonce 不匹配（重放） */
};

/* ------------------------------------------------------------------
 * 客户端认证风格（RFC 6749 §2.3）
 * ------------------------------------------------------------------ */
enum {
	XOAUTH2_AUTH_AUTO  = 0,   /* 同 BODY（公共客户端无 secret 时仅 client_id） */
	XOAUTH2_AUTH_BODY  = 1,   /* client_id/client_secret 放请求体 */
	XOAUTH2_AUTH_BASIC = 2,   /* HTTP Basic 头（id:secret 经标准 Base64） */
};

/* ------------------------------------------------------------------
 * 客户端配置
 * ------------------------------------------------------------------ */

/*
	传输回调：宿主提供 HTTP 能力（通用 token 交换 POST、微信 GET、JWKS/userinfo GET
	都经它）。请求 = method（"POST"/"GET"）+ url + form 编码 body
	（GET 时为 NULL）+ 可选 Authorization 头（AuthStyle=BASIC 与
	Bearer 场景非 NULL）。
	成功时把响应体写入 *psBody（xrtMalloc）、状态码写入 *piStatus 并返回 true。
	只要 *psBody 非 NULL，库在返回值为 true 或 false 时都接管所有权；内部处理
	结束后用 xrtFree 释放，xoauth2HttpGet 成功返回时则转交其调用方释放。
	回调不可交付静态/栈内存。网络层失败（连接/超时/TLS）返回
	false（*piStatus 置 0）。测试可用 mock 注入。
*/
typedef bool (*xoauth2httpproc)(const char* sMethod, const char* sUrl,
                                const char* sBody, const char* sAuthHeader,
                                char** psResponseBody, int* piStatus,
                                void* pContext);

typedef struct xoauth2config {
	const char* AuthorizeUrl;    /* 授权端点 */
	const char* TokenUrl;        /* token 端点 */
	const char* UserInfoUrl;     /* 可选 userinfo 端点 */
	const char* JwksUrl;         /* 可选 OIDC JWKS 端点（应用层经 xoauth2HttpGet 拉取） */
	const char* Issuer;          /* 可选 OIDC issuer（应用层喂给 xjwtcheck.Issuer） */
	const char* ClientId;
	const char* ClientSecret;    /* 公共客户端（PKCE only）可 NULL */
	const char* RedirectUri;
	const char* Scope;           /* 空格分隔 */
	bool        UsePkce;          /* 默认 true；预设会按 provider 支持度设置 */
	bool        UseNonce;         /* OIDC：BeginLogin 生成 nonce 参数（默认 false） */
	int         AuthStyle;        /* XOAUTH2_AUTH_* */
	xoauth2httpproc Http;         /* 传输回调；NULL = 未配置网络（换 token 返回 NETWORK 错误） */
	void*       HttpContext;      /* 透传给回调 */
} xoauth2config;

XRT_API void xoauth2ConfigInit(xoauth2config* pConfig);

/* ------------------------------------------------------------------
 * 客户端（含运行时会话状态：state + PKCE verifier + nonce）
 * 首次调用任何 Use* 预设前，必须以 `xoauth2client client = {0};` 初始化。
 * Microsoft 预设会按 tenant 动态生成端点 URL 与 issuer，由客户端持有
 * 所有权，重复预设与 xoauth2ClientUnit 时释放。
 * ------------------------------------------------------------------ */
typedef struct xoauth2client {
	xoauth2config Config;
	char sState[128];           /* 当前登录会话的 state（CSRF 防护） */
	char sVerifier[128];        /* PKCE code_verifier */
	char sChallenge[128];       /* PKCE code_challenge (S256) */
	char sNonce[128];           /* OIDC nonce（UseNonce 时生成） */
	char* pOwnedAuthUrl;        /* 预设分配的 AuthorizeUrl（可 NULL） */
	char* pOwnedTokenUrl;       /* 预设分配的 TokenUrl（可 NULL） */
	char* pOwnedIssuerUrl;      /* 预设分配的 Issuer（可 NULL） */
	bool Wechat;                /* 微信端点使用专有 GET 参数与响应格式 */
} xoauth2client;

/* Provider 预设（一行初始化）。tenant 超 256 字符时 Microsoft 预设
 * 置错误并让端点为 NULL。 */
XRT_API void xoauth2UseGithub(xoauth2client* pClient, const char* id, const char* secret, const char* redirect);
XRT_API void xoauth2UseGoogle(xoauth2client* pClient, const char* id, const char* secret, const char* redirect);
XRT_API void xoauth2UseWechat(xoauth2client* pClient, const char* appid, const char* appSecret, const char* redirect);
XRT_API void xoauth2UseMicrosoft(xoauth2client* pClient, const char* id, const char* secret, const char* redirect, const char* tenant);
XRT_API void xoauth2UseCustom(xoauth2client* pClient, const xoauth2config* pConfig);

/* 会话终止/客户端重置：清零 state/verifier（敏感）并释放预设持有的
 * 端点 URL。调用后如需复用客户端，须重新执行 Use* 预设。 */
XRT_API void xoauth2ClientUnit(xoauth2client* pClient);

/* ------------------------------------------------------------------
 * 登录流程
 * ------------------------------------------------------------------ */

/*
	生成授权 URL（含 state + PKCE）。
	内部自动：生成随机 state、生成 PKCE verifier/challenge (S256)、
	拼 provider 特有参数。UsePkce=false 时跳过 PKCE（预设按 provider
	支持度决定）。
	返回 URL 字符串（xrtFree 释放）；调用方将用户浏览器重定向到该 URL。
*/
XRT_API char* xoauth2BeginLogin(xoauth2client* pClient);

/*
	回调处理：用授权码换 token。
	内部自动：验 state（常时比较，防 CSRF）→ 焚毁 state/verifier
	（一次性，防重放）→ 构造 token 请求（通用端点按 AuthStyle 使用
	Basic 头或 body，微信端点使用 GET 查询参数）→ 网络交换 → 解析响应 JSON。
	成功返回 token（xoauth2TokenFree 释放），失败返回 NULL 并设 xerror。
	state 校验通过即消费会话；请求构造失败也清除 verifier/challenge。
	保留分配与传输回调的原始 xerror；无回调诊断时补 NETWORK。
	不会自动重发授权码；失败后必须重新 BeginLogin。
*/
typedef struct xoauth2token {
	/* 响应字符串字段拒绝解码后的嵌入 NUL，不交付截断的凭据或元数据。 */
	char* AccessToken;
	char* RefreshToken;         /* 可能为 NULL */
	char* IdToken;              /* 可能为 NULL（OIDC） */
	char* OpenId;               /* 微信响应的 openid；其他 provider 可为空 */
	char* TokenType;            /* 通常 "bearer"（统一小写存储） */
	int64_t ExpiresIn;            /* 秒 */
	char* Scope;                /* 实际授权的 scope */
	int64_t ObtainedAt;           /* 获取时刻（Unix epoch 秒） */
	int64_t ExpiresAt;            /* ObtainedAt + ExpiresIn */
} xoauth2token;

XRT_API xoauth2token* xoauth2CompleteLogin(xoauth2client* pClient,
                                   const char* sCode, const char* sState);
XRT_API void xoauth2TokenFree(xoauth2token* pToken);

/* ------------------------------------------------------------------
 * Token 管理
 * ------------------------------------------------------------------ */

/* 刷新 token；成功返回新 token，失败 NULL。旧 token 由调用方释放。 */
XRT_API xoauth2token* xoauth2Refresh(xoauth2client* pClient, const char* sRefreshToken);

/* 判断 token 是否将在 leewaySeconds 秒内过期（按 ExpiresAt 与当前时钟）。
 * provider 未返回 expires_in（ExpiresIn==0，如 GitHub）时视为不过期：
 * 未知有效期 ≠ 即将过期；此类 token 通常也无 refresh_token，
 * 调用方应按 provider 的实际有效期策略自行处理。 */
XRT_API bool xoauth2TokenExpiring(const xoauth2token* pToken, int leewaySeconds);

/* ------------------------------------------------------------------ */
/* OIDC / 通用 HTTP 辅助（数据耦合：xoauth2 不依赖 xjwt）               */
/* ------------------------------------------------------------------ */

/*
	经客户端配置的传输回调执行 GET，返回响应体文本（xrtFree），
	*piStatus 收状态码。sAuthHeader 可 NULL（如 Bearer 头）。
	典型用途：拉取 JWKS JSON 后由应用层喂给 xjwtJwksParse。
	失败分级同 token 交换：NETWORK / TOKEN_ENDPOINT（非 2xx）/
	TOKEN_RESPONSE（2xx 空 body）。
*/
XRT_API char* xoauth2HttpGet(xoauth2client* pClient, const char* sUrl,
                     const char* sAuthHeader, int* piStatus);

/*
	OIDC nonce 消费：与 BeginLogin 生成的 nonce 常时比对，
	匹配即焚毁（一次性，防重放）并返回 true；不匹配返回 false 并设
	XOAUTH2_ERROR_NONCE_MISMATCH。典型流程：xjwtClaimString 从
	id_token claims 取出 nonce 后交给本函数。
*/
XRT_API bool xoauth2NonceConsume(xoauth2client* pClient, const char* sNonce);

/*
	GET UserInfoUrl + Authorization: Bearer <accessToken>，
	返回 claims 对象（xrtValueRelease；非对象 JSON 拒绝），失败 NULL。
	适用于 Bearer Authorization 头的 provider。微信 /sns/userinfo 需同时
	传 access_token 和 openid 查询参数，使用 xoauth2GetWechatUserInfo。
*/
XRT_API xvalue* xoauth2GetUserInfo(xoauth2client* pClient, const char* sAccessToken);

/* 微信专用 userinfo；pToken 必须来自微信授权或刷新响应且含 OpenId。 */
XRT_API xvalue* xoauth2GetWechatUserInfo(xoauth2client* pClient,
	const xoauth2token* pToken);

/* ------------------------------------------------------------------
 * 工具（离线可用，便于测试）
 * ------------------------------------------------------------------ */

/* 生成 PKCE code_verifier（随机 43 字符）+ challenge (S256 Base64URL) */
XRT_API bool xoauth2PkceGenerate(char* sVerifier, size_t iVerifierSize,
                         char* sChallenge, size_t iChallengeSize);

/* 生成随机 state（URL-safe 字符串） */
XRT_API bool xoauth2StateGenerate(char* sState, size_t iStateSize);

/* URL 百分号编码 */
XRT_API char* xoauth2UrlEncode(const char* sText);

/* ------------------------------------------------------------------
 * 错误便捷读取：返回当前执行上下文中 xoauth2 域错误码
 * （XOAUTH2_ERROR_*），当前错误不属于 xoauth2 时返回 0。
 * 语义：仅在 xoauth2 调用失败后立即读取；成功调用不保证清除旧错误。
 * ------------------------------------------------------------------ */
XRT_API int xoauth2LastError(void);

/* ------------------------------------------------------------------
 * 便捷传输：直连 xrt net/tls/http1（支持 http:// 与 https://）。
 * 用作 xoauth2httpproc 回调（HttpContext 指向本结构）。
 * ------------------------------------------------------------------ */
typedef struct xoauth2httpxrt xoauth2httpxrt;

/* 零值可用；pBorrowedEngine 借用宿主 net engine（NULL 则自建），
 * sCaPem 为 NULL 时用系统证书库，uTimeoutUs 为 0 时默认 15s。
 * 此超时分别约束连接、整次发送及完整响应，不会逐分片重置。
 * Init 仅用于首次或已成功清理的外壳；失败回滚至少有 30 秒预算，与请求
 * 时限分开。失败后仍须清理外壳，清理未完成时只能重试 Cleanup/Unit/Destroy。
 * 未交付的堆构造由 CleanupPending 清理；有待清理对象时新建私有引擎
 * 会先非阻塞轮询，仍未完成则拒绝，借用引擎不受该限制。 */
XRT_API bool xoauth2HttpXrtInit(xoauth2httpxrt* pHttp, void* pBorrowedEngine,
                        const char* sCaPem, uint64_t uTimeoutUs);
XRT_API void xoauth2HttpXrtUnit(xoauth2httpxrt* pHttp);

/* 与请求串行调用；等待自建 engine 的异步 Close/Abort，最多 uTimeoutUs。
 * 成功释放内部资源（不释放句柄），重复调用或 NULL 均成功；不停止借用 engine。
 * 失败保留自建 engine 的拥有权，句柄仅可用于再次 Cleanup/Unit/Destroy。
 * 保留调用前的非空错误；需判断清理结果时使用此返回值。 */
XRT_API bool xoauth2HttpXrtCleanup(xoauth2httpxrt* pHttp);

/* 堆版本（opaque 结构无法栈上声明时用——脚本层/绑定层的正路；
 * Destroy 清理成功才释放整个句柄；失败保留句柄供重试。
 * 需要确认成功时先调用 Cleanup，成功后再 Destroy。参数语义与 Init 相同。 */
XRT_API xoauth2httpxrt* xoauth2HttpXrtCreate(void* pBorrowedEngine,
                                     const char* sCaPem, uint64_t uTimeoutUs);
XRT_API void            xoauth2HttpXrtDestroy(xoauth2httpxrt* pHttp);

/* 重试未交付失败堆构造的引擎退休；并发可用，入列不分配且不启动后台线程。
 * 宿主停止新调用、等待在途调用结束并清理已交付实例后，在退出/卸载前
 * 调用至 true；false 时保留库及运行环境并稍后重试。
 * uTimeoutUs == 0 为一次非阻塞轮询；非零为等待预算，退休 ERROR 提前结束。
 * true 表示队列及其他清理调用正在处理的对象全部释放；piPending 可空，
 * 非空时返回尚未完成数量。保留已有错误；无旧错误时报告退休错误或 XERR_TIMEOUT，
 * 非阻塞 BUSY 不制造错误。已交付句柄仍由 Cleanup/Unit/Destroy 清理。 */
XRT_API bool xoauth2HttpXrtCleanupPending(uint64_t uTimeoutUs, size_t* piPending);

/* 交付最多 1 MiB 的 C 字符串正文（拒绝原始 NUL），调用方用 xrtFree 释放。
 * 响应头与 trailer 各限 100 字段；HTTPS 关闭定界正文须认证 close_notify。
 * 参数有效时，失败清空响应体指针与状态码；不返回部分正文。
 * 保留异步生产过程的 MEMORY 原因，其他传输失败归类 NETWORK；清理
 * 不覆盖首个错误。超时请求取消在途任务并中止已取得的连接，不自动重发。
 * POST 失败可能已被服务端处理，宿主须按协议处理未知结果。 */
XRT_API bool xoauth2HttpXrt(const char* sMethod, const char* sUrl, const char* sBody,
                    const char* sAuthHeader,
                    char** psResponseBody, int* piStatus, void* pContext);

#ifdef __cplusplus
}
#endif

#endif /* selected xoauth2 */

#endif


/* ========================================================================== */
/* public: extlibs/xoauth2/include/xoauth2.h */
/* ========================================================================== */

#ifndef XOAUTH2_H
#define XOAUTH2_H


#endif

#endif

#if defined(XOAUTH2_IMPLEMENTATION) && !defined(XOAUTH2_IMPLEMENTATION_ONCE)
#define XOAUTH2_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xoauth2/src/internal/xoauth2_internal.h */
/* ========================================================================== */

#if defined(XOAUTH2_FEATURE_XOAUTH2)
/* xoauth2 内部头。 */
#ifndef XOAUTH2_INTERNAL_H
#define XOAUTH2_INTERNAL_H


struct xoauth2httpxrt {
	xnetengine* pEngine;      /* 借用或自建 */
	bool        bEngineOwned;
	xnetresolver* pResolver;
	void*       pVerifier;    /* xtlsverifier*（ opaque 存放，Unit 释放） */
	uint64_t    uTimeoutUs;
	struct xoauth2httpxrt* pPendingNext; /* 只用于未交付的失败堆构造。 */
};


typedef struct xoauth2url {
	char   sHost[256];
	uint16_t iPort;
	char   sPath[1024];
	bool   bTls;
	bool   bIpLiteral;
} xoauth2url;

void xoauth2__url_tls_names(const xoauth2url* pUrl,
	xtlsclientconfig* pTls, xtlsdialconfig* pDial);

bool xoauth2__url_parse(const char* sUrl, xoauth2url* pOut);
bool xoauth2__future_wait(xfuture* pFuture, xdeadline Deadline);

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
#endif


/* ========================================================================== */
/* source: extlibs/xoauth2/src/oauth2/xoauth2_core.c */
/* ========================================================================== */

#if defined(XOAUTH2_FEATURE_XOAUTH2)
/* xoauth2 核心：PKCE + state + URL 构造 + provider 预设 + 错误便捷读取。 */

/* ------------------------------------------------------------------ */
/* 错误                                                                 */
/* ------------------------------------------------------------------ */
void xoauth2__error(int iCode, const char* sMessage)
{
	xrtSetErrorInfo(XERR_STATE, "xrt.oauth2", iCode, sMessage);
}

int xoauth2LastError(void)
{
	const xerror* pErr = xrtGetError();
	if ( pErr == NULL ) return 0;
	cstr sDomain = xrtErrorDomain(pErr);
	if ( sDomain == NULL || strcmp(sDomain, "xrt.oauth2") != 0 ) return 0;
	return xrtErrorCode(pErr);
}

/* ------------------------------------------------------------------ */
/* Base64URL（PKCE challenge 需要）                                     */
/* ------------------------------------------------------------------ */
static const char s_B64url[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

char* xoauth2__base64url(const void* pData, size_t iSize)
{
	const unsigned char* p = (const unsigned char*)pData;
	size_t iOut = (iSize + 2) / 3 * 4;
	if ( iSize % 3 == 1 ) iOut -= 2;
	else if ( iSize % 3 == 2 ) iOut -= 1;
	char* s = (char*)xrtMalloc(iOut + 1);
	if ( s == NULL ) return NULL;
	size_t j = 0;
	for ( size_t i = 0; i < iSize; i += 3 ) {
		uint32_t v = (uint32_t)p[i] << 16;
		if ( i+1 < iSize ) v |= (uint32_t)p[i+1] << 8;
		if ( i+2 < iSize ) v |= p[i+2];
		s[j++] = s_B64url[(v>>18)&63];
		s[j++] = s_B64url[(v>>12)&63];
		if ( i+1 < iSize ) s[j++] = s_B64url[(v>>6)&63];
		if ( i+2 < iSize ) s[j++] = s_B64url[v&63];
	}
	s[j] = 0;
	return s;
}

/* ------------------------------------------------------------------ */
/* PKCE (RFC 7636)                                                      */
/* ------------------------------------------------------------------ */

bool xoauth2PkceGenerate(char* sVerifier, size_t iVerifierSize,
                         char* sChallenge, size_t iChallengeSize)
{
	if ( sVerifier == NULL || iVerifierSize < 64 ||
	     sChallenge == NULL || iChallengeSize < 64 ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "PKCE buffers too small");
		return false;
	}
	/* code_verifier = 32 字节随机 → Base64URL（43 字符） */
	unsigned char aRandom[32];
	if ( !xrtSecureRandom(aRandom, 32) ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "secure random failed");
		return false;
	}
	char* sB64 = xoauth2__base64url(aRandom, 32);
	if ( sB64 == NULL ) return false;
	strncpy(sVerifier, sB64, iVerifierSize - 1);
	sVerifier[iVerifierSize - 1] = 0;
	xrtFree(sB64);

	/* code_challenge = BASE64URL(SHA256(code_verifier)) */
	unsigned char aDigest[32];
	if ( !xrtSha256(sVerifier, strlen(sVerifier), aDigest) ) return false;
	sB64 = xoauth2__base64url(aDigest, 32);
	if ( sB64 == NULL ) return false;
	strncpy(sChallenge, sB64, iChallengeSize - 1);
	sChallenge[iChallengeSize - 1] = 0;
	xrtFree(sB64);
	return true;
}

/* ------------------------------------------------------------------ */
/* State                                                                */
/* ------------------------------------------------------------------ */

bool xoauth2StateGenerate(char* sState, size_t iStateSize)
{
	if ( sState == NULL || iStateSize < 32 ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "state buffer too small");
		return false;
	}
	unsigned char aRandom[24];
	if ( !xrtSecureRandom(aRandom, 24) ) return false;
	char* sB64 = xoauth2__base64url(aRandom, 24);
	if ( sB64 == NULL ) return false;
	strncpy(sState, sB64, iStateSize - 1);
	sState[iStateSize - 1] = 0;
	xrtFree(sB64);
	return true;
}

/* ------------------------------------------------------------------ */
/* URL 编码                                                             */
/* ------------------------------------------------------------------ */

bool xoauth2__url_safe(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
	       (c >= '0' && c <= '9') || c == '-' || c == '.' ||
	       c == '_' || c == '~';
}

char* xoauth2UrlEncode(const char* sText)
{
	if ( sText == NULL ) return NULL;
	size_t n = strlen(sText);
	/* 最坏情况：每字符 3 字节 */
	if ( n > ((size_t)-1 - 1u) / 3u ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "URL encoding input too long");
		return NULL;
	}
	char* sOut = (char*)xrtMalloc(n * 3 + 1);
	if ( sOut == NULL ) return NULL;
	size_t j = 0;
	for ( size_t i = 0; i < n; i++ ) {
		if ( xoauth2__url_safe(sText[i]) ) {
			sOut[j++] = sText[i];
		} else {
			j += snprintf(sOut + j, 4, "%%%02X", (unsigned char)sText[i]);
		}
	}
	sOut[j] = 0;
	return sOut;
}

/* ------------------------------------------------------------------ */
/* 授权 URL 构造                                                        */
/* ------------------------------------------------------------------ */

static bool xoauth2__add_url_size(size_t* pSize, size_t iAdd)
{
	if ( iAdd > (size_t)-1 - *pSize ) return false;
	*pSize += iAdd;
	return true;
}

char* xoauth2BeginLogin(xoauth2client* pClient)
{
	char* sId = NULL;
	char* sRedir = NULL;
	char* sScope = NULL;
	char* sUrl = NULL;
	size_t nCap;
	int n;
	if ( pClient == NULL || pClient->Config.ClientId == NULL ||
	     pClient->Config.AuthorizeUrl == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "xoauth2BeginLogin: not configured");
		return NULL;
	}

	/* 生成 state */
	if ( !xoauth2StateGenerate(pClient->sState, sizeof(pClient->sState)) ) {
		goto Fail;
	}

	/* 生成 PKCE（尊重预设：UsePkce=false 的 provider 不带 PKCE 参数） */
	if ( pClient->Config.UsePkce ) {
		if ( !xoauth2PkceGenerate(pClient->sVerifier, sizeof(pClient->sVerifier),
		                          pClient->sChallenge, sizeof(pClient->sChallenge)) ) {
			goto Fail;
		}
	}
	else {
		pClient->sVerifier[0] = 0;
		pClient->sChallenge[0] = 0;
	}

	/* OIDC nonce（provider 原样回显进 id_token，NonceConsume 常时比对焚毁） */
	if ( pClient->Config.UseNonce ) {
		if ( !xoauth2StateGenerate(pClient->sNonce, sizeof(pClient->sNonce)) )
			goto Fail;
	}
	else {
		pClient->sNonce[0] = 0;
	}

	/* 每个外部字段都编码；按编码后的实际长度分配。 */
	sId = xoauth2UrlEncode(pClient->Config.ClientId);
	sRedir = xoauth2UrlEncode(pClient->Config.RedirectUri ?
		pClient->Config.RedirectUri : "");
	if ( pClient->Config.Scope )
		sScope = xoauth2UrlEncode(pClient->Config.Scope);
	if ( sId == NULL || sRedir == NULL ||
		(pClient->Config.Scope != NULL && sScope == NULL) )
		goto Fail;
	nCap = 128u;
	if ( !xoauth2__add_url_size(&nCap, strlen(pClient->Config.AuthorizeUrl)) ||
		!xoauth2__add_url_size(&nCap, strlen(sId)) ||
		!xoauth2__add_url_size(&nCap, strlen(sRedir)) ||
		!xoauth2__add_url_size(&nCap, strlen(pClient->sState)) ||
		!xoauth2__add_url_size(&nCap, sScope ? strlen(sScope) : 0u) ||
		!xoauth2__add_url_size(&nCap,
			pClient->Config.UsePkce ? strlen(pClient->sChallenge) : 0u) ||
		!xoauth2__add_url_size(&nCap,
			pClient->Config.UseNonce ? strlen(pClient->sNonce) : 0u) )
	{
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "auth URL too long");
		goto Fail;
	}
	sUrl = (char*)xrtMalloc(nCap);
	if ( sUrl == NULL ) goto Fail;

	if ( pClient->Wechat )
		n = snprintf(sUrl, nCap,
			"%s?appid=%s&redirect_uri=%s&response_type=code&scope=%s&state=%s#wechat_redirect",
			pClient->Config.AuthorizeUrl, sId, sRedir,
			sScope ? sScope : "", pClient->sState);
	else
		n = snprintf(sUrl, nCap,
			"%s?response_type=code&client_id=%s&redirect_uri=%s&state=%s",
			pClient->Config.AuthorizeUrl, sId, sRedir, pClient->sState);

	if ( !pClient->Wechat && sScope != NULL && n > 0 && (size_t)n < nCap ) {
		n += snprintf(sUrl + n, nCap - n, "&scope=%s", sScope);
	}
	if ( pClient->Config.UsePkce && n > 0 && (size_t)n < nCap ) {
		n += snprintf(sUrl + n, nCap - n,
			"&code_challenge=%s&code_challenge_method=S256",
			pClient->sChallenge);
	}
	if ( pClient->Config.UseNonce && n > 0 && (size_t)n < nCap ) {
		n += snprintf(sUrl + n, nCap - n, "&nonce=%s", pClient->sNonce);
	}

	if ( n <= 0 || (size_t)n >= nCap ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "auth URL too long");
		goto Fail;
	}
	xrtFree(sId);
	xrtFree(sRedir);
	xrtFree(sScope);
	return sUrl;

Fail:
	xrtFree(sId);
	xrtFree(sRedir);
	xrtFree(sScope);
	xrtFree(sUrl);
	memset(pClient->sState, 0, sizeof(pClient->sState));
	memset(pClient->sVerifier, 0, sizeof(pClient->sVerifier));
	memset(pClient->sChallenge, 0, sizeof(pClient->sChallenge));
	memset(pClient->sNonce, 0, sizeof(pClient->sNonce));
	return NULL;
}

/* ------------------------------------------------------------------ */
/* 预设持有的端点 URL                                                    */
/* ------------------------------------------------------------------ */

void xoauth2__client_clear_owned(xoauth2client* pClient)
{
	if ( pClient == NULL ) return;
	if ( pClient->pOwnedAuthUrl ) xrtFree(pClient->pOwnedAuthUrl);
	if ( pClient->pOwnedTokenUrl ) xrtFree(pClient->pOwnedTokenUrl);
	if ( pClient->pOwnedIssuerUrl ) xrtFree(pClient->pOwnedIssuerUrl);
	pClient->pOwnedAuthUrl = NULL;
	pClient->pOwnedTokenUrl = NULL;
	pClient->pOwnedIssuerUrl = NULL;
}

/* ------------------------------------------------------------------ */
/* Provider 预设                                                        */
/* ------------------------------------------------------------------ */

void xoauth2ConfigInit(xoauth2config* pConfig)
{
	if ( pConfig == NULL ) return;
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->UsePkce = true;
	pConfig->AuthStyle = XOAUTH2_AUTH_AUTO;
}

void xoauth2UseGithub(xoauth2client* pClient, const char* id, const char* secret, const char* redirect)
{
	if ( pClient == NULL ) return;
	xoauth2__client_clear_owned(pClient);
	memset(pClient, 0, sizeof(*pClient));
	xoauth2ConfigInit(&pClient->Config);
	pClient->Config.AuthorizeUrl = "https://github.com/login/oauth/authorize";
	pClient->Config.TokenUrl = "https://github.com/login/oauth/access_token";
	pClient->Config.UserInfoUrl = "https://api.github.com/user";
	pClient->Config.ClientId = id;
	pClient->Config.ClientSecret = secret;
	pClient->Config.RedirectUri = redirect;
	pClient->Config.Scope = "read:user user:email";
	pClient->Config.AuthStyle = XOAUTH2_AUTH_BODY;
}

void xoauth2UseGoogle(xoauth2client* pClient, const char* id, const char* secret, const char* redirect)
{
	if ( pClient == NULL ) return;
	xoauth2__client_clear_owned(pClient);
	memset(pClient, 0, sizeof(*pClient));
	xoauth2ConfigInit(&pClient->Config);
	pClient->Config.AuthorizeUrl = "https://accounts.google.com/o/oauth2/v2/auth";
	pClient->Config.TokenUrl = "https://oauth2.googleapis.com/token";
	pClient->Config.UserInfoUrl = "https://openidconnect.googleapis.com/v1/userinfo";
	pClient->Config.ClientId = id;
	pClient->Config.ClientSecret = secret;
	pClient->Config.RedirectUri = redirect;
	/* Google 授权端点历史上不支持 PKCE（2024 起支持 S256，保守关闭） */
	pClient->Config.UsePkce = false;
	pClient->Config.Scope = "openid email profile";
	pClient->Config.AuthStyle = XOAUTH2_AUTH_BASIC;
	/* OIDC：issuer/JWKS 由应用层喂给 xjwt（数据耦合） */
	pClient->Config.Issuer = "https://accounts.google.com";
	pClient->Config.JwksUrl = "https://www.googleapis.com/oauth2/v3/certs";
	pClient->Config.UseNonce = true;
}

void xoauth2UseWechat(xoauth2client* pClient, const char* appid, const char* appSecret, const char* redirect)
{
	if ( pClient == NULL ) return;
	xoauth2__client_clear_owned(pClient);
	memset(pClient, 0, sizeof(*pClient));
	xoauth2ConfigInit(&pClient->Config);
	pClient->Config.AuthorizeUrl = "https://open.weixin.qq.com/connect/qrconnect";
	pClient->Config.TokenUrl = "https://api.weixin.qq.com/sns/oauth2/access_token";
	pClient->Config.ClientId = appid;        /* WeChat 用 appid */
	pClient->Config.ClientSecret = appSecret;
	pClient->Config.RedirectUri = redirect;
	pClient->Config.Scope = "snsapi_login";
	pClient->Config.UsePkce = false;
	pClient->Config.AuthStyle = XOAUTH2_AUTH_BODY;
	pClient->Config.UserInfoUrl = "https://api.weixin.qq.com/sns/userinfo";
	pClient->Wechat = true;
}

static bool xoauth2__microsoft_tenant_valid(const char* sTenant)
{
	size_t i, n = strlen(sTenant);
	if(n == 0u || n > 256u)
		return false;
	for(i = 0u; i < n; i++)
	{
		char c = sTenant[i];
		if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') || c == '-' ||
			(c == '.' && i > 0u && i + 1u < n && sTenant[i - 1u] != '.')))
			return false;
	}
	return true;
}

static char* xoauth2__microsoft_url(const char* sTenant, const char* sSuffix)
{
	static const char sBase[] = "https://login.microsoftonline.com/";
	size_t iBase = sizeof(sBase) - 1u;
	size_t iTenant = strlen(sTenant);
	size_t iSuffix = strlen(sSuffix);
	char* sUrl = (char*)xrtMalloc(iBase + iTenant + iSuffix + 1u);
	if(sUrl == NULL)
		return NULL;
	memcpy(sUrl, sBase, iBase);
	memcpy(sUrl + iBase, sTenant, iTenant);
	memcpy(sUrl + iBase + iTenant, sSuffix, iSuffix + 1u);
	return sUrl;
}

void xoauth2UseMicrosoft(xoauth2client* pClient, const char* id, const char* secret,
                         const char* redirect, const char* tenant)
{
	if ( pClient == NULL ) return;
	xoauth2__client_clear_owned(pClient);
	memset(pClient, 0, sizeof(*pClient));
	xoauth2ConfigInit(&pClient->Config);
	/* 按 tenant 动态生成端点；所有权归客户端（clear_owned/ClientUnit 释放） */
	const char* sTenant = tenant ? tenant : "common";
	if ( !xoauth2__microsoft_tenant_valid(sTenant) ) {
		/* 预设是 void 返回：置错误并把端点留空，BeginLogin 会拒绝 */
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
			"tenant must be 1-256 ASCII letters, digits, dots or hyphens");
		pClient->Config.ClientId = id;
		pClient->Config.ClientSecret = secret;
		pClient->Config.RedirectUri = redirect;
		return;
	}
	char* sAuth = xoauth2__microsoft_url(sTenant, "/oauth2/v2.0/authorize");
	char* sToken = xoauth2__microsoft_url(sTenant, "/oauth2/v2.0/token");
	char* sIssuer = xoauth2__microsoft_url(sTenant, "/v2.0");
	if ( sAuth == NULL || sToken == NULL || sIssuer == NULL ) {
		xrtFree(sAuth); xrtFree(sToken); xrtFree(sIssuer);
		return;
	}
	pClient->pOwnedAuthUrl = sAuth;
	pClient->pOwnedTokenUrl = sToken;
	pClient->pOwnedIssuerUrl = sIssuer;
	pClient->Config.AuthorizeUrl = sAuth;
	pClient->Config.TokenUrl = sToken;
	pClient->Config.Issuer = sIssuer;
	pClient->Config.JwksUrl = "https://login.microsoftonline.com/common/discovery/v2.0/keys";
	pClient->Config.ClientId = id;
	pClient->Config.ClientSecret = secret;
	pClient->Config.RedirectUri = redirect;
	pClient->Config.Scope = "openid profile email";
	pClient->Config.UsePkce = true;
	pClient->Config.AuthStyle = XOAUTH2_AUTH_BASIC;
	pClient->Config.UseNonce = true;
}

void xoauth2UseCustom(xoauth2client* pClient, const xoauth2config* pConfig)
{
	if ( pClient == NULL || pConfig == NULL ) return;
	xoauth2__client_clear_owned(pClient);
	memset(pClient, 0, sizeof(*pClient));
	pClient->Config = *pConfig;
}

void xoauth2ClientUnit(xoauth2client* pClient)
{
	if ( pClient == NULL ) return;
	/* 敏感会话状态先显式清零（memset 兜底） */
	memset(pClient->sState, 0, sizeof(pClient->sState));
	memset(pClient->sVerifier, 0, sizeof(pClient->sVerifier));
	memset(pClient->sChallenge, 0, sizeof(pClient->sChallenge));
	memset(pClient->sNonce, 0, sizeof(pClient->sNonce));
	xoauth2__client_clear_owned(pClient);
	/* 端点指针已失效；配置一并清零，调用方须重新预设 */
	memset(&pClient->Config, 0, sizeof(pClient->Config));
	pClient->Wechat = false;
}

void xoauth2TokenFree(xoauth2token* pToken)
{
	if ( pToken == NULL ) return;
	/* 令牌是敏感凭据：释放前清零，避免明文残留堆内存 */
	if ( pToken->AccessToken ) {
		memset(pToken->AccessToken, 0, strlen(pToken->AccessToken));
		xrtFree(pToken->AccessToken);
	}
	if ( pToken->RefreshToken ) {
		memset(pToken->RefreshToken, 0, strlen(pToken->RefreshToken));
		xrtFree(pToken->RefreshToken);
	}
	if ( pToken->IdToken ) {
		memset(pToken->IdToken, 0, strlen(pToken->IdToken));
		xrtFree(pToken->IdToken);
	}
	if ( pToken->OpenId ) {
		memset(pToken->OpenId, 0, strlen(pToken->OpenId));
		xrtFree(pToken->OpenId);
	}
	if ( pToken->TokenType ) xrtFree(pToken->TokenType);
	if ( pToken->Scope ) xrtFree(pToken->Scope);
	memset(pToken, 0, sizeof(*pToken));
	xrtFree(pToken);
}

bool xoauth2TokenExpiring(const xoauth2token* pToken, int leewaySeconds)
{
	if ( pToken == NULL ) return true;
	/* provider 未告知有效期（如 GitHub）：未知 ≠ 即将过期。
	 * 若当"过期"处理，无 refresh_token 的令牌会陷入每请求刷新
	 * 且刷新必然 DENIED 的死循环 */
	if ( pToken->ExpiresIn == 0 ) return false;
	/* 剩余时间 = ExpiresAt - 当前；ExpiresIn 是签发时的总有效期，
	 * 不反映流逝，必须用获取时刻换算 */
	int64_t now = (int64_t)(xrtNow() / 1000000);
	int64_t leeway = leewaySeconds > 0 ? (int64_t)leewaySeconds : 0;
	if ( now > INT64_MAX - leeway ) return true;
	return pToken->ExpiresAt <= now + leeway;
}

/* ------------------------------------------------------------------ */
/* OIDC nonce 消费（常时比对 + 一次性焚毁）                              */
/* ------------------------------------------------------------------ */
bool xoauth2NonceConsume(xoauth2client* pClient, const char* sNonce)
{
	if ( pClient == NULL || sNonce == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "xoauth2NonceConsume: null argument");
		return false;
	}
	size_t nMine = strlen(pClient->sNonce);
	size_t nTheirs = strlen(sNonce);
	if ( nMine == 0 || nMine != nTheirs ||
	     !xrtConstTimeEqual(pClient->sNonce, sNonce, nMine) ) {
		xoauth2__error(XOAUTH2_ERROR_NONCE_MISMATCH,
			"nonce mismatch (possible replay)");
		return false;
	}
	/* 匹配即焚毁：nonce 与 state 同为一次性会话凭证 */
	memset(pClient->sNonce, 0, sizeof(pClient->sNonce));
	return true;
}
#endif


/* ========================================================================== */
/* source: extlibs/xoauth2/src/oauth2/xoauth2_flow.c */
/* ========================================================================== */

#if defined(XOAUTH2_FEATURE_XOAUTH2)
/* xoauth2 token 交换 + 刷新；传输由回调注入。 */

/* ------------------------------------------------------------------ */
/* Token 响应 JSON 解析                                                  */
/* ------------------------------------------------------------------ */

static bool xoauth2__copy_token_field(const xvalue* p, const char* sKey,
	char** ppOut, bool bLowercase)
{
	xvalue* pField = xrtValueObjectGet(p, xrtStrView(sKey));
	xstrview Text;
	size_t i;
	if(pField == NULL)
		return true;
	if(!xrtValueGetString(pField, &Text) || Text.Size == SIZE_MAX) {
		xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE, "invalid token response field type");
		return false;
	}
	/* The public token stores C strings; accepting decoded NUL would lose credential bytes. */
	if(Text.Size != 0u && memchr(Text.Data, 0, Text.Size) != NULL) {
		xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE, "token response field contains NUL");
		return false;
	}
	*ppOut = (char*)xrtMalloc(Text.Size + 1u);
	if(*ppOut == NULL)
		return false;
	for(i = 0u; i < Text.Size; i++)
	{
		char c = ((const char*)Text.Data)[i];
		(*ppOut)[i] = (bLowercase && c >= 'A' && c <= 'Z') ?
			(char)(c + 32) : c;
	}
	(*ppOut)[Text.Size] = 0;
	return true;
}

static xoauth2token* xoauth2__parse_token_response_mode(
	const char* sJson, size_t iSize, bool bWechat)
{
	if ( sJson == NULL || iSize == 0 ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "empty token response");
		return NULL;
	}

	xvalue* p = xrtJsonParse(xrtStrViewN((cstr)sJson, iSize));
	if ( p == NULL ) {
		if ( xrtErrorKind(xrtGetError()) != XERR_MEMORY )
			xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE, "JSON parse failed");
		return NULL;
	}

	/* 检查 error（RFC 6749 §5.2：失败响应只有 error 系字段） */
	{
		xstrview sv;
		if ( xrtValueGetString(xrtValueObjectGet(p, xrtStrView("error")), &sv) ) {
			xoauth2__error(XOAUTH2_ERROR_TOKEN_DENIED, "provider returned error");
			xrtValueRelease(p);
			return NULL;
		}
		if ( bWechat && xrtValueObjectHas(p, xrtStrView("errcode")) ) {
			xoauth2__error(XOAUTH2_ERROR_TOKEN_DENIED,
				"WeChat returned an error code");
			xrtValueRelease(p);
			return NULL;
		}
	}

	xoauth2token* pToken = (xoauth2token*)xrtMalloc(sizeof(xoauth2token));
	if ( pToken == NULL ) { xrtValueRelease(p); return NULL; }
	memset(pToken, 0, sizeof(*pToken));

	/* 已出现的字段若类型错误或复制失败，整份响应必须失败。 */
	if(!xoauth2__copy_token_field(p, "access_token", &pToken->AccessToken, false) ||
		!xoauth2__copy_token_field(p, "refresh_token", &pToken->RefreshToken, false) ||
		!xoauth2__copy_token_field(p, "id_token", &pToken->IdToken, false) ||
		!xoauth2__copy_token_field(p, "openid", &pToken->OpenId, false) ||
		!xoauth2__copy_token_field(p, "token_type", &pToken->TokenType, true) ||
		!xoauth2__copy_token_field(p, "scope", &pToken->Scope, false))
	{
		xrtValueRelease(p);
		xoauth2TokenFree(pToken);
		return NULL;
	}
	{
		xvalue* pExp = xrtValueObjectGet(p, xrtStrView("expires_in"));
		if ( pExp != NULL ) {
			int64 v;
			/* RFC 6749 §4.2.2：expires_in 是十进制整数秒；
			 * 存在但类型错误显式拒绝（字符串时间戳曾是 xjwt 的 H1 教训） */
			if ( !xrtValueIs(pExp, XVALUE_INT) || !xrtValueGetInt(pExp, &v) ) {
				xrtValueRelease(p);
				xoauth2TokenFree(pToken);
				xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE,
					"expires_in must be an integer");
				return NULL;
			}
			pToken->ExpiresIn = v;
		}
	}
	/* 时间戳：获取时刻 + 过期时刻（TokenExpiring 的依据） */
	pToken->ObtainedAt = (int64_t)(xrtNow() / 1000000);
	if((pToken->ExpiresIn > 0 &&
		pToken->ObtainedAt > INT64_MAX - pToken->ExpiresIn) ||
		(pToken->ExpiresIn < 0 &&
		 pToken->ObtainedAt < INT64_MIN - pToken->ExpiresIn))
	{
		xrtValueRelease(p);
		xoauth2TokenFree(pToken);
		xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE,
			"expires_in overflows expiration timestamp");
		return NULL;
	}
	pToken->ExpiresAt = pToken->ObtainedAt + pToken->ExpiresIn;

	xrtValueRelease(p);
	if ( bWechat && pToken->TokenType == NULL ) {
		pToken->TokenType = (char*)xrtMalloc(sizeof("bearer"));
		if ( pToken->TokenType == NULL ) {
			xoauth2TokenFree(pToken);
			return NULL;
		}
		memcpy(pToken->TokenType, "bearer", sizeof("bearer"));
	}

	if ( pToken->AccessToken == NULL || pToken->AccessToken[0] == 0 ||
		 pToken->TokenType == NULL || pToken->TokenType[0] == 0 ||
		 (bWechat && (pToken->OpenId == NULL || pToken->OpenId[0] == 0)) ) {
		xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE,
			"token response is missing required fields");
		xoauth2TokenFree(pToken);
		return NULL;
	}
	return pToken;
}

xoauth2token* xoauth2__parse_token_response(const char* sJson, size_t iSize)
{
	return xoauth2__parse_token_response_mode(sJson, iSize, false);
}

/* ------------------------------------------------------------------ */
/* 请求构造                                                              */
/* ------------------------------------------------------------------ */

/* AuthStyle=BASIC：Authorization: Basic base64(urlenc(id):urlenc(secret))
 * RFC 6749 §2.3.1 要求 id/secret 先做 form 编码再拼接 */
char* xoauth2__build_auth_header(const xoauth2client* pClient)
{
	if ( pClient == NULL || pClient->Config.ClientId == NULL ) return NULL;
	if ( pClient->Config.AuthStyle != XOAUTH2_AUTH_BASIC ) return NULL;
	char* sId = xoauth2UrlEncode(pClient->Config.ClientId);
	if ( sId == NULL ) return NULL;
	char* sSecret = xoauth2UrlEncode(
		pClient->Config.ClientSecret ? pClient->Config.ClientSecret : "");
	if ( sSecret == NULL ) { xrtFree(sId); return NULL; }

	size_t n = strlen(sId) + 1 + strlen(sSecret);
	char* sJoined = (char*)xrtMalloc(n + 1);
	if ( sJoined != NULL ) {
		strcpy(sJoined, sId);
		strcat(sJoined, ":");
		strcat(sJoined, sSecret);
	}
	xrtFree(sId);
	xrtFree(sSecret);
	if ( sJoined == NULL ) return NULL;

	str sB64 = xrtBase64EncodeNew(sJoined, strlen(sJoined), NULL);
	xrtFree(sJoined);
	if ( sB64 == NULL ) return NULL;

	size_t nOut = strlen(sB64) + 16;
	char* sHeader = (char*)xrtMalloc(nOut);
	if ( sHeader != NULL )
		snprintf(sHeader, nOut, "Basic %s", sB64);
	xrtFree(sB64);
	return sHeader;
}

/* 两遍法：先测量总长，再写入。 */
static char* build_form(const char* const* keys,
                        const char* const* values, int nFields)
{
	size_t nNeed = 1;
	for ( int i = 0; i < nFields; i++ ) {
		char* sEnc = xoauth2UrlEncode(values[i]);
		if ( sEnc == NULL ) return NULL;
		size_t nKey = strlen(keys[i]);
		size_t nValue = strlen(sEnc);
		if(nNeed > SIZE_MAX - 2u ||
			nKey > SIZE_MAX - nNeed - 2u ||
			nValue > SIZE_MAX - nNeed - nKey - 2u)
		{
			xrtFree(sEnc);
			xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "token form too long");
			return NULL;
		}
		nNeed += nKey + nValue + 2u;  /* '=' + '&'/'\0' */
		xrtFree(sEnc);
	}
	char* sBody = (char*)xrtMalloc(nNeed);
	if ( sBody == NULL ) return NULL;
	sBody[0] = 0;
	size_t j = 0;
	for ( int i = 0; i < nFields; i++ ) {
		char* sEnc = xoauth2UrlEncode(values[i]);
		if ( sEnc == NULL ) { xrtFree(sBody); return NULL; }
		j += snprintf(sBody + j, nNeed - j, "%s%s=%s",
		              j > 0 ? "&" : "", keys[i], sEnc);
		xrtFree(sEnc);
	}
	return sBody;
}

char* xoauth2__build_token_request(xoauth2client* pClient, const char* sCode)
{
	if ( pClient == NULL || sCode == NULL ||
	     pClient->Config.ClientId == NULL ||
	     pClient->Config.RedirectUri == NULL ||
	     (pClient->Wechat && pClient->Config.ClientSecret == NULL) ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "token request: not configured");
		return NULL;
	}

	/* BASIC 风格：凭据走 Authorization 头，body 不带 client_id/secret */
	bool bBasic = pClient->Config.AuthStyle == XOAUTH2_AUTH_BASIC;

	const char* keys[8];
	const char* values[8];
	int nFields = 0;
	if ( pClient->Wechat ) {
		keys[nFields] = "appid"; values[nFields++] = pClient->Config.ClientId;
		keys[nFields] = "secret"; values[nFields++] = pClient->Config.ClientSecret;
		keys[nFields] = "code"; values[nFields++] = sCode;
		keys[nFields] = "grant_type"; values[nFields++] = "authorization_code";
		return build_form(keys, values, nFields);
	}
	keys[nFields] = "grant_type"; values[nFields++] = "authorization_code";
	keys[nFields] = "code"; values[nFields++] = sCode;
	keys[nFields] = "redirect_uri"; values[nFields++] = pClient->Config.RedirectUri;
	if ( !bBasic ) {
		keys[nFields] = "client_id"; values[nFields++] = pClient->Config.ClientId;
		if ( pClient->Config.ClientSecret != NULL ) {
			keys[nFields] = "client_secret";
			values[nFields++] = pClient->Config.ClientSecret;
		}
	}
	if ( pClient->Config.UsePkce && pClient->sVerifier[0] != 0 ) {
		keys[nFields] = "code_verifier"; values[nFields++] = pClient->sVerifier;
	}

	return build_form(keys, values, nFields);
}

char* xoauth2__build_refresh_request(const xoauth2client* pClient,
                                     const char* sRefreshToken)
{
	if ( pClient == NULL || sRefreshToken == NULL ||
	     pClient->Config.ClientId == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "refresh request: not configured");
		return NULL;
	}

	bool bBasic = pClient->Config.AuthStyle == XOAUTH2_AUTH_BASIC;

	const char* keys[8];
	const char* values[8];
	int nFields = 0;
	if ( pClient->Wechat ) {
		keys[nFields] = "appid"; values[nFields++] = pClient->Config.ClientId;
		keys[nFields] = "grant_type"; values[nFields++] = "refresh_token";
		keys[nFields] = "refresh_token"; values[nFields++] = sRefreshToken;
		return build_form(keys, values, nFields);
	}
	keys[nFields] = "grant_type"; values[nFields++] = "refresh_token";
	keys[nFields] = "refresh_token"; values[nFields++] = sRefreshToken;
	if ( !bBasic ) {
		keys[nFields] = "client_id"; values[nFields++] = pClient->Config.ClientId;
		if ( pClient->Config.ClientSecret != NULL ) {
			keys[nFields] = "client_secret";
			values[nFields++] = pClient->Config.ClientSecret;
		}
	}

	return build_form(keys, values, nFields);
}

/* ------------------------------------------------------------------ */
/* 会话消费（state 常时比较 + 一次性焚毁）                                */
/* ------------------------------------------------------------------ */

static bool state_consume(xoauth2client* pClient, const char* sState)
{
	size_t nMine = strlen(pClient->sState);
	size_t nTheirs = strlen(sState);
	if ( nMine == 0 || nMine != nTheirs ||
	     !xrtConstTimeEqual(pClient->sState, sState, nMine) ) {
		xoauth2__error(XOAUTH2_ERROR_STATE_MISMATCH,
			"state mismatch (possible CSRF)");
		return false;
	}
	/* state 一次性：校验通过即焚毁（无论后续交换成败，会话已消费） */
	xrtSecureZero(pClient->sState, sizeof(pClient->sState));
	return true;
}

/* verifier 焚毁：请求构造完成后一次性消费（RFC 7636 防重放） */
static void verifier_burn(xoauth2client* pClient)
{
	xrtSecureZero(pClient->sVerifier, sizeof(pClient->sVerifier));
	xrtSecureZero(pClient->sChallenge, sizeof(pClient->sChallenge));
}

/* ------------------------------------------------------------------ */
/* 交换（经传输回调）与状态码分级                                         */
/* ------------------------------------------------------------------ */

/*
	响应分级：
	- 回调返回 false → NETWORK（连接/超时/TLS）
	- 2xx → 解析 JSON（解析失败 TOKEN_RESPONSE；带 error 字段 TOKEN_DENIED）
	- 非 2xx 且 body 是带 error 的 JSON → TOKEN_DENIED
	- 其他非 2xx → TOKEN_ENDPOINT
*/
static xoauth2token* exchange(const xoauth2client* pClient,
	const char* sBody, const char* sAuth, bool bRefresh)
{
	xoauth2token* pToken = NULL;
	char* sResp = NULL;
	char* sRequestUrl = NULL;
	const char* sUrl = pClient->Config.TokenUrl;
	const char* sMethod = "POST";
	const char* sRequestBody = sBody;
	int iStatus = 0;

	if ( pClient->Config.Http == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_NETWORK, "no transport configured");
		return NULL;
	}
	if ( pClient->Wechat ) {
		static const char sWechatRefresh[] =
			"https://api.weixin.qq.com/sns/oauth2/refresh_token";
		size_t iBase;
		size_t iQuery = strlen(sBody);
		sUrl = bRefresh ? sWechatRefresh : pClient->Config.TokenUrl;
		iBase = strlen(sUrl);
		if ( iQuery > SIZE_MAX - 2u ||
			iBase > SIZE_MAX - iQuery - 2u ) {
			xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
				"WeChat token URL too long");
			return NULL;
		}
		sRequestUrl = (char*)xrtMalloc(iBase + iQuery + 2u);
		if ( sRequestUrl == NULL ) return NULL;
		memcpy(sRequestUrl, sUrl, iBase);
		sRequestUrl[iBase] = '?';
		memcpy(sRequestUrl + iBase + 1u, sBody, iQuery + 1u);
		sUrl = sRequestUrl;
		sMethod = "GET";
		sRequestBody = NULL;
	}
	/* A silent failed callback must not inherit a caller's stale error. */
	xrtClearError();
	if ( !pClient->Config.Http(sMethod, sUrl, sRequestBody, sAuth,
	                           &sResp, &iStatus, pClient->Config.HttpContext) ) {
		xerror* pError = xrtErrorRef(xrtGetError());
		xrtFree(sRequestUrl);
		xrtFree(sResp);
		if ( pError != NULL ) xrtSetErrorTake(pError);
		else xoauth2__error(XOAUTH2_ERROR_NETWORK, "transport failed");
		return NULL;
	}
	xrtFree(sRequestUrl);
	if ( iStatus >= 200 && iStatus < 300 ) {
		pToken = xoauth2__parse_token_response_mode(
			sResp ? sResp : "", sResp ? strlen(sResp) : 0,
			pClient->Wechat);
	}
	else if ( sResp != NULL && sResp[0] != 0 ) {
		/* 非 2xx：能提取 provider error 就报 DENIED。
		 * 响应体可能同时含完整令牌结构（恶意端点）——探测结果必须释放 */
		xoauth2token* pProbe;
		xrtClearError();
		pProbe = xoauth2__parse_token_response_mode(
			sResp, strlen(sResp), pClient->Wechat);
		if ( pProbe != NULL )
			xoauth2TokenFree(pProbe);
		if ( xoauth2LastError() != XOAUTH2_ERROR_TOKEN_DENIED &&
			xrtErrorKind(xrtGetError()) != XERR_MEMORY )
			xoauth2__error(XOAUTH2_ERROR_TOKEN_ENDPOINT,
				"token endpoint returned HTTP error status");
	}
	else {
		xoauth2__error(XOAUTH2_ERROR_TOKEN_ENDPOINT,
			"token endpoint returned HTTP error status");
	}
	xrtFree(sResp);
	return pToken;
}

/* ------------------------------------------------------------------ */
/* 登录回调 / 刷新                                                       */
/* ------------------------------------------------------------------ */

xoauth2token* xoauth2CompleteLogin(xoauth2client* pClient,
                                   const char* sCode, const char* sState)
{
	if ( pClient == NULL || sCode == NULL || sState == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "xoauth2CompleteLogin: null argument");
		return NULL;
	}
	if ( pClient->Config.TokenUrl == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "xoauth2CompleteLogin: not configured");
		return NULL;
	}

	/* 验 state（常时比较；通过即焚毁会话） */
	if ( !state_consume(pClient, sState) ) return NULL;

	/* 构造请求体与认证头，随即焚毁 verifier */
	char* sBody = xoauth2__build_token_request(pClient, sCode);
	if ( sBody == NULL ) {
		verifier_burn(pClient);
		return NULL;
	}
	char* sAuth = xoauth2__build_auth_header(pClient);
	verifier_burn(pClient);
	if(pClient->Config.AuthStyle == XOAUTH2_AUTH_BASIC && sAuth == NULL)
	{
		xrtFree(sBody);
		return NULL;
	}

	xoauth2token* pToken = exchange(pClient, sBody, sAuth, false);
	xrtFree(sAuth);
	xrtFree(sBody);
	return pToken;
}

xoauth2token* xoauth2Refresh(xoauth2client* pClient, const char* sRefreshToken)
{
	if ( pClient == NULL || sRefreshToken == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "xoauth2Refresh: null argument");
		return NULL;
	}
	if ( pClient->Config.TokenUrl == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "xoauth2Refresh: not configured");
		return NULL;
	}
	char* sBody = xoauth2__build_refresh_request(pClient, sRefreshToken);
	if ( sBody == NULL ) return NULL;
	char* sAuth = xoauth2__build_auth_header(pClient);
	if(pClient->Config.AuthStyle == XOAUTH2_AUTH_BASIC && sAuth == NULL)
	{
		xrtFree(sBody);
		return NULL;
	}

	xoauth2token* pToken = exchange(pClient, sBody, sAuth, true);
	xrtFree(sAuth);
	xrtFree(sBody);
	return pToken;
}

/* ------------------------------------------------------------------ */
/* OIDC / 通用 HTTP 辅助（数据耦合：不依赖 xjwt）                        */
/* ------------------------------------------------------------------ */

char* xoauth2HttpGet(xoauth2client* pClient, const char* sUrl,
                     const char* sAuthHeader, int* piStatus)
{
	char* sResp = NULL;
	int iStatus = 0;

	if ( pClient == NULL || sUrl == NULL || piStatus == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "xoauth2HttpGet: null argument");
		return NULL;
	}
	*piStatus = 0;
	if ( pClient->Config.Http == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_NETWORK, "no transport configured");
		return NULL;
	}
	xrtClearError();
	if ( !pClient->Config.Http("GET", sUrl, NULL, sAuthHeader,
	                           &sResp, &iStatus, pClient->Config.HttpContext) ) {
		xerror* pError = xrtErrorRef(xrtGetError());
		xrtFree(sResp);
		if ( pError != NULL ) xrtSetErrorTake(pError);
		else xoauth2__error(XOAUTH2_ERROR_NETWORK, "transport failed");
		return NULL;
	}
	*piStatus = iStatus;
	if ( iStatus < 200 || iStatus >= 300 ) {
		xrtFree(sResp);
		xoauth2__error(XOAUTH2_ERROR_TOKEN_ENDPOINT,
			"HTTP GET returned error status");
		return NULL;
	}
	if ( sResp == NULL || sResp[0] == '\0' ) {
		xrtFree(sResp);
		xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE, "empty response body");
		return NULL;
	}
	return sResp;
}

xvalue* xoauth2GetUserInfo(xoauth2client* pClient, const char* sAccessToken)
{
	char* sBearer = NULL;
	char* sBody = NULL;
	int iStatus = 0;
	xvalue* pClaims = NULL;

	if ( pClient == NULL || sAccessToken == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "xoauth2GetUserInfo: null argument");
		return NULL;
	}
	if ( pClient->Wechat ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
			"WeChat userinfo requires the token openid");
		return NULL;
	}
	if ( pClient->Config.UserInfoUrl == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
			"xoauth2GetUserInfo: no userinfo url configured");
		return NULL;
	}
	/* Bearer 头动态分配：JWT 形态的 access token 常超 512 字符，
	 * 定长缓冲会静默截断导致 userinfo 永远 401 */
	sBearer = (char*)xrtMalloc(strlen(sAccessToken) + 8);
	if ( sBearer == NULL ) return NULL;
	snprintf(sBearer, strlen(sAccessToken) + 8, "Bearer %s", sAccessToken);
	sBody = xoauth2HttpGet(pClient, pClient->Config.UserInfoUrl,
	                       sBearer, &iStatus);
	xrtFree(sBearer);
	if ( sBody == NULL ) return NULL;

	pClaims = xrtJsonParse(xrtStrView(sBody));
	xrtFree(sBody);
	if ( pClaims == NULL ) {
		if ( xrtErrorKind(xrtGetError()) != XERR_MEMORY )
			xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE, "userinfo JSON parse failed");
		return NULL;
	}
	/* userinfo 必须是 JSON 对象（OIDC Core §5.3）；非对象 fail-closed */
	if ( !xrtValueIs(pClaims, XVALUE_OBJECT) ) {
		xrtValueRelease(pClaims);
		xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE,
			"user info must be a JSON object");
		return NULL;
	}
	return pClaims;
}

xvalue* xoauth2GetWechatUserInfo(xoauth2client* pClient,
	const xoauth2token* pToken)
{
	char* sAccess = NULL;
	char* sOpenId = NULL;
	char* sUrl = NULL;
	char* sBody = NULL;
	xvalue* pClaims = NULL;
	int iStatus = 0;
	size_t nCap, nExtra;
	int nWritten;
	if ( pClient == NULL || !pClient->Wechat ||
		pClient->Config.UserInfoUrl == NULL || pToken == NULL ||
		pToken->AccessToken == NULL || pToken->AccessToken[0] == 0 ||
		pToken->OpenId == NULL || pToken->OpenId[0] == 0 ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
			"WeChat userinfo requires a token with access_token and openid");
		return NULL;
	}
	sAccess = xoauth2UrlEncode(pToken->AccessToken);
	sOpenId = xoauth2UrlEncode(pToken->OpenId);
	if ( sAccess == NULL || sOpenId == NULL ) goto Done;
	nCap = strlen(pClient->Config.UserInfoUrl);
	nExtra = sizeof("?access_token=&openid=");
	if ( strlen(sAccess) > SIZE_MAX - nExtra ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
			"WeChat userinfo URL too long");
		goto Done;
	}
	nExtra += strlen(sAccess);
	if ( strlen(sOpenId) > SIZE_MAX - nExtra ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
			"WeChat userinfo URL too long");
		goto Done;
	}
	nExtra += strlen(sOpenId);
	if ( nCap > SIZE_MAX - nExtra ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
			"WeChat userinfo URL too long");
		goto Done;
	}
	nCap += nExtra;
	sUrl = (char*)xrtMalloc(nCap);
	if ( sUrl == NULL ) goto Done;
	nWritten = snprintf(sUrl, nCap, "%s?access_token=%s&openid=%s",
		pClient->Config.UserInfoUrl, sAccess, sOpenId);
	if ( nWritten < 0 || (size_t)nWritten >= nCap )
		goto Done;
	sBody = xoauth2HttpGet(pClient, sUrl, NULL, &iStatus);
	if ( sBody == NULL ) goto Done;
	pClaims = xrtJsonParse(xrtStrView(sBody));
	if ( pClaims == NULL ) {
		if ( xrtErrorKind(xrtGetError()) != XERR_MEMORY )
			xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE,
				"invalid WeChat userinfo response");
	}
	else if ( !xrtValueIs(pClaims, XVALUE_OBJECT) ) {
		xrtValueRelease(pClaims);
		pClaims = NULL;
		xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE,
			"invalid WeChat userinfo response");
	}
	else if ( xrtValueObjectHas(pClaims, xrtStrView("errcode")) ) {
		xrtValueRelease(pClaims);
		pClaims = NULL;
		xoauth2__error(XOAUTH2_ERROR_TOKEN_DENIED,
			"WeChat userinfo was denied");
	}
Done:
	xrtFree(sAccess);
	xrtFree(sOpenId);
	xrtFree(sUrl);
	xrtFree(sBody);
	return pClaims;
}
#endif


/* ========================================================================== */
/* source: extlibs/xoauth2/src/oauth2/xoauth2_http.c */
/* ========================================================================== */

#if defined(XOAUTH2_FEATURE_XOAUTH2)
/* xoauth2 便捷传输：直连 xrt net engine + TLS + http1。
 * 模式取自 xacme_http（单次 POST、Connection: close、future 化 IO），
 * 按 oauth2 场景裁剪：无重放 nonce / 无重试；POST 带 form 头，GET 无实体头。
 * 支持 http://（明文，测试/内网 IdP）与 https://（系统或指定 CA 验证）。
 * 依赖由模块清单声明；通过公共 <xrt.h> 消费核心。 */

#define XOAUTH2_HTTP_FIELD_MAX 100u
#define XOAUTH2_HTTP_IO_CHUNK 16384u
#define XOAUTH2_HTTP_TIMEOUT_DEFAULT 15000000ull  /* 15s */
#define XOAUTH2_HTTP_ROLLBACK_TIMEOUT_MIN 30000000ull
/* 响应体上限：token/JWKS/userinfo 响应远小于 1MB；
 * 防 malformed Content-Length / 无限 chunked 把内存吃光 */
#define XOAUTH2_HTTP_MAX_BODY (1024u * 1024u)


/* Only unpublished failed heap constructors share this resource retirement queue. */
static xatomic32 __xoauth2PendingLock = { 0u };
static xoauth2httpxrt* __xoauth2PendingHead;
static xoauth2httpxrt* __xoauth2PendingTail;
static size_t __xoauth2PendingCount;

static void http_pending_lock(void)
{
	uint32 expected = 0u;
	while ( !xrtAtomic32CompareExchange(&__xoauth2PendingLock, &expected, 1u,
		XMEMORY_ACQUIRE, XMEMORY_RELAXED) ) {
		expected = 0u;
		xrtThreadYield();
	}
}

static void http_pending_unlock(void)
{
	xrtAtomic32Store(&__xoauth2PendingLock, 0u, XMEMORY_RELEASE);
}

/* Init failure has already released resolver/verifier; consume the heap owner without allocating. */
static void http_defer_owner(xoauth2httpxrt* pHttp)
{
	http_pending_lock();
	pHttp->pPendingNext = __xoauth2PendingHead;
	__xoauth2PendingHead = pHttp;
	if ( __xoauth2PendingTail == NULL ) __xoauth2PendingTail = pHttp;
	__xoauth2PendingCount++;
	http_pending_unlock();
}

bool xoauth2HttpXrtCleanupPending(uint64_t uTimeoutUs, size_t* piPending)
{
	xerror* pPrevious = xrtErrorRef(xrtGetError());
	xerror* pFirst = NULL;
	xdeadline deadline = xrtDeadlineAfter(uTimeoutUs);
	size_t iPending;
	bool bError = false;
	for (;;) {
		xoauth2httpxrt *pList, *pListTail, *pWait = NULL, *pWaitTail = NULL;
		http_pending_lock();
		pList = __xoauth2PendingHead;
		pListTail = __xoauth2PendingTail;
		__xoauth2PendingHead = NULL;
		__xoauth2PendingTail = NULL;
		http_pending_unlock();
		/* Claimed owners remain counted, including while another caller drains them. */
		while ( pList != NULL ) {
			xoauth2httpxrt* pNext = pList->pPendingNext;
			xnetretireresult retired = xrtNetEngineTryDestroy(pList->pEngine);
			if ( retired == XNET_RETIRE_READY ) {
				xrtSecureZero(pList, sizeof(*pList));
				xrtFree(pList);
				http_pending_lock();
				__xoauth2PendingCount--;
				http_pending_unlock();
			} else {
				if ( retired == XNET_RETIRE_ERROR ) {
					bError = true;
					if ( pFirst == NULL ) pFirst = xrtErrorRef(xrtGetError());
				}
				pList->pPendingNext = pWait;
				pWait = pList;
				if ( pWaitTail == NULL ) pWaitTail = pList;
			}
			pList = pNext;
			if ( pList != NULL && (bError ||
				(uTimeoutUs != 0u && xrtDeadlineExpired(deadline))) ) {
				/* Preserve the unvisited tail after ERROR or budget exhaustion. */
				if ( pWaitTail != NULL ) pWaitTail->pPendingNext = pList;
				else pWait = pList;
				pWaitTail = pListTail;
				pList = NULL;
			}
		}
		http_pending_lock();
		if ( pWait != NULL ) {
			pWaitTail->pPendingNext = __xoauth2PendingHead;
			if ( __xoauth2PendingTail == NULL ) __xoauth2PendingTail = pWaitTail;
			__xoauth2PendingHead = pWait;
		}
		iPending = __xoauth2PendingCount;
		http_pending_unlock();
		if ( iPending == 0u || bError || uTimeoutUs == 0u ) break;
		if ( xrtDeadlineExpired(deadline) ) {
			xrtSetErrorInfo(XERR_TIMEOUT, "xrt.oauth2", XOAUTH2_ERROR_NETWORK,
				"http pending cleanup still has live objects");
			break;
		}
		xrtSleep(1u);
	}
	if ( piPending != NULL ) *piPending = iPending;
	if ( pPrevious != NULL ) {
		xrtErrorFree(pFirst);
		xrtSetErrorTake(pPrevious);
	} else if ( pFirst != NULL ) xrtSetErrorTake(pFirst);
	return iPending == 0u;
}

/* ------------------------------------------------------------------ */
/* URL 解析（http(s)://host[:port]/path）                                */
/* ------------------------------------------------------------------ */

static bool url_scheme_equal(const char* sUrl, const char* sPrefix)
{
	while ( *sPrefix != 0 ) {
		unsigned char c = (unsigned char)*sUrl++;
		if ( c >= 'A' && c <= 'Z' ) c += 'a' - 'A';
		if ( c != (unsigned char)*sPrefix++ ) return false;
	}
	return true;
}

static bool url_hex_valid(unsigned char c)
{
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
		(c >= 'a' && c <= 'f');
}

static bool url_port_parse(const char* sPort, size_t iSize, uint16_t* pPort)
{
	unsigned iPort = 0;
	if ( iSize == 0u ) return false;
	for ( size_t i = 0; i < iSize; i++ ) {
		unsigned iDigit;
		if ( sPort[i] < '0' || sPort[i] > '9' ) return false;
		iDigit = (unsigned)(sPort[i] - '0');
		if ( iPort > (65535u - iDigit) / 10u ) return false;
		iPort = iPort * 10u + iDigit;
	}
	if ( iPort == 0u ) return false;
	*pPort = (uint16_t)iPort;
	return true;
}

static bool url_target_char_valid(unsigned char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		(c >= '0' && c <= '9') ||
		(c != 0u && strchr("-._~!$&'()*+,;=:@/?", (int)c) != NULL);
}

/* Validate the ASCII path/query/fragment grammar before discarding a fragment. */
static bool url_component_valid(const char* sText, size_t iSize)
{
	for ( size_t i = 0; i < iSize; i++ ) {
		unsigned char c = (unsigned char)sText[i];
		if ( c == '%' ) {
			if ( iSize - i < 3u || !url_hex_valid((unsigned char)sText[i + 1u]) ||
				!url_hex_valid((unsigned char)sText[i + 2u]) ) return false;
			i += 2u;
		} else if ( !url_target_char_valid(c) ) return false;
	}
	return true;
}

/* Legacy IPv4 components may mix decimal/octal and 0x hexadecimal forms. */
static bool url_numeric_host(const char* sHost, size_t iSize)
{
	size_t i = 0u;
	while ( i < iSize ) {
		size_t iStart;
		bool bHex = iSize - i >= 2u && sHost[i] == '0' &&
			(sHost[i + 1u] == 'x' || sHost[i + 1u] == 'X');
		if ( bHex ) i += 2u;
		iStart = i;
		while ( i < iSize && sHost[i] != '.' ) {
			unsigned char c = (unsigned char)sHost[i];
			if ( bHex ? !url_hex_valid(c) : (c < '0' || c > '9') ) return false;
			i++;
		}
		if ( i == iStart ) return false;
		if ( i < iSize ) i++; /* A root dot does not turn an address into a DNS identity. */
	}
	return true;
}

bool xoauth2__url_parse(const char* sUrl, xoauth2url* pOut)
{
	const char *sHost, *sTail, *sPort;
	size_t iAuthorityLen, iHostLen, iTargetLen, iPrefix;

	memset(pOut, 0, sizeof(*pOut));
	if ( sUrl == NULL ) return false;
	if ( url_scheme_equal(sUrl, "https://") ) {
		pOut->bTls = true;
		sHost = sUrl + 8;
	}
	else if ( url_scheme_equal(sUrl, "http://") ) {
		pOut->bTls = false;
		sHost = sUrl + 7;
	}
	else {
		return false;
	}
	pOut->iPort = pOut->bTls ? 443u : 80u;
	/* Authority 在首个 /、? 或 # 结束；userinfo 会改变目标主机，拒绝。 */
	iAuthorityLen = strcspn(sHost, "/?#");
	if ( iAuthorityLen == 0u || memchr(sHost, '@', iAuthorityLen) != NULL )
		return false;
	sTail = sHost + iAuthorityLen;
	iPrefix = *sTail == '/' ? 0u : 1u;
	iTargetLen = strcspn(sTail, "#");
	if ( iTargetLen > sizeof(pOut->sPath) - 1u - iPrefix ) return false;
	if ( !url_component_valid(sTail, iTargetLen) ) return false;
	if ( sTail[iTargetLen] == '#' &&
		!url_component_valid(sTail + iTargetLen + 1u, strlen(sTail + iTargetLen + 1u)) ) return false;
	if ( iPrefix ) pOut->sPath[0] = '/';
	memcpy(pOut->sPath + iPrefix, sTail, iTargetLen);
	pOut->sPath[iPrefix + iTargetLen] = 0;
	if ( sHost[0] == '[' ) {
		/* IPv6 字面量：[addr] 或 [addr]:port；Host 头按 RFC 9110 保留括号 */
		const char* sClose = (const char*)memchr(sHost, ']', iAuthorityLen);
		size_t iAddr;
		xnetaddr Address;
		bool bNumeric;
		if ( sClose == NULL ) return false;
		iAddr = (size_t)(sClose - sHost) + 1;
		if ( iAddr <= 2u || iAddr >= sizeof(pOut->sHost) ) return false;
		for ( size_t i = 1u; i + 1u < iAddr; i++ ) {
			unsigned char c = (unsigned char)sHost[i];
			if ( !url_hex_valid(c) && c != ':' && c != '.' ) return false;
		}
		memcpy(pOut->sHost, sHost, iAddr);
		pOut->sHost[iAddr] = 0;
		if ( iAddr < iAuthorityLen ) {
			if ( sHost[iAddr] != ':' ) return false;
			if ( !url_port_parse(sHost + iAddr + 1u,
				iAuthorityLen - iAddr - 1u, &pOut->iPort) ) return false;
		}
		pOut->sHost[iAddr - 1u] = 0;
		bNumeric = xrtNetAddrParse(&Address, pOut->sHost + 1u,
			pOut->iPort) && Address.Family == XNET_FAMILY_IPV6;
		pOut->sHost[iAddr - 1u] = ']';
		if ( !bNumeric ) return false;
		pOut->bIpLiteral = true;
		return true;
	}
	sPort = (const char*)memchr(sHost, ':', iAuthorityLen);
	iHostLen = iAuthorityLen;
	if ( sPort != NULL ) {
		if ( !url_port_parse(sPort + 1u,
			iAuthorityLen - (size_t)(sPort - sHost) - 1u,
			&pOut->iPort) ) return false;
		iHostLen = (size_t)(sPort - sHost);
	}
	if ( iHostLen == 0 || iHostLen >= sizeof(pOut->sHost) ||
		sHost[0] == '.' ||
		(iHostLen > 1u && sHost[iHostLen - 1u] == '.' &&
		 sHost[iHostLen - 2u] == '.') ) return false;
	for ( size_t i = 0; i < iHostLen; i++ ) {
		unsigned char c = (unsigned char)sHost[i];
		if ( !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_') )
			return false;
	}
	memcpy(pOut->sHost, sHost, iHostLen);
	{
		bool bNumeric = true;
		for ( size_t i = 0; i < iHostLen && bNumeric; i++ )
			bNumeric = (sHost[i] >= '0' && sHost[i] <= '9') ||
				sHost[i] == '.';
		if ( bNumeric ) {
			xnetaddr Address;
			if ( !xrtNetAddrParse(&Address, pOut->sHost,
				pOut->iPort) || Address.Family != XNET_FAMILY_IPV4 )
				return false;
			pOut->bIpLiteral = true;
		}
		else if ( url_numeric_host(sHost, iHostLen) ) return false;
	}
	return true;
}

void xoauth2__url_tls_names(const xoauth2url* pUrl,
	xtlsclientconfig* pTls, xtlsdialconfig* pDial)
{
	const char* sVerify = pUrl->sHost;
	size_t iVerifySize = strlen(sVerify);
	if ( pUrl->bIpLiteral && sVerify[0] == '[' ) {
		sVerify++;
		iVerifySize -= 2u;
	} else if ( !pUrl->bIpLiteral && iVerifySize > 0u &&
		sVerify[iVerifySize - 1u] == '.' ) {
		/* RFC 6066 的 SNI DNS 主机名不含末尾根点。 */
		iVerifySize--;
	}
	pTls->VerifyName = (xstrview){ sVerify, iVerifySize };
	if ( !pUrl->bIpLiteral )
		pTls->ServerName = pTls->VerifyName;
	else
		pTls->ServerName = (xstrview){ NULL, 0 };
	/* 即使 ServerName 为空，也不能让 Dial 从 IP 主机自动补成 SNI。 */
	pDial->ServerNameFromHost = false;
}

/* ------------------------------------------------------------------ */
/* 初始化 / 释放                                                         */
/* ------------------------------------------------------------------ */
bool xoauth2HttpXrtInit(xoauth2httpxrt* pHttp, void* pBorrowedEngine,
                        const char* sCaPem, uint64_t uTimeoutUs)
{
	if ( pHttp == NULL ) return false;
	memset(pHttp, 0, sizeof(*pHttp));
	pHttp->uTimeoutUs = (uTimeoutUs != 0) ? uTimeoutUs
	                                      : XOAUTH2_HTTP_TIMEOUT_DEFAULT;
	if ( pBorrowedEngine == NULL && !xoauth2HttpXrtCleanupPending(0u, NULL) ) {
		xoauth2__error(XOAUTH2_ERROR_NETWORK,
			"http pending cleanup must finish before creating a private engine");
		return false;
	}

	pHttp->pEngine = (xnetengine*)pBorrowedEngine;
	pHttp->pResolver = xrtNetResolverCreate(NULL);
	if ( pHttp->pResolver == NULL ) goto fail;

	{
		xtlsverifierconfig Verify;
		xx509store* pStore = NULL;
		xrtTlsVerifierConfigInit(&Verify);
		if ( sCaPem != NULL && sCaPem[0] != '\0' ) {
			size_t iAdded = 0;
			pStore = xrtX509StoreCreate();
			if ( pStore == NULL ||
			     !xrtX509StoreAddPem(pStore, sCaPem, strlen(sCaPem), &iAdded) ||
			     iAdded == 0 ) {
				xrtX509StoreFree(pStore);
				goto fail;
			}
			Verify.Store = pStore;
		}
		else {
			pStore = xrtX509StoreSystem();
			if ( pStore == NULL ) goto fail;
			Verify.Store = pStore;
		}
		pHttp->pVerifier = xrtTlsVerifierCreate(&Verify);
		xrtX509StoreFree(pStore);   /* 信任库已深复制进验证器 */
		if ( pHttp->pVerifier == NULL ) goto fail;
	}
	/* 配置完成后才启动；失败时有界退休，未完成的拥有者仍可重试。 */
	if ( pHttp->pEngine == NULL ) {
		xnetengineconfig Engine;
		xrtNetEngineConfigInit(&Engine);
		pHttp->pEngine = xrtNetEngineCreate(&Engine);
		if ( pHttp->pEngine == NULL ) goto fail;
		pHttp->bEngineOwned = true;
		if ( !xrtNetEngineStart(pHttp->pEngine) ) goto fail;
	}
	return true;

fail:
	{
		xerror* pCause = xrtErrorRef(xrtGetError());
		uint64_t uRequestTimeout = pHttp->uTimeoutUs;
		if ( pHttp->uTimeoutUs < XOAUTH2_HTTP_ROLLBACK_TIMEOUT_MIN )
			pHttp->uTimeoutUs = XOAUTH2_HTTP_ROLLBACK_TIMEOUT_MIN;
		(void)xoauth2HttpXrtCleanup(pHttp);
		pHttp->uTimeoutUs = uRequestTimeout;
		if ( pCause != NULL ) {
			xerror* pFailure = xrtErrorWrap(pCause, XERR_STATE, "xrt.oauth2",
				XOAUTH2_ERROR_NETWORK, "http transport init failed");
			if ( pFailure != NULL ) {
				xrtErrorFree(pCause);
				xrtSetErrorTake(pFailure);
			} else xrtSetErrorTake(pCause);
		} else xoauth2__error(XOAUTH2_ERROR_NETWORK, "http transport init failed");
	}
	return false;
}

xoauth2httpxrt* xoauth2HttpXrtCreate(void* pBorrowedEngine,
                                     const char* sCaPem, uint64_t uTimeoutUs)
{
	xoauth2httpxrt* pHttp = (xoauth2httpxrt*)xrtMalloc(sizeof(xoauth2httpxrt));
	if ( pHttp == NULL ) return NULL;
	if ( !xoauth2HttpXrtInit(pHttp, pBorrowedEngine, sCaPem, uTimeoutUs) ) {
		if ( pHttp->bEngineOwned && pHttp->pEngine != NULL ) http_defer_owner(pHttp);
		else xrtFree(pHttp);
		return NULL;
	}
	return pHttp;
}

void xoauth2HttpXrtDestroy(xoauth2httpxrt* pHttp)
{
	if ( pHttp == NULL ) return;
	if ( xoauth2HttpXrtCleanup(pHttp) ) xrtFree(pHttp);
}

void xoauth2HttpXrtUnit(xoauth2httpxrt* pHttp)
{
	(void)xoauth2HttpXrtCleanup(pHttp);
}

bool xoauth2HttpXrtCleanup(xoauth2httpxrt* pHttp)
{
	bool bReady = true;
	xerror* pPrevious;
	if ( pHttp == NULL ) return true;
	pPrevious = xrtErrorRef(xrtGetError());
	if ( pHttp->pResolver != NULL ) {
		xrtNetResolverDestroy(pHttp->pResolver);
		pHttp->pResolver = NULL;
	}
	if ( pHttp->bEngineOwned && pHttp->pEngine != NULL ) {
		xdeadline Deadline = xrtDeadlineAfter(pHttp->uTimeoutUs);
		for (;;) {
			xnetretireresult Result = xrtNetEngineTryDestroy(pHttp->pEngine);
			if ( Result == XNET_RETIRE_READY ) {
				pHttp->pEngine = NULL;
				pHttp->bEngineOwned = false;
				break;
			}
			if ( Result == XNET_RETIRE_ERROR ) { bReady = false; break; }
			if ( xrtDeadlineExpired(Deadline) ) {
				xoauth2__error(XOAUTH2_ERROR_NETWORK,
					"http transport engine still has live objects during cleanup");
				bReady = false;
				break;
			}
			/* Close/Abort 异步释放内部引用；只在 READY 时消费创建者拥有权。 */
			xrtSleep(1u);
		}
	} else {
		pHttp->pEngine = NULL;
		pHttp->bEngineOwned = false;
	}
	if ( pHttp->pVerifier != NULL ) {
		xrtTlsVerifierRelease((xtlsverifier*)pHttp->pVerifier);
		pHttp->pVerifier = NULL;
	}
	if ( pPrevious != NULL ) xrtSetErrorTake(pPrevious);
	return bReady;
}

/* ------------------------------------------------------------------ */
/* 流封装（future 化 IO，同 xacme）                                      */
/* ------------------------------------------------------------------ */
typedef struct xoauth2stream {
	xtlsstream* pTls;
	xnetstream* pTcp;
} xoauth2stream;

static bool future_resolved_for(xfuture* pFuture, uint64 uRemaining)
{
	return uRemaining != 0u &&
		xrtFutureWaitFor(pFuture, uRemaining) == XWAIT_OK &&
		xrtFutureState(pFuture) == XFUTURE_RESOLVED;
}

/* Wait only publishes the terminal state, not the producer's thread error.
 * Retain that diagnostic before releasing the observer or requesting cancel. */
static void future_finish(xfuture* pFuture, bool bResolved)
{
	xerror* pPrevious = NULL;
	if ( !bResolved ) {
		const xerror* pError = xrtFutureError(pFuture);
		if ( pError != NULL ) xrtSetError(pError);
		pPrevious = xrtErrorRef(xrtGetError());
		if ( xrtFutureState(pFuture) == XFUTURE_PENDING )
			(void)xrtFutureCancel(pFuture);
	}
	xrtFutureDestroy(pFuture);
	if ( pPrevious != NULL ) xrtSetErrorTake(pPrevious);
}

static void transport_error(const char* sMessage)
{
	/* An exhausted allocator is not a retryable network failure. */
	if ( xrtErrorKind(xrtGetError()) != XERR_MEMORY )
		xoauth2__error(XOAUTH2_ERROR_NETWORK, sMessage);
}

bool xoauth2__future_wait(xfuture* pFuture, xdeadline Deadline)
{
	uint64 uRemaining = xrtDeadlineRemaining(Deadline);
	bool bResolved = future_resolved_for(pFuture, uRemaining);
	future_finish(pFuture, bResolved);
	return bResolved;
}

static bool stream_send_all(xoauth2stream* pStream, const void* pData,
                            size_t iSize, uint64_t uUs)
{
	size_t iOffset = 0;
	xdeadline Deadline = xrtDeadlineAfter(uUs);
	while ( iOffset < iSize ) {
		size_t iChunk = iSize - iOffset;
		if ( xrtDeadlineExpired(Deadline) ) return false;
		if ( iChunk > XOAUTH2_HTTP_IO_CHUNK ) iChunk = XOAUTH2_HTTP_IO_CHUNK;
		if ( pStream->pTls != NULL ) {
			xfuture* pF = xrtTlsStreamSendAsync(
				pStream->pTls, (const uint8*)pData + iOffset, iChunk);
			if ( pF == NULL || !xoauth2__future_wait(pF, Deadline) ) return false;
		}
		else {
			xnetresult eResult;
			xfuture* pF;
			while ( (eResult = xrtNetStreamSend(
				pStream->pTcp, (const uint8*)pData + iOffset, iChunk))
				== XNET_RESULT_AGAIN ) {
				if ( xrtDeadlineExpired(Deadline) ) return false;
				pF = xrtNetStreamWaitAsync(
					pStream->pTcp, XNET_STREAM_WAIT_WRITE);
				if ( pF == NULL || !xoauth2__future_wait(pF, Deadline) ) return false;
			}
			if ( eResult != XNET_RESULT_OK ) {
				const xerror* pError = xrtNetStreamError(pStream->pTcp);
				if ( pError != NULL ) xrtSetError(pError);
				return false;
			}
		}
		iOffset += iChunk;
	}
	return true;
}

/* 返回 1=有数据，0=流结束，-1=超时，-2=I/O 失败。 */
static int stream_recv(xoauth2stream* pStream, uint8* pBuffer,
                       size_t iCapacity, size_t* pRead, xdeadline Deadline)
{
	xfuture* pFuture;
	xnetbytes* pBytes;
	xwaitresult eWait;
	uint64 uRemaining = xrtDeadlineRemaining(Deadline);
	if ( uRemaining == 0u ) return -1;
	if ( pStream->pTls != NULL )
		pFuture = xrtTlsStreamRecvAsync(pStream->pTls, iCapacity);
	else
		pFuture = xrtNetStreamRecvAsync(pStream->pTcp, iCapacity);
	if ( pFuture == NULL ) return -2;
	eWait = xrtFutureWaitFor(pFuture, uRemaining);
	if ( eWait == XWAIT_OK && xrtFutureState(pFuture) == XFUTURE_CLOSED ) {
		xrtFutureDestroy(pFuture);
		/* Recv 的 CLOSED 可能是 EOF，也可能是终止路径；确认正常读端结束。 */
		if ( pStream->pTls != NULL ) {
			bool bEnd;
			pFuture = xrtTlsStreamWaitAsync(pStream->pTls, XTLS_STREAM_WAIT_END);
			if ( pFuture == NULL ) return -2;
			eWait = xrtFutureWaitFor(pFuture, xrtDeadlineRemaining(Deadline));
			bEnd = eWait == XWAIT_OK &&
				xrtFutureState(pFuture) == XFUTURE_RESOLVED;
			future_finish(pFuture, bEnd);
			return bEnd ? 0 : (eWait == XWAIT_TIMEOUT ? -1 : -2);
		} else {
			xnetstreamstats Stats;
			const xerror* pError;
			if ( !xrtNetStreamStats(pStream->pTcp, &Stats) ) return -2;
			pError = xrtNetStreamError(pStream->pTcp);
			if ( pError != NULL ) xrtSetError(pError);
			return Stats.ReadEnded && pError == NULL ? 0 : -2;
		}
	}
	if ( eWait != XWAIT_OK ||
	     xrtFutureState(pFuture) != XFUTURE_RESOLVED ) {
		future_finish(pFuture, false);
		return eWait == XWAIT_TIMEOUT ? -1 : -2;
	}
	pBytes = (xnetbytes*)xrtFutureValue(pFuture);
	if ( pBytes == NULL || xrtNetBytesView(pBytes).Size == 0 ) {
		xrtFutureDestroy(pFuture);
		return -2;  /* Recv 成功必须交付非空数据；EOF 由 CLOSED 表示。 */
	}
	{
		xbytesview View = xrtNetBytesView(pBytes);
		if ( View.Size > iCapacity ) {
			xrtFutureDestroy(pFuture);
			return -2;
		}
		memcpy(pBuffer, View.Data, View.Size);
		*pRead = View.Size;
	}
	xrtFutureDestroy(pFuture);
	return 1;
}

static void stream_close(xoauth2stream* pStream, bool bAbort)
{
	if ( pStream->pTls != NULL ) {
		if ( bAbort ) (void)xrtTlsStreamAbort(pStream->pTls);
		else (void)xrtTlsStreamClose(pStream->pTls);
		xrtTlsStreamDestroy(pStream->pTls);
		pStream->pTls = NULL;
	}
	if ( pStream->pTcp != NULL ) {
		if ( bAbort ) (void)xrtNetStreamAbort(pStream->pTcp);
		else (void)xrtNetStreamClose(pStream->pTcp);
		xrtNetStreamDestroy(pStream->pTcp);
		pStream->pTcp = NULL;
	}
}

/* ------------------------------------------------------------------ */
/* 传输回调                                                             */
/* ------------------------------------------------------------------ */
bool xoauth2HttpXrt(const char* sMethod, const char* sUrl, const char* sBody,
                    const char* sAuthHeader,
                    char** psResponseBody, int* piStatus, void* pContext)
{
	xoauth2httpxrt* pHttp = (xoauth2httpxrt*)pContext;
	xoauth2url Url;
	xoauth2stream Stream = { NULL, NULL };
	xbuffer Request, Received, BodyBuffer;
	xhttpfield Fields[XOAUTH2_HTTP_FIELD_MAX];
	xhttp1head Head;
	xhttp1limits Limits;
	xhttp1errorinfo ProtocolError;
	xhttp1bodyplan Plan;
	xhttp1body Body;
	xhttp1bodylimits BodyLimits;
	xhttp1bodystatus eBody;
	char sHostHeader[280];
	char sLength[24];
	char sPortText[8];
	uint8 Chunk[XOAUTH2_HTTP_IO_CHUNK];
	size_t iRequestSize = 0, iUsed = 0, iConsumed = 0;
	size_t iBodyLen = sBody ? strlen(sBody) : 0;
	xfuture* pFuture = NULL;
	xerror* pFailure = NULL;
	bool bOk = false, bHeadDone = false, bStreamEnd = false;
	size_t iField = 0;
	xdeadline ResponseDeadline;

	if ( pHttp == NULL || sUrl == NULL || psResponseBody == NULL ||
	     piStatus == NULL || sMethod == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "http transport: null argument");
		return false;
	}
	*psResponseBody = NULL;
	*piStatus = 0;
	if ( pHttp->pEngine == NULL || pHttp->pResolver == NULL ||
	     pHttp->pVerifier == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
			"http transport is not initialized or has been cleaned up");
		return false;
	}
	if ( !xoauth2__url_parse(sUrl, &Url) ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
			"http transport URL is invalid or unsupported");
		return false;
	}
	xrtClearError();
	if ( (Url.bTls ? 443u : 80u) != Url.iPort )
		snprintf(sPortText, sizeof(sPortText), ":%u", (unsigned)Url.iPort);
	else
		sPortText[0] = '\0';
	snprintf(sHostHeader, sizeof(sHostHeader), "%s%s", Url.sHost, sPortText);

	/* ---- 请求组装 ---- */
	xrtBufferInit(&Request);
	xrtBufferInit(&Received);
	xrtBufferInit(&BodyBuffer);
	Fields[iField++] = (xhttpfield){
		XRT_STR_LITERAL("Host"), (xstrview){ sHostHeader, strlen(sHostHeader) } };
	Fields[iField++] = (xhttpfield){
		XRT_STR_LITERAL("User-Agent"), XRT_STR_LITERAL("xoauth2") };
	Fields[iField++] = (xhttpfield){
		XRT_STR_LITERAL("Accept"), XRT_STR_LITERAL("application/json") };
	Fields[iField++] = (xhttpfield){
		XRT_STR_LITERAL("Connection"), XRT_STR_LITERAL("close") };
	if ( sAuthHeader != NULL && sAuthHeader[0] != '\0' ) {
		Fields[iField++] = (xhttpfield){
			XRT_STR_LITERAL("Authorization"),
			(xstrview){ sAuthHeader, strlen(sAuthHeader) } };
	}
	if ( sBody != NULL ) {
		Fields[iField++] = (xhttpfield){
			XRT_STR_LITERAL("Content-Type"),
			XRT_STR_LITERAL("application/x-www-form-urlencoded") };
		snprintf(sLength, sizeof(sLength), "%llu", (unsigned long long)iBodyLen);
		Fields[iField++] = (xhttpfield){
			XRT_STR_LITERAL("Content-Length"),
			(xstrview){ sLength, strlen(sLength) } };
	}

	if ( !xrtHttp1RequestWrite(
		(xstrview){ sMethod, strlen(sMethod) }, (xstrview){ Url.sPath, strlen(Url.sPath) },
		XHTTP_VERSION_1_1, Fields, iField, NULL, 0, &iRequestSize) )
		goto done;
	if ( !xrtBufferResize(&Request, iRequestSize) ||
	     !xrtHttp1RequestWrite(
		(xstrview){ sMethod, strlen(sMethod) }, (xstrview){ Url.sPath, strlen(Url.sPath) },
		XHTTP_VERSION_1_1, Fields, iField,
		Request.Data, iRequestSize, &iRequestSize) )
		goto done;
	if ( iBodyLen > 0 &&
	     !xrtBufferAppend(&Request, (xbytesview){ (const uint8*)sBody, iBodyLen }) )
		goto done;

	/* ---- 连接 ---- */
	if ( Url.bTls ) {
		xtlsclientconfig Tls;
		xtlsdialconfig Dial;
		xrtTlsClientConfigInit(&Tls);
		Tls.Verifier = (xtlsverifier*)pHttp->pVerifier;
		xrtTlsDialConfigInit(&Dial);
		xoauth2__url_tls_names(&Url, &Tls, &Dial);
		Dial.Timeout = pHttp->uTimeoutUs;
		pFuture = xrtTlsDialAsync(pHttp->pEngine, pHttp->pResolver,
			Url.sHost, Url.iPort, &Tls, &Dial, NULL, NULL);
	}
	else {
		xnetdialconfig Dial;
		xrtNetDialConfigInit(&Dial);
		Dial.Timeout = pHttp->uTimeoutUs;
		pFuture = xrtNetDialAsync(pHttp->pEngine, pHttp->pResolver,
			Url.sHost, Url.iPort, &Dial, NULL, NULL);
	}
	if ( pFuture == NULL ) goto connect_fail;
	if ( xrtFutureWaitFor(pFuture, pHttp->uTimeoutUs) != XWAIT_OK ||
	     xrtFutureState(pFuture) != XFUTURE_RESOLVED )
		goto connect_fail;
	if ( Url.bTls ) {
		Stream.pTls = xrtTlsStreamRef((xtlsstream*)xrtFutureValue(pFuture));
		if ( Stream.pTls == NULL ) goto connect_fail;
	}
	else {
		Stream.pTcp = xrtNetStreamRef((xnetstream*)xrtFutureValue(pFuture));
		if ( Stream.pTcp == NULL ) goto connect_fail;
	}
	xrtFutureDestroy(pFuture);
	pFuture = NULL;

	/* ---- 发送 ---- */
	if ( !stream_send_all(&Stream, Request.Data, Request.Size, pHttp->uTimeoutUs) ) {
		transport_error("http transport send failed");
		goto done;
	}
	ResponseDeadline = xrtDeadlineAfter(pHttp->uTimeoutUs);

	/* ---- 接收头 ---- */
	xrtHttp1LimitsInit(&Limits);
	Limits.MaxFields = XOAUTH2_HTTP_FIELD_MAX;
	memset(&Head, 0, sizeof(Head));
	Head.Fields = Fields;
	Head.FieldCapacity = XOAUTH2_HTTP_FIELD_MAX;
	while ( !bHeadDone ) {
		xhttp1status eStatus = xrtHttp1ResponseParse(
			xrtBufferView(&Received), &Head, &Limits, &ProtocolError);
		if ( eStatus == XHTTP1_READY ) { bHeadDone = true; break; }
		if ( eStatus != XHTTP1_MORE ) {
			xoauth2__error(XOAUTH2_ERROR_NETWORK,
				"http transport response head invalid");
			goto done;
		}
		{
			int iGot = stream_recv(&Stream, Chunk, sizeof(Chunk),
				&iUsed, ResponseDeadline);
			if ( iGot < 0 ) {
				transport_error(
					iGot == -1 ? "http transport response head timeout" :
					"http transport response head read failed");
				goto done;
			}
			if ( iGot == 0 ) {
				transport_error(
					"http transport response head truncated");
				goto done;
			}
			if ( !xrtBufferAppend(&Received, (xbytesview){ Chunk, iUsed }) )
				goto done;
		}
	}
	*piStatus = (int)Head.Status;

	/* ---- 接收体 ---- */
	if ( !xrtHttp1ResponseBodyPlan(
		&Head, (xstrview){ sMethod, strlen(sMethod) }, &Plan) ) {
		xoauth2__error(XOAUTH2_ERROR_NETWORK,
			"http transport body plan failed");
		goto done;
	}
	xrtHttp1BodyLimitsInit(&BodyLimits);
	BodyLimits.MaxBody = XOAUTH2_HTTP_MAX_BODY;   /* 默认 UINT64_MAX，收紧 */
	BodyLimits.MaxTrailers = XOAUTH2_HTTP_FIELD_MAX;
	/* Body Plan 已保存分帧事实，Header 字段不再使用，存储可复用于 trailer。 */
	if ( !xrtHttp1BodyInit(&Body, &Plan, Fields, XOAUTH2_HTTP_FIELD_MAX, &BodyLimits) ) {
		xoauth2__error(XOAUTH2_ERROR_NETWORK,
			"http transport response body invalid");
		goto done;
	}
	iUsed = Head.Bytes;
	for ( ;; ) {
		xbytesview Data;
		eBody = xrtHttp1BodyRead(&Body,
			(xbytesview){ (uint8*)Received.Data + iUsed, Received.Size - iUsed },
			bStreamEnd, &iConsumed, &Data, &ProtocolError);
		iUsed += iConsumed;
		if ( eBody == XHTTP1_BODY_ERROR || eBody == XHTTP1_BODY_FIELDS ) {
			xoauth2__error(XOAUTH2_ERROR_NETWORK,
				"http transport response body invalid");
			goto done;
		}
		if ( eBody == XHTTP1_BODY_DATA ) {
			/* 回调交付 C 字符串，不能把含 NUL 的线格式正文悄然截成有效前缀。 */
			if ( Data.Size != 0 && memchr(Data.Data, 0, Data.Size) != NULL ) {
				xoauth2__error(XOAUTH2_ERROR_NETWORK,
				"http transport response body invalid");
				goto done;
			}
			if ( !xrtBufferAppend(&BodyBuffer, Data) ) goto done;
			continue;
		}
		if ( eBody == XHTTP1_BODY_DONE ) break;
		if ( bStreamEnd ) {
			xoauth2__error(XOAUTH2_ERROR_NETWORK,
				"http transport response body truncated");
			goto done;
		}
		{
			size_t iGot = 0;
			/* 借用正文已复制、Header 不再使用，仅保留尚未消费的 trailer 等字节。 */
			if ( iUsed != 0 ) {
				if ( !xrtBufferRemove(&Received, 0, iUsed) ) goto done;
				iUsed = 0;
			}
			int iResult = stream_recv(&Stream, Chunk, sizeof(Chunk),
				&iGot, ResponseDeadline);
			if ( iResult < 0 ) {
				transport_error(
						iResult == -1 ? "http transport response body timeout" :
						"http transport response body read failed");
				goto done;
			}
			if ( iResult == 0 ) { bStreamEnd = true; continue; }
			if ( !xrtBufferAppend(&Received, (xbytesview){ Chunk, iGot }) )
				goto done;
		}
	}

	*psResponseBody = (char*)xrtMalloc(BodyBuffer.Size + 1);
	if ( *psResponseBody == NULL ) goto done;
	if ( BodyBuffer.Size > 0 )
		memcpy(*psResponseBody, BodyBuffer.Data, BodyBuffer.Size);
	(*psResponseBody)[BodyBuffer.Size] = '\0';
	bOk = true;
	goto done;

connect_fail:
	if ( pFuture != NULL ) {
		const xerror* pError = xrtFutureError(pFuture);
		if ( pError != NULL ) xrtSetError(pError);
	}
	transport_error("http transport connect failed");
done:
	if ( !bOk ) pFailure = xrtErrorRef(xrtGetError());
	if ( pFuture != NULL ) future_finish(pFuture, bOk);
	stream_close(&Stream, !bOk);
	xrtBufferUnit(&Request);
	xrtBufferUnit(&Received);
	xrtBufferUnit(&BodyBuffer);
	if ( !bOk ) {
		if ( *psResponseBody != NULL ) {
			xrtFree(*psResponseBody);
			*psResponseBody = NULL;
		}
		*piStatus = 0;
	}
	if ( pFailure != NULL ) xrtSetErrorTake(pFailure);
	return bOk;
}
#endif

#endif
