#include "../test.h"



/* 生成可重复的伪随机测试数据。 */
static uint32 testHandshakeNext(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 对大量规范 nonce 交叉验证 Base64、Accept 和单字节篡改拒绝。 */
static void testHandshakeAcceptProperty(void)
{
	uint8 Random[XWS_KEY_BYTES];
	char Key[XWS_KEY_CAPACITY];
	char Accept[XWS_ACCEPT_CAPACITY];
	char Mutated[XWS_ACCEPT_CAPACITY];
	uint32 iState = UINT32_C(0x57534853);

	for ( size_t iRound = 0; iRound < 10000u; iRound++ ) {
		size_t iSize = 0;

		for ( size_t i = 0; i < sizeof(Random); i++ ) {
			Random[i] = (uint8)testHandshakeNext(&iState);
		}
		testRequire(
			xrtBase64Encode(
				Random,
				sizeof(Random),
				Key,
				sizeof(Key),
				&iSize,
				NULL
			) &&
			(iSize == XWS_KEY_SIZE) &&
			xrtWsKeyValid((xstrview){ Key, iSize }),
			"WebSocket generated key property failed"
		);
		testRequire(
			xrtWsAccept(
				(xstrview){ Key, iSize },
				Accept,
				sizeof(Accept)
			) &&
			xrtWsAcceptValid(
				(xstrview){ Key, iSize },
				(xstrview){ Accept, XWS_ACCEPT_SIZE }
			),
			"WebSocket accept round-trip property failed"
		);
		memcpy(Mutated, Accept, sizeof(Mutated));
		{
			size_t iChange = iRound % XWS_ACCEPT_SIZE;

			Mutated[iChange] =
				Mutated[iChange] == 'A' ? 'B' : 'A';
		}
		testRequire(
			!xrtWsAcceptValid(
				(xstrview){ Key, iSize },
				(xstrview){ Mutated, XWS_ACCEPT_SIZE }
			),
			"WebSocket mutated accept was accepted"
		);
	}
}



/* 验证长列表、客户端偏好顺序和大小写敏感匹配。 */
static void testHandshakeProtocolProperty(void)
{
	char Client[4096];
	char Server[4096];
	size_t iClient = 0;
	size_t iServer = 0;
	xstrview Selected;

	for ( size_t i = 0; i < 128u; i++ ) {
		int iWritten = snprintf(
			Client + iClient,
			sizeof(Client) - iClient,
			"%sp%u",
			i == 0 ? "" : ", ",
			(unsigned int)i
		);

		testRequire(
			(iWritten > 0) &&
			((size_t)iWritten < (sizeof(Client) - iClient)),
			"WebSocket client protocol fixture overflow"
		);
		iClient += (size_t)iWritten;
	}
	for ( size_t i = 128u; i != 0; i-- ) {
		int iWritten = snprintf(
			Server + iServer,
			sizeof(Server) - iServer,
			"%sp%u",
			i == 128u ? "" : ", ",
			(unsigned int)(i - 1u)
		);

		testRequire(
			(iWritten > 0) &&
			((size_t)iWritten < (sizeof(Server) - iServer)),
			"WebSocket server protocol fixture overflow"
		);
		iServer += (size_t)iWritten;
	}
	testRequire(
		xrtWsProtocolsValid((xstrview){ Client, iClient }) &&
		xrtWsProtocolsValid((xstrview){ Server, iServer }),
		"WebSocket long protocol list validation failed"
	);
	for ( size_t i = 0; i < 128u; i++ ) {
		char Name[16];
		int iSize = snprintf(
			Name,
			sizeof(Name),
			"p%u",
			(unsigned int)i
		);

		testRequire(
			(iSize > 0) &&
			xrtWsProtocolsHas(
				(xstrview){ Client, iClient },
				(xstrview){ Name, (size_t)iSize }
			),
			"WebSocket long protocol lookup failed"
		);
	}
	testRequire(
		xrtWsProtocolSelect(
			(xstrview){ Client, iClient },
			(xstrview){ Server, iServer },
			&Selected
		) &&
		(Selected.Size == 2u) &&
		(memcmp(Selected.Data, "p0", 2u) == 0),
		"WebSocket long protocol selection lost client preference"
	);
}



/* 运行 WebSocket 握手性质测试。 */
int main(void)
{
	testHandshakeAcceptProperty();
	testHandshakeProtocolProperty();
	printf("[PASS] websocket_handshake_property\n");
	return 0;
}
