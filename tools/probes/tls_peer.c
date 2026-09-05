/* 仅供外部互操作测试使用的 C ABI 桥；不属于 XRT 模块或生成单头。 */
#include <xrt.h>
#include <string.h>

xtlssession* xrtTestTlsCreate(int iServer, int iVersion, uint16 iCipher,
	uint16 iSignature, int iP256, const char* sName,
	const void* pCertificate, size_t iCertificateSize,
	const void* pPrivateKey, size_t iPrivateKeySize,
	const void* pRoot, size_t iRootSize)
{
	xtlsversion Version = iVersion == 13 ? XTLS_VERSION_13 : XTLS_VERSION_12;
	xtlscipher Cipher = (xtlscipher)iCipher;
	xtlssignature Signature = (xtlssignature)iSignature;
	uint16 Groups[] = { XTLS_GROUP_X25519, XTLS_GROUP_SECP256R1, XTLS_GROUP_SECP384R1 };
	xstrview Protocol = XRT_STR_LITERAL("xrt-interop");
	xbytesview Certificate = { pCertificate, iCertificateSize };
	xbytesview PrivateKey = { pPrivateKey, iPrivateKeySize };
	xtlspolicy Policy;
	xtlscontextconfig ContextConfig;
	xtlscontext* pContext;
	xtlssession* pSession = NULL;
	xtlsidentity* pIdentity = NULL;
	xtlsverifier* pVerifier = NULL;
	xx509store* pStore = NULL;

	xrtTlsPolicyInit(&Policy);
	Policy.Versions = &Version;
	Policy.VersionCount = 1;
	Policy.Ciphers = &Cipher;
	Policy.CipherCount = 1;
	Policy.Signatures = &Signature;
	Policy.SignatureCount = 1;
	Policy.Groups = iP256 ? Groups + 1 : Groups;
	Policy.GroupCount = iP256 ? 1 : 3;
	xrtTlsContextConfigInit(&ContextConfig);
	ContextConfig.Policy = &Policy;
	pContext = xrtTlsContextCreate(&ContextConfig);
	if ( pContext == NULL ) {
		return NULL;
	}
	if ( iServer ) {
		xtlsserverconfig Config;

		switch ( Signature ) {
			case XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256:
				pIdentity = xrtTlsIdentityP256(&Certificate, 1, PrivateKey);
				break;
			case XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384:
				pIdentity = xrtTlsIdentityP384(&Certificate, 1, PrivateKey);
				break;
			case XTLS_SIGNATURE_ED25519:
				pIdentity = xrtTlsIdentityEd25519(&Certificate, 1, PrivateKey);
				break;
			default:
				pIdentity = xrtTlsIdentityRsa(&Certificate, 1, PrivateKey);
				break;
		}
		if ( pIdentity == NULL ) {
			goto cleanup;
		}
		xrtTlsServerConfigInit(&Config);
		Config.Context = pContext;
		Config.Identity = pIdentity;
		Config.Protocols = &Protocol;
		Config.ProtocolCount = 1;
		Config.RequireProtocol = true;
		pSession = xrtTlsServerCreate(&Config, NULL);
	} else {
		xtlsclientconfig Config;
		xtlsverifierconfig Verify;

		pStore = xrtX509StoreCreate();
		if ( (pStore == NULL) ||
			(xrtX509StoreAdd(pStore, pRoot, iRootSize) != X509_VALUE) ) {
			goto cleanup;
		}
		xrtTlsVerifierConfigInit(&Verify);
		Verify.Store = pStore;
		pVerifier = xrtTlsVerifierCreate(&Verify);
		if ( pVerifier == NULL ) {
			goto cleanup;
		}
		xrtTlsClientConfigInit(&Config);
		Config.Context = pContext;
		Config.Verifier = pVerifier;
		Config.ServerName = (xstrview) { sName, strlen(sName) };
		Config.Protocols = &Protocol;
		Config.ProtocolCount = 1;
		pSession = xrtTlsClientCreate(&Config, NULL);
	}
cleanup:
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsVerifierRelease(pVerifier);
	xrtX509StoreFree(pStore);
	xrtTlsContextRelease(pContext);
	return pSession;
}

/* 每步有界驱动并取出密文和明文；返回 0 握手中、1 READY、2 CLOSED、-1 失败。 */
int xrtTestTlsStep(xtlssession* pSession, int iServer,
	const void* pInput, size_t iInputSize,
	void* pOutput, size_t* pOutputSize, void* pPlain, size_t* pPlainSize)
{
	size_t iCapacity = *pOutputSize;
	size_t iPlainCapacity = *pPlainSize;
	xtlsresult Result = XTLS_OK;

	*pOutputSize = *pPlainSize = 0;
	if ( (iInputSize != 0) &&
		(xrtTlsSessionFeed(pSession, pInput, iInputSize) != XTLS_OK) ) {
		return -1;
	}
	for ( size_t i = 0; i < 16u; i++ ) {
		Result = iServer ? xrtTlsServerDrive(pSession) : xrtTlsClientDrive(pSession);
		if ( Result != XTLS_OK ) {
			break;
		}
	}
	if ( Result == XTLS_ERROR ) {
		return -1;
	}
	while ( (xrtTlsSessionSendSize(pSession) != 0) && (*pOutputSize < iCapacity) ) {
		xnetspan Span;
		size_t iChunk;

		if ( !xrtTlsSessionSendFront(pSession, &Span) ) {
			return -1;
		}
		iChunk = Span.Size < iCapacity - *pOutputSize ? Span.Size : iCapacity - *pOutputSize;
		memcpy((uint8*)pOutput + *pOutputSize, Span.Data, iChunk);
		if ( !xrtTlsSessionSendConsume(pSession, iChunk) ) {
			return -1;
		}
		*pOutputSize += iChunk;
	}
	if ( xrtTlsSessionPlainSize(pSession) != 0 ) {
		if ( xrtTlsSessionRead(pSession, pPlain, iPlainCapacity, pPlainSize) != XTLS_OK ) {
			return -1;
		}
	}
	if ( xrtTlsSessionState(pSession) == XTLS_STATE_CLOSED ) {
		return 2;
	}
	return xrtTlsSessionState(pSession) == XTLS_STATE_READY ? 1 : 0;
}

bool xrtTestTlsAlpn(xtlssession* pSession, uint16 iCipher)
{
	xbytesview Protocol;

	return (xrtTlsSessionCipher(pSession) == (xtlscipher)iCipher) &&
		xrtTlsSessionProtocol(pSession, &Protocol) &&
		(Protocol.Size == 11u) && (memcmp(Protocol.Data, "xrt-interop", 11u) == 0);
}
