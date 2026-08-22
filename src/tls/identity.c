#include "../internal/xrt_tls_identity.h"



#if defined(XRT_FEATURE_TLS_IDENTITY)

/* 设置 TLS 身份层错误并返回 false。 */
bool __xrtTlsIdentityError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtTlsError(
		Kind, XTLS_ERROR_IDENTITY,
		sOperation, sMessage, SIZE_MAX
	);
	return false;
}



/* 包装当前错误为 TLS 身份失败原因并返回 false。 */
bool __xrtTlsIdentityCause(cstr sOperation, cstr sMessage)
{
	const xerror* pCause = xrtGetError();
	xerrkind Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_INTERNAL;

	__xrtTlsErrorCause(
		Kind, XTLS_ERROR_IDENTITY,
		sOperation, sMessage, SIZE_MAX, pCause
	);
	return false;
}



/* 验证需要实际身份对象的公共入口。 */
static bool __xrtTlsIdentityObjectValid(
	const xtlsidentity* pIdentity,
	cstr sOperation
)
{
	if ( pIdentity != NULL ) {
		return true;
	}
	return __xrtTlsIdentityError(
		XERR_ARGUMENT, sOperation, "TLS identity is null"
	);
}



/* 把 X.509 叶公钥映射到与协商层解耦的 TLS 身份类型。 */
static xtlsidentitytype __xrtTlsIdentityPublicType(
	const xx509pubkey* pPublicKey
)
{
	if ( pPublicKey->Type == X509_KEY_RSA ) {
		return XTLS_IDENTITY_RSA;
	}
	if ( pPublicKey->Type == X509_KEY_RSA_PSS ) {
		return XTLS_IDENTITY_RSA_PSS;
	}
	if ( pPublicKey->Type == X509_KEY_EC ) {
		if ( pPublicKey->Curve == X509_CURVE_P256 ) {
			return XTLS_IDENTITY_ECDSA_P256;
		}
		if ( pPublicKey->Curve == X509_CURVE_P384 ) {
			return XTLS_IDENTITY_ECDSA_P384;
		}
		if ( pPublicKey->Curve == X509_CURVE_P521 ) {
			return XTLS_IDENTITY_ECDSA_P521;
		}
	}
	if ( pPublicKey->Type == X509_KEY_ED25519 ) {
		return XTLS_IDENTITY_ED25519;
	}
	if ( pPublicKey->Type == X509_KEY_ED448 ) {
		return XTLS_IDENTITY_ED448;
	}
	return XTLS_IDENTITY_NONE;
}



/* 安全累加单块身份分配所需空间。 */
static bool __xrtTlsIdentityAddSize(
	size_t* pTotal,
	size_t iSize
)
{
	if ( iSize > (SIZE_MAX - *pTotal) ) {
		return __xrtTlsIdentityError(
			XERR_RANGE, "create-tls-identity",
			"TLS identity storage size overflows"
		);
	}
	*pTotal += iSize;
	return true;
}



/* 计算对象、视图、证书和对齐后端尾部的紧凑分配布局。 */
static bool __xrtTlsIdentitySize(
	const xbytesview* pCertificates,
	size_t iCertificateCount,
	size_t iExtraSize,
	size_t* pTotal,
	size_t* pExtraOffset
)
{
	size_t iSize = sizeof(xtlsidentity);
	size_t iPadding;

	if ( iCertificateCount > ((SIZE_MAX - iSize) / sizeof(xbytesview)) ) {
		return __xrtTlsIdentityError(
			XERR_RANGE, "create-tls-identity",
			"TLS identity certificate count overflows"
		);
	}
	iSize += iCertificateCount * sizeof(xbytesview);
	for ( size_t i = 0; i < iCertificateCount; i++ ) {
		if ( (pCertificates[i].Data == NULL) ||
			(pCertificates[i].Size == 0) ||
			!__xrtTlsIdentityAddSize(&iSize, pCertificates[i].Size) ) {
			if ( (pCertificates[i].Data == NULL) ||
				(pCertificates[i].Size == 0) ) {
				return __xrtTlsIdentityError(
					XERR_ARGUMENT, "create-tls-identity",
					"TLS identity certificate is empty"
				);
			}
			return false;
		}
	}
	iPadding = (sizeof(ptr) - (iSize % sizeof(ptr))) % sizeof(ptr);
	if ( !__xrtTlsIdentityAddSize(&iSize, iPadding) ) {
		return false;
	}
	*pExtraOffset = iSize;
	if ( !__xrtTlsIdentityAddSize(&iSize, iExtraSize) ) {
		return false;
	}
	*pTotal = iSize;
	return true;
}



/* 复制全部证书并在复制后的稳定地址上严格解析叶证书公钥。 */
static bool __xrtTlsIdentityCopyCertificates(
	xtlsidentity* pIdentity,
	const xbytesview* pCertificates,
	size_t iCertificateCount,
	xtlsidentitytype Type
)
{
	bytes pWrite = (bytes)(pIdentity->Certificates + iCertificateCount);

	for ( size_t i = 0; i < iCertificateCount; i++ ) {
		xx509cert Certificate;

		memcpy(pWrite, pCertificates[i].Data, pCertificates[i].Size);
		pIdentity->Certificates[i].Data = pWrite;
		pIdentity->Certificates[i].Size = pCertificates[i].Size;
		if ( !xrtX509Parse(pWrite, pCertificates[i].Size, &Certificate) ) {
			return __xrtTlsIdentityCause(
				"create-tls-identity",
				"TLS identity certificate parsing failed"
			);
		}
		if ( i == 0 ) {
			pIdentity->Leaf = Certificate;
			if ( !xrtX509PublicKey(
				&pIdentity->Leaf, &pIdentity->PublicKey
			) ) {
				return __xrtTlsIdentityCause(
					"create-tls-identity",
					"TLS identity leaf public key parsing failed"
				);
			}
		}
		pWrite += pCertificates[i].Size;
	}
	if ( __xrtTlsIdentityPublicType(&pIdentity->PublicKey) != Type ) {
		return __xrtTlsIdentityError(
			XERR_VALUE, "create-tls-identity",
			"TLS identity type does not match the leaf certificate"
		);
	}
	return true;
}



/* 创建证书快照和精确大小的后端私有尾部。 */
xtlsidentity* __xrtTlsIdentityNew(
	const xbytesview* pCertificates,
	size_t iCertificateCount,
	xtlsidentitytype Type,
	size_t iExtraSize,
	__xrttlsidentitysupportsproc pSupports,
	__xrttlsidentitysignproc pSign,
	ptr* pExtra
)
{
	xtlsidentity* pIdentity;
	size_t iSize = 0;
	size_t iExtraOffset = 0;

	if ( (pCertificates == NULL) || (iCertificateCount == 0) ||
		(Type == XTLS_IDENTITY_NONE) || (pSign == NULL) ) {
		(void)__xrtTlsIdentityError(
			XERR_ARGUMENT, "create-tls-identity",
			"TLS identity configuration is incomplete"
		);
		return NULL;
	}
	if ( !__xrtTlsIdentitySize(
		pCertificates, iCertificateCount, iExtraSize,
		&iSize, &iExtraOffset
	) ) {
		return NULL;
	}
	pIdentity = (xtlsidentity*)xrtCalloc(1, iSize);
	if ( pIdentity == NULL ) {
		(void)__xrtTlsIdentityCause(
			"create-tls-identity", "TLS identity allocation failed"
		);
		return NULL;
	}
	pIdentity->RefCount = 1;
	pIdentity->AllocationSize = iSize;
	pIdentity->CertificateCount = iCertificateCount;
	pIdentity->ExtraOffset = iExtraOffset;
	pIdentity->Type = Type;
	pIdentity->Certificates = (xbytesview*)(pIdentity + 1);
	pIdentity->Supports = pSupports;
	pIdentity->Sign = pSign;
	if ( !__xrtTlsIdentityCopyCertificates(
		pIdentity, pCertificates, iCertificateCount, Type
	) ) {
		xrtSecureZero(pIdentity, iSize);
		xrtFree(pIdentity);
		return NULL;
	}
	if ( pExtra != NULL ) {
		*pExtra = (bytes)pIdentity + iExtraOffset;
	}
	return pIdentity;
}



/* 返回仅供内置后端使用的对齐私有尾部。 */
ptr __xrtTlsIdentityExtra(const xtlsidentity* pIdentity)
{
	return (bytes)pIdentity + pIdentity->ExtraOffset;
}



/* 自定义身份按配置声明或通用兼容规则判断方案。 */
static bool __xrtTlsIdentityExternalSupports(
	const xtlsidentity* pIdentity,
	xtlsversion Version,
	xtlssignature Signature
)
{
	if ( pIdentity->ExternalSupports == NULL ) {
		return true;
	}
	return pIdentity->ExternalSupports(
		pIdentity->ExternalContext, Version, Signature
	);
}



/* 把不可变身份转发给调用方提供的外部签名器。 */
static bool __xrtTlsIdentityExternalSign(
	const xtlsidentity* pIdentity,
	xtlsversion Version,
	xtlssignature Signature,
	xbytesview Message,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return pIdentity->ExternalSign(
		pIdentity->ExternalContext, Version, Signature,
		Message, pOutput, iCapacity, pSize
	);
}



/* 创建深复制证书链、接管外部签名上下文的自定义共享身份。 */
XRT_API xtlsidentity* xrtTlsIdentityCreate(
	const xtlsidentityconfig* pConfig
)
{
	xtlsidentity* pIdentity;

	if ( (pConfig == NULL) || (pConfig->Sign == NULL) ) {
		(void)__xrtTlsIdentityError(
			XERR_ARGUMENT, "create-tls-identity",
			"TLS identity custom signer is missing"
		);
		return NULL;
	}
	pIdentity = __xrtTlsIdentityNew(
		pConfig->Certificates, pConfig->CertificateCount,
		pConfig->Type, 0,
		__xrtTlsIdentityExternalSupports,
		__xrtTlsIdentityExternalSign, NULL
	);
	if ( pIdentity == NULL ) {
		return NULL;
	}
	pIdentity->ExternalSupports = pConfig->Supports;
	pIdentity->ExternalSign = pConfig->Sign;
	pIdentity->ExternalRelease = pConfig->Release;
	pIdentity->ExternalContext = pConfig->Context;
	return pIdentity;
}



/* 增加共享身份引用并拒绝无效或耗尽的引用计数。 */
XRT_API xtlsidentity* xrtTlsIdentityRetain(
	const xtlsidentity* pIdentity
)
{
	if ( !__xrtTlsIdentityObjectValid(pIdentity, "retain-tls-identity") ) {
		return NULL;
	}
	if ( xrtRefRetain((volatile int32*)&pIdentity->RefCount) < 0 ) {
		(void)__xrtTlsIdentityError(
			XERR_STATE, "retain-tls-identity",
			"TLS identity reference is invalid"
		);
		return NULL;
	}
	return (xtlsidentity*)pIdentity;
}



/* 释放最后一个身份引用、签名上下文和已清除的紧凑存储。 */
XRT_API void xrtTlsIdentityRelease(xtlsidentity* pIdentity)
{
	size_t iSize;
	xtlsidentityreleaseproc pRelease;
	ptr pContext;

	if ( (pIdentity == NULL) ||
		(xrtRefRelease(&pIdentity->RefCount) != 0) ) {
		return;
	}
	iSize = pIdentity->AllocationSize;
	pRelease = pIdentity->ExternalRelease;
	pContext = pIdentity->ExternalContext;
	if ( pRelease != NULL ) {
		pRelease(pContext);
	}
	xrtSecureZero(pIdentity, iSize);
	xrtFree(pIdentity);
}



/* 返回叶证书对应的握手签名密钥类型。 */
XRT_API xtlsidentitytype xrtTlsIdentityType(
	const xtlsidentity* pIdentity
)
{
	if ( !__xrtTlsIdentityObjectValid(pIdentity, "get-tls-identity-type") ) {
		return XTLS_IDENTITY_NONE;
	}
	return pIdentity->Type;
}



/* 返回按 TLS 发送顺序保存的证书数量。 */
XRT_API size_t xrtTlsIdentityCertificateCount(
	const xtlsidentity* pIdentity
)
{
	if ( !__xrtTlsIdentityObjectValid(
		pIdentity, "count-tls-identity-certificates"
	) ) {
		return 0;
	}
	return pIdentity->CertificateCount;
}



/* 借用指定位置的完整 DER 证书。 */
XRT_API bool xrtTlsIdentityCertificate(
	const xtlsidentity* pIdentity,
	size_t iIndex,
	xbytesview* pCertificate
)
{
	if ( !__xrtTlsIdentityObjectValid(
		pIdentity, "get-tls-identity-certificate"
	) || (pCertificate == NULL) ) {
		if ( (pIdentity != NULL) && (pCertificate == NULL) ) {
			return __xrtTlsIdentityError(
				XERR_ARGUMENT, "get-tls-identity-certificate",
				"TLS identity certificate output is null"
			);
		}
		return false;
	}
	if ( iIndex >= pIdentity->CertificateCount ) {
		return __xrtTlsIdentityError(
			XERR_RANGE, "get-tls-identity-certificate",
			"TLS identity certificate index is out of range"
		);
	}
	*pCertificate = pIdentity->Certificates[iIndex];
	return true;
}



/* 借用已经严格解析并与身份类型匹配的叶证书公钥。 */
XRT_API bool xrtTlsIdentityPublicKey(
	const xtlsidentity* pIdentity,
	xx509pubkey* pPublicKey
)
{
	if ( !__xrtTlsIdentityObjectValid(
		pIdentity, "get-tls-identity-public-key"
	) || (pPublicKey == NULL) ) {
		if ( (pIdentity != NULL) && (pPublicKey == NULL) ) {
			return __xrtTlsIdentityError(
				XERR_ARGUMENT, "get-tls-identity-public-key",
				"TLS identity public key output is null"
			);
		}
		return false;
	}
	*pPublicKey = pIdentity->PublicKey;
	return true;
}



/* 判断身份类型、协议版本和后端能力是否共同支持签名方案。 */
XRT_API bool xrtTlsIdentityCanSign(
	const xtlsidentity* pIdentity,
	xtlsversion Version,
	xtlssignature Signature
)
{
	if ( !__xrtTlsIdentityObjectValid(
		pIdentity, "query-tls-identity-sign"
	) ) {
		return false;
	}
	if ( !xrtTlsSignatureCompatible(
		Version, Signature, pIdentity->Type
	) ) {
		return false;
	}
	return (pIdentity->Supports == NULL) ||
		pIdentity->Supports(pIdentity, Version, Signature);
}



/* 先查询精确长度，再执行一次不会越过容量的身份签名。 */
XRT_API bool xrtTlsIdentitySign(
	const xtlsidentity* pIdentity,
	xtlsversion Version,
	xtlssignature Signature,
	xbytesview Message,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	size_t iRequired = 0;
	size_t iWritten = 0;

	if ( !__xrtTlsIdentityObjectValid(pIdentity, "sign-tls-identity") ||
		(pSize == NULL) || ((Message.Data == NULL) && (Message.Size != 0)) ||
		((pOutput == NULL) && (iCapacity != 0)) ) {
		if ( (pIdentity != NULL) &&
			((pSize == NULL) ||
			 ((Message.Data == NULL) && (Message.Size != 0)) ||
			 ((pOutput == NULL) && (iCapacity != 0))) ) {
			return __xrtTlsIdentityError(
				XERR_ARGUMENT, "sign-tls-identity",
				"TLS identity signing arguments are invalid"
			);
		}
		return false;
	}
	if ( !xrtTlsIdentityCanSign(pIdentity, Version, Signature) ) {
		return __xrtTlsIdentityError(
			XERR_VALUE, "sign-tls-identity",
			"TLS identity does not support the signature scheme"
		);
	}
	if ( !pIdentity->Sign(
		pIdentity, Version, Signature, Message,
		NULL, 0, &iRequired
	) ) {
		return __xrtTlsIdentityCause(
			"sign-tls-identity", "TLS identity signature sizing failed"
		);
	}
	if ( iRequired == 0 ) {
		return __xrtTlsIdentityError(
			XERR_STATE, "sign-tls-identity",
			"TLS identity signer returned an empty signature size"
		);
	}
	if ( pOutput == NULL ) {
		*pSize = iRequired;
		return true;
	}
	if ( iCapacity < iRequired ) {
		return __xrtTlsIdentityError(
			XERR_RANGE, "sign-tls-identity",
			"TLS identity signature buffer is too small"
		);
	}
	if ( !pIdentity->Sign(
		pIdentity, Version, Signature, Message,
		pOutput, iCapacity, &iWritten
	) ) {
		return __xrtTlsIdentityCause(
			"sign-tls-identity", "TLS identity signing failed"
		);
	}
	if ( (iWritten == 0) || (iWritten > iCapacity) ) {
		return __xrtTlsIdentityError(
			XERR_STATE, "sign-tls-identity",
			"TLS identity signer returned an invalid signature size"
		);
	}
	*pSize = iWritten;
	return true;
}

#endif
