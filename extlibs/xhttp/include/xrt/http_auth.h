#ifndef XRT_HTTP_AUTH_H
#define XRT_HTTP_AUTH_H

#include <xhttp/export.h>

#include <xrt/http.h>

#if defined(XHTTP_FEATURE_HTTP_AUTH_BASIC) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE)
	#include <xrt/codec.h>
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_BEARER_CHALLENGE)
	#include <xrt/url.h>
#endif






#if defined(XHTTP_FEATURE_HTTP_AUTH) && \
	!defined(XRT_FEATURE_HTTP_PARAM)
	#error "XRT HTTP authentication support requires HTTP parameter support"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_BASIC) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH) || \
	 !defined(XRT_FEATURE_CODEC_BASE64))
	#error "XRT HTTP Basic authentication requires authentication and Base64 support"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_BEARER) && \
	!defined(XHTTP_FEATURE_HTTP_AUTH)
	#error "XRT HTTP Bearer authentication requires authentication support"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_BEARER_CHALLENGE) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH_BEARER) || \
	 !defined(XHTTP_FEATURE_URL_PARAM))
	#error "XRT HTTP Bearer challenge support requires Bearer authentication and URL support"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST) && \
	!defined(XHTTP_FEATURE_HTTP_AUTH)
	#error "XRT HTTP Digest metadata requires authentication support"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE) && \
	!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST)
	#error "XRT HTTP Digest challenge support requires Digest metadata"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST) || \
	 !defined(XHTTP_FEATURE_HTTP_EXT_VALUE))
	#error "XRT HTTP Digest credentials require Digest metadata and extended values"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_INFO) && \
	!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST)
	#error "XRT HTTP Digest Authentication-Info requires Digest metadata"
#endif











#if defined(XHTTP_FEATURE_HTTP_AUTH)

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



#if defined(XHTTP_FEATURE_HTTP_AUTH_BASIC)

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



#if defined(XHTTP_FEATURE_HTTP_AUTH_BEARER_CHALLENGE)

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



#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST)

#define XHTTP_DIGEST_MAX_SIZE 32u
#define XHTTP_DIGEST_MAX_TEXT_SIZE 64u

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












#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE)

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



#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS)

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



#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_INFO)

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












XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_AUTH)

/* 判断完整 token68；等号只允许末尾填充，谓词不修改线程错误。 */
XHTTP_API bool xrtHttpAuthToken68Valid(xstrview Text);



/* 读取 auth-param；输出可未对齐，无值或语法错误不推进游标并清空结果。 */
XHTTP_API xhttpnext xrtHttpAuthParamNext(
	xstrview Parameters,
	size_t* pOffset,
	xhttpparam* pParam
);



/* 迭代认证 challenge 列表；游标和结果可未对齐，错误不推进游标。 */
XHTTP_API xhttpnext xrtHttpChallengeNext(
	xstrview Challenges,
	size_t* pOffset,
	xhttpauth* pChallenge
);



/* 把 challenge 游标重置到字段列表起点；输出可未对齐。 */
XHTTP_API void xrtHttpAuthCursorInit(xhttpauthcursor* pCursor);



/* 跨同名字段迭代 challenge；字段、游标和结果可未对齐，错误不推进游标。 */
XHTTP_API xhttpnext xrtHttpFieldChallengeNext(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpauthcursor* pCursor,
	xhttpauth* pChallenge
);



/* 严格解析单份凭据；结果可未对齐，失败时清空合法的结果存储。 */
XHTTP_API bool xrtHttpAuthParse(
	xstrview Value,
	xhttpauth* pAuth
);



/* 写出 scheme 与可选认证数据；长度可未对齐，结果不附加零字符。 */
XHTTP_API bool xrtHttpAuthWrite(
	xstrview Scheme,
	xstrview Data,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾认证字段值；可选长度可未对齐，返回值由 xrtFree 释放。 */
XHTTP_API str xrtHttpAuthBuild(
	xstrview Scheme,
	xstrview Data,
	size_t* pSize
);

#endif



#if defined(XHTTP_FEATURE_HTTP_AUTH_BASIC)

/* 按原始 user:password 字节构建 Basic 字段值；长度输出可未对齐。 */
XHTTP_API bool xrtHttpBasicWrite(
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
XHTTP_API bool xrtHttpBasicRead(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbasicauth* pBasic
);



/* 严格解析 Basic challenge；未知扩展参数被忽略，realm 借用 Output。 */
XHTTP_API bool xrtHttpBasicChallengeRead(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbasicchallenge* pChallenge
);



/* 写出带 quoted realm 和可选 UTF-8 声明的 Basic challenge；长度可未对齐。 */
XHTTP_API bool xrtHttpBasicChallengeWrite(
	xstrview Realm,
	bool bUtf8,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



#if defined(XHTTP_FEATURE_HTTP_AUTH_BEARER)

/* 按 RFC 6750 b64token 规则判断 Bearer token。 */
XHTTP_API bool xrtHttpBearerTokenValid(xstrview Token);



/* 校验并写出完整 Bearer 字段值；结果不补零，长度输出可未对齐。 */
XHTTP_API bool xrtHttpBearerWrite(
	xstrview Token,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 严格解析完整 Bearer 字段值；结果可未对齐，Token 借用输入。 */
XHTTP_API bool xrtHttpBearerRead(
	xstrview Value,
	xstrview* pToken
);

#endif



#if defined(XHTTP_FEATURE_HTTP_AUTH_BEARER_CHALLENGE)

/*
	解析一份 Bearer challenge；标准参数被解码到 Output，未知参数被接受。
	空输出执行完整验证和长度查询，结果只发布存在位；固定输出可未对齐。
*/
XHTTP_API bool xrtHttpBearerChallengeRead(
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
XHTTP_API bool xrtHttpBearerChallengeWrite(
	const xhttpbearerchallenge* pChallenge,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST)

/* 解析 RFC 7616 算法 token；未知或无效名称返回 UNKNOWN 且不修改错误槽。 */
XHTTP_API xhttpdigestalgorithm xrtHttpDigestAlgorithmParse(xstrview Name);



/* 返回算法的规范线路名称；UNKNOWN 返回空视图且不修改错误槽。 */
XHTTP_API xstrview xrtHttpDigestAlgorithmName(
	xhttpdigestalgorithm Algorithm
);



/* 返回二进制摘要长度；未知算法返回零。 */
XHTTP_API size_t xrtHttpDigestSize(xhttpdigestalgorithm Algorithm);



/* 判断算法是否是 -sess 变体。 */
XHTTP_API bool xrtHttpDigestAlgorithmSession(
	xhttpdigestalgorithm Algorithm
);



/* 判断当前裁剪闭包是否包含指定算法的计算后端。 */
XHTTP_API bool xrtHttpDigestAlgorithmSupported(
	xhttpdigestalgorithm Algorithm
);



/* 解析单个 qop token；未知或无效值返回 NONE。 */
XHTTP_API xhttpdigestqop xrtHttpDigestQopParse(xstrview Name);



/* 返回 qop 的规范线路名称；NONE 返回空视图。 */
XHTTP_API xstrview xrtHttpDigestQopName(xhttpdigestqop Qop);

#endif
























#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE)

/*
	严格解析并解码一份 RFC 7616 Digest challenge。
	查询模式发布标志、算法和所需字节数；四个解码值视图保持为空。
	已知算法名使用静态规范视图，未知算法名始终借用输入字段值。
*/
XHTTP_API bool xrtHttpDigestChallengeRead(
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
XHTTP_API bool xrtHttpDigestChallengeWrite(
	const xhttpdigestchallenge* pChallenge,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS)

/*
	严格解析完整 Digest Authorization 值；查询模式只发布标志、枚举和长度。
	实际模式把标准文本解码到 Output，未知 AlgorithmName 借用输入。
*/
XHTTP_API bool xrtHttpDigestAuthRead(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestauth* pDigest
);



/* 按 RFC 7616 的 quoted/token 线路形式规范写出完整 Digest 凭据。 */
XHTTP_API bool xrtHttpDigestAuthWrite(
	const xhttpdigestauth* pDigest,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_INFO)

/* 严格解析 Digest Authentication-Info 参数列表；Algorithm 由请求上下文提供。 */
XHTTP_API bool xrtHttpDigestInfoRead(
	xstrview Value,
	xhttpdigestalgorithm Algorithm,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestinfo* pInfo
);



/* 写出 Digest Authentication-Info 参数列表，不添加字段名或冒号。 */
XHTTP_API bool xrtHttpDigestInfoWrite(
	const xhttpdigestinfo* pInfo,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif
