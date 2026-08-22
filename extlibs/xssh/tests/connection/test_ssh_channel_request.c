#include "../test.h"



/* 从完整 payload 读取公共 channel request。 */
static xsshchannelrequest testSshChannelRequestParse(
	const unsigned char* pPayload,
	size_t iSize
)
{
	xsshchannelrequest Request;

	testRequire(xrtSshChannelRequestRead(
		(xbytesview){ pPayload, iSize },
		&Request
	) == XSSH_OK, "ssh channel request envelope read failed");
	return Request;
}



/* 验证 shell、二进制 exec command 和 subsystem。 */
static void testSshChannelSessionRequests(void)
{
	static const unsigned char arrCommand[] = { 'e', 'c', 'h', 'o', 0u, 'x' };
	unsigned char arrPayload[128];
	xsshchannelrequest Request;
	xbytesview Value;
	xsshwriter Writer;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelShellWrite(&Writer, 3u, true) == XSSH_OK),
		"ssh shell request write failed");
	Request = testSshChannelRequestParse(arrPayload, Writer.Size);
	testRequire((Request.Recipient == 3u) && Request.WantReply &&
		(xrtSshChannelShellRead(&Request) == XSSH_OK),
		"ssh shell request mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelExecWrite(
			&Writer,
			4u,
			true,
			(xbytesview){ arrCommand, sizeof(arrCommand) }
		) == XSSH_OK), "ssh exec request write failed");
	Request = testSshChannelRequestParse(arrPayload, Writer.Size);
	testRequire((xrtSshChannelExecRead(&Request, &Value) == XSSH_OK) &&
		testSshBytesEqual(
			Value,
			(xbytesview){ arrCommand, sizeof(arrCommand) }
		), "ssh exec request mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelSubsystemWrite(
			&Writer,
			5u,
			false,
			XRT_BYTES_LITERAL("sftp")
		) == XSSH_OK), "ssh subsystem request write failed");
	Request = testSshChannelRequestParse(arrPayload, Writer.Size);
	testRequire(!Request.WantReply &&
		(xrtSshChannelSubsystemRead(&Request, &Value) == XSSH_OK) &&
		testSshBytesEqual(Value, XRT_BYTES_LITERAL("sftp")),
		"ssh subsystem request mismatch");
}



/* 验证 env、xon-xoff 与 window-change。 */
static void testSshChannelTerminalNotices(void)
{
	static const unsigned char arrValue[] = { 'x', 0u, 'y' };
	unsigned char arrPayload[128];
	xsshchannelwindowchange Change;
	xsshchannelrequest Request;
	xsshchannelenv Env;
	xsshwriter Writer;
	bool bClientCanDo;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelEnvWrite(
			&Writer,
			7u,
			true,
			XRT_BYTES_LITERAL("LANG"),
			(xbytesview){ arrValue, sizeof(arrValue) }
		) == XSSH_OK), "ssh env request write failed");
	Request = testSshChannelRequestParse(arrPayload, Writer.Size);
	testRequire((xrtSshChannelEnvRead(&Request, &Env) == XSSH_OK) &&
		testSshBytesEqual(Env.Name, XRT_BYTES_LITERAL("LANG")) &&
		testSshBytesEqual(
			Env.Value,
			(xbytesview){ arrValue, sizeof(arrValue) }
		), "ssh env request mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelXonXoffWrite(&Writer, 7u, true) == XSSH_OK),
		"ssh xon-xoff request write failed");
	Request = testSshChannelRequestParse(arrPayload, Writer.Size);
	testRequire(!Request.WantReply &&
		(xrtSshChannelXonXoffRead(&Request, &bClientCanDo) == XSSH_OK) &&
		bClientCanDo, "ssh xon-xoff request mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelWindowChangeWrite(
			&Writer,
			7u,
			120u,
			50u,
			1920u,
			1080u
		) == XSSH_OK), "ssh window-change request write failed");
	Request = testSshChannelRequestParse(arrPayload, Writer.Size);
	testRequire(!Request.WantReply &&
		(xrtSshChannelWindowChangeRead(&Request, &Change) == XSSH_OK) &&
		(Change.Columns == 120u) && (Change.Rows == 50u) &&
		(Change.PixelWidth == 1920u) && (Change.PixelHeight == 1080u),
		"ssh window-change request mismatch");
}



/* 验证 signal、break、exit-status 与 UTF-8 exit-signal。 */
static void testSshChannelProcessNotices(void)
{
	static const char arrMessage[] = {
		(char)0xe7, (char)0xbb, (char)0x88,
		(char)0xe6, (char)0xad, (char)0xa2
	};
	unsigned char arrPayload[128];
	xsshchannelexitsignal ExitSignal;
	xsshchannelrequest Request;
	xsshwriter Writer;
	xstrview Signal;
	uint32 iValue;

	testRequire(xrtSshChannelSignalValid(XRT_STR_LITERAL("TERM")) &&
		!xrtSshChannelSignalValid(XRT_STR_LITERAL("SIGTERM")) &&
		!xrtSshChannelSignalValid(XRT_STR_LITERAL("bad,name")),
		"ssh channel signal validation mismatch");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelSignalWrite(
			&Writer,
			8u,
			XRT_STR_LITERAL("TERM")
		) == XSSH_OK), "ssh signal request write failed");
	Request = testSshChannelRequestParse(arrPayload, Writer.Size);
	testRequire((xrtSshChannelSignalRead(&Request, &Signal) == XSSH_OK) &&
		testSshTextEqual(Signal, XRT_STR_LITERAL("TERM")),
		"ssh signal request mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelBreakWrite(&Writer, 8u, true, 500u) == XSSH_OK),
		"ssh break request write failed");
	Request = testSshChannelRequestParse(arrPayload, Writer.Size);
	testRequire(Request.WantReply &&
		(xrtSshChannelBreakRead(&Request, &iValue) == XSSH_OK) &&
		(iValue == 500u), "ssh break request mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelExitStatusWrite(&Writer, 8u, UINT32_MAX) == XSSH_OK),
		"ssh exit-status request write failed");
	Request = testSshChannelRequestParse(arrPayload, Writer.Size);
	testRequire(!Request.WantReply &&
		(xrtSshChannelExitStatusRead(&Request, &iValue) == XSSH_OK) &&
		(iValue == UINT32_MAX), "ssh exit-status request mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelExitSignalWrite(
			&Writer,
			8u,
			XRT_STR_LITERAL("ABRT"),
			true,
			(xstrview){ arrMessage, sizeof(arrMessage) },
			XRT_STR_LITERAL("zh-CN")
		) == XSSH_OK), "ssh exit-signal request write failed");
	Request = testSshChannelRequestParse(arrPayload, Writer.Size);
	testRequire(!Request.WantReply &&
		(xrtSshChannelExitSignalRead(&Request, &ExitSignal) == XSSH_OK) &&
		testSshTextEqual(ExitSignal.Signal, XRT_STR_LITERAL("ABRT")) &&
		ExitSignal.CoreDumped && testSshTextEqual(
			ExitSignal.Message,
			(xstrview){ arrMessage, sizeof(arrMessage) }
		) && testSshTextEqual(ExitSignal.Language, XRT_STR_LITERAL("zh-CN")),
		"ssh exit-signal request mismatch");
}



/* 验证类型、回复位、尾随、文本、容量和重叠边界。 */
static void testSshChannelRequestBoundaries(void)
{
	static const char arrInvalidUtf8[] = { (char)0xc0, (char)0x80 };
	unsigned char arrPayload[96];
	xsshchannelwindowchange Change = { 1u, 2u, 3u, 4u };
	xsshchannelrequest Request;
	xbytesview Value = XRT_BYTES_LITERAL("keep");
	xsshwriter Writer;
	size_t iSize;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, 8u) &&
		(xrtSshChannelExecWrite(
			&Writer,
			1u,
			true,
			XRT_BYTES_LITERAL("x")
		) == XSSH_ERROR_SPACE) && (Writer.Size == 0u),
		"ssh channel request short writer changed state");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelExecWrite(
			&Writer,
			1u,
			true,
			(xbytesview){ arrPayload + 8u, 8u }
		) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh channel request accepted overlapping input");
	testRequire((xrtSshChannelSignalWrite(
		&Writer,
		1u,
		XRT_STR_LITERAL("SIGTERM")
	) == XSSH_ERROR_ARGUMENT) && (xrtSshChannelExitSignalWrite(
		&Writer,
		1u,
		XRT_STR_LITERAL("TERM"),
		false,
		(xstrview){ arrInvalidUtf8, sizeof(arrInvalidUtf8) },
		XRT_STR_LITERAL("en")
	) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh channel request accepted invalid text");

	testRequire(xrtSshChannelExecWrite(
		&Writer,
		1u,
		true,
		XRT_BYTES_LITERAL("x")
	) == XSSH_OK, "ssh channel request boundary setup failed");
	iSize = Writer.Size;
	Request = testSshChannelRequestParse(arrPayload, iSize);
	Request.Fields.Size++;
	arrPayload[iSize] = 0u;
	testRequire((xrtSshChannelExecRead(&Request, &Value) ==
		XSSH_ERROR_PROTOCOL) && testSshBytesEqual(
		Value,
		XRT_BYTES_LITERAL("keep")
	), "ssh exec parser accepted trailing field bytes");

	Request = testSshChannelRequestParse(arrPayload, iSize);
	Request.Type = XRT_STR_LITERAL("subsystem");
	testRequire((xrtSshChannelExecRead(&Request, &Value) ==
		XSSH_ERROR_ARGUMENT) && testSshBytesEqual(
		Value,
		XRT_BYTES_LITERAL("keep")
	), "ssh request type mismatch changed output");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelWindowChangeWrite(
			&Writer,
			1u,
			80u,
			24u,
			0u,
			0u
		) == XSSH_OK), "ssh window-change boundary setup failed");
	Request = testSshChannelRequestParse(arrPayload, Writer.Size);
	Request.WantReply = true;
	testRequire((xrtSshChannelWindowChangeRead(&Request, &Change) ==
		XSSH_ERROR_ARGUMENT) && (Change.Columns == 1u),
		"ssh window-change accepted want-reply or changed output");
}



/* 运行标准 channel request 便利层和边界测试。 */
int main(void)
{
	testSshChannelSessionRequests();
	testSshChannelTerminalNotices();
	testSshChannelProcessNotices();
	testSshChannelRequestBoundaries();
	return 0;
}
