#include "../test.h"
#include "../fixtures/tls_identity_legacy.h"
#include "../fixtures/x509_path_legacy.h"



typedef struct test_tls_verify_time {
	xtime Value;
} test_tls_verify_time;



/* 返回证书夹具有效期内的确定时间。 */
static xtime testTlsVerifyRsaTime(ptr pContext)
{
	return ((test_tls_verify_time*)pContext)->Value;
}



/* 构建 RFC 8446 CertificateVerify 待签内容。 */
static size_t testTlsVerifyRsaContent(
	xbytesview TranscriptHash,
	void* pOutput,
	size_t iCapacity
)
{
	static const char Context[] = "TLS 1.3, server CertificateVerify";
	size_t iRequired = 65u + sizeof(Context) - 1u + TranscriptHash.Size;
	bytes pWrite = (bytes)pOutput;

	testRequire(iCapacity >= iRequired,
		"TLS CertificateVerify test content buffer is too small");
	memset(pWrite, 0x20, 64u);
	memcpy(pWrite + 64u, Context, sizeof(Context) - 1u);
	pWrite[64u + sizeof(Context) - 1u] = 0;
	memcpy(
		pWrite + 64u + sizeof(Context),
		TranscriptHash.Data, TranscriptHash.Size
	);
	return iRequired;
}



/* 历史证书和私钥必须覆盖信任快照、主机名与真实 PSS 握手验签。 */
int main(void)
{
	uint8 PrivateKey[2048];
	uint8 TranscriptHash[32];
	uint8 Content[160];
	uint8 Signature[512];
	xbytesview CertificateView = {
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT)
	};
	xx509cert Certificate;
	xx509cert Chain[2];
	xx509pubkey PublicKey;
	xx509store* pStore;
	xtlsverifierconfig Config;
	xtlsverifier* pVerifier;
	xtlsidentity* pIdentity;
	test_tls_verify_time Time;
	size_t iPrivateKeySize;
	size_t iContentSize;
	size_t iSignatureSize = 0;

	testRequire(xrtX509Parse(
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT), &Certificate
	) && xrtX509PublicKey(&Certificate, &PublicKey),
		"TLS RSA verifier certificate fixture failed");
	testRequire(xrtX509Parse(
		X509_PATH_LEAF, sizeof(X509_PATH_LEAF), &Chain[0]
	) && xrtX509Parse(
		X509_PATH_INTERMEDIATE, sizeof(X509_PATH_INTERMEDIATE), &Chain[1]
	), "TLS RSA verifier path fixture failed");
	pStore = xrtX509StoreCreate();
	testRequire((pStore != NULL) &&
		(xrtX509StoreAdd(
			pStore, X509_PATH_ROOT, sizeof(X509_PATH_ROOT)
		) == X509_VALUE), "TLS verifier trust store setup failed");
	Time.Value = Chain[0].NotBefore;
	xrtTlsVerifierConfigInit(&Config);
	Config.Store = pStore;
	Config.Time = testTlsVerifyRsaTime;
	Config.Context = &Time;
	pVerifier = xrtTlsVerifierCreate(&Config);
	testRequire(pVerifier != NULL, "TLS trust snapshot creation failed");
	xrtX509StoreFree(pStore);
	testRequire(xrtTlsVerifierVerify(
		pVerifier, XTLS_SERVER, XRT_STR_LITERAL("sha1-root.test.xrt"),
		Chain, 2u
	), "TLS default trust or host verification failed");
	testRequire(!xrtTlsVerifierVerify(
		pVerifier, XTLS_SERVER, XRT_STR_LITERAL("example.com"),
		Chain, 2u
	) && (xrtErrorCode(xrtGetError()) == XTLS_ERROR_VERIFY),
		"TLS default verifier accepted the wrong host");
	xrtClearError();

	testRequire(testTlsIdentityLegacyKey(
		PrivateKey, sizeof(PrivateKey), &iPrivateKeySize
	), "TLS RSA verifier private key decoding failed");
	pIdentity = xrtTlsIdentityRsa(
		&CertificateView, 1u,
		(xbytesview) { PrivateKey, iPrivateKeySize }
	);
	xrtSecureZero(PrivateKey, sizeof(PrivateKey));
	testRequire(pIdentity != NULL, "TLS RSA verifier identity creation failed");
	for ( size_t i = 0; i < sizeof(TranscriptHash); i++ ) {
		TranscriptHash[i] = (uint8)i;
	}
	iContentSize = testTlsVerifyRsaContent(
		(xbytesview) { TranscriptHash, sizeof(TranscriptHash) },
		Content, sizeof(Content)
	);
	testRequire(xrtTlsIdentitySign(
		pIdentity, XTLS_VERSION_13,
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
		(xbytesview) { Content, iContentSize },
		NULL, 0, &iSignatureSize
	) && (iSignatureSize <= sizeof(Signature)) && xrtTlsIdentitySign(
		pIdentity, XTLS_VERSION_13,
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
		(xbytesview) { Content, iContentSize },
		Signature, sizeof(Signature), &iSignatureSize
	), "TLS RSA CertificateVerify signing failed");
	testRequire(xrtTls13CertificateVerifySignature(
		XTLS_SERVER, XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
		(xbytesview) { TranscriptHash, sizeof(TranscriptHash) },
		(xbytesview) { Signature, iSignatureSize }, &PublicKey
	), "TLS RSA CertificateVerify signature was rejected");
	Signature[iSignatureSize - 1u] ^= 1u;
	testRequire(!xrtTls13CertificateVerifySignature(
		XTLS_SERVER, XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
		(xbytesview) { TranscriptHash, sizeof(TranscriptHash) },
		(xbytesview) { Signature, iSignatureSize }, &PublicKey
	), "TLS RSA CertificateVerify accepted a damaged signature");
	xrtClearError();

	xrtSecureZero(Signature, sizeof(Signature));
	xrtSecureZero(Content, sizeof(Content));
	xrtTlsIdentityRelease(pIdentity);
	xrtTlsVerifierRelease(pVerifier);
	return 0;
}
