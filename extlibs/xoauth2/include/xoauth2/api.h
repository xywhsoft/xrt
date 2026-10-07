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

#include <xoauth2/features.h>
#include <xrt.h>

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

/* 自定义端点配置（授权/token/userinfo/JWKS URL 与 scope）。 */
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

/* 自定义端点配置零值。 */
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
/* 一键套用 Google 端点（授权码流程）。 */
XRT_API void xoauth2UseGoogle(xoauth2client* pClient, const char* id, const char* secret, const char* redirect);
/* 一键套用微信开放平台端点。 */
XRT_API void xoauth2UseWechat(xoauth2client* pClient, const char* appid, const char* appSecret, const char* redirect);
/* 一键套用 Microsoft/Azure AD 端点（含 tenant）。 */
XRT_API void xoauth2UseMicrosoft(xoauth2client* pClient, const char* id, const char* secret, const char* redirect, const char* tenant);
/* 套用自定义端点配置。 */
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

/* 用回调 code 换取令牌（含 state 校验与 PKCE）。 */
XRT_API xoauth2token* xoauth2CompleteLogin(xoauth2client* pClient,
                                   const char* sCode, const char* sState);
/* 释放令牌对象。 */
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
 * sCaPem 为 NULL 时用系统证书库，uTimeoutMs 为 0 时默认 15s。
 * 此超时分别约束连接、整次发送及完整响应，不会逐分片重置。
 * Init 仅用于首次或已成功清理的外壳；失败回滚至少有 30 秒预算，与请求
 * 时限分开。失败后仍须清理外壳，清理未完成时只能重试 Cleanup/Unit/Destroy。
 * 未交付的堆构造由 CleanupPending 清理；有待清理对象时新建私有引擎
 * 会先非阻塞轮询，仍未完成则拒绝，借用引擎不受该限制。 */
XRT_API bool xoauth2HttpXrtInit(xoauth2httpxrt* pHttp, void* pBorrowedEngine,
                        const char* sCaPem, int64 uTimeoutMs);
/* 同步清理直连传输（语义见上方 Cleanup 注释）。 */
XRT_API void xoauth2HttpXrtUnit(xoauth2httpxrt* pHttp);

/* 与请求串行调用；等待自建 engine 的异步 Close/Abort，最多 uTimeoutMs。
 * 成功释放内部资源（不释放句柄），重复调用或 NULL 均成功；不停止借用 engine。
 * 失败保留自建 engine 的拥有权，句柄仅可用于再次 Cleanup/Unit/Destroy。
 * 保留调用前的非空错误；需判断清理结果时使用此返回值。 */
XRT_API bool xoauth2HttpXrtCleanup(xoauth2httpxrt* pHttp);

/* 堆版本（opaque 结构无法栈上声明时用——脚本层/绑定层的正路；
 * Destroy 清理成功才释放整个句柄；失败保留句柄供重试。
 * 需要确认成功时先调用 Cleanup，成功后再 Destroy。参数语义与 Init 相同。 */
XRT_API xoauth2httpxrt* xoauth2HttpXrtCreate(void* pBorrowedEngine,
                                     const char* sCaPem, int64 uTimeoutMs);
/* 堆句柄销毁（先 Cleanup 成功再释放）。 */
XRT_API void            xoauth2HttpXrtDestroy(xoauth2httpxrt* pHttp);

/* 重试未交付失败堆构造的引擎退休；并发可用，入列不分配且不启动后台线程。
 * 宿主停止新调用、等待在途调用结束并清理已交付实例后，在退出/卸载前
 * 调用至 true；false 时保留库及运行环境并稍后重试。
 * uTimeoutMs == 0 为一次非阻塞轮询；非零为等待预算，退休 ERROR 提前结束。
 * true 表示队列及其他清理调用正在处理的对象全部释放；piPending 可空，
 * 非空时返回尚未完成数量。保留已有错误；无旧错误时报告退休错误或 XERR_TIMEOUT，
 * 非阻塞 BUSY 不制造错误。已交付句柄仍由 Cleanup/Unit/Destroy 清理。 */
XRT_API bool xoauth2HttpXrtCleanupPending(int64 uTimeoutMs, size_t* piPending);

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
