#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE)

/* 当前 TLS 密钥交换层只承接已经具备完整密码后端的四个命名组。 */
static const xtlsgroupinfo __xrtTlsGroups[] = {
	{ XTLS_GROUP_X25519, XTLS_GROUP_KIND_XDH, 32, 32, 32 },
	{ XTLS_GROUP_X448, XTLS_GROUP_KIND_XDH, 56, 56, 56 },
	{ XTLS_GROUP_SECP256R1, XTLS_GROUP_KIND_ECDH, 32, 65, 32 },
	{ XTLS_GROUP_SECP384R1, XTLS_GROUP_KIND_ECDH, 48, 97, 48 }
};



/* 设置密钥交换层错误并返回 false。 */
static bool __xrtTlsKeyExchangeError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtTlsError(
		Kind, XTLS_ERROR_KEY_EXCHANGE,
		sOperation, sMessage, SIZE_MAX
	);
	return false;
}



/* 把密码后端失败保留为 TLS 密钥交换错误的原因。 */
static bool __xrtTlsKeyExchangeCause(
	cstr sOperation,
	cstr sMessage
)
{
	const xerror* pCause = xrtGetError();

	__xrtTlsErrorCause(
		XERR_PROTOCOL, XTLS_ERROR_KEY_EXCHANGE,
		sOperation, sMessage, SIZE_MAX, pCause
	);
	return false;
}



/* 调用当前已编译组后端生成一对临时密钥。 */
static bool __xrtTlsKeyShareGenerateBackend(
	uint16 iGroup,
	void* pPrivate,
	void* pPublic
)
{
	(void)pPrivate;
	(void)pPublic;
	switch ( iGroup ) {
		#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519)
			case XTLS_GROUP_X25519:
				return xrtX25519KeyPair(pPrivate, pPublic);
		#endif
		#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X448)
			case XTLS_GROUP_X448:
				return xrtX448KeyPair(pPrivate, pPublic);
		#endif
		#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256)
			case XTLS_GROUP_SECP256R1:
				return xrtP256KeyPair(pPrivate, pPublic);
		#endif
		#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P384)
			case XTLS_GROUP_SECP384R1:
				return xrtP384KeyPair(pPrivate, pPublic);
		#endif
		default:
			return false;
	}
}



/* 调用当前已编译组后端计算共享秘密。 */
static bool __xrtTlsKeyShareDeriveBackend(
	uint16 iGroup,
	xbytesview Private,
	xbytesview PeerPublic,
	void* pShared
)
{
	(void)Private;
	(void)PeerPublic;
	(void)pShared;
	switch ( iGroup ) {
		#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519)
			case XTLS_GROUP_X25519:
				return xrtX25519Shared(
					Private.Data, PeerPublic.Data, pShared
				);
		#endif
		#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X448)
			case XTLS_GROUP_X448:
				return xrtX448Shared(
					Private.Data, PeerPublic.Data, pShared
				);
		#endif
		#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256)
			case XTLS_GROUP_SECP256R1:
				return xrtP256Shared(
					Private.Data, PeerPublic.Data, pShared
				);
		#endif
		#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P384)
			case XTLS_GROUP_SECP384R1:
				return xrtP384Shared(
					Private.Data, PeerPublic.Data, pShared
				);
		#endif
		default:
			return false;
	}
}



/* 返回命名组的协议尺寸元数据，不把后端裁剪状态混入查询结果。 */
XRT_API const xtlsgroupinfo* xrtTlsGroupInfo(uint16 iGroup)
{
	for ( size_t i = 0;
		i < (sizeof(__xrtTlsGroups) / sizeof(__xrtTlsGroups[0])); i++ ) {
		if ( __xrtTlsGroups[i].Group == iGroup ) {
			return &__xrtTlsGroups[i];
		}
	}
	return NULL;
}



/* 判断命名组是否同时具有生成与共享秘密后端。 */
XRT_API bool xrtTlsGroupAvailable(uint16 iGroup)
{
	switch ( iGroup ) {
		#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519)
			case XTLS_GROUP_X25519:
				return true;
		#endif
		#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X448)
			case XTLS_GROUP_X448:
				return true;
		#endif
		#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256)
			case XTLS_GROUP_SECP256R1:
				return true;
		#endif
		#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P384)
			case XTLS_GROUP_SECP384R1:
				return true;
		#endif
		default:
			return false;
	}
}



/* 校验全部输出后，只生成调用方最终选择的一个组。 */
XRT_API bool xrtTlsKeyShareGenerate(
	uint16 iGroup,
	void* pPrivate,
	size_t iPrivateCapacity,
	void* pPublic,
	size_t iPublicCapacity
)
{
	const xtlsgroupinfo* pInfo = xrtTlsGroupInfo(iGroup);
	xbytesview Public;

	if ( pInfo == NULL ) {
		return __xrtTlsKeyExchangeError(
			XERR_UNSUPPORTED, "tls-key-share-generate",
			"TLS named group is unsupported"
		);
	}
	if ( (pPrivate == NULL) || (pPublic == NULL) ) {
		return __xrtTlsKeyExchangeError(
			XERR_ARGUMENT, "tls-key-share-generate",
			"TLS key-share output is invalid"
		);
	}
	if ( (iPrivateCapacity < pInfo->PrivateSize) ||
		(iPublicCapacity < pInfo->PublicSize) ) {
		return __xrtTlsKeyExchangeError(
			XERR_RANGE, "tls-key-share-generate",
			"TLS key-share output capacity is too small"
		);
	}
	Public.Data = (const uint8*)pPublic;
	Public.Size = pInfo->PublicSize;
	if ( __xrtTlsViewOverlap(pPrivate, pInfo->PrivateSize, Public) ) {
		return __xrtTlsKeyExchangeError(
			XERR_ARGUMENT, "tls-key-share-generate",
			"TLS private and public key outputs overlap"
		);
	}
	if ( !xrtTlsGroupAvailable(iGroup) ) {
		return __xrtTlsKeyExchangeError(
			XERR_UNSUPPORTED, "tls-key-share-generate",
			"TLS named-group backend is not available"
		);
	}
	if ( !__xrtTlsKeyShareGenerateBackend(iGroup, pPrivate, pPublic) ) {
		return __xrtTlsKeyExchangeCause(
			"tls-key-share-generate",
			"TLS key-share generation failed"
		);
	}
	return true;
}



/* 校验本地私钥和对端线路公钥尺寸后，失败原子地派生共享秘密。 */
XRT_API bool xrtTlsKeyShareDerive(
	uint16 iGroup,
	xbytesview Private,
	xbytesview PeerPublic,
	void* pShared,
	size_t iSharedCapacity
)
{
	const xtlsgroupinfo* pInfo = xrtTlsGroupInfo(iGroup);

	if ( pInfo == NULL ) {
		return __xrtTlsKeyExchangeError(
			XERR_UNSUPPORTED, "tls-key-share-derive",
			"TLS named group is unsupported"
		);
	}
	if ( !__xrtTlsViewValid(Private) ||
		(Private.Size != pInfo->PrivateSize) || (pShared == NULL) ) {
		return __xrtTlsKeyExchangeError(
			XERR_ARGUMENT, "tls-key-share-derive",
			"TLS private key or shared-secret output is invalid"
		);
	}
	if ( !__xrtTlsViewValid(PeerPublic) ||
		(PeerPublic.Size != pInfo->PublicSize) ) {
		return __xrtTlsKeyExchangeError(
			XERR_PROTOCOL, "tls-key-share-derive",
			"peer TLS key share has the wrong size"
		);
	}
	if ( iSharedCapacity < pInfo->SharedSize ) {
		return __xrtTlsKeyExchangeError(
			XERR_RANGE, "tls-key-share-derive",
			"TLS shared-secret output capacity is too small"
		);
	}
	if ( !xrtTlsGroupAvailable(iGroup) ) {
		return __xrtTlsKeyExchangeError(
			XERR_UNSUPPORTED, "tls-key-share-derive",
			"TLS named-group backend is not available"
		);
	}
	if ( !__xrtTlsKeyShareDeriveBackend(
		iGroup, Private, PeerPublic, pShared
	) ) {
		return __xrtTlsKeyExchangeCause(
			"tls-key-share-derive",
			"TLS key-share derivation failed"
		);
	}
	return true;
}

#endif
