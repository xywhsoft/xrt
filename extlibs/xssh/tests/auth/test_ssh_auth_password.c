#include "../test.h"



/* 验证普通密码请求往返。 */
static void testSshPasswordBasic(void)
{
	unsigned char arrPayload[128];
	xsshwriter Writer;
	xsshauthpassword Password;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthPasswordWrite(
			&Writer,
			XRT_STR_LITERAL("alice"),
			XRT_STR_LITERAL("密码")
		) == XSSH_OK) && (xrtSshAuthPasswordRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Password
		) == XSSH_OK) && !Password.Change && testSshTextEqual(
			Password.User,
			XRT_STR_LITERAL("alice")
		) && testSshTextEqual(
			Password.Password,
			XRT_STR_LITERAL("密码")
		) && (Password.NewPassword.Size == 0u),
		"ssh password request mismatch");
}



/* 验证旧密码、新密码和服务端更改提示。 */
static void testSshPasswordChange(void)
{
	unsigned char arrPayload[192];
	xsshwriter Writer;
	xsshauthpassword Password;
	xsshauthpasswordprompt Prompt;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthPasswordChangeWrite(
			&Writer,
			XRT_STR_LITERAL("bob"),
			XRT_STR_LITERAL("old"),
			XRT_STR_LITERAL("new")
		) == XSSH_OK) && (xrtSshAuthPasswordRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Password
		) == XSSH_OK) && Password.Change && testSshTextEqual(
			Password.Password,
			XRT_STR_LITERAL("old")
		) && testSshTextEqual(
			Password.NewPassword,
			XRT_STR_LITERAL("new")
		), "ssh password change request mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthPasswordPromptWrite(
			&Writer,
			XRT_STR_LITERAL("Please change password"),
			XRT_STR_LITERAL("en-US")
		) == XSSH_OK) && (xrtSshAuthPasswordPromptRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Prompt
		) == XSSH_OK) && testSshTextEqual(
			Prompt.Prompt,
			XRT_STR_LITERAL("Please change password")
		) && testSshTextEqual(Prompt.Language, XRT_STR_LITERAL("en-US")),
		"ssh password change prompt mismatch");
}



/* 验证密码消息的截断、尾随、UTF-8、空间和重叠边界。 */
static void testSshPasswordBoundaries(void)
{
	static const char arrBadUtf8[] = { (char)0xc0, (char)0x80 };
	unsigned char arrPayload[128];
	xsshwriter Writer;
	xsshauthpassword Password;
	size_t iRequestSize;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthPasswordChangeWrite(
			&Writer,
			XRT_STR_LITERAL("bob"),
			XRT_STR_LITERAL("old"),
			XRT_STR_LITERAL("new")
		) == XSSH_OK), "ssh password boundary setup failed");
	iRequestSize = Writer.Size;
	testRequire(xrtSshAuthPasswordRead(
		(xbytesview){ arrPayload, iRequestSize - 1u },
		&Password
	) == XSSH_NEED_MORE, "ssh password truncated new value was not incremental");
	arrPayload[iRequestSize] = 0u;
	testRequire(xrtSshAuthPasswordRead(
		(xbytesview){ arrPayload, iRequestSize + 1u },
		&Password
	) == XSSH_ERROR_PROTOCOL, "ssh password accepted trailing data");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)),
		"ssh password invalid writer setup failed");
	testRequire((xrtSshAuthPasswordWrite(
		&Writer,
		XRT_STR_LITERAL("bob"),
		(xstrview){ arrBadUtf8, sizeof(arrBadUtf8) }
	) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh password accepted invalid UTF-8");
	testRequire((xrtSshAuthPasswordWrite(
		&Writer,
		XRT_STR_LITERAL("bob"),
		(xstrview){ (const char*)arrPayload + 2u, 1u }
	) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh password accepted overlapping secret");
	Writer.Capacity = 8u;
	testRequire((xrtSshAuthPasswordWrite(
		&Writer,
		XRT_STR_LITERAL("bob"),
		XRT_STR_LITERAL("secret")
	) == XSSH_ERROR_SPACE) && (Writer.Size == 0u),
		"ssh password short write changed state");
}



/* 运行 password 方法消息与边界测试。 */
int main(void)
{
	testSshPasswordBasic();
	testSshPasswordChange();
	testSshPasswordBoundaries();
	return 0;
}
