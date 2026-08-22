#include "../test.h"



/* 固定种子的线性同余序列保证每次回归覆盖相同分片。 */
static uint32 testHttp1BodyRandom(uint32* pState)
{
	*pState = (*pState * UINT32_C(1664525)) + UINT32_C(1013904223);
	return *pState;
}



/* 验证读取端发布的借用正文完全位于本轮输入窗口内。 */
static bool testHttp1BodyViewInside(xbytesview Data, xbytesview Input)
{
	uintptr_t iData = (uintptr_t)Data.Data;
	uintptr_t iInput = (uintptr_t)Input.Data;

	if ( Data.Size == 0 ) {
		return false;
	}
	return (Data.Data != NULL) && (iData >= iInput) &&
		((iData - iInput) <= Input.Size) &&
		(Data.Size <= (Input.Size - (iData - iInput)));
}



/* 用随机可见窗口推进一份完整 chunked 报文并检查全部公开计数不变量。 */
static void testHttp1BodyFragmented(
	const uint8* pWire,
	size_t iWireSize,
	const uint8* pPayload,
	size_t iPayloadSize,
	uint32* pRandom
)
{
	xhttpfield Trailers[2];
	xhttp1bodylimits Limits;
	xhttp1errorinfo Error;
	xhttp1bodyplan Plan = { XHTTP1_BODY_CHUNKED, 0 };
	xhttp1body Body;
	size_t iVisible = 0;
	size_t iOffset = 0;
	size_t iPayload = 0;
	size_t iGuard = 0;
	xhttp1bodystatus Status = XHTTP1_BODY_MORE;

	xrtHttp1BodyLimitsInit(&Limits);
	Limits.MaxBody = iPayloadSize + 1u;
	testRequire(xrtHttp1BodyInit(
		&Body, &Plan, Trailers, 2, &Limits
	), "HTTP/1 mutation Body init failed");
	while ( iGuard++ < (iWireSize * 6u + 128u) ) {
		xbytesview Input;
		xbytesview Data;
		size_t iConsumed = 0;
		uint64 iReceived = Body.Received;
		uint64 iWireBytes = Body.WireBytes;

		if ( (iVisible < iWireSize) &&
			((iVisible == iOffset) || (Status == XHTTP1_BODY_MORE)) ) {
			size_t iAdd = 1u +
				(size_t)(testHttp1BodyRandom(pRandom) % 29u);

			iVisible = ((iWireSize - iVisible) < iAdd) ?
				iWireSize : (iVisible + iAdd);
		}
		Input.Data = pWire + iOffset;
		Input.Size = iVisible - iOffset;
		Status = xrtHttp1BodyRead(
			&Body, Input, iVisible == iWireSize,
			&iConsumed, &Data, &Error
		);
		testRequire(iConsumed <= Input.Size,
			"HTTP/1 mutation consumed beyond input");
		testRequire((Body.Received >= iReceived) &&
			(Body.WireBytes >= iWireBytes) &&
			(Body.Received <= iPayloadSize) &&
			(Body.WireBytes <= (uint64)(iOffset + iConsumed)),
			"HTTP/1 mutation counters moved outside their bounds");
		if ( Status == XHTTP1_BODY_DATA ) {
			testRequire(testHttp1BodyViewInside(Data, Input),
				"HTTP/1 mutation published an out-of-window view");
			testRequire((Data.Size <= (iPayloadSize - iPayload)) &&
				(memcmp(Data.Data, pPayload + iPayload, Data.Size) == 0),
				"HTTP/1 mutation changed decoded payload bytes");
			iPayload += Data.Size;
		}
		iOffset += iConsumed;
		if ( Status == XHTTP1_BODY_DONE ) {
			break;
		}
		testRequire((Status == XHTTP1_BODY_MORE) ||
			(Status == XHTTP1_BODY_DATA),
			"HTTP/1 valid mutation fixture reached an invalid state");
	}
	testRequire((Status == XHTTP1_BODY_DONE) &&
		(iOffset == iWireSize) && (iPayload == iPayloadSize) &&
		xrtHttp1BodyDone(&Body),
		"HTTP/1 fragmented mutation fixture did not finish exactly");
}



/* 随机生成正文和 chunk 边界，再使用发送端与读取端做确定性回环。 */
static void testHttp1BodyRandomRoundTrips(void)
{
	static const xhttpfield Trailers[] = {
		{ XRT_STR_INIT("Digest"), XRT_STR_INIT("sha-256=:fixed:") }
	};
	uint8 Payload[4096];
	uint8 Wire[32768];
	uint32 iRandom = UINT32_C(0x243F6A88);
	size_t iRound;

	for ( iRound = 0; iRound < 300; iRound++ ) {
		size_t iPayloadSize =
			(size_t)(testHttp1BodyRandom(&iRandom) %
				(sizeof(Payload) + 1u));
		size_t iPayload = 0;
		size_t iWire = 0;
		size_t i;

		for ( i = 0; i < iPayloadSize; i++ ) {
			Payload[i] = (uint8)(testHttp1BodyRandom(&iRandom) >> 24);
		}
		while ( iPayload < iPayloadSize ) {
			size_t iChunk = 1u +
				(size_t)(testHttp1BodyRandom(&iRandom) % 127u);
			size_t iWritten;

			if ( iChunk > (iPayloadSize - iPayload) ) {
				iChunk = iPayloadSize - iPayload;
			}
			testRequire(xrtHttp1ChunkWrite(
				(xbytesview){ Payload + iPayload, iChunk },
				Wire + iWire, sizeof(Wire) - iWire, &iWritten
			), "HTTP/1 mutation chunk writer failed");
			iPayload += iChunk;
			iWire += iWritten;
		}
		{
			size_t iWritten;

			testRequire(xrtHttp1ChunkEndWrite(
				Trailers, 1, Wire + iWire,
				sizeof(Wire) - iWire, &iWritten
			), "HTTP/1 mutation last chunk writer failed");
			iWire += iWritten;
		}
		testHttp1BodyFragmented(
			Wire, iWire, Payload, iPayloadSize, &iRandom
		);
	}
}



/* 每个终止 chunk 前缀在可靠 EOF 下都必须失败，不能误报完整消息。 */
static void testHttp1BodyTruncation(void)
{
	static const uint8 Wire[] =
		"4;foo=bar\r\ntest\r\n0\r\nDigest: ok\r\n\r\n";
	size_t iPrefix;

	for ( iPrefix = 0; iPrefix < (sizeof(Wire) - 1u); iPrefix++ ) {
		xhttpfield Trailers[1];
		xhttp1body Body;
		xhttp1bodylimits Limits;
		xhttp1bodyplan Plan = { XHTTP1_BODY_CHUNKED, 0 };
		xhttp1errorinfo Error;
		size_t iOffset = 0;
		size_t iGuard = 0;
		xhttp1bodystatus Status = XHTTP1_BODY_MORE;

		xrtHttp1BodyLimitsInit(&Limits);
		testRequire(xrtHttp1BodyInit(
			&Body, &Plan, Trailers, 1, &Limits
		), "HTTP/1 truncation Body init failed");
		while ( iGuard++ < (iPrefix + 32u) ) {
			xbytesview Data;
			size_t iConsumed = 0;

			Status = xrtHttp1BodyRead(
				&Body,
				(xbytesview){ Wire + iOffset, iPrefix - iOffset },
				true, &iConsumed, &Data, &Error
			);
			testRequire(iConsumed <= (iPrefix - iOffset),
				"HTTP/1 truncation consumed beyond prefix");
			iOffset += iConsumed;
			if ( Status != XHTTP1_BODY_DATA ) {
				break;
			}
		}
		testRequire((Status == XHTTP1_BODY_ERROR) &&
			(Error.Code == XHTTP1_ERROR_BODY_INCOMPLETE),
			"HTTP/1 truncated chunked body was accepted");
		xrtClearError();
	}
}



/* 执行发送端生成、随机网络分片和全前缀 EOF 回归。 */
int main(void)
{
	testHttp1BodyRandomRoundTrips();
	testHttp1BodyTruncation();
	printf("[PASS] http1_body_mutation\n");
	return 0;
}
