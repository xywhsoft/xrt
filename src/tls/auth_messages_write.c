#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_AUTH_MESSAGES_WRITE)

/* 设置认证消息编码错误并返回零。 */
static size_t __xrtTlsAuthMessageWriteError(
	xtlserror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtTlsError(XERR_VALUE, Code, sOperation, sMessage, SIZE_MAX);
	return 0;
}



/* 返回编码证书颁发者名称向量所需长度。 */
XRT_API size_t xrtTlsAuthoritiesSize(
	const xbytesview* pNames,
	size_t iCount
)
{
	size_t iListSize = 0;

	if ( ((iCount != 0) && (pNames == NULL)) ||
		(iCount > SIZE_MAX / sizeof(xbytesview)) ) {
		return __xrtTlsAuthMessageWriteError(
			XTLS_ERROR_ARGUMENT, "size-certificate-authorities",
			"TLS certificate-authority name array is invalid"
		);
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		size_t iEntrySize;

		if ( !__xrtTlsViewValid(pNames[i]) ||
			(pNames[i].Size == 0) || (pNames[i].Size > UINT16_MAX) ) {
			return __xrtTlsAuthMessageWriteError(
				XTLS_ERROR_CERTIFICATE, "size-certificate-authorities",
				"TLS certificate-authority name is empty, invalid or too long"
			);
		}
		iEntrySize = 2u + pNames[i].Size;
		if ( iEntrySize > UINT16_MAX - iListSize ) {
			return __xrtTlsAuthMessageWriteError(
				XTLS_ERROR_CERTIFICATE, "size-certificate-authorities",
				"TLS certificate-authority vector exceeds its 16-bit limit"
			);
		}
		iListSize += iEntrySize;
	}
	return 2u + iListSize;
}



/* 检查证书颁发者数组或名称是否与输出区域重叠。 */
static bool __xrtTlsAuthoritiesOverlap(
	const xbytesview* pNames,
	size_t iCount,
	const void* pOutput,
	size_t iOutputSize
)
{
	xbytesview Array;

	Array.Data = (const uint8*)pNames;
	Array.Size = iCount * sizeof(xbytesview);
	if ( __xrtTlsViewOverlap(pOutput, iOutputSize, Array) ) {
		return true;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( __xrtTlsViewOverlap(
			pOutput, iOutputSize, pNames[i]
		) ) {
			return true;
		}
	}
	return false;
}



/* 失败原子地编码带 16 位总长的证书颁发者名称向量。 */
XRT_API bool xrtTlsAuthoritiesEncode(
	const xbytesview* pNames,
	size_t iCount,
	void* pOutput,
	size_t iOutputSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired = xrtTlsAuthoritiesSize(pNames, iCount);
	size_t iOffset = 2u;

	if ( iRequired == 0 ) {
		return false;
	}
	if ( pOutput == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-certificate-authorities",
			"TLS certificate-authority output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( iOutputSize < iRequired ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_CERTIFICATE, "write-certificate-authorities",
			"TLS certificate-authority output buffer is too small", SIZE_MAX
		);
		return false;
	}
	if ( __xrtTlsAuthoritiesOverlap(
		pNames, iCount, pOutput, iRequired
	) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-certificate-authorities",
			"TLS certificate-authority input overlaps its output", SIZE_MAX
		);
		return false;
	}

	for ( size_t i = 0; i < iCount; i++ ) {
		__xrtTlsWrite16(pWrite + iOffset, (uint16)pNames[i].Size);
		iOffset += 2u;
		memcpy(pWrite + iOffset, pNames[i].Data, pNames[i].Size);
		iOffset += pNames[i].Size;
	}
	__xrtTlsWrite16(pWrite, (uint16)(iRequired - 2u));
	return true;
}



/* 返回编码 TLS 1.2 CertificateRequest 正文所需长度。 */
XRT_API size_t xrtTls12CertificateRequestSize(
	const xtls12certificaterequest* pRequest
)
{
	if ( !__xrtTls12CertificateRequestValid(
		pRequest, "size-tls12-certificate-request", XERR_VALUE
	) ) {
		return 0;
	}
	return 3u + pRequest->CertificateTypes.Size +
		pRequest->Signatures.Data.Size + pRequest->AuthorityData.Size;
}



/* 检查 TLS 1.2 CertificateRequest 字段与输出区域是否重叠。 */
static bool __xrtTls12CertificateRequestOverlap(
	const xtls12certificaterequest* pRequest,
	const void* pOutput,
	size_t iOutputSize
)
{
	return __xrtTlsViewOverlap(
		pOutput, iOutputSize, pRequest->CertificateTypes
	) || __xrtTlsViewOverlap(
		pOutput, iOutputSize, pRequest->Signatures.Data
	) || __xrtTlsViewOverlap(
		pOutput, iOutputSize, pRequest->AuthorityData
	);
}



/* 失败原子地编码 TLS 1.2 CertificateRequest 正文。 */
XRT_API bool xrtTls12CertificateRequestEncode(
	const xtls12certificaterequest* pRequest,
	void* pOutput,
	size_t iOutputSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired = xrtTls12CertificateRequestSize(pRequest);
	size_t iOffset = 0;
	xtls12certificaterequest Request;

	if ( iRequired == 0 ) {
		return false;
	}
	if ( pOutput == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"write-tls12-certificate-request",
			"TLS 1.2 CertificateRequest output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( iOutputSize < iRequired ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_CERTIFICATE,
			"write-tls12-certificate-request",
			"TLS 1.2 CertificateRequest output buffer is too small", SIZE_MAX
		);
		return false;
	}
	Request = *pRequest;
	if ( __xrtTls12CertificateRequestOverlap(
		&Request, pOutput, iRequired
	) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"write-tls12-certificate-request",
			"TLS 1.2 CertificateRequest input overlaps its output", SIZE_MAX
		);
		return false;
	}

	pWrite[iOffset++] = (uint8)Request.CertificateTypes.Size;
	memcpy(
		pWrite + iOffset, Request.CertificateTypes.Data,
		Request.CertificateTypes.Size
	);
	iOffset += Request.CertificateTypes.Size;
	__xrtTlsWrite16(
		pWrite + iOffset, (uint16)Request.Signatures.Data.Size
	);
	iOffset += 2u;
	memcpy(
		pWrite + iOffset, Request.Signatures.Data.Data,
		Request.Signatures.Data.Size
	);
	iOffset += Request.Signatures.Data.Size;
	memcpy(
		pWrite + iOffset, Request.AuthorityData.Data,
		Request.AuthorityData.Size
	);
	return true;
}



/* 验证 TLS 1.3 CertificateRequest 的直接线路字段。 */
static bool __xrtTls13CertificateRequestValid(
	xbytesview RequestContext,
	xbytesview Extensions,
	cstr sOperation,
	xerrkind Kind
)
{
	xtlsids Signatures;
	xtlsids CertificateSignatures;
	xbytesview AuthorityData;

	if ( !__xrtTlsViewValid(RequestContext) ||
		!__xrtTlsViewValid(Extensions) || (sOperation == NULL) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			sOperation != NULL ? sOperation : "validate-tls13-certificate-request",
			"TLS 1.3 CertificateRequest fields are invalid", SIZE_MAX
		);
		return false;
	}
	if ( (RequestContext.Size > UINT8_MAX) ||
		(Extensions.Size > UINT16_MAX) ) {
		__xrtTlsError(
			Kind, XTLS_ERROR_LIMIT, sOperation,
			"TLS 1.3 CertificateRequest field exceeds its wire limit", SIZE_MAX
		);
		return false;
	}
	return __xrtTls13CertificateRequestExtensions(
		Extensions, &Signatures, &CertificateSignatures,
		&AuthorityData, sOperation, Kind
	);
}



/* 返回编码 TLS 1.3 CertificateRequest 正文所需长度。 */
XRT_API size_t xrtTls13CertificateRequestSize(
	xbytesview RequestContext,
	xbytesview Extensions
)
{
	if ( !__xrtTls13CertificateRequestValid(
		RequestContext, Extensions,
		"size-tls13-certificate-request", XERR_VALUE
	) ) {
		return 0;
	}
	return 3u + RequestContext.Size + Extensions.Size;
}



/* 失败原子地编码 TLS 1.3 CertificateRequest 正文。 */
XRT_API bool xrtTls13CertificateRequestEncode(
	xbytesview RequestContext,
	xbytesview Extensions,
	void* pOutput,
	size_t iOutputSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired = xrtTls13CertificateRequestSize(
		RequestContext, Extensions
	);
	size_t iOffset = 0;

	if ( iRequired == 0 ) {
		return false;
	}
	if ( pOutput == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"write-tls13-certificate-request",
			"TLS 1.3 CertificateRequest output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( iOutputSize < iRequired ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_CERTIFICATE,
			"write-tls13-certificate-request",
			"TLS 1.3 CertificateRequest output buffer is too small", SIZE_MAX
		);
		return false;
	}
	if ( __xrtTlsViewOverlap(
		pOutput, iRequired, RequestContext
	) || __xrtTlsViewOverlap(
		pOutput, iRequired, Extensions
	) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"write-tls13-certificate-request",
			"TLS 1.3 CertificateRequest input overlaps its output", SIZE_MAX
		);
		return false;
	}

	pWrite[iOffset++] = (uint8)RequestContext.Size;
	if ( RequestContext.Size != 0 ) {
		memcpy(
			pWrite + iOffset, RequestContext.Data, RequestContext.Size
		);
		iOffset += RequestContext.Size;
	}
	__xrtTlsWrite16(pWrite + iOffset, (uint16)Extensions.Size);
	iOffset += 2u;
	memcpy(pWrite + iOffset, Extensions.Data, Extensions.Size);
	return true;
}



/* 返回编码 TLS 1.2 ECDHE ServerKeyExchange 正文所需长度。 */
XRT_API size_t xrtTls12ServerKeyExchangeSize(
	uint16 iGroup,
	xbytesview PublicKey,
	const xtlscertificateverify* pVerify
)
{
	if ( !__xrtTls12ServerKeyExchangeValid(
		iGroup, PublicKey, pVerify,
		"size-tls12-server-key-exchange", XERR_VALUE
	) ) {
		return 0;
	}
	return 8u + PublicKey.Size + pVerify->Signature.Size;
}



/* 失败原子地编码 TLS 1.2 ECDHE ServerKeyExchange 正文。 */
XRT_API bool xrtTls12ServerKeyExchangeEncode(
	uint16 iGroup,
	xbytesview PublicKey,
	const xtlscertificateverify* pVerify,
	void* pOutput,
	size_t iOutputSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired = xrtTls12ServerKeyExchangeSize(
		iGroup, PublicKey, pVerify
	);
	size_t iOffset = 0;
	xtlscertificateverify Verify;

	if ( iRequired == 0 ) {
		return false;
	}
	if ( pOutput == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"write-tls12-server-key-exchange",
			"TLS 1.2 ServerKeyExchange output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( iOutputSize < iRequired ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_HANDSHAKE,
			"write-tls12-server-key-exchange",
			"TLS 1.2 ServerKeyExchange output buffer is too small", SIZE_MAX
		);
		return false;
	}
	Verify = *pVerify;
	if ( __xrtTlsViewOverlap(
		pOutput, iRequired, PublicKey
	) || __xrtTlsViewOverlap(
		pOutput, iRequired, Verify.Signature
	) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"write-tls12-server-key-exchange",
			"TLS 1.2 ServerKeyExchange input overlaps its output", SIZE_MAX
		);
		return false;
	}

	pWrite[iOffset++] = 3u;
	__xrtTlsWrite16(pWrite + iOffset, iGroup);
	iOffset += 2u;
	pWrite[iOffset++] = (uint8)PublicKey.Size;
	memcpy(pWrite + iOffset, PublicKey.Data, PublicKey.Size);
	iOffset += PublicKey.Size;
	__xrtTlsWrite16(pWrite + iOffset, Verify.Scheme);
	iOffset += 2u;
	__xrtTlsWrite16(pWrite + iOffset, (uint16)Verify.Signature.Size);
	iOffset += 2u;
	memcpy(pWrite + iOffset, Verify.Signature.Data, Verify.Signature.Size);
	return true;
}



/* 返回编码 TLS 1.2 ECDHE ClientKeyExchange 正文所需长度。 */
XRT_API size_t xrtTls12ClientKeyExchangeSize(xbytesview PublicKey)
{
	if ( !__xrtTlsViewValid(PublicKey) || (PublicKey.Size == 0) ||
		(PublicKey.Size > UINT8_MAX) ) {
		return __xrtTlsAuthMessageWriteError(
			XTLS_ERROR_HANDSHAKE, "size-tls12-client-key-exchange",
			"TLS 1.2 ClientKeyExchange public key is not encodable"
		);
	}
	return 1u + PublicKey.Size;
}



/* 编码 TLS 1.2 ECDHE ClientKeyExchange，允许公钥与输出重叠。 */
XRT_API bool xrtTls12ClientKeyExchangeEncode(
	xbytesview PublicKey,
	void* pOutput,
	size_t iOutputSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired = xrtTls12ClientKeyExchangeSize(PublicKey);

	if ( iRequired == 0 ) {
		return false;
	}
	if ( pOutput == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			"write-tls12-client-key-exchange",
			"TLS 1.2 ClientKeyExchange output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( iOutputSize < iRequired ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_HANDSHAKE,
			"write-tls12-client-key-exchange",
			"TLS 1.2 ClientKeyExchange output buffer is too small", SIZE_MAX
		);
		return false;
	}
	memmove(pWrite + 1u, PublicKey.Data, PublicKey.Size);
	pWrite[0] = (uint8)PublicKey.Size;
	return true;
}



/* 返回编码 OCSP CertificateStatus 正文所需长度。 */
XRT_API size_t xrtTlsCertificateStatusSize(
	const xtlscertificatestatusmessage* pStatus
)
{
	if ( !__xrtTlsCertificateStatusValid(
		pStatus, "size-certificate-status", XERR_VALUE
	) ) {
		return 0;
	}
	return 4u + pStatus->Response.Size;
}



/* 编码 OCSP CertificateStatus，允许响应与输出重叠。 */
XRT_API bool xrtTlsCertificateStatusEncode(
	const xtlscertificatestatusmessage* pStatus,
	void* pOutput,
	size_t iOutputSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired = xrtTlsCertificateStatusSize(pStatus);
	xtlscertificatestatusmessage Status;

	if ( iRequired == 0 ) {
		return false;
	}
	if ( pOutput == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-certificate-status",
			"TLS CertificateStatus output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( iOutputSize < iRequired ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_CERTIFICATE, "write-certificate-status",
			"TLS CertificateStatus output buffer is too small", SIZE_MAX
		);
		return false;
	}
	Status = *pStatus;
	memmove(pWrite + 4u, Status.Response.Data, Status.Response.Size);
	pWrite[0] = Status.Type;
	__xrtTlsWrite24(pWrite + 1u, (uint32)Status.Response.Size);
	return true;
}



/* 返回编码 CompressedCertificate 正文所需长度。 */
XRT_API size_t xrtTlsCompressedCertificateSize(
	const xtlscompressedcertificate* pCertificate
)
{
	if ( !__xrtTlsCompressedCertificateValid(
		pCertificate, "size-compressed-certificate", XERR_VALUE
	) ) {
		return 0;
	}
	return 5u + pCertificate->Data.Size;
}



/* 编码 CompressedCertificate，允许压缩数据与输出重叠。 */
XRT_API bool xrtTlsCompressedCertificateEncode(
	const xtlscompressedcertificate* pCertificate,
	void* pOutput,
	size_t iOutputSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired = xrtTlsCompressedCertificateSize(pCertificate);
	xtlscompressedcertificate Certificate;

	if ( iRequired == 0 ) {
		return false;
	}
	if ( pOutput == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-compressed-certificate",
			"TLS CompressedCertificate output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( iOutputSize < iRequired ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_CERTIFICATE, "write-compressed-certificate",
			"TLS CompressedCertificate output buffer is too small", SIZE_MAX
		);
		return false;
	}
	Certificate = *pCertificate;
	memmove(pWrite + 5u, Certificate.Data.Data, Certificate.Data.Size);
	__xrtTlsWrite16(pWrite, Certificate.Algorithm);
	__xrtTlsWrite24(
		pWrite + 2u, (uint32)Certificate.UncompressedSize
	);
	return true;
}

#endif
