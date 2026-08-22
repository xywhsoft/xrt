#ifndef XRT_HTTP_CACHE_STORE_H
#define XRT_HTTP_CACHE_STORE_H

#include <xrt/http_cache_validate.h>
#include <xrt/http_vary.h>



#if defined(XHTTP_FEATURE_HTTP_CACHE_STORE) && \
	(!defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE) || \
	 !defined(XHTTP_FEATURE_HTTP_VARY) || \
	 !defined(XRT_FEATURE_MAP) || \
	 !defined(XRT_FEATURE_MUTEX))
	#error "XRT HTTP cache store requires cache validation, Vary, Map and Mutex"
#endif



#if defined(XHTTP_FEATURE_HTTP_CACHE_STORE)

#define XHTTP_CACHE_ENTRIES_DEFAULT		1024u
#define XHTTP_CACHE_BYTES_DEFAULT		(UINT64_C(64) * 1024u * 1024u)
#define XHTTP_CACHE_ENTRY_BYTES_DEFAULT	(UINT64_C(8) * 1024u * 1024u)



/* Cache 是引用计数、线程安全且按容量淘汰的内存响应存储。 */
typedef struct xhttpcache xhttpcache;



/* CacheRecord 是可跨线程持有的不可变响应快照。 */
typedef struct xhttpcacherecord xhttpcacherecord;



/*
	主键至少包含方法和目标 URI；Partition 用于站点、用户或租户隔离。
	Fields 是产生响应时的有效请求字段，Record 只保存 Vary 实际引用的字段。
*/
typedef struct xhttpcachekey {
	xstrview Method;
	xstrview URI;
	xstrview Partition;
	const xhttpfield* Fields;
	size_t FieldCount;
} xhttpcachekey;



/* Part 保存表示正文中的一段连续字节，Offset 使用完整表示的零基偏移。 */
typedef struct xhttpcachepart {
	uint64 Offset;
	xbytesview Data;
} xhttpcachepart;



typedef enum xhttpcacherecordflag {
	XHTTP_CACHE_RECORD_NONE = 0,

	/* Length 是已知的完整表示长度。 */
	XHTTP_CACHE_RECORD_HAS_LENGTH = UINT32_C(0x00000001),

	/* Parts 已经无空洞覆盖 [0, Length)，空表示允许 Length 为零。 */
	XHTTP_CACHE_RECORD_COMPLETE = UINT32_C(0x00000002)
} xhttpcacherecordflag;



/*
	RecordInput 全部借用调用方数据，创建成功后得到完全独立的紧凑副本。
	RequestClock 和 ResponseClock 来自同一单调时钟，ResponseTime 是墙钟时间。
*/
typedef struct xhttpcacherecordinput {
	xhttpcachekey Key;
	xhttpversion Version;
	uint16 Status;
	uint32 Flags;
	xstrview Reason;
	const xhttpfield* Fields;
	size_t FieldCount;
	const xhttpfield* Trailers;
	size_t TrailerCount;
	const xhttpcachepart* Parts;
	size_t PartCount;
	uint64 Length;
	xtime ResponseTime;
	uint64 RequestClock;
	uint64 ResponseClock;
} xhttpcacherecordinput;



/*
	所有限额都是硬逻辑限额；MaxBytes 计算不可变 Record 的实际分配大小。
	MaxEntries 独立约束索引和 LRU 元数据，零值配置无效，应先调用 ConfigInit。
*/
typedef struct xhttpcacheconfig {
	size_t InitialEntries;
	size_t MaxEntries;
	size_t MaxBytes;
	size_t MaxEntryBytes;
} xhttpcacheconfig;



/* Lookup 把正常未命中与执行错误分开。 */
typedef enum xhttpcachelookup {
	XHTTP_CACHE_LOOKUP_ERROR = -1,
	XHTTP_CACHE_LOOKUP_MISS = 0,
	XHTTP_CACHE_LOOKUP_HIT = 1
} xhttpcachelookup;



/*
	写入结果把并发冲突、容量拒绝、首次保存和变体替换分开。
	普通 Put 不返回 CONFLICT；条件 Insert 和 Replace 可以返回 CONFLICT。
*/
typedef enum xhttpcacheput {
	XHTTP_CACHE_PUT_ERROR = -1,
	XHTTP_CACHE_PUT_CONFLICT = 0,
	XHTTP_CACHE_PUT_REJECTED = 1,
	XHTTP_CACHE_PUT_STORED = 2,
	XHTTP_CACHE_PUT_REPLACED = 3
} xhttpcacheput;



/* 按不可变 Record 身份删除时区分错误、并发冲突和成功删除。 */
typedef enum xhttpcachechange {
	XHTTP_CACHE_CHANGE_ERROR = -1,
	XHTTP_CACHE_CHANGE_CONFLICT = 0,
	XHTTP_CACHE_CHANGE_APPLIED = 1
} xhttpcachechange;



/* Stats 是锁内取得的一次一致快照，累计计数从 Cache 创建起单调递增。 */
typedef struct xhttpcachestats {
	size_t Entries;
	size_t Bytes;
	uint64 Lookups;
	uint64 Hits;
	uint64 Misses;
	uint64 Stores;
	uint64 Replacements;
	uint64 Rejected;
	uint64 Conflicts;
	uint64 Evictions;
	uint64 Removals;
} xhttpcachestats;



/*
	Ops 为持久化、跨进程或应用自定义存储提供统一后端契约。
	除 Close 外的回调均为必需项，并且必须自行保证并发调用安全。
	Get 命中时转移一个独立 Record 引用；写入回调均不接管调用方引用。
	Insert 必须把“不存在”判断和插入作为一个不可分割的操作。
	Replace 的 Expected 是先前 Get 返回的不可变版本；后端只有确认该版本仍是
	对应变体的当前值时，才能原子提交 Replacement。持久化后端可以把 Expected
	映射为自己的修订号，但不能把比较和写入拆开。
	RemoveRecord 使用相同的当前版本条件并把比较和删除作为一个原子操作。
	条件不成立返回 CONFLICT，不得覆盖更新者，也不应伪装成执行错误。
	Remove、RemoveURI 和 Clear 用 false 报告失败，删除数量输出不能代替状态。
	统一句柄会验证 Get 命中的 Record 与查询 Key 匹配；Stats 失败时输出保持全零。
*/
typedef struct xhttpcacheops {
	xhttpcachelookup (*Get)(
		ptr pContext,
		const xhttpcachekey* pKey,
		xhttpcacherecord** ppRecord
	);
	xhttpcacheput (*Put)(
		ptr pContext,
		xhttpcacherecord* pRecord
	);
	xhttpcacheput (*Insert)(
		ptr pContext,
		xhttpcacherecord* pRecord
	);
	xhttpcacheput (*Replace)(
		ptr pContext,
		const xhttpcacherecord* pExpected,
		xhttpcacherecord* pReplacement
	);
	xhttpcachechange (*RemoveRecord)(
		ptr pContext,
		const xhttpcacherecord* pExpected
	);
	bool (*Remove)(
		ptr pContext,
		const xhttpcachekey* pKey,
		size_t* pRemoved
	);
	bool (*RemoveURI)(
		ptr pContext,
		xstrview URI,
		xstrview Partition,
		size_t* pRemoved
	);
	bool (*Clear)(ptr pContext);
	bool (*Stats)(
		ptr pContext,
		xhttpcachestats* pStats
	);
	void (*Close)(ptr pContext);
} xhttpcacheops;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_CACHE_STORE)

/* 初始化默认缓存限额；固定配置描述符支持未对齐存储。 */
XRT_API void xrtHttpCacheConfigInit(xhttpcacheconfig* pConfig);



/* 初始化没有分区和请求字段的主键；未对齐输出在返回前一次性发布。 */
XRT_API bool xrtHttpCacheKeyInit(
	xhttpcachekey* pKey,
	xstrview Method,
	xstrview URI
);



/* 初始化 HTTP/1.1 响应输入；Key 与输出描述符都支持未对齐存储。 */
XRT_API bool xrtHttpCacheRecordInputInit(
	xhttpcacherecordinput* pInput,
	const xhttpcachekey* pKey,
	uint16 iStatus
);



/*
	创建完全拥有输入内容的不可变 Record。
	Vary 星号被正常响应语义允许，但不能形成可复用记录，因此返回不支持错误。
	RecordInput 与正文片段数组可以未对齐，返回前已复制全部借用内容。
*/
XRT_API xhttpcacherecord* xrtHttpCacheRecordCreate(
	const xhttpcacherecordinput* pInput
);



/* 增加 Record 引用并返回原指针；引用溢出时返回空指针。 */
XRT_API xhttpcacherecord* xrtHttpCacheRecordRetain(
	const xhttpcacherecord* pRecord
);



/* 释放 Record 引用；空指针是安全的空操作。 */
XRT_API void xrtHttpCacheRecordRelease(xhttpcacherecord* pRecord);



/* 返回 Record 拥有的主键；Fields 只包含 Vary 引用的原请求字段。 */
XRT_API const xhttpcachekey* xrtHttpCacheRecordKey(
	const xhttpcacherecord* pRecord
);



/* 返回响应协议版本。 */
XRT_API xhttpversion xrtHttpCacheRecordVersion(
	const xhttpcacherecord* pRecord
);



/* 返回最终响应状态码。 */
XRT_API uint16 xrtHttpCacheRecordStatus(
	const xhttpcacherecord* pRecord
);



/* 返回 Record 标志。 */
XRT_API uint32 xrtHttpCacheRecordFlags(
	const xhttpcacherecord* pRecord
);



/* 返回借用的 reason phrase。 */
XRT_API xstrview xrtHttpCacheRecordReason(
	const xhttpcacherecord* pRecord
);



/* 返回响应 Header 数量。 */
XRT_API size_t xrtHttpCacheRecordFieldCount(
	const xhttpcacherecord* pRecord
);



/* 返回指定响应 Header，越界返回空指针且不设置错误。 */
XRT_API const xhttpfield* xrtHttpCacheRecordFieldAt(
	const xhttpcacherecord* pRecord,
	size_t iIndex
);



/* 返回首个同名响应 Header，未找到返回空指针。 */
XRT_API const xhttpfield* xrtHttpCacheRecordField(
	const xhttpcacherecord* pRecord,
	xstrview Name
);



/* 返回响应 Trailer 数量。 */
XRT_API size_t xrtHttpCacheRecordTrailerCount(
	const xhttpcacherecord* pRecord
);



/* 返回指定响应 Trailer，越界返回空指针且不设置错误。 */
XRT_API const xhttpfield* xrtHttpCacheRecordTrailerAt(
	const xhttpcacherecord* pRecord,
	size_t iIndex
);



/* 返回正文片段数量。 */
XRT_API size_t xrtHttpCacheRecordPartCount(
	const xhttpcacherecord* pRecord
);



/* 返回指定不可变正文片段，越界返回空指针且不设置错误。 */
XRT_API const xhttpcachepart* xrtHttpCacheRecordPartAt(
	const xhttpcacherecord* pRecord,
	size_t iIndex
);



/* 返回全部片段有效载荷字节数。 */
XRT_API uint64 xrtHttpCacheRecordBodyBytes(
	const xhttpcacherecord* pRecord
);



/* 返回完整表示长度；未知时返回零并由 HAS_LENGTH 标志区分。 */
XRT_API uint64 xrtHttpCacheRecordLength(
	const xhttpcacherecord* pRecord
);



/* 返回响应接收墙钟时间。 */
XRT_API xtime xrtHttpCacheRecordResponseTime(
	const xhttpcacherecord* pRecord
);



/* 返回发出请求时的单调时钟。 */
XRT_API uint64 xrtHttpCacheRecordRequestClock(
	const xhttpcacherecord* pRecord
);



/* 返回收到响应时的单调时钟。 */
XRT_API uint64 xrtHttpCacheRecordResponseClock(
	const xhttpcacherecord* pRecord
);



/* 返回 Record 单次紧凑分配的实际字节数。 */
XRT_API size_t xrtHttpCacheRecordCharge(
	const xhttpcacherecord* pRecord
);



/* 返回去重后的 Vary 选择字段数量。 */
XRT_API size_t xrtHttpCacheRecordVaryCount(
	const xhttpcacherecord* pRecord
);



/* 返回指定 Vary 字段名，越界返回空视图且不设置错误。 */
XRT_API xstrview xrtHttpCacheRecordVaryAt(
	const xhttpcacherecord* pRecord,
	size_t iIndex
);



/*
	判断主键和 Vary 原请求字段是否匹配。
	字段值按顺序比较并忽略两端 OWS；缺失字段只匹配缺失字段。
*/
XRT_API bool xrtHttpCacheRecordMatches(
	const xhttpcacherecord* pRecord,
	const xhttpcachekey* pKey
);



/* 构造验证协议层可直接使用的借用 Entry 视图；输出支持未对齐存储。 */
XRT_API bool xrtHttpCacheRecordEntry(
	const xhttpcacherecord* pRecord,
	xhttpcacheentry* pEntry
);



/* 创建线程安全的有界内存缓存；可选配置在返回前从未对齐存储快照。 */
XRT_API xhttpcache* xrtHttpCacheCreate(
	const xhttpcacheconfig* pConfig
);



/*
	用自定义线程安全后端创建统一 Cache 句柄。
	句柄从可未对齐存储复制 Ops；除 Close 外的全部回调都必须存在。
	最后一个引用释放时调用可选 Close，Context 由后端定义。
*/
XRT_API xhttpcache* xrtHttpCacheOpen(
	const xhttpcacheops* pOps,
	ptr pContext
);



/* 增加 Cache 引用并返回原指针；引用溢出时返回空指针。 */
XRT_API xhttpcache* xrtHttpCacheRetain(const xhttpcache* pCache);



/* 释放 Cache 引用；空指针是安全的空操作。 */
XRT_API void xrtHttpCacheRelease(xhttpcache* pCache);



/*
	查找最新匹配记录并返回独立引用。
	未命中和错误都会把输出清空，命中记录由调用方 Release。
	Key 与记录指针输出支持未对齐存储，但彼此及借用内容不得重叠。
*/
XRT_API xhttpcachelookup xrtHttpCacheGet(
	xhttpcache* pCache,
	const xhttpcachekey* pKey,
	xhttpcacherecord** ppRecord
);



/*
	保存 Record 的独立引用，函数不接管调用方引用。
	相同主键、Vary 维度和原请求值替换旧记录；容量不足是正常拒绝。
	该无条件接口使用最后写入者获胜，不返回 CONFLICT。
*/
XRT_API xhttpcacheput xrtHttpCachePut(
	xhttpcache* pCache,
	xhttpcacherecord* pRecord
);



/*
	仅在相同主键和 Vary 变体尚不存在时原子插入 Record。
	已有当前记录时返回 CONFLICT，不以旧快照覆盖并发写入。
	函数不接管调用方引用；成功后 Cache 独立持有 Record。
*/
XRT_API xhttpcacheput xrtHttpCacheInsert(
	xhttpcache* pCache,
	xhttpcacherecord* pRecord
);



/*
	仅在 Expected 仍是对应 Vary 变体的当前 Record 时原子提交 Replacement。
	两个 Record 的方法、URI 和 Partition 必须相同；Vary 可以按新元数据变化。
	Expected 应来自先前 Get；函数不接管任何引用，成功后 Cache 独立持有
	Replacement。并发更新使条件失效时返回 CONFLICT。
*/
XRT_API xhttpcacheput xrtHttpCacheReplace(
	xhttpcache* pCache,
	const xhttpcacherecord* pExpected,
	xhttpcacherecord* pReplacement
);



/*
	仅在 Expected 仍是当前 Record 时原子删除。
	记录已经被替换、删除或淘汰时返回 CONFLICT，不把正常竞态报告为执行错误。
	函数不接管 Expected 引用。
*/
XRT_API xhttpcachechange xrtHttpCacheRemoveRecord(
	xhttpcache* pCache,
	const xhttpcacherecord* pExpected
);



/*
	删除与完整 Key 和 Vary 选择匹配的全部记录。
	pRemoved 可以为空或未对齐；成功且没有匹配记录与后端失败保持可区分。
*/
XRT_API bool xrtHttpCacheRemove(
	xhttpcache* pCache,
	const xhttpcachekey* pKey,
	size_t* pRemoved
);



/*
	删除 URI 和 Partition 下的全部方法与 Vary 变体。
	pRemoved 可以为空或未对齐；失败时输出保持为零。
*/
XRT_API bool xrtHttpCacheRemoveURI(
	xhttpcache* pCache,
	xstrview URI,
	xstrview Partition,
	size_t* pRemoved
);



/* 删除全部记录并保留 Map 桶容量供后续复用。 */
XRT_API bool xrtHttpCacheClear(xhttpcache* pCache);



/* 取得一致统计快照；未对齐输出受支持，失败时输出全零。 */
XRT_API bool xrtHttpCacheStats(
	xhttpcache* pCache,
	xhttpcachestats* pStats
);

#endif



XRT_EXTERN_C_END

#endif
