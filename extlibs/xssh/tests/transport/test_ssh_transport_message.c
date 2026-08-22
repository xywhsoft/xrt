#include "../test.h"



/* 验证无字段消息和消息号探测。 */
static void testSshTransportEmptyMessages(void)
{
	unsigned char arrPayload[8];
	xsshwriter Writer;
	uint8 iMessage = 0u;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshNewKeysWrite(&Writer) == XSSH_OK) && (Writer.Size == 1u) &&
		(xrtSshMessageType(
			(xbytesview){ arrPayload, Writer.Size },
			&iMessage
		) == XSSH_OK) && (iMessage == XSSH_MSG_NEWKEYS) &&
		(xrtSshNewKeysRead(
			(xbytesview){ arrPayload, Writer.Size }
		) == XSSH_OK), "ssh newkeys message failed");
	arrPayload[1] = 0u;
	testRequire(xrtSshNewKeysRead(
		(xbytesview){ arrPayload, 2u }
	) == XSSH_ERROR_PROTOCOL, "ssh newkeys accepted trailing data");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshNewCompressWrite(&Writer) == XSSH_OK) &&
		(xrtSshNewCompressRead(
			(xbytesview){ arrPayload, Writer.Size }
		) == XSSH_OK), "ssh newcompress message failed");
}



/* 验证 RFC 4253 通用 transport 消息往返。 */
static void testSshTransportCoreMessages(void)
{
	unsigned char arrPayload[256];
	xsshwriter Writer;
	xsshdisconnect Disconnect;
	xsshignore Ignore;
	xsshdebug Debug;
	xsshservice Service;
	uint32 iSequence = 0u;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshDisconnectWrite(
			&Writer,
			XSSH_DISCONNECT_BY_APPLICATION,
			XRT_STR_LITERAL("done"),
			XRT_STR_LITERAL("en")
		) == XSSH_OK) && (xrtSshDisconnectRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Disconnect
		) == XSSH_OK) &&
		(Disconnect.Reason == XSSH_DISCONNECT_BY_APPLICATION) &&
		testSshTextEqual(Disconnect.Description, XRT_STR_LITERAL("done")) &&
		testSshTextEqual(Disconnect.Language, XRT_STR_LITERAL("en")),
		"ssh disconnect roundtrip failed");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshIgnoreWrite(
			&Writer,
			XRT_BYTES_LITERAL("\0binary")
		) == XSSH_OK) && (xrtSshIgnoreRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Ignore
		) == XSSH_OK) && testSshBytesEqual(
			Ignore.Data,
			XRT_BYTES_LITERAL("\0binary")
		), "ssh ignore roundtrip failed");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshUnimplementedWrite(&Writer, 0x12345678u) == XSSH_OK) &&
		(xrtSshUnimplementedRead(
			(xbytesview){ arrPayload, Writer.Size },
			&iSequence
		) == XSSH_OK) && (iSequence == 0x12345678u),
		"ssh unimplemented roundtrip failed");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshDebugWrite(
			&Writer,
			true,
			XRT_STR_LITERAL("trace"),
			XRT_STR_LITERAL("")
		) == XSSH_OK) && (xrtSshDebugRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Debug
		) == XSSH_OK) && Debug.AlwaysDisplay &&
		testSshTextEqual(Debug.Message, XRT_STR_LITERAL("trace")),
		"ssh debug roundtrip failed");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshServiceRequestWrite(
			&Writer,
			XRT_STR_LITERAL("ssh-userauth")
		) == XSSH_OK) && (xrtSshServiceRequestRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Service
		) == XSSH_OK) && testSshTextEqual(
			Service.Name,
			XRT_STR_LITERAL("ssh-userauth")
		), "ssh service request roundtrip failed");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshServiceAcceptWrite(
			&Writer,
			XRT_STR_LITERAL("ssh-connection")
		) == XSSH_OK) && (xrtSshServiceAcceptRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Service
		) == XSSH_OK), "ssh service accept roundtrip failed");
}



/* 验证 EXT_INFO 任意二进制值和无分配迭代器。 */
static void testSshTransportExtInfo(void)
{
	static const unsigned char arrBinary[] = { 0u, 1u, 0xffu, 2u };
	xsshextension arrExtensions[2];
	unsigned char arrPayload[256];
	xsshwriter Writer;
	xsshextinfo ExtInfo;
	xsshextension Extension;

	arrExtensions[0].Name = XRT_STR_LITERAL("server-sig-algs");
	arrExtensions[0].Value = XRT_BYTES_LITERAL("ssh-ed25519");
	arrExtensions[1].Name = XRT_STR_LITERAL("example@example.com");
	arrExtensions[1].Value = (xbytesview){ arrBinary, sizeof(arrBinary) };
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshExtInfoWrite(
			&Writer,
			arrExtensions,
			2u
		) == XSSH_OK) && (xrtSshExtInfoRead(
			(xbytesview){ arrPayload, Writer.Size },
			&ExtInfo
		) == XSSH_OK) && (ExtInfo.Count == 2u),
		"ssh ext-info setup failed");
	testRequire(xrtSshExtInfoNext(&ExtInfo, &Extension) &&
		testSshTextEqual(
			Extension.Name,
			arrExtensions[0].Name
		) && testSshBytesEqual(
			Extension.Value,
			arrExtensions[0].Value
		), "ssh ext-info first item failed");
	testRequire(xrtSshExtInfoNext(&ExtInfo, &Extension) &&
		testSshBytesEqual(Extension.Value, arrExtensions[1].Value) &&
		!xrtSshExtInfoNext(&ExtInfo, &Extension),
		"ssh ext-info binary item failed");
}



/* 验证短包、非法名称、重叠和输出原子性。 */
static void testSshTransportFailures(void)
{
	unsigned char arrPayload[128];
	xsshwriter Writer;
	xsshwriter KeepWriter;
	xsshservice Service;
	xsshservice KeepService;
	xsshextinfo ExtInfo;
	xsshextinfo KeepExtInfo;
	size_t iSize;

	memset(arrPayload, 0x5a, sizeof(arrPayload));
	testRequire(xrtSshWriterInit(&Writer, arrPayload, 4u),
		"ssh transport short writer init failed");
	KeepWriter = Writer;
	testRequire((xrtSshDisconnectWrite(
		&Writer,
		XSSH_DISCONNECT_BY_APPLICATION,
		XRT_STR_LITERAL("long"),
		XRT_STR_LITERAL("")
	) == XSSH_ERROR_SPACE) &&
		(memcmp(&Writer, &KeepWriter, sizeof(Writer)) == 0) &&
		(arrPayload[0] == 0x5au), "ssh transport short write was partial");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)),
		"ssh transport overlap writer init failed");
	KeepWriter = Writer;
	testRequire((xrtSshIgnoreWrite(
		&Writer,
		(xbytesview){ arrPayload + 2u, 16u }
	) == XSSH_ERROR_ARGUMENT) &&
		(memcmp(&Writer, &KeepWriter, sizeof(Writer)) == 0),
		"ssh transport overlapping input was accepted");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshServiceRequestWrite(
			&Writer,
			XRT_STR_LITERAL("ssh-userauth")
		) == XSSH_OK), "ssh transport failure setup failed");
	iSize = Writer.Size;
	memset(&KeepService, 0x5a, sizeof(KeepService));
	Service = KeepService;
	testRequire((xrtSshServiceRequestRead(
		(xbytesview){ arrPayload, iSize - 1u },
		&Service
	) == XSSH_NEED_MORE) &&
		(memcmp(&Service, &KeepService, sizeof(Service)) == 0),
		"ssh service truncation changed output");

	arrPayload[5] = ',';
	testRequire((xrtSshServiceRequestRead(
		(xbytesview){ arrPayload, iSize },
		&Service
	) == XSSH_ERROR_PROTOCOL) &&
		(memcmp(&Service, &KeepService, sizeof(Service)) == 0),
		"ssh service invalid name changed output");

	arrPayload[0] = XSSH_MSG_EXT_INFO;
	arrPayload[1] = 0xffu;
	arrPayload[2] = 0xffu;
	arrPayload[3] = 0xffu;
	arrPayload[4] = 0xffu;
	memset(&KeepExtInfo, 0x5a, sizeof(KeepExtInfo));
	ExtInfo = KeepExtInfo;
	testRequire((xrtSshExtInfoRead(
		(xbytesview){ arrPayload, 5u },
		&ExtInfo
	) == XSSH_NEED_MORE) &&
		(memcmp(&ExtInfo, &KeepExtInfo, sizeof(ExtInfo)) == 0),
		"ssh ext-info impossible count changed output");
}



/* 运行 SSH transport 公共消息测试。 */
int main(void)
{
	testSshTransportEmptyMessages();
	testSshTransportCoreMessages();
	testSshTransportExtInfo();
	testSshTransportFailures();
	return 0;
}
