#include "../test.h"



/* 验证 ECDH init 与 reply 的确定性线格式。 */
static void testSshEcdhRoundtrip(void)
{
	static const unsigned char arrInitExpected[] = {
		XSSH_MSG_KEX_ECDH_INIT, 0u, 0u, 0u, 3u, 1u, 2u, 3u
	};
	unsigned char arrPayload[64];
	xsshwriter Writer;
	xsshecdhinit Init;
	xsshecdhreply Reply;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshEcdhInitWrite(
			&Writer,
			XRT_BYTES_LITERAL("\1\2\3")
		) == XSSH_OK) && (Writer.Size == sizeof(arrInitExpected)) &&
		(memcmp(arrPayload, arrInitExpected, sizeof(arrInitExpected)) == 0) &&
		(xrtSshEcdhInitRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Init
		) == XSSH_OK) && testSshBytesEqual(
			Init.ClientPublic,
			XRT_BYTES_LITERAL("\1\2\3")
		), "ssh ecdh init roundtrip failed");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshEcdhReplyWrite(
			&Writer,
			XRT_BYTES_LITERAL("host"),
			XRT_BYTES_LITERAL("public"),
			XRT_BYTES_LITERAL("signature")
		) == XSSH_OK) && (xrtSshEcdhReplyRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Reply
		) == XSSH_OK) && testSshBytesEqual(
			Reply.ServerHostKey,
			XRT_BYTES_LITERAL("host")
		) && testSshBytesEqual(
			Reply.ServerPublic,
			XRT_BYTES_LITERAL("public")
		) && testSshBytesEqual(
			Reply.Signature,
			XRT_BYTES_LITERAL("signature")
		), "ssh ecdh reply roundtrip failed");
}



/* 验证容量、截断和尾随数据不发布部分状态。 */
static void testSshEcdhFailureAtomic(void)
{
	unsigned char arrPayload[32];
	xsshwriter Writer;
	xsshecdhinit Init;
	xsshecdhinit Keep;
	size_t iPayloadSize;

	memset(arrPayload, 0x5a, sizeof(arrPayload));
	testRequire(xrtSshWriterInit(&Writer, arrPayload, 4u) &&
		(xrtSshEcdhInitWrite(
			&Writer,
			XRT_BYTES_LITERAL("public")
		) == XSSH_ERROR_SPACE) && (Writer.Size == 0u) &&
		(arrPayload[0] == 0x5au), "ssh ecdh short write was partial");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshEcdhInitWrite(
			&Writer,
			XRT_BYTES_LITERAL("public")
		) == XSSH_OK), "ssh ecdh failure setup failed");
	iPayloadSize = Writer.Size;
	memset(&Keep, 0x5a, sizeof(Keep));
	Init = Keep;
	testRequire((xrtSshEcdhInitRead(
		(xbytesview){ arrPayload, iPayloadSize - 1u },
		&Init
	) == XSSH_NEED_MORE) && (memcmp(&Init, &Keep, sizeof(Init)) == 0),
		"ssh ecdh truncation changed output");
	arrPayload[iPayloadSize] = 0u;
	testRequire((xrtSshEcdhInitRead(
		(xbytesview){ arrPayload, iPayloadSize + 1u },
		&Init
	) == XSSH_ERROR_PROTOCOL) &&
		(memcmp(&Init, &Keep, sizeof(Init)) == 0),
		"ssh ecdh trailing data was accepted");

	memset(arrPayload, 0x5a, sizeof(arrPayload));
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)),
		"ssh ecdh overlap writer init failed");
	testRequire((xrtSshEcdhInitWrite(
		&Writer,
		(xbytesview){ arrPayload + 2u, 8u }
	) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u) &&
		(arrPayload[0] == 0x5au), "ssh ecdh overlapping input was accepted");
}



/* 运行 ECDH 报文编解码测试。 */
int main(void)
{
	testSshEcdhRoundtrip();
	testSshEcdhFailureAtomic();
	return 0;
}
