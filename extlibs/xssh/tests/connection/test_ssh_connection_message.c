#include "../test.h"



/* 验证未知全局请求字段不经解释地完整往返。 */
static void testSshGlobalRequest(void)
{
	static const unsigned char arrFields[] = { 0u, 1u, 2u, 3u };
	unsigned char arrPayload[128];
	xsshwriter Writer;
	xsshglobalrequest Request;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshGlobalRequestWrite(
			&Writer,
			XRT_STR_LITERAL("keepalive@example.com"),
			true,
			(xbytesview){ arrFields, sizeof(arrFields) }
		) == XSSH_OK) && (xrtSshGlobalRequestRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Request
		) == XSSH_OK) && testSshTextEqual(
		Request.Name,
		XRT_STR_LITERAL("keepalive@example.com")
	) && Request.WantReply && testSshBytesEqual(
		Request.Fields,
		(xbytesview){ arrFields, sizeof(arrFields) }
	), "ssh global request mismatch");
}



/* 验证 success 专用数据和无字段 failure。 */
static void testSshGlobalResponses(void)
{
	static const unsigned char arrFields[] = { 0u, 0u, 0x1fu, 0x90u };
	unsigned char arrPayload[32];
	xbytesview Fields;
	xsshwriter Writer;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshGlobalSuccessWrite(
			&Writer,
			(xbytesview){ arrFields, sizeof(arrFields) }
		) == XSSH_OK) && (xrtSshGlobalSuccessRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Fields
		) == XSSH_OK) && testSshBytesEqual(
		Fields,
		(xbytesview){ arrFields, sizeof(arrFields) }
	), "ssh global success mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshGlobalFailureWrite(&Writer) == XSSH_OK) &&
		(xrtSshGlobalFailureRead(
			(xbytesview){ arrPayload, Writer.Size }
		) == XSSH_OK), "ssh global failure mismatch");
}



/* 验证名称、截断、尾随、容量和重叠边界。 */
static void testSshGlobalBoundaries(void)
{
	unsigned char arrPayload[64];
	xsshwriter Writer;
	xsshglobalrequest Request;
	xbytesview Fields;
	size_t iSize;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshGlobalRequestWrite(
			&Writer,
			XRT_STR_LITERAL("bad,name"),
			false,
			(xbytesview){ NULL, 0u }
		) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh global request accepted invalid name");
	testRequire((xrtSshGlobalRequestWrite(
		&Writer,
		XRT_STR_LITERAL("test"),
		false,
		(xbytesview){ arrPayload + 8u, 4u }
	) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh global request accepted overlapping fields");

	testRequire(xrtSshGlobalRequestWrite(
		&Writer,
		XRT_STR_LITERAL("test"),
		false,
		(xbytesview){ NULL, 0u }
	) == XSSH_OK, "ssh global boundary setup failed");
	iSize = Writer.Size;
	testRequire(xrtSshGlobalRequestRead(
		(xbytesview){ arrPayload, iSize - 1u },
		&Request
	) == XSSH_NEED_MORE, "ssh global truncated request was not incremental");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, 4u) &&
		(xrtSshGlobalRequestWrite(
			&Writer,
			XRT_STR_LITERAL("test"),
			false,
			(xbytesview){ NULL, 0u }
		) == XSSH_ERROR_SPACE) && (Writer.Size == 0u),
		"ssh global short request changed state");

	arrPayload[0] = XSSH_MSG_REQUEST_FAILURE;
	arrPayload[1] = 0u;
	testRequire((xrtSshGlobalFailureRead(
		(xbytesview){ arrPayload, 2u }
	) == XSSH_ERROR_PROTOCOL) && (xrtSshGlobalSuccessRead(
		(xbytesview){ arrPayload, 1u },
		&Fields
	) == XSSH_ERROR_PROTOCOL),
		"ssh global response accepted wrong shape or message");
}



/* 运行全局请求与响应消息测试。 */
int main(void)
{
	testSshGlobalRequest();
	testSshGlobalResponses();
	testSshGlobalBoundaries();
	return 0;
}
