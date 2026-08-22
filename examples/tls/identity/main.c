#include "../common.h"



/* 选择每种内置身份在 TLS 1.3 中的常用签名方案。 */
static xtlssignature exampleSignature(const xtlsidentity* pIdentity)
{
	switch ( xrtTlsIdentityType(pIdentity) ) {
		case XTLS_IDENTITY_RSA:
			return XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256;

		case XTLS_IDENTITY_RSA_PSS:
			return XTLS_SIGNATURE_RSA_PSS_PSS_SHA256;

		case XTLS_IDENTITY_ECDSA_P256:
			return XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256;

		case XTLS_IDENTITY_ECDSA_P384:
			return XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384;

		case XTLS_IDENTITY_ED25519:
			return XTLS_SIGNATURE_ED25519;

		default:
			return (xtlssignature)0;
	}
}



/* 从 DER 证书与私钥创建身份，并演示精确长度查询和实际签名。 */
int main(int argc, char** argv)
{
	static const uint8 Message[] = "xrt TLS identity example";
	uint8* pCertificateData = NULL;
	uint8* pPrivateData = NULL;
	uint8* pSignature = NULL;
	size_t iCertificateSize = 0;
	size_t iPrivateSize = 0;
	size_t iSignatureSize = 0;
	xbytesview Certificate;
	xtlsidentity* pIdentity = NULL;
	xtlssignature Signature;
	int iResult = 1;

	if ( argc != 4 ) {
		printf("usage: identity <rsa|p256|p384|ed25519> <certificate.der> <private.der>\n");
		return 0;
	}
	if ( !exampleTlsReadFile(
		argv[2], &pCertificateData, &iCertificateSize
	) || !exampleTlsReadFile(
		argv[3], &pPrivateData, &iPrivateSize
	) ) {
		fprintf(stderr, "failed to read DER input\n");
		goto Cleanup;
	}
	Certificate = (xbytesview) { pCertificateData, iCertificateSize };
	pIdentity = exampleTlsIdentity(
		argv[1], &Certificate, 1u,
		(xbytesview) { pPrivateData, iPrivateSize }
	);
	if ( pIdentity == NULL ) {
		fprintf(stderr, "failed to create TLS identity\n");
		goto Cleanup;
	}
	Signature = exampleSignature(pIdentity);
	if ( (Signature == 0) || !xrtTlsIdentitySign(
		pIdentity, XTLS_VERSION_13, Signature,
		(xbytesview) { Message, sizeof(Message) - 1u },
		NULL, 0, &iSignatureSize
	) ) {
		fprintf(stderr, "identity cannot sign with the selected TLS scheme\n");
		goto Cleanup;
	}
	pSignature = (uint8*)malloc(iSignatureSize);
	if ( (pSignature == NULL) || !xrtTlsIdentitySign(
		pIdentity, XTLS_VERSION_13, Signature,
		(xbytesview) { Message, sizeof(Message) - 1u },
		pSignature, iSignatureSize, &iSignatureSize
	) ) {
		fprintf(stderr, "TLS identity signing failed\n");
		goto Cleanup;
	}
	printf(
		"identity=%d certificates=%zu signature=%zu bytes\n",
		(int)xrtTlsIdentityType(pIdentity),
		xrtTlsIdentityCertificateCount(pIdentity), iSignatureSize
	);
	iResult = 0;

Cleanup:
	free(pSignature);
	xrtTlsIdentityRelease(pIdentity);
	free(pPrivateData);
	free(pCertificateData);
	return iResult;
}
