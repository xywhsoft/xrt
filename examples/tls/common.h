#ifndef XRT_EXAMPLE_TLS_COMMON_H
#define XRT_EXAMPLE_TLS_COMMON_H

#include <xrt.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* 读取命令行指定的非空二进制文件，返回的所有权由调用方释放。 */
static inline bool exampleTlsReadFile(
	cstr sPath,
	uint8** ppData,
	size_t* pSize
)
{
	FILE* pFile;
	long iLength;
	uint8* pBuffer;

	*ppData = NULL;
	*pSize = 0;
	pFile = fopen(sPath, "rb");
	if ( pFile == NULL ) {
		return false;
	}
	if ( (fseek(pFile, 0, SEEK_END) != 0) ||
		((iLength = ftell(pFile)) <= 0) ||
		(fseek(pFile, 0, SEEK_SET) != 0) ) {
		fclose(pFile);
		return false;
	}
	pBuffer = (uint8*)malloc((size_t)iLength);
	if ( pBuffer == NULL ) {
		fclose(pFile);
		return false;
	}
	if ( fread(pBuffer, 1u, (size_t)iLength, pFile) !=
		(size_t)iLength ) {
		free(pBuffer);
		fclose(pFile);
		return false;
	}
	fclose(pFile);
	*ppData = pBuffer;
	*pSize = (size_t)iLength;
	return true;
}



/* 按已裁剪进构建的内置后端创建强类型 TLS 身份。 */
static inline xtlsidentity* exampleTlsIdentity(
	cstr sAlgorithm,
	const xbytesview* pCertificates,
	size_t iCertificateCount,
	xbytesview PrivateKey
)
{
	#if defined(XRT_FEATURE_TLS_IDENTITY_RSA)
		if ( strcmp(sAlgorithm, "rsa") == 0 ) {
			return xrtTlsIdentityRsa(
				pCertificates, iCertificateCount, PrivateKey
			);
		}
	#endif
	#if defined(XRT_FEATURE_TLS_IDENTITY_P256)
		if ( strcmp(sAlgorithm, "p256") == 0 ) {
			return xrtTlsIdentityP256(
				pCertificates, iCertificateCount, PrivateKey
			);
		}
	#endif
	#if defined(XRT_FEATURE_TLS_IDENTITY_P384)
		if ( strcmp(sAlgorithm, "p384") == 0 ) {
			return xrtTlsIdentityP384(
				pCertificates, iCertificateCount, PrivateKey
			);
		}
	#endif
	#if defined(XRT_FEATURE_TLS_IDENTITY_ED25519)
		if ( strcmp(sAlgorithm, "ed25519") == 0 ) {
			return xrtTlsIdentityEd25519(
				pCertificates, iCertificateCount, PrivateKey
			);
		}
	#endif
	(void)sAlgorithm;
	(void)pCertificates;
	(void)iCertificateCount;
	(void)PrivateKey;
	return NULL;
}

#endif
