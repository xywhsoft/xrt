#ifndef XHTTP_POLICY_HTTP_AUTH_POLICY_H
#define XHTTP_POLICY_HTTP_AUTH_POLICY_H

#include <xhttp-policy/export.h>

#include <xrt/http_auth.h>
#include <xrt/crypto.h>
#include <xrt/codec.h>
#include <xrt/sync.h>
#include <xrt/random.h>

XRT_EXTERN_C_BEGIN

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_REPLAY) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_MD5)
	#include <xrt/crypto.h>
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_REPLAY) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION)
	#include <xrt/sync.h>
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE_RANDOM)
	#include <xrt/random.h>
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_MD5) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2) || \
	 !defined(XRT_FEATURE_CRYPTO_MD5))
	#error "XRT HTTP Digest MD5 requires Digest SHA-2 and MD5 support"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE_RANDOM) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE) || \
	 !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XRT HTTP Digest random nonce requires nonce and secure random support"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT) || \
	 !defined(XRT_FEATURE_MUTEX))
	#error "XRT HTTP Digest session support requires Digest client and mutex support"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2)

/*
	Secret 是规范十六进制 H(username:realm:password)，可直接持久化。
	EntityHash 仅供 auth-int 使用，并由流式或一次性摘要入口预先计算。
*/
typedef struct xhttpdigestproof {
	xhttpdigestalgorithm Algorithm;
	xhttpdigestqop Qop;
	uint32 NonceCount;
	xstrview Secret;
	xstrview Nonce;
	xstrview Cnonce;
	xstrview Uri;
	xstrview EntityHash;
} xhttpdigestproof;

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE)

#define XHTTP_DIGEST_NONCE_KEY_MIN 32u
#define XHTTP_DIGEST_NONCE_SALT_SIZE 16u
#define XHTTP_DIGEST_NONCE_TEXT_SIZE 76u



/* 无状态 nonce 验证结果；INVALID 不表示库调用失败。 */
typedef enum xhttpdigestnoncecheck {
	XHTTP_DIGEST_NONCE_ERROR = -1,
	XHTTP_DIGEST_NONCE_INVALID = 0,
	XHTTP_DIGEST_NONCE_VALID,
	XHTTP_DIGEST_NONCE_STALE
} xhttpdigestnoncecheck;

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_REPLAY)

#define XHTTP_DIGEST_REPLAY_KEY_SIZE XRT_SHA256_SIZE



/* 重放键是带域分隔和长度边界的 SHA-256 摘要，可直接交给外部原子存储。 */
typedef struct xhttpdigestreplaykey {
	uint8 Bytes[XHTTP_DIGEST_REPLAY_KEY_SIZE];
} xhttpdigestreplaykey;



/* 内置重放表按分片设置硬容量；LifetimeSeconds 必须与 nonce 策略一致。 */
typedef struct xhttpdigestreplayconfig {
	size_t Shards;
	size_t EntriesPerShard;
	int64 LifetimeSeconds;
} xhttpdigestreplayconfig;



/* 重放检查的策略结果；只有 ERROR 表示库调用失败。 */
typedef enum xhttpdigestreplaycheck {
	XHTTP_DIGEST_REPLAY_ERROR = -1,
	XHTTP_DIGEST_REPLAY_REPLAY = 0,
	XHTTP_DIGEST_REPLAY_ACCEPTED,
	XHTTP_DIGEST_REPLAY_EXPIRED,
	XHTTP_DIGEST_REPLAY_FULL
} xhttpdigestreplaycheck;



/* 统计按分片锁取得一致快照，计数达到 uint64 上限后保持饱和。 */
typedef struct xhttpdigestreplaystats {
	size_t Entries;
	size_t Capacity;
	uint64 Accepted;
	uint64 Replayed;
	uint64 Expired;
	uint64 Full;
	uint64 Purged;
} xhttpdigestreplaystats;



/* DigestReplay 是线程安全、有硬容量边界的进程内 nonce-count 存储。 */
typedef struct xhttpdigestreplay xhttpdigestreplay;

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_VERIFY)

#define XHTTP_DIGEST_VERIFY_REQUIRE_USERHASH UINT32_C(0x00000001)



/* 证明验证结果是普通协议决策；只有 ERROR 表示库调用失败。 */
typedef enum xhttpdigestverifycheck {
	XHTTP_DIGEST_VERIFY_ERROR = -1,
	XHTTP_DIGEST_VERIFY_INVALID = 0,
	XHTTP_DIGEST_VERIFY_VALID,
	XHTTP_DIGEST_VERIFY_STALE
} xhttpdigestverifycheck;



/*
	验证描述符把客户端凭据绑定到实际 challenge、请求行和已查得的 Secret。
	EntityHash 只在 qop=auth-int 时提供，所有文本和对象均由调用方借用。
*/
typedef struct xhttpdigestverification {
	uint32 Flags;
	const xhttpdigestauth* Auth;
	const xhttpdigestchallenge* Challenge;
	xstrview Secret;
	xstrview Method;
	xstrview RequestTarget;
	xstrview EntityHash;
} xhttpdigestverification;

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT)

#define XHTTP_DIGEST_ALGORITHMS_MD5 \
	UINT32_C(0x00000001)
#define XHTTP_DIGEST_ALGORITHMS_MD5_SESSION \
	UINT32_C(0x00000002)
#define XHTTP_DIGEST_ALGORITHMS_SHA256 \
	UINT32_C(0x00000004)
#define XHTTP_DIGEST_ALGORITHMS_SHA256_SESSION \
	UINT32_C(0x00000008)
#define XHTTP_DIGEST_ALGORITHMS_SHA512_256 \
	UINT32_C(0x00000010)
#define XHTTP_DIGEST_ALGORITHMS_SHA512_256_SESSION \
	UINT32_C(0x00000020)
#define XHTTP_DIGEST_ALGORITHMS_SHA2 \
	((uint32)XHTTP_DIGEST_ALGORITHMS_SHA256 | \
	 (uint32)XHTTP_DIGEST_ALGORITHMS_SHA256_SESSION | \
	 (uint32)XHTTP_DIGEST_ALGORITHMS_SHA512_256 | \
	 (uint32)XHTTP_DIGEST_ALGORITHMS_SHA512_256_SESSION)
#define XHTTP_DIGEST_ALGORITHMS_ALL \
	((uint32)XHTTP_DIGEST_ALGORITHMS_MD5 | \
	 (uint32)XHTTP_DIGEST_ALGORITHMS_MD5_SESSION | \
	 (uint32)XHTTP_DIGEST_ALGORITHMS_SHA2)

#define XHTTP_DIGEST_QOPS_AUTH \
	UINT32_C(0x00000001)
#define XHTTP_DIGEST_QOPS_AUTH_INT \
	UINT32_C(0x00000002)
#define XHTTP_DIGEST_QOPS_ALL \
	((uint32)XHTTP_DIGEST_QOPS_AUTH | \
	 (uint32)XHTTP_DIGEST_QOPS_AUTH_INT)

#define XHTTP_DIGEST_POLICY_PREFER_AUTH_INT \
	UINT32_C(0x00000001)
#define XHTTP_DIGEST_POLICY_PLAIN_USERNAME \
	UINT32_C(0x00000002)
#define XHTTP_DIGEST_POLICY_REQUIRE_USERHASH \
	UINT32_C(0x00000004)
#define XHTTP_DIGEST_POLICY_REQUIRE_UTF8 \
	UINT32_C(0x00000008)

#define XHTTP_DIGEST_CLIENT_USERNAME_EXTENDED \
	UINT32_C(0x00000001)



/* 客户端协商结果区分普通策略拒绝与库调用错误。 */
typedef enum xhttpdigestchoosecheck {
	XHTTP_DIGEST_CHOOSE_ERROR = -1,
	XHTTP_DIGEST_CHOOSE_REJECTED = 0,
	XHTTP_DIGEST_CHOOSE_ACCEPTED
} xhttpdigestchoosecheck;



/* 响应证明结果区分认证不匹配与库调用错误。 */
typedef enum xhttpdigestinfocheck {
	XHTTP_DIGEST_INFO_ERROR = -1,
	XHTTP_DIGEST_INFO_INVALID = 0,
	XHTTP_DIGEST_INFO_VALID
} xhttpdigestinfocheck;



/* 算法和 qop 是允许集合；Flags 只控制客户端本地偏好。 */
typedef struct xhttpdigestpolicy {
	uint32 Flags;
	uint32 Algorithms;
	uint32 Qops;
} xhttpdigestpolicy;



/* 选择结果固定 challenge 算法、单个 qop 和用户名传输方式。 */
typedef struct xhttpdigestchoice {
	xhttpdigestalgorithm Algorithm;
	xhttpdigestqop Qop;
	bool UserHash;
} xhttpdigestchoice;



/*
	客户端凭据输入只借用调用方数据；Secret 是规范十六进制 H(A1)。
	EntityHash 只在 qop=auth-int 时提供，UsernameLanguage 只供 username* 使用。
*/
typedef struct xhttpdigestclientauth {
	uint32 Flags;
	const xhttpdigestchallenge* Challenge;
	const xhttpdigestchoice* Choice;
	xstrview Username;
	xstrview UsernameLanguage;
	xstrview Secret;
	xstrview Method;
	xstrview RequestTarget;
	xstrview Cnonce;
	xstrview EntityHash;
	uint32 NonceCount;
} xhttpdigestclientauth;



/*
	响应证明输入必须保留实际请求使用的 Proof；ResponseEntityHash 只供 auth-int。
	Info 和 Proof 由调用方借用，允许验证乱序完成的并发请求。
*/
typedef struct xhttpdigestinfoverification {
	const xhttpdigestinfo* Info;
	const xhttpdigestproof* Proof;
	xstrview ResponseEntityHash;
} xhttpdigestinfoverification;

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION)

/* 会话拥有认证状态；Exchange 固定一次请求使用的 nonce-count 与响应证明。 */
typedef struct xhttpdigestsession xhttpdigestsession;
typedef struct xhttpdigestexchange xhttpdigestexchange;



/* VALID 只表示响应证明有效；UPDATED 表示 nextnonce 已原子采用。 */
typedef enum xhttpdigestsessioncheck {
	XHTTP_DIGEST_SESSION_ERROR = -1,
	XHTTP_DIGEST_SESSION_INVALID = 0,
	XHTTP_DIGEST_SESSION_VALID,
	XHTTP_DIGEST_SESSION_UPDATED,
	XHTTP_DIGEST_SESSION_SUPERSEDED
} xhttpdigestsessioncheck;



/* 创建和更新时复制全部字段；Flags 使用 XHTTP_DIGEST_CLIENT_*。 */
typedef struct xhttpdigestsessionconfig {
	uint32 Flags;
	const xhttpdigestchallenge* Challenge;
	const xhttpdigestchoice* Choice;
	xstrview Username;
	xstrview UsernameLanguage;
	xstrview Secret;
	xstrview Cnonce;
} xhttpdigestsessionconfig;

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2)

/* 使用所选 Digest 算法散列任意连续数据，并写出规范小写十六进制。 */
XHTTP_POLICY_API bool xrtHttpDigestHash(
	xhttpdigestalgorithm Algorithm,
	const void* pData,
	size_t iDataSize,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 计算可持久化的 H(username:realm:password)；session 算法仍返回基础值。 */
XHTTP_POLICY_API bool xrtHttpDigestSecret(
	xhttpdigestalgorithm Algorithm,
	xstrview Username,
	xstrview Realm,
	xstrview Password,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 计算 RFC 7616 userhash，即 H(username:realm)。 */
XHTTP_POLICY_API bool xrtHttpDigestUserHash(
	xhttpdigestalgorithm Algorithm,
	xstrview Username,
	xstrview Realm,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 计算 Authorization 中的 request-digest；Method 使用请求线路中的原始方法。 */
XHTTP_POLICY_API bool xrtHttpDigestRequest(
	const xhttpdigestproof* pProof,
	xstrview Method,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 计算 Authentication-Info 中的 rspauth。 */
XHTTP_POLICY_API bool xrtHttpDigestRspAuth(
	const xhttpdigestproof* pProof,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 不修改错误槽地常量时间比较两个大小写不敏感的十六进制摘要。 */
XHTTP_POLICY_API bool xrtHttpDigestEqual(xstrview Left, xstrview Right);

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE)

/*
	从 Unix 秒、调用方 salt 和上下文构建 HMAC-SHA256 无状态 nonce。
	Key 至少 32 字节；输出是不补等号且不补零的固定 76 字节 Base64URL。
*/
XHTTP_POLICY_API bool xrtHttpDigestNonceWrite(
	xbytesview Key,
	xbytesview Context,
	int64 iIssuedSeconds,
	const void* pSalt,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/*
	验证无状态 nonce 的签名、上下文和时间窗口。
	无效线路值、签名或超出未来容差返回 INVALID；合法过期值返回 STALE。
*/
XHTTP_POLICY_API xhttpdigestnoncecheck xrtHttpDigestNonceVerify(
	xstrview Nonce,
	xbytesview Key,
	xbytesview Context,
	int64 iNowSeconds,
	int64 iLifetimeSeconds,
	int64 iFutureSkewSeconds,
	int64* pIssuedSeconds
);

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE_RANDOM)

/* 使用操作系统安全随机源生成 salt，再构建无状态 nonce。 */
XHTTP_POLICY_API bool xrtHttpDigestNonceCreate(
	xbytesview Key,
	xbytesview Context,
	int64 iIssuedSeconds,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_VERIFY)

/*
	验证 challenge、请求目标和 request-digest，不检查 nonce 签名或时效。
	该入口适合自定义 nonce、轮换 key-ring 或外部认证策略。
*/
XHTTP_POLICY_API xhttpdigestverifycheck xrtHttpDigestProofVerify(
	const xhttpdigestverification* pVerification
);



/*
	验证内置无状态 nonce 与 request-digest；过期 nonce 仅在证明有效时返回 STALE。
	IssuedSeconds 只在 VALID 或 STALE 时写入，可以为空。
*/
XHTTP_POLICY_API xhttpdigestverifycheck xrtHttpDigestVerify(
	const xhttpdigestverification* pVerification,
	xbytesview NonceKey,
	xbytesview NonceContext,
	int64 iNowSeconds,
	int64 iLifetimeSeconds,
	int64 iFutureSkewSeconds,
	int64* pIssuedSeconds
);

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT)

/* 初始化现代默认策略：SHA-2、auth/auth-int、优先 auth、遵循 userhash。 */
XHTTP_POLICY_API void xrtHttpDigestPolicyInit(
	xhttpdigestpolicy* pPolicy
);



/*
	按本地策略判断一份已解析 challenge；空 Policy 使用现代默认策略。
	普通不支持或策略拒绝返回 REJECTED，不修改线程错误槽。
*/
XHTTP_POLICY_API xhttpdigestchoosecheck xrtHttpDigestChallengeChoose(
	const xhttpdigestchallenge* pChallenge,
	const xhttpdigestpolicy* pPolicy,
	xhttpdigestchoice* pChoice
);



/*
	构造完整 Digest 凭据；查询模式返回用户名摘要和 response 的合计字节数。
	实际输出中的所有新视图借用 Output，其余视图借用输入和 challenge。
*/
XHTTP_POLICY_API bool xrtHttpDigestClientAuth(
	const xhttpdigestclientauth* pInput,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestauth* pAuth
);



/*
	校验 Authentication-Info 的 rspauth、qop、cnonce 和 nc。
	缺少 rspauth 或任何上下文不匹配均返回 INVALID，不发布 nextnonce 决策。
*/
XHTTP_POLICY_API xhttpdigestinfocheck xrtHttpDigestInfoVerify(
	const xhttpdigestinfoverification* pVerification
);

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION)

/* 创建并复制 Digest 客户端状态；首个请求的 nonce-count 为 1。 */
XHTTP_POLICY_API xhttpdigestsession* xrtHttpDigestSessionCreate(
	const xhttpdigestsessionconfig* pConfig
);



/* 增加会话引用；Exchange 会自动持有创建它的会话。 */
XHTTP_POLICY_API xhttpdigestsession* xrtHttpDigestSessionRetain(
	xhttpdigestsession* pSession
);



/* 释放会话引用；空指针是安全空操作。 */
XHTTP_POLICY_API void xrtHttpDigestSessionRelease(
	xhttpdigestsession* pSession
);



/* 原子替换 challenge、选择结果与凭据，并把 nonce-count 重置为 1。 */
XHTTP_POLICY_API bool xrtHttpDigestSessionUpdate(
	xhttpdigestsession* pSession,
	const xhttpdigestsessionconfig* pConfig
);



/* 为一次请求保留唯一 nonce-count；返回对象可跨线程等待乱序响应。 */
XHTTP_POLICY_API xhttpdigestexchange* xrtHttpDigestSessionAuthorize(
	xhttpdigestsession* pSession,
	xstrview Method,
	xstrview RequestTarget,
	xstrview EntityHash
);



/* 增加请求 Exchange 引用。 */
XHTTP_POLICY_API xhttpdigestexchange* xrtHttpDigestExchangeRetain(
	xhttpdigestexchange* pExchange
);



/* 释放 Exchange 及其持有的会话、状态和敏感数据。 */
XHTTP_POLICY_API void xrtHttpDigestExchangeRelease(
	xhttpdigestexchange* pExchange
);



/* 返回 Exchange 生命周期内稳定的 Authorization 协议对象。 */
XHTTP_POLICY_API const xhttpdigestauth* xrtHttpDigestExchangeAuth(
	const xhttpdigestexchange* pExchange
);



/* 返回 Exchange 生命周期内稳定的响应验证 proof。 */
XHTTP_POLICY_API const xhttpdigestproof* xrtHttpDigestExchangeProof(
	const xhttpdigestexchange* pExchange
);



/*
	验证 Authentication-Info，并在证明有效时原子采用 nextnonce。
	NextCnonce 为空时沿用当前 cnonce；旧 Exchange 的 nextnonce 返回 SUPERSEDED。
*/
XHTTP_POLICY_API xhttpdigestsessioncheck xrtHttpDigestSessionAccept(
	xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	const xhttpdigestinfo* pInfo,
	xstrview ResponseEntityHash,
	xstrview NextCnonce
);

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_REPLAY)

/* 初始化适合普通多线程 HTTP 服务的有界默认配置。 */
XHTTP_POLICY_API void xrtHttpDigestReplayConfigInit(
	xhttpdigestreplayconfig* pConfig
);



/* 从用户名、nonce 和 cnonce 构建无歧义的固定重放键。 */
XHTTP_POLICY_API bool xrtHttpDigestReplayKey(
	xstrview Username,
	xstrview Nonce,
	xstrview Cnonce,
	xhttpdigestreplaykey* pKey
);



/* 创建并发安全的内置重放表；配置为空时使用默认值。 */
XHTTP_POLICY_API xhttpdigestreplay* xrtHttpDigestReplayCreate(
	const xhttpdigestreplayconfig* pConfig
);



/* 销毁重放表；空指针是安全空操作，不能与其他操作并发。 */
XHTTP_POLICY_API void xrtHttpDigestReplayDestroy(
	xhttpdigestreplay* pReplay
);



/* 按预先派生的键原子检查并推进最大 nonce-count。 */
XHTTP_POLICY_API xhttpdigestreplaycheck xrtHttpDigestReplayCheckKey(
	xhttpdigestreplay* pReplay,
	const xhttpdigestreplaykey* pKey,
	uint32 iNonceCount,
	int64 iIssuedSeconds,
	int64 iNowSeconds
);



/* 派生键并原子检查，适合已经验证 Digest 证明的普通服务端路径。 */
XHTTP_POLICY_API xhttpdigestreplaycheck xrtHttpDigestReplayCheck(
	xhttpdigestreplay* pReplay,
	xstrview Username,
	xstrview Nonce,
	xstrview Cnonce,
	uint32 iNonceCount,
	int64 iIssuedSeconds,
	int64 iNowSeconds
);



/* 删除指定时间已经超出 nonce 有效期的记录并返回删除数量。 */
XHTTP_POLICY_API size_t xrtHttpDigestReplayPurge(
	xhttpdigestreplay* pReplay,
	int64 iNowSeconds
);



/* 清空全部重放记录并保留映射、堆和统计计数。 */
XHTTP_POLICY_API void xrtHttpDigestReplayClear(
	xhttpdigestreplay* pReplay
);



/* 取得当前容量、条目和累计策略结果；输出可位于合法未对齐存储。 */
XHTTP_POLICY_API bool xrtHttpDigestReplayStats(
	xhttpdigestreplay* pReplay,
	xhttpdigestreplaystats* pStats
);

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA256) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA512_256))
	#error "XRT HTTP Digest SHA-2 requires Digest, SHA-256 and SHA-512/256"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST) || \
	 !defined(XRT_FEATURE_CODEC_BASE64) || \
	 !defined(XRT_FEATURE_CRYPTO_HMAC_SHA256))
	#error "XRT HTTP Digest nonce requires Digest, Base64 and HMAC-SHA256"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_REPLAY) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA256) || \
	 !defined(XRT_FEATURE_MUTEX))
	#error "XRT HTTP Digest replay protection requires Digest, SHA-256 and mutex support"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_VERIFY) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE))
	#error "XRT HTTP Digest verification requires challenge, credentials, SHA-2 and nonce support"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_INFO) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2))
	#error "XRT HTTP Digest client support requires challenge, credentials, info and SHA-2 support"
#endif

XRT_EXTERN_C_END

#endif
