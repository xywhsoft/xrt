#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_HELLO)
/* HelloRetryRequest 使用 RFC 8446 固定 random 标记。 */
static const uint8 __xrtTlsRetryRandom[XTLS_RANDOM_SIZE] = {
	0xCF, 0x21, 0xAD, 0x74, 0xE5, 0x9A, 0x61, 0x11,
	0xBE, 0x1D, 0x8C, 0x02, 0x1E, 0x65, 0xB8, 0x91,
	0xC2, 0xA2, 0x11, 0x16, 0x7A, 0xBB, 0x8C, 0x5E,
	0x07, 0x9E, 0x09, 0xE2, 0xC8, 0xA8, 0x33, 0x9C
};



/* 判断 32 字节 random 是否是 HelloRetryRequest 固定标记。 */
bool __xrtTlsHelloRetry(xbytesview Random)
{
	return (Random.Size == XTLS_RANDOM_SIZE) &&
		(Random.Data != NULL) &&
		(memcmp(
			Random.Data, __xrtTlsRetryRandom, XTLS_RANDOM_SIZE
		) == 0);
}


/* 解析带一字节长度前缀的完整字节向量。 */
static bool __xrtTlsByteVector(
	xbytesview Data,
	bool bAllowEmpty,
	cstr sOperation
)
{
	if ( !__xrtTlsViewValid(Data) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, sOperation,
			"TLS byte vector input is invalid", SIZE_MAX
		);
		return false;
	}
	if ( (Data.Size < 1u) ||
		((!bAllowEmpty) && (Data.Data[0] == 0)) ||
		((size_t)Data.Data[0] != Data.Size - 1u) ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_EXTENSION, sOperation,
			"TLS byte vector length is inconsistent", 0
		);
	}
	return true;
}


bool __xrtTlsHelloExtensions(
	xbytesview Extensions,
	bool bServer,
	bool bRetry
)
{
	xtlsextensioncursor Cursor;
	xtlsextension Extension;
	xtlsitemresult Result;

	if ( !xrtTlsExtensionsInit(&Cursor, Extensions) ) {
		return false;
	}
	while ( (Result = xrtTlsExtensionsRead(
		&Cursor, &Extension
	)) == XTLS_ITEM_VALUE ) {
		switch ( Extension.Type ) {
			case XTLS_EXTENSION_SERVER_NAME:
				if ( bServer ) {
					if ( Extension.Data.Size != 0 ) {
						return __xrtTlsHelloError(
							XTLS_ERROR_EXTENSION, "parse-server-hello",
							"TLS server-name acknowledgement must be empty",
							Cursor.Offset - Extension.EncodedSize
						);
					}
				} else {
					xtlsservernamecursor Names;

					if ( !xrtTlsServerNames(Extension.Data, &Names) ) {
						return false;
					}
				}
				break;

			case XTLS_EXTENSION_ALPN:
				if ( bServer ) {
					xbytesview Protocol;

					if ( !xrtTlsProtocolSelected(
						Extension.Data, &Protocol
					) ) {
						return false;
					}
				} else {
					xtlsprotocolcursor Protocols;

					if ( !xrtTlsProtocols(Extension.Data, &Protocols) ) {
						return false;
					}
				}
				break;

			case XTLS_EXTENSION_SUPPORTED_VERSIONS:
				if ( bServer ) {
					uint16 iVersion;

					if ( !xrtTlsServerVersion(
						Extension.Data, &iVersion
					) ) {
						return false;
					}
				} else {
					xtlsids Versions;

					if ( !xrtTlsClientVersions(
						Extension.Data, &Versions
					) ) {
						return false;
					}
				}
				break;

			case XTLS_EXTENSION_SUPPORTED_GROUPS:
				if ( !bServer ) {
					xtlsids Groups;

					if ( !xrtTlsGroups(Extension.Data, &Groups) ) {
						return false;
					}
				}
				break;

			case XTLS_EXTENSION_SIGNATURE_ALGORITHMS:
			case XTLS_EXTENSION_SIGNATURE_ALGORITHMS_CERT:
				if ( !bServer ) {
					xtlsids Signatures;

					if ( !xrtTlsSignatures(
						Extension.Data, &Signatures
					) ) {
						return false;
					}
				}
				break;

			case XTLS_EXTENSION_KEY_SHARE:
				if ( bServer ) {
					if ( bRetry ) {
						uint16 iGroup;

						if ( !xrtTlsRetryGroup(
							Extension.Data, &iGroup
						) ) {
							return false;
						}
					} else {
						xtlskeyshare Share;

						if ( !xrtTlsServerKeyShare(
							Extension.Data, &Share
						) ) {
							return false;
						}
					}
				} else {
					xtlskeysharecursor Shares;

					if ( !xrtTlsClientKeyShares(
						Extension.Data, &Shares
					) ) {
						return false;
					}
				}
				break;

			case XTLS_EXTENSION_EC_POINT_FORMATS:
			case XTLS_EXTENSION_PSK_KEY_EXCHANGE_MODES:
				if ( !__xrtTlsByteVector(
					Extension.Data, false, "parse-hello-extension"
				) ) {
					return false;
				}
				break;

			case XTLS_EXTENSION_RENEGOTIATION_INFO:
				if ( !__xrtTlsByteVector(
					Extension.Data, true, "parse-renegotiation-info"
				) ) {
					return false;
				}
				break;

			case XTLS_EXTENSION_PRE_SHARED_KEY:
				if ( (!bServer) && (Cursor.Offset != Extensions.Size) ) {
					return __xrtTlsHelloError(
						XTLS_ERROR_EXTENSION, "parse-client-hello",
						"TLS pre_shared_key must be the final extension",
						Cursor.Offset - Extension.EncodedSize
					);
				}
				break;

			default:
				break;
		}
	}
	return Result == XTLS_ITEM_DONE;
}



/* 检查客户端压缩列表是否至少包含 null compression。 */
static bool __xrtTlsCompressionHasNull(xbytesview Methods)
{
	for ( size_t i = 0; i < Methods.Size; i++ ) {
		if ( Methods.Data[i] == 0 ) {
			return true;
		}
	}
	return false;
}



/* 对 TLS 1.3 ClientHello 强制唯一的 null compression 兼容字段。 */
bool __xrtTlsClientCompressionValid(
	const xtlsclienthello* pHello
)
{
	xtlsextension Extension;
	xtlsitemresult Result;

	Result = xrtTlsExtensionsFind(
		pHello->Extensions, XTLS_EXTENSION_SUPPORTED_VERSIONS, &Extension
	);
	if ( Result == XTLS_ITEM_ERROR ) {
		return false;
	}
	if ( Result == XTLS_ITEM_VALUE ) {
		xtlsids Versions;

		if ( !xrtTlsClientVersions(Extension.Data, &Versions) ) {
			return false;
		}
		if ( xrtTlsIdsContain(&Versions, XTLS_VERSION_13) &&
			((pHello->CompressionMethods.Size != 1u) ||
			 (pHello->CompressionMethods.Data[0] != 0)) ) {
			return __xrtTlsHelloError(
				XTLS_ERROR_HANDSHAKE, "parse-client-hello",
				"TLS 1.3 ClientHello requires only null compression", 0
			);
		}
	}
	return true;
}



/* 严格解析一条 ClientHello 正文。 */
XRT_API bool xrtTlsClientHelloParse(
	xbytesview Body,
	xtlsclienthello* pHello
)
{
	xtlsclienthello Hello;
	size_t iOffset = 0;
	size_t iSize;

	if ( (pHello == NULL) || !__xrtTlsViewValid(Body) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-client-hello",
			"TLS ClientHello input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( Body.Size < 41u ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_HANDSHAKE, "parse-client-hello",
			"TLS ClientHello fixed fields are truncated", Body.Size
		);
	}

	memset(&Hello, 0, sizeof(Hello));
	Hello.LegacyVersion = __xrtTlsRead16(Body.Data);
	if ( Hello.LegacyVersion != XTLS_VERSION_12 ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_VERSION, "parse-client-hello",
			"XRT requires TLS ClientHello legacy_version 0x0303", 0
		);
	}
	iOffset = 2u;
	Hello.Random.Data = Body.Data + iOffset;
	Hello.Random.Size = XTLS_RANDOM_SIZE;
	iOffset += XTLS_RANDOM_SIZE;

	iSize = Body.Data[iOffset++];
	if ( (iSize > XTLS_SESSION_ID_MAX) || (iSize > Body.Size - iOffset) ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_HANDSHAKE, "parse-client-hello",
			"TLS ClientHello session identifier is invalid", iOffset - 1u
		);
	}
	Hello.SessionId.Data = Body.Data + iOffset;
	Hello.SessionId.Size = iSize;
	iOffset += iSize;

	if ( Body.Size - iOffset < 2u ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_HANDSHAKE, "parse-client-hello",
			"TLS ClientHello cipher list is truncated", iOffset
		);
	}
	iSize = __xrtTlsRead16(Body.Data + iOffset);
	iOffset += 2u;
	if ( (iSize < 2u) || ((iSize & 1u) != 0) ||
		(iSize > Body.Size - iOffset) ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_HANDSHAKE, "parse-client-hello",
			"TLS ClientHello cipher list length is invalid", iOffset - 2u
		);
	}
	Hello.CipherSuites.Data.Data = Body.Data + iOffset;
	Hello.CipherSuites.Data.Size = iSize;
	iOffset += iSize;

	if ( Body.Size - iOffset < 1u ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_HANDSHAKE, "parse-client-hello",
			"TLS ClientHello compression list is truncated", iOffset
		);
	}
	iSize = Body.Data[iOffset++];
	if ( (iSize == 0) || (iSize > Body.Size - iOffset) ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_HANDSHAKE, "parse-client-hello",
			"TLS ClientHello compression list length is invalid", iOffset - 1u
		);
	}
	Hello.CompressionMethods.Data = Body.Data + iOffset;
	Hello.CompressionMethods.Size = iSize;
	if ( !__xrtTlsCompressionHasNull(Hello.CompressionMethods) ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_HANDSHAKE, "parse-client-hello",
			"TLS ClientHello does not offer null compression", iOffset
		);
	}
	iOffset += iSize;

	if ( iOffset != Body.Size ) {
		if ( Body.Size - iOffset < 2u ) {
			return __xrtTlsHelloError(
				XTLS_ERROR_HANDSHAKE, "parse-client-hello",
				"TLS ClientHello extension length is truncated", iOffset
			);
		}
		iSize = __xrtTlsRead16(Body.Data + iOffset);
		iOffset += 2u;
		if ( iSize != Body.Size - iOffset ) {
			return __xrtTlsHelloError(
				XTLS_ERROR_HANDSHAKE, "parse-client-hello",
				"TLS ClientHello extension length is inconsistent",
				iOffset - 2u
			);
		}
		Hello.Extensions.Data = Body.Data + iOffset;
		Hello.Extensions.Size = iSize;
	}
	if ( !__xrtTlsHelloExtensions(Hello.Extensions, false, false) ||
		!__xrtTlsClientCompressionValid(&Hello) ) {
		return false;
	}
	*pHello = Hello;
	return true;
}



/* 严格解析一条 ServerHello 或 HelloRetryRequest 正文。 */
XRT_API bool xrtTlsServerHelloParse(
	xbytesview Body,
	xtlsserverhello* pHello
)
{
	xtlsserverhello Hello;
	size_t iOffset;
	size_t iSize;

	if ( (pHello == NULL) || !__xrtTlsViewValid(Body) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-server-hello",
			"TLS ServerHello input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( Body.Size < 38u ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_HANDSHAKE, "parse-server-hello",
			"TLS ServerHello fixed fields are truncated", Body.Size
		);
	}

	memset(&Hello, 0, sizeof(Hello));
	Hello.LegacyVersion = __xrtTlsRead16(Body.Data);
	if ( Hello.LegacyVersion != XTLS_VERSION_12 ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_VERSION, "parse-server-hello",
			"XRT requires TLS ServerHello legacy_version 0x0303", 0
		);
	}
	iOffset = 2u;
	Hello.Random.Data = Body.Data + iOffset;
	Hello.Random.Size = XTLS_RANDOM_SIZE;
	Hello.Retry = __xrtTlsHelloRetry(Hello.Random);
	iOffset += XTLS_RANDOM_SIZE;

	iSize = Body.Data[iOffset++];
	if ( (iSize > XTLS_SESSION_ID_MAX) || (iSize > Body.Size - iOffset) ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_HANDSHAKE, "parse-server-hello",
			"TLS ServerHello session identifier is invalid", iOffset - 1u
		);
	}
	Hello.SessionId.Data = Body.Data + iOffset;
	Hello.SessionId.Size = iSize;
	iOffset += iSize;
	if ( Body.Size - iOffset < 3u ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_HANDSHAKE, "parse-server-hello",
			"TLS ServerHello cipher or compression is truncated", iOffset
		);
	}
	Hello.CipherSuite = __xrtTlsRead16(Body.Data + iOffset);
	iOffset += 2u;
	Hello.CompressionMethod = Body.Data[iOffset++];
	if ( Hello.CompressionMethod != 0 ) {
		return __xrtTlsHelloError(
			XTLS_ERROR_HANDSHAKE, "parse-server-hello",
			"TLS ServerHello selected a non-null compression method",
			iOffset - 1u
		);
	}

	if ( iOffset != Body.Size ) {
		if ( Body.Size - iOffset < 2u ) {
			return __xrtTlsHelloError(
				XTLS_ERROR_HANDSHAKE, "parse-server-hello",
				"TLS ServerHello extension length is truncated", iOffset
			);
		}
		iSize = __xrtTlsRead16(Body.Data + iOffset);
		iOffset += 2u;
		if ( iSize != Body.Size - iOffset ) {
			return __xrtTlsHelloError(
				XTLS_ERROR_HANDSHAKE, "parse-server-hello",
				"TLS ServerHello extension length is inconsistent",
				iOffset - 2u
			);
		}
		Hello.Extensions.Data = Body.Data + iOffset;
		Hello.Extensions.Size = iSize;
	}
	if ( !__xrtTlsHelloExtensions(
		Hello.Extensions, true, Hello.Retry
	) ) {
		return false;
	}
	*pHello = Hello;
	return true;
}


#endif
