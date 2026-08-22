#include "../test.h"



/* 使用独立参考规则穷举全部十六位状态码。 */
static bool testCloseCodeReference(uint16 iCode)
{
	return ((iCode >= 1000u) && (iCode <= 1003u)) ||
		((iCode >= 1007u) && (iCode <= 1014u)) ||
		((iCode >= 3000u) && (iCode <= 4999u));
}



/* 穷举状态码并覆盖每个合法原因长度的写出往返。 */
int main(void)
{
	char Reason[XWS_CLOSE_REASON_MAX];
	uint8 Payload[XWS_CLOSE_PAYLOAD_MAX];
	uint32 iCode;
	size_t iLength;

	for ( iCode = 0; iCode <= UINT16_MAX; iCode++ ) {
		testRequire(
			xrtWsCloseCodeValid((uint16)iCode) ==
				testCloseCodeReference((uint16)iCode),
			"WebSocket Close code exhaustive reference mismatch"
		);
	}

	for ( iLength = 0; iLength <= sizeof(Reason); iLength++ ) {
		xwsclose Close;
		xbytesview Input;
		xstrview Text;
		size_t iSize = 0;
		size_t i;

		for ( i = 0; i < iLength; i++ ) {
			Reason[i] = (char)('a' + (i % 26u));
		}
		Text.Data = Reason;
		Text.Size = iLength;
		testRequire(
			xrtWsCloseWrite(
				XWS_CLOSE_NORMAL,
				Text,
				Payload,
				sizeof(Payload),
				&iSize
			) && (iSize == iLength + 2u),
			"WebSocket Close reason boundary write failed"
		);
		Input.Data = Payload;
		Input.Size = iSize;
		testRequire(
			xrtWsCloseParse(Input, &Close) &&
			(Close.Code == XWS_CLOSE_NORMAL) &&
			(Close.Reason.Size == iLength) &&
			(memcmp(Close.Reason.Data, Reason, iLength) == 0),
			"WebSocket Close reason boundary round trip failed"
		);
	}
	printf("[PASS] websocket_close_property\n");
	return 0;
}
