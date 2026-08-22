#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_MESSAGES_WRITE)

/* 报告消息编码参数错误并返回零。 */
static size_t __xrtTlsMessageWriteError(
	xtlserror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtTlsError(
		XERR_VALUE, Code, sOperation, sMessage, SIZE_MAX
	);
	return 0;
}



/* 检查消息编码层接受的协议版本。 */
static bool __xrtTlsMessageWriteVersion(
	xtlsversion Version,
	cstr sOperation
)
{
	if ( __xrtTlsVersionSupported(Version) ) {
		return true;
	}
	__xrtTlsError(
		XERR_VALUE, XTLS_ERROR_VERSION, sOperation,
		"TLS message version is not supported", SIZE_MAX
	);
	return false;
}



/* 计算并验证完整 Certificate 正文长度。 */
XRT_API size_t xrtTlsCertificateSize(
	xtlsversion Version,
	xbytesview RequestContext,
	const xtlscertificateentry* pEntries,
	size_t iCount
)
{
	size_t iListSize = 0;
	size_t iPrefixSize;

	if ( !__xrtTlsViewValid(RequestContext) ||
		((iCount != 0) && (pEntries == NULL)) ||
		(iCount > SIZE_MAX / sizeof(xtlscertificateentry)) ) {
		return __xrtTlsMessageWriteError(
			XTLS_ERROR_ARGUMENT, "size-certificate-message",
			"TLS Certificate entries or request context are invalid"
		);
	}
	if ( !__xrtTlsMessageWriteVersion(
		Version, "size-certificate-message"
	) ) {
		return 0;
	}
	if ( ((Version == XTLS_VERSION_12) &&
		 (RequestContext.Size != 0)) ||
		((Version == XTLS_VERSION_13) &&
		 (RequestContext.Size > UINT8_MAX)) ) {
		return __xrtTlsMessageWriteError(
			XTLS_ERROR_CERTIFICATE, "size-certificate-message",
			"TLS Certificate request context is not encodable"
		);
	}

	/* 每个条目先做完整验证，再参与 24 位列表长度计算。 */
	for ( size_t i = 0; i < iCount; i++ ) {
		size_t iEntrySize;

		if ( !__xrtTlsViewValid(pEntries[i].Data) ||
			!__xrtTlsViewValid(pEntries[i].Extensions) ||
			(pEntries[i].Data.Size == 0) ||
			(pEntries[i].Data.Size > XTLS_HANDSHAKE_BODY_MAX) ) {
			return __xrtTlsMessageWriteError(
				XTLS_ERROR_CERTIFICATE, "size-certificate-message",
				"TLS Certificate entry is not encodable"
			);
		}
		iEntrySize = 3u + pEntries[i].Data.Size;
		if ( Version == XTLS_VERSION_12 ) {
			if ( pEntries[i].Extensions.Size != 0 ) {
				return __xrtTlsMessageWriteError(
					XTLS_ERROR_CERTIFICATE, "size-certificate-message",
					"TLS 1.2 Certificate entry cannot carry extensions"
				);
			}
		} else {
			if ( (pEntries[i].Extensions.Size > UINT16_MAX) ||
				!xrtTlsExtensionsValidate(pEntries[i].Extensions) ) {
				return 0;
			}
			iEntrySize += 2u + pEntries[i].Extensions.Size;
		}
		if ( (iEntrySize > XTLS_HANDSHAKE_BODY_MAX) ||
			(iListSize > XTLS_HANDSHAKE_BODY_MAX - iEntrySize) ) {
			return __xrtTlsMessageWriteError(
				XTLS_ERROR_CERTIFICATE, "size-certificate-message",
				"TLS Certificate list exceeds the 24-bit wire limit"
			);
		}
		iListSize += iEntrySize;
	}
	iPrefixSize = Version == XTLS_VERSION_13 ?
		4u + RequestContext.Size : 3u;
	if ( iListSize > XTLS_HANDSHAKE_BODY_MAX - iPrefixSize ) {
		return __xrtTlsMessageWriteError(
			XTLS_ERROR_CERTIFICATE, "size-certificate-message",
			"TLS Certificate body exceeds the 24-bit wire limit"
		);
	}
	return iPrefixSize + iListSize;
}



/* 检查 Certificate 编码输入是否与输出区域重叠。 */
static bool __xrtTlsCertificateOverlap(
	const void* pOutput,
	size_t iOutputSize,
	xbytesview RequestContext,
	const xtlscertificateentry* pEntries,
	size_t iCount
)
{
	xbytesview Array;

	Array.Data = (const uint8*)pEntries;
	Array.Size = iCount * sizeof(xtlscertificateentry);
	if ( __xrtTlsViewOverlap(
		pOutput, iOutputSize, RequestContext
	) || __xrtTlsViewOverlap(
		pOutput, iOutputSize, Array
	) ) {
		return true;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( __xrtTlsViewOverlap(
			pOutput, iOutputSize, pEntries[i].Data
		) || __xrtTlsViewOverlap(
			pOutput, iOutputSize, pEntries[i].Extensions
		) ) {
			return true;
		}
	}
	return false;
}



/* 失败原子地编码完整 Certificate 正文。 */
XRT_API bool xrtTlsCertificateEncode(
	xtlsversion Version,
	xbytesview RequestContext,
	const xtlscertificateentry* pEntries,
	size_t iCount,
	void* pOutput,
	size_t iOutputSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired = xrtTlsCertificateSize(
		Version, RequestContext, pEntries, iCount
	);
	size_t iOffset = 0;
	size_t iListOffset;

	if ( iRequired == 0 ) {
		return false;
	}
	if ( pOutput == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-certificate-message",
			"TLS Certificate output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( iOutputSize < iRequired ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_CERTIFICATE, "write-certificate-message",
			"TLS Certificate output buffer is too small", SIZE_MAX
		);
		return false;
	}
	if ( __xrtTlsCertificateOverlap(
		pOutput, iRequired, RequestContext, pEntries, iCount
	) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-certificate-message",
			"TLS Certificate input overlaps its output", SIZE_MAX
		);
		return false;
	}

	/* TLS 1.3 首先写入请求上下文，TLS 1.2 直接进入列表。 */
	if ( Version == XTLS_VERSION_13 ) {
		pWrite[iOffset++] = (uint8)RequestContext.Size;
		if ( RequestContext.Size != 0 ) {
			memcpy(
				pWrite + iOffset,
				RequestContext.Data,
				RequestContext.Size
			);
			iOffset += RequestContext.Size;
		}
	}
	iListOffset = iOffset;
	iOffset += 3u;
	for ( size_t i = 0; i < iCount; i++ ) {
		__xrtTlsWrite24(
			pWrite + iOffset, (uint32)pEntries[i].Data.Size
		);
		iOffset += 3u;
		memcpy(
			pWrite + iOffset,
			pEntries[i].Data.Data,
			pEntries[i].Data.Size
		);
		iOffset += pEntries[i].Data.Size;
		if ( Version == XTLS_VERSION_13 ) {
			__xrtTlsWrite16(
				pWrite + iOffset,
				(uint16)pEntries[i].Extensions.Size
			);
			iOffset += 2u;
			if ( pEntries[i].Extensions.Size != 0 ) {
				memcpy(
					pWrite + iOffset,
					pEntries[i].Extensions.Data,
					pEntries[i].Extensions.Size
				);
				iOffset += pEntries[i].Extensions.Size;
			}
		}
	}
	__xrtTlsWrite24(
		pWrite + iListOffset, (uint32)(iOffset - iListOffset - 3u)
	);
	return true;
}



/* 返回编码 EncryptedExtensions 正文所需长度。 */
XRT_API size_t xrtTlsEncryptedExtensionsSize(xbytesview Extensions)
{
	if ( !__xrtTlsViewValid(Extensions) ||
		(Extensions.Size > UINT16_MAX) ) {
		return __xrtTlsMessageWriteError(
			XTLS_ERROR_EXTENSION, "size-encrypted-extensions",
			"TLS EncryptedExtensions vector is not encodable"
		);
	}
	if ( !__xrtTlsEncryptedExtensionsValid(Extensions) ) {
		return 0;
	}
	return 2u + Extensions.Size;
}



/* 失败原子地编码 EncryptedExtensions 正文。 */
XRT_API bool xrtTlsEncryptedExtensionsEncode(
	xbytesview Extensions,
	void* pOutput,
	size_t iOutputSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired;

	if ( (pOutput == NULL) || !__xrtTlsViewValid(Extensions) ||
		(Extensions.Size > UINT16_MAX) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-encrypted-extensions",
			"TLS EncryptedExtensions input or output is invalid", SIZE_MAX
		);
		return false;
	}
	iRequired = 2u + Extensions.Size;
	if ( iOutputSize < iRequired ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_EXTENSION, "write-encrypted-extensions",
			"TLS EncryptedExtensions output buffer is too small", SIZE_MAX
		);
		return false;
	}
	if ( !__xrtTlsEncryptedExtensionsValid(Extensions) ) {
		return false;
	}

	/* 先移动负载再写前缀，因此原始扩展向量允许与输出重叠。 */
	if ( Extensions.Size != 0 ) {
		memmove(pWrite + 2u, Extensions.Data, Extensions.Size);
	}
	__xrtTlsWrite16(pWrite, (uint16)Extensions.Size);
	return true;
}



/* 返回编码 CertificateVerify 正文所需长度。 */
XRT_API size_t xrtTlsCertificateVerifySize(
	const xtlscertificateverify* pVerify
)
{
	if ( (pVerify == NULL) || !__xrtTlsViewValid(
		pVerify != NULL ?
			pVerify->Signature : (xbytesview) { NULL, 1u }
	) || ((pVerify != NULL) &&
		((pVerify->Signature.Size == 0) ||
		 (pVerify->Signature.Size > UINT16_MAX))) ) {
		return __xrtTlsMessageWriteError(
			XTLS_ERROR_VERIFY, "size-certificate-verify",
			"TLS CertificateVerify signature is not encodable"
		);
	}
	return 4u + pVerify->Signature.Size;
}



/* 失败原子地编码 CertificateVerify 正文。 */
XRT_API bool xrtTlsCertificateVerifyEncode(
	const xtlscertificateverify* pVerify,
	void* pOutput,
	size_t iOutputSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired = xrtTlsCertificateVerifySize(pVerify);
	xtlscertificateverify Verify;

	if ( iRequired == 0 ) {
		return false;
	}
	if ( pOutput == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-certificate-verify",
			"TLS CertificateVerify output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( iOutputSize < iRequired ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_VERIFY, "write-certificate-verify",
			"TLS CertificateVerify output buffer is too small", SIZE_MAX
		);
		return false;
	}
	Verify = *pVerify;
	memmove(
		pWrite + 4u,
		Verify.Signature.Data,
		Verify.Signature.Size
	);
	__xrtTlsWrite16(pWrite, Verify.Scheme);
	__xrtTlsWrite16(pWrite + 2u, (uint16)Verify.Signature.Size);
	return true;
}



/* 编码非空 Finished 验证数据正文。 */
XRT_API bool xrtTlsFinishedEncode(
	xbytesview VerifyData,
	void* pOutput,
	size_t iOutputSize
)
{
	if ( (pOutput == NULL) || !__xrtTlsViewValid(VerifyData) ||
		(VerifyData.Size == 0) ||
		(VerifyData.Size > XTLS_HANDSHAKE_BODY_MAX) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-finished",
			"TLS Finished input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( iOutputSize < VerifyData.Size ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_VERIFY, "write-finished",
			"TLS Finished output buffer is too small", SIZE_MAX
		);
		return false;
	}
	memmove(pOutput, VerifyData.Data, VerifyData.Size);
	return true;
}



/* 编码 TLS 1.3 单字节 KeyUpdate 请求。 */
XRT_API bool xrtTlsKeyUpdateEncode(
	xtlskeyupdate Request,
	void* pOutput,
	size_t iOutputSize
)
{
	if ( (pOutput == NULL) ||
		((Request != XTLS_KEY_UPDATE_NOT_REQUESTED) &&
		 (Request != XTLS_KEY_UPDATE_REQUESTED)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-key-update",
			"TLS KeyUpdate request or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( iOutputSize < 1u ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_HANDSHAKE, "write-key-update",
			"TLS KeyUpdate output buffer is too small", SIZE_MAX
		);
		return false;
	}
	((uint8*)pOutput)[0] = (uint8)Request;
	return true;
}



/* 返回编码 NewSessionTicket 正文所需长度。 */
XRT_API size_t xrtTlsSessionTicketSize(
	const xtlssessionticket* pTicket
)
{
	if ( (pTicket == NULL) || !__xrtTlsViewValid(
		pTicket != NULL ? pTicket->Nonce : (xbytesview) { NULL, 1u }
	) || !__xrtTlsViewValid(
		pTicket != NULL ? pTicket->Ticket : (xbytesview) { NULL, 1u }
	) || !__xrtTlsViewValid(
		pTicket != NULL ? pTicket->Extensions : (xbytesview) { NULL, 1u }
	) ) {
		return __xrtTlsMessageWriteError(
			XTLS_ERROR_ARGUMENT, "size-session-ticket",
			"TLS NewSessionTicket view is invalid"
		);
	}
	if ( !__xrtTlsMessageWriteVersion(
		pTicket->Version, "size-session-ticket"
	) ) {
		return 0;
	}
	if ( pTicket->Version == XTLS_VERSION_12 ) {
		if ( (pTicket->AgeAdd != 0) || (pTicket->Nonce.Size != 0) ||
			(pTicket->Extensions.Size != 0) ||
			(pTicket->Ticket.Size > UINT16_MAX) ) {
			return __xrtTlsMessageWriteError(
				XTLS_ERROR_RESUME, "size-session-ticket",
				"TLS 1.2 NewSessionTicket fields are not encodable"
			);
		}
		return 6u + pTicket->Ticket.Size;
	}
	if ( (pTicket->Lifetime > XTLS13_TICKET_LIFETIME_MAX) ||
		(pTicket->Nonce.Size > UINT8_MAX) ||
		(pTicket->Ticket.Size == 0) ||
		(pTicket->Ticket.Size > UINT16_MAX) ||
		(pTicket->Extensions.Size > UINT16_MAX) ) {
		return __xrtTlsMessageWriteError(
			XTLS_ERROR_RESUME, "size-session-ticket",
			"TLS 1.3 NewSessionTicket fields are not encodable"
		);
	}
	if ( !__xrtTlsTicketExtensionsValid(pTicket->Extensions) ) {
		return 0;
	}
	return 13u + pTicket->Nonce.Size +
		pTicket->Ticket.Size + pTicket->Extensions.Size;
}



/* 检查 NewSessionTicket 输入字段是否与输出区域重叠。 */
static bool __xrtTlsSessionTicketOverlap(
	const xtlssessionticket* pTicket,
	const void* pOutput,
	size_t iOutputSize
)
{
	return __xrtTlsViewOverlap(
		pOutput, iOutputSize, pTicket->Nonce
	) || __xrtTlsViewOverlap(
		pOutput, iOutputSize, pTicket->Ticket
	) || __xrtTlsViewOverlap(
		pOutput, iOutputSize, pTicket->Extensions
	);
}



/* 失败原子地编码版本对应的 NewSessionTicket 正文。 */
XRT_API bool xrtTlsSessionTicketEncode(
	const xtlssessionticket* pTicket,
	void* pOutput,
	size_t iOutputSize
)
{
	uint8* pWrite = (uint8*)pOutput;
	size_t iRequired = xrtTlsSessionTicketSize(pTicket);
	size_t iOffset = 0;
	xtlssessionticket Ticket;

	if ( iRequired == 0 ) {
		return false;
	}
	if ( pOutput == NULL ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-session-ticket",
			"TLS NewSessionTicket output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( iOutputSize < iRequired ) {
		__xrtTlsError(
			XERR_RANGE, XTLS_ERROR_RESUME, "write-session-ticket",
			"TLS NewSessionTicket output buffer is too small", SIZE_MAX
		);
		return false;
	}
	Ticket = *pTicket;
	if ( __xrtTlsSessionTicketOverlap(
		&Ticket, pOutput, iRequired
	) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "write-session-ticket",
			"TLS NewSessionTicket input overlaps its output", SIZE_MAX
		);
		return false;
	}

	__xrtTlsWrite32(pWrite + iOffset, Ticket.Lifetime);
	iOffset += 4u;
	if ( Ticket.Version == XTLS_VERSION_12 ) {
		__xrtTlsWrite16(pWrite + iOffset, (uint16)Ticket.Ticket.Size);
		iOffset += 2u;
		if ( Ticket.Ticket.Size != 0 ) {
			memcpy(
				pWrite + iOffset,
				Ticket.Ticket.Data,
				Ticket.Ticket.Size
			);
		}
		return true;
	}

	/* TLS 1.3 依次编码 age_add、nonce、非空票据和扩展向量。 */
	__xrtTlsWrite32(pWrite + iOffset, Ticket.AgeAdd);
	iOffset += 4u;
	pWrite[iOffset++] = (uint8)Ticket.Nonce.Size;
	if ( Ticket.Nonce.Size != 0 ) {
		memcpy(
			pWrite + iOffset,
			Ticket.Nonce.Data,
			Ticket.Nonce.Size
		);
		iOffset += Ticket.Nonce.Size;
	}
	__xrtTlsWrite16(pWrite + iOffset, (uint16)Ticket.Ticket.Size);
	iOffset += 2u;
	memcpy(
		pWrite + iOffset,
		Ticket.Ticket.Data,
		Ticket.Ticket.Size
	);
	iOffset += Ticket.Ticket.Size;
	__xrtTlsWrite16(pWrite + iOffset, (uint16)Ticket.Extensions.Size);
	iOffset += 2u;
	if ( Ticket.Extensions.Size != 0 ) {
		memcpy(
			pWrite + iOffset,
			Ticket.Extensions.Data,
			Ticket.Extensions.Size
		);
	}
	return true;
}

#endif
