#include "../test.h"



typedef struct testmailbuildsink {
	char Data[4096];
	size_t Size;
	const void* Direct;
	bool SawDirect;
	bool Fail;
} testmailbuildsink;



/* 把流式片段收集到固定缓冲，并记录指定片段是否保持借用地址。 */
static bool testMailBuildWrite(xbytesview Data, ptr pUserData)
{
	testmailbuildsink* pSink = (testmailbuildsink*)pUserData;

	if ( pSink->Fail || (Data.Size > (sizeof(pSink->Data) - pSink->Size - 1u)) ) {
		return false;
	}
	if ( Data.Data == pSink->Direct ) {
		pSink->SawDirect = true;
	}
	memcpy(pSink->Data + pSink->Size, Data.Data, Data.Size);
	pSink->Size += Data.Size;
	pSink->Data[pSink->Size] = 0;
	return true;
}



/* 验证字段 helper、字段块和正文共享同一条流式输出路径。 */
static void testMailBuilderMessage(void)
{
	const xmailaddress arrTo[] = {
		{ XRT_STR_INIT("User One"), XRT_STR_INIT("one@example.com") },
		{ XRT_STR_INIT("中文"), XRT_STR_INIT("zh@example.cn") }
	};
	static const char sDirectHeader[] = "X-Trace: direct\r\n";
	static const char sBody[] = "body\r\n";
	testmailbuildsink Sink = { 0 };
	xmailbuilder Builder;

	testRequire(xrtMailBuilderInit(
		&Builder,
		testMailBuildWrite,
		&Sink
	), "mail builder init failed");
	testRequire(xrtMailBuilderHeader(
		&Builder,
		XRT_STR_LITERAL("MIME-Version"),
		XRT_STR_LITERAL("1.0"),
		0
	), "mail builder plain header failed");
	testRequire(xrtMailBuilderWordHeader(
		&Builder,
		XRT_STR_LITERAL("Subject"),
		XRT_STR_LITERAL("Hello 中文"),
		XMAIL_WORD_BASE64,
		0
	), "mail builder word header failed");
	testRequire(xrtMailBuilderAddressHeader(
		&Builder,
		XRT_STR_LITERAL("To"),
		arrTo,
		2u,
		XMAIL_WORD_BASE64,
		XMAIL_ADDRESS_DEFAULT,
		0
	), "mail builder address header failed");
	Sink.Direct = sDirectHeader;
	testRequire(xrtMailBuilderHeaderBlock(
		&Builder,
		XRT_STR_LITERAL(sDirectHeader)
	) && Sink.SawDirect, "mail builder did not preserve direct header block");
	testRequire(xrtMailBuilderHeadersEnd(&Builder),
		"mail builder headers end failed");
	Sink.Direct = sBody;
	Sink.SawDirect = false;
	testRequire(xrtMailBuilderBody(&Builder, sBody, sizeof(sBody) - 1u) &&
		Sink.SawDirect, "mail builder did not preserve direct body data");
	testRequire(xrtMailBuilderFinish(&Builder) &&
		(Builder.State == XMAIL_BUILDER_CLOSED) &&
		(Builder.Written == Sink.Size), "mail builder finish mismatch");
	testRequire(strstr(Sink.Data, "MIME-Version: 1.0\r\n") != NULL &&
		strstr(Sink.Data, "Subject: =?UTF-8?B?") != NULL &&
		strstr(Sink.Data, "User One <one@example.com>") != NULL &&
		strstr(Sink.Data, "=?UTF-8?B?5Lit5paH?= <zh@example.cn>") != NULL &&
		strstr(Sink.Data, "X-Trace: direct\r\n\r\nbody\r\n") != NULL,
		"mail builder message output mismatch");
}



/* 验证 multipart 分隔片段可直接接在正文流中。 */
static void testMailBuilderMultipart(void)
{
	testmailbuildsink Sink = { 0 };
	xmailbuilder Builder;

	testRequire(xrtMailBuilderInit(&Builder, testMailBuildWrite, &Sink) &&
		xrtMailBuilderHeader(
			&Builder,
			XRT_STR_LITERAL("Content-Type"),
			XRT_STR_LITERAL("multipart/mixed; boundary=mix"),
			0
		) && xrtMailBuilderHeadersEnd(&Builder) &&
		xrtMailBuilderPartBegin(
			&Builder,
			XRT_STR_LITERAL("mix"),
			XMAIL_MULTIPART_FIRST
		) && xrtMailBuilderHeader(
			&Builder,
			XRT_STR_LITERAL("Content-Type"),
			XRT_STR_LITERAL("text/plain"),
			0
		) && xrtMailBuilderHeadersEnd(&Builder) && xrtMailBuilderBody(
			&Builder,
			"part\r\n",
			6u
		) && xrtMailBuilderMultipart(
			&Builder,
			XRT_STR_LITERAL("mix"),
			XMAIL_MULTIPART_CLOSE
		) && xrtMailBuilderFinish(&Builder),
		"mail builder multipart path failed");
	testRequire(strstr(Sink.Data, "\r\n--mix\r\nContent-Type") != NULL &&
		strstr(Sink.Data, "\r\n--mix--\r\n") != NULL,
		"mail builder multipart output mismatch");
	testRequire(strstr(Sink.Data, "part\r\n\r\n--mix--") == NULL,
		"mail builder duplicated CRLF before a boundary");
}



/* 验证状态错误、坏字段块和 sink 失败不会继续发布输出。 */
static void testMailBuilderErrors(void)
{
	testmailbuildsink Sink = { 0 };
	xmailbuilder Builder;
	size_t iBefore;

	testRequire(xrtMailBuilderInit(&Builder, testMailBuildWrite, &Sink),
		"mail builder error setup failed");
	testRequire(!xrtMailBuilderBody(&Builder, "bad", 3u) && (Sink.Size == 0),
		"mail builder accepted body before fields end");
	testRequire(!xrtMailBuilderHeaderBlock(
		&Builder,
		XRT_STR_LITERAL("X-Test: one\r\n\r\n")
	) && (Sink.Size == 0), "mail builder accepted a header separator block");
	testRequire(xrtMailBuilderHeadersEnd(&Builder),
		"mail builder failed after recoverable state error");
	iBefore = Sink.Size;
	Sink.Fail = true;
	xrtClearError();
	testRequire(!xrtMailBuilderBody(&Builder, "data", 4u) &&
		(Builder.State == XMAIL_BUILDER_FAILED) &&
		(Sink.Size == iBefore) && xrtErrorFind(
			xrtGetError(),
			"xrt.mail",
			XMAIL_ERROR_CALLBACK
		) != NULL, "mail builder callback failure contract mismatch");
	testRequire(!xrtMailBuilderFinish(&Builder),
		"failed mail builder was reusable");
}



/* 运行流式邮件构建全部契约测试。 */
int main(void)
{
	testMailBuilderMessage();
	testMailBuilderMultipart();
	testMailBuilderErrors();
	return 0;
}
