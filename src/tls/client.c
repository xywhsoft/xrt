#include "../internal/xrt_tls_client.h"



#if defined(XRT_FEATURE_TLS_CLIENT)

#define XTLS_CLIENT_VERSION_CAPACITY 2u
#define XTLS_CLIENT_CIPHER_CAPACITY 16u
#define XTLS_CLIENT_GROUP_CAPACITY 8u
#define XTLS_CLIENT_SIGNATURE_CAPACITY 32u



/* 创建期候选数组只保存当前构建真正能够执行的协议能力。 */
typedef struct xtlsclientoffer {
	uint16 Versions[XTLS_CLIENT_VERSION_CAPACITY];
	uint16 Ciphers[XTLS_CLIENT_CIPHER_CAPACITY];
	uint16 Groups[XTLS_CLIENT_GROUP_CAPACITY];
	uint16 Signatures[XTLS_CLIENT_SIGNATURE_CAPACITY];
	size_t VersionCount;
	size_t CipherCount;
	size_t GroupCount;
	size_t SignatureCount;
	size_t SecretCapacity;
	size_t PrivateCapacity;
	size_t PublicCapacity;
	const xtlsgroupinfo* KeyShare;
	bool Tls12;
	bool Tls13;
} xtlsclientoffer;



#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)

/* 比较两个可为空的字节视图，不对零长度空指针执行库调用。 */
static bool __xrtTlsClientViewEqual(xbytesview Left, xbytesview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 返回证书验证和恢复票据共同使用的有效对端身份。 */
static xstrview __xrtTlsClientVerifyName(
	const xtlsclientconfig* pConfig
)
{
	return pConfig->VerifyName.Size != 0 ?
		pConfig->VerifyName : pConfig->ServerName;
}



/* 校验恢复对象的路由绑定，并为省略项提供精确的名称和 ALPN 默认值。 */
static bool __xrtTlsClientResumeConfig(
	xtlsclientconfig* pConfig,
	xstrview* pResumeProtocol,
	xtlsresumeinfo* pInfo,
	uint32* pAge
)
{
	xbytesview ConfigName;
	xstrview VerifyName;
	xtime iNow;
	bool bProtocol = false;

	memset(pInfo, 0, sizeof(*pInfo));
	*pAge = 0;
	if ( pConfig->Resume == NULL ) {
		return true;
	}
	if ( ((pConfig->ServerName.Data == NULL) &&
		(pConfig->ServerName.Size != 0)) ||
		((pConfig->VerifyName.Data == NULL) &&
		 (pConfig->VerifyName.Size != 0)) ||
		((pConfig->Protocols == NULL) &&
		 (pConfig->ProtocolCount != 0)) ) {
		return __xrtTlsClientError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "create-tls-client",
			"TLS client name or ALPN array is invalid"
		);
	}
	iNow = xrtNow();
	if ( !xrtTlsResumeInfo(pConfig->Resume, pInfo) ) {
		return __xrtTlsClientCause(
			"create-tls-client",
			"TLS client resume object is invalid"
		);
	}
	if ( !xrtTlsResumeTicketAge(pConfig->Resume, iNow, pAge) ) {
		return __xrtTlsClientError(
			XERR_VALUE, XTLS_ERROR_RESUME, "create-tls-client",
			"TLS client resume object is expired or not valid yet"
		);
	}
	if ( pInfo->Version != XTLS_VERSION_13 ) {
		return __xrtTlsClientError(
			XERR_UNSUPPORTED, XTLS_ERROR_VERSION, "create-tls-client",
			"TLS client resume object is not a TLS 1.3 ticket"
		);
	}
	VerifyName = __xrtTlsClientVerifyName(pConfig);
	ConfigName = (xbytesview) {
		(const uint8*)VerifyName.Data,
		VerifyName.Size
	};
	if ( VerifyName.Size == 0 ) {
		pConfig->ServerName = pInfo->ServerName;
		pConfig->VerifyName = pInfo->ServerName;
	} else if ( !__xrtTlsClientViewEqual(
		ConfigName,
		(xbytesview) {
			(const uint8*)pInfo->ServerName.Data,
			pInfo->ServerName.Size
		}
	) ) {
		return __xrtTlsClientError(
			XERR_VALUE, XTLS_ERROR_NEGOTIATION, "create-tls-client",
			"TLS client resume server name does not match the request"
		);
	}
	if ( pConfig->ProtocolCount == 0 ) {
		if ( pInfo->Protocol.Size != 0 ) {
			pResumeProtocol->Data = (const char*)pInfo->Protocol.Data;
			pResumeProtocol->Size = pInfo->Protocol.Size;
			pConfig->Protocols = pResumeProtocol;
			pConfig->ProtocolCount = 1u;
		}
		return true;
	}
	if ( pInfo->Protocol.Size == 0 ) {
		return __xrtTlsClientError(
			XERR_VALUE, XTLS_ERROR_NEGOTIATION, "create-tls-client",
			"TLS client resume without ALPN cannot be used with an ALPN request"
		);
	}
	for ( size_t i = 0; i < pConfig->ProtocolCount; i++ ) {
		const xstrview* pProtocol = &pConfig->Protocols[i];

		if ( (pProtocol->Data == NULL) || (pProtocol->Size == 0) ) {
			return __xrtTlsClientError(
				XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "create-tls-client",
				"TLS client ALPN protocol list is invalid"
			);
		}
		if ( __xrtTlsClientViewEqual(
			(xbytesview) {
				(const uint8*)pProtocol->Data, pProtocol->Size
			}, pInfo->Protocol
		) ) {
			bProtocol = true;
		}
	}
	if ( !bProtocol ) {
		return __xrtTlsClientError(
			XERR_VALUE, XTLS_ERROR_NEGOTIATION, "create-tls-client",
			"TLS client resume ALPN is not present in the request"
		);
	}
	return true;
}



/* 恢复票据的原始套件必须仍位于当前策略可执行的 offer 中。 */
static bool __xrtTlsClientResumeOffer(
	const xtlsclientoffer* pOffer,
	const xtlsresumeinfo* pInfo
)
{
	const xtlscipherinfo* pCipher = xrtTlsCipherInfo(pInfo->Cipher);

	if ( (pCipher == NULL) || (pCipher->Version != XTLS_VERSION_13) ||
		(pCipher->HashSize != pInfo->Secret.Size) ) {
		return __xrtTlsClientError(
			XERR_VALUE, XTLS_ERROR_CIPHER, "create-tls-client",
			"TLS client resume cipher or secret is invalid"
		);
	}
	for ( size_t i = 0; i < pOffer->CipherCount; i++ ) {
		if ( pOffer->Ciphers[i] == (uint16)pInfo->Cipher ) {
			return true;
		}
	}
	return __xrtTlsClientError(
		XERR_UNSUPPORTED, XTLS_ERROR_CIPHER, "create-tls-client",
		"TLS client resume cipher is disabled by the current policy"
	);
}

#endif



#if !defined(XRT_FEATURE_TLS_CLIENT_RESUME)

/* 返回证书验证使用的有效对端身份。 */
static xstrview __xrtTlsClientVerifyName(
	const xtlsclientconfig* pConfig
)
{
	return pConfig->VerifyName.Size != 0 ?
		pConfig->VerifyName : pConfig->ServerName;
}

#endif



/* 设置客户端角色错误并返回 false。 */
bool __xrtTlsClientError(
	xerrkind Kind,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtTlsError(Kind, Code, sOperation, sMessage, SIZE_MAX);
	return false;
}



/* 包装客户端创建阶段的底层失败并保留原因链。 */
bool __xrtTlsClientCause(cstr sOperation, cstr sMessage)
{
	const xerror* pCause = xrtGetError();
	xerrkind Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_INTERNAL;

	__xrtTlsErrorCause(
		Kind, XTLS_ERROR_HANDSHAKE,
		sOperation, sMessage, SIZE_MAX, pCause
	);
	return false;
}



/* 安全累加角色尾部存储尺寸。 */
static bool __xrtTlsClientAddSize(size_t* pSize, size_t iAdd)
{
	if ( iAdd > (SIZE_MAX - *pSize) ) {
		return __xrtTlsClientError(
			XERR_RANGE, XTLS_ERROR_LIMIT, "create-tls-client",
			"TLS client state size overflows"
		);
	}
	*pSize += iAdd;
	return true;
}



/* 只声明本构建可验证的线路签名；混合版本不能暴露 TLS 1.3 的缺失后端。 */
static bool __xrtTlsClientSignatureSupported(
	xtlssignature Signature,
	bool bTls12,
	bool bTls13
)
{
	(void)bTls12;
	(void)bTls13;
	switch ( Signature ) {
		#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY)
			#if defined(XRT_FEATURE_X509_VERIFY_RSA)
				case XTLS_SIGNATURE_RSA_PKCS1_SHA256:
				case XTLS_SIGNATURE_RSA_PKCS1_SHA384:
				case XTLS_SIGNATURE_RSA_PKCS1_SHA512:
					return bTls12;
				case XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256:
				case XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384:
				case XTLS_SIGNATURE_RSA_PSS_RSAE_SHA512:
				case XTLS_SIGNATURE_RSA_PSS_PSS_SHA256:
				case XTLS_SIGNATURE_RSA_PSS_PSS_SHA384:
				case XTLS_SIGNATURE_RSA_PSS_PSS_SHA512:
					return true;
			#endif
			#if defined(XRT_FEATURE_X509_VERIFY_ECDSA)
				case XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256:
				case XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384:
					return true;
				case XTLS_SIGNATURE_ECDSA_SECP521R1_SHA512:
					/* TLS 1.2 的 0x0603 是 SHA-512/ECDSA，不限定 P-521。 */
					return bTls12 && !bTls13;
			#endif
			#if defined(XRT_FEATURE_X509_VERIFY_ED25519)
				case XTLS_SIGNATURE_ED25519:
					return true;
			#endif
		#endif
		default:
			return false;
	}
}



/* 收集当前构建可完整执行的 TLS 1.3 和已验证 TLS 1.2 首航能力。 */
static bool __xrtTlsClientOffer(
	const xtlspolicy* pPolicy,
	bool bResumeOnly,
	xtlsclientoffer* pOffer
)
{
	memset(pOffer, 0, sizeof(*pOffer));
	for ( size_t i = 0; i < pPolicy->VersionCount; i++ ) {
		xtlsversion Version = pPolicy->Versions[i];

		if ( Version == XTLS_VERSION_13 ) {
			pOffer->Tls13 = true;
		} else if ( Version == XTLS_VERSION_12 ) {
			#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY) && \
				defined(XRT_FEATURE_TLS_AUTH_MESSAGES) && \
				defined(XRT_FEATURE_TLS_AUTH_MESSAGES_WRITE)
				pOffer->Tls12 = true;
			#else
				continue;
			#endif
		} else {
			continue;
		}
		pOffer->Versions[pOffer->VersionCount++] = (uint16)Version;
	}
	if ( pOffer->VersionCount == 0 ) {
		return __xrtTlsClientError(
			XERR_UNSUPPORTED, XTLS_ERROR_VERSION, "create-tls-client",
			"TLS client policy has no role version enabled by this build"
		);
	}
	for ( size_t i = 0; i < pPolicy->CipherCount; i++ ) {
		const xtlscipherinfo* pInfo = xrtTlsCipherInfo(
			pPolicy->Ciphers[i]
		);
		xcryptohash Hash;

		if ( (pInfo == NULL) ||
			((pInfo->Version != XTLS_VERSION_13) &&
			 (pInfo->Version != XTLS_VERSION_12)) ||
			((pInfo->Version == XTLS_VERSION_13) && !pOffer->Tls13) ||
			((pInfo->Version == XTLS_VERSION_12) && !pOffer->Tls12) ) {
			continue;
		}
		Hash = __xrtTlsHash(pInfo->Hash);
		if ( __xrtTlsRecordCipherSupported(
			pInfo->Version, pInfo->Cipher
		) && __xrtTlsScheduleHashSupported(Hash) ) {
			if ( pOffer->CipherCount == XTLS_CLIENT_CIPHER_CAPACITY ) {
				return __xrtTlsClientError(
					XERR_RANGE, XTLS_ERROR_LIMIT, "create-tls-client",
					"TLS client cipher offer exceeds internal capacity"
				);
			}
			pOffer->Ciphers[pOffer->CipherCount++] = (uint16)pInfo->Cipher;
			if ( pOffer->SecretCapacity < pInfo->HashSize ) {
				pOffer->SecretCapacity = pInfo->HashSize;
			}
		}
	}
	if ( pOffer->CipherCount == 0 ) {
		return __xrtTlsClientError(
			XERR_UNSUPPORTED, XTLS_ERROR_CIPHER, "create-tls-client",
			"TLS client has no enabled role record and schedule backend"
		);
	}

	for ( size_t i = 0; i < pPolicy->GroupCount; i++ ) {
		const xtlsgroupinfo* pInfo = xrtTlsGroupInfo(pPolicy->Groups[i]);

		if ( (pInfo == NULL) || !xrtTlsGroupAvailable(pInfo->Group) ) {
			continue;
		}
		if ( pOffer->GroupCount == XTLS_CLIENT_GROUP_CAPACITY ) {
			return __xrtTlsClientError(
				XERR_RANGE, XTLS_ERROR_LIMIT, "create-tls-client",
				"TLS client group offer exceeds internal capacity"
			);
		}
		pOffer->Groups[pOffer->GroupCount++] = pInfo->Group;
		if ( pOffer->KeyShare == NULL ) {
			pOffer->KeyShare = pInfo;
		}
		if ( pOffer->PrivateCapacity < pInfo->PrivateSize ) {
			pOffer->PrivateCapacity = pInfo->PrivateSize;
		}
		if ( pOffer->PublicCapacity < pInfo->PublicSize ) {
			pOffer->PublicCapacity = pInfo->PublicSize;
		}
	}
	if ( pOffer->KeyShare == NULL ) {
		return __xrtTlsClientError(
			XERR_UNSUPPORTED, XTLS_ERROR_KEY_EXCHANGE,
			"create-tls-client",
			"TLS client has no enabled key exchange backend"
		);
	}

	for ( size_t i = 0; i < pPolicy->SignatureCount; i++ ) {
		const xtlssignatureinfo* pInfo = xrtTlsSignatureInfo(
			pPolicy->Signatures[i]
		);

		if ( (pInfo == NULL) || !__xrtTlsClientSignatureSupported(
			pInfo->Signature, pOffer->Tls12, pOffer->Tls13
		) || (!pOffer->Tls13 && ((pInfo->Minimum > XTLS_VERSION_12) ||
			 (pInfo->Maximum < XTLS_VERSION_12))) ||
			(!pOffer->Tls12 && ((pInfo->Minimum > XTLS_VERSION_13) ||
			 (pInfo->Maximum < XTLS_VERSION_13))) ) {
			continue;
		}
		if ( pOffer->SignatureCount == XTLS_CLIENT_SIGNATURE_CAPACITY ) {
			return __xrtTlsClientError(
				XERR_RANGE, XTLS_ERROR_LIMIT, "create-tls-client",
				"TLS client signature offer exceeds internal capacity"
			);
		}
		pOffer->Signatures[pOffer->SignatureCount++] =
			(uint16)pInfo->Signature;
	}
	if ( (pOffer->SignatureCount == 0) && !bResumeOnly ) {
		return __xrtTlsClientError(
			XERR_UNSUPPORTED, XTLS_ERROR_NEGOTIATION,
			"create-tls-client",
			"TLS client policy has no signature scheme for an enabled version"
		);
	}
	return true;
}



/* 验证借用配置并计算 ALPN 内容长度。 */
static bool __xrtTlsClientConfigValid(
	const xtlsclientconfig* pConfig,
	size_t* pProtocolBytes
)
{
	size_t iBytes = 0;
	xstrview VerifyName = __xrtTlsClientVerifyName(pConfig);

	if ( ((pConfig->ServerName.Data == NULL) &&
		(pConfig->ServerName.Size != 0)) ||
		((pConfig->VerifyName.Data == NULL) &&
		 (pConfig->VerifyName.Size != 0)) ||
		((pConfig->Protocols == NULL) &&
		 (pConfig->ProtocolCount != 0)) ) {
		return __xrtTlsClientError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "create-tls-client",
			"TLS client name or ALPN array is invalid"
		);
	}
	#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY)
		if ( (pConfig->Verifier == NULL) && !pConfig->ResumeOnly ) {
			return __xrtTlsClientError(
				XERR_ARGUMENT, XTLS_ERROR_VERIFY, "create-tls-client",
				"TLS client requires a verifier outside resume-only mode"
			);
		}
	#else
		if ( !pConfig->ResumeOnly ) {
			return __xrtTlsClientError(
				XERR_UNSUPPORTED, XTLS_ERROR_VERIFY, "create-tls-client",
				"TLS client build has no certificate verifier"
			);
		}
	#endif
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		if ( pConfig->ResumeOnly && (pConfig->Resume == NULL) ) {
			return __xrtTlsClientError(
				XERR_ARGUMENT, XTLS_ERROR_RESUME, "create-tls-client",
				"TLS resume-only mode requires a resume object"
			);
		}
	#else
		if ( pConfig->ResumeOnly ) {
			return __xrtTlsClientError(
				XERR_UNSUPPORTED, XTLS_ERROR_RESUME, "create-tls-client",
				"TLS client build has no session resumption support"
			);
		}
	#endif
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		if ( pConfig->ResumeLimit > XTLS_CLIENT_RESUME_LIMIT_MAX ) {
			return __xrtTlsClientError(
				XERR_RANGE, XTLS_ERROR_LIMIT, "create-tls-client",
				"TLS client resume queue limit is too large"
			);
		}
	#endif
	if ( pConfig->ProtocolCount >
		((XTLS_EXTENSION_DATA_MAX - 2u) / 2u) ) {
		return __xrtTlsClientError(
			XERR_RANGE, XTLS_ERROR_EXTENSION, "create-tls-client",
			"TLS client ALPN protocol count is too large"
		);
	}
	if ( pConfig->ServerName.Size > (XTLS_EXTENSION_DATA_MAX - 5u) ) {
		return __xrtTlsClientError(
			XERR_RANGE, XTLS_ERROR_EXTENSION, "create-tls-client",
			"TLS client server name is too long"
		);
	}
	if ( VerifyName.Size > (XTLS_EXTENSION_DATA_MAX - 5u) ) {
		return __xrtTlsClientError(
			XERR_RANGE, XTLS_ERROR_EXTENSION, "create-tls-client",
			"TLS client verification name is too long"
		);
	}
	for ( size_t i = 0; i < pConfig->ServerName.Size; i++ ) {
		if ( pConfig->ServerName.Data[i] == '\0' ) {
			return __xrtTlsClientError(
				XERR_VALUE, XTLS_ERROR_EXTENSION, "create-tls-client",
				"TLS client server name contains a null byte"
			);
		}
	}
	for ( size_t i = 0; i < VerifyName.Size; i++ ) {
		if ( VerifyName.Data[i] == '\0' ) {
			return __xrtTlsClientError(
				XERR_VALUE, XTLS_ERROR_EXTENSION, "create-tls-client",
				"TLS client verification name contains a null byte"
			);
		}
	}
	for ( size_t i = 0; i < pConfig->ProtocolCount; i++ ) {
		const xstrview* pProtocol = &pConfig->Protocols[i];

		if ( (pProtocol->Data == NULL) || (pProtocol->Size == 0) ||
			(pProtocol->Size > UINT8_MAX) ||
			(iBytes > XTLS_EXTENSION_DATA_MAX - 3u - pProtocol->Size) ) {
			return __xrtTlsClientError(
				XERR_RANGE, XTLS_ERROR_EXTENSION, "create-tls-client",
				"TLS client ALPN protocol list is invalid"
			);
		}
		iBytes += pProtocol->Size;
	}
	*pProtocolBytes = iBytes;
	return true;
}



/* 计算 ClientHello 扩展向量的精确长度。 */
static bool __xrtTlsClientExtensionSize(
	const xtlsclientconfig* pConfig,
	const xtlsclientoffer* pOffer,
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		const xtlsresumeinfo* pResume,
	#endif
	size_t* pSize
)
{
	size_t iSize = 0;
	size_t iProtocols = 0;

	if ( pConfig->ServerName.Size != 0 ) {
		iSize += XTLS_EXTENSION_HEADER_SIZE + 5u +
			pConfig->ServerName.Size;
	}
	iSize += XTLS_EXTENSION_HEADER_SIZE + 1u +
		(pOffer->VersionCount * 2u);
	if ( pOffer->Tls12 ) {
		iSize += 2u * XTLS_EXTENSION_HEADER_SIZE + 1u;
	}
	iSize += XTLS_EXTENSION_HEADER_SIZE + 2u +
		(pOffer->GroupCount * 2u);
	if ( pOffer->SignatureCount != 0 ) {
		iSize += XTLS_EXTENSION_HEADER_SIZE + 2u +
			(pOffer->SignatureCount * 2u);
	}
	for ( size_t i = 0; i < pConfig->ProtocolCount; i++ ) {
		iProtocols += 1u + pConfig->Protocols[i].Size;
	}
	if ( pConfig->ProtocolCount != 0 ) {
		iSize += XTLS_EXTENSION_HEADER_SIZE + 2u + iProtocols;
	}
	iSize += XTLS_EXTENSION_HEADER_SIZE + 6u +
		pOffer->KeyShare->PublicSize;
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		if ( pConfig->Resume != NULL ) {
			iSize += XTLS_EXTENSION_HEADER_SIZE + 2u;
			iSize += XTLS_EXTENSION_HEADER_SIZE + 11u +
				pResume->Ticket.Size + pResume->Secret.Size;
		}
	#else
		(void)pConfig;
	#endif
	if ( iSize > UINT16_MAX ) {
		return __xrtTlsClientError(
			XERR_RANGE, XTLS_ERROR_EXTENSION, "create-tls-client",
			"TLS client extension vector is too long"
		);
	}
	*pSize = iSize;
	return true;
}



/* 计算完整 ClientHello 握手消息的精确长度。 */
static bool __xrtTlsClientHelloSize(
	const xtlsclientoffer* pOffer,
	size_t iExtensions,
	size_t* pSize
)
{
	size_t iBody = 2u + XTLS_RANDOM_SIZE + 1u +
		XTLS_SESSION_ID_MAX + 2u + (pOffer->CipherCount * 2u) +
		1u + 1u + 2u + iExtensions;
	size_t iSize = xrtTlsHandshakeSize(iBody);

	if ( iSize == 0 ) {
		return false;
	}
	*pSize = iSize;
	return true;
}



/* 把借用配置、密钥和工作区排进会话尾部连续存储。 */
static void __xrtTlsClientLayout(
	xtlsclientstate* pState,
	const xtlsclientconfig* pConfig,
	const xtlsclientoffer* pOffer,
	size_t iExtensions,
	size_t iHello
)
{
	bytes pStorage = (bytes)(pState + 1);
	xstrview VerifyName = __xrtTlsClientVerifyName(pConfig);
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		uintptr_t iAddress;
		size_t iPadding;
	#endif

	pState->Protocols = (xbytesview*)pStorage;
	pStorage += pConfig->ProtocolCount * sizeof(xbytesview);
	pState->Versions = (uint16*)pStorage;
	pState->VersionCount = pOffer->VersionCount;
	memcpy(
		pStorage, pOffer->Versions,
		pOffer->VersionCount * sizeof(uint16)
	);
	pStorage += pOffer->VersionCount * sizeof(uint16);
	pState->Ciphers = (uint16*)pStorage;
	pState->CipherCount = pOffer->CipherCount;
	memcpy(
		pStorage, pOffer->Ciphers,
		pOffer->CipherCount * sizeof(uint16)
	);
	pStorage += pOffer->CipherCount * sizeof(uint16);
	pState->Groups = (uint16*)pStorage;
	pState->GroupCount = pOffer->GroupCount;
	memcpy(
		pStorage, pOffer->Groups,
		pOffer->GroupCount * sizeof(uint16)
	);
	pStorage += pOffer->GroupCount * sizeof(uint16);
	pState->Signatures = (uint16*)pStorage;
	pState->SignatureCount = pOffer->SignatureCount;
	memcpy(
		pStorage, pOffer->Signatures,
		pOffer->SignatureCount * sizeof(uint16)
	);
	pStorage += pOffer->SignatureCount * sizeof(uint16);
	if ( VerifyName.Size != 0 ) {
		pState->ServerName.Data = pStorage;
		pState->ServerName.Size = VerifyName.Size;
		memcpy(pStorage, VerifyName.Data, VerifyName.Size);
		pStorage += VerifyName.Size;
	}
	if ( pConfig->VerifyName.Size == 0 ) {
		pState->SniName = pState->ServerName;
	} else if ( pConfig->ServerName.Size != 0 ) {
		pState->SniName.Data = pStorage;
		pState->SniName.Size = pConfig->ServerName.Size;
		memcpy(pStorage, pConfig->ServerName.Data, pConfig->ServerName.Size);
		pStorage += pConfig->ServerName.Size;
	}
	pState->ProtocolCount = pConfig->ProtocolCount;
	for ( size_t i = 0; i < pConfig->ProtocolCount; i++ ) {
		pState->Protocols[i].Data = pStorage;
		pState->Protocols[i].Size = pConfig->Protocols[i].Size;
		memcpy(pStorage, pConfig->Protocols[i].Data, pConfig->Protocols[i].Size);
		pStorage += pConfig->Protocols[i].Size;
	}
	pState->PrivateKey = pStorage;
	pState->PrivateKeySize = pOffer->KeyShare->PrivateSize;
	pState->PrivateKeyCapacity = pOffer->PrivateCapacity;
	pStorage += pState->PrivateKeyCapacity;
	pState->PublicKey = pStorage;
	pState->PublicKeySize = pOffer->KeyShare->PublicSize;
	pState->PublicKeyCapacity = pOffer->PublicCapacity;
	pStorage += pState->PublicKeyCapacity;
	pState->Workspace = pStorage;
	pState->WorkspaceSize = iExtensions;
	pStorage += iExtensions;
	pState->ClientHello = pStorage;
	pState->ClientHelloSize = iHello;
	pStorage += iHello;
	pState->HandshakeSecret = pStorage;
	pStorage += pOffer->SecretCapacity;
	pState->ClientHandshakeTraffic = pStorage;
	pStorage += pOffer->SecretCapacity;
	pState->ServerHandshakeTraffic = pStorage;
	pStorage += pOffer->SecretCapacity;
	pState->ClientApplicationTraffic = pStorage;
	pStorage += pOffer->SecretCapacity;
	pState->ServerApplicationTraffic = pStorage;
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		pStorage += pOffer->SecretCapacity;
		pState->ResumptionMaster = pStorage;
		pStorage += pOffer->SecretCapacity;
		iAddress = (uintptr_t)pStorage;
		iPadding = (sizeof(ptr) - (iAddress % sizeof(ptr))) % sizeof(ptr);
		pStorage += iPadding;
		pState->Resumes = (xtlsresume**)pStorage;
		pState->ResumeLimit = pConfig->ResumeLimit;
	#endif
	pState->SecretCapacity = pOffer->SecretCapacity;
	pState->Group = pOffer->KeyShare->Group;
	pState->Offer12 = pOffer->Tls12;
	pState->Offer13 = pOffer->Tls13;
	pState->ResumeOnly = pConfig->ResumeOnly;
}



/* 根据客户端稳定配置计算一次 ClientHello 的扩展尺寸。 */
static bool __xrtTlsClientStateExtensionSize(
	const xtlsclientstate* pState,
	xbytesview Cookie,
	size_t* pSize
)
{
	size_t iSize = 0;
	size_t iProtocols = 0;

	if ( (pState == NULL) || (pSize == NULL) ||
		((Cookie.Data == NULL) && (Cookie.Size != 0)) ||
		(Cookie.Size > XTLS_EXTENSION_DATA_MAX - 2u) ) {
		return __xrtTlsClientError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "build-client-hello",
			"TLS client retry cookie or size output is invalid"
		);
	}
	if ( pState->SniName.Size != 0 ) {
		iSize += XTLS_EXTENSION_HEADER_SIZE + 5u +
			pState->SniName.Size;
	}
	iSize += XTLS_EXTENSION_HEADER_SIZE + 1u +
		(pState->VersionCount * 2u);
	if ( pState->Offer12 ) {
		iSize += 2u * XTLS_EXTENSION_HEADER_SIZE + 1u;
	}
	iSize += XTLS_EXTENSION_HEADER_SIZE + 2u +
		(pState->GroupCount * 2u);
	if ( pState->SignatureCount != 0 ) {
		iSize += XTLS_EXTENSION_HEADER_SIZE + 2u +
			(pState->SignatureCount * 2u);
	}
	for ( size_t i = 0; i < pState->ProtocolCount; i++ ) {
		iProtocols += 1u + pState->Protocols[i].Size;
	}
	if ( pState->ProtocolCount != 0 ) {
		iSize += XTLS_EXTENSION_HEADER_SIZE + 2u + iProtocols;
	}
	iSize += XTLS_EXTENSION_HEADER_SIZE + 6u +
		pState->PublicKeySize;
	if ( Cookie.Size != 0 ) {
		iSize += XTLS_EXTENSION_HEADER_SIZE + 2u + Cookie.Size;
	}
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		if ( pState->OfferResume != NULL ) {
			xtlsresumeinfo Resume;

			if ( !xrtTlsResumeInfo(pState->OfferResume, &Resume) ) {
				return false;
			}
			iSize += XTLS_EXTENSION_HEADER_SIZE + 2u;
			iSize += XTLS_EXTENSION_HEADER_SIZE + 11u +
				Resume.Ticket.Size + Resume.Secret.Size;
		}
	#endif
	if ( iSize > UINT16_MAX ) {
		return __xrtTlsClientError(
			XERR_RANGE, XTLS_ERROR_EXTENSION, "build-client-hello",
			"TLS client extension vector is too long"
		);
	}
	*pSize = iSize;
	return true;
}



/* 为首航或 HRR 重试构建并排队当前 ClientHello。 */
bool __xrtTlsClientHelloQueue(
	xtlssession* pSession,
	xtlsclientstate* pState,
	xbytesview Cookie,
	bool bRetry
)
{
	xtlswriter Writer;
	xtlsclienthello Hello;
	xtlskeyshare Share;
	const xtlsgroupinfo* pGroup;
	const xtlslimits* pLimits;
	uint8 CipherBytes[XTLS_CLIENT_CIPHER_CAPACITY * 2u];
	uint8 Compression = 0;
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		xtlsresumeinfo Resume;
		xtlspsk Psk;
		uint8 Mode = XTLS_PSK_DHE_KE;
		uint8 Binder[XTLS_CLIENT_SECRET_MAX_SIZE];
	#endif
	bytes pStorage = NULL;
	bytes pWorkspace;
	bytes pHello;
	bytes pOldWorkspace;
	bytes pOldHello;
	size_t iOldWorkspaceSize;
	size_t iOldHelloSize;
	size_t iExtensions;
	size_t iBodySize;
	size_t iHelloSize;
	bool bResult = false;

	if ( (pSession == NULL) || (pState == NULL) ) {
		return __xrtTlsClientError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "build-client-hello",
			"TLS client session or state is null"
		);
	}
	pGroup = xrtTlsGroupInfo(pState->Group);
	pLimits = xrtTlsContextLimits(pSession->Context);
	if ( (pGroup == NULL) || (pLimits == NULL) ||
		(pGroup->PrivateSize > pState->PrivateKeyCapacity) ||
		(pGroup->PublicSize > pState->PublicKeyCapacity) ) {
		return __xrtTlsClientError(
			XERR_UNSUPPORTED, XTLS_ERROR_KEY_EXCHANGE,
			"build-client-hello",
			"TLS retry group exceeds the client key-share capacity"
		);
	}
	pState->PrivateKeySize = pGroup->PrivateSize;
	pState->PublicKeySize = pGroup->PublicSize;
	if ( !__xrtTlsClientStateExtensionSize(
		pState, Cookie, &iExtensions
	) ) {
		return false;
	}
	iBodySize = 2u + XTLS_RANDOM_SIZE + 1u +
		XTLS_SESSION_ID_MAX + 2u + (pState->CipherCount * 2u) +
		1u + 1u + 2u + iExtensions;
	iHelloSize = xrtTlsHandshakeSize(iBodySize);
	if ( (iHelloSize == 0) || (iHelloSize > pLimits->HandshakeLimit) ) {
		return __xrtTlsClientError(
			XERR_RANGE, XTLS_ERROR_LIMIT, "build-client-hello",
			"TLS ClientHello exceeds the configured handshake limit"
		);
	}
	pWorkspace = pState->Workspace;
	pHello = pState->ClientHello;
	if ( bRetry ) {
		size_t iStorage = iExtensions;

		if ( !__xrtTlsClientAddSize(&iStorage, iHelloSize) ) {
			return false;
		}
		pStorage = (bytes)xrtMalloc(iStorage);
		if ( pStorage == NULL ) {
			return __xrtTlsClientCause(
				"build-client-hello",
				"TLS retry ClientHello allocation failed"
			);
		}
		pWorkspace = pStorage;
		pHello = pStorage + iExtensions;
	} else if ( (iExtensions != pState->WorkspaceSize) ||
		(iHelloSize != pState->ClientHelloSize) ) {
		return __xrtTlsClientError(
			XERR_INTERNAL, XTLS_ERROR_INTERNAL, "build-client-hello",
			"TLS initial ClientHello layout is inconsistent"
		);
	}
	if ( (!bRetry &&
		(!xrtSecureRandom(pState->Random, sizeof(pState->Random)) ||
		 !xrtSecureRandom(pState->SessionId, sizeof(pState->SessionId)))) ||
		!xrtTlsKeyShareGenerate(
			pState->Group,
			pState->PrivateKey, pState->PrivateKeySize,
			pState->PublicKey, pState->PublicKeySize
	) ) {
		(void)__xrtTlsClientCause(
			"start-tls-client", "TLS client random or key share generation failed"
		);
		goto cleanup;
	}
	if ( !xrtTlsWriterInit(
		&Writer, pWorkspace, iExtensions
	) ) {
		goto cleanup;
	}
	if ( (pState->SniName.Size != 0) && !xrtTlsWriterHostName(
		&Writer, pState->SniName
	) ) {
		goto cleanup;
	}
	if ( !xrtTlsWriterClientVersions(
		&Writer, pState->Versions, pState->VersionCount
	) || (pState->Offer12 && !xrtTlsWriterExtension(
		&Writer, XTLS_EXTENSION_EXTENDED_MASTER_SECRET,
		(xbytesview) { NULL, 0 }
	)) || (pState->Offer12 && !xrtTlsWriterExtension(
		&Writer, XTLS_EXTENSION_RENEGOTIATION_INFO,
		(xbytesview) { &Compression, 1u }
	)) || !xrtTlsWriterIds(
		&Writer, XTLS_EXTENSION_SUPPORTED_GROUPS,
		pState->Groups, pState->GroupCount
	) || ((pState->SignatureCount != 0) && !xrtTlsWriterIds(
		&Writer, XTLS_EXTENSION_SIGNATURE_ALGORITHMS,
		pState->Signatures, pState->SignatureCount
	)) ) {
		goto cleanup;
	}
	if ( (pState->ProtocolCount != 0) && !xrtTlsWriterProtocols(
		&Writer, pState->Protocols, pState->ProtocolCount
	) ) {
		goto cleanup;
	}
	Share.Group = pState->Group;
	Share.Key.Data = pState->PublicKey;
	Share.Key.Size = pState->PublicKeySize;
	if ( !xrtTlsWriterClientKeyShares(&Writer, &Share, 1u) ) {
		goto cleanup;
	}
	if ( (Cookie.Size != 0) && !xrtTlsWriterRetryCookie(
		&Writer, Cookie
	) ) {
		goto cleanup;
	}
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		memset(Binder, 0, sizeof(Binder));
		if ( pState->OfferResume != NULL ) {
			if ( !xrtTlsResumeInfo(pState->OfferResume, &Resume) ||
				(Resume.Secret.Size > sizeof(Binder)) ) {
				goto cleanup;
			}
			Psk.Identity = Resume.Ticket;
			Psk.ObfuscatedAge = pState->ResumeAge;
			Psk.Binder = (xbytesview) {
				Binder, Resume.Secret.Size
			};
			if ( !xrtTlsWriterPskModes(&Writer, &Mode, 1u) ||
				!xrtTlsWriterClientPsks(&Writer, &Psk, 1u) ) {
				goto cleanup;
			}
		}
		xrtSecureZero(Binder, sizeof(Binder));
	#endif
	if ( Writer.Size != iExtensions ) {
		goto cleanup;
	}
	for ( size_t i = 0; i < pState->CipherCount; i++ ) {
		__xrtTlsWrite16(CipherBytes + (i * 2u), pState->Ciphers[i]);
	}
	memset(&Hello, 0, sizeof(Hello));
	Hello.LegacyVersion = XTLS_VERSION_12;
	Hello.Random = (xbytesview) {
		pState->Random, sizeof(pState->Random)
	};
	Hello.SessionId = (xbytesview) {
		pState->SessionId, sizeof(pState->SessionId)
	};
	Hello.CipherSuites.Data = (xbytesview) {
		CipherBytes, pState->CipherCount * 2u
	};
	Hello.CompressionMethods = (xbytesview) { &Compression, 1u };
	Hello.Extensions = xrtTlsWriterData(&Writer);
	if ( (xrtTlsClientHelloSize(&Hello) != iBodySize) ||
		!xrtTlsClientHelloEncode(
			&Hello, pHello + XTLS_HANDSHAKE_HEADER_SIZE,
			iBodySize
		) || !xrtTlsHandshakeEncode(
			XTLS_HANDSHAKE_CLIENT_HELLO,
			(xbytesview) {
				pHello + XTLS_HANDSHAKE_HEADER_SIZE,
				iBodySize
			}, pHello, iHelloSize
		) ) {
		goto cleanup;
	}
	pOldWorkspace = pState->Workspace;
	iOldWorkspaceSize = pState->WorkspaceSize;
	pOldHello = pState->ClientHello;
	iOldHelloSize = pState->ClientHelloSize;
	pState->Workspace = pWorkspace;
	pState->WorkspaceSize = iExtensions;
	pState->ClientHello = pHello;
	pState->ClientHelloSize = iHelloSize;
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		if ( (pState->OfferResume != NULL) &&
			!__xrtTlsClientResumeBinder(
				pState, bRetry ? &pState->Transcript : NULL
			) ) {
			goto restore;
		}
	#endif
	if ( __xrtTlsSessionRecordPlain(
		pSession, XTLS_RECORD_HANDSHAKE, XTLS_VERSION_12,
		(xbytesview) { pState->ClientHello, pState->ClientHelloSize }
	) != XTLS_OK ) {
		goto restore;
	}
	if ( !__xrtTlsSessionSetState(
		pSession, XTLS_STATE_HANDSHAKE
	) || !__xrtTlsSessionSetWait(
		pSession, XTLS_WAIT_INPUT | XTLS_WAIT_OUTPUT
	) ) {
		goto restore;
	}
	if ( bRetry ) {
		pState->RetryStorage = pStorage;
		pStorage = NULL;
	}
	pState->Step = XTLS_CLIENT_WAIT_SERVER_HELLO;
	bResult = true;
	goto cleanup;

restore:
	pState->Workspace = pOldWorkspace;
	pState->WorkspaceSize = iOldWorkspaceSize;
	pState->ClientHello = pOldHello;
	pState->ClientHelloSize = iOldHelloSize;

cleanup:
	if ( pStorage != NULL ) {
		xrtSecureZero(pStorage, iExtensions + iHelloSize);
		xrtFree(pStorage);
	}
	return bResult;
}



/* 释放客户端跨记录状态、对端证书和共享验证器引用。 */
static void __xrtTlsClientClean(xtlssession* pSession, ptr pRole)
{
	xtlsclientstate* pState = (xtlsclientstate*)pRole;

	(void)pSession;
	if ( pState != NULL ) {
		xrtTlsHandshakeReaderUnit(&pState->Reader);
		__xrtTlsTranscriptClear(&pState->Transcript);
		__xrtTlsRecordKeyClear(&pState->PendingReadKey);
		#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY)
			xrtFree(pState->Peer);
			xrtTlsVerifierRelease(pState->Verifier);
		#endif
		if ( pState->RetryStorage != NULL ) {
			xrtSecureZero(
				pState->RetryStorage,
				pState->WorkspaceSize + pState->ClientHelloSize
			);
			xrtFree(pState->RetryStorage);
			pState->RetryStorage = NULL;
		}
		#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
			__xrtTlsClientResumeClear(pState);
			xrtTlsResumeRelease((xtlsresume*)pState->OfferResume);
			pState->OfferResume = NULL;
		#endif
	}
}



/* 初始化不借用任何外部资源的客户端配置。 */
XRT_API void xrtTlsClientConfigInit(xtlsclientconfig* pConfig)
{
	if ( pConfig == NULL ) {
		(void)__xrtTlsClientError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"init-tls-client-config", "TLS client config is null"
		);
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->ResumeLimit = XTLS_CLIENT_RESUME_LIMIT_DEFAULT;
}



#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY)

/* 验证公共证书查询确实指向客户端会话。 */
static xtlsclientstate* __xrtTlsClientQueryState(
	const xtlssession* pSession,
	cstr sOperation
)
{
	if ( pSession == NULL ) {
		(void)__xrtTlsClientError(
			XERR_ARGUMENT,
			XTLS_ERROR_ARGUMENT,
			sOperation,
			"TLS client session is null"
		);
		return NULL;
	}
	if ( (pSession->Role != XTLS_CLIENT) ||
		(pSession->AllocationSize <= sizeof(*pSession)) ) {
		(void)__xrtTlsClientError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			sOperation,
			"TLS session is not a verified client"
		);
		return NULL;
	}
	return (xtlsclientstate*)(pSession + 1);
}



/* 返回完整证书握手保留的对端证书数量。 */
XRT_API size_t xrtTlsClientCertificateCount(
	const xtlssession* pSession
)
{
	xtlsclientstate* pState = __xrtTlsClientQueryState(
		pSession,
		"count-tls-client-certificates"
	);

	return ((pState != NULL) && (pState->Peer != NULL)) ?
		pState->Peer->CertificateCount : 0;
}



/* 借用完整证书握手保留的一张解析视图。 */
XRT_API const xx509cert* xrtTlsClientCertificate(
	const xtlssession* pSession,
	size_t iIndex
)
{
	xtlsclientstate* pState = __xrtTlsClientQueryState(
		pSession,
		"get-tls-client-certificate"
	);

	if ( pState == NULL ) {
		return NULL;
	}
	if ( pState->Peer == NULL ) {
		(void)__xrtTlsClientError(
			XERR_STATE,
			XTLS_ERROR_STATE,
			"get-tls-client-certificate",
			"TLS client has no certificate from a full verified handshake"
		);
		return NULL;
	}
	if ( iIndex >= pState->Peer->CertificateCount ) {
		(void)__xrtTlsClientError(
			XERR_RANGE,
			XTLS_ERROR_ARGUMENT,
			"get-tls-client-certificate",
			"TLS client certificate index is out of range"
		);
		return NULL;
	}
	return &pState->Peer->Certificates[iIndex];
}

#endif



/* 创建单分配客户端状态并排队首条 ClientHello。 */
XRT_API xtlssession* xrtTlsClientCreate(
	const xtlsclientconfig* pConfig,
	xnetbufpool* pPool
)
{
	xtlsclientconfig Config;
	xtlsclientoffer Offer;
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		xstrview ResumeProtocol;
		xtlsresumeinfo ResumeInfo;
		uint32 iResumeAge;
	#endif
	const xtlscontext* pContext;
	xtlscontext* pDefault = NULL;
	const xtlspolicy* pPolicy;
	const xtlslimits* pLimits;
	xtlslimits Limits;
	xstrview VerifyName;
	size_t iProtocolBytes = 0;
	size_t iExtensions = 0;
	size_t iHello;
	size_t iRoleSize = sizeof(xtlsclientstate);
	xtlssession* pSession;
	xtlsclientstate* pState;
	xtlshandshakereaderconfig ReaderConfig;

	if ( pConfig == NULL ) {
		xrtTlsClientConfigInit(&Config);
	} else {
		Config = *pConfig;
	}
	pConfig = &Config;
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		memset(&ResumeProtocol, 0, sizeof(ResumeProtocol));
		if ( !__xrtTlsClientResumeConfig(
			&Config, &ResumeProtocol, &ResumeInfo, &iResumeAge
		) ) {
			return NULL;
		}
	#endif
	if ( !__xrtTlsClientConfigValid(pConfig, &iProtocolBytes) ) {
		return NULL;
	}
	VerifyName = __xrtTlsClientVerifyName(pConfig);
	pContext = pConfig->Context;
	if ( pContext == NULL ) {
		pDefault = xrtTlsContextCreate(NULL);
		if ( pDefault == NULL ) {
			return NULL;
		}
		pContext = pDefault;
	}
	pPolicy = xrtTlsContextPolicy(pContext);
	pLimits = xrtTlsContextLimits(pContext);
	if ( (pPolicy == NULL) || (pLimits == NULL) ||
		!__xrtTlsClientOffer(pPolicy, pConfig->ResumeOnly, &Offer)
		#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
			|| ((pConfig->Resume != NULL) &&
			 !__xrtTlsClientResumeOffer(&Offer, &ResumeInfo))
		#endif
		||
		!__xrtTlsClientExtensionSize(
			pConfig, &Offer,
			#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
				&ResumeInfo,
			#endif
			&iExtensions
		) || !__xrtTlsClientHelloSize(&Offer, iExtensions, &iHello) ||
		!__xrtTlsClientAddSize(
			&iRoleSize, pConfig->ProtocolCount * sizeof(xbytesview)
		) || !__xrtTlsClientAddSize(
			&iRoleSize, Offer.VersionCount * sizeof(uint16)
		) || !__xrtTlsClientAddSize(
			&iRoleSize, Offer.CipherCount * sizeof(uint16)
		) || !__xrtTlsClientAddSize(
			&iRoleSize, Offer.GroupCount * sizeof(uint16)
		) || !__xrtTlsClientAddSize(
			&iRoleSize, Offer.SignatureCount * sizeof(uint16)
		) || !__xrtTlsClientAddSize(
			&iRoleSize, VerifyName.Size
		) || !__xrtTlsClientAddSize(
			&iRoleSize,
			pConfig->VerifyName.Size != 0 ?
				pConfig->ServerName.Size : 0
		) || !__xrtTlsClientAddSize(
			&iRoleSize, iProtocolBytes
		) || !__xrtTlsClientAddSize(
			&iRoleSize, Offer.PrivateCapacity
		) || !__xrtTlsClientAddSize(
			&iRoleSize, Offer.PublicCapacity
		) || !__xrtTlsClientAddSize(
			&iRoleSize, iExtensions
		) || !__xrtTlsClientAddSize(
			&iRoleSize, iHello
		) || !__xrtTlsClientAddSize(
			&iRoleSize, Offer.SecretCapacity
		) || !__xrtTlsClientAddSize(
			&iRoleSize, Offer.SecretCapacity
		) || !__xrtTlsClientAddSize(
			&iRoleSize, Offer.SecretCapacity
		) || !__xrtTlsClientAddSize(
			&iRoleSize, Offer.SecretCapacity
		) || !__xrtTlsClientAddSize(
			&iRoleSize, Offer.SecretCapacity
		)
		#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
			|| !__xrtTlsClientAddSize(
				&iRoleSize, Offer.SecretCapacity
			) || !__xrtTlsClientAddSize(
				&iRoleSize, sizeof(ptr) - 1u
			) || !__xrtTlsClientAddSize(
				&iRoleSize, pConfig->ResumeLimit * sizeof(xtlsresume*)
			)
		#endif
	) {
		xrtTlsContextRelease(pDefault);
		return NULL;
	}
	Limits = *pLimits;
	if ( iHello > Limits.HandshakeLimit ) {
		(void)__xrtTlsClientError(
			XERR_RANGE, XTLS_ERROR_LIMIT, "create-tls-client",
			"TLS ClientHello exceeds the configured handshake limit"
		);
		xrtTlsContextRelease(pDefault);
		return NULL;
	}
	pSession = __xrtTlsSessionCreateSized(
		pContext, pPool, XTLS_CLIENT, iRoleSize, __xrtTlsClientClean
	);
	xrtTlsContextRelease(pDefault);
	if ( pSession == NULL ) {
		return NULL;
	}
	pState = (xtlsclientstate*)__xrtTlsSessionRoleData(pSession);
	__xrtTlsClientLayout(
		pState, pConfig, &Offer, iExtensions, iHello
	);
	pSession->KeyUpdate = xrtTlsClientKeyUpdate;
	#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
		if ( pConfig->Resume != NULL ) {
			pState->OfferResume = xrtTlsResumeRetain(pConfig->Resume);
			if ( pState->OfferResume == NULL ) {
				xrtTlsSessionDestroy(pSession);
				return NULL;
			}
			pState->ResumeAge = iResumeAge;
		}
	#endif
	#if defined(XRT_FEATURE_TLS_CLIENT_VERIFY)
		if ( pConfig->Verifier != NULL ) {
			pState->Verifier = xrtTlsVerifierRetain(pConfig->Verifier);
			if ( pState->Verifier == NULL ) {
				xrtTlsSessionDestroy(pSession);
				return NULL;
			}
		}
	#endif
	xrtTlsHandshakeReaderConfigInit(&ReaderConfig);
	ReaderConfig.Limit = Limits.HandshakeLimit;
	if ( ReaderConfig.Retain > ReaderConfig.Limit ) {
		ReaderConfig.Retain = ReaderConfig.Limit;
	}
	if ( !xrtTlsHandshakeReaderInit(
		&pState->Reader, &ReaderConfig
	) || !__xrtTlsClientHelloQueue(
		pSession, pState, (xbytesview) { NULL, 0 }, false
	) ) {
		xrtTlsSessionDestroy(pSession);
		return NULL;
	}
	return pSession;
}

#endif
