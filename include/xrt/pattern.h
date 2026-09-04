#ifndef XRT_PATTERN_H
#define XRT_PATTERN_H

#include <xrt/core.h>
#include <xrt/error.h>



#if defined(XRT_FEATURE_PATTERN)

/* 默认预算适用于大量路由，也阻止不可信模式造成无界编译。 */
#define XPATTERN_PATTERN_DEFAULT		(1024u * 1024u)
#define XPATTERN_PATTERNS_DEFAULT	100000u
#define XPATTERN_CAPTURES_DEFAULT	256u
#define XPATTERN_STATES_DEFAULT		1000000u
#define XPATTERN_COMPILED_DEFAULT	(512u * 1024u * 1024u)



/* 零值永远不是有效的 Builder 条目句柄。 */
typedef uint64 xpatternid;

#define XPATTERN_ID_INVALID ((xpatternid)0)



/* 未命中不是错误，全部匹配入口使用同一三态结果。 */
typedef enum xpatternresult {
	XPATTERN_ERROR = -1,
	XPATTERN_NONE = 0,
	XPATTERN_MATCH = 1
} xpatternresult;



/* pattern 模块错误代码在 xrt.pattern 域内保持稳定。 */
typedef enum xpatternerror {
	XPATTERN_ERROR_CONFIG = 1601,
	XPATTERN_ERROR_PATTERN,
	XPATTERN_ERROR_LIMIT,
	XPATTERN_ERROR_CONFLICT,
	XPATTERN_ERROR_CAPACITY
} xpatternerror;



/*
	分隔符是字节集合：模式中的分隔字节仍要求精确匹配，捕获不能吞掉
	集合中的任意字节。全部模式共享一份配置，以便编译成单一确定性程序。
*/
typedef struct xpatternconfig {
	uint32 Flags;
	xstrview Separators;
	size_t MaxPatternBytes;
	size_t MaxPatterns;
	size_t MaxCaptures;
	size_t MaxStates;
	size_t MaxCompiledBytes;
	uint32 Reserved[4];
} xpatternconfig;



/* Value 仅作为借用值随命中返回，XRT 不获取或释放其所有权。 */
typedef struct xpatternspec {
	xstrview Pattern;
	ptr Value;
	int32 Priority;
	uint32 Flags;
} xpatternspec;



/*
	{name} 捕获非空字段，也可写成 prefix{name}suffix；一个字段至多一个普通
	捕获，前后缀至少一侧非空。{*name} 仍独占最终字段并可捕获空尾部。
	捕获由调用方数组按模式中的出现顺序保存，PatternIndex 属于当前快照。
*/
typedef struct xpatternmatch {
	xpatternid Id;
	size_t PatternIndex;
	ptr Value;
	size_t CaptureCount;
} xpatternmatch;



/* 编译对象不可变、可跨线程共享并通过引用计数管理。 */
typedef struct xpattern xpattern;



/* Builder 可变且不保证并发安全；成功编译不会清空其中的模式。 */
typedef struct xpatternbuilder xpatternbuilder;



XRT_EXTERN_C_BEGIN



/* 初始化默认的 "/" 分隔符、严格语义和有限资源预算。 */
XRT_API void xrtPatternConfigInit(xpatternconfig* pConfig);



/*
	直接解析并完整匹配一条模式。模式、参数和捕获容量有效时不分配内存；
	捕获视图借用 Text。容量不足时返回错误并把所需数量写入
	pCaptureCount，不产生部分捕获。
*/
XRT_API xpatternresult xrtPatternExtract(
	xstrview Pattern,
	xstrview Text,
	xstrview* arrCapture,
	size_t iCapacity,
	size_t* pCaptureCount
);



/* 使用自定义分隔符和预算执行一次性匹配。 */
XRT_API xpatternresult xrtPatternExtractConfig(
	xstrview Pattern,
	xstrview Text,
	const xpatternconfig* pConfig,
	xstrview* arrCapture,
	size_t iCapacity,
	size_t* pCaptureCount
);



/* 使用默认配置编译一条可重复匹配的模式。 */
XRT_API xpattern* xrtPatternCompile(xstrview Pattern);



/* 使用高级配置编译一条可重复匹配的模式。 */
XRT_API xpattern* xrtPatternCompileConfig(
	xstrview Pattern,
	const xpatternconfig* pConfig
);



/* 使用默认配置把多条模式编译成一个不可变匹配程序。 */
XRT_API xpattern* xrtPatternCompileMany(
	const xpatternspec* arrSpec,
	size_t iCount
);



/* 使用高级配置把多条模式编译成一个不可变匹配程序。 */
XRT_API xpattern* xrtPatternCompileManyConfig(
	const xpatternspec* arrSpec,
	size_t iCount,
	const xpatternconfig* pConfig
);



/* 增加不可变编译对象引用并返回原指针。 */
XRT_API xpattern* xrtPatternRef(xpattern* pPattern);



/* 释放不可变编译对象引用。 */
XRT_API void xrtPatternRelease(xpattern* pPattern);



/* 返回编译对象中的模式数量。 */
XRT_API size_t xrtPatternCount(const xpattern* pPattern);



/* 返回编译对象实际占用的单块存储字节数。 */
XRT_API size_t xrtPatternCompiledBytes(const xpattern* pPattern);



/* 返回编译对象复制并归一化后的分隔符集合。 */
XRT_API xstrview xrtPatternSeparators(const xpattern* pPattern);



/* 返回指定模式的原始表达式视图。 */
XRT_API xstrview xrtPatternSource(
	const xpattern* pPattern,
	size_t iPattern
);



/* 返回指定模式的稳定 Builder ID。直接批量编译时 ID 也保持非零。 */
XRT_API xpatternid xrtPatternId(
	const xpattern* pPattern,
	size_t iPattern
);



/* 返回指定模式携带的借用值。 */
XRT_API ptr xrtPatternValue(
	const xpattern* pPattern,
	size_t iPattern
);



/* 返回指定模式的捕获数量。 */
XRT_API size_t xrtPatternCaptureCount(
	const xpattern* pPattern,
	size_t iPattern
);



/* 返回整个编译对象中单条模式所需的最大捕获数量。 */
XRT_API size_t xrtPatternMaxCaptureCount(const xpattern* pPattern);



/* 返回指定捕获的借用名称。 */
XRT_API bool xrtPatternCaptureName(
	const xpattern* pPattern,
	size_t iPattern,
	size_t iCapture,
	xstrview* pName
);



/* 按名称查找捕获索引，未找到时返回 XRT_NPOS。 */
XRT_API size_t xrtPatternCaptureIndex(
	const xpattern* pPattern,
	size_t iPattern,
	xstrview Name
);



/* 只选择最优模式，不记录或输出捕获。 */
XRT_API xpatternresult xrtPatternLookup(
	const xpattern* pPattern,
	xstrview Text,
	xpatternmatch* pMatch
);



/* 选择最优模式并按出现顺序输出借用 Text 的捕获视图。 */
XRT_API xpatternresult xrtPatternMatch(
	const xpattern* pPattern,
	xstrview Text,
	xstrview* arrCapture,
	size_t iCapacity,
	xpatternmatch* pMatch
);



/* 只判断是否有模式匹配，不返回模式或捕获。 */
XRT_API xpatternresult xrtPatternTest(
	const xpattern* pPattern,
	xstrview Text
);



/* 从 pattern 错误的机器数据中读取模式内字节位置。 */
XRT_API bool xrtPatternErrorOffset(
	const xerror* pError,
	size_t* pOffset
);



/* 从批量编译错误的机器数据中读取失败模式索引。 */
XRT_API bool xrtPatternErrorPattern(
	const xerror* pError,
	size_t* pPatternIndex
);



/* 使用默认配置创建空 Builder。 */
XRT_API xpatternbuilder* xrtPatternBuilderCreate(void);



/* 使用高级配置创建空 Builder；分隔符会立即被复制。 */
XRT_API xpatternbuilder* xrtPatternBuilderCreateConfig(
	const xpatternconfig* pConfig
);



/* 释放 Builder、其中复制的模式以及内部缓存的编译快照。 */
XRT_API void xrtPatternBuilderFree(xpatternbuilder* pBuilder);



/* 清空全部条目并使已有 ID 失效，同时保留已分配槽容量。 */
XRT_API void xrtPatternBuilderClear(xpatternbuilder* pBuilder);



/* 保证 Builder 至少可保存指定数量的活动条目。 */
XRT_API bool xrtPatternBuilderReserve(
	xpatternbuilder* pBuilder,
	size_t iCapacity
);



/* 返回 Builder 中的活动条目数量。 */
XRT_API size_t xrtPatternBuilderCount(const xpatternbuilder* pBuilder);



/* 返回每次成功结构修改后递增的版本。 */
XRT_API uint64 xrtPatternBuilderVersion(const xpatternbuilder* pBuilder);



/* 判断 Builder 是否存在尚未成功编译的结构修改。 */
XRT_API bool xrtPatternBuilderDirty(const xpatternbuilder* pBuilder);



/* 复制并追加一条模式，返回稳定代际 ID。 */
XRT_API xpatternid xrtPatternBuilderAdd(
	xpatternbuilder* pBuilder,
	const xpatternspec* pSpec
);



/* 原子追加一批模式；任意一条失败时 Builder 保持不变。 */
XRT_API bool xrtPatternBuilderAddMany(
	xpatternbuilder* pBuilder,
	const xpatternspec* arrSpec,
	size_t iCount,
	xpatternid* arrId
);



/* 替换有效 ID 的模式和值，同时保留 ID 与原始注册顺序。 */
XRT_API bool xrtPatternBuilderSet(
	xpatternbuilder* pBuilder,
	xpatternid Id,
	const xpatternspec* pSpec
);



/* 删除有效 ID；不存在或陈旧 ID 返回 false，但不属于执行错误。 */
XRT_API bool xrtPatternBuilderRemove(
	xpatternbuilder* pBuilder,
	xpatternid Id
);



/*
	编译当前版本并返回新引用。无修改时复用缓存；失败不影响上一个快照，
	也不会撤销 Builder 中等待修正的修改。
*/
XRT_API xpattern* xrtPatternBuilderCompile(xpatternbuilder* pBuilder);



XRT_EXTERN_C_END

#endif

#endif
