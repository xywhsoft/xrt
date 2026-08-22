#include "../test.h"



/* 固定种子的线性同余序列让随机输入回归保持完全可复现。 */
static uint32 testHttp1MessageRandom(uint32* pState)
{
	*pState = (*pState * UINT32_C(1664525)) + UINT32_C(1013904223);
	return *pState;
}



/* 检查任意解析结果都没有越过输入，成功消息还必须能够原样重放正文。 */
static void testHttp1MessageResult(
	xbytesview Input,
	xhttp1status Status,
	const xhttp1message* pMessage
)
{
	uint8 Output[2048];
	size_t iSize;

	testRequire((Status >= XHTTP1_ERROR) && (Status <= XHTTP1_FIELDS),
		"HTTP/1 Message mutation returned an unknown status");
	if ( Status == XHTTP1_FIELDS ) {
		testRequire(
			(pMessage->Head.FieldCount > pMessage->Head.FieldCapacity) ||
			(pMessage->TrailerCount > pMessage->TrailerCapacity),
			"HTTP/1 Message mutation published FIELDS without a shortage"
		);
		return;
	}
	if ( Status != XHTTP1_READY ) {
		return;
	}
	testRequire((pMessage->Wire.Data == Input.Data) &&
		(pMessage->Wire.Size <= Input.Size),
		"HTTP/1 Message mutation published an out-of-input frame");
	testRequire(xrtHttp1MessageBodyCopy(
		pMessage, NULL, 0, &iSize
	) && (iSize == pMessage->BodyBytes),
		"HTTP/1 Message mutation Body size query disagreed");
	if ( iSize <= sizeof(Output) ) {
		testRequire(xrtHttp1MessageBodyCopy(
			pMessage, Output, sizeof(Output), &iSize
		), "HTTP/1 Message mutation could not replay a ready Body");
	}
}



/* 把有界任意字节同时送入请求和响应入口，覆盖共享分帧器的失败边界。 */
static void testHttp1MessageArbitraryInputs(void)
{
	uint8 Input[2048];
	xhttpfield Fields[16];
	xhttpfield Trailers[16];
	xhttp1bodylimits BodyLimits;
	xhttp1errorinfo Error;
	xhttp1limits HeadLimits;
	xhttp1message Message;
	uint32 iRandom = UINT32_C(0x6A09E667);
	size_t iRound;

	xrtHttp1LimitsInit(&HeadLimits);
	HeadLimits.MaxHead = sizeof(Input);
	HeadLimits.MaxStartLine = 512;
	HeadLimits.MaxFieldLine = 512;
	HeadLimits.MaxFields = 128;
	xrtHttp1BodyLimitsInit(&BodyLimits);
	BodyLimits.MaxBody = sizeof(Input);
	BodyLimits.MaxChunkLine = 512;
	BodyLimits.MaxTrailer = sizeof(Input);
	BodyLimits.MaxTrailerLine = 512;
	BodyLimits.MaxTrailers = 128;

	for ( iRound = 0; iRound < 5000; iRound++ ) {
		xbytesview Bytes;
		xhttp1status Status;
		size_t iSize =
			(size_t)(testHttp1MessageRandom(&iRandom) % sizeof(Input));
		size_t i;

		for ( i = 0; i < iSize; i++ ) {
			Input[i] = (uint8)(testHttp1MessageRandom(&iRandom) >> 24);
		}
		Bytes = (xbytesview){ Input, iSize };

		xrtHttp1MessageInit(&Message, Fields, 16, Trailers, 16);
		Status = xrtHttp1RequestMessageParse(
			Bytes, (testHttp1MessageRandom(&iRandom) & 1u) != 0,
			&Message, &HeadLimits, &BodyLimits, &Error
		);
		testHttp1MessageResult(Bytes, Status, &Message);
		xrtClearError();

		xrtHttp1MessageInit(&Message, Fields, 16, Trailers, 16);
		Status = xrtHttp1ResponseMessageParse(
			Bytes, (testHttp1MessageRandom(&iRandom) & 1u) != 0,
			XRT_STR_LITERAL("GET"),
			&Message, &HeadLimits, &BodyLimits, &Error
		);
		testHttp1MessageResult(Bytes, Status, &Message);
		xrtClearError();
	}
}



/* 每个完整消息前缀在可靠 EOF 下都必须报告截断，不能继续等待。 */
static void testHttp1MessageTruncation(void)
{
	static const uint8 Wire[] =
		"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
		"4\r\nWiki\r\n0\r\nDigest: ok\r\n\r\n";
	xhttpfield Fields[2];
	xhttpfield Trailers[1];
	xhttp1errorinfo Error;
	xhttp1message Message;
	size_t iPrefix;

	for ( iPrefix = 0; iPrefix < (sizeof(Wire) - 1u); iPrefix++ ) {
		xrtHttp1MessageInit(&Message, Fields, 2, Trailers, 1);
		testRequire(xrtHttp1ResponseMessageParse(
			(xbytesview){ Wire, iPrefix }, true,
			XRT_STR_LITERAL("GET"),
			&Message, NULL, NULL, &Error
		) == XHTTP1_ERROR,
			"HTTP/1 Message truncated EOF was not rejected");
		testRequire((Error.Code == XHTTP1_ERROR_HEAD_INCOMPLETE) ||
			(Error.Code == XHTTP1_ERROR_BODY_INCOMPLETE),
			"HTTP/1 Message truncated EOF reported an unrelated error");
		xrtClearError();
	}
}



/* 运行任意输入与全前缀可靠 EOF 回归。 */
int main(void)
{
	testHttp1MessageArbitraryInputs();
	testHttp1MessageTruncation();
	printf("[PASS] http1_message_mutation\n");
	return 0;
}
