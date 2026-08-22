#ifndef XRT_HTTP_DIGEST_H
#define XRT_HTTP_DIGEST_H

#include <xrt/http_structured.h>



#if defined(XHTTP_FEATURE_HTTP_DIGEST) && \
	!defined(XHTTP_FEATURE_HTTP_STRUCTURED)
	#error "XRT HTTP digest fields require Structured Fields support"
#endif

#if defined(XHTTP_FEATURE_HTTP_DIGEST_WRITE) && \
	(!defined(XHTTP_FEATURE_HTTP_DIGEST) || \
	 !defined(XHTTP_FEATURE_HTTP_STRUCTURED_WRITE))
	#error "XRT HTTP digest writer requires digest parser and Structured Fields writer support"
#endif

#if defined(XHTTP_FEATURE_HTTP_DIGEST_SHA2) && \
	(!defined(XHTTP_FEATURE_HTTP_DIGEST_WRITE) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA256) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA512))
	#error "XRT HTTP SHA-2 digest support requires digest writer, SHA-256 and SHA-512"
#endif



#if defined(XHTTP_FEATURE_HTTP_DIGEST)

/* 摘要目标区分消息内容与完整选定表示。 */
typedef enum xhttpdigesttarget {
	XHTTP_DIGEST_CONTENT = 1,
	XHTTP_DIGEST_REPRESENTATION
} xhttpdigesttarget;



/* 摘要成员借用算法 key、Byte Sequence 和可扩展参数。 */
typedef struct xhttpdigest {
	xstrview Algorithm;
	xhttpstructuredbare Value;
	xstrview Parameters;
} xhttpdigest;



/* 偏好成员使用 0 到 10 的整数权重。 */
typedef struct xhttpdigestpreference {
	xstrview Algorithm;
	uint8 Weight;
	xstrview Parameters;
} xhttpdigestpreference;



/* 游标绑定首次迭代的来源、摘要目标和成员类型；仅通过初始化函数创建。 */
typedef struct xhttpdigestcursor {
	xhttpstructuredmapcursor Structured;
	uint8 State;
} xhttpdigestcursor;



XRT_EXTERN_C_BEGIN



/* 初始化摘要或摘要偏好游标。 */
XRT_API void xrtHttpDigestCursorInit(xhttpdigestcursor* pCursor);



/* 严格验证完整 Content-Digest 或 Repr-Digest 字段值。 */
XRT_API bool xrtHttpDigestValid(xstrview Value);



/* 迭代完整且不可变的摘要值；算法按首次位置输出，重复值取最后一次。 */
XRT_API xhttpnext xrtHttpDigestNext(
	xstrview Value,
	xhttpdigestcursor* pCursor,
	xhttpdigest* pDigest
);



/* 跨重复字段行迭代；字段数组、目标和借用内容在游标期间不可改变。 */
XRT_API xhttpnext xrtHttpDigestFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpdigesttarget Target,
	xhttpdigestcursor* pCursor,
	xhttpdigest* pDigest
);



/* 解码摘要；空输出查询长度，不附加零字节，长度输出允许未对齐。 */
XRT_API bool xrtHttpDigestRead(
	const xhttpdigest* pDigest,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 严格验证完整 Want-Content-Digest 或 Want-Repr-Digest 值。 */
XRT_API bool xrtHttpDigestPreferenceValid(xstrview Value);



/* 迭代完整且不可变的摘要偏好值，游标不可与摘要迭代混用。 */
XRT_API xhttpnext xrtHttpDigestPreferenceNext(
	xstrview Value,
	xhttpdigestcursor* pCursor,
	xhttpdigestpreference* pPreference
);



/* 跨重复摘要偏好字段行迭代。 */
XRT_API xhttpnext xrtHttpDigestPreferenceFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpdigesttarget Target,
	xhttpdigestcursor* pCursor,
	xhttpdigestpreference* pPreference
);



XRT_EXTERN_C_END

#endif



#if defined(XHTTP_FEATURE_HTTP_DIGEST_WRITE)

XRT_EXTERN_C_BEGIN



/* 写出一个算法的摘要 Dictionary；适合最常见的单算法路径。 */
XRT_API bool xrtHttpDigestWrite(
	xstrview Algorithm,
	xbytesview Digest,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 写出一个算法的偏好 Dictionary，权重必须位于 0 到 10。 */
XRT_API bool xrtHttpDigestPreferenceWrite(
	xstrview Algorithm,
	uint8 iWeight,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



XRT_EXTERN_C_END

#endif



#if defined(XHTTP_FEATURE_HTTP_DIGEST_SHA2)

/* SHA-2 摘要验证明确区分不支持、内容不匹配和操作错误。 */
typedef enum xhttpdigestmatch {
	XHTTP_DIGEST_MATCH_ERROR = -1,
	XHTTP_DIGEST_MATCH_UNSUPPORTED = 0,
	XHTTP_DIGEST_MATCH_MISMATCH = 1,
	XHTTP_DIGEST_MATCH_OK = 2
} xhttpdigestmatch;



XRT_EXTERN_C_BEGIN



/* 计算连续内容的 SHA-256 并写出 Dictionary；长度输出允许未对齐。 */
XRT_API bool xrtHttpDigestSha256Write(
	const void* pData,
	size_t iSize,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 计算连续内容的 SHA-512 并写出 Dictionary；长度输出允许未对齐。 */
XRT_API bool xrtHttpDigestSha512Write(
	const void* pData,
	size_t iSize,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 按成员算法常量时间校验；未知算法不解码载荷并返回 UNSUPPORTED。 */
XRT_API xhttpdigestmatch xrtHttpDigestSha2Verify(
	const xhttpdigest* pDigest,
	const void* pData,
	size_t iSize
);



XRT_EXTERN_C_END

#endif

#endif
