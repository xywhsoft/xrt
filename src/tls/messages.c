#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_MESSAGES)

/* 报告握手消息语义错误并返回 false。 */
static bool __xrtTlsMessageError(
	xtlserror Code,
	cstr sOperation,
	cstr sMessage,
	size_t iOffset
)
{
	__xrtTlsError(
		XERR_PROTOCOL, Code, sOperation, sMessage, iOffset
	);
	return false;
}



/* 检查消息编解码层接受的协议版本。 */
static bool __xrtTlsMessageVersion(
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



/* 验证完整证书条目向量，不限制条目数量。 */
static bool __xrtTlsCertificateListValid(
	xtlsversion Version,
	xbytesview Entries
)
{
	xtlscertificatecursor Cursor;
	xtlscertificateentry Entry;
	xtlsitemresult Result;

	Cursor.Version = Version;
	Cursor.Data = Entries;
	Cursor.Offset = 0;
	do {
		Result = xrtTlsCertificatesRead(&Cursor, &Entry);
	} while ( Result == XTLS_ITEM_VALUE );
	return Result == XTLS_ITEM_DONE;
}



/* 严格解析 TLS 1.2 或 TLS 1.3 Certificate 正文。 */
XRT_API bool xrtTlsCertificateParse(
	xtlsversion Version,
	xbytesview Body,
	xtlscertificatemessage* pMessage
)
{
	xtlscertificatemessage Message;
	size_t iOffset = 0;
	size_t iListSize;

	if ( (pMessage == NULL) || !__xrtTlsViewValid(Body) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-certificate-message",
			"TLS Certificate input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( !__xrtTlsMessageVersion(Version, "parse-certificate-message") ) {
		return false;
	}
	if ( Body.Size > XTLS_HANDSHAKE_BODY_MAX ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_LIMIT, "parse-certificate-message",
			"TLS Certificate body exceeds the 24-bit wire limit", SIZE_MAX
		);
	}
	memset(&Message, 0, sizeof(Message));
	Message.Version = Version;

	/* TLS 1.3 在证书列表前携带一字节请求上下文。 */
	if ( Version == XTLS_VERSION_13 ) {
		size_t iContextSize;

		if ( Body.Size < 1u ) {
			return __xrtTlsMessageError(
				XTLS_ERROR_CERTIFICATE, "parse-certificate-message",
				"TLS 1.3 Certificate request context is truncated", 0
			);
		}
		iContextSize = Body.Data[iOffset++];
		if ( iContextSize > Body.Size - iOffset ) {
			return __xrtTlsMessageError(
				XTLS_ERROR_CERTIFICATE, "parse-certificate-message",
				"TLS 1.3 Certificate request context is truncated", iOffset
			);
		}
		Message.RequestContext.Data = Body.Data + iOffset;
		Message.RequestContext.Size = iContextSize;
		iOffset += iContextSize;
	}

	/* 两个版本都使用 24 位证书列表长度，且必须恰好消费正文。 */
	if ( Body.Size - iOffset < 3u ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_CERTIFICATE, "parse-certificate-message",
			"TLS Certificate list length is truncated", iOffset
		);
	}
	iListSize = __xrtTlsRead24(Body.Data + iOffset);
	iOffset += 3u;
	if ( iListSize != Body.Size - iOffset ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_CERTIFICATE, "parse-certificate-message",
			"TLS Certificate list length is inconsistent", iOffset - 3u
		);
	}
	Message.Entries.Data = Body.Data + iOffset;
	Message.Entries.Size = iListSize;
	if ( !__xrtTlsCertificateListValid(Version, Message.Entries) ) {
		return false;
	}
	*pMessage = Message;
	return true;
}



/* 从已经严格解析的消息初始化证书游标。 */
XRT_API bool xrtTlsCertificateEntries(
	const xtlscertificatemessage* pMessage,
	xtlscertificatecursor* pCursor
)
{
	xtlscertificatecursor Cursor;

	if ( (pMessage == NULL) || (pCursor == NULL) ||
		!__xrtTlsViewValid(pMessage != NULL ?
			pMessage->RequestContext : (xbytesview) { NULL, 1u }) ||
		!__xrtTlsViewValid(pMessage != NULL ?
			pMessage->Entries : (xbytesview) { NULL, 1u }) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "init-certificate-entries",
			"TLS Certificate message or cursor is invalid", SIZE_MAX
		);
		return false;
	}
	if ( !__xrtTlsMessageVersion(
		pMessage->Version, "init-certificate-entries"
	) ) {
		return false;
	}
	if ( (pMessage->Version == XTLS_VERSION_12) &&
		(pMessage->RequestContext.Size != 0) ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_CERTIFICATE, "init-certificate-entries",
			"TLS 1.2 Certificate cannot carry a request context", SIZE_MAX
		);
	}
	if ( ((pMessage->Version == XTLS_VERSION_13) &&
		 (pMessage->RequestContext.Size > UINT8_MAX)) ||
		(pMessage->Entries.Size > XTLS_HANDSHAKE_BODY_MAX) ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_CERTIFICATE, "init-certificate-entries",
			"TLS Certificate message fields exceed their wire limits", SIZE_MAX
		);
	}
	Cursor.Version = pMessage->Version;
	Cursor.Data = pMessage->Entries;
	Cursor.Offset = 0;
	*pCursor = Cursor;
	return true;
}



/* 读取下一证书条目并严格验证其版本专用尾部。 */
XRT_API xtlsitemresult xrtTlsCertificatesRead(
	xtlscertificatecursor* pCursor,
	xtlscertificateentry* pEntry
)
{
	xtlscertificatecursor Cursor;
	xtlscertificateentry Entry;
	size_t iRemaining;
	size_t iCertificateSize;
	size_t iExtensionSize;

	if ( (pCursor == NULL) || (pEntry == NULL) ||
		!__xrtTlsViewValid(pCursor != NULL ?
			pCursor->Data : (xbytesview) { NULL, 1u }) ||
		((pCursor != NULL) && (pCursor->Offset > pCursor->Data.Size)) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "read-certificates",
			"TLS Certificate cursor or output is invalid", SIZE_MAX
		);
		return XTLS_ITEM_ERROR;
	}
	if ( !__xrtTlsMessageVersion(pCursor->Version, "read-certificates") ) {
		return XTLS_ITEM_ERROR;
	}
	if ( pCursor->Data.Size > XTLS_HANDSHAKE_BODY_MAX ) {
		__xrtTlsMessageError(
			XTLS_ERROR_LIMIT, "read-certificates",
			"TLS Certificate list exceeds the 24-bit wire limit", SIZE_MAX
		);
		return XTLS_ITEM_ERROR;
	}
	if ( pCursor->Offset == pCursor->Data.Size ) {
		return XTLS_ITEM_DONE;
	}

	Cursor = *pCursor;
	memset(&Entry, 0, sizeof(Entry));
	iRemaining = Cursor.Data.Size - Cursor.Offset;
	if ( iRemaining < 3u ) {
		__xrtTlsMessageError(
			XTLS_ERROR_CERTIFICATE, "read-certificates",
			"TLS Certificate entry length is truncated", Cursor.Offset
		);
		return XTLS_ITEM_ERROR;
	}
	iCertificateSize = __xrtTlsRead24(
		Cursor.Data.Data + Cursor.Offset
	);
	if ( (iCertificateSize == 0) ||
		(iCertificateSize > iRemaining - 3u) ) {
		__xrtTlsMessageError(
			XTLS_ERROR_CERTIFICATE, "read-certificates",
			"TLS Certificate entry is empty or truncated", Cursor.Offset
		);
		return XTLS_ITEM_ERROR;
	}
	Cursor.Offset += 3u;
	Entry.Data.Data = Cursor.Data.Data + Cursor.Offset;
	Entry.Data.Size = iCertificateSize;
	Cursor.Offset += iCertificateSize;

	/* TLS 1.3 每个证书条目还携带独立的 16 位扩展向量。 */
	if ( Cursor.Version == XTLS_VERSION_13 ) {
		iRemaining = Cursor.Data.Size - Cursor.Offset;
		if ( iRemaining < 2u ) {
			__xrtTlsMessageError(
				XTLS_ERROR_CERTIFICATE, "read-certificates",
				"TLS 1.3 Certificate entry extensions are truncated",
				Cursor.Offset
			);
			return XTLS_ITEM_ERROR;
		}
		iExtensionSize = __xrtTlsRead16(
			Cursor.Data.Data + Cursor.Offset
		);
		if ( iExtensionSize > iRemaining - 2u ) {
			__xrtTlsMessageError(
				XTLS_ERROR_CERTIFICATE, "read-certificates",
				"TLS 1.3 Certificate entry extensions are truncated",
				Cursor.Offset
			);
			return XTLS_ITEM_ERROR;
		}
		Cursor.Offset += 2u;
		Entry.Extensions.Data = Cursor.Data.Data + Cursor.Offset;
		Entry.Extensions.Size = iExtensionSize;
		if ( !xrtTlsExtensionsValidate(Entry.Extensions) ) {
			return XTLS_ITEM_ERROR;
		}
		Cursor.Offset += iExtensionSize;
	}
	*pCursor = Cursor;
	*pEntry = Entry;
	return XTLS_ITEM_VALUE;
}



/* 验证 EncryptedExtensions 中已知扩展的局部线路形状。 */
bool __xrtTlsEncryptedExtensionsValid(xbytesview Extensions)
{
	xtlsextensioncursor Cursor;
	xtlsextension Extension;
	xtlsitemresult Result;

	if ( !xrtTlsExtensionsInit(&Cursor, Extensions) ) {
		return false;
	}
	while ( (Result = xrtTlsExtensionsRead(
		&Cursor, &Extension
	)) == XTLS_ITEM_VALUE ) {
		switch ( Extension.Type ) {
			case XTLS_EXTENSION_SERVER_NAME:
			case XTLS_EXTENSION_EARLY_DATA:
				if ( Extension.Data.Size != 0 ) {
					return __xrtTlsMessageError(
						XTLS_ERROR_EXTENSION, "parse-encrypted-extensions",
						"TLS acknowledgement extension must be empty",
						Cursor.Offset - Extension.EncodedSize
					);
				}
				break;

			case XTLS_EXTENSION_ALPN: {
				xbytesview Protocol;

				if ( !xrtTlsProtocolSelected(Extension.Data, &Protocol) ) {
					return false;
				}
				break;
			}

			case XTLS_EXTENSION_SUPPORTED_GROUPS: {
				xtlsids Groups;

				if ( !xrtTlsGroups(Extension.Data, &Groups) ) {
					return false;
				}
				break;
			}

			case XTLS_EXTENSION_MAX_FRAGMENT_LENGTH:
				if ( (Extension.Data.Size != 1u) ||
					(Extension.Data.Data[0] < 1u) ||
					(Extension.Data.Data[0] > 4u) ) {
					return __xrtTlsMessageError(
						XTLS_ERROR_EXTENSION, "parse-encrypted-extensions",
						"TLS maximum fragment length selection is invalid",
						Cursor.Offset - Extension.EncodedSize
					);
				}
				break;

			case XTLS_EXTENSION_RECORD_SIZE_LIMIT:
				if ( (Extension.Data.Size != 2u) ||
					(__xrtTlsRead16(Extension.Data.Data) < 64u) ) {
					return __xrtTlsMessageError(
						XTLS_ERROR_EXTENSION, "parse-encrypted-extensions",
						"TLS record size limit selection is invalid",
						Cursor.Offset - Extension.EncodedSize
					);
				}
				break;

			case XTLS_EXTENSION_HEARTBEAT:
				if ( (Extension.Data.Size != 1u) ||
					(Extension.Data.Data[0] < 1u) ||
					(Extension.Data.Data[0] > 2u) ) {
					return __xrtTlsMessageError(
						XTLS_ERROR_EXTENSION, "parse-encrypted-extensions",
						"TLS heartbeat mode selection is invalid",
						Cursor.Offset - Extension.EncodedSize
					);
				}
				break;

			default:
				break;
		}
	}
	return Result == XTLS_ITEM_DONE;
}



/* 严格解析 TLS 1.3 EncryptedExtensions 正文。 */
XRT_API bool xrtTlsEncryptedExtensionsParse(
	xbytesview Body,
	xbytesview* pExtensions
)
{
	xbytesview Extensions;
	size_t iSize;

	if ( (pExtensions == NULL) || !__xrtTlsViewValid(Body) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-encrypted-extensions",
			"TLS EncryptedExtensions input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( Body.Size < 2u ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_EXTENSION, "parse-encrypted-extensions",
			"TLS EncryptedExtensions length is truncated", 0
		);
	}
	iSize = __xrtTlsRead16(Body.Data);
	if ( iSize != Body.Size - 2u ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_EXTENSION, "parse-encrypted-extensions",
			"TLS EncryptedExtensions length is inconsistent", 0
		);
	}
	Extensions.Data = Body.Data + 2u;
	Extensions.Size = iSize;
	if ( !__xrtTlsEncryptedExtensionsValid(Extensions) ) {
		return false;
	}
	*pExtensions = Extensions;
	return true;
}



/* 严格解析 CertificateVerify 正文。 */
XRT_API bool xrtTlsCertificateVerifyParse(
	xbytesview Body,
	xtlscertificateverify* pVerify
)
{
	xtlscertificateverify Verify;
	size_t iSignatureSize;

	if ( (pVerify == NULL) || !__xrtTlsViewValid(Body) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-certificate-verify",
			"TLS CertificateVerify input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( Body.Size < 4u ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_VERIFY, "parse-certificate-verify",
			"TLS CertificateVerify body is truncated", Body.Size
		);
	}
	iSignatureSize = __xrtTlsRead16(Body.Data + 2u);
	if ( (iSignatureSize == 0) ||
		(iSignatureSize != Body.Size - 4u) ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_VERIFY, "parse-certificate-verify",
			"TLS CertificateVerify signature length is inconsistent", 2u
		);
	}
	Verify.Scheme = __xrtTlsRead16(Body.Data);
	Verify.Signature.Data = Body.Data + 4u;
	Verify.Signature.Size = iSignatureSize;
	*pVerify = Verify;
	return true;
}



/* 按协商长度严格解析 Finished 验证数据。 */
XRT_API bool xrtTlsFinishedParse(
	xbytesview Body,
	size_t iExpectedSize,
	xbytesview* pVerifyData
)
{
	if ( (pVerifyData == NULL) || !__xrtTlsViewValid(Body) ||
		(iExpectedSize == 0) ||
		(iExpectedSize > XTLS_HANDSHAKE_BODY_MAX) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-finished",
			"TLS Finished input, expected size or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( Body.Size != iExpectedSize ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_VERIFY, "parse-finished",
			"TLS Finished verify data length is inconsistent", 0
		);
	}
	*pVerifyData = Body;
	return true;
}



/* 严格解析 TLS 1.3 KeyUpdate 请求。 */
XRT_API bool xrtTlsKeyUpdateParse(
	xbytesview Body,
	xtlskeyupdate* pRequest
)
{
	if ( (pRequest == NULL) || !__xrtTlsViewValid(Body) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-key-update",
			"TLS KeyUpdate input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( (Body.Size != 1u) ||
		(Body.Data[0] > XTLS_KEY_UPDATE_REQUESTED) ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_HANDSHAKE, "parse-key-update",
			"TLS KeyUpdate request value is invalid", 0
		);
	}
	*pRequest = (xtlskeyupdate)Body.Data[0];
	return true;
}



/* 验证 TLS 1.3 票据扩展的已知局部语义。 */
bool __xrtTlsTicketExtensionsValid(xbytesview Extensions)
{
	xtlsextensioncursor Cursor;
	xtlsextension Extension;
	xtlsitemresult Result;

	if ( !xrtTlsExtensionsInit(&Cursor, Extensions) ) {
		return false;
	}
	while ( (Result = xrtTlsExtensionsRead(
		&Cursor, &Extension
	)) == XTLS_ITEM_VALUE ) {
		if ( (Extension.Type == XTLS_EXTENSION_EARLY_DATA) &&
			(Extension.Data.Size != 4u) ) {
			return __xrtTlsMessageError(
				XTLS_ERROR_EXTENSION, "parse-session-ticket",
				"TLS ticket early-data limit must contain four bytes",
				Cursor.Offset - Extension.EncodedSize
			);
		}
	}
	return Result == XTLS_ITEM_DONE;
}



/* 严格解析 TLS 1.2 NewSessionTicket 正文。 */
static bool __xrtTls12SessionTicket(
	xbytesview Body,
	xtlssessionticket* pTicket
)
{
	xtlssessionticket Ticket;
	size_t iTicketSize;

	if ( Body.Size < 6u ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_RESUME, "parse-session-ticket",
			"TLS 1.2 NewSessionTicket body is truncated", Body.Size
		);
	}
	iTicketSize = __xrtTlsRead16(Body.Data + 4u);
	if ( iTicketSize != Body.Size - 6u ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_RESUME, "parse-session-ticket",
			"TLS 1.2 ticket length is inconsistent", 4u
		);
	}
	memset(&Ticket, 0, sizeof(Ticket));
	Ticket.Version = XTLS_VERSION_12;
	Ticket.Lifetime = __xrtTlsRead32(Body.Data);
	Ticket.Ticket.Data = Body.Data + 6u;
	Ticket.Ticket.Size = iTicketSize;
	*pTicket = Ticket;
	return true;
}



/* 严格解析 TLS 1.3 NewSessionTicket 正文。 */
static bool __xrtTls13SessionTicket(
	xbytesview Body,
	xtlssessionticket* pTicket
)
{
	xtlssessionticket Ticket;
	size_t iOffset = 0;
	size_t iNonceSize;
	size_t iTicketSize;
	size_t iExtensionSize;

	if ( Body.Size < 13u ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_RESUME, "parse-session-ticket",
			"TLS 1.3 NewSessionTicket body is truncated", Body.Size
		);
	}
	memset(&Ticket, 0, sizeof(Ticket));
	Ticket.Version = XTLS_VERSION_13;
	Ticket.Lifetime = __xrtTlsRead32(Body.Data + iOffset);
	iOffset += 4u;
	if ( Ticket.Lifetime > XTLS13_TICKET_LIFETIME_MAX ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_RESUME, "parse-session-ticket",
			"TLS 1.3 ticket lifetime exceeds seven days", 0
		);
	}
	Ticket.AgeAdd = __xrtTlsRead32(Body.Data + iOffset);
	iOffset += 4u;
	iNonceSize = Body.Data[iOffset++];
	if ( iNonceSize > Body.Size - iOffset ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_RESUME, "parse-session-ticket",
			"TLS 1.3 ticket nonce is truncated", iOffset - 1u
		);
	}
	Ticket.Nonce.Data = Body.Data + iOffset;
	Ticket.Nonce.Size = iNonceSize;
	iOffset += iNonceSize;
	if ( Body.Size - iOffset < 2u ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_RESUME, "parse-session-ticket",
			"TLS 1.3 ticket length is truncated", iOffset
		);
	}
	iTicketSize = __xrtTlsRead16(Body.Data + iOffset);
	iOffset += 2u;
	if ( (iTicketSize == 0) || (iTicketSize > Body.Size - iOffset) ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_RESUME, "parse-session-ticket",
			"TLS 1.3 ticket is empty or truncated", iOffset - 2u
		);
	}
	Ticket.Ticket.Data = Body.Data + iOffset;
	Ticket.Ticket.Size = iTicketSize;
	iOffset += iTicketSize;
	if ( Body.Size - iOffset < 2u ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_RESUME, "parse-session-ticket",
			"TLS 1.3 ticket extensions length is truncated", iOffset
		);
	}
	iExtensionSize = __xrtTlsRead16(Body.Data + iOffset);
	iOffset += 2u;
	if ( iExtensionSize != Body.Size - iOffset ) {
		return __xrtTlsMessageError(
			XTLS_ERROR_RESUME, "parse-session-ticket",
			"TLS 1.3 ticket extensions length is inconsistent", iOffset - 2u
		);
	}
	Ticket.Extensions.Data = Body.Data + iOffset;
	Ticket.Extensions.Size = iExtensionSize;
	if ( !__xrtTlsTicketExtensionsValid(Ticket.Extensions) ) {
		return false;
	}
	*pTicket = Ticket;
	return true;
}



/* 严格解析版本对应的 NewSessionTicket 正文。 */
XRT_API bool xrtTlsSessionTicketParse(
	xtlsversion Version,
	xbytesview Body,
	xtlssessionticket* pTicket
)
{
	if ( (pTicket == NULL) || !__xrtTlsViewValid(Body) ) {
		__xrtTlsError(
			XERR_ARGUMENT, XTLS_ERROR_ARGUMENT, "parse-session-ticket",
			"TLS NewSessionTicket input or output is invalid", SIZE_MAX
		);
		return false;
	}
	if ( !__xrtTlsMessageVersion(Version, "parse-session-ticket") ) {
		return false;
	}
	if ( Version == XTLS_VERSION_12 ) {
		return __xrtTls12SessionTicket(Body, pTicket);
	}
	return __xrtTls13SessionTicket(Body, pTicket);
}

#endif
