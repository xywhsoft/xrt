#ifndef XRT_INTERNAL_HTTP_CACHE_STORE_H
#define XRT_INTERNAL_HTTP_CACHE_STORE_H

#include "xrt_http_cache_validate.h"
#include <xrt/http_cache_store.h>

#if defined(XHTTP_FEATURE_HTTP_CACHE_STORE)
	#include <xrt/map.h>
	#include <xrt/sync.h>
#endif



#if defined(XHTTP_FEATURE_HTTP_CACHE_STORE)

/* 每个 Vary 维度引用 Record 内连续保存的原请求字段。 */
typedef struct xrt_http_cache_vary {
	xstrview Name;
	size_t Field;
	size_t FieldCount;
} xrt_http_cache_vary;



/* 不可变 Record 的数组、文本和正文全部位于结构后的同一分配中。 */
struct xhttpcacherecord {
	volatile int32 References;
	size_t Charge;
	xhttpcachekey Key;
	xhttpversion Version;
	uint16 Status;
	uint32 Flags;
	xstrview Reason;
	xhttpfield* Fields;
	size_t FieldCount;
	xhttpfield* Trailers;
	size_t TrailerCount;
	xhttpcachepart* Parts;
	size_t PartCount;
	uint64 BodyBytes;
	uint64 Length;
	xtime ResponseTime;
	uint64 RequestClock;
	uint64 ResponseClock;
	xtime SelectionTime;
	xrt_http_cache_vary* Vary;
	size_t VaryCount;
};



/* 验证主键视图、方法、URI 和字段数组，并复制到对齐快照。 */
bool __xrtHttpCacheKeyResolve(
	const xhttpcachekey* pInput,
	xhttpcachekey* pKey
);



/* 判断一段内存是否覆盖已验证主键或任一借用内容。 */
bool __xrtHttpCacheKeyOverlap(
	const xhttpcachekey* pKey,
	const void* pMemory,
	size_t iSize
);



/* 判断已校验主键和全部 Vary 原请求字段是否匹配。 */
bool __xrtHttpCacheRecordMatchesValid(
	const xhttpcacherecord* pRecord,
	const xhttpcachekey* pKey
);



/* 比较两个 Record 是否描述同一个 Vary 变体。 */
bool __xrtHttpCacheRecordVariantEqual(
	const xhttpcacherecord* pLeft,
	const xhttpcacherecord* pRight
);



/* 返回用于多个匹配响应之间选择最新项的 Date 或接收时间。 */
xtime __xrtHttpCacheRecordSelectionTime(
	const xhttpcacherecord* pRecord
);

#endif

#endif
