#include "../../../dev/bench/bench_common.h"

#define XMAIL_MODULE_XMAIL
#include <xmail.h>




/* 只累计同步写出字节，避免基准引入额外复制和分配。 */
static bool benchMailWrite(xbytesview Data, ptr pUserData)
{
	size_t* pWritten = (size_t*)pUserData;

	if ( Data.Size > (SIZE_MAX - *pWritten) ) {
		return false;
	}
	*pWritten += Data.Size;
	return true;
}



/* 测量无分配邮件编码与字段游标热路径。 */
int main(int argc, char** argv)
{
	static const char sBody[] =
		"Content streaming keeps the protocol path explicit. "
		"UTF-8 mail body: xlang standard library.\n";
	static const char sHeaders[] =
		"From: sender@example.com\r\n"
		"To: receiver@example.com\r\n"
		"Subject: benchmark\r\n"
		"Received: first\r\n"
		"Received: second\r\n\r\n";
	static const char sWords[] =
		"=?UTF-8?B?5Lit5paH6YKu5Lu2?= "
		"=?US-ASCII?Q?benchmark?=";
	static const char sAddresses[] =
		"Team: A <a@example.com>, \"B, User\" <b@example.com>;, "
		"next@[IPv6:2001:db8::1]";
	static const char sParameters[] =
		"; filename*0*=UTF-8'en'%E4%B8%AD"
		"; filename*1*=%E6%96%87.txt; size=1024";
	static const char sMultipart[] =
		"--bench\r\nContent-Type: text/plain\r\n\r\none\r\n"
		"--bench\r\n\r\ntwo\r\n--bench--\r\n";
	static const char sMessage[] =
		"From: sender@example.com\r\n"
		"To: receiver@example.com\r\n"
		"Content-Transfer-Encoding: base64\r\n"
		"\r\n"
		"aGVsbG8=\r\n";
	static const unsigned char arrAttachment[] = {
		0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
		0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0
	};
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 100000u);
	char arrOutput[512];
	xmailaddress ComposeTo;
	xmailattachment ComposeAttachment;
	xmailmessage ComposeMessage;
	size_t iOutputSize;
	uint64 iChecksum = 0;
	xbenchtimer Timer;
	uint64 iQpElapsed;
	uint64 iBase64Elapsed;
	uint64 iHeaderElapsed;
	uint64 iWordElapsed;
	uint64 iAddressElapsed;
	uint64 iParamElapsed;
	uint64 iMultipartElapsed;
	uint64 iMessageElapsed;
	uint64 iTreeElapsed;
	uint64 iBuilderElapsed;
	uint64 iComposeElapsed;

	if ( iIterations == 0 ) {
		return 1;
	}
	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		if ( !xrtMailQpWrite(
			sBody,
			sizeof(sBody) - 1u,
			0,
			XMAIL_QP_TEXT,
			arrOutput,
			sizeof(arrOutput),
			&iOutputSize
		) ) {
			return 2;
		}
		iChecksum += iOutputSize;
	}
	xbenchTimerStop(&Timer);
	iQpElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		if ( !xrtMailBase64Write(
			sBody,
			sizeof(sBody) - 1u,
			0,
			arrOutput,
			sizeof(arrOutput),
			&iOutputSize
		) ) {
			return 3;
		}
		iChecksum += iOutputSize;
	}
	xbenchTimerStop(&Timer);
	iBase64Elapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xmailheadercursor Cursor;
		xmailheaderview Header;
		xmailnext Next;

		if ( !xrtMailHeaderCursorInit(&Cursor, XRT_STR_LITERAL(sHeaders)) ) {
			return 4;
		}
		while ( (Next = xrtMailHeaderNext(&Cursor, &Header)) == XMAIL_NEXT_ITEM ) {
			iChecksum += Header.Name.Size + Header.Value.Size;
		}
		if ( Next != XMAIL_NEXT_END ) {
			return 5;
		}
	}
	xbenchTimerStop(&Timer);
	iHeaderElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		if ( !xrtMailWordDecodeWrite(
			XRT_STR_LITERAL(sWords),
			XMAIL_WORD_STRICT,
			arrOutput,
			sizeof(arrOutput),
			&iOutputSize
		) ) {
			return 6;
		}
		iChecksum += iOutputSize;
	}
	xbenchTimerStop(&Timer);
	iWordElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xmailaddresscursor Cursor;
		xmailaddressview Address;
		xmailnext Next;

		if ( !xrtMailAddressCursorInit(
			&Cursor,
			XRT_STR_LITERAL(sAddresses),
			XMAIL_ADDRESS_DEFAULT
		) ) {
			return 7;
		}
		while ( (Next = xrtMailAddressNext(&Cursor, &Address)) ==
			XMAIL_NEXT_ITEM ) {
			iChecksum += Address.Source.Size;
		}
		if ( Next != XMAIL_NEXT_END ) {
			return 8;
		}
	}
	xbenchTimerStop(&Timer);
	iAddressElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xmailparaminfo Info;

		if ( xrtMailParamFindWrite(
			XRT_STR_LITERAL(sParameters),
			XRT_STR_LITERAL("filename"),
			arrOutput,
			sizeof(arrOutput),
			&iOutputSize,
			&Info
		) != XMAIL_NEXT_ITEM ) {
			return 9;
		}
		iChecksum += iOutputSize + Info.Sections;
	}
	xbenchTimerStop(&Timer);
	iParamElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xmailmultipartcursor Cursor;
		xmailmultipartview Part;
		xmailnext Next;

		if ( !xrtMailMultipartCursorInit(
			&Cursor,
			XRT_STR_LITERAL(sMultipart),
			XRT_STR_LITERAL("bench"),
			0
		) ) {
			return 10;
		}
		while ( (Next = xrtMailMultipartNext(&Cursor, &Part)) ==
			XMAIL_NEXT_ITEM ) {
			iChecksum += Part.Headers.Size + Part.Body.Size;
		}
		if ( Next != XMAIL_NEXT_END ) {
			return 11;
		}
	}
	xbenchTimerStop(&Timer);
	iMultipartElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xmailmessageview Message;
		xmailtransfer Transfer;

		if ( !xrtMailMessageParse(
			XRT_STR_LITERAL(sMessage),
			0,
			0,
			&Message
		) || !xrtMailMessageTransfer(&Message, &Transfer) ) {
			return 12;
		}
		iChecksum += Message.HeaderCount + Message.Body.Size + (uint64)Transfer;
	}
	xbenchTimerStop(&Timer);
	iMessageElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xmailtree Tree;

		if ( !xrtMailTreeParse(XRT_STR_LITERAL(sMessage), NULL, &Tree) ) {
			return 13;
		}
		iChecksum += Tree.PartCount + Tree.Root->Data.Size;
		xrtMailTreeFree(&Tree);
	}
	xbenchTimerStop(&Timer);
	iTreeElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xmailbuilder Builder;
		size_t iWritten = 0;

		if ( !xrtMailBuilderInit(&Builder, benchMailWrite, &iWritten) ||
			 !xrtMailBuilderHeader(
				&Builder,
				XRT_STR_LITERAL("Content-Type"),
				XRT_STR_LITERAL("text/plain; charset=UTF-8"),
				0
			 ) || !xrtMailBuilderHeadersEnd(&Builder) ||
			 !xrtMailBuilderBody(&Builder, sBody, sizeof(sBody) - 1u) ||
			 !xrtMailBuilderFinish(&Builder) ) {
			return 14;
		}
		iChecksum += iWritten;
	}
	xbenchTimerStop(&Timer);
	iBuilderElapsed = xbenchTimerElapsedNs(&Timer);

	xrtMailMessageInit(&ComposeMessage);
	ComposeMessage.From = (xmailaddress){
		XRT_STR_LITERAL("Sender"),
		XRT_STR_LITERAL("sender@example.com")
	};
	ComposeTo = (xmailaddress){
		XRT_STR_LITERAL("Receiver"),
		XRT_STR_LITERAL("receiver@example.net")
	};
	ComposeAttachment = (xmailattachment){
		XRT_STR_LITERAL("bench.bin"),
		XRT_STR_LITERAL("application/octet-stream"),
		XRT_STR_LITERAL(""),
		{ arrAttachment, sizeof(arrAttachment) },
		false
	};
	ComposeMessage.To = &ComposeTo;
	ComposeMessage.ToCount = 1u;
	ComposeMessage.Subject = XRT_STR_LITERAL("benchmark");
	ComposeMessage.Text = XRT_STR_LITERAL(sBody);
	ComposeMessage.Attachments = &ComposeAttachment;
	ComposeMessage.AttachmentCount = 1u;
	ComposeMessage.Date = XRT_STR_LITERAL("Sun, 16 Aug 2026 12:00:00 +0800");
	ComposeMessage.MessageId = XRT_STR_LITERAL("<bench@example.com>");
	ComposeMessage.MixedBoundary = XRT_STR_LITERAL("xmail-bench-boundary");
	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		size_t iWritten = 0;
		size_t iReported;

		if ( !xrtMailComposeWrite(
			&ComposeMessage,
			benchMailWrite,
			&iWritten,
			&iReported
		) || (iWritten != iReported) ) {
			return 15;
		}
		iChecksum += iWritten;
	}
	xbenchTimerStop(&Timer);
	iComposeElapsed = xbenchTimerElapsedNs(&Timer);

	printf("xrt Mail content benchmark\n");
	xbenchPrintMetricDouble(
		"qp_mib_per_sec",
		xbenchSafeRate(
			(uint64)iIterations * (sizeof(sBody) - 1u),
			iQpElapsed
		) / (1024.0 * 1024.0)
	);
	xbenchPrintMetricDouble(
		"base64_mib_per_sec",
		xbenchSafeRate(
			(uint64)iIterations * (sizeof(sBody) - 1u),
			iBase64Elapsed
		) / (1024.0 * 1024.0)
	);
	xbenchPrintMetricDouble(
		"header_ops_per_sec",
		xbenchSafeRate(iIterations, iHeaderElapsed)
	);
	xbenchPrintMetricDouble(
		"word_ops_per_sec",
		xbenchSafeRate(iIterations, iWordElapsed)
	);
	xbenchPrintMetricDouble(
		"address_ops_per_sec",
		xbenchSafeRate(iIterations, iAddressElapsed)
	);
	xbenchPrintMetricDouble(
		"param_ops_per_sec",
		xbenchSafeRate(iIterations, iParamElapsed)
	);
	xbenchPrintMetricDouble(
		"multipart_ops_per_sec",
		xbenchSafeRate(iIterations, iMultipartElapsed)
	);
	xbenchPrintMetricDouble(
		"message_ops_per_sec",
		xbenchSafeRate(iIterations, iMessageElapsed)
	);
	xbenchPrintMetricDouble(
		"tree_ops_per_sec",
		xbenchSafeRate(iIterations, iTreeElapsed)
	);
	xbenchPrintMetricDouble(
		"builder_ops_per_sec",
		xbenchSafeRate(iIterations, iBuilderElapsed)
	);
	xbenchPrintMetricDouble(
		"compose_ops_per_sec",
		xbenchSafeRate(iIterations, iComposeElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	return 0;
}
