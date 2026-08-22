#include "../test.h"



typedef struct testmailcomposesink {
	char Data[131072];
	size_t Size;
	size_t Calls;
	size_t FailAfter;
} testmailcomposesink;



/* 收集 Compose 的同步输出，并支持确定性的回调失败注入。 */
static bool testMailComposeWrite(xbytesview Data, ptr pUserData)
{
	testmailcomposesink* pSink = (testmailcomposesink*)pUserData;

	if ( (pSink->FailAfter != 0) && (pSink->Calls >= pSink->FailAfter) ) {
		return false;
	}
	if ( Data.Size > (sizeof(pSink->Data) - pSink->Size - 1u) ) {
		return false;
	}
	memcpy(pSink->Data + pSink->Size, Data.Data, Data.Size);
	pSink->Size += Data.Size;
	pSink->Calls++;
	pSink->Data[pSink->Size] = 0;
	return true;
}



/* 初始化覆盖 mixed、alternative、related、Bcc 和自定义字段的消息。 */
static void testMailComposeMessage(
	xmailmessage* pMessage,
	xmailaddress* pTo,
	xmailaddress* pBcc,
	xmailattachment* pAttachments,
	unsigned char* pLargeData,
	size_t iLargeSize,
	xmailheaderview* pHeaders
)
{
	static const unsigned char arrLogo[] = "img";

	xrtMailMessageInit(pMessage);
	pMessage->From = (xmailaddress){
		XRT_STR_LITERAL("Sender"),
		XRT_STR_LITERAL("sender@example.com")
	};
	pTo[0] = (xmailaddress){
		XRT_STR_LITERAL("Receiver"),
		XRT_STR_LITERAL("receiver@example.com")
	};
	pBcc[0] = (xmailaddress){
		XRT_STR_LITERAL("Secret"),
		XRT_STR_LITERAL("secret@example.net")
	};
	pMessage->To = pTo;
	pMessage->ToCount = 1u;
	pMessage->Bcc = pBcc;
	pMessage->BccCount = 1u;
	pMessage->Subject = XRT_STR_LITERAL("Hello 中文");
	pMessage->Text = XRT_STR_LITERAL("plain 中文");
	pMessage->Html = XRT_STR_LITERAL("<p>html</p>");
	pAttachments[0] = (xmailattachment){
		XRT_STR_LITERAL("logo.png"),
		XRT_STR_LITERAL("image/png"),
		XRT_STR_LITERAL("logo@example.com"),
		{ arrLogo, sizeof(arrLogo) - 1u },
		true
	};
	pAttachments[1] = (xmailattachment){
		XRT_STR_LITERAL("报告 1.bin"),
		XRT_STR_LITERAL("application/octet-stream"),
		XRT_STR_LITERAL(""),
		{ pLargeData, iLargeSize },
		false
	};
	pMessage->Attachments = pAttachments;
	pMessage->AttachmentCount = 2u;
	pHeaders[0] = (xmailheaderview){
		XRT_STR_LITERAL("X-Mailer"),
		XRT_STR_LITERAL("xmail-compose-test")
	};
	pMessage->Headers = pHeaders;
	pMessage->HeaderCount = 1u;
	pMessage->Date = XRT_STR_LITERAL("Tue, 12 May 2026 10:00:00 +0000");
	pMessage->MessageId = XRT_STR_LITERAL("<fixed@example.com>");
	pMessage->MixedBoundary = XRT_STR_LITERAL("mix-fixed");
	pMessage->AlternativeBoundary = XRT_STR_LITERAL("alt-fixed");
	pMessage->RelatedBoundary = XRT_STR_LITERAL("rel-fixed");
}



/* 验证高层消息的结构、流式附件和 owned 入口完全一致。 */
static void testMailComposeComplete(void)
{
	unsigned char arrLarge[6000];
	xmailmessage Message;
	xmailaddress arrTo[1];
	xmailaddress arrBcc[1];
	xmailattachment arrAttachments[2];
	xmailheaderview arrHeaders[1];
	testmailcomposesink Sink = { 0 };
	size_t iWritten;
	size_t iOwnedSize;
	str sOwned;

	for ( size_t i = 0; i < sizeof(arrLarge); i++ ) {
		arrLarge[i] = (unsigned char)(i & 0xFFu);
	}
	testMailComposeMessage(
		&Message,
		arrTo,
		arrBcc,
		arrAttachments,
		arrLarge,
		sizeof(arrLarge),
		arrHeaders
	);
	testRequire(xrtMailComposeWrite(
		&Message,
		testMailComposeWrite,
		&Sink,
		&iWritten
	) && (iWritten == Sink.Size) && (Sink.Calls > 20u),
		"mail compose streaming output failed");
	testRequire(strstr(Sink.Data, "Date: Tue, 12 May 2026") != NULL &&
		strstr(Sink.Data, "Message-ID: <fixed@example.com>") != NULL &&
		strstr(Sink.Data, "Subject: =?UTF-8?B?") != NULL &&
		strstr(Sink.Data, "X-Mailer: xmail-compose-test") != NULL &&
		strstr(Sink.Data, "multipart/mixed; boundary=\"mix-fixed\"") != NULL &&
		strstr(Sink.Data, "multipart/alternative; boundary=\"alt-fixed\"") != NULL &&
		strstr(Sink.Data, "multipart/related; boundary=\"rel-fixed\"") != NULL &&
		strstr(Sink.Data, "Content-ID: <logo@example.com>") != NULL &&
		strstr(Sink.Data,
			"filename*=UTF-8''%E6%8A%A5%E5%91%8A%201.bin") != NULL &&
		strstr(Sink.Data, "aW1n\r\n") != NULL,
		"mail compose MIME structure mismatch");
	testRequire(strstr(Sink.Data, "Bcc:") == NULL &&
		strstr(Sink.Data, "secret@example.net") == NULL,
		"mail compose leaked Bcc into the message");
	testRequire(strstr(Sink.Data, "aW1n\r\n\r\n--rel-fixed") == NULL &&
		strstr(Sink.Data, "--rel-fixed--\r\n\r\n--alt-fixed") == NULL,
		"mail compose duplicated CRLF before nested boundaries");

	sOwned = xrtMailCompose(&Message, &iOwnedSize);
	testRequire((sOwned != NULL) && (iOwnedSize == Sink.Size) &&
		(memcmp(sOwned, Sink.Data, Sink.Size) == 0),
		"owned mail compose output differs from sink output");
	xrtFree(sOwned);
}



/* 验证空可选字段会生成 Date 和 Message-ID。 */
static void testMailComposeDefaults(void)
{
	xmailmessage Message;
	xmailaddress To;
	str sOutput;
	size_t iSize;

	xrtMailMessageInit(&Message);
	Message.From = (xmailaddress){
		XRT_STR_LITERAL(""),
		XRT_STR_LITERAL("from@example.com")
	};
	To = (xmailaddress){
		XRT_STR_LITERAL(""),
		XRT_STR_LITERAL("to@example.net")
	};
	Message.To = &To;
	Message.ToCount = 1u;
	Message.Subject = XRT_STR_LITERAL("default");
	Message.Text = XRT_STR_LITERAL("body");
	sOutput = xrtMailCompose(&Message, &iSize);
	testRequire((sOutput != NULL) && (iSize != 0) &&
		(strstr(sOutput, "Date: ") != NULL) &&
		(strstr(sOutput, "Message-ID: <") != NULL) &&
		(strstr(sOutput, "@example.com>") != NULL),
		"mail compose default metadata generation failed");
	xrtFree(sOutput);
}



/* 验证超长附件名会折叠为合法字段并可完整解析。 */
static void testMailComposeLongFileName(void)
{
	char arrFileName[1600];
	unsigned char arrData[] = { 1u, 2u, 3u };
	xmailmessage Message;
	xmailaddress To;
	xmailattachment Attachment;
	testmailcomposesink Sink = { 0 };
	xmailheadercursor Cursor;
	xmailheaderview Header;
	xmaildispositionview Disposition;
	xmailparaminfo Info;
	char arrUnfolded[4096];
	char arrDecoded[sizeof(arrFileName) + 1u];
	const char* sHeader;
	const char* sEnd;
	size_t iWritten;
	size_t iUnfoldedSize;
	size_t iDecodedSize;

	memset(arrFileName, 'n', sizeof(arrFileName));
	xrtMailMessageInit(&Message);
	Message.From = (xmailaddress){
		XRT_STR_LITERAL(""),
		XRT_STR_LITERAL("sender@example.com")
	};
	To = (xmailaddress){
		XRT_STR_LITERAL(""),
		XRT_STR_LITERAL("receiver@example.net")
	};
	Message.To = &To;
	Message.ToCount = 1u;
	Message.Subject = XRT_STR_LITERAL("long filename");
	Message.Text = XRT_STR_LITERAL("body");
	Attachment = (xmailattachment){
		testMailViewN(arrFileName, sizeof(arrFileName)),
		XRT_STR_LITERAL("application/octet-stream"),
		XRT_STR_LITERAL(""),
		{ arrData, sizeof(arrData) },
		false
	};
	Message.Attachments = &Attachment;
	Message.AttachmentCount = 1u;
	Message.Date = XRT_STR_LITERAL("Tue, 12 May 2026 10:00:00 +0000");
	Message.MessageId = XRT_STR_LITERAL("<long@example.com>");
	Message.MixedBoundary = XRT_STR_LITERAL("mix-long");
	testRequire(xrtMailComposeWrite(
		&Message,
		testMailComposeWrite,
		&Sink,
		&iWritten
	), "mail compose rejected a long attachment filename");

	sHeader = strstr(Sink.Data, "Content-Disposition: attachment");
	testRequire(sHeader != NULL,
		"mail compose omitted the long Content-Disposition field");
	sEnd = strstr(sHeader, "\r\n\r\n");
	testRequire(sEnd != NULL, "long Content-Disposition field has no terminator");
	for ( const char* sLine = sHeader; sLine < sEnd; ) {
		const char* sLineEnd = strstr(sLine, "\r\n");

		testRequire((sLineEnd != NULL) &&
			((size_t)(sLineEnd - sLine) <= XMAIL_HEADER_LINE_HARD),
			"long Content-Disposition emitted an oversized physical line");
		sLine = sLineEnd + 2u;
	}
	testRequire(xrtMailHeaderCursorInit(
		&Cursor,
		testMailViewN(sHeader, (size_t)(sEnd - sHeader) + 4u)
	) && (xrtMailHeaderNext(&Cursor, &Header) == XMAIL_NEXT_ITEM) &&
		xrtMailHeaderUnfoldWrite(
			Header.Value,
			arrUnfolded,
			sizeof(arrUnfolded),
			&iUnfoldedSize
		) && xrtMailDispositionParse(
			testMailViewN(arrUnfolded, iUnfoldedSize),
			&Disposition
		) && (xrtMailParamFindWrite(
			Disposition.Parameters,
			XRT_STR_LITERAL("filename"),
			arrDecoded,
			sizeof(arrDecoded),
			&iDecodedSize,
			&Info
		) == XMAIL_NEXT_ITEM) &&
		(iDecodedSize == sizeof(arrFileName)) &&
		(memcmp(arrDecoded, arrFileName, sizeof(arrFileName)) == 0) &&
		Info.Continued,
		"long Content-Disposition filename did not round-trip");
}



/* 验证所有描述错误都在第一个 sink 回调前失败。 */
static void testMailComposeErrors(void)
{
	unsigned char arrLarge[32] = { 0 };
	xmailmessage Message;
	xmailaddress arrTo[1];
	xmailaddress arrBcc[1];
	xmailattachment arrAttachments[2];
	xmailheaderview arrHeaders[1];
	testmailcomposesink Sink = { 0 };
	size_t iWritten = 777u;

	testMailComposeMessage(
		&Message,
		arrTo,
		arrBcc,
		arrAttachments,
		arrLarge,
		sizeof(arrLarge),
		arrHeaders
	);
	Message.ToCount = 0;
	Message.BccCount = 0;
	testRequire(!xrtMailComposeWrite(&Message, testMailComposeWrite,
		&Sink, &iWritten) && (Sink.Size == 0) && (iWritten == 777u),
		"mail compose accepted a message without recipients");

	testMailComposeMessage(
		&Message,
		arrTo,
		arrBcc,
		arrAttachments,
		arrLarge,
		sizeof(arrLarge),
		arrHeaders
	);
	arrHeaders[0].Name = XRT_STR_LITERAL("Subject");
	testRequire(!xrtMailComposeWrite(&Message, testMailComposeWrite,
		&Sink, &iWritten) && (Sink.Size == 0),
		"mail compose accepted a managed custom header");

	testMailComposeMessage(
		&Message,
		arrTo,
		arrBcc,
		arrAttachments,
		arrLarge,
		sizeof(arrLarge),
		arrHeaders
	);
	Message.AlternativeBoundary = Message.MixedBoundary;
	testRequire(!xrtMailComposeWrite(&Message, testMailComposeWrite,
		&Sink, &iWritten) && (Sink.Size == 0),
		"mail compose accepted duplicate nested boundaries");

	testMailComposeMessage(
		&Message,
		arrTo,
		arrBcc,
		arrAttachments,
		arrLarge,
		sizeof(arrLarge),
		arrHeaders
	);
	arrAttachments[1].FileName = (xstrview){ "\xC3\x28", 2u };
	testRequire(!xrtMailComposeWrite(&Message, testMailComposeWrite,
		&Sink, &iWritten) && (Sink.Size == 0),
		"mail compose accepted an invalid UTF-8 filename");

	testMailComposeMessage(
		&Message,
		arrTo,
		arrBcc,
		arrAttachments,
		arrLarge,
		sizeof(arrLarge),
		arrHeaders
	);
	Sink.FailAfter = 2u;
	testRequire(!xrtMailComposeWrite(&Message, testMailComposeWrite,
		&Sink, &iWritten) && (Sink.Calls == 2u) && (Sink.Size != 0),
		"mail compose did not propagate sink failure");
}



/* 运行高层 MIME Compose 全部契约测试。 */
int main(void)
{
	testMailComposeComplete();
	testMailComposeDefaults();
	testMailComposeLongFileName();
	testMailComposeErrors();
	return 0;
}
