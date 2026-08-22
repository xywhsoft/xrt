#include "../test.h"



/* 验证未知 channel open 类型字段完整往返。 */
static void testSshChannelOpen(void)
{
	static const unsigned char arrFields[] = { 0u, 1u, 0x80u, 0xffu };
	unsigned char arrPayload[128];
	xsshwriter Writer;
	xsshchannelopen Open;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelOpenWrite(
			&Writer,
			XRT_STR_LITERAL("direct-example@example.com"),
			UINT32_MAX,
			UINT32_MAX,
			32768u,
			(xbytesview){ arrFields, sizeof(arrFields) }
		) == XSSH_OK) && (xrtSshChannelOpenRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Open
		) == XSSH_OK) && testSshTextEqual(
		Open.Type,
		XRT_STR_LITERAL("direct-example@example.com")
	) && (Open.Sender == UINT32_MAX) && (Open.Window == UINT32_MAX) &&
		(Open.MaxPacket == 32768u) && testSshBytesEqual(
		Open.Fields,
		(xbytesview){ arrFields, sizeof(arrFields) }
	), "ssh channel open mismatch");
}



/* 验证 open confirmation 与 UTF-8 failure。 */
static void testSshChannelOpenResponses(void)
{
	static const unsigned char arrFields[] = { 4u, 3u, 2u, 1u };
	static const char arrDescription[] = {
		(char)0xe8, (char)0xb5, (char)0x84,
		(char)0xe6, (char)0xba, (char)0x90,
		(char)0xe4, (char)0xb8, (char)0x8d,
		(char)0xe8, (char)0xb6, (char)0xb3
	};
	unsigned char arrPayload[128];
	xsshchannelconfirmation Confirmation;
	xsshchannelopenfailure Failure;
	xsshwriter Writer;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelOpenConfirmationWrite(
			&Writer,
			9u,
			10u,
			0u,
			65535u,
			(xbytesview){ arrFields, sizeof(arrFields) }
		) == XSSH_OK) && (xrtSshChannelOpenConfirmationRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Confirmation
		) == XSSH_OK) && (Confirmation.Recipient == 9u) &&
		(Confirmation.Sender == 10u) && (Confirmation.Window == 0u) &&
		(Confirmation.MaxPacket == 65535u) && testSshBytesEqual(
		Confirmation.Fields,
		(xbytesview){ arrFields, sizeof(arrFields) }
	), "ssh channel open confirmation mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelOpenFailureWrite(
			&Writer,
			9u,
			XSSH_CHANNEL_OPEN_RESOURCE_SHORTAGE,
			(xstrview){ arrDescription, sizeof(arrDescription) },
			XRT_STR_LITERAL("zh-CN")
		) == XSSH_OK) && (xrtSshChannelOpenFailureRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Failure
		) == XSSH_OK) && (Failure.Recipient == 9u) &&
		(Failure.Reason == XSSH_CHANNEL_OPEN_RESOURCE_SHORTAGE) &&
		testSshTextEqual(
			Failure.Description,
			(xstrview){ arrDescription, sizeof(arrDescription) }
		) && testSshTextEqual(Failure.Language, XRT_STR_LITERAL("zh-CN")),
		"ssh channel open failure mismatch");
}



/* 验证窗口、普通数据和 extended data 的完整 uint32 与二进制范围。 */
static void testSshChannelFlowMessages(void)
{
	static const unsigned char arrData[] = { 0u, 1u, 0u, 0xffu };
	unsigned char arrPayload[64];
	xsshchannelextendeddata Extended;
	xsshchanneladjust Adjust;
	xsshchanneldata Data;
	xsshwriter Writer;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelWindowAdjustWrite(
			&Writer,
			UINT32_MAX,
			UINT32_MAX
		) == XSSH_OK) && (xrtSshChannelWindowAdjustRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Adjust
		) == XSSH_OK) && (Adjust.Recipient == UINT32_MAX) &&
		(Adjust.Bytes == UINT32_MAX), "ssh channel window adjust mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelDataWrite(
			&Writer,
			7u,
			(xbytesview){ arrData, sizeof(arrData) }
		) == XSSH_OK) && (xrtSshChannelDataRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Data
		) == XSSH_OK) && (Data.Recipient == 7u) && testSshBytesEqual(
		Data.Data,
		(xbytesview){ arrData, sizeof(arrData) }
	), "ssh channel data mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelExtendedDataWrite(
			&Writer,
			7u,
			XSSH_CHANNEL_EXTENDED_DATA_STDERR,
			(xbytesview){ NULL, 0u }
		) == XSSH_OK) && (xrtSshChannelExtendedDataRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Extended
		) == XSSH_OK) && (Extended.Recipient == 7u) &&
		(Extended.Type == XSSH_CHANNEL_EXTENDED_DATA_STDERR) &&
		(Extended.Data.Size == 0u), "ssh channel extended data mismatch");
}



/* 验证 request 原始字段和四种固定 channel 消息。 */
static void testSshChannelRequestAndSimple(void)
{
	static const unsigned char arrFields[] = { 0u, 0u, 0u, 3u, 'l', 's', 0u };
	unsigned char arrPayload[64];
	xsshchannelrequest Request;
	xsshwriter Writer;
	uint32 iRecipient;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelRequestWrite(
			&Writer,
			42u,
			XRT_STR_LITERAL("exec"),
			true,
			(xbytesview){ arrFields, sizeof(arrFields) }
		) == XSSH_OK) && (xrtSshChannelRequestRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Request
		) == XSSH_OK) && (Request.Recipient == 42u) &&
		testSshTextEqual(Request.Type, XRT_STR_LITERAL("exec")) &&
		Request.WantReply && testSshBytesEqual(
		Request.Fields,
		(xbytesview){ arrFields, sizeof(arrFields) }
	), "ssh channel request mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelEofWrite(&Writer, 1u) == XSSH_OK) &&
		(xrtSshChannelEofRead(
			(xbytesview){ arrPayload, Writer.Size },
			&iRecipient
		) == XSSH_OK) && (iRecipient == 1u), "ssh channel EOF mismatch");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelCloseWrite(&Writer, 2u) == XSSH_OK) &&
		(xrtSshChannelCloseRead(
			(xbytesview){ arrPayload, Writer.Size },
			&iRecipient
		) == XSSH_OK) && (iRecipient == 2u), "ssh channel close mismatch");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelSuccessWrite(&Writer, 3u) == XSSH_OK) &&
		(xrtSshChannelSuccessRead(
			(xbytesview){ arrPayload, Writer.Size },
			&iRecipient
		) == XSSH_OK) && (iRecipient == 3u), "ssh channel success mismatch");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelFailureWrite(&Writer, 4u) == XSSH_OK) &&
		(xrtSshChannelFailureRead(
			(xbytesview){ arrPayload, Writer.Size },
			&iRecipient
		) == XSSH_OK) && (iRecipient == 4u), "ssh channel failure mismatch");
}



/* 验证非法字段、截断、尾随、短缓冲和输入输出重叠。 */
static void testSshChannelBoundaries(void)
{
	static const char arrInvalidUtf8[] = { (char)0xc0, (char)0x80 };
	unsigned char arrPayload[96];
	xsshchannelopen Open = { XRT_STR_LITERAL("keep"), 1u, 2u, 3u,
		XRT_BYTES_LITERAL("keep") };
	xsshchanneldata Data;
	xsshwriter Writer;
	uint32 iRecipient = 99u;
	size_t iSize;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelOpenWrite(
			&Writer,
			XRT_STR_LITERAL("bad,name"),
			0u,
			0u,
			1u,
			(xbytesview){ NULL, 0u }
		) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u) &&
		(xrtSshChannelOpenWrite(
			&Writer,
			XRT_STR_LITERAL("session"),
			0u,
			0u,
			0u,
			(xbytesview){ NULL, 0u }
		) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh channel open accepted invalid fixed fields");
	testRequire((xrtSshChannelOpenFailureWrite(
		&Writer,
		0u,
		1u,
		(xstrview){ arrInvalidUtf8, sizeof(arrInvalidUtf8) },
		XRT_STR_LITERAL("en")
	) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u) &&
		(xrtSshChannelOpenFailureWrite(
			&Writer,
			0u,
			1u,
			XRT_STR_LITERAL("denied"),
			XRT_STR_LITERAL("en US")
		) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh channel failure accepted invalid text");

	testRequire(xrtSshChannelOpenWrite(
		&Writer,
		XRT_STR_LITERAL("session"),
		1u,
		2u,
		3u,
		(xbytesview){ NULL, 0u }
	) == XSSH_OK, "ssh channel boundary setup failed");
	iSize = Writer.Size;
	testRequire((xrtSshChannelOpenRead(
		(xbytesview){ arrPayload, iSize - 1u },
		&Open
	) == XSSH_NEED_MORE) && testSshTextEqual(
		Open.Type,
		XRT_STR_LITERAL("keep")
	), "ssh channel truncated open changed output");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, 8u) &&
		(xrtSshChannelDataWrite(
			&Writer,
			1u,
			XRT_BYTES_LITERAL("x")
		) == XSSH_ERROR_SPACE) && (Writer.Size == 0u),
		"ssh channel short writer changed state");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelDataWrite(
			&Writer,
			1u,
			(xbytesview){ arrPayload + 8u, 4u }
		) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh channel writer accepted overlapping data");

	testRequire(xrtSshChannelDataWrite(
		&Writer,
		1u,
		XRT_BYTES_LITERAL("x")
	) == XSSH_OK, "ssh channel data boundary setup failed");
	arrPayload[Writer.Size] = 0u;
	testRequire((xrtSshChannelDataRead(
		(xbytesview){ arrPayload, Writer.Size + 1u },
		&Data
	) == XSSH_ERROR_PROTOCOL), "ssh channel data accepted trailing bytes");

	arrPayload[0] = XSSH_MSG_CHANNEL_EOF;
	arrPayload[1] = 0u;
	arrPayload[2] = 0u;
	arrPayload[3] = 0u;
	arrPayload[4] = 7u;
	arrPayload[5] = 0u;
	testRequire((xrtSshChannelEofRead(
		(xbytesview){ arrPayload, 6u },
		&iRecipient
	) == XSSH_ERROR_PROTOCOL) && (iRecipient == 99u),
		"ssh channel simple message accepted trailing bytes");
	testRequire(xrtSshChannelOpenRead(
		(xbytesview){ arrPayload, 5u },
		(xsshchannelopen*)arrPayload
	) == XSSH_ERROR_ARGUMENT, "ssh channel reader accepted overlapping output");
}



/* 运行 RFC 4254 channel 公共报文与边界测试。 */
int main(void)
{
	testSshChannelOpen();
	testSshChannelOpenResponses();
	testSshChannelFlowMessages();
	testSshChannelRequestAndSimple();
	testSshChannelBoundaries();
	return 0;
}
