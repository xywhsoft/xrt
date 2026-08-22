#ifndef XRT_HTTP_SERVER_STATIC_H
#define XRT_HTTP_SERVER_STATIC_H

#include <xrt/codec.h>
#include <xrt/http_server_future.h>
#include <xrt/http_static.h>
#include <xrt/mime.h>
#include <xrt/random.h>



#if defined(XRT_FEATURE_HTTP_SERVER_STATIC) && \
	(!defined(XRT_FEATURE_CODEC_HEX) || \
	 !defined(XRT_FEATURE_HTTP_SERVER_FUTURE) || \
	 !defined(XRT_FEATURE_HTTP_SERVER_BODY_ASYNC) || \
	 !defined(XRT_FEATURE_HTTP_STATIC_PATH) || \
	 !defined(XRT_FEATURE_HTTP_STATIC_FILE) || \
	 !defined(XRT_FEATURE_HTTP_STATIC_PLAN) || \
	 !defined(XRT_FEATURE_HTTP_STATIC_RESPONSE) || \
	 !defined(XRT_FEATURE_HTTP_STATIC_MULTIPART_BODY) || \
	 !defined(XRT_FEATURE_HTTP_RANGE_MULTIPART) || \
	 !defined(XRT_FEATURE_MIME_TYPES) || \
	 !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XRT HTTP static server support requires codec, server Future, static protocol, MIME and secure random support"
#endif



#if defined(XRT_FEATURE_HTTP_SERVER_STATIC)

/* 静态 Reply 组合层的稳定错误代码保留失败发生的准确阶段。 */
typedef enum xhttpserverstaticerror {
	XHTTP_SERVER_STATIC_ERROR_CONFIG = 1,
	XHTTP_SERVER_STATIC_ERROR_TARGET,
	XHTTP_SERVER_STATIC_ERROR_PATH,
	XHTTP_SERVER_STATIC_ERROR_BOUNDARY,
	XHTTP_SERVER_STATIC_ERROR_PLAN,
	XHTTP_SERVER_STATIC_ERROR_RESPONSE,
	XHTTP_SERVER_STATIC_ERROR_BODY,
	XHTTP_SERVER_STATIC_ERROR_REPLY
} xhttpserverstaticerror;



/*
	Reply 配置同时服务已打开文件和一站式异步服务。
	ContentType 为空时按资源路径推断；Boundary 为空时仅在多范围响应中自动生成。
*/
typedef struct xhttpstaticreplyconfig {
	xhttpstaticplanconfig Plan;
	xstrview ContentType;
	xstrview CacheControl;
	xstrview Boundary;
} xhttpstaticreplyconfig;



/*
	服务配置把 URL 挂载、有序目录索引和静态 Reply 策略组成一个完整但可替换的高层路径。
	Indexes 按顺序借用 IndexCount 个安全文件名；计数为零时不为尾随斜杠查找索引文件。
*/
typedef struct xhttpstaticserveconfig {
	xhttpstaticpathconfig Path;
	xhttpstaticreplyconfig Reply;
	const xstrview* Indexes;
	size_t IndexCount;
} xhttpstaticserveconfig;



XRT_EXTERN_C_BEGIN



/* 初始化默认 Reply 配置；输出可以未对齐，但必须覆盖完整结构范围。 */
XRT_API void xrtHttpStaticReplyConfigInit(
	xhttpstaticreplyconfig* pConfig
);



/* 初始化默认服务配置；输出可以未对齐，但必须覆盖完整结构范围。 */
XRT_API void xrtHttpStaticServeConfigInit(
	xhttpstaticserveconfig* pConfig
);



/*
	把纯静态响应描述转换为拥有型 Reply，并保留可选正文的独立引用。
	SendBody 为 true 时必须提供声明了精确长度的正文，且长度必须与响应一致；无正文响应不得附带正文。
	响应描述符可以未对齐；函数先验证并快照全部字段，再创建拥有型 Reply。
*/
XRT_API xhttpreply* xrtHttpReplyFromStatic(
	const xhttpstaticresponse* pResponse,
	xhttpbody* pBody
);



/*
	根据请求、已打开的安全静态文件和资源路径构建完整 Reply。
	成功时从 pFile 一次性取走正文文件；失败时尽量保持文件可重试，调用方始终拥有 pFile。
	可选配置可以未对齐，并在任何异步或文件操作前完成一次快照。
*/
XRT_API xhttpreply* xrtHttpReplyStatic(
	const xhttpserverrequest* pRequest,
	xhttpstaticfile* pFile,
	xstrview ResourcePath,
	const xhttpstaticreplyconfig* pConfig
);



/*
	映射请求路径并在任务池中打开根内文件，返回拥有 xhttpreply* 的 Future。
	路径未命中、被安全策略拒绝或资源不存在时成功生成 404；格式错误的 target 生成 400。
	Root 必须存活到 Future 终态；pPool 必须继续覆盖 Reply 正文和异步文件关闭。
	请求引用与配置文本由函数独立保留；取消 Future 会跳过尚未开始的 Reply 组合。
	可选配置可以未对齐；函数返回后调用方可立即修改或释放配置及其借用文本。
*/
XRT_API xfuture* xrtHttpReplyStaticFuture(
	xtaskpool* pPool,
	xroot Root,
	const xhttpserverrequest* pRequest,
	const xhttpstaticserveconfig* pConfig
);



/*
	为 Connection 当前请求启动静态文件 Future 并绑定唯一最终响应。
	成功只表示 Future 已被连接接管，不表示文件已经打开或响应已经发送。
*/
XRT_API bool xrtHttpConnStatic(
	xhttpconn* pConnection,
	xtaskpool* pPool,
	xroot Root,
	const xhttpstaticserveconfig* pConfig
);



XRT_EXTERN_C_END

#endif

#endif
