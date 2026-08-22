#include "../internal/xrt_proxy.h"

#include <xrt/codec.h>
#include <xrt/http1.h>



#if defined(XRT_FEATURE_NET_PROXY_HTTP_CONNECT)

#define XRT_NET_PROXY_HTTP_INTERIM_LIMIT 8u



/* 安全累加 HTTP CONNECT 临时存储和报文长度。 */
static bool __xrtNetProxyHttpAddSize(size_t* pSize, size_t iAdd)
{
	if ( iAdd > (SIZE_MAX - *pSize) ) {
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 返回十进制端口的字符数量。 */
static size_t __xrtNetProxyHttpPortSize(uint16 iPort)
{
	if ( iPort >= 10000 ) {
		return 5;
	}
	if ( iPort >= 1000 ) {
		return 4;
	}
	if ( iPort >= 100 ) {
		return 3;
	}
	if ( iPort >= 10 ) {
		return 2;
	}
	return 1;
}



/* 从右向左写入已经确认非零的十进制端口。 */
static void __xrtNetProxyHttpPortWrite(
	char* sOutput,
	size_t iSize,
	uint16 iPort
)
{
	while ( iSize != 0 ) {
		sOutput[--iSize] = (char)('0' + (iPort % 10));
		iPort = (uint16)(iPort / 10);
	}
}



/*
	验证目标是纯主机而不是 URL，并识别需要方括号的 IPv6 字面量。
	网络作用域中的百分号会在线路 authority 中编码为 RFC 6874 的 %25。
*/
static bool __xrtNetProxyHttpHost(
	const xnetproxyhandshake* pHandshake,
	xstrview* pHost,
	bool* pIpv6,
	bool* pScope
)
{
	xstrview Host;
	xnetaddr Address;
	bool bBracketed;
	size_t i;

	Host.Data = pHandshake->TargetHost;
	Host.Size = pHandshake->TargetSize;
	bBracketed = (Host.Size >= 2) &&
		(Host.Data[0] == '[') &&
		(Host.Data[Host.Size - 1u] == ']');
	if ( bBracketed ) {
		Host.Data++;
		Host.Size -= 2u;
	}
	if ( Host.Size == 0 ) {
		return false;
	}
	if ( memchr(Host.Data, ':', Host.Size) != NULL ) {
		if ( !__xrtNetAddrTryParse(&Address, Host, 0) ||
			(Address.Family != XNET_FAMILY_IPV6) ) {
			return false;
		}
		*pHost = Host;
		*pIpv6 = true;
		*pScope = memchr(Host.Data, '%', Host.Size) != NULL;
		return true;
	}
	if ( bBracketed ) {
		return false;
	}
	for ( i = 0; i < Host.Size; i++ ) {
		unsigned char iByte = (unsigned char)Host.Data[i];

		if ( (iByte <= UINT8_C(0x20)) ||
			(iByte == UINT8_C(0x7F)) ||
			(iByte == (unsigned char)'/') ||
			(iByte == (unsigned char)'?') ||
			(iByte == (unsigned char)'#') ||
			(iByte == (unsigned char)'@') ||
			(iByte == (unsigned char)'[') ||
			(iByte == (unsigned char)']') ) {
			return false;
		}
	}
	*pHost = Host;
	*pIpv6 = false;
	*pScope = false;
	return true;
}



/* 计算规范 authority 的精确长度。 */
static bool __xrtNetProxyHttpAuthoritySize(
	xstrview Host,
	bool bIpv6,
	bool bScope,
	uint16 iPort,
	size_t* pSize
)
{
	size_t iSize = Host.Size;

	if ( bIpv6 && !__xrtNetProxyHttpAddSize(&iSize, 2) ) {
		return false;
	}
	if ( bScope && !__xrtNetProxyHttpAddSize(&iSize, 2) ) {
		return false;
	}
	if ( !__xrtNetProxyHttpAddSize(
		&iSize, 1u + __xrtNetProxyHttpPortSize(iPort)
	) ) {
		return false;
	}
	*pSize = iSize;
	return true;
}



/* 写入 host:port；IPv6 使用方括号，作用域百分号编码为 %25。 */
static void __xrtNetProxyHttpAuthorityWrite(
	char* sOutput,
	xstrview Host,
	bool bIpv6,
	bool bScope,
	uint16 iPort
)
{
	size_t iPosition = 0;
	size_t i;
	size_t iPortSize = __xrtNetProxyHttpPortSize(iPort);

	if ( bIpv6 ) {
		sOutput[iPosition++] = '[';
	}
	for ( i = 0; i < Host.Size; i++ ) {
		if ( bScope && (Host.Data[i] == '%') ) {
			sOutput[iPosition++] = '%';
			sOutput[iPosition++] = '2';
			sOutput[iPosition++] = '5';
		} else {
			sOutput[iPosition++] = Host.Data[i];
		}
	}
	if ( bIpv6 ) {
		sOutput[iPosition++] = ']';
	}
	sOutput[iPosition++] = ':';
	__xrtNetProxyHttpPortWrite(
		sOutput + iPosition, iPortSize, iPort
	);
}



/* 计算规范 Base64 输出长度，拒绝全部中间算术溢出。 */
static bool __xrtNetProxyHttpBase64Size(
	size_t iInput,
	size_t* pOutput
)
{
	size_t iGroups;

	if ( iInput > (SIZE_MAX - 2u) ) {
		return false;
	}
	iGroups = (iInput + 2u) / 3u;
	if ( iGroups > (SIZE_MAX / 4u) ) {
		return false;
	}
	*pOutput = iGroups * 4u;
	return true;
}



/* 清理敏感临时存储，并在可能改变当前错误前持有底层原因。 */
static bool __xrtNetProxyHttpRequestFail(
	xnetproxyhandshake* pHandshake,
	char* sScratch,
	size_t iScratch,
	bool bCancelOutput,
	xerrkind Kind,
	xneterror Code,
	cstr sMessage
)
{
	const xerror* pCurrent = xrtGetError();
	xerror* pCause = pCurrent != NULL ? xrtErrorRef(pCurrent) : NULL;

	if ( bCancelOutput ) {
		(void)xrtNetBufCancel(&pHandshake->Output);
	}
	xrtSecureZero(sScratch, iScratch);
	xrtFree(sScratch);
	(void)__xrtNetProxyHandshakeFailCause(
		pHandshake, Kind, Code,
		"start-http-connect", sMessage, pCause
	);
	xrtErrorFree(pCause);
	return false;
}



/* 把完整 CONNECT Header 直接写入握手输出缓冲，避免二次报文复制。 */
static bool __xrtNetProxyHttpRequest(
	xnetproxyhandshake* pHandshake
)
{
	xnetproxyinfo Info;
	xstrview Host;
	xstrview Authority;
	xhttpfield Fields[2];
	xnetwspan Output;
	char* sScratch;
	char* sCredentials;
	char* sAuthorization;
	size_t iAuthority;
	size_t iCredentials = 0;
	size_t iEncoded = 0;
	size_t iScratch;
	size_t iRequest;
	size_t iWritten;
	size_t iFieldCount = 1;
	bool bIpv6;
	bool bScope;
	bool bAuth;
	const xerror* pCause;

	if ( !xrtNetProxyInfo(pHandshake->Proxy, &Info) ||
		!__xrtNetProxyHttpHost(
			pHandshake, &Host, &bIpv6, &bScope
		) || !__xrtNetProxyHttpAuthoritySize(
			Host, bIpv6, bScope,
			pHandshake->TargetPort, &iAuthority
		) ) {
		(void)__xrtNetProxyHandshakeFail(
			pHandshake, XERR_VALUE, XNET_ERROR_PROXY_CONFIG,
			"start-http-connect",
			"HTTP CONNECT target host is not a valid authority"
		);
		return false;
	}
	bAuth = Info.Auth != XNET_PROXY_AUTH_NONE;
	iScratch = iAuthority;
	if ( bAuth ) {
		if ( (Info.Username.Size > (SIZE_MAX - 1u)) ||
			((Info.Username.Size + 1u) >
			 (SIZE_MAX - Info.Password.Size)) ) {
			(void)__xrtNetProxyHandshakeFail(
				pHandshake, XERR_RANGE, XNET_ERROR_PROXY_LIMIT,
				"start-http-connect",
				"HTTP Basic credential size overflows"
			);
			return false;
		}
		iCredentials = Info.Username.Size + 1u + Info.Password.Size;
		if ( !__xrtNetProxyHttpBase64Size(
			iCredentials, &iEncoded
		) || !__xrtNetProxyHttpAddSize(
			&iScratch, iCredentials
		) || !__xrtNetProxyHttpAddSize(
			&iScratch, 6u + iEncoded + 1u
		) ) {
			(void)__xrtNetProxyHandshakeFail(
				pHandshake, XERR_RANGE, XNET_ERROR_PROXY_LIMIT,
				"start-http-connect",
				"HTTP Basic authorization size overflows"
			);
			return false;
		}
	}
	sScratch = (char*)xrtMalloc(iScratch);
	if ( sScratch == NULL ) {
		pCause = xrtGetError();
		(void)__xrtNetProxyHandshakeFailCause(
			pHandshake, XERR_MEMORY, XNET_ERROR_PROXY_CREATE,
			"start-http-connect",
			"HTTP CONNECT scratch allocation failed",
			pCause
		);
		return false;
	}
	__xrtNetProxyHttpAuthorityWrite(
		sScratch, Host, bIpv6, bScope, pHandshake->TargetPort
	);
	Authority.Data = sScratch;
	Authority.Size = iAuthority;
	Fields[0].Name = XRT_STR_LITERAL("Host");
	Fields[0].Value = Authority;
	if ( bAuth ) {
		sCredentials = sScratch + iAuthority;
		sAuthorization = sCredentials + iCredentials;
		if ( Info.Username.Size != 0 ) {
			memcpy(
				sCredentials,
				Info.Username.Data,
				Info.Username.Size
			);
		}
		sCredentials[Info.Username.Size] = ':';
		if ( Info.Password.Size != 0 ) {
			memcpy(
				sCredentials + Info.Username.Size + 1u,
				Info.Password.Data,
				Info.Password.Size
			);
		}
		memcpy(sAuthorization, "Basic ", 6);
		if ( !xrtBase64Encode(
			sCredentials,
			iCredentials,
			sAuthorization + 6u,
			iEncoded + 1u,
			&iWritten,
			NULL
		) || (iWritten != iEncoded) ) {
			return __xrtNetProxyHttpRequestFail(
				pHandshake, sScratch, iScratch, false,
				XERR_PROTOCOL, XNET_ERROR_PROXY_AUTH,
				"HTTP Basic authorization encoding failed"
			);
		}
		Fields[1].Name = XRT_STR_LITERAL("Proxy-Authorization");
		Fields[1].Value.Data = sAuthorization;
		Fields[1].Value.Size = 6u + iEncoded;
		iFieldCount = 2;
	}
	if ( !xrtHttp1RequestWrite(
		XRT_STR_LITERAL("CONNECT"),
		Authority,
		XHTTP_VERSION_1_1,
		Fields,
		iFieldCount,
		NULL,
		0,
		&iRequest
	) ) {
		return __xrtNetProxyHttpRequestFail(
			pHandshake, sScratch, iScratch, false,
			XERR_PROTOCOL, XNET_ERROR_PROXY_PROTOCOL,
			"HTTP CONNECT request validation failed"
		);
	}
	if ( !xrtNetBufReserve(
		&pHandshake->Output, iRequest, &Output
	) ) {
		return __xrtNetProxyHttpRequestFail(
			pHandshake, sScratch, iScratch, false,
			XERR_MEMORY, XNET_ERROR_PROXY_CREATE,
			"HTTP CONNECT output allocation failed"
		);
	}
	if ( !xrtHttp1RequestWrite(
		XRT_STR_LITERAL("CONNECT"),
		Authority,
		XHTTP_VERSION_1_1,
		Fields,
		iFieldCount,
		Output.Data,
		Output.Size,
		&iWritten
	) || (iWritten != iRequest) ) {
		return __xrtNetProxyHttpRequestFail(
			pHandshake, sScratch, iScratch, true,
			XERR_PROTOCOL, XNET_ERROR_PROXY_PROTOCOL,
			"HTTP CONNECT request construction failed"
		);
	}
	if ( !xrtNetBufCommit(&pHandshake->Output, iWritten) ) {
		return __xrtNetProxyHttpRequestFail(
			pHandshake, sScratch, iScratch, true,
			XERR_MEMORY, XNET_ERROR_PROXY_CREATE,
			"HTTP CONNECT output commit failed"
		);
	}
	xrtSecureZero(sScratch, iScratch);
	xrtFree(sScratch);
	pHandshake->State = XNET_PROXY_HANDSHAKE_WRITE;
	return true;
}



/*
	从上次检查位置寻找 Header 空行。
	返回一表示找到，零表示等待，负一表示已经设置限额错误。
*/
static int __xrtNetProxyHttpFindHead(
	xnetproxyhandshake* pHandshake,
	xnetbuf* pInput,
	size_t* pHeadBytes
)
{
	static const uint8 End[] = { '\r', '\n', '\r', '\n' };
	size_t iSize = xrtNetBufSize(pInput);
	size_t iPosition = pHandshake->ScanOffset;

	if ( iPosition > iSize ) {
		iPosition = 0;
	}
	for ( ;; ) {
		uint8 Candidate[4];
		size_t iFound = xrtNetBufFind(
			pInput, (uint8)'\r', iPosition
		);

		if ( iFound == XRT_NPOS ) {
			pHandshake->ScanOffset = iSize;
			break;
		}
		if ( (iFound + sizeof(Candidate)) > iSize ) {
			pHandshake->ScanOffset = iFound;
			break;
		}
		(void)xrtNetBufPeek(
			pInput, iFound, Candidate, sizeof(Candidate)
		);
		if ( memcmp(Candidate, End, sizeof(End)) == 0 ) {
			*pHeadBytes = iFound + sizeof(End);
			if ( *pHeadBytes > pHandshake->ReceiveLimit ) {
				(void)__xrtNetProxyHandshakeFail(
					pHandshake, XERR_RANGE, XNET_ERROR_PROXY_LIMIT,
					"read-http-connect",
					"HTTP CONNECT response Header exceeds the receive limit"
				);
				return -1;
			}
			return 1;
		}
		iPosition = iFound + 1u;
		pHandshake->ScanOffset = iPosition;
		if ( iPosition >= pHandshake->ReceiveLimit ) {
			break;
		}
	}
	if ( iSize > pHandshake->ReceiveLimit ) {
		(void)__xrtNetProxyHandshakeFail(
			pHandshake, XERR_RANGE, XNET_ERROR_PROXY_LIMIT,
			"read-http-connect",
			"HTTP CONNECT response Header exceeds the receive limit"
		);
		return -1;
	}
	return 0;
}



/* 生成标准 HTTP/1.1 CONNECT 请求并进入响应阶段。 */
bool __xrtNetProxyHttpStart(xnetproxyhandshake* pHandshake)
{
	if ( pHandshake == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pHandshake->Stage = 1;
	return __xrtNetProxyHttpRequest(pHandshake);
}



/* 使用共享 HTTP/1 解析器增量消费代理响应和有限数量的 1xx。 */
xnetproxyhandshakestate __xrtNetProxyHttpStep(
	xnetproxyhandshake* pHandshake,
	xnetbuf* pInput
)
{
	if ( (pHandshake == NULL) || (pInput == NULL) ) {
		return __xrtNetProxyHandshakeFail(
			pHandshake, XERR_ARGUMENT, XNET_ERROR_PROXY_PROTOCOL,
			"read-http-connect",
			"HTTP CONNECT handshake or input is null"
		);
	}
	for ( ;; ) {
		xnetspan Span;
		xhttp1limits Limits;
		xhttp1errorinfo ParseError;
		xhttp1head Head;
		xhttp1status Status;
		size_t iHeadBytes;
		int iFound = __xrtNetProxyHttpFindHead(
			pHandshake, pInput, &iHeadBytes
		);

		if ( iFound < 0 ) {
			return pHandshake->State;
		}
		if ( iFound == 0 ) {
			return XNET_PROXY_HANDSHAKE_READ;
		}
		if ( !xrtNetBufPullup(pInput, iHeadBytes, &Span) ) {
			return __xrtNetProxyHandshakeFailCause(
				pHandshake, XERR_MEMORY, XNET_ERROR_PROXY_CREATE,
				"read-http-connect",
				"HTTP CONNECT response pullup failed",
				xrtGetError()
			);
		}
		xrtHttp1LimitsInit(&Limits);
		Limits.MaxHead = pHandshake->ReceiveLimit;
		xrtHttp1HeadInit(&Head, NULL, 0);
		Status = xrtHttp1ResponseParse(
			(xbytesview){ Span.Data, iHeadBytes },
			&Head,
			&Limits,
			&ParseError
		);
		if ( (Status != XHTTP1_READY) &&
			(Status != XHTTP1_FIELDS) ) {
			return __xrtNetProxyHandshakeFailCause(
				pHandshake, XERR_PROTOCOL, XNET_ERROR_PROXY_PROTOCOL,
				"read-http-connect",
				"HTTP CONNECT response is malformed",
				xrtGetError()
			);
		}
		pHandshake->Code = Head.Status;
		pHandshake->HasCode = true;
		if ( (Head.Status >= 100) && (Head.Status < 200) &&
			(Head.Status != 101) ) {
			pHandshake->InterimCount++;
			if ( pHandshake->InterimCount >
				XRT_NET_PROXY_HTTP_INTERIM_LIMIT ) {
				return __xrtNetProxyHandshakeFail(
					pHandshake, XERR_PROTOCOL,
					XNET_ERROR_PROXY_PROTOCOL,
					"read-http-connect",
					"HTTP CONNECT sent too many interim responses"
				);
			}
			(void)xrtNetBufConsume(pInput, iHeadBytes);
			pHandshake->ScanOffset = 0;
			continue;
		}
		if ( (Head.Status >= 200) && (Head.Status < 300) ) {
			(void)xrtNetBufConsume(pInput, iHeadBytes);
			pHandshake->ScanOffset = 0;
			pHandshake->State = XNET_PROXY_HANDSHAKE_READY;
			return pHandshake->State;
		}
		if ( Head.Status == 407 ) {
			return __xrtNetProxyHandshakeFail(
				pHandshake, XERR_PERMISSION, XNET_ERROR_PROXY_AUTH,
				"read-http-connect",
				"HTTP CONNECT proxy authentication failed"
			);
		}
		return __xrtNetProxyHandshakeFail(
			pHandshake,
			(Head.Status == 101) ? XERR_PROTOCOL : XERR_IO,
			(Head.Status == 101) ? XNET_ERROR_PROXY_PROTOCOL :
				XNET_ERROR_PROXY_CONNECT,
			"read-http-connect",
			(Head.Status == 101) ?
				"HTTP CONNECT cannot switch protocols with status 101" :
				"HTTP CONNECT proxy rejected the tunnel"
		);
	}
}

#endif
