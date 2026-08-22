#include "../test.h"



/* 验证通用认证请求保留未知方法字段。 */
static void testSshAuthGeneric(void)
{
	static const unsigned char arrFields[] = { 0u, 0xffu, 1u };
	unsigned char arrPayload[128];
	xsshwriter Writer;
	xsshauthrequest Request;
	size_t iSize;

	testRequire((xrtSshAuthRequestSize(
		XRT_STR_LITERAL("用户"),
		XRT_STR_LITERAL("ssh-connection"),
		XRT_STR_LITERAL("custom@example.com"),
		sizeof(arrFields),
		&iSize
	) == XSSH_OK) && xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) &&
		(xrtSshAuthRequestWrite(
			&Writer,
			XRT_STR_LITERAL("用户"),
			XRT_STR_LITERAL("ssh-connection"),
			XRT_STR_LITERAL("custom@example.com"),
			(xbytesview){ arrFields, sizeof(arrFields) }
		) == XSSH_OK) && (Writer.Size == iSize) && (xrtSshAuthRequestRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Request
		) == XSSH_OK) && testSshTextEqual(
			Request.User,
			XRT_STR_LITERAL("用户")
		) && testSshTextEqual(
			Request.Service,
			XRT_STR_LITERAL("ssh-connection")
		) && testSshTextEqual(
			Request.Method,
			XRT_STR_LITERAL("custom@example.com")
		) && testSshBytesEqual(
			Request.Fields,
			(xbytesview){ arrFields, sizeof(arrFields) }
		), "ssh generic auth request mismatch");
}



/* 验证 none 探测与失败、成功消息。 */
static void testSshAuthCommon(void)
{
	unsigned char arrPayload[128];
	xsshwriter Writer;
	xsshauthfailure Failure;
	xstrview User = XRT_STR_LITERAL("keep");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthNoneWrite(&Writer, XRT_STR_LITERAL("alice")) == XSSH_OK) &&
		(xrtSshAuthNoneRead(
			(xbytesview){ arrPayload, Writer.Size },
			&User
		) == XSSH_OK) && testSshTextEqual(User, XRT_STR_LITERAL("alice")),
		"ssh none auth mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthFailureWrite(
			&Writer,
			XRT_STR_LITERAL("publickey,password"),
			true
		) == XSSH_OK) && (xrtSshAuthFailureRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Failure
		) == XSSH_OK) && Failure.PartialSuccess && testSshTextEqual(
			Failure.Methods,
			XRT_STR_LITERAL("publickey,password")
		), "ssh auth failure mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthSuccessWrite(&Writer) == XSSH_OK) &&
		(xrtSshAuthSuccessRead(
			(xbytesview){ arrPayload, Writer.Size }
		) == XSSH_OK), "ssh auth success mismatch");
	arrPayload[Writer.Size++] = 0u;
	testRequire(xrtSshAuthSuccessRead(
		(xbytesview){ arrPayload, Writer.Size }
	) == XSSH_ERROR_PROTOCOL, "ssh auth success accepted trailing data");
}



/* 验证横幅 UTF-8、language tag 与失败原子性。 */
static void testSshAuthBanner(void)
{
	static const char arrBadUtf8[] = { (char)0xc0, (char)0x80 };
	unsigned char arrPayload[128];
	xsshwriter Writer;
	xsshauthbanner Banner;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthBannerWrite(
			&Writer,
			XRT_STR_LITERAL("欢迎"),
			XRT_STR_LITERAL("zh-CN")
		) == XSSH_OK) && (xrtSshAuthBannerRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Banner
		) == XSSH_OK) && testSshTextEqual(
			Banner.Message,
			XRT_STR_LITERAL("欢迎")
		) && testSshTextEqual(Banner.Language, XRT_STR_LITERAL("zh-CN")),
		"ssh auth banner mismatch");

	Writer.Size = 0u;
	testRequire((xrtSshAuthBannerWrite(
		&Writer,
		(xstrview){ arrBadUtf8, sizeof(arrBadUtf8) },
		XRT_STR_LITERAL("")
	) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh auth banner accepted invalid UTF-8");
}



/* 验证空间、重叠、名称和截断输入边界。 */
static void testSshAuthBoundaries(void)
{
	static const unsigned char arrTruncated[] = {
		XSSH_MSG_USERAUTH_REQUEST, 0u, 0u, 0u, 2u, 'a'
	};
	unsigned char arrPayload[64];
	xsshwriter Writer;
	xsshauthrequest Request;
	size_t iSize;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, 4u),
		"ssh auth short writer setup failed");
	iSize = Writer.Size;
	testRequire((xrtSshAuthNoneWrite(
		&Writer,
		XRT_STR_LITERAL("alice")
	) == XSSH_ERROR_SPACE) && (Writer.Size == iSize),
		"ssh auth short write changed state");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)),
		"ssh auth overlap writer setup failed");
	testRequire((xrtSshAuthRequestWrite(
		&Writer,
		(xstrview){ (const char*)arrPayload, 1u },
		XRT_STR_LITERAL("ssh-connection"),
		XRT_STR_LITERAL("none"),
		(xbytesview){ NULL, 0u }
	) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh auth write accepted overlapping input");

	testRequire(xrtSshAuthRequestWrite(
		&Writer,
		XRT_STR_LITERAL("alice"),
		XRT_STR_LITERAL("bad service"),
		XRT_STR_LITERAL("none"),
		(xbytesview){ NULL, 0u }
	) == XSSH_ERROR_ARGUMENT, "ssh auth accepted invalid service name");
	testRequire(xrtSshAuthRequestRead(
		(xbytesview){ arrTruncated, sizeof(arrTruncated) },
		&Request
	) == XSSH_NEED_MORE, "ssh auth truncated request was not incremental");
}



/* 运行公共认证消息与边界测试。 */
int main(void)
{
	testSshAuthGeneric();
	testSshAuthCommon();
	testSshAuthBanner();
	testSshAuthBoundaries();
	return 0;
}
