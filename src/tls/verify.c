#include "../internal/xrt_tls.h"

#include <xrt/tls_verify.h>



#if defined(XRT_FEATURE_TLS_VERIFY)

struct xtlsverifier {
	volatile int32 RefCount;
	xx509store* Store;
	xtlsverifyproc Verify;
	xtlsverifypolicyproc Policy;
	xtlsverifytimeproc Time;
	xtlsverifyreleaseproc Release;
	ptr Context;
	bool AllowSha1;
};



/* 设置 TLS 验证错误并返回 false。 */
static bool __xrtTlsVerifyError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtTlsError(
		Kind, XTLS_ERROR_VERIFY, sOperation, sMessage, SIZE_MAX
	);
	return false;
}



/* 使用当前错误作为原因设置 TLS 验证错误。 */
static bool __xrtTlsVerifyCause(cstr sOperation, cstr sMessage)
{
	const xerror* pCause = xrtGetError();
	xerrkind Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_PROTOCOL;

	__xrtTlsErrorCause(
		Kind, XTLS_ERROR_VERIFY, sOperation,
		sMessage, SIZE_MAX, pCause
	);
	return false;
}



/* 深复制信任库，使验证器不再借用可变锚和 DER 视图。 */
static xx509store* __xrtTlsVerifierStore(const xx509store* pSource)
{
	xx509store* pStore;
	size_t iCount;

	if ( pSource == NULL ) {
		return NULL;
	}
	iCount = xrtX509StoreCount(pSource);
	pStore = xrtX509StoreCreate();
	if ( pStore == NULL ) {
		return NULL;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		const xx509cert* pCertificate = xrtX509StoreCertificate(
			pSource, i
		);

		if ( (pCertificate == NULL) ||
			(xrtX509StoreAdd(
				pStore, pCertificate->Raw.Data, pCertificate->Raw.Size
			) != X509_VALUE) ) {
			xrtX509StoreFree(pStore);
			return NULL;
		}
	}
	return pStore;
}



/* 验证公开对端视图的指针、角色和证书数量。 */
static bool __xrtTlsPeerValid(const xtlspeer* pPeer)
{
	if ( (pPeer == NULL) ||
		((pPeer->Role != XTLS_CLIENT) && (pPeer->Role != XTLS_SERVER)) ||
		((pPeer->Name.Data == NULL) && (pPeer->Name.Size != 0)) ||
		(pPeer->Certificates == NULL) ||
		(pPeer->CertificateCount == 0) ) {
		return __xrtTlsVerifyError(
			XERR_ARGUMENT, "verify-tls-peer",
			"TLS peer verification input is invalid"
		);
	}
	for ( size_t i = 0; i < pPeer->CertificateCount; i++ ) {
		if ( (pPeer->Certificates[i].Raw.Data == NULL) ||
			(pPeer->Certificates[i].Raw.Size == 0) ) {
			return __xrtTlsVerifyError(
				XERR_ARGUMENT, "verify-tls-peer",
				"TLS peer contains an invalid certificate view"
			);
		}
	}
	if ( (pPeer->Role == XTLS_SERVER) && (pPeer->Name.Size == 0) ) {
		return __xrtTlsVerifyError(
			XERR_VALUE, "verify-tls-peer",
			"TLS server verification requires an identity name"
		);
	}
	return true;
}



/* 为路径构建分配发行者和输出指针工作区。 */
static const xx509cert** __xrtTlsPeerWorkspace(size_t iCount)
{
	if ( iCount > (SIZE_MAX / (2u * sizeof(const xx509cert*))) ) {
		(void)__xrtTlsVerifyError(
			XERR_RANGE, "verify-tls-peer",
			"TLS peer certificate count overflows path workspace"
		);
		return NULL;
	}
	return (const xx509cert**)xrtMalloc(
		2u * iCount * sizeof(const xx509cert*)
	);
}



/* 隔离调用线程错误槽并执行默认路径后的附加验证策略。 */
static bool __xrtTlsVerifyPolicy(
	const xtlsverifiedpeer* pPeer,
	xtlsverifypolicyproc pPolicy,
	ptr pContext
)
{
	xerror* pBefore = xrtTakeError();
	bool bAccepted = pPolicy(pPeer, pContext);
	xerror* pAfter = xrtTakeError();

	if ( bAccepted ) {
		xrtErrorFree(pAfter);
		if ( pBefore != NULL ) {
			__xrtErrorSetOwned(pBefore);
		}
		return true;
	}
	xrtErrorFree(pBefore);
	if ( pAfter == NULL ) {
		return __xrtTlsVerifyError(
			XERR_PERMISSION, "verify-tls-policy",
			"TLS verification policy rejected the peer"
		);
	}
	__xrtErrorSetOwned(pAfter);
	return __xrtTlsVerifyCause(
		"verify-tls-policy", "TLS verification policy failed"
	);
}



/* 使用显式时间、证书和借用信任库执行默认路径与身份验证。 */
static bool __xrtTlsPeerVerify(
	const xtlspeer* pPeer,
	const xx509store* pStore,
	xtlsverifypolicyproc pPolicy,
	ptr pContext,
	bool bAllowSha1
)
{
	static const uint8 ServerPurpose[] = {
		0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01
	};
	static const uint8 ClientPurpose[] = {
		0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02
	};
	const xx509cert** ppWorkspace;
	const xx509cert** ppPath;
	xx509pathsource Source;
	xx509pathconfig Config;
	xx509pathresult Result;
	xtlsverifiedpeer Verified;
	bool bVerified;

	if ( !__xrtTlsPeerValid(pPeer) || (pStore == NULL) ) {
		if ( pStore == NULL ) {
			(void)__xrtTlsVerifyError(
				XERR_ARGUMENT, "verify-tls-peer",
				"TLS peer trust store is null"
			);
		}
		return false;
	}
	ppWorkspace = __xrtTlsPeerWorkspace(pPeer->CertificateCount);
	if ( ppWorkspace == NULL ) {
		return __xrtTlsVerifyCause(
			"verify-tls-peer", "TLS peer path workspace allocation failed"
		);
	}
	for ( size_t i = 1; i < pPeer->CertificateCount; i++ ) {
		ppWorkspace[i - 1u] = &pPeer->Certificates[i];
	}
	ppPath = ppWorkspace + pPeer->CertificateCount;
	xrtX509PathConfigInit(&Config);
	memset(&Result, 0, sizeof(Result));
	Config.Time = pPeer->Time;
	if ( bAllowSha1 ) {
		Config.Flags |= X509_PATH_ALLOW_SHA1;
	}
	Config.KeyUsage = X509_USAGE_DIGITAL_SIGNATURE;
	if ( pPeer->Role == XTLS_SERVER ) {
		Config.Purpose = (xbytesview) {
			ServerPurpose, sizeof(ServerPurpose)
		};
	} else {
		Config.Purpose = (xbytesview) {
			ClientPurpose, sizeof(ClientPurpose)
		};
	}
	bVerified = xrtX509StoreSource(
		pStore, ppWorkspace, pPeer->CertificateCount - 1u, &Source
	) && xrtX509PathBuild(
		&pPeer->Certificates[0], &Source, &Config,
		ppPath, pPeer->CertificateCount, &Result
	);
	if ( !bVerified ) {
		xrtFree(ppWorkspace);
		return __xrtTlsVerifyCause(
			"verify-tls-peer",
			"TLS peer certification path is invalid"
		);
	}
	if ( pPeer->Role == XTLS_SERVER ) {
		xx509result Match = xrtX509MatchHost(
			&pPeer->Certificates[0], pPeer->Name, NULL
		);

		if ( Match == X509_DONE ) {
			xrtFree(ppWorkspace);
			return __xrtTlsVerifyError(
				XERR_PROTOCOL, "verify-tls-peer",
				"TLS peer certificate does not match the requested name"
			);
		}
		bVerified = Match == X509_VALUE;
	}
	if ( !bVerified ) {
		xrtFree(ppWorkspace);
		return __xrtTlsVerifyCause(
			"verify-tls-peer",
			"TLS peer identity is invalid"
		);
	}
	if ( pPolicy != NULL ) {
		Verified.Peer = pPeer;
		Verified.Path = ppPath;
		Verified.PathCount = Result.Count;
		Verified.Anchor = Result.Anchor;
		bVerified = __xrtTlsVerifyPolicy(
			&Verified, pPolicy, pContext
		);
	}
	xrtFree(ppWorkspace);
	return bVerified;
}



/* 使用默认现代算法策略验证路径、用途和对端身份。 */
XRT_API bool xrtTlsPeerVerify(
	const xtlspeer* pPeer,
	const xx509store* pStore,
	xtlsverifypolicyproc pPolicy,
	ptr pContext
)
{
	return __xrtTlsPeerVerify(
		pPeer, pStore, pPolicy, pContext, false
	);
}



/* 初始化尚未绑定信任库、回调或自定义时钟的验证器配置。 */
XRT_API void xrtTlsVerifierConfigInit(xtlsverifierconfig* pConfig)
{
	if ( pConfig == NULL ) {
		(void)__xrtTlsVerifyError(
			XERR_ARGUMENT, "init-tls-verifier-config",
			"TLS verifier config is null"
		);
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
}



/* 创建深复制信任库并接管自定义上下文的共享验证器。 */
XRT_API xtlsverifier* xrtTlsVerifierCreate(
	const xtlsverifierconfig* pConfig
)
{
	xtlsverifier* pVerifier;
	xx509store* pStore;

	if ( (pConfig == NULL) ||
		((pConfig->Store == NULL) && (pConfig->Verify == NULL)) ) {
		(void)__xrtTlsVerifyError(
			XERR_ARGUMENT, "create-tls-verifier",
			"TLS verifier requires a trust store or custom verifier"
		);
		return NULL;
	}
	if ( (pConfig->Verify == NULL) &&
		(xrtX509StoreCount(pConfig->Store) == 0) ) {
		(void)__xrtTlsVerifyError(
			XERR_VALUE, "create-tls-verifier",
			"TLS verifier trust store is empty"
		);
		return NULL;
	}
	pStore = __xrtTlsVerifierStore(pConfig->Store);
	if ( (pConfig->Store != NULL) && (pStore == NULL) ) {
		(void)__xrtTlsVerifyCause(
			"create-tls-verifier", "TLS trust store snapshot failed"
		);
		return NULL;
	}
	pVerifier = (xtlsverifier*)xrtMalloc(sizeof(*pVerifier));
	if ( pVerifier == NULL ) {
		xrtX509StoreFree(pStore);
		(void)__xrtTlsVerifyCause(
			"create-tls-verifier", "TLS verifier allocation failed"
		);
		return NULL;
	}
	memset(pVerifier, 0, sizeof(*pVerifier));
	pVerifier->RefCount = 1;
	pVerifier->Store = pStore;
	pVerifier->Verify = pConfig->Verify;
	pVerifier->Policy = pConfig->Policy;
	pVerifier->Time = pConfig->Time;
	pVerifier->Release = pConfig->Release;
	pVerifier->Context = pConfig->Context;
	pVerifier->AllowSha1 = pConfig->AllowSha1;
	return pVerifier;
}



/* 增加不可变验证器引用。 */
XRT_API xtlsverifier* xrtTlsVerifierRetain(
	const xtlsverifier* pVerifier
)
{
	if ( pVerifier == NULL ) {
		(void)__xrtTlsVerifyError(
			XERR_ARGUMENT, "retain-tls-verifier",
			"TLS verifier is null"
		);
		return NULL;
	}
	if ( xrtRefRetain((volatile int32*)&pVerifier->RefCount) < 0 ) {
		(void)__xrtTlsVerifyError(
			XERR_STATE, "retain-tls-verifier",
			"TLS verifier reference is invalid"
		);
		return NULL;
	}
	return (xtlsverifier*)pVerifier;
}



/* 释放最后一个验证器引用、信任快照和自定义上下文。 */
XRT_API void xrtTlsVerifierRelease(xtlsverifier* pVerifier)
{
	if ( (pVerifier == NULL) ||
		(xrtRefRelease(&pVerifier->RefCount) != 0) ) {
		return;
	}
	if ( pVerifier->Release != NULL ) {
		pVerifier->Release(pVerifier->Context);
	}
	xrtX509StoreFree(pVerifier->Store);
	memset(pVerifier, 0, sizeof(*pVerifier));
	xrtFree(pVerifier);
}



/* 执行自定义或默认信任决策。 */
XRT_API bool xrtTlsVerifierVerify(
	const xtlsverifier* pVerifier,
	xtlsrole Role,
	xstrview Name,
	const xx509cert* pCertificates,
	size_t iCertificateCount
)
{
	xtlspeer Peer;
	xtlsverifydecision Decision = XTLS_VERIFY_DEFAULT;
	xerror* pBefore = NULL;
	xerror* pAfter = NULL;

	if ( pVerifier == NULL ) {
		return __xrtTlsVerifyError(
			XERR_ARGUMENT, "run-tls-verifier", "TLS verifier is null"
		);
	}
	Peer.Role = Role;
	Peer.Name = Name;
	Peer.Time = pVerifier->Time != NULL ?
		pVerifier->Time(pVerifier->Context) : xrtNow();
	Peer.Certificates = pCertificates;
	Peer.CertificateCount = iCertificateCount;
	if ( !__xrtTlsPeerValid(&Peer) ) {
		return false;
	}
	if ( pVerifier->Verify != NULL ) {
		pBefore = xrtTakeError();
		Decision = pVerifier->Verify(&Peer, pVerifier->Context);
		pAfter = xrtTakeError();
		if ( Decision == XTLS_VERIFY_ERROR ) {
			xrtErrorFree(pBefore);
			if ( pAfter == NULL ) {
				return __xrtTlsVerifyError(
					XERR_INTERNAL, "run-tls-verifier",
					"TLS custom verifier failed without setting an error"
				);
			}
			__xrtErrorSetOwned(pAfter);
			return __xrtTlsVerifyCause(
				"run-tls-verifier", "TLS custom verifier failed"
			);
		}
		xrtErrorFree(pAfter);
		if ( pBefore != NULL ) {
			__xrtErrorSetOwned(pBefore);
		}
	}
	if ( Decision == XTLS_VERIFY_ACCEPT ) {
		return true;
	}
	if ( Decision == XTLS_VERIFY_DEFAULT ) {
		if ( pVerifier->Store == NULL ) {
			return __xrtTlsVerifyError(
				XERR_STATE, "run-tls-verifier",
				"TLS custom verifier deferred without a trust store"
			);
		}
		return __xrtTlsPeerVerify(
			&Peer, pVerifier->Store,
			pVerifier->Policy, pVerifier->Context,
			pVerifier->AllowSha1
		);
	}
	if ( Decision == XTLS_VERIFY_REJECT ) {
		return __xrtTlsVerifyError(
			XERR_PERMISSION, "run-tls-verifier",
			"TLS custom verifier rejected the peer"
		);
	}
	return __xrtTlsVerifyError(
		XERR_VALUE, "run-tls-verifier",
		"TLS custom verifier returned an invalid decision"
	);
}



/* 把 TLS 线路签名方案转换为通用 X.509 密码验签描述。 */
static bool __xrtTlsVerifyScheme(
	xtlsversion Version,
	xtlssignature Signature,
	const xx509pubkey* pPublicKey,
	xx509signature* pScheme,
	cstr sOperation
)
{
	memset(pScheme, 0, sizeof(*pScheme));
	switch ( Signature ) {
		case XTLS_SIGNATURE_RSA_PKCS1_SHA256:
		case XTLS_SIGNATURE_RSA_PKCS1_SHA384:
		case XTLS_SIGNATURE_RSA_PKCS1_SHA512:
			if ( (Version != XTLS_VERSION_12) ||
				(pPublicKey->Type != X509_KEY_RSA) ) {
				break;
			}
			pScheme->Type = X509_SIGNATURE_RSA_PKCS1;
			break;
		case XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256:
			if ( (pPublicKey->Type != X509_KEY_EC) ||
				((Version == XTLS_VERSION_13) &&
				 (pPublicKey->Curve != X509_CURVE_P256)) ) {
				break;
			}
			pScheme->Type = X509_SIGNATURE_ECDSA;
			pScheme->Hash = X509_HASH_SHA256;
			return true;
		case XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384:
			if ( (pPublicKey->Type != X509_KEY_EC) ||
				((Version == XTLS_VERSION_13) &&
				 (pPublicKey->Curve != X509_CURVE_P384)) ) {
				break;
			}
			pScheme->Type = X509_SIGNATURE_ECDSA;
			pScheme->Hash = X509_HASH_SHA384;
			return true;
		case XTLS_SIGNATURE_ECDSA_SECP521R1_SHA512:
			if ( (Version != XTLS_VERSION_12) ||
				(pPublicKey->Type != X509_KEY_EC) ) {
				break;
			}
			pScheme->Type = X509_SIGNATURE_ECDSA;
			pScheme->Hash = X509_HASH_SHA512;
			return true;
		case XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256:
		case XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384:
		case XTLS_SIGNATURE_RSA_PSS_RSAE_SHA512:
			if ( pPublicKey->Type != X509_KEY_RSA ) {
				break;
			}
			pScheme->Type = X509_SIGNATURE_RSA_PSS;
			break;
		case XTLS_SIGNATURE_RSA_PSS_PSS_SHA256:
		case XTLS_SIGNATURE_RSA_PSS_PSS_SHA384:
		case XTLS_SIGNATURE_RSA_PSS_PSS_SHA512:
			if ( pPublicKey->Type != X509_KEY_RSA_PSS ) {
				break;
			}
			pScheme->Type = X509_SIGNATURE_RSA_PSS;
			break;
		case XTLS_SIGNATURE_ED25519:
			if ( pPublicKey->Type != X509_KEY_ED25519 ) {
				break;
			}
			pScheme->Type = X509_SIGNATURE_ED25519;
			return true;
		default:
			break;
	}
	if ( pScheme->Type == X509_SIGNATURE_RSA_PKCS1 ) {
		pScheme->Hash = X509_HASH_SHA256;
		if ( Signature == XTLS_SIGNATURE_RSA_PKCS1_SHA384 ) {
			pScheme->Hash = X509_HASH_SHA384;
		} else if ( Signature == XTLS_SIGNATURE_RSA_PKCS1_SHA512 ) {
			pScheme->Hash = X509_HASH_SHA512;
		}
		return true;
	}
	if ( pScheme->Type == X509_SIGNATURE_RSA_PSS ) {
		pScheme->MaskHash = X509_HASH_SHA256;
		pScheme->SaltSize = 32u;
		if ( (Signature == XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384) ||
			(Signature == XTLS_SIGNATURE_RSA_PSS_PSS_SHA384) ) {
			pScheme->MaskHash = X509_HASH_SHA384;
			pScheme->SaltSize = 48u;
		} else if ( (Signature == XTLS_SIGNATURE_RSA_PSS_RSAE_SHA512) ||
			(Signature == XTLS_SIGNATURE_RSA_PSS_PSS_SHA512) ) {
			pScheme->MaskHash = X509_HASH_SHA512;
			pScheme->SaltSize = 64u;
		}
		pScheme->Hash = pScheme->MaskHash;
		pScheme->Trailer = 1u;
		return true;
	}
	return __xrtTlsVerifyError(
		XERR_PROTOCOL, sOperation,
		"TLS signature scheme and certificate key are incompatible"
	);
}



/* 验证 TLS 1.2 ECDHE 参数签名，不让调用方重复拼接线路签名内容。 */
XRT_API bool xrtTls12ServerKeyExchangeVerify(
	xtlssignature SignatureScheme,
	xbytesview ClientRandom,
	xbytesview ServerRandom,
	xbytesview Parameters,
	xbytesview Signature,
	const xx509pubkey* pPublicKey
)
{
	uint8 Content[(XTLS_RANDOM_SIZE * 2u) + 3u + 1u + UINT8_MAX];
	size_t iContentSize;
	xx509signature Scheme;

	if ( (ClientRandom.Data == NULL) ||
		(ClientRandom.Size != XTLS_RANDOM_SIZE) ||
		(ServerRandom.Data == NULL) ||
		(ServerRandom.Size != XTLS_RANDOM_SIZE) ||
		(Parameters.Data == NULL) || (Parameters.Size < 4u) ||
		(Parameters.Size > (3u + 1u + UINT8_MAX)) ||
		(Signature.Data == NULL) || (Signature.Size == 0) ||
		(pPublicKey == NULL) ) {
		return __xrtTlsVerifyError(
			XERR_ARGUMENT, "verify-tls12-server-key-exchange",
			"TLS 1.2 ServerKeyExchange signature input is invalid"
		);
	}
	if ( !__xrtTlsVerifyScheme(
		XTLS_VERSION_12, SignatureScheme, pPublicKey, &Scheme,
		"verify-tls12-server-key-exchange"
	) ) {
		return false;
	}
	memcpy(Content, ClientRandom.Data, XTLS_RANDOM_SIZE);
	memcpy(
		Content + XTLS_RANDOM_SIZE,
		ServerRandom.Data, XTLS_RANDOM_SIZE
	);
	memcpy(
		Content + (XTLS_RANDOM_SIZE * 2u),
		Parameters.Data, Parameters.Size
	);
	iContentSize = (XTLS_RANDOM_SIZE * 2u) + Parameters.Size;
	if ( !xrtX509SignatureVerify(
		&Scheme, (xbytesview) { Content, iContentSize },
		Signature, pPublicKey
	) ) {
		xrtSecureZero(Content, sizeof(Content));
		return __xrtTlsVerifyCause(
			"verify-tls12-server-key-exchange",
			"TLS 1.2 ServerKeyExchange signature is invalid"
		);
	}
	xrtSecureZero(Content, sizeof(Content));
	return true;
}



/* 验证 TLS 1.3 对端 CertificateVerify。 */
XRT_API bool xrtTls13CertificateVerifySignature(
	xtlsrole Signer,
	xtlssignature SignatureScheme,
	xbytesview TranscriptHash,
	xbytesview Signature,
	const xx509pubkey* pPublicKey
)
{
	uint8 Content[64u + 34u + 1u + 48u];
	size_t iContentSize;
	xx509signature Scheme;

	if ( ((Signer != XTLS_CLIENT) && (Signer != XTLS_SERVER)) ||
		((TranscriptHash.Size != 32u) && (TranscriptHash.Size != 48u)) ||
		(TranscriptHash.Data == NULL) || (Signature.Data == NULL) ||
		(Signature.Size == 0) || (pPublicKey == NULL) ) {
		return __xrtTlsVerifyError(
			XERR_ARGUMENT, "verify-tls13-certificate-signature",
			"TLS 1.3 CertificateVerify input is invalid"
		);
	}
	if ( !__xrtTlsVerifyScheme(
		XTLS_VERSION_13, SignatureScheme, pPublicKey, &Scheme,
		"verify-tls13-certificate-signature"
	) ) {
		return false;
	}
	iContentSize = xrtTls13CertificateVerifyContentSize(
		Signer, TranscriptHash.Size
	);
	if ( (iContentSize == 0) || !xrtTls13CertificateVerifyContentEncode(
		Signer, TranscriptHash, Content, sizeof(Content)
	) ) {
		xrtSecureZero(Content, sizeof(Content));
		return false;
	}
	if ( !xrtX509SignatureVerify(
		&Scheme, (xbytesview) { Content, iContentSize },
		Signature, pPublicKey
	) ) {
		xrtSecureZero(Content, sizeof(Content));
		return __xrtTlsVerifyCause(
			"verify-tls13-certificate-signature",
			"TLS 1.3 CertificateVerify signature is invalid"
		);
	}
	xrtSecureZero(Content, sizeof(Content));
	return true;
}

#endif
