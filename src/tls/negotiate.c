#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_NEGOTIATE)

/* 设置本地协商配置错误并返回游标错误结果。 */
static xtlsitemresult __xrtTlsNegotiateError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtTlsError(
		Kind, XTLS_ERROR_NEGOTIATION, sOperation,
		sMessage, SIZE_MAX
	);
	return XTLS_ITEM_ERROR;
}



/* 所有公开签名元数据只保留一份，协商与后续认证状态机共同复用。 */
static const xtlssignatureinfo __xrtTlsSignatureInfos[] = {
	{
		XTLS_SIGNATURE_RSA_PKCS1_SHA256,
		XTLS_IDENTITY_RSA, 32u, XTLS_VERSION_12, XTLS_VERSION_12
	},
	{
		XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256,
		XTLS_IDENTITY_ECDSA_P256, 32u, XTLS_VERSION_12, XTLS_VERSION_13
	},
	{
		XTLS_SIGNATURE_RSA_PKCS1_SHA384,
		XTLS_IDENTITY_RSA, 48u, XTLS_VERSION_12, XTLS_VERSION_12
	},
	{
		XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384,
		XTLS_IDENTITY_ECDSA_P384, 48u, XTLS_VERSION_12, XTLS_VERSION_13
	},
	{
		XTLS_SIGNATURE_RSA_PKCS1_SHA512,
		XTLS_IDENTITY_RSA, 64u, XTLS_VERSION_12, XTLS_VERSION_12
	},
	{
		XTLS_SIGNATURE_ECDSA_SECP521R1_SHA512,
		XTLS_IDENTITY_ECDSA_P521, 64u, XTLS_VERSION_12, XTLS_VERSION_13
	},
	{
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
		XTLS_IDENTITY_RSA, 32u, XTLS_VERSION_12, XTLS_VERSION_13
	},
	{
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384,
		XTLS_IDENTITY_RSA, 48u, XTLS_VERSION_12, XTLS_VERSION_13
	},
	{
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA512,
		XTLS_IDENTITY_RSA, 64u, XTLS_VERSION_12, XTLS_VERSION_13
	},
	{
		XTLS_SIGNATURE_ED25519,
		XTLS_IDENTITY_ED25519, 0u, XTLS_VERSION_12, XTLS_VERSION_13
	},
	{
		XTLS_SIGNATURE_ED448,
		XTLS_IDENTITY_ED448, 0u, XTLS_VERSION_12, XTLS_VERSION_13
	},
	{
		XTLS_SIGNATURE_RSA_PSS_PSS_SHA256,
		XTLS_IDENTITY_RSA_PSS, 32u, XTLS_VERSION_12, XTLS_VERSION_13
	},
	{
		XTLS_SIGNATURE_RSA_PSS_PSS_SHA384,
		XTLS_IDENTITY_RSA_PSS, 48u, XTLS_VERSION_12, XTLS_VERSION_13
	},
	{
		XTLS_SIGNATURE_RSA_PSS_PSS_SHA512,
		XTLS_IDENTITY_RSA_PSS, 64u, XTLS_VERSION_12, XTLS_VERSION_13
	}
};



/* 统一验证调用方偏好列表的指针、索引范围和唯一性。 */
static bool __xrtTlsPreferenceListValid(
	const void* pPreferred,
	size_t iPreferredCount,
	size_t iItemSize,
	cstr sOperation
)
{
	const uint8* pData = (const uint8*)pPreferred;

	if ( (iItemSize == 0) ||
		((pPreferred == NULL) && (iPreferredCount != 0)) ||
		(iPreferredCount > (SIZE_MAX / iItemSize)) ) {
		__xrtTlsNegotiateError(
			XERR_ARGUMENT, sOperation,
			"TLS preference list is invalid"
		);
		return false;
	}
	for ( size_t i = 0; i < iPreferredCount; i++ ) {
		for ( size_t j = 0; j < i; j++ ) {
			if ( memcmp(
				pData + (i * iItemSize),
				pData + (j * iItemSize),
				iItemSize
			) == 0 ) {
				__xrtTlsNegotiateError(
					XERR_VALUE, sOperation,
					"TLS preference list contains a duplicate"
				);
				return false;
			}
		}
	}
	return true;
}



/* 验证身份枚举属于当前协商层。 */
static bool __xrtTlsIdentityValid(xtlsidentitytype Identity)
{
	return (Identity >= XTLS_IDENTITY_NONE) &&
		(Identity <= XTLS_IDENTITY_ED448);
}



/* 验证调用方提供的版本偏好。 */
static bool __xrtTlsVersionsValid(
	const xtlsversion* pPreferred,
	size_t iPreferredCount,
	cstr sOperation
)
{
	if ( !__xrtTlsPreferenceListValid(
		pPreferred, iPreferredCount, sizeof(*pPreferred), sOperation
	) ) {
		return false;
	}
	for ( size_t i = 0; i < iPreferredCount; i++ ) {
		if ( !__xrtTlsVersionSupported(pPreferred[i]) ) {
			__xrtTlsNegotiateError(
				XERR_VALUE, sOperation,
				"TLS version preference is unsupported"
			);
			return false;
		}
	}
	return true;
}



/* 验证调用方提供的密码套件偏好。 */
static bool __xrtTlsCiphersValid(
	const xtlscipher* pPreferred,
	size_t iPreferredCount,
	cstr sOperation
)
{
	if ( !__xrtTlsPreferenceListValid(
		pPreferred, iPreferredCount, sizeof(*pPreferred), sOperation
	) ) {
		return false;
	}
	for ( size_t i = 0; i < iPreferredCount; i++ ) {
		if ( xrtTlsCipherInfo(pPreferred[i]) == NULL ) {
			__xrtTlsNegotiateError(
				XERR_VALUE, sOperation,
				"TLS cipher preference is unsupported"
			);
			return false;
		}
	}
	return true;
}



/* 验证调用方提供的签名偏好。 */
static bool __xrtTlsSignaturesValid(
	const xtlssignature* pPreferred,
	size_t iPreferredCount,
	cstr sOperation
)
{
	if ( !__xrtTlsPreferenceListValid(
		pPreferred, iPreferredCount, sizeof(*pPreferred), sOperation
	) ) {
		return false;
	}
	for ( size_t i = 0; i < iPreferredCount; i++ ) {
		if ( xrtTlsSignatureInfo(pPreferred[i]) == NULL ) {
			__xrtTlsNegotiateError(
				XERR_VALUE, sOperation,
				"TLS signature preference is unsupported"
			);
			return false;
		}
	}
	return true;
}



/* 在已经完整验证的 key_share 游标中查找指定组。 */
static bool __xrtTlsKeyShareFindValid(
	const xtlskeysharecursor* pStart,
	uint16 iGroup,
	xtlskeyshare* pShare
)
{
	xtlskeysharecursor Cursor = *pStart;
	xtlskeyshare Share;
	xtlsitemresult Result;

	while ( (Result = xrtTlsKeySharesRead(
		&Cursor, &Share
	)) == XTLS_ITEM_VALUE ) {
		if ( Share.Group == iGroup ) {
			*pShare = Share;
			return true;
		}
	}
	return false;
}



/* 验证 key_share 是 supported_groups 的同序子序列。 */
static bool __xrtTlsKeySharesMatchGroups(
	const xtlsids* pGroups,
	const xtlskeysharecursor* pStart
)
{
	xtlskeysharecursor Cursor = *pStart;
	xtlskeyshare Share;
	xtlsitemresult Result;
	size_t iGroup = 0;
	size_t iGroupCount = xrtTlsIdsCount(pGroups);

	while ( (Result = xrtTlsKeySharesRead(
		&Cursor, &Share
	)) == XTLS_ITEM_VALUE ) {
		bool bFound = false;

		while ( iGroup < iGroupCount ) {
			uint16 iValue;

			(void)xrtTlsIdsGet(pGroups, iGroup, &iValue);
			iGroup++;
			if ( iValue == Share.Group ) {
				bFound = true;
				break;
			}
		}
		if ( !bFound ) {
			__xrtTlsError(
				XERR_PROTOCOL, XTLS_ERROR_NEGOTIATION,
				"select-key-share",
				"TLS key shares are not an ordered subset of supported groups",
				SIZE_MAX
			);
			return false;
		}
	}
	return Result == XTLS_ITEM_DONE;
}



/* 发布一个可直接使用或需要重试的密钥共享选择。 */
static void __xrtTlsKeySharePublish(
	uint16 iGroup,
	const xtlskeyshare* pShare,
	xtlskeyshareselection* pSelection
)
{
	xtlskeyshareselection Selection;

	memset(&Selection, 0, sizeof(Selection));
	Selection.Share.Group = iGroup;
	if ( pShare != NULL ) {
		Selection.Share = *pShare;
	} else {
		Selection.Retry = true;
	}
	*pSelection = Selection;
}



/* 按本地偏好顺序选择 16 位标识交集。 */
XRT_API xtlsitemresult xrtTlsIdsSelect(
	const xtlsids* pOffered,
	const uint16* pPreferred,
	size_t iPreferredCount,
	uint16* pSelected
)
{
	if ( (pOffered == NULL) || (pSelected == NULL) ) {
		return __xrtTlsNegotiateError(
			XERR_ARGUMENT, "select-tls-id",
			"TLS offered identifiers or output is invalid"
		);
	}
	if ( !__xrtTlsPreferenceListValid(
		pPreferred, iPreferredCount, sizeof(*pPreferred),
		"select-tls-id"
	) ) {
		return XTLS_ITEM_ERROR;
	}
	if ( !__xrtTlsIdsDataValid(
		pOffered->Data, "select-tls-id", XERR_PROTOCOL
	) ) {
		return XTLS_ITEM_ERROR;
	}
	for ( size_t i = 0; i < iPreferredCount; i++ ) {
		if ( xrtTlsIdsContain(pOffered, pPreferred[i]) ) {
			*pSelected = pPreferred[i];
			return XTLS_ITEM_VALUE;
		}
	}
	return XTLS_ITEM_DONE;
}



/* 从完整客户端 key_share 扩展负载查找指定组。 */
XRT_API xtlsitemresult xrtTlsKeyShareFind(
	xbytesview KeyShares,
	uint16 iGroup,
	xtlskeyshare* pShare
)
{
	xtlskeysharecursor Cursor;
	xtlskeyshare Share;

	if ( pShare == NULL ) {
		return __xrtTlsNegotiateError(
			XERR_ARGUMENT, "find-key-share",
			"TLS key-share output is invalid"
		);
	}
	if ( !xrtTlsClientKeyShares(KeyShares, &Cursor) ) {
		return XTLS_ITEM_ERROR;
	}
	if ( !__xrtTlsKeyShareFindValid(&Cursor, iGroup, &Share) ) {
		return XTLS_ITEM_DONE;
	}
	*pShare = Share;
	return XTLS_ITEM_VALUE;
}



/* 按本地版本偏好选择 supported_versions 中的第一个交集。 */
XRT_API xtlsitemresult xrtTlsVersionSelect(
	const xtlsids* pOffered,
	const xtlsversion* pPreferred,
	size_t iPreferredCount,
	xtlsversion* pSelected
)
{
	if ( (pOffered == NULL) || (pSelected == NULL) ) {
		return __xrtTlsNegotiateError(
			XERR_ARGUMENT, "select-tls-version",
			"TLS offered versions or output is invalid"
		);
	}
	if ( !__xrtTlsVersionsValid(
		pPreferred, iPreferredCount, "select-tls-version"
	) ) {
		return XTLS_ITEM_ERROR;
	}
	if ( !__xrtTlsIdsDataValid(
		pOffered->Data, "select-tls-version", XERR_PROTOCOL
	) ) {
		return XTLS_ITEM_ERROR;
	}
	for ( size_t i = 0; i < iPreferredCount; i++ ) {
		if ( xrtTlsIdsContain(pOffered, (uint16)pPreferred[i]) ) {
			*pSelected = pPreferred[i];
			return XTLS_ITEM_VALUE;
		}
	}
	return XTLS_ITEM_DONE;
}



/* 从 ClientHello 选择版本，并正确处理扩展缺失的 TLS 1.2 路径。 */
XRT_API xtlsitemresult xrtTlsClientVersionSelect(
	const xtlsclienthello* pHello,
	const xtlsversion* pPreferred,
	size_t iPreferredCount,
	xtlsversion* pSelected
)
{
	static const uint8 Tls12Data[] = { 0x03, 0x03 };
	xtlsextension Extension;
	xtlsids Offered;
	xtlsitemresult Result;

	if ( (pHello == NULL) || (pSelected == NULL) ) {
		return __xrtTlsNegotiateError(
			XERR_ARGUMENT, "select-client-version",
			"TLS ClientHello or version output is invalid"
		);
	}
	if ( pHello->LegacyVersion != XTLS_VERSION_12 ) {
		return __xrtTlsNegotiateError(
			XERR_VALUE, "select-client-version",
			"TLS ClientHello legacy version is invalid"
		);
	}
	Result = xrtTlsExtensionsFind(
		pHello->Extensions, XTLS_EXTENSION_SUPPORTED_VERSIONS,
		&Extension
	);
	if ( Result == XTLS_ITEM_ERROR ) {
		return Result;
	}
	if ( Result == XTLS_ITEM_VALUE ) {
		if ( !xrtTlsClientVersions(Extension.Data, &Offered) ) {
			return XTLS_ITEM_ERROR;
		}
	} else {
		Offered.Data.Data = Tls12Data;
		Offered.Data.Size = sizeof(Tls12Data);
	}
	return xrtTlsVersionSelect(
		&Offered, pPreferred, iPreferredCount, pSelected
	);
}



/* 判断密码套件能否用于指定版本和握手身份。 */
XRT_API bool xrtTlsCipherCompatible(
	xtlsversion Version,
	xtlscipher Cipher,
	xtlsidentitytype Identity
)
{
	const xtlscipherinfo* pInfo = xrtTlsCipherInfo(Cipher);

	if ( (pInfo == NULL) || (pInfo->Version != Version) ||
		!__xrtTlsIdentityValid(Identity) ) {
		return false;
	}
	if ( Version == XTLS_VERSION_13 ) {
		return true;
	}
	if ( pInfo->Authentication == XTLS_CIPHER_AUTH_RSA ) {
		return (Identity == XTLS_IDENTITY_RSA) ||
			(Identity == XTLS_IDENTITY_RSA_PSS);
	}
	if ( pInfo->Authentication == XTLS_CIPHER_AUTH_ECDSA ) {
		return (Identity == XTLS_IDENTITY_ECDSA_P256) ||
			(Identity == XTLS_IDENTITY_ECDSA_P384) ||
			(Identity == XTLS_IDENTITY_ECDSA_P521) ||
			(Identity == XTLS_IDENTITY_ED25519) ||
			(Identity == XTLS_IDENTITY_ED448);
	}
	return false;
}



/* 按本地偏好选择版本、身份和对端都接受的密码套件。 */
XRT_API xtlsitemresult xrtTlsCipherSelect(
	xtlsversion Version,
	const xtlsids* pOffered,
	xtlsidentitytype Identity,
	const xtlscipher* pPreferred,
	size_t iPreferredCount,
	xtlscipher* pSelected
)
{
	if ( (pOffered == NULL) || (pSelected == NULL) ||
		!__xrtTlsVersionSupported(Version) ||
		!__xrtTlsIdentityValid(Identity) ) {
		return __xrtTlsNegotiateError(
			XERR_ARGUMENT, "select-tls-cipher",
			"TLS cipher selection input or output is invalid"
		);
	}
	if ( !__xrtTlsCiphersValid(
		pPreferred, iPreferredCount, "select-tls-cipher"
	) ) {
		return XTLS_ITEM_ERROR;
	}
	if ( !__xrtTlsIdsDataValid(
		pOffered->Data, "select-tls-cipher", XERR_PROTOCOL
	) ) {
		return XTLS_ITEM_ERROR;
	}
	for ( size_t i = 0; i < iPreferredCount; i++ ) {
		if ( xrtTlsCipherCompatible(
			Version, pPreferred[i], Identity
		) && xrtTlsIdsContain(
			pOffered, (uint16)pPreferred[i]
		) ) {
			*pSelected = pPreferred[i];
			return XTLS_ITEM_VALUE;
		}
	}
	return XTLS_ITEM_DONE;
}



/* 返回签名方案对应的进程期只读元数据。 */
XRT_API const xtlssignatureinfo* xrtTlsSignatureInfo(
	xtlssignature Signature
)
{
	for ( size_t i = 0;
		i < (sizeof(__xrtTlsSignatureInfos) / sizeof(__xrtTlsSignatureInfos[0]));
		i++ ) {
		if ( __xrtTlsSignatureInfos[i].Signature == Signature ) {
			return &__xrtTlsSignatureInfos[i];
		}
	}
	return NULL;
}



/* 判断身份是否属于任一 ECDSA 曲线。 */
static bool __xrtTlsEcdsaIdentity(xtlsidentitytype Identity)
{
	return (Identity == XTLS_IDENTITY_ECDSA_P256) ||
		(Identity == XTLS_IDENTITY_ECDSA_P384) ||
		(Identity == XTLS_IDENTITY_ECDSA_P521);
}



/* 判断签名方案能否用于指定版本和握手身份。 */
XRT_API bool xrtTlsSignatureCompatible(
	xtlsversion Version,
	xtlssignature Signature,
	xtlsidentitytype Identity
)
{
	const xtlssignatureinfo* pInfo = xrtTlsSignatureInfo(Signature);

	if ( !__xrtTlsVersionSupported(Version) ||
		!__xrtTlsIdentityValid(Identity) || (pInfo == NULL) ||
		(Version < pInfo->Minimum) || (Version > pInfo->Maximum) ) {
		return false;
	}
	if ( !__xrtTlsEcdsaIdentity(Identity) ) {
		return pInfo->Identity == Identity;
	}
	if ( !__xrtTlsEcdsaIdentity(pInfo->Identity) ) {
		return false;
	}
	return (Version == XTLS_VERSION_12) || (pInfo->Identity == Identity);
}



/* 按本地偏好选择版本、身份和对端都接受的握手签名方案。 */
XRT_API xtlsitemresult xrtTlsSignatureSelect(
	xtlsversion Version,
	const xtlsids* pOffered,
	xtlsidentitytype Identity,
	const xtlssignature* pPreferred,
	size_t iPreferredCount,
	xtlssignature* pSelected
)
{
	if ( (pOffered == NULL) || (pSelected == NULL) ||
		!__xrtTlsVersionSupported(Version) ||
		!__xrtTlsIdentityValid(Identity) ) {
		return __xrtTlsNegotiateError(
			XERR_ARGUMENT, "select-tls-signature",
			"TLS signature selection input or output is invalid"
		);
	}
	if ( !__xrtTlsSignaturesValid(
		pPreferred, iPreferredCount, "select-tls-signature"
	) ) {
		return XTLS_ITEM_ERROR;
	}
	if ( !__xrtTlsIdsDataValid(
		pOffered->Data, "select-tls-signature", XERR_PROTOCOL
	) ) {
		return XTLS_ITEM_ERROR;
	}
	for ( size_t i = 0; i < iPreferredCount; i++ ) {
		if ( xrtTlsSignatureCompatible(
			Version, pPreferred[i], Identity
		) && xrtTlsIdsContain(
			pOffered, (uint16)pPreferred[i]
		) ) {
			*pSelected = pPreferred[i];
			return XTLS_ITEM_VALUE;
		}
	}
	return XTLS_ITEM_DONE;
}



/* 选择可直接使用或需要 HelloRetryRequest 的共同密钥共享组。 */
XRT_API xtlsitemresult xrtTlsKeyShareSelect(
	const xtlsids* pGroups,
	xbytesview KeyShares,
	const uint16* pPreferred,
	size_t iPreferredCount,
	xtlskeysharepolicy Policy,
	xtlskeyshareselection* pSelection
)
{
	xtlskeysharecursor Cursor;
	xtlskeyshare Share;
	uint16 iFirstGroup = 0;
	bool bHasKeyShares;
	bool bHasFirst = false;

	if ( (pGroups == NULL) || (pSelection == NULL) ||
		((Policy != XTLS_KEY_SHARE_PREFER_GROUP) &&
		 (Policy != XTLS_KEY_SHARE_PREFER_READY)) ) {
		return __xrtTlsNegotiateError(
			XERR_ARGUMENT, "select-key-share",
			"TLS key-share selection input or output is invalid"
		);
	}
	if ( !__xrtTlsPreferenceListValid(
		pPreferred, iPreferredCount, sizeof(*pPreferred),
		"select-key-share"
	) ) {
		return XTLS_ITEM_ERROR;
	}
	if ( !__xrtTlsIdsDataValid(
		pGroups->Data, "select-key-share", XERR_PROTOCOL
	) ) {
		return XTLS_ITEM_ERROR;
	}
	bHasKeyShares = (KeyShares.Data != NULL) || (KeyShares.Size != 0);
	if ( bHasKeyShares ) {
		if ( !xrtTlsClientKeyShares(KeyShares, &Cursor) ||
			!__xrtTlsKeySharesMatchGroups(pGroups, &Cursor) ) {
			return XTLS_ITEM_ERROR;
		}
	} else {
		memset(&Cursor, 0, sizeof(Cursor));
	}

	for ( size_t i = 0; i < iPreferredCount; i++ ) {
		if ( !xrtTlsIdsContain(pGroups, pPreferred[i]) ) {
			continue;
		}
		if ( !bHasFirst ) {
			iFirstGroup = pPreferred[i];
			bHasFirst = true;
		}
		if ( bHasKeyShares && __xrtTlsKeyShareFindValid(
			&Cursor, pPreferred[i], &Share
		) ) {
			__xrtTlsKeySharePublish(
				pPreferred[i], &Share, pSelection
			);
			return XTLS_ITEM_VALUE;
		}
		if ( Policy == XTLS_KEY_SHARE_PREFER_GROUP ) {
			__xrtTlsKeySharePublish(
				pPreferred[i], NULL, pSelection
			);
			return XTLS_ITEM_VALUE;
		}
	}
	if ( bHasFirst ) {
		__xrtTlsKeySharePublish(iFirstGroup, NULL, pSelection);
		return XTLS_ITEM_VALUE;
	}
	return XTLS_ITEM_DONE;
}

#endif
