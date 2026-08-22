#ifndef XRT_HTTP_CACHE_RANGE_H
#define XRT_HTTP_CACHE_RANGE_H

#include <xrt/http_cache_validate.h>



#if defined(XHTTP_FEATURE_HTTP_CACHE_RANGE) && \
	(!defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE) || \
	 !defined(XHTTP_FEATURE_HTTP_RANGE))
	#error "XRT HTTP cache range support requires cache validation and HTTP ranges"
#endif



#if defined(XHTTP_FEATURE_HTTP_CACHE_RANGE)

/* 片段输入标志描述传输完整性、multipart 层级和表示变换事实。 */
typedef enum xhttpcachefragmentinputflag {
	XHTTP_CACHE_FRAGMENT_INPUT_NONE = 0,
	XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE = UINT32_C(0x00000001),
	XHTTP_CACHE_FRAGMENT_BODY_COMPLETE = UINT32_C(0x00000002),
	XHTTP_CACHE_FRAGMENT_MULTIPART_PART = UINT32_C(0x00000004),
	XHTTP_CACHE_FRAGMENT_TRANSFORMED = UINT32_C(0x00000008)
} xhttpcachefragmentinputflag;



/*
	Fields 是响应 Header，RangeFields 是 multipart part Header。
	普通 206 的 RangeFields 为空时直接从 Fields 读取 Content-Range。
*/
typedef struct xhttpcachefragmentinput {
	xstrview Method;
	const xhttpfield* Fields;
	size_t FieldCount;
	const xhttpfield* RangeFields;
	size_t RangeFieldCount;
	xtime ResponseTime;
	uint64 BodySize;
	uint16 Status;
	uint32 Flags;
} xhttpcachefragmentinput;



/* 片段跳过原因可以组合，便于缓存统计和诊断保守拒绝。 */
typedef enum xhttpcachefragmentreason {
	XHTTP_CACHE_FRAGMENT_REASON_NONE = 0,
	XHTTP_CACHE_FRAGMENT_REASON_METHOD = UINT32_C(0x00000001),
	XHTTP_CACHE_FRAGMENT_REASON_STATUS = UINT32_C(0x00000002),
	XHTTP_CACHE_FRAGMENT_REASON_HEADERS = UINT32_C(0x00000004),
	XHTTP_CACHE_FRAGMENT_REASON_TRANSFORMED = UINT32_C(0x00000008),
	XHTTP_CACHE_FRAGMENT_REASON_CONTENT_RANGE = UINT32_C(0x00000010),
	XHTTP_CACHE_FRAGMENT_REASON_CONTENT_LENGTH = UINT32_C(0x00000020),
	XHTTP_CACHE_FRAGMENT_REASON_BODY_LENGTH = UINT32_C(0x00000040),
	XHTTP_CACHE_FRAGMENT_REASON_EMPTY = UINT32_C(0x00000080)
} xhttpcachefragmentreason;



/* 片段计划区分参数错误、保守跳过和可以保存。 */
typedef enum xhttpcachefragmentdecision {
	XHTTP_CACHE_FRAGMENT_ERROR = -1,
	XHTTP_CACHE_FRAGMENT_SKIP = 0,
	XHTTP_CACHE_FRAGMENT_STORE = 1
} xhttpcachefragmentdecision;



/* 片段标志区分正文区间和已知完整表示长度。 */
typedef enum xhttpcachefragmentflag {
	XHTTP_CACHE_FRAGMENT_NONE = 0,
	XHTTP_CACHE_FRAGMENT_HAS_RANGE = UINT32_C(0x00000001),
	XHTTP_CACHE_FRAGMENT_HAS_LENGTH = UINT32_C(0x00000002)
} xhttpcachefragmentflag;



/*
	片段借用响应 Header；Range 是已经收到的实际字节闭区间。
	SourceStatus 保留 Header 来源是 200 还是 206，存储状态统一按 200 处理。
*/
typedef struct xhttpcachefragment {
	xhttpcacheentry Entry;
	xhttpbyterange Range;
	uint64 Length;
	uint16 SourceStatus;
	uint32 Flags;
} xhttpcachefragment;



/* 片段规范化动作由 Header 存储层执行，不绑定具体 Header 容器。 */
typedef enum xhttpcachefragmentaction {
	XHTTP_CACHE_FRAGMENT_ACTION_NONE = 0,
	XHTTP_CACHE_FRAGMENT_AS_200 = UINT32_C(0x00000001),
	XHTTP_CACHE_FRAGMENT_MARK_INCOMPLETE = UINT32_C(0x00000002),
	XHTTP_CACHE_FRAGMENT_REMOVE_CONTENT_RANGE = UINT32_C(0x00000004),
	XHTTP_CACHE_FRAGMENT_REMOVE_CONTENT_LENGTH = UINT32_C(0x00000008),
	XHTTP_CACHE_FRAGMENT_SET_CONTENT_LENGTH = UINT32_C(0x00000010)
} xhttpcachefragmentaction;



/* 片段计划同时返回协议决定、全部原因和 Header 规范化动作。 */
typedef struct xhttpcachefragmentplan {
	xhttpcachefragment Fragment;
	xhttpcachefragmentdecision Decision;
	uint32 Actions;
	uint32 Reasons;
} xhttpcachefragmentplan;



/*
	覆盖集借用按起点排序、互不重叠且不相邻的闭区间。
	HasLength 为 true 时所有区间都必须位于 Length 内。
*/
typedef struct xhttpcachecoverage {
	const xhttpbyterange* Ranges;
	size_t RangeCount;
	uint64 Length;
	bool HasLength;
} xhttpcachecoverage;



/* 覆盖判断区分参数错误、未覆盖和完整覆盖。 */
typedef enum xhttpcachecoverageresult {
	XHTTP_CACHE_COVERAGE_ERROR = -1,
	XHTTP_CACHE_COVERAGE_MISS = 0,
	XHTTP_CACHE_COVERAGE_HIT = 1
} xhttpcachecoverageresult;



/* 缺口游标允许在规范覆盖集上无分配前向迭代。 */
typedef struct xhttpcachemissingcursor {
	size_t Range;
	uint64 Next;
	bool Started;
	bool Finished;
} xhttpcachemissingcursor;



/* 组合决定区分独立表示、协议冲突、增量应用和完整替换。 */
typedef enum xhttpcachecombinedecision {
	XHTTP_CACHE_COMBINE_ERROR = -1,
	XHTTP_CACHE_COMBINE_SEPARATE = 0,
	XHTTP_CACHE_COMBINE_APPLY = 1,
	XHTTP_CACHE_COMBINE_REPLACE = 2,
	XHTTP_CACHE_COMBINE_CONFLICT = 3
} xhttpcachecombinedecision;



/* 组合动作描述 Header 来源和新响应字段更新要求。 */
typedef enum xhttpcachecombineaction {
	XHTTP_CACHE_COMBINE_ACTION_NONE = 0,
	XHTTP_CACHE_COMBINE_USE_INCOMING_FIELDS = UINT32_C(0x00000001),
	XHTTP_CACHE_COMBINE_UPDATE_INCOMING_FIELDS = UINT32_C(0x00000002),
	XHTTP_CACHE_COMBINE_REMOVE_CONTENT_RANGE = UINT32_C(0x00000004),
	XHTTP_CACHE_COMBINE_REMOVE_CONTENT_LENGTH = UINT32_C(0x00000008),
	XHTTP_CACHE_COMBINE_SET_CONTENT_LENGTH = UINT32_C(0x00000010),
	XHTTP_CACHE_COMBINE_MARK_INCOMPLETE = UINT32_C(0x00000020),
	XHTTP_CACHE_COMBINE_AS_200 = UINT32_C(0x00000040)
} xhttpcachecombineaction;



/*
	Index 和 RemoveCount 描述规范区间数组中的替换窗口。
	HeaderIndex 为 XRT_NPOS 时使用 Incoming Header，否则借用对应 Stored Header。
*/
typedef struct xhttpcachecombineplan {
	xhttpbyterange Range;
	uint64 Length;
	size_t Index;
	size_t RemoveCount;
	size_t ResultCount;
	size_t HeaderIndex;
	xhttpcachecombinedecision Decision;
	uint32 Actions;
	bool HasRange;
	bool HasLength;
	bool Complete;
} xhttpcachecombineplan;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_CACHE_RANGE)

/* 初始化尚未收到 Header 或正文的片段输入。 */
XRT_API void xrtHttpCacheFragmentInputInit(
	xhttpcachefragmentinput* pInput
);



/*
	把完整或截断 200、单段 206、multipart 中的一个 part 规范化为缓存片段。
	调用方必须先完成缓存键、Vary 和可存储策略判断；STORE 只表示片段结构可保存。
	协议性跳过返回 SKIP 并填写 Reasons；参数错误返回 ERROR 且不修改计划。
*/
XRT_API xhttpcachefragmentdecision xrtHttpCacheFragmentPlan(
	const xhttpcachefragmentinput* pInput,
	xhttpcachefragmentplan* pPlan
);



/* 判断公开片段的 Header、区间、完整长度和 partial 标志是否自洽。 */
XRT_API bool xrtHttpCacheFragmentValid(
	const xhttpcachefragment* pFragment
);



/* 判断片段是否已经覆盖完整表示，包括长度为零的空表示。 */
XRT_API bool xrtHttpCacheFragmentComplete(
	const xhttpcachefragment* pFragment
);



/* 判断规范覆盖集的排序、相邻合并和完整长度约束是否成立。 */
XRT_API bool xrtHttpCacheCoverageValid(
	const xhttpcachecoverage* pCoverage
);



/* 判断一个闭区间是否完全位于覆盖集中。 */
XRT_API xhttpcachecoverageresult xrtHttpCacheCoverageCovers(
	const xhttpcachecoverage* pCoverage,
	const xhttpbyterange* pRange
);



/* 初始化可以重复使用的缺口游标。 */
XRT_API void xrtHttpCacheMissingCursorInit(
	xhttpcachemissingcursor* pCursor
);



/*
	按起点顺序返回目标闭区间内尚未保存的连续缺口。
	覆盖集和目标必须保持不变，直到游标返回 END 或重新初始化。
*/
XRT_API xhttpnext xrtHttpCacheMissingNext(
	const xhttpcachecoverage* pCoverage,
	const xhttpbyterange* pTarget,
	xhttpcachemissingcursor* pCursor,
	xhttpbyterange* pMissing
);



/*
	按 RFC 9111 与 RFC 9110 组合一个新片段。
	Stored 和 Incoming 必须属于同一缓存键及 Vary 变体，Stored 按区间规范排序。
	现有片段只有整组共享同一个强验证器才能增量组合。
*/
XRT_API xhttpcachecombinedecision xrtHttpCacheCombinePlan(
	const xhttpcachefragment* pStored,
	size_t iCount,
	const xhttpcachefragment* pIncoming,
	xhttpcachecombineplan* pPlan
);

#endif



XRT_EXTERN_C_END

#endif
