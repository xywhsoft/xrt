#include "../test.h"



/* 验证 mode writer、无固定数量迭代和扩展 opcode 停止规则。 */
static void testSshTerminalModes(void)
{
	static const unsigned char arrUnsupported[] = { 160u, 1u, 2u, 3u };
	static const unsigned char arrMissingEnd[] = { XSSH_TTY_OP_ECHO, 0u, 0u, 0u, 1u };
	static const unsigned char arrTrailing[] = { XSSH_TTY_OP_END, 1u };
	unsigned char arrModes[384];
	xsshterminalmodes Modes;
	xsshterminalmode Mode;
	xsshwriter Writer;
	size_t i;

	testRequire(xrtSshWriterInit(&Writer, arrModes, sizeof(arrModes)),
		"ssh terminal mode writer init failed");
	for ( i = 0u; i < 64u; ++i ) {
		testRequire(xrtSshTerminalModeWrite(
			&Writer,
			(uint8)(i + 1u),
			(uint32)(1000u + i)
		) == XSSH_OK, "ssh terminal mode write failed");
	}
	testRequire((xrtSshTerminalModeEnd(&Writer) == XSSH_OK) &&
		(xrtSshTerminalModesRead(
			(xbytesview){ arrModes, Writer.Size },
			&Modes
		) == XSSH_OK) && (Modes.Count == 64u) && !Modes.Unsupported,
		"ssh terminal mode prevalidation mismatch");
	for ( i = 0u; i < 64u; ++i ) {
		testRequire(xrtSshTerminalModesNext(&Modes, &Mode) &&
			(Mode.Opcode == (uint8)(i + 1u)) &&
			(Mode.Value == (uint32)(1000u + i)),
			"ssh terminal mode iteration mismatch");
	}
	testRequire(!xrtSshTerminalModesNext(&Modes, &Mode),
		"ssh terminal mode iterator exceeded count");

	testRequire((xrtSshTerminalModesRead(
		(xbytesview){ NULL, 0u },
		&Modes
	) == XSSH_OK) && (Modes.Count == 0u) &&
		(xrtSshTerminalModesRead(
			(xbytesview){ arrUnsupported, sizeof(arrUnsupported) },
			&Modes
		) == XSSH_OK) && Modes.Unsupported && (Modes.Count == 0u),
		"ssh terminal empty or unsupported mode mismatch");
	testRequire((xrtSshTerminalModesRead(
		(xbytesview){ arrMissingEnd, sizeof(arrMissingEnd) },
		&Modes
	) == XSSH_ERROR_PROTOCOL) && (xrtSshTerminalModesRead(
		(xbytesview){ arrTrailing, sizeof(arrTrailing) },
		&Modes
	) == XSSH_ERROR_PROTOCOL), "ssh terminal malformed modes were accepted");

	testRequire(xrtSshWriterInit(&Writer, arrModes, sizeof(arrModes)) &&
		(xrtSshTerminalModeWrite(
			&Writer,
			XSSH_TTY_OP_END,
			0u
		) == XSSH_ERROR_ARGUMENT) && (xrtSshTerminalModeWrite(
			&Writer,
			XSSH_TTY_OP_UNSUPPORTED_MIN,
			0u
		) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh terminal mode writer accepted reserved opcode");
}



/* 验证完整 PTY request 与 mode stream 借用。 */
static void testSshChannelPty(void)
{
	unsigned char arrModes[32];
	unsigned char arrPayload[160];
	xsshterminalmodes Modes;
	xsshterminalmode Mode;
	xsshchannelrequest Request;
	xsshchannelpty Pty;
	xsshwriter ModeWriter;
	xsshwriter Writer;

	testRequire(xrtSshWriterInit(&ModeWriter, arrModes, sizeof(arrModes)) &&
		(xrtSshTerminalModeWrite(
			&ModeWriter,
			XSSH_TTY_OP_ECHO,
			0u
		) == XSSH_OK) && (xrtSshTerminalModeWrite(
			&ModeWriter,
			XSSH_TTY_OP_ISPEED,
			115200u
		) == XSSH_OK) && (xrtSshTerminalModeEnd(&ModeWriter) == XSSH_OK),
		"ssh PTY mode setup failed");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelPtyWrite(
			&Writer,
			5u,
			true,
			XRT_BYTES_LITERAL("xterm-256color"),
			120u,
			50u,
			1920u,
			1080u,
			(xbytesview){ arrModes, ModeWriter.Size }
		) == XSSH_OK) && (xrtSshChannelRequestRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Request
		) == XSSH_OK) && (xrtSshChannelPtyRead(&Request, &Pty) == XSSH_OK),
		"ssh PTY request read/write failed");
	testRequire((Request.Recipient == 5u) && Request.WantReply &&
		testSshBytesEqual(Pty.Terminal, XRT_BYTES_LITERAL("xterm-256color")) &&
		(Pty.Columns == 120u) && (Pty.Rows == 50u) &&
		(Pty.PixelWidth == 1920u) && (Pty.PixelHeight == 1080u) &&
		(xrtSshTerminalModesRead(Pty.Modes, &Modes) == XSSH_OK) &&
		xrtSshTerminalModesNext(&Modes, &Mode) &&
		(Mode.Opcode == XSSH_TTY_OP_ECHO) && (Mode.Value == 0u) &&
		xrtSshTerminalModesNext(&Modes, &Mode) &&
		(Mode.Opcode == XSSH_TTY_OP_ISPEED) && (Mode.Value == 115200u) &&
		!xrtSshTerminalModesNext(&Modes, &Mode), "ssh PTY fields mismatch");
}



/* 验证 PTY mode 语法、短缓冲、重叠与尾随字段。 */
static void testSshChannelPtyBoundaries(void)
{
	static const unsigned char arrBadModes[] = {
		XSSH_TTY_OP_ECHO, 0u, 0u, 0u, 1u
	};
	unsigned char arrPayload[128];
	xsshchannelrequest Request;
	xsshchannelpty Pty = { XRT_BYTES_LITERAL("keep"), 1u, 2u, 3u, 4u,
		XRT_BYTES_LITERAL("keep") };
	xsshwriter Writer;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelPtyWrite(
			&Writer,
			1u,
			true,
			XRT_BYTES_LITERAL("xterm"),
			80u,
			24u,
			0u,
			0u,
			(xbytesview){ arrBadModes, sizeof(arrBadModes) }
		) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh PTY writer accepted unterminated modes");
	testRequire((xrtSshChannelPtyWrite(
		&Writer,
		1u,
		true,
		(xbytesview){ arrPayload + 8u, 8u },
		80u,
		24u,
		0u,
		0u,
		(xbytesview){ NULL, 0u }
	) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh PTY writer accepted overlapping terminal");

	testRequire(xrtSshChannelPtyWrite(
		&Writer,
		1u,
		true,
		XRT_BYTES_LITERAL("xterm"),
		80u,
		24u,
		0u,
		0u,
		(xbytesview){ NULL, 0u }
	) == XSSH_OK, "ssh PTY boundary setup failed");
	testRequire(xrtSshChannelRequestRead(
		(xbytesview){ arrPayload, Writer.Size },
		&Request
	) == XSSH_OK, "ssh PTY boundary envelope failed");
	Request.Fields.Size--;
	testRequire((xrtSshChannelPtyRead(&Request, &Pty) == XSSH_NEED_MORE) &&
		testSshBytesEqual(Pty.Terminal, XRT_BYTES_LITERAL("keep")),
		"ssh PTY truncation changed output");
}



/* 运行 PTY 与无固定上限 terminal mode 测试。 */
int main(void)
{
	testSshTerminalModes();
	testSshChannelPty();
	testSshChannelPtyBoundaries();
	return 0;
}
