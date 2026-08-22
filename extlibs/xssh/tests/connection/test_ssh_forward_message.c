#include "../test.h"



/* 验证 tcpip-forward、cancel 和动态端口 success。 */
static void testSshForwardGlobal(void)
{
	static const unsigned char arrAddress[] = { '0', '.', '0', '.', '0', '.', '0' };
	unsigned char arrPayload[128];
	xsshglobalrequest Request;
	xsshtcpipforward Forward;
	xsshwriter Writer;
	uint32 iPort;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshTcpipForwardWrite(
			&Writer,
			(xbytesview){ arrAddress, sizeof(arrAddress) },
			0u
		) == XSSH_OK) && (xrtSshGlobalRequestRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Request
		) == XSSH_OK) && (xrtSshTcpipForwardRead(
			&Request,
			&Forward
		) == XSSH_OK) && Request.WantReply &&
		testSshBytesEqual(
			Forward.Address,
			(xbytesview){ arrAddress, sizeof(arrAddress) }
		) && (Forward.Port == 0u), "ssh tcpip-forward mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshTcpipForwardCancelWrite(
			&Writer,
			XRT_BYTES_LITERAL("127.0.0.1"),
			2222u
		) == XSSH_OK) && (xrtSshGlobalRequestRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Request
		) == XSSH_OK) && (xrtSshTcpipForwardCancelRead(
			&Request,
			&Forward
		) == XSSH_OK) && (Forward.Port == 2222u),
		"ssh cancel-tcpip-forward mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshTcpipForwardSuccessWrite(&Writer, UINT32_MAX) == XSSH_OK) &&
		(xrtSshTcpipForwardSuccessRead(
			(xbytesview){ arrPayload, Writer.Size },
			&iPort
		) == XSSH_OK) && (iPort == UINT32_MAX),
		"ssh tcpip-forward success mismatch");
}



/* 验证 direct-tcpip 和 forwarded-tcpip 公共字段。 */
static void testSshForwardChannels(void)
{
	static const unsigned char arrHost[] = { 'd', 'b', 0u, 'x' };
	unsigned char arrPayload[192];
	xsshchannelopen Open;
	xsshtcpipopen Tcpip;
	xsshwriter Writer;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshDirectTcpipOpenWrite(
			&Writer,
			11u,
			UINT32_MAX,
			32768u,
			(xbytesview){ arrHost, sizeof(arrHost) },
			5432u,
			XRT_BYTES_LITERAL("127.0.0.1"),
			50000u
		) == XSSH_OK) && (xrtSshChannelOpenRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Open
		) == XSSH_OK) && (xrtSshDirectTcpipOpenRead(
			&Open,
			&Tcpip
		) == XSSH_OK) && (Open.Sender == 11u) &&
		(Open.Window == UINT32_MAX) && (Open.MaxPacket == 32768u) &&
		testSshBytesEqual(
			Tcpip.Host,
			(xbytesview){ arrHost, sizeof(arrHost) }
		) && (Tcpip.Port == 5432u) && testSshBytesEqual(
			Tcpip.Originator,
			XRT_BYTES_LITERAL("127.0.0.1")
		) && (Tcpip.OriginatorPort == 50000u),
		"ssh direct-tcpip mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshForwardedTcpipOpenWrite(
			&Writer,
			12u,
			65536u,
			16384u,
			XRT_BYTES_LITERAL("0.0.0.0"),
			10022u,
			XRT_BYTES_LITERAL("192.0.2.8"),
			45000u
		) == XSSH_OK) && (xrtSshChannelOpenRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Open
		) == XSSH_OK) && (xrtSshForwardedTcpipOpenRead(
			&Open,
			&Tcpip
		) == XSSH_OK) && (Tcpip.Port == 10022u) &&
		(Tcpip.OriginatorPort == 45000u), "ssh forwarded-tcpip mismatch");
}



/* 验证类型、回复位、尾随、容量、重叠和 max-packet 边界。 */
static void testSshForwardBoundaries(void)
{
	unsigned char arrPayload[128];
	xsshglobalrequest Request;
	xsshtcpipforward Forward = { XRT_BYTES_LITERAL("keep"), 9u };
	xsshchannelopen Open;
	xsshtcpipopen Tcpip = {
		XRT_BYTES_LITERAL("keep"), 1u, XRT_BYTES_LITERAL("keep"), 2u
	};
	xsshwriter Writer;
	uint32 iPort = 7u;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, 8u) &&
		(xrtSshTcpipForwardWrite(
			&Writer,
			XRT_BYTES_LITERAL("x"),
			1u
		) == XSSH_ERROR_SPACE) && (Writer.Size == 0u),
		"ssh forwarding short writer changed state");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshTcpipForwardWrite(
			&Writer,
			(xbytesview){ arrPayload + 8u, 8u },
			1u
		) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh forwarding writer accepted overlapping address");
	testRequire((xrtSshDirectTcpipOpenWrite(
		&Writer,
		1u,
		1u,
		0u,
		XRT_BYTES_LITERAL("host"),
		1u,
		XRT_BYTES_LITERAL("origin"),
		2u
	) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh forwarding accepted zero max-packet");

	testRequire(xrtSshTcpipForwardWrite(
		&Writer,
		XRT_BYTES_LITERAL("host"),
		1u
	) == XSSH_OK, "ssh forwarding boundary setup failed");
	testRequire(xrtSshGlobalRequestRead(
		(xbytesview){ arrPayload, Writer.Size },
		&Request
	) == XSSH_OK, "ssh forwarding boundary envelope failed");
	Request.WantReply = false;
	testRequire((xrtSshTcpipForwardRead(&Request, &Forward) ==
		XSSH_ERROR_ARGUMENT) && (Forward.Port == 9u),
		"ssh forwarding accepted no-reply request or changed output");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshDirectTcpipOpenWrite(
			&Writer,
			1u,
			1u,
			1u,
			XRT_BYTES_LITERAL("host"),
			1u,
			XRT_BYTES_LITERAL("origin"),
			2u
		) == XSSH_OK) && (xrtSshChannelOpenRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Open
		) == XSSH_OK), "ssh forwarding open boundary setup failed");
	Open.Fields.Size++;
	arrPayload[Writer.Size] = 0u;
	testRequire((xrtSshDirectTcpipOpenRead(&Open, &Tcpip) ==
		XSSH_ERROR_PROTOCOL) && (Tcpip.Port == 1u),
		"ssh forwarding open accepted trailing bytes or changed output");

	arrPayload[0] = XSSH_MSG_REQUEST_SUCCESS;
	arrPayload[1] = 0u;
	arrPayload[2] = 0u;
	arrPayload[3] = 0u;
	testRequire((xrtSshTcpipForwardSuccessRead(
		(xbytesview){ arrPayload, 4u },
		&iPort
	) == XSSH_NEED_MORE) && (iPort == 7u),
		"ssh forwarding success truncation changed output");
}



/* 运行 RFC 4254 TCP/IP forwarding 报文与边界测试。 */
int main(void)
{
	testSshForwardGlobal();
	testSshForwardChannels();
	testSshForwardBoundaries();
	return 0;
}
