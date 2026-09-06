/*
 * 范例：tls/handshake_extra —— 自定义身份/验证器与握手后自省
 * ----------------------------------------------------------------
 * 演示 API：
 *   【身份】      xrtTlsIdentityCreate（外部签名器形态）
 *                 xrtTlsIdentityRetain / CertificateCount /
 *                 Certificate / PublicKey / CanSign
 *   【验证器】    xrtTlsVerifierRetain / VerifierVerify
 *                 xrtTlsPeerVerify（默认路径 + 信任库）
 *                 xrtTls12ServerKeyExchangeVerify /
 *                 xrtTls13CertificateVerifySignature（密码学验签）
 *   【客户端】    xrtTlsClientCertificateCount / Certificate /
 *                 KeyUpdate
 *   【服务端】    xrtTlsServerName / Cookie / Resumed / KeyUpdate /
 *                 Ticket / TicketNew（TicketNew 在 resume_tour 演示）
 * 模块宏：XRT_MODULE_TLS（+ CLIENT/SERVER/IDENTITY/VERIFIER）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -I examples/tls -include xrt.h
 *       impl.c examples/tls/handshake_extra/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   extra: identity create/retain/certs/pubkey/cansign ok
 *   extra: handshake with custom signer identity ok
 *   extra: client/server introspection + keyupdate ok
 *   extra: verifier retain/verify + peer verify via store ok
 *   extra: tls12 ske + tls13 certificateverify signatures ok
 *
 * 自定义签名器用嵌入 P-256 私钥真签——身份直接驱动
 *   完整 TLS 1.3 握手，证明外部签名器契约闭环。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>
#include "../x509/fixture.h"

/* 信任库与默认路径验证结果（验证器回调内填充）。 */
static xx509store* g_pPeerStore;
static bool g_bPeerVerified;

/* 嵌入私钥是 SEC1 DER（30 77 02 01 01 04 20 <32 字节标量>）——
 * xrtEcdsaP256 族收裸 32 字节标量，偏移 7 起即私钥。 */
#define EXAMPLE_P256_SCALAR (exampleLeafKeyDer + 7u)

/* 外部签名器上下文：无状态，密钥来自嵌入常量。 */
static bool exampleSupports(ptr pContext, xtlsversion Version,
	xtlssignature Signature)
{
	(void)pContext;
	return ((Version == XTLS_VERSION_12) ||
			(Version == XTLS_VERSION_13)) &&
		(Signature == XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256);
}

static bool exampleSign(ptr pContext, xtlsversion Version,
	xtlssignature Signature, xbytesview Message, void* pOutput,
	size_t iCapacity, size_t* pSize)
{
	uint8 arrDigest[XRT_SHA256_SIZE];

	(void)pContext;
	(void)Version;
	uint8 arrRaw[64];

	/* 探测调用（pOutput 为空）：报 DER 上界并返回成功。 */
	if ( pOutput == NULL ) {
		*pSize = 72u;
		return true;
	}
	if ( (Signature != XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256) ||
		!xrtSha256(Message.Data, Message.Size, arrDigest) ||
		!xrtEcdsaP256Sign(XCRYPTO_HASH_SHA256, arrDigest,
			EXAMPLE_P256_SCALAR, arrRaw) ) {
		return false;
	}
	/* TLS 线格式是 DER——裸 r||s 须编码为 SEQUENCE。 */
	return xrtEcdsaDerEncode(arrRaw, 32u, pOutput, iCapacity, pSize);
}

/* 验证器回调：接受并捕获对端。 */
static xtlsverifydecision exampleAccept(
	const xtlspeer* pPeer,
	ptr pContext
)
{
	(void)pContext;
	/* 对端指针仅回调期间有效——默认路径验证在回调内完成。 */
	if ( (pPeer != NULL) && (pPeer->CertificateCount != 0u) &&
		xrtTlsPeerVerify(pPeer, g_pPeerStore, NULL, NULL) ) {
		g_bPeerVerified = true;
		return XTLS_VERIFY_ACCEPT;
	}
	return XTLS_VERIFY_REJECT;
}

/* 按角色驱动。 */
static xtlsresult exampleDrive(xtlssession* pSession)
{
	return (xrtTlsSessionRole(pSession) == XTLS_SERVER) ?
		xrtTlsServerDrive(pSession) :
		xrtTlsClientDrive(pSession);
}

/* 密文搬运：喂入 → 驱动对端 → 消费源（借用形态的存活契约）。 */
static bool exampleMove(xtlssession* pSource, xtlssession* pTarget)
{
	xnetspan Span;

	while ( xrtTlsSessionSendSize(pSource) != 0u ) {
		xtlsresult Result;

		if ( !xrtTlsSessionSendFront(pSource, &Span) ||
			(Span.Size == 0u) ) {
			return false;
		}
		Result = xrtTlsSessionFeedBorrow(pTarget, Span.Data,
			Span.Size);
		if ( Result != XTLS_OK ) {
			return false;
		}
		Result = exampleDrive(pTarget);
		if ( (Result != XTLS_OK) && (Result != XTLS_AGAIN) ) {
			return false;
		}
		if ( !xrtTlsSessionSendConsume(pSource, Span.Size) ) {
			return false;
		}
	}
	return true;
}

static bool exampleHandshake(xtlssession* pClient, xtlssession* pServer)
{
	for ( size_t i = 0; i < 4096u; i++ ) {
		xtlsresult C = xrtTlsClientDrive(pClient);
		xtlsresult S = xrtTlsServerDrive(pServer);

		if ( ((C != XTLS_OK) && (C != XTLS_AGAIN)) ||
			((S != XTLS_OK) && (S != XTLS_AGAIN)) ||
			!exampleMove(pClient, pServer) ||
			!exampleMove(pServer, pClient) ) {
			return false;
		}
		if ( (xrtTlsSessionState(pClient) == XTLS_STATE_READY) &&
			(xrtTlsSessionState(pServer) == XTLS_STATE_READY) ) {
			return true;
		}
	}
	return false;
}

int main(void)
{
	/* 真实两证书链：叶证书（SAN localhost）+ CA。 */
	xbytesview arrChain[2] = {
		{ exampleLeafDer, sizeof(exampleLeafDer) },
		{ exampleCaDer, sizeof(exampleCaDer) }
	};
	xtlsidentityconfig IdentityConfig;
	xtlsidentity* pIdentity = NULL;
	xtlsidentity* pRetained = NULL;
	xtlsverifierconfig VerifierConfig;
	xtlsverifier* pVerifier = NULL;
	xtlsclientconfig ClientConfig;
	xtlsserverconfig ServerConfig;
	xtlssession* pClient = NULL;
	xtlssession* pServer = NULL;
	xx509cert Leaf;
	xx509pubkey LeafKey;
	xbytesview Stored;
	int iResult = 1;

	/* ---- 身份：外部签名器 + 证书链深复制快照。 ---- */
	memset(&IdentityConfig, 0, sizeof(IdentityConfig));
	IdentityConfig.Certificates = arrChain;
	IdentityConfig.CertificateCount = 2u;
	IdentityConfig.Type = XTLS_IDENTITY_ECDSA_P256;
	IdentityConfig.Supports = exampleSupports;
	IdentityConfig.Sign = exampleSign;
	pIdentity = xrtTlsIdentityCreate(&IdentityConfig);
	if ( (pIdentity == NULL) ||
		((pRetained = xrtTlsIdentityRetain(pIdentity)) == NULL) ||
		(xrtTlsIdentityCertificateCount(pIdentity) != 2u) ||
		!xrtTlsIdentityCertificate(pIdentity, 0u, &Stored) ||
		(Stored.Size != sizeof(exampleLeafDer)) ||
		(memcmp(Stored.Data, exampleLeafDer,
			sizeof(exampleLeafDer)) != 0) ||
		!xrtTlsIdentityPublicKey(pIdentity, &LeafKey) ||
		!xrtTlsIdentityCanSign(pIdentity, XTLS_VERSION_13,
			XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256) ||
		xrtTlsIdentityCanSign(pIdentity, XTLS_VERSION_13,
			XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256) ) {
		goto Cleanup;
	}
	printf("extra: identity create/retain/certs/pubkey/cansign ok\n");

	/* ---- 用自定义签名器身份完成真实握手。
	 * 自签嵌入证书先入锚，验证器回调内走默认路径。 ---- */
	g_pPeerStore = xrtX509StoreCreate();
	if ( (g_pPeerStore == NULL) ||
		(xrtX509StoreAdd(g_pPeerStore, exampleCaDer,
			sizeof(exampleCaDer)) != X509_VALUE) ) {
		goto Cleanup;
	}
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = exampleAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.ServerName = XRT_STR_LITERAL("localhost");
	ClientConfig.VerifyName = XRT_STR_LITERAL("localhost");
	ClientConfig.Verifier = pVerifier;
	xrtTlsServerConfigInit(&ServerConfig);
	ServerConfig.Identity = pIdentity;
	pClient = xrtTlsClientCreate(&ClientConfig, NULL);
	pServer = xrtTlsServerCreate(&ServerConfig, NULL);
	if ( (pClient == NULL) || (pServer == NULL) ||
		!exampleHandshake(pClient, pServer) ) {
		goto Cleanup;
	}
	printf("extra: handshake with custom signer identity ok\n");

	/* ---- 客户端/服务端自省 + 双向 KeyUpdate。 ---- */
	{
		const xx509cert* pPeerCert;
		xbytesview ServerName;
		uint64 Cookie;

		if ( (xrtTlsClientCertificateCount(pClient) != 2u) ||
			((pPeerCert = xrtTlsClientCertificate(pClient, 0u)) ==
				NULL) ||
			!xrtX509Parse(exampleLeafDer,
				sizeof(exampleLeafDer), &Leaf) ||
			(pPeerCert->Raw.Size != Leaf.Raw.Size) ||
			!xrtTlsServerName(pServer, &ServerName) ||
			(ServerName.Size != 9u) ||
			(memcmp(ServerName.Data, "localhost", 9u) != 0) ) {
			goto Cleanup;
		}
		/* 未配置选择器时 Cookie 保持调用方初值零。 */
		Cookie = 0u;
		(void)xrtTlsServerCookie(pServer, &Cookie);
		if ( (Cookie != 0u) || xrtTlsServerResumed(pServer) ) {
			goto Cleanup;
		}
	}
	if ( (xrtTlsServerKeyUpdate(pServer,
			XTLS_KEY_UPDATE_NOT_REQUESTED) != XTLS_OK) ||
		!exampleMove(pServer, pClient) ||
		(xrtTlsClientKeyUpdate(pClient,
			XTLS_KEY_UPDATE_REQUESTED) != XTLS_OK) ||
		!exampleMove(pClient, pServer) ) {
		goto Cleanup;
	}
	printf("extra: client/server introspection + keyupdate ok\n");

	/* ---- 验证器引用 + 决策调用 + 默认路径 PeerVerify。 ---- */
	{
		xtlsverifier* pRetainedVerifier = xrtTlsVerifierRetain(
			pVerifier);

		if ( (pRetainedVerifier == NULL) ||
			!g_bPeerVerified ||
			!xrtTlsVerifierVerify(pVerifier, XTLS_SERVER,
				XRT_STR_LITERAL("localhost"), &Leaf, 1u) ) {
			xrtTlsVerifierRelease(pRetainedVerifier);
			goto Cleanup;
		}
		xrtTlsVerifierRelease(pRetainedVerifier);
	}
	printf("extra: verifier retain/verify + peer verify via store ok\n");

	/* ---- TLS1.2 SKE 与 TLS1.3 CertificateVerify 验签。 ---- */
	if ( !xrtX509PublicKey(&Leaf, &LeafKey) ) {
		goto Cleanup;
	}
	{
		/* 1.2 SKE：内容 = ClientRandom||ServerRandom||Params，
		 * 签名同样 DER 编码（线格式）。 */
		static const uint8 arrClientRandom[32] = { 1u };
		static const uint8 arrServerRandom[32] = { 2u };
		static const uint8 arrParams[8] = { 3u };
		uint8 arrContent[72];
		uint8 arrDigest[XRT_SHA256_SIZE];
		uint8 arrRaw[64];
		uint8 arrDer[72];
		size_t iDerSize = 0;

		memcpy(arrContent, arrClientRandom, 32u);
		memcpy(arrContent + 32u, arrServerRandom, 32u);
		memcpy(arrContent + 64u, arrParams, 8u);
		if ( !xrtSha256(arrContent, 72u, arrDigest) ||
			!xrtEcdsaP256Sign(XCRYPTO_HASH_SHA256, arrDigest,
				EXAMPLE_P256_SCALAR, arrRaw) ||
			!xrtEcdsaDerEncode(arrRaw, 32u, arrDer,
				sizeof(arrDer), &iDerSize) ||
			!xrtTls12ServerKeyExchangeVerify(
				XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256,
				(xbytesview) { arrClientRandom, 32u },
				(xbytesview) { arrServerRandom, 32u },
				(xbytesview) { arrParams, 8u },
				(xbytesview) { arrDer, iDerSize },
				&LeafKey) ) {
			goto Cleanup;
		}
		arrDer[2] ^= 1u;
		if ( xrtTls12ServerKeyExchangeVerify(
				XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256,
				(xbytesview) { arrClientRandom, 32u },
				(xbytesview) { arrServerRandom, 32u },
				(xbytesview) { arrParams, 8u },
				(xbytesview) { arrDer, iDerSize },
				&LeafKey) ) {
			goto Cleanup;
		}
	}
	{
		/* 1.3 CertificateVerify：用公开的 ContentEncode 构造
		 * 待签内容，签名 DER 编码后按线格式验证。 */
		uint8 arrContent[130];
		uint8 arrHash[32];
		uint8 arrDigest[XRT_SHA256_SIZE];
		uint8 arrRaw[64];
		uint8 arrDer[72];
		size_t iDerSize = 0;

		memset(arrHash, 0xAB, sizeof(arrHash));
		if ( !xrtTls13CertificateVerifyContentEncode(XTLS_SERVER,
				(xbytesview) { arrHash, 32u }, arrContent,
				sizeof(arrContent)) ||
			!xrtSha256(arrContent, 130u, arrDigest) ||
			!xrtEcdsaP256Sign(XCRYPTO_HASH_SHA256, arrDigest,
				EXAMPLE_P256_SCALAR, arrRaw) ||
			!xrtEcdsaDerEncode(arrRaw, 32u, arrDer,
				sizeof(arrDer), &iDerSize) ||
			!xrtTls13CertificateVerifySignature(XTLS_SERVER,
				XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256,
				(xbytesview) { arrHash, 32u },
				(xbytesview) { arrDer, iDerSize },
				&LeafKey) ) {
			goto Cleanup;
		}
		arrDer[2] ^= 1u;
		if ( xrtTls13CertificateVerifySignature(XTLS_SERVER,
				XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256,
				(xbytesview) { arrHash, 32u },
				(xbytesview) { arrDer, iDerSize },
				&LeafKey) ) {
			goto Cleanup;
		}
	}
	printf("extra: tls12 ske + tls13 certificateverify signatures ok\n");
	iResult = 0;

Cleanup:
	xrtX509StoreFree(g_pPeerStore);
	xrtTlsSessionDestroy(pServer);
	xrtTlsSessionDestroy(pClient);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pRetained);
	xrtTlsIdentityRelease(pIdentity);
	return iResult;
}
