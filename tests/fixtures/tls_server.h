#ifndef XRT_TEST_TLS_SERVER_H
#define XRT_TEST_TLS_SERVER_H

#include "../test.h"
#include "tls_identity_legacy.h"



/* 确定性随机状态只控制测试分片，不参与任何密码操作。 */
typedef struct test_tls_server_rng {
	uint32 Value;
} test_tls_server_rng;



/* 比较两个允许为空的不透明协议视图。 */
static inline bool testTlsServerViewEqual(
	xbytesview Left,
	xbytesview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 生成下一段确定性非零随机分片。 */
static inline uint32 testTlsServerRandom(test_tls_server_rng* pRng)
{
	uint32 iValue = pRng->Value;

	if ( iValue == 0 ) {
		iValue = UINT32_C(0x9E3779B9);
	}
	iValue ^= (iValue << 13u);
	iValue ^= (iValue >> 17u);
	iValue ^= (iValue << 5u);
	pRng->Value = iValue;
	return iValue;
}



/* 测试验证器只接管信任决策，握手签名仍由客户端严格验证。 */
static inline xtlsverifydecision testTlsServerAccept(
	const xtlspeer* pPeer,
	ptr pContext
)
{
	(void)pContext;
	return ((pPeer != NULL) && (pPeer->Role == XTLS_SERVER) &&
		(pPeer->CertificateCount != 0) &&
		(((uintptr_t)pPeer->Certificates %
			TEST_ALIGNOF(xx509cert)) == 0)) ?
		XTLS_VERIFY_ACCEPT : XTLS_VERIFY_REJECT;
}



/* 创建只启用测试所需 TLS 1.3 能力的可定制共享上下文。 */
static inline xtlscontext* testTlsServerContextWithLimits(
	const xtlslimits* pLimits
)
{
	static const xtlsversion Versions[] = { XTLS_VERSION_13 };
	static const xtlscipher Ciphers[] = {
		XTLS_AES_128_GCM_SHA256
	};
	static const uint16 Groups[] = { XTLS_GROUP_X25519 };
	static const xtlssignature Signatures[] = {
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256
	};
	xtlspolicy Policy;
	xtlscontextconfig Config;

	xrtTlsPolicyInit(&Policy);
	Policy.Versions = Versions;
	Policy.VersionCount = sizeof(Versions) / sizeof(Versions[0]);
	Policy.Ciphers = Ciphers;
	Policy.CipherCount = sizeof(Ciphers) / sizeof(Ciphers[0]);
	Policy.Groups = Groups;
	Policy.GroupCount = sizeof(Groups) / sizeof(Groups[0]);
	Policy.Signatures = Signatures;
	Policy.SignatureCount = sizeof(Signatures) / sizeof(Signatures[0]);
	xrtTlsContextConfigInit(&Config);
	Config.Policy = &Policy;
	if ( pLimits != NULL ) {
		Config.Limits = *pLimits;
	}
	Config.Limits.RecordBudget = 4u;
	Config.Limits.HandshakeBudget = 4u;
	return xrtTlsContextCreate(&Config);
}



/* 创建使用默认队列上限的测试 TLS 1.3 上下文。 */
static inline xtlscontext* testTlsServerContext(void)
{
	return testTlsServerContextWithLimits(NULL);
}



/* 从历史真实证书和匹配私钥创建服务端身份。 */
static inline xtlsidentity* testTlsServerIdentity(void)
{
	uint8 PrivateKey[2048];
	xbytesview Certificate = {
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT)
	};
	size_t iPrivateKeySize = 0;
	xtlsidentity* pIdentity;

	testRequire(testTlsIdentityLegacyKey(
		PrivateKey, sizeof(PrivateKey), &iPrivateKeySize
	), "TLS server private key decoding failed");
	pIdentity = xrtTlsIdentityRsa(
		&Certificate, 1u,
		(xbytesview) { PrivateKey, iPrivateKeySize }
	);
	xrtSecureZero(PrivateKey, sizeof(PrivateKey));
	return pIdentity;
}



/* 前置声明供 READY 会话夹具复用下方的双端驱动器。 */
static inline bool testTlsServerHandshake(
	xtlssession* pClient,
	xtlssession* pServer,
	test_tls_server_rng* pRng
);



/* 创建一对使用 http/1.1 并已经完成真实证书握手的会话。 */
static inline bool testTlsServerReady(
	xtlscontext* pContext,
	xtlsidentity* pIdentity,
	xtlsverifier* pVerifier,
	test_tls_server_rng* pRng,
	xtlssession** ppClient,
	xtlssession** ppServer
)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("http/1.1")
	};
	xtlsclientconfig ClientConfig;
	xtlsserverconfig ServerConfig;

	*ppClient = NULL;
	*ppServer = NULL;
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.Context = pContext;
	ClientConfig.ServerName = XRT_STR_LITERAL("example.com");
	ClientConfig.Protocols = Protocols;
	ClientConfig.ProtocolCount = sizeof(Protocols) / sizeof(Protocols[0]);
	ClientConfig.Verifier = pVerifier;
	xrtTlsServerConfigInit(&ServerConfig);
	ServerConfig.Context = pContext;
	ServerConfig.Identity = pIdentity;
	ServerConfig.Protocols = Protocols;
	ServerConfig.ProtocolCount = sizeof(Protocols) / sizeof(Protocols[0]);
	ServerConfig.RequireProtocol = true;
	*ppClient = xrtTlsClientCreate(&ClientConfig, NULL);
	*ppServer = xrtTlsServerCreate(&ServerConfig, NULL);
	if ( (*ppClient == NULL) || (*ppServer == NULL) ||
		!testTlsServerHandshake(*ppClient, *ppServer, pRng) ) {
		xrtTlsSessionDestroy(*ppServer);
		xrtTlsSessionDestroy(*ppClient);
		*ppServer = NULL;
		*ppClient = NULL;
		return false;
	}
	return true;
}



/* 从一个会话移动一段随机大小密文到另一个会话。 */
static inline bool testTlsServerMove(
	xtlssession* pSource,
	xtlssession* pTarget,
	test_tls_server_rng* pRng,
	size_t iMaximum,
	bool* pProgress
)
{
	xnetspan Span;
	size_t iChunk;

	if ( xrtTlsSessionSendSize(pSource) == 0 ) {
		return true;
	}
	if ( !xrtTlsSessionSendFront(pSource, &Span) || (Span.Size == 0) ) {
		return false;
	}
	iChunk = 1u +
		(testTlsServerRandom(pRng) % (uint32)iMaximum);
	if ( iChunk > Span.Size ) {
		iChunk = Span.Size;
	}
	if ( (xrtTlsSessionFeed(
		pTarget, Span.Data, iChunk
	) != XTLS_OK) || !xrtTlsSessionSendConsume(
		pSource, iChunk
	) ) {
		return false;
	}
	*pProgress = true;
	return true;
}



/* 在任意小分片下驱动两端直到同时到达 READY。 */
static inline bool testTlsServerHandshake(
	xtlssession* pClient,
	xtlssession* pServer,
	test_tls_server_rng* pRng
)
{
	for ( size_t i = 0; i < 8192u; i++ ) {
		xtlsresult ClientResult = xrtTlsClientDrive(pClient);
		xtlsresult ServerResult = xrtTlsServerDrive(pServer);
		bool bProgress = false;

		if ( ((ClientResult != XTLS_OK) &&
			 (ClientResult != XTLS_AGAIN)) ||
			((ServerResult != XTLS_OK) &&
			 (ServerResult != XTLS_AGAIN)) ) {
			const xerror* pError = xrtGetError();

			fprintf(
				stderr,
				"[TLS] step=%llu client=%d server=%d states=%d/%d "
				"op=%s error=%s\n",
				(unsigned long long)i,
				(int)ClientResult, (int)ServerResult,
				(int)xrtTlsSessionState(pClient),
				(int)xrtTlsSessionState(pServer),
				pError != NULL ? xrtErrorOperation(pError) : "none",
				pError != NULL ? xrtErrorMessage(pError) : "none"
			);
			return false;
		}
		if ( !testTlsServerMove(
			pClient, pServer, pRng, 37u, &bProgress
		) || !testTlsServerMove(
			pServer, pClient, pRng, 53u, &bProgress
		) ) {
			return false;
		}
		if ( (xrtTlsSessionState(pClient) == XTLS_STATE_READY) &&
			(xrtTlsSessionState(pServer) == XTLS_STATE_READY) ) {
			return true;
		}
		if ( !bProgress &&
			(xrtTlsSessionFeedSize(pClient) == 0) &&
			(xrtTlsSessionFeedSize(pServer) == 0) ) {
			fprintf(
				stderr,
				"[TLS] stalled step=%llu states=%d/%d send=%llu/%llu\n",
				(unsigned long long)i,
				(int)xrtTlsSessionState(pClient),
				(int)xrtTlsSessionState(pServer),
				(unsigned long long)xrtTlsSessionSendSize(pClient),
				(unsigned long long)xrtTlsSessionSendSize(pServer)
			);
			return false;
		}
	}
	return false;
}



/* 发送并在随机密文分片下验证一段应用数据。 */
static inline bool testTlsServerTransfer(
	xtlssession* pSource,
	xtlssession* pTarget,
	bool bSourceClient,
	const void* pData,
	size_t iSize,
	test_tls_server_rng* pRng
)
{
	uint8 Received[4096];
	size_t iWritten = 0;
	size_t iReceived = 0;

	if ( (iSize > sizeof(Received)) ||
		(xrtTlsSessionWrite(
			pSource, pData, iSize, &iWritten
		) != XTLS_OK) || (iWritten != iSize) ) {
		return false;
	}
	while ( iReceived < iSize ) {
		bool bProgress = false;
		size_t iRead = 0;
		xtlsresult Result;

		if ( !testTlsServerMove(
			pSource, pTarget, pRng, 29u, &bProgress
		) ) {
			return false;
		}
		Result = bSourceClient ?
			xrtTlsServerDrive(pTarget) : xrtTlsClientDrive(pTarget);
		if ( (Result != XTLS_OK) && (Result != XTLS_AGAIN) ) {
			return false;
		}
		Result = xrtTlsSessionRead(
			pTarget, Received + iReceived,
			iSize - iReceived, &iRead
		);
		if ( Result == XTLS_OK ) {
			iReceived += iRead;
			bProgress = bProgress || (iRead != 0);
		} else if ( Result != XTLS_AGAIN ) {
			return false;
		}
		if ( !bProgress ) {
			return false;
		}
	}
	return memcmp(Received, pData, iSize) == 0;
}



/* 驱动后握手消息及其应答，直到双方密文输入输出全部排空。 */
static inline bool testTlsServerPostHandshake(
	xtlssession* pClient,
	xtlssession* pServer,
	test_tls_server_rng* pRng
)
{
	for ( size_t i = 0; i < 8192u; i++ ) {
		xtlsresult ClientResult = xrtTlsClientDrive(pClient);
		xtlsresult ServerResult = xrtTlsServerDrive(pServer);
		bool bProgress = (ClientResult == XTLS_OK) ||
			(ServerResult == XTLS_OK);

		if ( ((ClientResult != XTLS_OK) &&
			 (ClientResult != XTLS_AGAIN)) ||
			((ServerResult != XTLS_OK) &&
			 (ServerResult != XTLS_AGAIN)) ) {
			return false;
		}
		if ( !testTlsServerMove(
			pClient, pServer, pRng, 17u, &bProgress
		) || !testTlsServerMove(
			pServer, pClient, pRng, 19u, &bProgress
		) ) {
			return false;
		}
		if ( (xrtTlsSessionSendSize(pClient) == 0) &&
			(xrtTlsSessionSendSize(pServer) == 0) &&
			(xrtTlsSessionFeedSize(pClient) == 0) &&
			(xrtTlsSessionFeedSize(pServer) == 0) ) {
			return true;
		}
		if ( !bProgress ) {
			return false;
		}
	}
	return false;
}



/* 完成双向认证关闭，并要求双方都只在响应密文排空后进入 CLOSED。 */
static inline bool testTlsServerClose(
	xtlssession* pClient,
	xtlssession* pServer,
	test_tls_server_rng* pRng
)
{
	if ( xrtTlsSessionClose(pClient) != XTLS_OK ) {
		return false;
	}
	for ( size_t i = 0; i < 8192u; i++ ) {
		xtlsresult ClientResult = xrtTlsClientDrive(pClient);
		xtlsresult ServerResult = xrtTlsServerDrive(pServer);
		bool bProgress = (ClientResult == XTLS_OK) ||
			(ServerResult == XTLS_OK);

		if ( ((ClientResult != XTLS_OK) &&
			 (ClientResult != XTLS_AGAIN) &&
			 (ClientResult != XTLS_CLOSED)) ||
			((ServerResult != XTLS_OK) &&
			 (ServerResult != XTLS_AGAIN) &&
			 (ServerResult != XTLS_CLOSED)) ) {
			return false;
		}
		if ( !testTlsServerMove(
			pClient, pServer, pRng, 7u, &bProgress
		) || !testTlsServerMove(
			pServer, pClient, pRng, 11u, &bProgress
		) ) {
			return false;
		}
		if ( (xrtTlsSessionState(pClient) == XTLS_STATE_CLOSED) &&
			(xrtTlsSessionState(pServer) == XTLS_STATE_CLOSED) &&
			(xrtTlsSessionSendSize(pClient) == 0) &&
			(xrtTlsSessionSendSize(pServer) == 0) ) {
			return true;
		}
		if ( !bProgress ) {
			return false;
		}
	}
	return false;
}

#endif
