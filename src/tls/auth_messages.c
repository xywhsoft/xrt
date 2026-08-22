#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_AUTH_MESSAGES)

/* 设置认证消息错误并返回 false。 */
static bool __xrtTlsAuthMessageError(
	xerrkind Kind,
	xtlserror Code,
	cstr sOperation,
	cstr sMessage,
	size_t iOffset
)
{
	__xrtTlsError(Kind, Code, sOperation, sMessage, iOffset);
	return false;
}



/* 读取一项已经去除外层总长的证书颁发者名称。 */
static xtlsitemresult __xrtTlsAuthorityRead(
	xtlsauthoritycursor* pCursor,
	xbytesview* pName,
	cstr sOperation,
	xerrkind Kind
)
{
	xtlsauthoritycursor Cursor;
	xbytesview Name;
	size_t iRemaining;
	size_t iNameSize;

	if ( (pCursor == NULL) || (pName == NULL) || (sOperation == NULL) ||
		!__xrtTlsViewValid(pCursor != NULL ?
			pCursor->Data : (xbytesview) { NULL, 1u }) ||
		((pCursor != NULL) &&
		 ((pCursor->Offset > pCursor->Data.Size) ||
		  (pCursor->Data.Size > UINT16_MAX))) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			sOperation != NULL ? sOperation : "read-certificate-authorities",
			"TLS certificate-authority cursor or output is invalid", SIZE_MAX
		);
		return XTLS_ITEM_ERROR;
	}
	if ( pCursor->Offset == pCursor->Data.Size ) {
		return XTLS_ITEM_DONE;
	}

	Cursor = *pCursor;
	iRemaining = Cursor.Data.Size - Cursor.Offset;
	if ( iRemaining < 2u ) {
		__xrtTlsAuthMessageError(
			Kind, XTLS_ERROR_CERTIFICATE, sOperation,
			"TLS certificate-authority name length is truncated", Cursor.Offset
		);
		return XTLS_ITEM_ERROR;
	}
	iNameSize = __xrtTlsRead16(Cursor.Data.Data + Cursor.Offset);
	if ( (iNameSize == 0) || (iNameSize > iRemaining - 2u) ) {
		__xrtTlsAuthMessageError(
			Kind, XTLS_ERROR_CERTIFICATE, sOperation,
			"TLS certificate-authority name is empty or truncated", Cursor.Offset
		);
		return XTLS_ITEM_ERROR;
	}
	Name.Data = Cursor.Data.Data + Cursor.Offset + 2u;
	Name.Size = iNameSize;
	Cursor.Offset += 2u + iNameSize;
	*pCursor = Cursor;
	*pName = Name;
	return XTLS_ITEM_VALUE;
}



/* 验证完整证书颁发者向量并可选发布去除前缀后的游标。 */
bool __xrtTlsAuthoritiesValid(
	xbytesview Data,
	xtlsauthoritycursor* pCursor,
	cstr sOperation,
	xerrkind Kind
)
{
	xtlsauthoritycursor Cursor;
	xbytesview Name;
	xtlsitemresult Result;
	size_t iSize;

	if ( !__xrtTlsViewValid(Data) || (sOperation == NULL) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			sOperation != NULL ? sOperation : "parse-certificate-authorities",
			"TLS certificate-authority vector is invalid", SIZE_MAX
		);
		return false;
	}
	if ( (Data.Size < 2u) || (Data.Size > (size_t)UINT16_MAX + 2u) ) {
		return __xrtTlsAuthMessageError(
			Kind, XTLS_ERROR_CERTIFICATE, sOperation,
			"TLS certificate-authority vector is truncated or too long", 0
		);
	}
	iSize = __xrtTlsRead16(Data.Data);
	if ( iSize != Data.Size - 2u ) {
		return __xrtTlsAuthMessageError(
			Kind, XTLS_ERROR_CERTIFICATE, sOperation,
			"TLS certificate-authority vector length is inconsistent", 0
		);
	}
	Cursor.Data.Data = Data.Data + 2u;
	Cursor.Data.Size = iSize;
	Cursor.Offset = 0;
	do {
		Result = __xrtTlsAuthorityRead(
			&Cursor, &Name, sOperation, Kind
		);
	} while ( Result == XTLS_ITEM_VALUE );
	if ( Result == XTLS_ITEM_ERROR ) {
		return false;
	}
	Cursor.Offset = 0;
	if ( pCursor != NULL ) {
		*pCursor = Cursor;
	}
	return true;
}



/* 严格解析证书颁发者名称向量并初始化游标。 */
XRT_API bool xrtTlsAuthorities(
	xbytesview Data,
	xtlsauthoritycursor* pCursor
)
{
	if ( pCursor == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-certificate-authorities",
			"TLS certificate-authority cursor is invalid", SIZE_MAX
		);
		return false;
	}
	return __xrtTlsAuthoritiesValid(
		Data, pCursor, "parse-certificate-authorities", XERR_PROTOCOL
	);
}



/* 读取下一项非空 DER DistinguishedName。 */
XRT_API xtlsitemresult xrtTlsAuthoritiesRead(
	xtlsauthoritycursor* pCursor,
	xbytesview* pName
)
{
	return __xrtTlsAuthorityRead(
		pCursor, pName, "read-certificate-authorities", XERR_PROTOCOL
	);
}



/* 验证 TLS 1.2 CertificateRequest 的全部借用字段。 */
bool __xrtTls12CertificateRequestValid(
	const xtls12certificaterequest* pRequest,
	cstr sOperation,
	xerrkind Kind
)
{
	size_t iBodySize;

	if ( (pRequest == NULL) || (sOperation == NULL) ||
		!__xrtTlsViewValid(pRequest != NULL ?
			pRequest->CertificateTypes : (xbytesview) { NULL, 1u }) ||
		!__xrtTlsViewValid(pRequest != NULL ?
			pRequest->Signatures.Data : (xbytesview) { NULL, 1u }) ||
		!__xrtTlsViewValid(pRequest != NULL ?
			pRequest->AuthorityData : (xbytesview) { NULL, 1u }) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			sOperation != NULL ? sOperation : "validate-certificate-request",
			"TLS 1.2 CertificateRequest fields are invalid", SIZE_MAX
		);
		return false;
	}
	if ( (pRequest->CertificateTypes.Size == 0) ||
		(pRequest->CertificateTypes.Size > UINT8_MAX) ) {
		return __xrtTlsAuthMessageError(
			Kind, XTLS_ERROR_CERTIFICATE, sOperation,
			"TLS 1.2 certificate-type list is empty or too long", 0
		);
	}
	if ( pRequest->Signatures.Data.Size > UINT16_MAX ) {
		return __xrtTlsAuthMessageError(
			Kind, XTLS_ERROR_LIMIT, sOperation,
			"TLS 1.2 signature list exceeds its 16-bit limit", SIZE_MAX
		);
	}
	if ( !__xrtTlsIdsDataValid(
		pRequest->Signatures.Data, sOperation, Kind
	) ) {
		return false;
	}
	if ( !__xrtTlsAuthoritiesValid(
		pRequest->AuthorityData, NULL, sOperation, Kind
	) ) {
		return false;
	}
	iBodySize = 3u + pRequest->CertificateTypes.Size +
		pRequest->Signatures.Data.Size + pRequest->AuthorityData.Size;
	if ( iBodySize > XTLS_HANDSHAKE_BODY_MAX ) {
		return __xrtTlsAuthMessageError(
			Kind, XTLS_ERROR_LIMIT, sOperation,
			"TLS 1.2 CertificateRequest exceeds the handshake limit", SIZE_MAX
		);
	}
	return true;
}



/* 严格解析 TLS 1.2 CertificateRequest 正文。 */
XRT_API bool xrtTls12CertificateRequestParse(
	xbytesview Body,
	xtls12certificaterequest* pRequest
)
{
	xtls12certificaterequest Request;
	size_t iOffset = 0;
	size_t iTypeSize;
	size_t iSignatureSize;

	if ( (pRequest == NULL) || !__xrtTlsViewValid(Body) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-tls12-certificate-request",
			"TLS 1.2 CertificateRequest input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( (Body.Size == 0) || (Body.Size > XTLS_HANDSHAKE_BODY_MAX) ) {
		return __xrtTlsAuthMessageError(
			XERR_PROTOCOL, XTLS_ERROR_CERTIFICATE,
			"parse-tls12-certificate-request",
			"TLS 1.2 CertificateRequest is empty or too long", 0
		);
	}
	memset(&Request, 0, sizeof(Request));
	iTypeSize = Body.Data[iOffset++];
	if ( (iTypeSize == 0) || (iTypeSize > Body.Size - iOffset) ) {
		return __xrtTlsAuthMessageError(
			XERR_PROTOCOL, XTLS_ERROR_CERTIFICATE,
			"parse-tls12-certificate-request",
			"TLS 1.2 certificate-type list is empty or truncated", 0
		);
	}
	Request.CertificateTypes.Data = Body.Data + iOffset;
	Request.CertificateTypes.Size = iTypeSize;
	iOffset += iTypeSize;
	if ( Body.Size - iOffset < 4u ) {
		return __xrtTlsAuthMessageError(
			XERR_PROTOCOL, XTLS_ERROR_CERTIFICATE,
			"parse-tls12-certificate-request",
			"TLS 1.2 signature or authority vector is truncated", iOffset
		);
	}
	iSignatureSize = __xrtTlsRead16(Body.Data + iOffset);
	iOffset += 2u;
	if ( (iSignatureSize > Body.Size - iOffset) ||
		(Body.Size - iOffset - iSignatureSize < 2u) ) {
		return __xrtTlsAuthMessageError(
			XERR_PROTOCOL, XTLS_ERROR_CERTIFICATE,
			"parse-tls12-certificate-request",
			"TLS 1.2 signature list is truncated", iOffset - 2u
		);
	}
	Request.Signatures.Data.Data = Body.Data + iOffset;
	Request.Signatures.Data.Size = iSignatureSize;
	iOffset += iSignatureSize;
	Request.AuthorityData.Data = Body.Data + iOffset;
	Request.AuthorityData.Size = Body.Size - iOffset;
	if ( !__xrtTls12CertificateRequestValid(
		&Request, "parse-tls12-certificate-request", XERR_PROTOCOL
	) ) {
		return false;
	}
	*pRequest = Request;
	return true;
}



/* 验证 TLS 1.3 CertificateRequest 扩展并发布常用认证字段。 */
bool __xrtTls13CertificateRequestExtensions(
	xbytesview Extensions,
	xtlsids* pSignatures,
	xtlsids* pCertificateSignatures,
	xbytesview* pAuthorityData,
	cstr sOperation,
	xerrkind Kind
)
{
	xtlsextensioncursor Cursor;
	xtlsextension Extension;
	xtlsitemresult Result;
	xtlsids Signatures;
	xtlsids CertificateSignatures;
	xtlsauthoritycursor Authorities;
	xbytesview AuthorityData = { NULL, 0 };
	bool bSignatures = false;

	if ( (pSignatures == NULL) || (pCertificateSignatures == NULL) ||
		(pAuthorityData == NULL) || (sOperation == NULL) ||
		!__xrtTlsViewValid(Extensions) ||
		(Extensions.Size > UINT16_MAX) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			sOperation != NULL ? sOperation : "validate-tls13-certificate-request",
			"TLS 1.3 CertificateRequest extensions or outputs are invalid",
			SIZE_MAX
		);
		return false;
	}
	memset(&Signatures, 0, sizeof(Signatures));
	memset(&CertificateSignatures, 0, sizeof(CertificateSignatures));
	if ( !xrtTlsExtensionsInit(&Cursor, Extensions) ) {
		return false;
	}
	while ( (Result = xrtTlsExtensionsRead(
		&Cursor, &Extension
	)) == XTLS_ITEM_VALUE ) {
		switch ( Extension.Type ) {
			case XTLS_EXTENSION_SIGNATURE_ALGORITHMS:
				if ( !xrtTlsSignatures(Extension.Data, &Signatures) ) {
					return false;
				}
				bSignatures = true;
				break;

			case XTLS_EXTENSION_SIGNATURE_ALGORITHMS_CERT:
				if ( !xrtTlsSignatures(
					Extension.Data, &CertificateSignatures
				) ) {
					return false;
				}
				break;

			case XTLS_EXTENSION_CERTIFICATE_AUTHORITIES:
				if ( !__xrtTlsAuthoritiesValid(
					Extension.Data, &Authorities, sOperation, Kind
				) ) {
					return false;
				}
				if ( Authorities.Data.Size == 0 ) {
					return __xrtTlsAuthMessageError(
						Kind, XTLS_ERROR_CERTIFICATE, sOperation,
						"TLS 1.3 certificate-authority list cannot be empty",
						Cursor.Offset - Extension.EncodedSize
					);
				}
				AuthorityData = Extension.Data;
				break;

			default:
				break;
		}
	}
	if ( Result == XTLS_ITEM_ERROR ) {
		return false;
	}
	if ( !bSignatures ) {
		return __xrtTlsAuthMessageError(
			Kind, XTLS_ERROR_EXTENSION, sOperation,
			"TLS 1.3 CertificateRequest lacks signature_algorithms", SIZE_MAX
		);
	}
	*pSignatures = Signatures;
	*pCertificateSignatures = CertificateSignatures;
	*pAuthorityData = AuthorityData;
	return true;
}



/* 严格解析 TLS 1.3 CertificateRequest 正文。 */
XRT_API bool xrtTls13CertificateRequestParse(
	xbytesview Body,
	xtls13certificaterequest* pRequest
)
{
	xtls13certificaterequest Request;
	size_t iOffset = 0;
	size_t iContextSize;
	size_t iExtensionSize;

	if ( (pRequest == NULL) || !__xrtTlsViewValid(Body) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-tls13-certificate-request",
			"TLS 1.3 CertificateRequest input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( (Body.Size < 3u) || (Body.Size > XTLS_HANDSHAKE_BODY_MAX) ) {
		return __xrtTlsAuthMessageError(
			XERR_PROTOCOL, XTLS_ERROR_CERTIFICATE,
			"parse-tls13-certificate-request",
			"TLS 1.3 CertificateRequest is truncated or too long", 0
		);
	}
	memset(&Request, 0, sizeof(Request));
	iContextSize = Body.Data[iOffset++];
	if ( iContextSize > Body.Size - iOffset ) {
		return __xrtTlsAuthMessageError(
			XERR_PROTOCOL, XTLS_ERROR_CERTIFICATE,
			"parse-tls13-certificate-request",
			"TLS 1.3 certificate request context is truncated", 0
		);
	}
	Request.RequestContext.Data = Body.Data + iOffset;
	Request.RequestContext.Size = iContextSize;
	iOffset += iContextSize;
	if ( Body.Size - iOffset < 2u ) {
		return __xrtTlsAuthMessageError(
			XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
			"parse-tls13-certificate-request",
			"TLS 1.3 CertificateRequest extension length is truncated", iOffset
		);
	}
	iExtensionSize = __xrtTlsRead16(Body.Data + iOffset);
	iOffset += 2u;
	if ( iExtensionSize != Body.Size - iOffset ) {
		return __xrtTlsAuthMessageError(
			XERR_PROTOCOL, XTLS_ERROR_EXTENSION,
			"parse-tls13-certificate-request",
			"TLS 1.3 CertificateRequest extension length is inconsistent",
			iOffset - 2u
		);
	}
	Request.Extensions.Data = Body.Data + iOffset;
	Request.Extensions.Size = iExtensionSize;
	if ( !__xrtTls13CertificateRequestExtensions(
		Request.Extensions, &Request.Signatures,
		&Request.CertificateSignatures, &Request.AuthorityData,
		"parse-tls13-certificate-request", XERR_PROTOCOL
	) ) {
		return false;
	}
	*pRequest = Request;
	return true;
}



/* 验证 TLS 1.2 ECDHE ServerKeyExchange 的可编码字段。 */
bool __xrtTls12ServerKeyExchangeValid(
	uint16 iGroup,
	xbytesview PublicKey,
	const xtlscertificateverify* pVerify,
	cstr sOperation,
	xerrkind Kind
)
{
	(void)iGroup;
	if ( (pVerify == NULL) || (sOperation == NULL) ||
		!__xrtTlsViewValid(PublicKey) ||
		!__xrtTlsViewValid(pVerify != NULL ?
			pVerify->Signature : (xbytesview) { NULL, 1u }) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			sOperation != NULL ? sOperation : "validate-server-key-exchange",
			"TLS 1.2 ServerKeyExchange fields are invalid", SIZE_MAX
		);
		return false;
	}
	if ( (PublicKey.Size == 0) || (PublicKey.Size > UINT8_MAX) ||
		(pVerify->Signature.Size == 0) ||
		(pVerify->Signature.Size > UINT16_MAX) ) {
		return __xrtTlsAuthMessageError(
			Kind, XTLS_ERROR_HANDSHAKE, sOperation,
			"TLS 1.2 ServerKeyExchange key or signature is not encodable",
			SIZE_MAX
		);
	}
	return true;
}



/* 严格解析 TLS 1.2 ECDHE ServerKeyExchange 正文。 */
XRT_API bool xrtTls12ServerKeyExchangeParse(
	xbytesview Body,
	xtls12serverkeyexchange* pExchange
)
{
	xtls12serverkeyexchange Exchange;
	size_t iKeySize;
	size_t iParameterSize;

	if ( (pExchange == NULL) || !__xrtTlsViewValid(Body) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-tls12-server-key-exchange",
			"TLS 1.2 ServerKeyExchange input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( (Body.Size < 10u) || (Body.Data[0] != 3u) ) {
		return __xrtTlsAuthMessageError(
			XERR_PROTOCOL, XTLS_ERROR_HANDSHAKE,
			"parse-tls12-server-key-exchange",
			"TLS 1.2 ServerKeyExchange lacks named-curve parameters", 0
		);
	}
	memset(&Exchange, 0, sizeof(Exchange));
	Exchange.Group = __xrtTlsRead16(Body.Data + 1u);
	iKeySize = Body.Data[3];
	if ( (iKeySize == 0) || (iKeySize > Body.Size - 4u) ) {
		return __xrtTlsAuthMessageError(
			XERR_PROTOCOL, XTLS_ERROR_HANDSHAKE,
			"parse-tls12-server-key-exchange",
			"TLS 1.2 ServerKeyExchange public key is empty or truncated", 3u
		);
	}
	iParameterSize = 4u + iKeySize;
	if ( Body.Size - iParameterSize < 5u ) {
		return __xrtTlsAuthMessageError(
			XERR_PROTOCOL, XTLS_ERROR_VERIFY,
			"parse-tls12-server-key-exchange",
			"TLS 1.2 ServerKeyExchange signature is truncated", iParameterSize
		);
	}
	Exchange.PublicKey.Data = Body.Data + 4u;
	Exchange.PublicKey.Size = iKeySize;
	Exchange.Parameters.Data = Body.Data;
	Exchange.Parameters.Size = iParameterSize;
	if ( !xrtTlsCertificateVerifyParse(
		(xbytesview) {
			Body.Data + iParameterSize, Body.Size - iParameterSize
		}, &Exchange.Verify
	) || !__xrtTls12ServerKeyExchangeValid(
		Exchange.Group, Exchange.PublicKey, &Exchange.Verify,
		"parse-tls12-server-key-exchange", XERR_PROTOCOL
	) ) {
		return false;
	}
	*pExchange = Exchange;
	return true;
}



/* 严格解析 TLS 1.2 ECDHE ClientKeyExchange 公钥。 */
XRT_API bool xrtTls12ClientKeyExchangeParse(
	xbytesview Body,
	xbytesview* pPublicKey
)
{
	xbytesview PublicKey;
	size_t iKeySize;

	if ( (pPublicKey == NULL) || !__xrtTlsViewValid(Body) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-tls12-client-key-exchange",
			"TLS 1.2 ClientKeyExchange input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( Body.Size < 2u ) {
		return __xrtTlsAuthMessageError(
			XERR_PROTOCOL, XTLS_ERROR_HANDSHAKE,
			"parse-tls12-client-key-exchange",
			"TLS 1.2 ClientKeyExchange public key is empty or truncated", 0
		);
	}
	iKeySize = Body.Data[0];
	if ( (iKeySize == 0) || (iKeySize != Body.Size - 1u) ) {
		return __xrtTlsAuthMessageError(
			XERR_PROTOCOL, XTLS_ERROR_HANDSHAKE,
			"parse-tls12-client-key-exchange",
			"TLS 1.2 ClientKeyExchange public-key length is inconsistent", 0
		);
	}
	PublicKey.Data = Body.Data + 1u;
	PublicKey.Size = iKeySize;
	*pPublicKey = PublicKey;
	return true;
}



/* 验证 OCSP CertificateStatus 的可编码字段。 */
bool __xrtTlsCertificateStatusValid(
	const xtlscertificatestatusmessage* pStatus,
	cstr sOperation,
	xerrkind Kind
)
{
	if ( (pStatus == NULL) || (sOperation == NULL) ||
		!__xrtTlsViewValid(pStatus != NULL ?
			pStatus->Response : (xbytesview) { NULL, 1u }) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			sOperation != NULL ? sOperation : "validate-certificate-status",
			"TLS CertificateStatus fields are invalid", SIZE_MAX
		);
		return false;
	}
	if ( (pStatus->Type != XTLS_CERTIFICATE_STATUS_OCSP) ||
		(pStatus->Response.Size == 0) ||
		(pStatus->Response.Size > XTLS_HANDSHAKE_BODY_MAX - 4u) ) {
		return __xrtTlsAuthMessageError(
			Kind, XTLS_ERROR_CERTIFICATE, sOperation,
			"TLS CertificateStatus type or response is invalid", SIZE_MAX
		);
	}
	return true;
}



/* 严格解析 OCSP CertificateStatus 正文。 */
XRT_API bool xrtTlsCertificateStatusParse(
	xbytesview Body,
	xtlscertificatestatusmessage* pStatus
)
{
	xtlscertificatestatusmessage Status;
	size_t iResponseSize;

	if ( (pStatus == NULL) || !__xrtTlsViewValid(Body) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-certificate-status",
			"TLS CertificateStatus input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( Body.Size < 5u ) {
		return __xrtTlsAuthMessageError(
			XERR_PROTOCOL, XTLS_ERROR_CERTIFICATE, "parse-certificate-status",
			"TLS CertificateStatus response is empty or truncated", Body.Size
		);
	}
	iResponseSize = __xrtTlsRead24(Body.Data + 1u);
	if ( iResponseSize != Body.Size - 4u ) {
		return __xrtTlsAuthMessageError(
			XERR_PROTOCOL, XTLS_ERROR_CERTIFICATE, "parse-certificate-status",
			"TLS CertificateStatus response length is inconsistent", 1u
		);
	}
	Status.Type = Body.Data[0];
	Status.Response.Data = Body.Data + 4u;
	Status.Response.Size = iResponseSize;
	if ( !__xrtTlsCertificateStatusValid(
		&Status, "parse-certificate-status", XERR_PROTOCOL
	) ) {
		return false;
	}
	*pStatus = Status;
	return true;
}



/* 验证 CompressedCertificate 的可编码字段。 */
bool __xrtTlsCompressedCertificateValid(
	const xtlscompressedcertificate* pCertificate,
	cstr sOperation,
	xerrkind Kind
)
{
	if ( (pCertificate == NULL) || (sOperation == NULL) ||
		!__xrtTlsViewValid(pCertificate != NULL ?
			pCertificate->Data : (xbytesview) { NULL, 1u }) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT,
			sOperation != NULL ? sOperation : "validate-compressed-certificate",
			"TLS CompressedCertificate fields are invalid", SIZE_MAX
		);
		return false;
	}
	if ( (pCertificate->UncompressedSize == 0) ||
		(pCertificate->UncompressedSize > XTLS_HANDSHAKE_BODY_MAX) ||
		(pCertificate->Data.Size == 0) ||
		(pCertificate->Data.Size > XTLS_HANDSHAKE_BODY_MAX - 5u) ) {
		return __xrtTlsAuthMessageError(
			Kind, XTLS_ERROR_CERTIFICATE, sOperation,
			"TLS CompressedCertificate length is invalid", SIZE_MAX
		);
	}
	return true;
}



/* 严格解析 TLS 1.3 CompressedCertificate 正文。 */
XRT_API bool xrtTlsCompressedCertificateParse(
	xbytesview Body,
	xtlscompressedcertificate* pCertificate
)
{
	xtlscompressedcertificate Certificate;

	if ( (pCertificate == NULL) || !__xrtTlsViewValid(Body) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-compressed-certificate",
			"TLS CompressedCertificate input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( (Body.Size < 6u) || (Body.Size > XTLS_HANDSHAKE_BODY_MAX) ) {
		return __xrtTlsAuthMessageError(
			XERR_PROTOCOL, XTLS_ERROR_CERTIFICATE, "parse-compressed-certificate",
			"TLS CompressedCertificate body is empty, truncated or too long", 0
		);
	}
	Certificate.Algorithm = __xrtTlsRead16(Body.Data);
	Certificate.UncompressedSize = __xrtTlsRead24(Body.Data + 2u);
	Certificate.Data.Data = Body.Data + 5u;
	Certificate.Data.Size = Body.Size - 5u;
	if ( !__xrtTlsCompressedCertificateValid(
		&Certificate, "parse-compressed-certificate", XERR_PROTOCOL
	) ) {
		return false;
	}
	*pCertificate = Certificate;
	return true;
}

#endif
