#include <stdlib.h>

#include <xrt/error.h>
#include <xrt/tls.h>



#define XRT_TLS_FUZZ_INPUT_MAX ((size_t)1048576u)
#define XRT_TLS_FUZZ_READER_LIMIT ((size_t)65536u)



/* 判断借用字节视图是否完整位于指定输入中。 */
static bool __xrtTlsFuzzViewInside(
	xbytesview Input,
	xbytesview View
)
{
	uintptr_t iInput;
	uintptr_t iView;

	if ( View.Data == NULL ) {
		return View.Size == 0;
	}
	if ( Input.Data == NULL ) {
		return false;
	}
	iInput = (uintptr_t)Input.Data;
	iView = (uintptr_t)View.Data;
	return (iView >= iInput) &&
		((iView - iInput) <= Input.Size) &&
		(View.Size <= (Input.Size - (iView - iInput)));
}



/* 把完整握手正文交给对应的严格消息解析器。 */
static void __xrtTlsFuzzMessage(const xtlshandshake* pMessage)
{
	if ( pMessage->Type == XTLS_HANDSHAKE_CLIENT_HELLO ) {
		xtlsclienthello Hello;

		(void)xrtTlsClientHelloParse(pMessage->Body, &Hello);
	} else if ( pMessage->Type == XTLS_HANDSHAKE_SERVER_HELLO ) {
		xtlsserverhello Hello;

		(void)xrtTlsServerHelloParse(pMessage->Body, &Hello);
	} else if ( pMessage->Type == XTLS_HANDSHAKE_CERTIFICATE ) {
		xtlscertificatemessage Certificate;

		(void)xrtTlsCertificateParse(
			XTLS_VERSION_12, pMessage->Body, &Certificate
		);
		xrtClearError();
		(void)xrtTlsCertificateParse(
			XTLS_VERSION_13, pMessage->Body, &Certificate
		);
	} else if ( pMessage->Type == XTLS_HANDSHAKE_CERTIFICATE_VERIFY ) {
		xtlscertificateverify Verify;

		(void)xrtTlsCertificateVerifyParse(pMessage->Body, &Verify);
	} else if ( pMessage->Type == XTLS_HANDSHAKE_CERTIFICATE_REQUEST ) {
		xtls12certificaterequest Request12;
		xtls13certificaterequest Request13;

		(void)xrtTls12CertificateRequestParse(
			pMessage->Body, &Request12
		);
		xrtClearError();
		(void)xrtTls13CertificateRequestParse(
			pMessage->Body, &Request13
		);
	} else if ( pMessage->Type == XTLS_HANDSHAKE_CERTIFICATE_STATUS ) {
		xtlscertificatestatusmessage Status;

		(void)xrtTlsCertificateStatusParse(pMessage->Body, &Status);
	} else if ( pMessage->Type == XTLS_HANDSHAKE_COMPRESSED_CERTIFICATE ) {
		xtlscompressedcertificate Certificate;

		(void)xrtTlsCompressedCertificateParse(
			pMessage->Body, &Certificate
		);
	}
	xrtClearError();
}



/* 覆盖记录和单片握手解析，并验证所有成功视图的范围。 */
static void __xrtTlsFuzzDirect(xbytesview Input)
{
	xtlsrecord Record;
	xtlshandshake Message;
	size_t iRequired = 0;
	xtlsresult Result;

	Result = xrtTlsRecordParse(Input, &Record, &iRequired);
	if ( Result == XTLS_OK ) {
		if ( (Record.EncodedSize > Input.Size) ||
			!__xrtTlsFuzzViewInside(Input, Record.Payload) ) {
			abort();
		}
		if ( Record.Type == XTLS_RECORD_HANDSHAKE ) {
			Result = xrtTlsHandshakeParse(
				Record.Payload, &Message, &iRequired
			);
			if ( Result == XTLS_OK ) {
				if ( (Message.EncodedSize > Record.Payload.Size) ||
					!__xrtTlsFuzzViewInside(
						Record.Payload, Message.Body
					) ) {
					abort();
				}
				__xrtTlsFuzzMessage(&Message);
			}
		}
	}
	xrtClearError();

	Result = xrtTlsHandshakeParse(Input, &Message, &iRequired);
	if ( Result == XTLS_OK ) {
		if ( (Message.EncodedSize > Input.Size) ||
			!__xrtTlsFuzzViewInside(Input, Message.Body) ) {
			abort();
		}
		__xrtTlsFuzzMessage(&Message);
	}
	xrtClearError();
}



/* 用确定性小分片覆盖握手 reader 的重组、错误和重置路径。 */
static void __xrtTlsFuzzReader(xbytesview Input)
{
	xtlshandshakereaderconfig Config;
	xtlshandshakereader Reader;
	xtlshandshake Message;
	size_t iOffset = 0;
	size_t iGuard = 0;
	uint8 iSelector = Input.Size == 0 ? 0 : Input.Data[0];

	xrtTlsHandshakeReaderConfigInit(&Config);
	Config.Limit = XRT_TLS_FUZZ_READER_LIMIT;
	Config.Retain = 256u;
	if ( !xrtTlsHandshakeReaderInit(&Reader, &Config) ) {
		abort();
	}
	while ( (iOffset < Input.Size) && (iGuard++ <= (Input.Size + 8u)) ) {
		size_t iChunk = 1u + (size_t)(
			(iSelector + (uint8)iGuard) % 31u
		);
		size_t iConsumed = 0;
		xtlsresult Result;

		if ( iChunk > (Input.Size - iOffset) ) {
			iChunk = Input.Size - iOffset;
		}
		Result = xrtTlsHandshakeReaderRead(
			&Reader,
			(xbytesview) { Input.Data + iOffset, iChunk },
			&iConsumed,
			&Message
		);
		if ( iConsumed > iChunk ) {
			abort();
		}
		iOffset += iConsumed;
		if ( Result == XTLS_OK ) {
			if ( (Message.EncodedSize < XTLS_HANDSHAKE_HEADER_SIZE) ||
				(Message.EncodedSize >
				 (XRT_TLS_FUZZ_READER_LIMIT + XTLS_HANDSHAKE_HEADER_SIZE)) ) {
				abort();
			}
			__xrtTlsFuzzMessage(&Message);
			if ( !xrtTlsHandshakeReaderReset(&Reader) ) {
				abort();
			}
			continue;
		}
		if ( Result == XTLS_ERROR ) {
			break;
		}
		if ( (Result != XTLS_AGAIN) ||
			((iConsumed == 0) && (iChunk != 0)) ) {
			abort();
		}
	}
	if ( iGuard > (Input.Size + 9u) ) {
		abort();
	}
	xrtTlsHandshakeReaderUnit(&Reader);
	xrtClearError();
}



/* 统一公开确定性回归和 libFuzzer 使用的 TLS 协议入口。 */
int xrtTlsFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	xbytesview Input;

	if ( ((pData == NULL) && (iSize != 0)) ||
		(iSize > XRT_TLS_FUZZ_INPUT_MAX) ) {
		return 0;
	}
	Input.Data = pData;
	Input.Size = iSize;
	__xrtTlsFuzzDirect(Input);
	if ( iSize <= XRT_TLS_FUZZ_READER_LIMIT ) {
		__xrtTlsFuzzReader(Input);
	}
	return 0;
}



#if defined(XRT_TLS_FUZZ_LIBFUZZER)

/* 把独立 TLS 入口适配为 Clang/libFuzzer 约定符号。 */
int LLVMFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	return xrtTlsFuzzerTestOneInput(pData, iSize);
}

#endif
