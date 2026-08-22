#ifndef XRT_HTTP_AUTH_H
#define XRT_HTTP_AUTH_H

#include <xrt/http.h>

#if defined(XRT_FEATURE_HTTP_AUTH_BASIC) || \
	defined(XRT_FEATURE_HTTP_AUTH_DIGEST_NONCE)
	#include <xrt/codec.h>
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_BEARER_CHALLENGE)
	#include <xrt/url.h>
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_SHA2) || \
	defined(XRT_FEATURE_HTTP_AUTH_DIGEST_NONCE) || \
	defined(XRT_FEATURE_HTTP_AUTH_DIGEST_REPLAY) || \
	defined(XRT_FEATURE_HTTP_AUTH_DIGEST_MD5)
	#include <xrt/crypto.h>
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_REPLAY) || \
	defined(XRT_FEATURE_HTTP_AUTH_DIGEST_SESSION)
	#include <xrt/sync.h>
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_NONCE_RANDOM)
	#include <xrt/random.h>
#endif



#if defined(XRT_FEATURE_HTTP_AUTH) && \
	!defined(XRT_FEATURE_HTTP_PARAM)
	#error "XRT HTTP authentication support requires HTTP parameter support"
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_BASIC) && \
	(!defined(XRT_FEATURE_HTTP_AUTH) || \
	 !defined(XRT_FEATURE_CODEC_BASE64))
	#error "XRT HTTP Basic authentication requires authentication and Base64 support"
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_BEARER) && \
	!defined(XRT_FEATURE_HTTP_AUTH)
	#error "XRT HTTP Bearer authentication requires authentication support"
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_BEARER_CHALLENGE) && \
	(!defined(XRT_FEATURE_HTTP_AUTH_BEARER) || \
	 !defined(XRT_FEATURE_URL))
	#error "XRT HTTP Bearer challenge support requires Bearer authentication and URL support"
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST) && \
	!defined(XRT_FEATURE_HTTP_AUTH)
	#error "XRT HTTP Digest metadata requires authentication support"
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE) && \
	!defined(XRT_FEATURE_HTTP_AUTH_DIGEST)
	#error "XRT HTTP Digest challenge support requires Digest metadata"
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS) && \
	(!defined(XRT_FEATURE_HTTP_AUTH_DIGEST) || \
	 !defined(XRT_FEATURE_HTTP_EXT_VALUE))
	#error "XRT HTTP Digest credentials require Digest metadata and extended values"
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_INFO) && \
	!defined(XRT_FEATURE_HTTP_AUTH_DIGEST)
	#error "XRT HTTP Digest Authentication-Info requires Digest metadata"
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_SHA2) && \
	(!defined(XRT_FEATURE_HTTP_AUTH_DIGEST) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA256) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA512_256))
	#error "XRT HTTP Digest SHA-2 requires Digest, SHA-256 and SHA-512/256"
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_MD5) && \
	(!defined(XRT_FEATURE_HTTP_AUTH_DIGEST_SHA2) || \
	 !defined(XRT_FEATURE_CRYPTO_MD5))
	#error "XRT HTTP Digest MD5 requires Digest SHA-2 and MD5 support"
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_NONCE) && \
	(!defined(XRT_FEATURE_HTTP_AUTH_DIGEST) || \
	 !defined(XRT_FEATURE_CODEC_BASE64) || \
	 !defined(XRT_FEATURE_CRYPTO_HMAC_SHA256))
	#error "XRT HTTP Digest nonce requires Digest, Base64 and HMAC-SHA256"
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_NONCE_RANDOM) && \
	(!defined(XRT_FEATURE_HTTP_AUTH_DIGEST_NONCE) || \
	 !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XRT HTTP Digest random nonce requires nonce and secure random support"
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_REPLAY) && \
	(!defined(XRT_FEATURE_HTTP_AUTH_DIGEST) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA256) || \
	 !defined(XRT_FEATURE_MUTEX))
	#error "XRT HTTP Digest replay protection requires Digest, SHA-256 and mutex support"
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_VERIFY) && \
	(!defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE) || \
	 !defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS) || \
	 !defined(XRT_FEATURE_HTTP_AUTH_DIGEST_SHA2) || \
	 !defined(XRT_FEATURE_HTTP_AUTH_DIGEST_NONCE))
	#error "XRT HTTP Digest verification requires challenge, credentials, SHA-2 and nonce support"
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CLIENT) && \
	(!defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE) || \
	 !defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS) || \
	 !defined(XRT_FEATURE_HTTP_AUTH_DIGEST_INFO) || \
	 !defined(XRT_FEATURE_HTTP_AUTH_DIGEST_SHA2))
	#error "XRT HTTP Digest client support requires challenge, credentials, info and SHA-2 support"
#endif

#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_SESSION) && \
	(!defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CLIENT) || \
	 !defined(XRT_FEATURE_MUTEX))
	#error "XRT HTTP Digest session support requires Digest client and mutex support"
#endif



#if defined(XRT_FEATURE_HTTP_AUTH)

/* 认证数据区分空数据、token68 和逗号分隔 auth-param。 */
typedef enum xhttpauthkind {
	XHTTP_AUTH_NONE = 0,
	XHTTP_AUTH_TOKEN68,
	XHTTP_AUTH_PARAMS
} xhttpauthkind;



/* 认证视图完全借用原字段值，Scheme 按 ASCII 大小写不敏感。 */
typedef struct xhttpauth {
	xstrview Scheme;
	xstrview Data;
	xhttpauthkind Kind;
} xhttpauth;



/* challenge 游标同时记录字段位置和当前字段值内的位置。 */
typedef struct xhttpauthcursor {
	size_t FieldIndex;
	size_t ValueOffset;
} xhttpauthcursor;

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_BASIC)

/* Basic 用户名和密码借用解码输出，均不包含分隔冒号。 */
typedef struct xhttpbasicauth {
	xstrview User;
	xstrview Password;
} xhttpbasicauth;



/* Basic challenge 的 realm 借用解码输出，Utf8 表示服务端声明 UTF-8。 */
typedef struct xhttpbasicchallenge {
	xstrview Realm;
	bool Utf8;
} xhttpbasicchallenge;

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_BEARER_CHALLENGE)

#define XHTTP_BEARER_HAS_REALM \
	UINT32_C(0x00000001)
#define XHTTP_BEARER_HAS_SCOPE \
	UINT32_C(0x00000002)
#define XHTTP_BEARER_HAS_ERROR \
	UINT32_C(0x00000004)
#define XHTTP_BEARER_HAS_ERROR_DESCRIPTION \
	UINT32_C(0x00000008)
#define XHTTP_BEARER_HAS_ERROR_URI \
	UINT32_C(0x00000010)



/* Bearer challenge 的标准参数；存在位区分缺失值与显式空 realm。 */
typedef struct xhttpbearerchallenge {
	uint32 Flags;
	xstrview Realm;
	xstrview Scope;
	xstrview Error;
	xstrview ErrorDescription;
	xstrview ErrorUri;
} xhttpbearerchallenge;

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST)

#define XRT_HTTP_DIGEST_MAX_SIZE 32u
#define XRT_HTTP_DIGEST_MAX_TEXT_SIZE 64u

/* Digest 算法包含 RFC 7616 的普通与 session 变体。 */
typedef enum xhttpdigestalgorithm {
	XHTTP_DIGEST_ALGORITHM_UNKNOWN = 0,
	XHTTP_DIGEST_ALGORITHM_MD5,
	XHTTP_DIGEST_ALGORITHM_MD5_SESSION,
	XHTTP_DIGEST_ALGORITHM_SHA256,
	XHTTP_DIGEST_ALGORITHM_SHA256_SESSION,
	XHTTP_DIGEST_ALGORITHM_SHA512_256,
	XHTTP_DIGEST_ALGORITHM_SHA512_256_SESSION
} xhttpdigestalgorithm;



/* Digest quality-of-protection；NONE 只表示无值或未知值。 */
typedef enum xhttpdigestqop {
	XHTTP_DIGEST_QOP_NONE = 0,
	XHTTP_DIGEST_QOP_AUTH,
	XHTTP_DIGEST_QOP_AUTH_INT
} xhttpdigestqop;

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_SHA2)

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



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_NONCE)

#define XRT_HTTP_DIGEST_NONCE_KEY_MIN 32u
#define XRT_HTTP_DIGEST_NONCE_SALT_SIZE 16u
#define XRT_HTTP_DIGEST_NONCE_TEXT_SIZE 76u



/* 无状态 nonce 验证结果；INVALID 不表示库调用失败。 */
typedef enum xhttpdigestnoncecheck {
	XHTTP_DIGEST_NONCE_ERROR = -1,
	XHTTP_DIGEST_NONCE_INVALID = 0,
	XHTTP_DIGEST_NONCE_VALID,
	XHTTP_DIGEST_NONCE_STALE
} xhttpdigestnoncecheck;

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_REPLAY)

#define XRT_HTTP_DIGEST_REPLAY_KEY_SIZE XRT_SHA256_SIZE



/* 重放键是带域分隔和长度边界的 SHA-256 摘要，可直接交给外部原子存储。 */
typedef struct xhttpdigestreplaykey {
	uint8 Bytes[XRT_HTTP_DIGEST_REPLAY_KEY_SIZE];
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



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE)

#define XHTTP_DIGEST_CHALLENGE_HAS_DOMAIN \
	UINT32_C(0x00000001)
#define XHTTP_DIGEST_CHALLENGE_HAS_OPAQUE \
	UINT32_C(0x00000002)
#define XHTTP_DIGEST_CHALLENGE_HAS_STALE \
	UINT32_C(0x00000004)
#define XHTTP_DIGEST_CHALLENGE_STALE \
	UINT32_C(0x00000008)
#define XHTTP_DIGEST_CHALLENGE_UTF8 \
	UINT32_C(0x00000010)
#define XHTTP_DIGEST_CHALLENGE_HAS_USERHASH \
	UINT32_C(0x00000020)
#define XHTTP_DIGEST_CHALLENGE_USERHASH \
	UINT32_C(0x00000040)
#define XHTTP_DIGEST_CHALLENGE_QOP_AUTH \
	UINT32_C(0x00000080)
#define XHTTP_DIGEST_CHALLENGE_QOP_AUTH_INT \
	UINT32_C(0x00000100)
#define XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT \
	UINT32_C(0x00000200)



/* Digest challenge 保存已解码标准值；未知 AlgorithmName 借用输入 token。 */
typedef struct xhttpdigestchallenge {
	uint32 Flags;
	xhttpdigestalgorithm Algorithm;
	xstrview Realm;
	xstrview Domain;
	xstrview Nonce;
	xstrview Opaque;
	xstrview AlgorithmName;
} xhttpdigestchallenge;

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS)

#define XHTTP_DIGEST_AUTH_USERNAME_EXTENDED \
	UINT32_C(0x00000001)
#define XHTTP_DIGEST_AUTH_HAS_OPAQUE \
	UINT32_C(0x00000002)
#define XHTTP_DIGEST_AUTH_HAS_USERHASH \
	UINT32_C(0x00000004)
#define XHTTP_DIGEST_AUTH_USERHASH \
	UINT32_C(0x00000008)
#define XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT \
	UINT32_C(0x00000010)



/*
	Digest 凭据保存解码后的标准值；UsernameLanguage 仅用于 username*。
	Response 保留十六进制文本，NonceCount 保存 nc 的数值形式。
*/
typedef struct xhttpdigestauth {
	uint32 Flags;
	xhttpdigestalgorithm Algorithm;
	xhttpdigestqop Qop;
	uint32 NonceCount;
	xstrview Username;
	xstrview UsernameLanguage;
	xstrview Realm;
	xstrview Nonce;
	xstrview Uri;
	xstrview Cnonce;
	xstrview Response;
	xstrview Opaque;
	xstrview AlgorithmName;
} xhttpdigestauth;

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_INFO)

#define XHTTP_DIGEST_INFO_HAS_NEXT_NONCE \
	UINT32_C(0x00000001)
#define XHTTP_DIGEST_INFO_HAS_RESPONSE \
	UINT32_C(0x00000002)



/* Algorithm 是请求上下文，不在线路中重复；其余值来自 Authentication-Info。 */
typedef struct xhttpdigestinfo {
	uint32 Flags;
	xhttpdigestalgorithm Algorithm;
	xhttpdigestqop Qop;
	uint32 NonceCount;
	xstrview NextNonce;
	xstrview Response;
	xstrview Cnonce;
} xhttpdigestinfo;

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_VERIFY)

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



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CLIENT)

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



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_SESSION)

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



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_AUTH)

/* 判断完整 token68；等号只允许末尾填充，谓词不修改线程错误。 */
XRT_API bool xrtHttpAuthToken68Valid(xstrview Text);



/* 读取 auth-param；输出可未对齐，无值或语法错误不推进游标并清空结果。 */
XRT_API xhttpnext xrtHttpAuthParamNext(
	xstrview Parameters,
	size_t* pOffset,
	xhttpparam* pParam
);



/* 迭代认证 challenge 列表；游标和结果可未对齐，错误不推进游标。 */
XRT_API xhttpnext xrtHttpChallengeNext(
	xstrview Challenges,
	size_t* pOffset,
	xhttpauth* pChallenge
);



/* 把 challenge 游标重置到字段列表起点；输出可未对齐。 */
XRT_API void xrtHttpAuthCursorInit(xhttpauthcursor* pCursor);



/* 跨同名字段迭代 challenge；字段、游标和结果可未对齐，错误不推进游标。 */
XRT_API xhttpnext xrtHttpFieldChallengeNext(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpauthcursor* pCursor,
	xhttpauth* pChallenge
);



/* 严格解析单份凭据；结果可未对齐，失败时清空合法的结果存储。 */
XRT_API bool xrtHttpAuthParse(
	xstrview Value,
	xhttpauth* pAuth
);



/* 写出 scheme 与可选认证数据；长度可未对齐，结果不附加零字符。 */
XRT_API bool xrtHttpAuthWrite(
	xstrview Scheme,
	xstrview Data,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾认证字段值；可选长度可未对齐，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpAuthBuild(
	xstrview Scheme,
	xstrview Data,
	size_t* pSize
);

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_BASIC)

/* 按原始 user:password 字节构建 Basic 字段值；长度输出可未对齐。 */
XRT_API bool xrtHttpBasicWrite(
	xstrview User,
	xstrview Password,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/*
	严格解码完整 Basic 字段值；空输出可验证并查询 user:password 字节数。
	实际成功时 Basic 的两个视图借用 Output，输出与输入不得重叠；
	固定描述符可未对齐，合法描述符在解析失败时得到空 Basic 结果。
*/
XRT_API bool xrtHttpBasicRead(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbasicauth* pBasic
);



/* 严格解析 Basic challenge；未知扩展参数被忽略，realm 借用 Output。 */
XRT_API bool xrtHttpBasicChallengeRead(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbasicchallenge* pChallenge
);



/* 写出带 quoted realm 和可选 UTF-8 声明的 Basic challenge；长度可未对齐。 */
XRT_API bool xrtHttpBasicChallengeWrite(
	xstrview Realm,
	bool bUtf8,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_BEARER)

/* 按 RFC 6750 b64token 规则判断 Bearer token。 */
XRT_API bool xrtHttpBearerTokenValid(xstrview Token);



/* 校验并写出完整 Bearer 字段值；结果不补零，长度输出可未对齐。 */
XRT_API bool xrtHttpBearerWrite(
	xstrview Token,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 严格解析完整 Bearer 字段值；结果可未对齐，Token 借用输入。 */
XRT_API bool xrtHttpBearerRead(
	xstrview Value,
	xstrview* pToken
);

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_BEARER_CHALLENGE)

/*
	解析一份 Bearer challenge；标准参数被解码到 Output，未知参数被接受。
	空输出执行完整验证和长度查询，结果只发布存在位；固定输出可未对齐。
*/
XRT_API bool xrtHttpBearerChallengeRead(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbearerchallenge* pChallenge
);



/*
	按固定顺序写出 Bearer 标准 challenge；至少设置一个参数存在位。
	需要扩展参数时使用通用 xrtHttpAuthWrite 构建完整参数列表。
*/
XRT_API bool xrtHttpBearerChallengeWrite(
	const xhttpbearerchallenge* pChallenge,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST)

/* 解析 RFC 7616 算法 token；未知或无效名称返回 UNKNOWN 且不修改错误槽。 */
XRT_API xhttpdigestalgorithm xrtHttpDigestAlgorithmParse(xstrview Name);



/* 返回算法的规范线路名称；UNKNOWN 返回空视图且不修改错误槽。 */
XRT_API xstrview xrtHttpDigestAlgorithmName(
	xhttpdigestalgorithm Algorithm
);



/* 返回二进制摘要长度；未知算法返回零。 */
XRT_API size_t xrtHttpDigestSize(xhttpdigestalgorithm Algorithm);



/* 判断算法是否是 -sess 变体。 */
XRT_API bool xrtHttpDigestAlgorithmSession(
	xhttpdigestalgorithm Algorithm
);



/* 判断当前裁剪闭包是否包含指定算法的计算后端。 */
XRT_API bool xrtHttpDigestAlgorithmSupported(
	xhttpdigestalgorithm Algorithm
);



/* 解析单个 qop token；未知或无效值返回 NONE。 */
XRT_API xhttpdigestqop xrtHttpDigestQopParse(xstrview Name);



/* 返回 qop 的规范线路名称；NONE 返回空视图。 */
XRT_API xstrview xrtHttpDigestQopName(xhttpdigestqop Qop);

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_SHA2)

/* 使用所选 Digest 算法散列任意连续数据，并写出规范小写十六进制。 */
XRT_API bool xrtHttpDigestHash(
	xhttpdigestalgorithm Algorithm,
	const void* pData,
	size_t iDataSize,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 计算可持久化的 H(username:realm:password)；session 算法仍返回基础值。 */
XRT_API bool xrtHttpDigestSecret(
	xhttpdigestalgorithm Algorithm,
	xstrview Username,
	xstrview Realm,
	xstrview Password,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 计算 RFC 7616 userhash，即 H(username:realm)。 */
XRT_API bool xrtHttpDigestUserHash(
	xhttpdigestalgorithm Algorithm,
	xstrview Username,
	xstrview Realm,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 计算 Authorization 中的 request-digest；Method 使用请求线路中的原始方法。 */
XRT_API bool xrtHttpDigestRequest(
	const xhttpdigestproof* pProof,
	xstrview Method,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 计算 Authentication-Info 中的 rspauth。 */
XRT_API bool xrtHttpDigestRspAuth(
	const xhttpdigestproof* pProof,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 不修改错误槽地常量时间比较两个大小写不敏感的十六进制摘要。 */
XRT_API bool xrtHttpDigestEqual(xstrview Left, xstrview Right);

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_NONCE)

/*
	从 Unix 秒、调用方 salt 和上下文构建 HMAC-SHA256 无状态 nonce。
	Key 至少 32 字节；输出是不补等号且不补零的固定 76 字节 Base64URL。
*/
XRT_API bool xrtHttpDigestNonceWrite(
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
XRT_API xhttpdigestnoncecheck xrtHttpDigestNonceVerify(
	xstrview Nonce,
	xbytesview Key,
	xbytesview Context,
	int64 iNowSeconds,
	int64 iLifetimeSeconds,
	int64 iFutureSkewSeconds,
	int64* pIssuedSeconds
);

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_NONCE_RANDOM)

/* 使用操作系统安全随机源生成 salt，再构建无状态 nonce。 */
XRT_API bool xrtHttpDigestNonceCreate(
	xbytesview Key,
	xbytesview Context,
	int64 iIssuedSeconds,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_VERIFY)

/*
	验证 challenge、请求目标和 request-digest，不检查 nonce 签名或时效。
	该入口适合自定义 nonce、轮换 key-ring 或外部认证策略。
*/
XRT_API xhttpdigestverifycheck xrtHttpDigestProofVerify(
	const xhttpdigestverification* pVerification
);



/*
	验证内置无状态 nonce 与 request-digest；过期 nonce 仅在证明有效时返回 STALE。
	IssuedSeconds 只在 VALID 或 STALE 时写入，可以为空。
*/
XRT_API xhttpdigestverifycheck xrtHttpDigestVerify(
	const xhttpdigestverification* pVerification,
	xbytesview NonceKey,
	xbytesview NonceContext,
	int64 iNowSeconds,
	int64 iLifetimeSeconds,
	int64 iFutureSkewSeconds,
	int64* pIssuedSeconds
);

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CLIENT)

/* 初始化现代默认策略：SHA-2、auth/auth-int、优先 auth、遵循 userhash。 */
XRT_API void xrtHttpDigestPolicyInit(
	xhttpdigestpolicy* pPolicy
);



/*
	按本地策略判断一份已解析 challenge；空 Policy 使用现代默认策略。
	普通不支持或策略拒绝返回 REJECTED，不修改线程错误槽。
*/
XRT_API xhttpdigestchoosecheck xrtHttpDigestChallengeChoose(
	const xhttpdigestchallenge* pChallenge,
	const xhttpdigestpolicy* pPolicy,
	xhttpdigestchoice* pChoice
);



/*
	构造完整 Digest 凭据；查询模式返回用户名摘要和 response 的合计字节数。
	实际输出中的所有新视图借用 Output，其余视图借用输入和 challenge。
*/
XRT_API bool xrtHttpDigestClientAuth(
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
XRT_API xhttpdigestinfocheck xrtHttpDigestInfoVerify(
	const xhttpdigestinfoverification* pVerification
);

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_SESSION)

/* 创建并复制 Digest 客户端状态；首个请求的 nonce-count 为 1。 */
XRT_API xhttpdigestsession* xrtHttpDigestSessionCreate(
	const xhttpdigestsessionconfig* pConfig
);



/* 增加会话引用；Exchange 会自动持有创建它的会话。 */
XRT_API xhttpdigestsession* xrtHttpDigestSessionRetain(
	xhttpdigestsession* pSession
);



/* 释放会话引用；空指针是安全空操作。 */
XRT_API void xrtHttpDigestSessionRelease(
	xhttpdigestsession* pSession
);



/* 原子替换 challenge、选择结果与凭据，并把 nonce-count 重置为 1。 */
XRT_API bool xrtHttpDigestSessionUpdate(
	xhttpdigestsession* pSession,
	const xhttpdigestsessionconfig* pConfig
);



/* 为一次请求保留唯一 nonce-count；返回对象可跨线程等待乱序响应。 */
XRT_API xhttpdigestexchange* xrtHttpDigestSessionAuthorize(
	xhttpdigestsession* pSession,
	xstrview Method,
	xstrview RequestTarget,
	xstrview EntityHash
);



/* 增加请求 Exchange 引用。 */
XRT_API xhttpdigestexchange* xrtHttpDigestExchangeRetain(
	xhttpdigestexchange* pExchange
);



/* 释放 Exchange 及其持有的会话、状态和敏感数据。 */
XRT_API void xrtHttpDigestExchangeRelease(
	xhttpdigestexchange* pExchange
);



/* 返回 Exchange 生命周期内稳定的 Authorization 协议对象。 */
XRT_API const xhttpdigestauth* xrtHttpDigestExchangeAuth(
	const xhttpdigestexchange* pExchange
);



/* 返回 Exchange 生命周期内稳定的响应验证 proof。 */
XRT_API const xhttpdigestproof* xrtHttpDigestExchangeProof(
	const xhttpdigestexchange* pExchange
);



/*
	验证 Authentication-Info，并在证明有效时原子采用 nextnonce。
	NextCnonce 为空时沿用当前 cnonce；旧 Exchange 的 nextnonce 返回 SUPERSEDED。
*/
XRT_API xhttpdigestsessioncheck xrtHttpDigestSessionAccept(
	xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	const xhttpdigestinfo* pInfo,
	xstrview ResponseEntityHash,
	xstrview NextCnonce
);

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_REPLAY)

/* 初始化适合普通多线程 HTTP 服务的有界默认配置。 */
XRT_API void xrtHttpDigestReplayConfigInit(
	xhttpdigestreplayconfig* pConfig
);



/* 从用户名、nonce 和 cnonce 构建无歧义的固定重放键。 */
XRT_API bool xrtHttpDigestReplayKey(
	xstrview Username,
	xstrview Nonce,
	xstrview Cnonce,
	xhttpdigestreplaykey* pKey
);



/* 创建并发安全的内置重放表；配置为空时使用默认值。 */
XRT_API xhttpdigestreplay* xrtHttpDigestReplayCreate(
	const xhttpdigestreplayconfig* pConfig
);



/* 销毁重放表；空指针是安全空操作，不能与其他操作并发。 */
XRT_API void xrtHttpDigestReplayDestroy(
	xhttpdigestreplay* pReplay
);



/* 按预先派生的键原子检查并推进最大 nonce-count。 */
XRT_API xhttpdigestreplaycheck xrtHttpDigestReplayCheckKey(
	xhttpdigestreplay* pReplay,
	const xhttpdigestreplaykey* pKey,
	uint32 iNonceCount,
	int64 iIssuedSeconds,
	int64 iNowSeconds
);



/* 派生键并原子检查，适合已经验证 Digest 证明的普通服务端路径。 */
XRT_API xhttpdigestreplaycheck xrtHttpDigestReplayCheck(
	xhttpdigestreplay* pReplay,
	xstrview Username,
	xstrview Nonce,
	xstrview Cnonce,
	uint32 iNonceCount,
	int64 iIssuedSeconds,
	int64 iNowSeconds
);



/* 删除指定时间已经超出 nonce 有效期的记录并返回删除数量。 */
XRT_API size_t xrtHttpDigestReplayPurge(
	xhttpdigestreplay* pReplay,
	int64 iNowSeconds
);



/* 清空全部重放记录并保留映射、堆和统计计数。 */
XRT_API void xrtHttpDigestReplayClear(
	xhttpdigestreplay* pReplay
);



/* 取得当前容量、条目和累计策略结果；输出可位于合法未对齐存储。 */
XRT_API bool xrtHttpDigestReplayStats(
	xhttpdigestreplay* pReplay,
	xhttpdigestreplaystats* pStats
);

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE)

/*
	严格解析并解码一份 RFC 7616 Digest challenge。
	查询模式发布标志、算法和所需字节数；四个解码值视图保持为空。
	已知算法名使用静态规范视图，未知算法名始终借用输入字段值。
*/
XRT_API bool xrtHttpDigestChallengeRead(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge
);



/*
	按 RFC 7616 要求的 quoted/token 形式规范写出 Digest challenge。
	写出要求至少一个已知 qop；未知算法通过 AlgorithmName token 扩展。
*/
XRT_API bool xrtHttpDigestChallengeWrite(
	const xhttpdigestchallenge* pChallenge,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS)

/*
	严格解析完整 Digest Authorization 值；查询模式只发布标志、枚举和长度。
	实际模式把标准文本解码到 Output，未知 AlgorithmName 借用输入。
*/
XRT_API bool xrtHttpDigestAuthRead(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestauth* pDigest
);



/* 按 RFC 7616 的 quoted/token 线路形式规范写出完整 Digest 凭据。 */
XRT_API bool xrtHttpDigestAuthWrite(
	const xhttpdigestauth* pDigest,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_INFO)

/* 严格解析 Digest Authentication-Info 参数列表；Algorithm 由请求上下文提供。 */
XRT_API bool xrtHttpDigestInfoRead(
	xstrview Value,
	xhttpdigestalgorithm Algorithm,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestinfo* pInfo
);



/* 写出 Digest Authentication-Info 参数列表，不添加字段名或冒号。 */
XRT_API bool xrtHttpDigestInfoWrite(
	const xhttpdigestinfo* pInfo,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif
