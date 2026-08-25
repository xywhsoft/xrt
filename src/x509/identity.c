#include "../internal/xrt_x509.h"

#include <xrt/net.h>



#if defined(XRT_FEATURE_X509_IDENTITY)

/* 比较两个已经通过语法校验的 DNS 名称。 */
static bool __xrtX509IdentityDnsMatch(
	xstrview Pattern,
	size_t iPatternSize,
	bool bWildcard,
	xstrview Host,
	size_t iHostSize
)
{
	if ( !bWildcard ) {
		return (iPatternSize == iHostSize) &&
			__xrtX509AsciiEqual(
				(xbytesview) { (const uint8*)Pattern.Data, iPatternSize },
				(xbytesview) { (const uint8*)Host.Data, iHostSize }
			);
	}
	for ( size_t i = 0; i < iHostSize; i++ ) {
		if ( Host.Data[i] != '.' ) {
			continue;
		}
		return ((iHostSize - i) == (iPatternSize - 1u)) &&
			__xrtX509AsciiEqual(
				(xbytesview) {
					(const uint8*)Host.Data + i, iHostSize - i
				},
				(xbytesview) {
					(const uint8*)Pattern.Data + 1u, iPatternSize - 1u
				}
			);
	}
	return false;
}



/* 返回引用主机是否必须按 IPv4 文本解释。 */
static bool __xrtX509IdentityLooksIPv4(xstrview Host)
{
	bool bDot = false;

	for ( size_t i = 0; i < Host.Size; i++ ) {
		if ( Host.Data[i] == '.' ) {
			bDot = true;
			continue;
		}
		if ( (Host.Data[i] < '0') || (Host.Data[i] > '9') ) {
			return false;
		}
	}
	return bDot;
}



/* 从引用主机严格解析无 Scope 的 IPv4 或 IPv6 地址。 */
static bool __xrtX509IdentityIp(xstrview Host, xnetaddr* pAddress)
{
	char sAddress[64];
	const xerror* pCause;

	if ( (Host.Size == 0) || (Host.Size >= sizeof(sAddress)) ||
		(memchr(Host.Data, '%', Host.Size) != NULL) ) {
		__xrtX509Error(
			XERR_VALUE, X509_ERROR_IDENTITY, "x509-match-host",
			"reference IP address is empty, too long or contains a scope",
			SIZE_MAX, NULL
		);
		return false;
	}
	memcpy(sAddress, Host.Data, Host.Size);
	sAddress[Host.Size] = 0;
	if ( !xrtNetAddrParse(pAddress, sAddress, 0) ) {
		pCause = xrtGetError();
		__xrtX509Error(
			XERR_VALUE, X509_ERROR_IDENTITY, "x509-match-host",
			"reference host is not a valid numeric IP address",
			SIZE_MAX, pCause
		);
		return false;
	}
	return true;
}



/* 按 RFC 9525 比较一个证书 DNS pattern 与引用 DNS 名。 */
XRT_API xx509result xrtX509MatchDns(
	xstrview Pattern,
	xstrview Host
)
{
	size_t iPatternSize;
	size_t iHostSize;
	bool bWildcard;
	bool bHostWildcard;

	if ( ((Pattern.Data == NULL) && (Pattern.Size != 0)) ||
		((Host.Data == NULL) && (Host.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	if ( !__xrtX509DnsName(
		(xbytesview) { (const uint8*)Pattern.Data, Pattern.Size },
		true, &iPatternSize, &bWildcard
	) || !__xrtX509DnsName(
		(xbytesview) { (const uint8*)Host.Data, Host.Size },
		false, &iHostSize, &bHostWildcard
	) ) {
		__xrtX509Error(
			XERR_VALUE, X509_ERROR_DNS_NAME, "x509-match-dns",
			"DNS pattern or reference name has invalid labels",
			SIZE_MAX, NULL
		);
		return X509_ERROR;
	}
	return __xrtX509IdentityDnsMatch(
		Pattern, iPatternSize, bWildcard, Host, iHostSize
	) ? X509_VALUE : X509_DONE;
}



/* 在 SAN 的 DNS-ID 或 IP-ID 中匹配引用主机。 */
XRT_API xx509result xrtX509MatchHost(
	const xx509cert* pCert,
	xstrview Host,
	xx509genname* pName
)
{
	xx509gencursor Cursor;
	xx509genname Name;
	xx509result Result;
	xnetaddr Address = {0};
	xstrview Reference = Host;
	size_t iHostSize = 0;
	bool bWildcard = false;
	bool bIp;
	const xerror* pCause;

	if ( (pCert == NULL) ||
		((Host.Data == NULL) && (Host.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	if ( Host.Size == 0 ) {
		goto InvalidReference;
	}
	if ( (Reference.Size >= 2u) && (Reference.Data[0] == '[') &&
		(Reference.Data[Reference.Size - 1u] == ']') ) {
		Reference.Data++;
		Reference.Size -= 2u;
		if ( memchr(Reference.Data, ':', Reference.Size) == NULL ) {
			goto InvalidReference;
		}
	} else if ( (memchr(Reference.Data, '[', Reference.Size) != NULL) ||
		(memchr(Reference.Data, ']', Reference.Size) != NULL) ) {
		goto InvalidReference;
	}
	bIp = (memchr(Reference.Data, ':', Reference.Size) != NULL) ||
		__xrtX509IdentityLooksIPv4(Reference);
	if ( bIp ) {
		if ( !__xrtX509IdentityIp(Reference, &Address) ) {
			return X509_ERROR;
		}
	} else if ( !__xrtX509DnsName(
		(xbytesview) { (const uint8*)Reference.Data, Reference.Size },
		false, &iHostSize, &bWildcard
	) ) {
		goto InvalidReference;
	}
	Result = xrtX509SubjectAltName(pCert, &Cursor);
	if ( Result == X509_ERROR ) {
		pCause = xrtGetError();
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_IDENTITY, "x509-match-host",
			"certificate subjectAltName is malformed", SIZE_MAX, pCause
		);
		return X509_ERROR;
	}
	if ( Result == X509_DONE ) {
		return X509_DONE;
	}
	while ( (Result = xrtX509GeneralNameRead(
		&Cursor, &Name
	)) == X509_VALUE ) {
		bool bMatch = false;

		if ( bIp && (Name.Type == X509_NAME_IP) ) {
			size_t iSize = (Address.Family == XNET_FAMILY_IPV4) ? 4u : 16u;

			bMatch = (Name.Value.Size == iSize) &&
				(memcmp(Name.Value.Data, Address.Address, iSize) == 0);
		} else if ( !bIp && (Name.Type == X509_NAME_DNS) ) {
			size_t iPatternSize;
			bool bPatternWildcard;
			xstrview Pattern = {
				(const char*)Name.Value.Data,
				Name.Value.Size
			};

			if ( __xrtX509DnsName(
				(xbytesview) {
					(const uint8*)Pattern.Data, Pattern.Size
				}, true, &iPatternSize, &bPatternWildcard
			) ) {
				bMatch = __xrtX509IdentityDnsMatch(
					Pattern, iPatternSize, bPatternWildcard,
					Reference, iHostSize
				);
			}
		}
		if ( bMatch ) {
			if ( pName != NULL ) {
				*pName = Name;
			}
			return X509_VALUE;
		}
	}
	if ( Result == X509_ERROR ) {
		pCause = xrtGetError();
		__xrtX509Error(
			XERR_PROTOCOL, X509_ERROR_IDENTITY, "x509-match-host",
			"certificate subjectAltName traversal failed", SIZE_MAX, pCause
		);
		return X509_ERROR;
	}
	return X509_DONE;

InvalidReference:
	__xrtX509Error(
		XERR_VALUE, X509_ERROR_IDENTITY, "x509-match-host",
		"reference host is not a valid DNS name or IP address",
		SIZE_MAX, NULL
	);
	return X509_ERROR;
}

#endif
