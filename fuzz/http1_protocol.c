#include <stdlib.h>
#include <string.h>

#include <xrt/http1.h>
#include <xrt/http_connection.h>
#include <xrt/http_expect.h>
#include <xrt/http_te.h>
#include <xrt/http_trailer.h>



#define XRT_HTTP1_FUZZ_INPUT_MAX ((size_t)1048576u)
#define XRT_HTTP1_FUZZ_HEAD_MAX ((size_t)131072u)
#define XRT_HTTP1_FUZZ_TEXT_MAX ((size_t)65536u)
#define XRT_HTTP1_FUZZ_TRAILER_MAX ((size_t)65536u)
#define XRT_HTTP1_FUZZ_BODY_MAX UINT64_C(524288)
#define XRT_HTTP1_FUZZ_FIELDS 32u
#define XRT_HTTP1_FUZZ_TRAILERS 16u
#define XRT_HTTP1_FUZZ_COPY_MAX 4096u



/* 返回输入中的一个选择字节，短输入按零补齐。 */
static uint8 __xrtHttp1FuzzByte(
	const uint8* pData,
	size_t iSize,
	size_t iOffset
)
{
	return iOffset < iSize ? pData[iOffset] : 0;
}



/* 判断借用视图是否完整落在原始输入范围内。 */
static bool __xrtHttp1FuzzViewInside(
	xbytesview Input,
	xstrview View
)
{
	uintptr_t iInput;
	uintptr_t iView;

	if ( View.Data == NULL ) {
		return View.Size == 0;
	}
	if ( Input.Data == NULL ) {
		return false;
	}
	iInput = (uintptr_t)Input.Data;
	iView = (uintptr_t)View.Data;
	return (iView >= iInput) &&
		((iView - iInput) <= Input.Size) &&
		(View.Size <= (Input.Size - (iView - iInput)));
}



/* 验证已完成 Header 的全部借用范围和公开计数。 */
static void __xrtHttp1FuzzHeadReady(
	xbytesview Input,
	const xhttp1head* pHead,
	size_t iCapacity
)
{
	if ( (pHead->Bytes > Input.Size) ||
		(pHead->FieldCount > iCapacity) ||
		!__xrtHttp1FuzzViewInside(Input, pHead->Method) ||
		!__xrtHttp1FuzzViewInside(Input, pHead->Target) ||
		!__xrtHttp1FuzzViewInside(Input, pHead->Reason) ) {
		abort();
	}
	for ( size_t i = 0; i < pHead->FieldCount; i++ ) {
		if ( !__xrtHttp1FuzzViewInside(Input, pHead->Fields[i].Name) ||
			!__xrtHttp1FuzzViewInside(Input, pHead->Fields[i].Value) ) {
			abort();
		}
	}
}



/* 迭代任意 Transfer-Encoding 文本并检查游标单调前进。 */
static void __xrtHttp1FuzzTransferCoding(xstrview Value)
{
	xhttp1transfercoding Coding;
	size_t iOffset = 0;
	size_t iGuard = 0;

	for ( ;; ) {
		size_t iBefore = iOffset;
		xhttpnext Next = xrtHttp1TransferCodingNext(
			Value, &iOffset, &Coding
		);

		if ( Next != XHTTP_NEXT_ITEM ) {
			break;
		}
		if ( (iOffset <= iBefore) || (iOffset > Value.Size) ||
			(Coding.Name.Size == 0) ||
			(++iGuard > (Value.Size + 1u)) ) {
			abort();
		}
	}
	xrtClearError();

}



/* 以单字段和重复字段路径覆盖 Expect 的完整预校验与借用范围。 */
static void __xrtHttp1FuzzExpect(
	const uint8* pData,
	size_t iSize
)
{
	xstrview Value = { (cstr)pData, iSize };
	xhttpexpectcursor Cursor;
	xhttpexpectation Expectation;
	xhttpfield Fields[2];
	xhttpexpectfieldcursor FieldCursor;
	xhttpnext Next;
	size_t iCount = 0;
	size_t iItems = 0;
	size_t iGuard = 0;
	bool bValid;

	bValid = xrtHttpExpectCount(Value, &iCount);
	xrtClearError();
	if ( xrtHttpExpectValid(Value) != bValid ) {
		abort();
	}
	xrtClearError();
	xrtHttpExpectCursorInit(&Cursor);
	for ( ;; ) {
		size_t iBefore = Cursor.Offset;

		Next = xrtHttpExpectNext(
			Value, &Cursor, &Expectation
		);
		if ( Next != XHTTP_NEXT_ITEM ) {
			break;
		}
		if ( !bValid || (Cursor.Offset <= iBefore) ||
			(Cursor.Offset > Value.Size) ||
			(Expectation.Name.Size == 0) ||
			!__xrtHttp1FuzzViewInside(
				(xbytesview){ pData, iSize },
				Expectation.Element
			) || (++iGuard > (Value.Size + 1u)) ) {
			abort();
		}
		iItems++;
	}
	if ( bValid && ((Next != XHTTP_NEXT_END) ||
		(iItems != iCount)) ) {
		abort();
	}
	xrtClearError();

	Fields[0] = (xhttpfield){
		XRT_STR_LITERAL("Expect"),
		(xstrview){ (cstr)pData, iSize / 2u }
	};
	Fields[1] = (xhttpfield){
		XRT_STR_LITERAL("expect"),
		(xstrview){
			(cstr)(pData == NULL ? NULL :
				(pData + (iSize / 2u))),
			iSize - (iSize / 2u)
		}
	};
	(void)xrtHttpExpectFields(Fields, 2u);
	xrtClearError();
	xrtHttpExpectFieldCursorInit(&FieldCursor);
	iGuard = 0;
	for ( ;; ) {
		Next = xrtHttpExpectFieldNext(
			Fields, 2u, &FieldCursor, &Expectation
		);
		if ( Next != XHTTP_NEXT_ITEM ) {
			break;
		}
		if ( !__xrtHttp1FuzzViewInside(
				(xbytesview){ pData, iSize },
				Expectation.Element
			) || (++iGuard > (iSize + 2u)) ) {
			abort();
		}
	}
	xrtClearError();

	memset(&Expectation, 0xA5, sizeof(Expectation));
	if ( !xrtHttpExpectationParse(Value, &Expectation) &&
		((Expectation.Element.Data != NULL) ||
		 (Expectation.Name.Data != NULL)) ) {
		abort();
	}
	xrtClearError();
}



/* 覆盖 TE 单字段、重复字段、汇总与 Connection 逐跳组合的公开游标。 */
static void __xrtHttp1FuzzTe(
	const uint8* pData,
	size_t iSize
)
{
	xstrview Value = { (cstr)pData, iSize };
	xhttpfield Fields[2];
	xhttptecursor Cursor;
	xhttptefieldcursor FieldCursor;
	xhttpfieldtokencursor ConnectionCursor;
	xhttptecoding Coding;
	xhttpteinfo Info;
	xstrview Option;
	xhttpnext Next;
	size_t iCount = 0;
	size_t iItems = 0;
	size_t iTrailerNames = 0;
	size_t iGuard = 0;
	bool bValid;

	bValid = xrtHttpTeCount(Value, &iCount);
	xrtClearError();
	if ( xrtHttpTeValid(Value) != bValid ) {
		abort();
	}
	xrtClearError();
	xrtHttpTeCursorInit(&Cursor);
	for ( ;; ) {
		size_t iBefore = Cursor.Offset;

		Next = xrtHttpTeNext(Value, &Cursor, &Coding);
		if ( Next != XHTTP_NEXT_ITEM ) {
			break;
		}
		if ( !bValid || (Cursor.Offset <= iBefore) ||
			(Cursor.Offset > Value.Size) ||
			(Coding.Coding.Size == 0) ||
			(Coding.Quality > XHTTP_QUALITY_MAX) ||
			!__xrtHttp1FuzzViewInside(
				(xbytesview){ pData, iSize }, Coding.Element
			) || (++iGuard > (Value.Size + 1u)) ) {
			abort();
		}
		iItems++;
	}
	if ( bValid && ((Next != XHTTP_NEXT_END) ||
		(iItems != iCount)) ) {
		abort();
	}
	xrtClearError();

	memset(&Coding, 0xA5, sizeof(Coding));
	if ( !xrtHttpTeCodingParse(Value, &Coding) &&
		((Coding.Element.Data != NULL) ||
		 (Coding.Coding.Data != NULL)) ) {
		abort();
	}
	xrtClearError();

	Fields[0] = (xhttpfield){
		XRT_STR_LITERAL("TE"),
		(xstrview){ (cstr)pData, iSize / 2u }
	};
	Fields[1] = (xhttpfield){
		XRT_STR_LITERAL("te"),
		(xstrview){
			(cstr)(pData == NULL ? NULL :
				(pData + (iSize / 2u))),
			iSize - (iSize / 2u)
		}
	};
	(void)xrtHttpTeParse(Fields, 2u, &Info);
	xrtClearError();
	(void)xrtHttpTeQuality(
		Fields, 2u, XRT_STR_LITERAL("gzip")
	);
	xrtClearError();
	(void)xrtHttpTeAcceptsTrailers(Fields, 2u);
	xrtClearError();
	xrtHttpTeFieldCursorInit(&FieldCursor);
	iGuard = 0;
	for ( ;; ) {
		Next = xrtHttpTeFieldNext(
			Fields, 2u, &FieldCursor, &Coding
		);
		if ( Next != XHTTP_NEXT_ITEM ) {
			break;
		}
		if ( !__xrtHttp1FuzzViewInside(
			(xbytesview){ pData, iSize }, Coding.Element
		) || (++iGuard > (iSize + 2u)) ) {
			abort();
		}
	}
	xrtClearError();

	Fields[0].Name = XRT_STR_LITERAL("Connection");
	Fields[1].Name = XRT_STR_LITERAL("connection");
	xrtHttpConnectionCursorInit(&ConnectionCursor);
	iGuard = 0;
	for ( ;; ) {
		Next = xrtHttpConnectionNext(
			Fields, 2u, &ConnectionCursor, &Option
		);
		if ( Next != XHTTP_NEXT_ITEM ) {
			break;
		}
		if ( !__xrtHttp1FuzzViewInside(
			(xbytesview){ pData, iSize }, Option
		) || (++iGuard > (iSize + 2u)) ) {
			abort();
		}
	}
	xrtClearError();

	Fields[0].Name = XRT_STR_LITERAL("Trailer");
	Fields[1].Name = XRT_STR_LITERAL("trailer");
	(void)xrtHttpTrailerCount(
		Fields, 2u, &iTrailerNames
	);
	xrtClearError();
	(void)xrtHttpTrailerFind(
		Fields, 2u, XRT_STR_LITERAL("Digest")
	);
	xrtClearError();
}



/* 让请求和响应 Header 解析器处理同一份任意输入。 */
static void __xrtHttp1FuzzHead(
	const uint8* pData,
	size_t iSize
)
{
	xhttpfield Fields[XRT_HTTP1_FUZZ_FIELDS];
	xhttp1errorinfo Error;
	xhttp1limits Limits;
	xhttp1head Head;
	xbytesview Input = { pData, iSize };
	xhttp1status Status;

	xrtHttp1LimitsInit(&Limits);
	Limits.MaxHead = XRT_HTTP1_FUZZ_HEAD_MAX;
	Limits.MaxStartLine = XRT_HTTP1_FUZZ_TEXT_MAX;
	Limits.MaxFieldLine = XRT_HTTP1_FUZZ_TEXT_MAX;
	Limits.MaxFields = XRT_HTTP1_FUZZ_FIELDS;

	xrtHttp1HeadInit(&Head, Fields, XRT_HTTP1_FUZZ_FIELDS);
	Status = xrtHttp1RequestParse(Input, &Head, &Limits, &Error);
	if ( Status == XHTTP1_READY ) {
		__xrtHttp1FuzzHeadReady(Input, &Head, XRT_HTTP1_FUZZ_FIELDS);
		for ( size_t i = 0; i < Head.FieldCount; i++ ) {
			if ( xrtHttpFieldNameEqual(
				Head.Fields[i].Name,
				XRT_STR_LITERAL("Transfer-Encoding")
			) ) {
				__xrtHttp1FuzzTransferCoding(Head.Fields[i].Value);
			}
		}
	}
	xrtClearError();

	xrtHttp1HeadInit(&Head, Fields, XRT_HTTP1_FUZZ_FIELDS);
	Status = xrtHttp1ResponseParse(Input, &Head, &Limits, &Error);
	if ( Status == XHTTP1_READY ) {
		__xrtHttp1FuzzHeadReady(Input, &Head, XRT_HTTP1_FUZZ_FIELDS);
	}
	xrtClearError();
	__xrtHttp1FuzzTransferCoding((xstrview){ (cstr)pData, iSize });
}



/* 使用一种正文计划驱动任意输入，并验证消费和借用范围。 */
static void __xrtHttp1FuzzBodyPlan(
	const uint8* pData,
	size_t iSize,
	const xhttp1bodyplan* pPlan,
	bool bEnd
)
{
	xhttpfield Trailers[XRT_HTTP1_FUZZ_TRAILERS];
	xhttp1bodylimits Limits;
	xhttp1errorinfo Error;
	xhttp1body Body;
	size_t iPosition = 0;
	size_t iGuard = 0;

	xrtHttp1BodyLimitsInit(&Limits);
	Limits.MaxBody = XRT_HTTP1_FUZZ_BODY_MAX;
	Limits.MaxChunkLine = XRT_HTTP1_FUZZ_TEXT_MAX;
	Limits.MaxTrailer = XRT_HTTP1_FUZZ_TRAILER_MAX;
	Limits.MaxTrailerLine = XRT_HTTP1_FUZZ_TEXT_MAX;
	Limits.MaxTrailers = XRT_HTTP1_FUZZ_TRAILERS;
	if ( !xrtHttp1BodyInit(
		&Body, pPlan, Trailers, XRT_HTTP1_FUZZ_TRAILERS, &Limits
	) ) {
		xrtClearError();
		return;
	}

	while ( iGuard++ <= (iSize + 8u) ) {
		const uint8* pInput = (pData == NULL) ? NULL :
			(pData + iPosition);
		xbytesview Input = { pInput, iSize - iPosition };
		xbytesview Data;
		size_t iConsumed = 0;
		xhttp1bodystatus Status = xrtHttp1BodyRead(
			&Body, Input, bEnd, &iConsumed, &Data, &Error
		);

		if ( iConsumed > Input.Size ) {
			abort();
		}
		if ( Status == XHTTP1_BODY_DATA ) {
			if ( (iConsumed == 0) || (Data.Size == 0) ||
				!__xrtHttp1FuzzViewInside(
					Input,
					(xstrview){ (cstr)Data.Data, Data.Size }
				) ) {
				abort();
			}
		}
		iPosition += iConsumed;
		if ( Status != XHTTP1_BODY_DATA ) {
			break;
		}
	}
	if ( iGuard > (iSize + 9u) ) {
		abort();
	}
	xrtClearError();
}



/* 覆盖 fixed、chunked、close 和独立 trailer 解析状态机。 */
static void __xrtHttp1FuzzBody(
	const uint8* pData,
	size_t iSize
)
{
	xhttpfield Trailers[XRT_HTTP1_FUZZ_TRAILERS];
	xhttp1bodylimits Limits;
	xhttp1errorinfo Error;
	xhttp1bodyplan Plan;
	size_t iBytes;
	size_t iCount;
	bool bChunkedEnd;
	bool bCloseEnd;
	size_t iBound = iSize > (size_t)XRT_HTTP1_FUZZ_BODY_MAX ?
		(size_t)XRT_HTTP1_FUZZ_BODY_MAX : iSize;

	Plan = (xhttp1bodyplan){
		XHTTP1_BODY_FIXED,
		(uint64)(__xrtHttp1FuzzByte(pData, iSize, 0) %
			(iBound + 1u))
	};
	__xrtHttp1FuzzBodyPlan(pData, iSize, &Plan, true);
	bChunkedEnd =
		(__xrtHttp1FuzzByte(pData, iSize, 1) & 1u) != 0;
	Plan = (xhttp1bodyplan){ XHTTP1_BODY_CHUNKED, 0 };
	__xrtHttp1FuzzBodyPlan(
		pData, iSize, &Plan, bChunkedEnd
	);
	bCloseEnd =
		(__xrtHttp1FuzzByte(pData, iSize, 2) & 1u) != 0;
	Plan = (xhttp1bodyplan){ XHTTP1_BODY_CLOSE, 0 };
	__xrtHttp1FuzzBodyPlan(
		pData, iSize, &Plan, bCloseEnd
	);

	xrtHttp1BodyLimitsInit(&Limits);
	Limits.MaxBody = XRT_HTTP1_FUZZ_BODY_MAX;
	Limits.MaxChunkLine = XRT_HTTP1_FUZZ_TEXT_MAX;
	Limits.MaxTrailer = XRT_HTTP1_FUZZ_TRAILER_MAX;
	Limits.MaxTrailerLine = XRT_HTTP1_FUZZ_TEXT_MAX;
	Limits.MaxTrailers = XRT_HTTP1_FUZZ_TRAILERS;
	(void)xrtHttp1TrailersParse(
		(xbytesview){ pData, iSize },
		Trailers, XRT_HTTP1_FUZZ_TRAILERS,
		&Limits, &iBytes, &iCount, &Error
	);
	xrtClearError();
}



/* 扫描一类完整消息，并验证正文长度查询和有界复制。 */
static void __xrtHttp1FuzzMessageKind(
	const uint8* pData,
	size_t iSize,
	xhttpkind Kind,
	bool bEnd
)
{
	xhttpfield Fields[XRT_HTTP1_FUZZ_FIELDS];
	xhttpfield Trailers[XRT_HTTP1_FUZZ_TRAILERS];
	xhttp1bodylimits BodyLimits;
	xhttp1errorinfo Error;
	xhttp1limits HeadLimits;
	xhttp1message Message;
	xhttp1status Status;
	uint8 Output[XRT_HTTP1_FUZZ_COPY_MAX];
	size_t iRequired = 0;

	xrtHttp1LimitsInit(&HeadLimits);
	HeadLimits.MaxHead = XRT_HTTP1_FUZZ_HEAD_MAX;
	HeadLimits.MaxStartLine = XRT_HTTP1_FUZZ_TEXT_MAX;
	HeadLimits.MaxFieldLine = XRT_HTTP1_FUZZ_TEXT_MAX;
	HeadLimits.MaxFields = XRT_HTTP1_FUZZ_FIELDS;
	xrtHttp1BodyLimitsInit(&BodyLimits);
	BodyLimits.MaxBody = XRT_HTTP1_FUZZ_BODY_MAX;
	BodyLimits.MaxChunkLine = XRT_HTTP1_FUZZ_TEXT_MAX;
	BodyLimits.MaxTrailer = XRT_HTTP1_FUZZ_TRAILER_MAX;
	BodyLimits.MaxTrailerLine = XRT_HTTP1_FUZZ_TEXT_MAX;
	BodyLimits.MaxTrailers = XRT_HTTP1_FUZZ_TRAILERS;
	xrtHttp1MessageInit(
		&Message, Fields, XRT_HTTP1_FUZZ_FIELDS,
		Trailers, XRT_HTTP1_FUZZ_TRAILERS
	);
	Status = (Kind == XHTTP_REQUEST) ?
		xrtHttp1RequestMessageParse(
			(xbytesview){ pData, iSize }, bEnd,
			&Message, &HeadLimits, &BodyLimits, &Error
		) :
		xrtHttp1ResponseMessageParse(
			(xbytesview){ pData, iSize }, bEnd,
			XRT_STR_LITERAL("GET"),
			&Message, &HeadLimits, &BodyLimits, &Error
		);
	if ( Status == XHTTP1_READY ) {
		if ( (Message.Wire.Size > iSize) ||
			(Message.Head.Bytes > Message.Wire.Size) ||
			!xrtHttp1MessageBodyCopy(
				&Message, NULL, 0, &iRequired
			) || (iRequired != Message.BodyBytes) ) {
			abort();
		}
		if ( iRequired <= sizeof(Output) ) {
			size_t iCopied = 0;

			if ( !xrtHttp1MessageBodyCopy(
				&Message, Output, sizeof(Output), &iCopied
			) || (iCopied != iRequired) ) {
				abort();
			}
		}
	}
	xrtClearError();
}



/* 让完整请求和响应扫描器覆盖可靠 EOF 与增量输入语义。 */
static void __xrtHttp1FuzzMessage(
	const uint8* pData,
	size_t iSize
)
{
	bool bEnd = (__xrtHttp1FuzzByte(pData, iSize, 3) & 1u) != 0;

	__xrtHttp1FuzzMessageKind(pData, iSize, XHTTP_REQUEST, bEnd);
	__xrtHttp1FuzzMessageKind(pData, iSize, XHTTP_RESPONSE, bEnd);
}



/* 从任意合法字段值构造 Header，并要求写出与重解析严格往返。 */
static void __xrtHttp1FuzzHeadRoundTrip(
	const uint8* pData,
	size_t iSize
)
{
	xhttpfield ParsedFields[1];
	xhttpfield Field;
	xhttp1errorinfo Error;
	xhttp1head Head;
	uint8 Output[1024];
	xstrview Value = {
		(cstr)pData,
		iSize > 256u ? 256u : iSize
	};
	size_t iWritten = 0;
	uint16 iStatus = (uint16)(100u +
		(__xrtHttp1FuzzByte(pData, iSize, 4) % 500u));

	if ( !xrtHttpFieldValueValid(Value) ) {
		Value = (xstrview){ NULL, 0 };
	}
	Field = (xhttpfield){ XRT_STR_LITERAL("X-Fuzz"), Value };
	if ( !xrtHttp1RequestWrite(
		XRT_STR_LITERAL("POST"), XRT_STR_LITERAL("/fuzz"),
		XHTTP_VERSION_1_1, &Field, 1,
		Output, sizeof(Output), &iWritten
	) ) {
		abort();
	}
	xrtHttp1HeadInit(&Head, ParsedFields, 1);
	if ( (xrtHttp1RequestParse(
		(xbytesview){ Output, iWritten }, &Head, NULL, &Error
	) != XHTTP1_READY) || (Head.Bytes != iWritten) ) {
		abort();
	}

	if ( !xrtHttp1ResponseWrite(
		XHTTP_VERSION_1_1, iStatus, Value, &Field, 1,
		Output, sizeof(Output), &iWritten
	) ) {
		abort();
	}
	xrtHttp1HeadInit(&Head, ParsedFields, 1);
	if ( (xrtHttp1ResponseParse(
		(xbytesview){ Output, iWritten }, &Head, NULL, &Error
	) != XHTTP1_READY) || (Head.Bytes != iWritten) ||
		(Head.Status != iStatus) ) {
		abort();
	}
	xrtClearError();
}



/* 把有界任意正文封装为 chunked，再由读取端还原并逐字节核对。 */
static void __xrtHttp1FuzzChunkRoundTrip(
	const uint8* pData,
	size_t iSize
)
{
	xhttp1bodylimits Limits;
	xhttp1errorinfo Error;
	xhttp1bodyplan Plan = { XHTTP1_BODY_CHUNKED, 0 };
	xhttp1body Body;
	uint8 Wire[512];
	uint8 Decoded[256];
	size_t iPayload = iSize > sizeof(Decoded) ? sizeof(Decoded) : iSize;
	size_t iWireSize;
	size_t iWritten;
	size_t iWire = 0;
	size_t iDecoded = 0;
	size_t iGuard = 0;

	if ( !xrtHttp1ChunkWrite(
		(xbytesview){ pData, iPayload },
		Wire, sizeof(Wire), &iWritten
	) ) {
		abort();
	}
	iWireSize = iWritten;
	if ( !xrtHttp1ChunkEndWrite(
		NULL, 0, Wire + iWireSize,
		sizeof(Wire) - iWireSize, &iWritten
	) ) {
		abort();
	}
	iWireSize += iWritten;
	xrtHttp1BodyLimitsInit(&Limits);
	Limits.MaxBody = sizeof(Decoded);
	if ( !xrtHttp1BodyInit(&Body, &Plan, NULL, 0, &Limits) ) {
		abort();
	}

	while ( iGuard++ <= (iWireSize + 8u) ) {
		xbytesview Data;
		size_t iConsumed = 0;
		xhttp1bodystatus Status = xrtHttp1BodyRead(
			&Body,
			(xbytesview){ Wire + iWire, iWireSize - iWire },
			true, &iConsumed, &Data, &Error
		);

		if ( iConsumed > (iWireSize - iWire) ) {
			abort();
		}
		iWire += iConsumed;
		if ( Status == XHTTP1_BODY_DATA ) {
			if ( (Data.Size == 0) ||
				(Data.Size > (sizeof(Decoded) - iDecoded)) ) {
				abort();
			}
			memcpy(Decoded + iDecoded, Data.Data, Data.Size);
			iDecoded += Data.Size;
			continue;
		}
		if ( (Status != XHTTP1_BODY_DONE) ||
			(iWire != iWireSize) || (iDecoded != iPayload) ||
			((iPayload != 0) &&
			 (memcmp(Decoded, pData, iPayload) != 0)) ) {
			abort();
		}
		break;
	}
	if ( iGuard > (iWireSize + 9u) ) {
		abort();
	}
	xrtClearError();
}



/* 统一公开确定性回归和 libFuzzer 使用的 HTTP/1 协议入口。 */
int xrtHttp1FuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	if ( ((pData == NULL) && (iSize != 0)) ||
		(iSize > XRT_HTTP1_FUZZ_INPUT_MAX) ) {
		return 0;
	}

	__xrtHttp1FuzzHead(pData, iSize);
	__xrtHttp1FuzzExpect(pData, iSize);
	__xrtHttp1FuzzTe(pData, iSize);
	__xrtHttp1FuzzBody(pData, iSize);
	__xrtHttp1FuzzMessage(pData, iSize);
	__xrtHttp1FuzzHeadRoundTrip(pData, iSize);
	__xrtHttp1FuzzChunkRoundTrip(pData, iSize);
	return 0;
}



#if defined(XRT_HTTP1_FUZZ_LIBFUZZER)

/* 把独立 HTTP/1 入口适配为 Clang/libFuzzer 约定符号。 */
int LLVMFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	return xrtHttp1FuzzerTestOneInput(pData, iSize);
}

#endif
