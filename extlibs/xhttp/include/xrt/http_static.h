#ifndef XRT_HTTP_STATIC_H
#define XRT_HTTP_STATIC_H

#include <xrt/string.h>

#if defined(XHTTP_FEATURE_HTTP_STATIC_PLAN) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_FILE)
	#include <xrt/http_semantics.h>
#endif

#if defined(XHTTP_FEATURE_HTTP_STATIC_FILE)
	#include <xrt/file.h>
	#include <xrt/file_async.h>
	#include <xrt/http_body_file.h>
#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_PLAN) && \
	(!defined(XHTTP_FEATURE_HTTP_PRECONDITION) || \
	 !defined(XHTTP_FEATURE_HTTP_RANGE))
	#error "XRT HTTP static planning requires precondition and range support"
#endif

#if defined(XHTTP_FEATURE_HTTP_STATIC_RESPONSE) && \
	(!defined(XHTTP_FEATURE_HTTP_STATIC_PLAN) || \
	 !defined(XHTTP_FEATURE_HTTP_RANGE_MULTIPART))
	#error "XRT HTTP static response support requires static planning and multipart"
#endif

#if defined(XHTTP_FEATURE_HTTP_STATIC_PATH) && \
	(!defined(XRT_FEATURE_CODEC_PERCENT) || \
	 !defined(XRT_FEATURE_PATH_SAFE))
	#error "XRT HTTP static path mapping requires percent codec and safe path support"
#endif

#if defined(XHTTP_FEATURE_HTTP_STATIC_FILE) && \
	(!defined(XRT_FEATURE_ATOMIC) || \
	 !defined(XRT_FEATURE_FILE_ROOT) || \
	 !defined(XHTTP_FEATURE_HTTP_BODY_FILE) || \
	 !defined(XHTTP_FEATURE_HTTP_PRECONDITION))
	#error "XRT HTTP static files require atomic, file roots, file bodies and HTTP preconditions"
#endif

#if defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY) && \
	(!defined(XHTTP_FEATURE_HTTP_RANGE_MULTIPART) || \
	 !defined(XHTTP_FEATURE_HTTP_STATIC_FILE) || \
	 !defined(XHTTP_FEATURE_HTTP_BODY_FILE))
	#error "XRT HTTP static multipart bodies require multipart, static files and file bodies"
#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_PATH)

/* 默认使用可移植文件名；隐藏段必须由调用方明确放行。 */
typedef enum xhttpstaticpathflag {
	XHTTP_STATIC_PATH_PORTABLE = UINT32_C(0x00000001),
	XHTTP_STATIC_PATH_ALLOW_HIDDEN = UINT32_C(0x00000002)
} xhttpstaticpathflag;



/* 路径写入区分协议错误、正常路由未命中和成功映射。 */
typedef enum xhttpstaticpathstatus {
	XHTTP_STATIC_PATH_ERROR = -1,
	XHTTP_STATIC_PATH_NO_MATCH = 0,
	XHTTP_STATIC_PATH_MATCH = 1
} xhttpstaticpathstatus;



/*
	Mount 是已解码且规范化的 origin 路径，根挂载为斜杠。
	非根挂载不得以斜杠结尾，比较区分大小写并要求完整段边界。
*/
typedef struct xhttpstaticpathconfig {
	xstrview Mount;
	uint32 Flags;
} xhttpstaticpathconfig;



/* 高层映射结果拥有 Path；未命中时其余字段保持空值。 */
typedef struct xhttpstaticpath {
	str Path;
	size_t Size;
	bool Matched;
	bool TrailingSlash;
} xhttpstaticpath;

#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_FILE)

typedef struct xhttpstaticfile xhttpstaticfile;



/* 静态文件错误保留受限文件、异步文件和正文层的完整原因链。 */
typedef enum xhttpstaticfileerror {
	XHTTP_STATIC_FILE_ERROR_SUBMIT = 1,
	XHTTP_STATIC_FILE_ERROR_OPEN,
	XHTTP_STATIC_FILE_ERROR_STAT,
	XHTTP_STATIC_FILE_ERROR_TYPE,
	XHTTP_STATIC_FILE_ERROR_SIZE,
	XHTTP_STATIC_FILE_ERROR_RANGE,
	XHTTP_STATIC_FILE_ERROR_ADOPT,
	XHTTP_STATIC_FILE_ERROR_BODY,
	XHTTP_STATIC_FILE_ERROR_REFERENCE,
	XHTTP_STATIC_FILE_ERROR_CONSUMED
} xhttpstaticfileerror;

#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_PLAN)

/*
	静态响应计划的 MaxRanges 同时限制解析工作和调用方输出容量。
	MergeGap 表示两个闭区间之间最多允许多少未选择字节仍合并。
*/
typedef struct xhttpstaticplanconfig {
	size_t MaxRanges;
	uint64 MergeGap;
} xhttpstaticplanconfig;



/*
	计划只描述协议结果，不创建 Reply 或 Body。
	Ranges 的前 RangeCount 项按起点排序并且已经合并。
*/
typedef struct xhttpstaticplan {
	uint16 Status;
	bool SendBody;
	bool AcceptRanges;
	uint64 CompleteLength;
	uint64 SelectedLength;
	size_t RangeCount;
} xhttpstaticplan;

#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_RESPONSE)

/* 静态响应最多生成七个协议字段，业务字段仍由调用方自由追加。 */
#define XHTTP_STATIC_RESPONSE_MAX_FIELDS 7u



/*
	ContentType 为空时使用 application/octet-stream，CacheControl 为空时省略。
	多范围响应必须提供 1 到 70 字节的 token Boundary，正文长度由范围自动计算。
*/
typedef struct xhttpstaticresponseconfig {
	xstrview ContentType;
	xstrview CacheControl;
	xstrview Boundary;
} xhttpstaticresponseconfig;



/*
	响应字段借用配置文本、静态文本和调用方工作区。
	结构本身不拥有内存，可以直接交给 HTTP/1、HTTP/2 或自定义响应层。
*/
typedef struct xhttpstaticresponse {
	uint16 Status;
	bool SendBody;
	bool Multipart;
	uint64 BodyLength;
	xstrview ContentType;
	xstrview Boundary;
	xhttpfield Fields[XHTTP_STATIC_RESPONSE_MAX_FIELDS];
	size_t FieldCount;
} xhttpstaticresponse;

#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY)

/* 静态多范围正文只补充采用阶段错误，读取错误保留文件正文的完整原因链。 */
typedef enum xhttpstaticmultipartbodyerror {
	XHTTP_STATIC_MULTIPART_BODY_ERROR_ADOPT = 1
} xhttpstaticmultipartbodyerror;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_STATIC_PATH)

/* 初始化根挂载、可移植文件名和默认拒绝隐藏段的安全配置；输出结构可以未对齐。 */
XRT_API void xrtHttpStaticPathConfigInit(
	xhttpstaticpathconfig* pConfig
);



/*
	把原始 URL path 映射为零结尾的相对文件路径。
	空输出只查询长度；容量不足不修改输出，并通过 pSize 返回所需正文长度。
	配置、pSize 与 pTrailingSlash 可以未对齐，但都必须是完整且不回绕的内存区间。
*/
XRT_API xhttpstaticpathstatus xrtHttpStaticPathWrite(
	xstrview RawPath,
	const xhttpstaticpathconfig* pConfig,
	char* sOutput,
	size_t iCapacity,
	size_t* pSize,
	bool* pTrailingSlash
);



/* 初始化一个可交给映射与释放函数使用的空拥有型结果；结果结构可以未对齐。 */
XRT_API void xrtHttpStaticPathInit(
	xhttpstaticpath* pPath
);



/*
	分配并映射相对路径；正常未命中返回 true 且 Matched 为 false。
	pPath 必须是已经初始化或释放后的空结果。
*/
XRT_API bool xrtHttpStaticPathMap(
	xstrview RawPath,
	const xhttpstaticpathconfig* pConfig,
	xhttpstaticpath* pPath
);



/* 释放拥有型路径并恢复为空结果；空指针和空结果均可安全传入。 */
XRT_API void xrtHttpStaticPathFree(
	xhttpstaticpath* pPath
);

#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_FILE)

/*
	同步打开文件根内的普通文件，并采用同一文件句柄作为异步资源。
	该函数可能执行阻塞文件系统操作；调用方拥有返回引用。
*/
XRT_API xhttpstaticfile* xrtHttpStaticFileOpen(
	xtaskpool* pPool,
	xroot Root,
	cstr sPath
);



/*
	在任务池中打开文件根内的普通文件。
	成功 Future 的值为借用 xhttpstaticfile；Root 必须存活到 Future 终态。
	路径文本在函数返回前复制；任务池必须存活到资源及其正文全部释放。
*/
XRT_API xfuture* xrtHttpStaticFileFuture(
	xtaskpool* pPool,
	xroot Root,
	cstr sPath
);



/* 增加静态文件资源引用并返回原指针。 */
XRT_API xhttpstaticfile* xrtHttpStaticFileRef(
	xhttpstaticfile* pFile
);



/* 释放资源引用；尚未取走的异步文件会进入异步关闭流程。 */
XRT_API void xrtHttpStaticFileDestroy(
	xhttpstaticfile* pFile
);



/* 返回从最终打开文件句柄取得的借用只读元数据。 */
XRT_API const xfileinfo* xrtHttpStaticFileInfo(
	const xhttpstaticfile* pFile
);



/*
	返回借用的当前表示验证器。
	弱 ETag 和 Last-Modified 直接来自同一打开文件的稳定元数据。
*/
XRT_API const xhttprepresentation* xrtHttpStaticFileRepresentation(
	const xhttpstaticfile* pFile
);



/* 返回静态文件的完整字节长度；参数无效时返回零并设置错误。 */
XRT_API uint64 xrtHttpStaticFileSize(
	const xhttpstaticfile* pFile
);



/*
	一次性取走底层异步文件，供 multipart 或自定义正文实现使用。
	成功后静态文件对象仍保留元数据，但不能再次创建正文。
*/
XRT_API xasyncfile* xrtHttpStaticFileTakeFile(
	xhttpstaticfile* pFile
);



/*
	一次性创建并取走严格文件区间正文。
	失败时资源仍归静态文件对象，调用方可以修正参数或内存条件后重试。
*/
XRT_API xhttpbody* xrtHttpStaticFileTakeBody(
	xhttpstaticfile* pFile,
	uint64 iOffset,
	uint64 iLength
);



/* 一次性创建并取走完整文件正文。 */
XRT_API xhttpbody* xrtHttpStaticFileTakeBodyAll(
	xhttpstaticfile* pFile
);

#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_PLAN)

/* 初始化静态响应计划默认值；输出可以未对齐，但必须覆盖完整结构范围。 */
XRT_API void xrtHttpStaticPlanConfigInit(
	xhttpstaticplanconfig* pConfig
);



/*
	评估 GET/HEAD 静态表示的条件请求和 Range。
	Ranges 容量必须至少为 Config.MaxRanges，且不得覆盖任何输入或 Plan。
	配置、字段数组、Current、Ranges 和 Plan 均可未对齐，但范围必须完整且不回绕。
*/
XRT_API bool xrtHttpStaticPlanBuild(
	xstrview Method,
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttprepresentation* pCurrent,
	uint64 iLength,
	xhttpbyterange* pRanges,
	size_t iRangeCapacity,
	const xhttpstaticplanconfig* pConfig,
	xhttpstaticplan* pPlan
);

#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_RESPONSE)

/* 初始化默认响应配置；输出可以未对齐，但必须覆盖完整且不回绕的结构范围。 */
XRT_API void xrtHttpStaticResponseConfigInit(
	xhttpstaticresponseconfig* pConfig
);



/*
	把静态计划转换为状态、正文长度和协议字段。
	pResponse 为空时执行精确工作区长度查询；实际构建不分配内存。
	固定结构、范围数组和标量输出均可未对齐；函数只发布完整成功结果。
*/
XRT_API bool xrtHttpStaticResponseBuild(
	const xhttpstaticplan* pPlan,
	const xhttpbyterange* pRanges,
	const xhttprepresentation* pCurrent,
	const xhttpstaticresponseconfig* pConfig,
	void* pWorkspace,
	size_t iCapacity,
	size_t* pSize,
	xhttpstaticresponse* pResponse
);

#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY)

/*
	采用一个可读异步文件并创建不可重放的多范围正文。
	成功后正文独占关闭文件；失败时文件所有权仍归调用方。
	范围数组可以未对齐，但必须覆盖完整且不回绕的结构范围；函数返回前会复制范围。
*/
XRT_API xhttpbody* xrtHttpStaticMultipartBodyAdopt(
	xasyncfile* pFile,
	const xhttpbyterange* pRanges,
	size_t iRangeCount,
	uint64 iCompleteLength,
	xstrview ContentType,
	xstrview Boundary
);



/*
	从静态文件资源一次性取走底层文件并创建多范围正文。
	任何构造失败都不会消费静态文件，调用方可以修正后重试。
	范围数组可以未对齐，但必须覆盖完整且不回绕的结构范围；函数返回前会复制范围。
*/
XRT_API xhttpbody* xrtHttpStaticFileTakeMultipartBody(
	xhttpstaticfile* pFile,
	const xhttpbyterange* pRanges,
	size_t iRangeCount,
	xstrview ContentType,
	xstrview Boundary
);

#endif



XRT_EXTERN_C_END

#endif
