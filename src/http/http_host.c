#include "../internal/xrt_http.h"



#if defined(XRT_FEATURE_HTTP_HOST)

/* 判断 ASCII 十进制数字。 */
static bool __xrtHttpAuthorityDigit(unsigned char iByte)
{
	return (iByte >= (unsigned char)'0') &&
		(iByte <= (unsigned char)'9');
}



/* 返回 ASCII 十六进制数字值，非法字节返回负数。 */
int __xrtHttpAuthorityHex(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'0') &&
		(iByte <= (unsigned char)'9') ) {
		return (int)(iByte - (unsigned char)'0');
	}
	if ( (iByte >= (unsigned char)'a') &&
		(iByte <= (unsigned char)'f') ) {
		return 10 + (int)(iByte - (unsigned char)'a');
	}
	if ( (iByte >= (unsigned char)'A') &&
		(iByte <= (unsigned char)'F') ) {
		return 10 + (int)(iByte - (unsigned char)'A');
	}
	return -1;
}



/* 判断 RFC 3986 reg-name 可直接使用的 ASCII 字节。 */
bool __xrtHttpAuthorityNameByte(unsigned char iByte)
{
	return ((iByte >= (unsigned char)'A') &&
		(iByte <= (unsigned char)'Z')) ||
		((iByte >= (unsigned char)'a') &&
		 (iByte <= (unsigned char)'z')) ||
		__xrtHttpAuthorityDigit(iByte) ||
		(iByte == (unsigned char)'-') ||
		(iByte == (unsigned char)'.') ||
		(iByte == (unsigned char)'_') ||
		(iByte == (unsigned char)'~') ||
		(iByte == (unsigned char)'!') ||
		(iByte == (unsigned char)'$') ||
		(iByte == (unsigned char)'&') ||
		(iByte == (unsigned char)'\'') ||
		(iByte == (unsigned char)'(') ||
		(iByte == (unsigned char)')') ||
		(iByte == (unsigned char)'*') ||
		(iByte == (unsigned char)'+') ||
		(iByte == (unsigned char)',') ||
		(iByte == (unsigned char)';') ||
		(iByte == (unsigned char)'=');
}



/* 严格验证 RFC 3986 IPv4 文本。 */
bool __xrtHttpAuthorityIpv4(xstrview Text)
{
	size_t i = 0;

	for ( size_t iPart = 0; iPart < 4u; iPart++ ) {
		uint32 iValue = 0;
		size_t iStart = i;

		while ( (i < Text.Size) &&
			__xrtHttpAuthorityDigit((unsigned char)Text.Data[i]) ) {
			iValue = (iValue * 10u) +
				(uint32)(Text.Data[i] - '0');
			i++;
		}
		if ( (i == iStart) || ((i - iStart) > 3u) ||
			(iValue > 255u) ||
			(((i - iStart) > 1u) && (Text.Data[iStart] == '0')) ) {
			return false;
		}
		if ( iPart == 3u ) {
			return i == Text.Size;
		}
		if ( (i >= Text.Size) || (Text.Data[i++] != '.') ) {
			return false;
		}
	}
	return false;
}



/* 严格验证 RFC 3986 IPv6 文本，不接受 ZoneID。 */
bool __xrtHttpAuthorityIpv6(xstrview Text)
{
	size_t i = 0;
	size_t iGroups = 0;
	bool bCompressed = false;

	if ( Text.Size == 0 ) {
		return false;
	}
	if ( Text.Data[0] == ':' ) {
		if ( (Text.Size < 2u) || (Text.Data[1] != ':') ) {
			return false;
		}
		bCompressed = true;
		i = 2u;
	}
	while ( i < Text.Size ) {
		size_t iStart = i;
		bool bIpv4 = false;

		while ( (i < Text.Size) && (Text.Data[i] != ':') ) {
			bIpv4 = bIpv4 || (Text.Data[i] == '.');
			i++;
		}
		if ( i == iStart ) {
			return false;
		}
		if ( bIpv4 ) {
			if ( (i != Text.Size) || !__xrtHttpAuthorityIpv4(
				(xstrview){ Text.Data + iStart, i - iStart }
			) ) {
				return false;
			}
			iGroups += 2u;
			break;
		}
		if ( (i - iStart) > 4u ) {
			return false;
		}
		for ( size_t j = iStart; j < i; j++ ) {
			if ( __xrtHttpAuthorityHex(
				(unsigned char)Text.Data[j]
			) < 0 ) {
				return false;
			}
		}
		iGroups++;
		if ( i == Text.Size ) {
			break;
		}
		i++;
		if ( i == Text.Size ) {
			return false;
		}
		if ( Text.Data[i] == ':' ) {
			if ( bCompressed ) {
				return false;
			}
			bCompressed = true;
			i++;
			if ( i == Text.Size ) {
				break;
			}
		}
	}
	return bCompressed ? (iGroups < 8u) : (iGroups == 8u);
}



/* 严格验证 RFC 3986 IPvFuture 文本。 */
static bool __xrtHttpAuthorityIpvFuture(xstrview Text)
{
	size_t i = 1u;
	size_t iVersion = i;

	if ( (Text.Size < 4u) ||
		((Text.Data[0] != 'v') && (Text.Data[0] != 'V')) ) {
		return false;
	}
	while ( (i < Text.Size) && (__xrtHttpAuthorityHex(
		(unsigned char)Text.Data[i]
	) >= 0) ) {
		i++;
	}
	if ( (i == iVersion) || (i >= Text.Size) ||
		(Text.Data[i++] != '.') || (i == Text.Size) ) {
		return false;
	}
	for ( ; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( !__xrtHttpAuthorityNameByte(iByte) &&
			(iByte != (unsigned char)':') ) {
			return false;
		}
	}
	return true;
}



/* 验证 reg-name，百分号转义必须完整且使用十六进制数字。 */
static bool __xrtHttpAuthorityName(xstrview Text)
{
	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( __xrtHttpAuthorityNameByte(iByte) ) {
			continue;
		}
		if ( (iByte != (unsigned char)'%') ||
			((i + 2u) >= Text.Size) ||
			(__xrtHttpAuthorityHex(
				(unsigned char)Text.Data[i + 1u]
			) < 0) || (__xrtHttpAuthorityHex(
				(unsigned char)Text.Data[i + 2u]
			) < 0) ) {
			return false;
		}
		i += 2u;
	}
	return true;
}



/* 解析十进制端口并保留超出 uint16 的合法协议文本。 */
static bool __xrtHttpAuthorityPortParse(
	xstrview Text,
	uint16* pPort,
	bool* pValue
)
{
	uint32 iValue = 0;
	bool bValue = Text.Size != 0;

	for ( size_t i = 0; i < Text.Size; i++ ) {
		uint32 iDigit;

		if ( !__xrtHttpAuthorityDigit(
			(unsigned char)Text.Data[i]
		) ) {
			return false;
		}
		iDigit = (uint32)(Text.Data[i] - '0');
		if ( bValue ) {
			if ( iValue > ((UINT16_MAX - iDigit) / 10u) ) {
				bValue = false;
				iValue = 0;
			} else {
				iValue = (iValue * 10u) + iDigit;
			}
		}
	}
	*pPort = bValue ? (uint16)iValue : 0;
	*pValue = bValue;
	return true;
}



/* 无错误副作用地解析一个不含 userinfo 的 HTTP authority。 */
static bool __xrtHttpAuthorityParseValue(
	xstrview Value,
	xhttpauthority* pAuthority
)
{
	xhttpauthority Authority = { 0 };
	size_t iHostEnd = Value.Size;
	size_t iPortStart = XRT_NPOS;
	bool bPortValue;

	Authority.Text = Value;
	if ( (Value.Size != 0) && (Value.Data[0] == '[') ) {
		size_t iClose = 1u;

		while ( (iClose < Value.Size) && (Value.Data[iClose] != ']') ) {
			iClose++;
		}
		if ( (iClose >= Value.Size) || (iClose == 1u) ) {
			return false;
		}
		Authority.Host = (xstrview){ Value.Data + 1u, iClose - 1u };
		if ( !(((Authority.Host.Data[0] == 'v') ||
			(Authority.Host.Data[0] == 'V')) ?
			__xrtHttpAuthorityIpvFuture(Authority.Host) :
			__xrtHttpAuthorityIpv6(Authority.Host)) ) {
			return false;
		}
		Authority.Flags |= XHTTP_AUTHORITY_IP_LITERAL;
		if ( (iClose + 1u) < Value.Size ) {
			if ( Value.Data[iClose + 1u] != ':' ) {
				return false;
			}
			iPortStart = iClose + 2u;
		} else if ( (iClose + 1u) != Value.Size ) {
			return false;
		}
	} else {
		size_t iColon = XRT_NPOS;

		for ( size_t i = 0; i < Value.Size; i++ ) {
			if ( Value.Data[i] == '@' ) {
				return false;
			}
			if ( Value.Data[i] != ':' ) {
				continue;
			}
			if ( iColon != XRT_NPOS ) {
				return false;
			}
			iColon = i;
		}
		if ( iColon != XRT_NPOS ) {
			iHostEnd = iColon;
			iPortStart = iColon + 1u;
		}
		Authority.Host = (xstrview){ Value.Data, iHostEnd };
		if ( !__xrtHttpAuthorityName(Authority.Host) ) {
			return false;
		}
	}
	if ( iPortStart != XRT_NPOS ) {
		Authority.PortText = (xstrview){
			Value.Data + iPortStart, Value.Size - iPortStart
		};
		if ( !__xrtHttpAuthorityPortParse(
			Authority.PortText, &Authority.Port, &bPortValue
		) ) {
			return false;
		}
		Authority.Flags |= XHTTP_AUTHORITY_HAS_PORT;
		if ( Authority.PortText.Size == 0 ) {
			Authority.Flags |= XHTTP_AUTHORITY_PORT_EMPTY;
		} else if ( bPortValue ) {
			Authority.Flags |= XHTTP_AUTHORITY_PORT_VALUE;
		}
	}
	*pAuthority = Authority;
	return true;
}



/* 解析 Host 字段使用的借用型 authority。 */
XRT_API bool xrtHttpHostParse(
	xstrview Value,
	xhttpauthority* pHost
)
{
	xhttpauthority Host = { 0 };

	if ( !__xrtRangeValid(pHost, sizeof(Host)) ||
		!__xrtHttpViewValid(Value) || __xrtRangesOverlap(
			pHost, sizeof(Host), Value.Data, Value.Size
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpAuthorityParseValue(Value, &Host) ) {
		memcpy(pHost, &Host, sizeof(Host));
		__xrtErrorSetValue();
		return false;
	}
	memcpy(pHost, &Host, sizeof(Host));
	return true;
}



/* 验证 Host 字段值。 */
XRT_API bool xrtHttpHostValid(xstrview Value)
{
	xhttpauthority Host;

	return xrtHttpHostParse(Value, &Host);
}



/* 严格验证 RFC 3986 IPv4 文本。 */
XRT_API bool xrtHttpIpv4Valid(xstrview Value)
{
	return __xrtHttpViewValid(Value) &&
		__xrtHttpAuthorityIpv4(Value);
}



/* 严格验证 RFC 3986 IPv6 文本。 */
XRT_API bool xrtHttpIpv6Valid(xstrview Value)
{
	return __xrtHttpViewValid(Value) &&
		__xrtHttpAuthorityIpv6(Value);
}



/* 按 HTTP 的 ASCII 大小写规则比较两个 Host 视图。 */
XRT_API bool xrtHttpHostEqual(xstrview Left, xstrview Right)
{
	return xrtHttpTokenEqual(Left, Right);
}



/* 验证已经拆分的 authority 字段，允许各视图来自不同存储。 */
static bool __xrtHttpAuthorityValueValid(
	const xhttpauthority* pAuthority
)
{
	const uint32 iKnown = XHTTP_AUTHORITY_HAS_PORT |
		XHTTP_AUTHORITY_IP_LITERAL |
		XHTTP_AUTHORITY_PORT_EMPTY |
		XHTTP_AUTHORITY_PORT_VALUE;
	uint16 iPort = 0;
	bool bPortValue = false;

	if ( (pAuthority->Flags & ~iKnown) != 0 ) {
		return false;
	}
	if ( (pAuthority->Flags & XHTTP_AUTHORITY_IP_LITERAL) != 0 ) {
		if ( !(((pAuthority->Host.Size != 0) &&
			((pAuthority->Host.Data[0] == 'v') ||
			 (pAuthority->Host.Data[0] == 'V'))) ?
			__xrtHttpAuthorityIpvFuture(pAuthority->Host) :
			__xrtHttpAuthorityIpv6(pAuthority->Host)) ) {
			return false;
		}
	} else if ( !__xrtHttpAuthorityName(pAuthority->Host) ) {
		return false;
	}
	if ( (pAuthority->Flags & XHTTP_AUTHORITY_HAS_PORT) == 0 ) {
		return ((pAuthority->Flags & (XHTTP_AUTHORITY_PORT_EMPTY |
			XHTTP_AUTHORITY_PORT_VALUE)) == 0) &&
			(pAuthority->Port == 0) &&
			(pAuthority->PortText.Size == 0);
	}
	if ( pAuthority->PortText.Size == 0 ) {
		if ( (pAuthority->Flags & XHTTP_AUTHORITY_PORT_EMPTY) != 0 ) {
			return ((pAuthority->Flags &
				XHTTP_AUTHORITY_PORT_VALUE) == 0) &&
				(pAuthority->Port == 0);
		}
		return (pAuthority->Flags &
			XHTTP_AUTHORITY_PORT_VALUE) != 0;
	}
	if ( !__xrtHttpAuthorityPortParse(
		pAuthority->PortText, &iPort, &bPortValue
	) ) {
		return false;
	}
	return ((pAuthority->Flags & XHTTP_AUTHORITY_PORT_EMPTY) == 0) &&
		(bPortValue == ((pAuthority->Flags &
			XHTTP_AUTHORITY_PORT_VALUE) != 0)) &&
		(bPortValue ? (pAuthority->Port == iPort) :
			(pAuthority->Port == 0));
}



/* 验证拆分后的 authority 结构。 */
XRT_API bool xrtHttpAuthorityValid(
	const xhttpauthority* pAuthority
)
{
	xhttpauthority Authority;

	if ( !__xrtRangeValid(pAuthority, sizeof(Authority)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Authority, pAuthority, sizeof(Authority));
	if ( !__xrtHttpViewValid(Authority.Text) ||
		!__xrtHttpViewValid(Authority.Host) ||
		!__xrtHttpViewValid(Authority.PortText) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtHttpAuthorityValueValid(&Authority) ) {
		__xrtErrorSetValue();
		return false;
	}
	return true;
}



/* 取得可直接交给网络层的显式或默认端口。 */
XRT_API bool xrtHttpAuthorityPort(
	const xhttpauthority* pAuthority,
	uint16 iDefaultPort,
	uint16* pPort
)
{
	xhttpauthority Authority;
	uint16 iPort;

	if ( !__xrtRangeValid(pAuthority, sizeof(Authority)) ||
		!__xrtRangeValid(pPort, sizeof(iPort)) || __xrtRangesOverlap(
			pAuthority, sizeof(Authority), pPort, sizeof(iPort)
		) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Authority, pAuthority, sizeof(Authority));
	if ( !__xrtHttpViewValid(Authority.Text) ||
		!__xrtHttpViewValid(Authority.Host) ||
		!__xrtHttpViewValid(Authority.PortText) ||
		!__xrtHttpAuthorityValueValid(&Authority) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( ((Authority.Flags & XHTTP_AUTHORITY_HAS_PORT) != 0) &&
		((Authority.Flags & XHTTP_AUTHORITY_PORT_EMPTY) == 0) ) {
		if ( (Authority.Flags & XHTTP_AUTHORITY_PORT_VALUE) == 0 ) {
			__xrtErrorSetRange();
			return false;
		}
		iPort = Authority.Port;
	} else {
		iPort = iDefaultPort;
	}
	memcpy(pPort, &iPort, sizeof(iPort));
	return true;
}



#endif
