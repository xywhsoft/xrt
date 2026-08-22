#ifndef XRT_NET_INTERFACE_H
#define XRT_NET_INTERFACE_H

#include <xrt/net.h>



#if defined(XRT_FEATURE_NET_INTERFACE) && !defined(XRT_FEATURE_NET)
	#error "XRT network interface support requires XRT_FEATURE_NET"
#endif

#if defined(XRT_FEATURE_NET_INTERFACE_TEXT) && \
	!defined(XRT_FEATURE_NET_INTERFACE)
	#error "XRT network interface text helpers require XRT_FEATURE_NET_INTERFACE"
#endif

#if defined(XRT_FEATURE_NET_INTERFACE_TEXT) && \
	!defined(XRT_FEATURE_CODEC_HEX)
	#error "XRT network interface text helpers require XRT_FEATURE_CODEC_HEX"
#endif



#if defined(XRT_FEATURE_NET_INTERFACE)

/* 前缀长度不可由平台可靠取得时使用该值。 */
#define XNET_INTERFACE_PREFIX_UNKNOWN 255u



/* 接口标志只表达跨平台能够稳定观察的状态和能力。 */
typedef enum xnetinterfaceflag {
	XNET_INTERFACE_UP = 0x0001,
	XNET_INTERFACE_RUNNING = 0x0002,
	XNET_INTERFACE_LOOPBACK = 0x0004,
	XNET_INTERFACE_BROADCAST = 0x0008,
	XNET_INTERFACE_POINT_TO_POINT = 0x0010,
	XNET_INTERFACE_MULTICAST = 0x0020
} xnetinterfaceflag;



/* 接口地址不带传输端口，PrefixLength 为网络前缀位数。 */
typedef struct xnetinterfaceaddress {
	xnetaddr Address;
	uint8 PrefixLength;
} xnetinterfaceaddress;



/* 名称、硬件地址和地址数组均借用所属接口快照。 */
typedef struct xnetinterface {
	uint32 IPv4Index;
	uint32 IPv6Index;
	uint32 Flags;
	uint32 Mtu;
	xstrview Name;
	xstrview DisplayName;
	xbytesview HardwareAddress;
	const xnetinterfaceaddress* Addresses;
	size_t AddressCount;
} xnetinterface;



/* 接口列表拥有 Items 以及所有条目借用的存储。 */
typedef struct xnetinterfacelist {
	const xnetinterface* Items;
	size_t Count;
} xnetinterfacelist;



XRT_EXTERN_C_BEGIN



/*
	把规范名称或显示名称转换为接口索引。
	UNSPEC 优先返回 IPv6 索引，再返回 IPv4 索引；失败返回零。
*/
XRT_API uint32 xrtNetInterfaceIndex(cstr sName, xnetfamily Family);



/*
	输出指定接口索引的规范名称并返回所需长度。
	UNSPEC 同时匹配 IPv4 与 IPv6 索引；空输出可查询所需大小。
*/
XRT_API size_t xrtNetInterfaceName(
	uint32 iIndex,
	xnetfamily Family,
	char* sName,
	size_t iCapacity
);



/* 创建当前系统接口、地址和元数据的一致快照。 */
XRT_API bool xrtNetInterfaces(xnetinterfacelist* pList);



/* 释放接口快照拥有的全部存储并清零。 */
XRT_API void xrtNetInterfacesFree(xnetinterfacelist* pList);



/*
	选择一个适合本机诊断的单播地址。
	这是确定性偏好查询，不代表公网出口、默认路由或服务监听策略。
*/
XRT_API bool xrtNetLocalAddress(
	xnetaddr* pAddress,
	xnetfamily Family
);



/*
	输出首选活动接口的原始硬件地址并返回所需字节数。
	空输出可查询大小；缓冲不足时不写入并报告完整所需大小。
*/
XRT_API size_t xrtNetLocalHardware(
	void* pAddress,
	size_t iCapacity
);



/* 输出本机主机名并返回不含结尾零字节的所需长度。 */
XRT_API size_t xrtNetHostName(
	char* sName,
	size_t iCapacity
);



#if defined(XRT_FEATURE_NET_INTERFACE_TEXT)

/* 输出首选本机地址文本并返回不含结尾零字节的所需长度。 */
XRT_API size_t xrtNetLocalAddressText(
	xnetfamily Family,
	char* sAddress,
	size_t iCapacity
);



/* 分配并返回首选本机地址文本。 */
XRT_API str xrtNetLocalAddressString(xnetfamily Family);



/* 输出首选接口硬件地址的大写紧凑 HEX 文本。 */
XRT_API size_t xrtNetLocalHardwareText(
	char* sAddress,
	size_t iCapacity
);



/* 分配并返回首选接口硬件地址的大写紧凑 HEX 文本。 */
XRT_API str xrtNetLocalHardwareString(void);



/* 分配并返回本机主机名。 */
XRT_API str xrtNetHostNameString(void);

#endif



XRT_EXTERN_C_END

#endif

#endif
