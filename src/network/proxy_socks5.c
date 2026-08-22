#include "../internal/xrt_proxy.h"



#if defined(XRT_FEATURE_NET_PROXY_SOCKS5)

#define XRT_NET_SOCKS5_VERSION 0x05u
#define XRT_NET_SOCKS5_AUTH_VERSION 0x01u
#define XRT_NET_SOCKS5_METHOD_NONE 0x00u
#define XRT_NET_SOCKS5_METHOD_PASSWORD 0x02u
#define XRT_NET_SOCKS5_METHOD_REJECTED 0xFFu
#define XRT_NET_SOCKS5_COMMAND_CONNECT 0x01u
#define XRT_NET_SOCKS5_ADDRESS_IPV4 0x01u
#define XRT_NET_SOCKS5_ADDRESS_HOST 0x03u
#define XRT_NET_SOCKS5_ADDRESS_IPV6 0x04u
#define XRT_NET_SOCKS5_PACKET_MAX 513u



/* SOCKS5 内部阶段只表示下一份服务器回复的类型。 */
typedef enum __xrt_net_socks5_stage {
	XRT_NET_SOCKS5_STAGE_METHOD = 1,
	XRT_NET_SOCKS5_STAGE_AUTH,
	XRT_NET_SOCKS5_STAGE_CONNECT
} __xrt_net_socks5_stage;



/* 返回当前不可变代理配置。 */
static xnetproxyinfo __xrtNetSocks5Info(
	const xnetproxyhandshake* pHandshake
)
{
	xnetproxyinfo Info;

	memset(&Info, 0, sizeof(Info));
	(void)xrtNetProxyInfo(pHandshake->Proxy, &Info);
	return Info;
}



/* 生成只包含允许认证方法的问候报文，默认凭据不会降级为匿名。 */
static bool __xrtNetSocks5Greeting(
	xnetproxyhandshake* pHandshake
)
{
	xnetproxyinfo Info = __xrtNetSocks5Info(pHandshake);
	uint8 Packet[4];
	size_t iSize;
	bool bQueued;

	Packet[0] = XRT_NET_SOCKS5_VERSION;
	if ( Info.Auth == XNET_PROXY_AUTH_NONE ) {
		Packet[1] = 1;
		Packet[2] = XRT_NET_SOCKS5_METHOD_NONE;
		iSize = 3;
	} else if ( Info.Auth == XNET_PROXY_AUTH_REQUIRED ) {
		Packet[1] = 1;
		Packet[2] = XRT_NET_SOCKS5_METHOD_PASSWORD;
		iSize = 3;
	} else {
		Packet[1] = 2;
		Packet[2] = XRT_NET_SOCKS5_METHOD_NONE;
		Packet[3] = XRT_NET_SOCKS5_METHOD_PASSWORD;
		iSize = 4;
	}
	bQueued = __xrtNetProxyHandshakeQueue(
		pHandshake, Packet, iSize
	);
	xrtSecureZero(Packet, sizeof(Packet));
	return bQueued;
}



/* 生成 RFC 1929 用户名密码子协商报文。 */
static bool __xrtNetSocks5Auth(
	xnetproxyhandshake* pHandshake
)
{
	xnetproxyinfo Info = __xrtNetSocks5Info(pHandshake);
	uint8 Packet[XRT_NET_SOCKS5_PACKET_MAX];
	size_t iWrite = 0;
	bool bQueued;

	Packet[iWrite++] = XRT_NET_SOCKS5_AUTH_VERSION;
	Packet[iWrite++] = (uint8)Info.Username.Size;
	if ( Info.Username.Size != 0 ) {
		memcpy(Packet + iWrite, Info.Username.Data, Info.Username.Size);
		iWrite += Info.Username.Size;
	}
	Packet[iWrite++] = (uint8)Info.Password.Size;
	if ( Info.Password.Size != 0 ) {
		memcpy(Packet + iWrite, Info.Password.Data, Info.Password.Size);
		iWrite += Info.Password.Size;
	}
	bQueued = __xrtNetProxyHandshakeQueue(
		pHandshake, Packet, iWrite
	);
	xrtSecureZero(Packet, sizeof(Packet));
	return bQueued;
}



/* 生成支持 IPv4、IPv6 和远端域名解析的 CONNECT 请求。 */
static bool __xrtNetSocks5Connect(
	xnetproxyhandshake* pHandshake
)
{
	uint8 Packet[4u + 1u + UINT8_MAX + 2u];
	xstrview Target;
	xnetaddr Address;
	size_t iWrite = 0;
	bool bNumeric;
	bool bQueued;

	Target.Data = pHandshake->TargetHost;
	Target.Size = pHandshake->TargetSize;
	Packet[iWrite++] = XRT_NET_SOCKS5_VERSION;
	Packet[iWrite++] = XRT_NET_SOCKS5_COMMAND_CONNECT;
	Packet[iWrite++] = 0;
	bNumeric = __xrtNetAddrTryParse(&Address, Target, 0);
	if ( bNumeric && (Address.Family == XNET_FAMILY_IPV4) ) {
		Packet[iWrite++] = XRT_NET_SOCKS5_ADDRESS_IPV4;
		memcpy(Packet + iWrite, Address.Address, 4);
		iWrite += 4;
	} else if ( bNumeric && (Address.Family == XNET_FAMILY_IPV6) ) {
		Packet[iWrite++] = XRT_NET_SOCKS5_ADDRESS_IPV6;
		memcpy(Packet + iWrite, Address.Address, 16);
		iWrite += 16;
	} else {
		Packet[iWrite++] = XRT_NET_SOCKS5_ADDRESS_HOST;
		Packet[iWrite++] = (uint8)Target.Size;
		memcpy(Packet + iWrite, Target.Data, Target.Size);
		iWrite += Target.Size;
	}
	Packet[iWrite++] = (uint8)(pHandshake->TargetPort >> 8);
	Packet[iWrite++] = (uint8)pHandshake->TargetPort;
	bQueued = __xrtNetProxyHandshakeQueue(
		pHandshake, Packet, iWrite
	);
	xrtSecureZero(Packet, sizeof(Packet));
	return bQueued;
}



/* 根据 SOCKS5 回复码提供稳定错误文本。 */
static cstr __xrtNetSocks5ReplyMessage(uint8 iReply)
{
	switch ( iReply ) {
		case XNET_SOCKS5_GENERAL_FAILURE:
			return "SOCKS5 proxy reported a general failure";
		case XNET_SOCKS5_RULESET_DENIED:
			return "SOCKS5 proxy denied the connection by policy";
		case XNET_SOCKS5_NETWORK_UNREACHABLE:
			return "SOCKS5 proxy could not reach the network";
		case XNET_SOCKS5_HOST_UNREACHABLE:
			return "SOCKS5 proxy could not reach the host";
		case XNET_SOCKS5_CONNECTION_REFUSED:
			return "SOCKS5 target refused the connection";
		case XNET_SOCKS5_TTL_EXPIRED:
			return "SOCKS5 proxy reported an expired TTL";
		case XNET_SOCKS5_COMMAND_UNSUPPORTED:
			return "SOCKS5 proxy does not support CONNECT";
		case XNET_SOCKS5_ADDRESS_UNSUPPORTED:
			return "SOCKS5 proxy does not support the target address type";
		default:
			return "SOCKS5 proxy returned an unknown failure code";
	}
}



/* 消费方法选择回复并生成认证或 CONNECT 报文。 */
static xnetproxyhandshakestate __xrtNetSocks5Method(
	xnetproxyhandshake* pHandshake,
	xnetbuf* pInput
)
{
	xnetproxyinfo Info = __xrtNetSocks5Info(pHandshake);
	uint8 Reply[2];
	uint8 iMethod;
	bool bAllowed;

	if ( xrtNetBufSize(pInput) < sizeof(Reply) ) {
		return XNET_PROXY_HANDSHAKE_READ;
	}
	(void)xrtNetBufPeek(pInput, 0, Reply, sizeof(Reply));
	if ( Reply[0] != XRT_NET_SOCKS5_VERSION ) {
		return __xrtNetProxyHandshakeFail(
			pHandshake, XERR_PROTOCOL, XNET_ERROR_PROXY_PROTOCOL,
			"negotiate-socks5", "SOCKS5 method reply has an invalid version"
		);
	}
	iMethod = Reply[1];
	if ( iMethod == XRT_NET_SOCKS5_METHOD_REJECTED ) {
		return __xrtNetProxyHandshakeFail(
			pHandshake, XERR_PERMISSION, XNET_ERROR_PROXY_AUTH,
			"authenticate-socks5", "SOCKS5 proxy rejected all authentication methods"
		);
	}
	bAllowed = ((iMethod == XRT_NET_SOCKS5_METHOD_NONE) &&
		((Info.Auth == XNET_PROXY_AUTH_NONE) ||
		 (Info.Auth == XNET_PROXY_AUTH_OPTIONAL))) ||
		((iMethod == XRT_NET_SOCKS5_METHOD_PASSWORD) &&
		 ((Info.Auth == XNET_PROXY_AUTH_REQUIRED) ||
		  (Info.Auth == XNET_PROXY_AUTH_OPTIONAL)));
	if ( !bAllowed ) {
		return __xrtNetProxyHandshakeFail(
			pHandshake, XERR_PERMISSION, XNET_ERROR_PROXY_AUTH,
			"authenticate-socks5", "SOCKS5 proxy selected a disallowed authentication method"
		);
	}
	if ( iMethod == XRT_NET_SOCKS5_METHOD_PASSWORD ) {
		if ( !__xrtNetSocks5Auth(pHandshake) ) {
			return pHandshake->State;
		}
		pHandshake->Stage = XRT_NET_SOCKS5_STAGE_AUTH;
	} else {
		if ( !__xrtNetSocks5Connect(pHandshake) ) {
			return pHandshake->State;
		}
		pHandshake->Stage = XRT_NET_SOCKS5_STAGE_CONNECT;
	}
	(void)xrtNetBufConsume(pInput, sizeof(Reply));
	return pHandshake->State;
}



/* 消费用户名密码子协商回复并生成 CONNECT 报文。 */
static xnetproxyhandshakestate __xrtNetSocks5AuthReply(
	xnetproxyhandshake* pHandshake,
	xnetbuf* pInput
)
{
	uint8 Reply[2];

	if ( xrtNetBufSize(pInput) < sizeof(Reply) ) {
		return XNET_PROXY_HANDSHAKE_READ;
	}
	(void)xrtNetBufPeek(pInput, 0, Reply, sizeof(Reply));
	if ( Reply[0] != XRT_NET_SOCKS5_AUTH_VERSION ) {
		return __xrtNetProxyHandshakeFail(
			pHandshake, XERR_PROTOCOL, XNET_ERROR_PROXY_PROTOCOL,
			"authenticate-socks5", "SOCKS5 password reply has an invalid version"
		);
	}
	if ( Reply[1] != 0 ) {
		return __xrtNetProxyHandshakeFail(
			pHandshake, XERR_PERMISSION, XNET_ERROR_PROXY_AUTH,
			"authenticate-socks5", "SOCKS5 username or password was rejected"
		);
	}
	if ( !__xrtNetSocks5Connect(pHandshake) ) {
		return pHandshake->State;
	}
	pHandshake->Stage = XRT_NET_SOCKS5_STAGE_CONNECT;
	(void)xrtNetBufConsume(pInput, sizeof(Reply));
	return pHandshake->State;
}



/* 解析完整绑定端点，并在成功前完成所有可能失败的内存申请。 */
static bool __xrtNetSocks5Bound(
	xnetproxyhandshake* pHandshake,
	const uint8* pReply,
	size_t iSize
)
{
	xnetproxyendpoint Bound;
	str sHost = NULL;
	size_t iHostSize = 0;
	size_t iPortOffset;

	memset(&Bound, 0, sizeof(Bound));
	if ( pReply[3] == XRT_NET_SOCKS5_ADDRESS_IPV4 ) {
		Bound.Address.Family = XNET_FAMILY_IPV4;
		memcpy(Bound.Address.Address, pReply + 4, 4);
		iPortOffset = 8;
	} else if ( pReply[3] == XRT_NET_SOCKS5_ADDRESS_IPV6 ) {
		Bound.Address.Family = XNET_FAMILY_IPV6;
		memcpy(Bound.Address.Address, pReply + 4, 16);
		iPortOffset = 20;
	} else {
		iHostSize = pReply[4];
		iPortOffset = 5u + iHostSize;
		sHost = (str)xrtMalloc(iHostSize + 1u);
		if ( sHost == NULL ) {
			(void)__xrtNetProxyHandshakeFail(
				pHandshake, XERR_MEMORY, XNET_ERROR_PROXY_CREATE,
				"parse-socks5-bound", "SOCKS5 bound host allocation failed"
			);
			return false;
		}
		memcpy(sHost, pReply + 5, iHostSize);
		sHost[iHostSize] = 0;
		Bound.Host.Data = sHost;
		Bound.Host.Size = iHostSize;
	}
	if ( (iPortOffset + 2u) != iSize ) {
		xrtFree(sHost);
		(void)__xrtNetProxyHandshakeFail(
			pHandshake, XERR_INTERNAL, XNET_ERROR_PROXY_PROTOCOL,
			"parse-socks5-bound", "SOCKS5 bound endpoint length is inconsistent"
		);
		return false;
	}
	Bound.Address.Port = (uint16)(
		((uint16)pReply[iPortOffset] << 8) |
		(uint16)pReply[iPortOffset + 1u]
	);
	pHandshake->BoundHost = sHost;
	pHandshake->Bound = Bound;
	pHandshake->HasBound = true;
	return true;
}



/* 消费 CONNECT 回复，保留同一输入链中已经预读的应用层数据。 */
static xnetproxyhandshakestate __xrtNetSocks5ConnectReply(
	xnetproxyhandshake* pHandshake,
	xnetbuf* pInput
)
{
	uint8 Header[5];
	uint8 Reply[4u + 1u + UINT8_MAX + 2u];
	size_t iNeed;

	if ( xrtNetBufSize(pInput) < 4u ) {
		return XNET_PROXY_HANDSHAKE_READ;
	}
	(void)xrtNetBufPeek(pInput, 0, Header, 4);
	if ( (Header[0] != XRT_NET_SOCKS5_VERSION) ||
		(Header[2] != 0) ) {
		return __xrtNetProxyHandshakeFail(
			pHandshake, XERR_PROTOCOL, XNET_ERROR_PROXY_PROTOCOL,
			"connect-socks5", "SOCKS5 CONNECT reply header is invalid"
		);
	}
	pHandshake->Code = Header[1];
	pHandshake->HasCode = true;
	if ( Header[1] != XNET_SOCKS5_SUCCEEDED ) {
		return __xrtNetProxyHandshakeFail(
			pHandshake, XERR_IO, XNET_ERROR_PROXY_CONNECT,
			"connect-socks5", __xrtNetSocks5ReplyMessage(Header[1])
		);
	}
	if ( Header[3] == XRT_NET_SOCKS5_ADDRESS_IPV4 ) {
		iNeed = 10;
	} else if ( Header[3] == XRT_NET_SOCKS5_ADDRESS_IPV6 ) {
		iNeed = 22;
	} else if ( Header[3] == XRT_NET_SOCKS5_ADDRESS_HOST ) {
		if ( xrtNetBufSize(pInput) < sizeof(Header) ) {
			return XNET_PROXY_HANDSHAKE_READ;
		}
		(void)xrtNetBufPeek(pInput, 0, Header, sizeof(Header));
		if ( Header[4] == 0 ) {
			return __xrtNetProxyHandshakeFail(
				pHandshake, XERR_PROTOCOL, XNET_ERROR_PROXY_PROTOCOL,
				"connect-socks5", "SOCKS5 bound host is empty"
			);
		}
		iNeed = 7u + Header[4];
	} else {
		return __xrtNetProxyHandshakeFail(
			pHandshake, XERR_PROTOCOL, XNET_ERROR_PROXY_PROTOCOL,
			"connect-socks5", "SOCKS5 CONNECT reply address type is invalid"
		);
	}
	if ( iNeed > pHandshake->ReceiveLimit ) {
		return __xrtNetProxyHandshakeFail(
			pHandshake, XERR_RANGE, XNET_ERROR_PROXY_LIMIT,
			"connect-socks5", "SOCKS5 CONNECT reply exceeds the receive limit"
		);
	}
	if ( xrtNetBufSize(pInput) < iNeed ) {
		return XNET_PROXY_HANDSHAKE_READ;
	}
	(void)xrtNetBufPeek(pInput, 0, Reply, iNeed);
	if ( !__xrtNetSocks5Bound(pHandshake, Reply, iNeed) ) {
		xrtSecureZero(Reply, sizeof(Reply));
		return pHandshake->State;
	}
	(void)xrtNetBufConsume(pInput, iNeed);
	xrtSecureZero(Reply, sizeof(Reply));
	pHandshake->State = XNET_PROXY_HANDSHAKE_READY;
	return pHandshake->State;
}



/* 初始化 SOCKS5 方法协商。 */
bool __xrtNetProxySocks5Start(xnetproxyhandshake* pHandshake)
{
	if ( pHandshake == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pHandshake->Stage = XRT_NET_SOCKS5_STAGE_METHOD;
	return __xrtNetSocks5Greeting(pHandshake);
}



/* 按当前 SOCKS5 阶段增量消费一份服务器回复。 */
xnetproxyhandshakestate __xrtNetProxySocks5Step(
	xnetproxyhandshake* pHandshake,
	xnetbuf* pInput
)
{
	if ( (pHandshake == NULL) || (pInput == NULL) ) {
		return __xrtNetProxyHandshakeFail(
			pHandshake, XERR_ARGUMENT, XNET_ERROR_PROXY_PROTOCOL,
			"step-socks5", "SOCKS5 handshake or input is null"
		);
	}
	switch ( pHandshake->Stage ) {
		case XRT_NET_SOCKS5_STAGE_METHOD:
			return __xrtNetSocks5Method(pHandshake, pInput);
		case XRT_NET_SOCKS5_STAGE_AUTH:
			return __xrtNetSocks5AuthReply(pHandshake, pInput);
		case XRT_NET_SOCKS5_STAGE_CONNECT:
			return __xrtNetSocks5ConnectReply(pHandshake, pInput);
		default:
			return __xrtNetProxyHandshakeFail(
				pHandshake, XERR_INTERNAL, XNET_ERROR_PROXY_PROTOCOL,
				"step-socks5", "SOCKS5 handshake stage is invalid"
			);
	}
}

#endif
