#ifndef XRT_WEBSOCKET_UPGRADE_H
#define XRT_WEBSOCKET_UPGRADE_H

#include <xrt/http1.h>
#include <xrt/websocket.h>

#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE_STREAM)
	#include <xrt/websocket_stream.h>
#endif



#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE) && \
	(!defined(XRT_FEATURE_WEBSOCKET_HANDSHAKE) || \
	 !defined(XRT_FEATURE_WEBSOCKET_KEYGEN) || \
	 !defined(XRT_FEATURE_HTTP1_HEAD) || \
	 !defined(XRT_FEATURE_HTTP_HOST))
	#error "XRT WebSocket Upgrade requires handshake, key generation, HTTP/1 Head and Host"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE_DEFLATE) && \
	(!defined(XRT_FEATURE_WEBSOCKET_UPGRADE) || \
	 !defined(XRT_FEATURE_WEBSOCKET_DEFLATE))
	#error "XRT WebSocket Upgrade Deflate requires Upgrade and permessage-deflate"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE_STREAM) && \
	(!defined(XRT_FEATURE_WEBSOCKET_UPGRADE) || \
	 !defined(XRT_FEATURE_WEBSOCKET_STREAM) || \
	 !defined(XRT_FEATURE_HTTP1_NET))
	#error "XRT WebSocket Upgrade Stream requires Upgrade, Stream and HTTP/1 network support"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE_STREAM) && \
	defined(XRT_FEATURE_WEBSOCKET_UPGRADE_DEFLATE) && \
	!defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
	#error "XRT WebSocket compressed Upgrade Stream requires compressed Stream support"
#endif



#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE)

#define XWS_UPGRADE_REQUEST_FIELDS_MAX 7u
#define XWS_UPGRADE_RESPONSE_FIELDS_MAX 5u



/*
	服务端压缩策略返回 true 表示接受并写回 Response。
	返回 false 且不设置错误表示主动放弃，设置错误表示协商失败。
*/
typedef bool (*xwsupgradeacceptproc)(
	const xwsdeflate* pOffer,
	xwsdeflate* pResponse,
	ptr pData
);



/* 服务端配置只描述子协议和可选压缩策略，不绑定 HTTP 或网络对象。 */
typedef struct xwsupgradeserverconfig {
	xstrview Protocols;
	xwsupgradeacceptproc AcceptDeflate;
	ptr DeflateData;
	bool EnableDeflate;
	bool RequireDeflate;
} xwsupgradeserverconfig;



/* 客户端配置保存本次实际发出的子协议和压缩 offer。 */
typedef struct xwsupgradeclientconfig {
	xstrview Protocols;
	xwsdeflate Deflate;
	bool EnableDeflate;
	bool RequireDeflate;
} xwsupgradeclientconfig;



/*
	Protocol 借用被校验的 HTTP Header；Accept 和压缩响应由结果自身持有。
	结果只保存建立 WebSocket Stream 所需的协商事实。
*/
typedef struct xwsupgrade {
	char Accept[XWS_ACCEPT_CAPACITY];
	xstrview Protocol;
	xwsdeflate Deflate;
	char Extensions[XWS_DEFLATE_MAX_SIZE + 1u];
	size_t ExtensionSize;
	bool DeflateEnabled;
} xwsupgrade;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE)

/* 初始化不选择子协议、不开启压缩的服务端配置。 */
XRT_API void xrtWsUpgradeServerConfigInit(
	xwsupgradeserverconfig* pConfig
);



/* 无分配验证服务端配置。 */
XRT_API bool xrtWsUpgradeServerConfigValid(
	const xwsupgradeserverconfig* pConfig
);



/* 初始化不提供子协议、不开启压缩的客户端配置。 */
XRT_API void xrtWsUpgradeClientConfigInit(
	xwsupgradeclientconfig* pConfig
);



/* 无分配验证客户端配置和压缩 offer。 */
XRT_API bool xrtWsUpgradeClientConfigValid(
	const xwsupgradeclientconfig* pConfig
);



/*
	严格校验完整 HTTP/1.1 WebSocket Upgrade 请求并计算响应事实。
	重复子协议字段按线路顺序合并，重复协议、正文分帧字段和错误 Upgrade 均拒绝。
*/
XRT_API bool xrtWsUpgradeRequestCheck(
	const xhttp1head* pRequest,
	const xwsupgradeserverconfig* pConfig,
	xwsupgrade* pUpgrade
);



/*
	严格校验 101 响应与本次 Key、子协议和压缩 offer 的绑定关系。
	成功时 Protocol 借用 Response，失败不会修改 Upgrade。
*/
XRT_API bool xrtWsUpgradeResponseCheck(
	const xhttp1head* pResponse,
	xstrview Key,
	const xwsupgradeclientconfig* pConfig,
	xwsupgrade* pUpgrade
);



/*
	填充标准客户端握手字段；调用方可在返回计数后继续追加 Origin 等字段。
	Fields 为空且容量为零时只查询字段数量，容量不足不写入字段数组。
*/
XRT_API bool xrtWsUpgradeRequestFields(
	xstrview Host,
	xstrview Key,
	xstrview Protocols,
	xstrview Extensions,
	xhttpfield* pFields,
	size_t iCapacity,
	size_t* pCount
);



/*
	填充标准 101 响应字段；自定义扩展可直接传入已经协商完成的字段值。
	Fields 为空且容量为零时只查询字段数量，容量不足不写入字段数组。
*/
XRT_API bool xrtWsUpgradeResponseFields(
	xstrview Accept,
	xstrview Protocol,
	xstrview Extensions,
	xhttpfield* pFields,
	size_t iCapacity,
	size_t* pCount
);

#endif



#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE_STREAM)

/* 以协商结果初始化可继续调整限额和事件策略的 WebSocket Stream 配置。 */
XRT_API bool xrtWsUpgradeStreamConfig(
	xwsstreamconfig* pConfig,
	xwsrole Role,
	const xwsupgrade* pUpgrade
);

#endif



#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE)
XRT_EXTERN_C_END
#endif

#endif
