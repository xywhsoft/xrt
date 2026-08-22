#include "../test.h"



/* 回环输出按需增长，并由测试在每轮结束时统一释放。 */
typedef struct test_deflate_roundtrip_output {
	bytes Data;
	size_t Size;
	size_t Capacity;
} test_deflate_roundtrip_output;



/* 收集任意大小的编码分片。 */
static bool testDeflateRoundtripOutput(
	xbytesview Data,
	ptr pData
)
{
	test_deflate_roundtrip_output* pOutput =
		(test_deflate_roundtrip_output*)pData;
	size_t iRequired;
	size_t iCapacity;
	bytes pNew;

	if ( Data.Size > (SIZE_MAX - pOutput->Size) ) {
		return false;
	}
	iRequired = pOutput->Size + Data.Size;
	if ( iRequired > pOutput->Capacity ) {
		iCapacity = pOutput->Capacity != 0 ?
			pOutput->Capacity : 256u;
		while ( iCapacity < iRequired ) {
			if ( iCapacity > (SIZE_MAX / 2u) ) {
				iCapacity = iRequired;
				break;
			}
			iCapacity *= 2u;
		}
		pNew = (bytes)xrtRealloc(
			pOutput->Data,
			iCapacity
		);
		if ( pNew == NULL ) {
			return false;
		}
		pOutput->Data = pNew;
		pOutput->Capacity = iCapacity;
	}
	if ( Data.Size != 0 ) {
		memcpy(
			pOutput->Data + pOutput->Size,
			Data.Data,
			Data.Size
		);
	}
	pOutput->Size += Data.Size;
	return true;
}



/* 将编码格式映射到严格对应的解码格式。 */
static xinflateformat testDeflateInflateFormat(
	xdeflateformat Format
)
{
	switch ( Format ) {
		case XDEFLATE_RAW:
			return XINFLATE_RAW;

		case XDEFLATE_ZLIB:
			return XINFLATE_ZLIB;

		default:
			return XINFLATE_GZIP;
	}
}



/* 按变化输入分片编码，再用独立 Inflate 完整验证线路表示。 */
static void testDeflateRoundtrip(
	const uint8* pPlain,
	size_t iPlainSize,
	xdeflateformat Format,
	int32 iLevel,
	xdeflatestrategy Strategy
)
{
	test_deflate_roundtrip_output Output;
	xdeflateconfig DeflateConfig;
	xinflateconfig InflateConfig;
	xdeflate* pDeflate;
	bytes pDecoded;
	size_t iDecoded = 0;
	size_t iOffset = 0;
	size_t iChunk = 1;

	memset(&Output, 0, sizeof(Output));
	xrtDeflateConfigInit(&DeflateConfig);
	DeflateConfig.Format = Format;
	DeflateConfig.Level = iLevel;
	DeflateConfig.Strategy = Strategy;
	pDeflate = xrtDeflateCreate(&DeflateConfig);
	testRequire(
		pDeflate != NULL,
		"Deflate roundtrip encoder create failed"
	);
	while ( iOffset < iPlainSize ) {
		size_t iSize = iPlainSize - iOffset;
		xdeflateflush Flush;

		if ( iSize > iChunk ) {
			iSize = iChunk;
		}
		Flush = (iOffset + iSize) == iPlainSize ?
			XDEFLATE_FLUSH_FINISH :
			XDEFLATE_FLUSH_NONE;
		testRequire(
			xrtDeflateWrite(
				pDeflate,
				(xbytesview){
					pPlain + iOffset,
					iSize
				},
				Flush,
				testDeflateRoundtripOutput,
				&Output
			),
			"Deflate roundtrip streaming write failed"
		);
		iOffset += iSize;
		iChunk = (iChunk % 257u) + 1u;
	}
	if ( iPlainSize == 0 ) {
		testRequire(
			xrtDeflateWrite(
				pDeflate,
				(xbytesview){ NULL, 0 },
				XDEFLATE_FLUSH_FINISH,
				testDeflateRoundtripOutput,
				&Output
			),
			"Deflate roundtrip empty write failed"
		);
	}
	testRequire(
		xrtDeflateDone(pDeflate) &&
		(xrtDeflateOutputSize(pDeflate) ==
		 Output.Size),
		"Deflate roundtrip terminal counter mismatch"
	);
	xrtDeflateDestroy(pDeflate);

	xrtInflateConfigInit(&InflateConfig);
	InflateConfig.Format =
		testDeflateInflateFormat(Format);
	InflateConfig.OutputLimit = iPlainSize;
	pDecoded = xrtInflateAll(
		(xbytesview){
			Output.Data,
			Output.Size
		},
		&InflateConfig,
		&iDecoded
	);
	testRequire(
		(pDecoded != NULL) &&
		(iDecoded == iPlainSize) &&
		(memcmp(
			pDecoded,
			pPlain,
			iPlainSize
		 ) == 0),
		"Deflate and Inflate roundtrip mismatch"
	);
	xrtFree(pDecoded);
	xrtFree(Output.Data);
}



/* 验证 SYNC 和 FULL 中间边界不破坏后续完整解码。 */
static void testDeflateFlushRoundtrip(
	xdeflateformat Format
)
{
	static const char First[] =
		"first first first ";
	static const char Second[] =
		"second second second ";
	static const char Third[] =
		"third third third";
	static const char Plain[] =
		"first first first "
		"second second second "
		"third third third";
	test_deflate_roundtrip_output Output;
	xdeflateconfig DeflateConfig;
	xinflateconfig InflateConfig;
	xdeflate* pDeflate;
	bytes pDecoded;
	size_t iDecoded = 0;

	memset(&Output, 0, sizeof(Output));
	xrtDeflateConfigInit(&DeflateConfig);
	DeflateConfig.Format = Format;
	pDeflate = xrtDeflateCreate(&DeflateConfig);
	testRequire(
		(pDeflate != NULL) &&
		xrtDeflateWrite(
			pDeflate,
			XRT_BYTES_LITERAL(First),
			XDEFLATE_FLUSH_SYNC,
			testDeflateRoundtripOutput,
			&Output
		) &&
		xrtDeflateWrite(
			pDeflate,
			XRT_BYTES_LITERAL(Second),
			XDEFLATE_FLUSH_FULL,
			testDeflateRoundtripOutput,
			&Output
		) &&
		xrtDeflateWrite(
			pDeflate,
			XRT_BYTES_LITERAL(Third),
			XDEFLATE_FLUSH_FINISH,
			testDeflateRoundtripOutput,
			&Output
		),
		"Deflate Flush roundtrip encode failed"
	);
	xrtDeflateDestroy(pDeflate);

	xrtInflateConfigInit(&InflateConfig);
	InflateConfig.Format =
		testDeflateInflateFormat(Format);
	InflateConfig.OutputLimit =
		sizeof(Plain) - 1u;
	pDecoded = xrtInflateAll(
		(xbytesview){
			Output.Data,
			Output.Size
		},
		&InflateConfig,
		&iDecoded
	);
	testRequire(
		(pDecoded != NULL) &&
		(iDecoded == (sizeof(Plain) - 1u)) &&
		(memcmp(
			pDecoded,
			Plain,
			iDecoded
		 ) == 0),
		"Deflate Flush roundtrip decode failed"
	);
	xrtFree(pDecoded);
	xrtFree(Output.Data);
}



/* 构造同时包含可压缩区域和高变化区域的确定性负载。 */
static void testDeflatePayload(
	uint8* pData,
	size_t iSize
)
{
	uint32 iState = UINT32_C(0x12345678);
	size_t i;

	for ( i = 0; i < iSize; i++ ) {
		if ( (i % 97u) < 64u ) {
			pData[i] = (uint8)(
				'A' + (i % 7u)
			);
		} else {
			iState = (iState *
				UINT32_C(1664525)) +
				UINT32_C(1013904223);
			pData[i] = (uint8)(
				iState >> 24u
			);
		}
	}
}



/* 构造只有较大历史窗口才能复用的确定性负载。 */
static void testDeflateWindowPayload(
	uint8* pData,
	size_t iSize
)
{
	uint32 iState = UINT32_C(0xC001D00D);
	size_t i;

	testRequire(
		iSize >= 4096u,
		"Deflate window fixture is too small"
	);
	for ( i = 0; i < 2048u; i++ ) {
		iState = (iState * UINT32_C(1103515245)) +
			UINT32_C(12345);
		pData[i] = (uint8)(iState >> 24u);
	}
	for ( i = 2048u; i < iSize; i++ ) {
		pData[i] = pData[(i - 2048u) & 2047u];
	}
}



/* 验证 8 到 15 位窗口同时约束编码距离、zlib CINFO 和解码接受范围。 */
static void testDeflateWindows(void)
{
	uint8 Plain[8192];
	xdeflateconfig DeflateConfig;
	xinflateconfig InflateConfig;
	bytes pWide;
	size_t iWideSize = 0;
	size_t iBits;

	testDeflateWindowPayload(Plain, sizeof(Plain));
	for ( iBits = XDEFLATE_WINDOW_MIN;
		iBits <= XDEFLATE_WINDOW_MAX;
		iBits++ ) {
		bytes pEncoded;
		bytes pDecoded;
		size_t iEncoded = 0;
		size_t iDecoded = 0;

		xrtDeflateConfigInit(&DeflateConfig);
		DeflateConfig.Format = XDEFLATE_ZLIB;
		DeflateConfig.WindowBits = (uint8)iBits;
		pEncoded = xrtDeflateAll(
			(xbytesview){ Plain, sizeof(Plain) },
			&DeflateConfig,
			&iEncoded
		);
		testRequire(
			(pEncoded != NULL) &&
			(iEncoded >= 2u) &&
			((pEncoded[0] >> 4u) ==
			 (uint8)(iBits - 8u)),
			"Deflate zlib window Header mismatch"
		);

		xrtInflateConfigInit(&InflateConfig);
		InflateConfig.Format = XINFLATE_ZLIB;
		InflateConfig.WindowBits = (uint8)iBits;
		InflateConfig.OutputLimit = sizeof(Plain);
		pDecoded = xrtInflateAll(
			(xbytesview){ pEncoded, iEncoded },
			&InflateConfig,
			&iDecoded
		);
		testRequire(
			(pDecoded != NULL) &&
			(iDecoded == sizeof(Plain)) &&
			(memcmp(pDecoded, Plain, sizeof(Plain)) == 0),
			"Deflate window roundtrip mismatch"
		);
		xrtFree(pDecoded);
		xrtFree(pEncoded);
	}

	xrtDeflateConfigInit(&DeflateConfig);
	DeflateConfig.Format = XDEFLATE_RAW;
	DeflateConfig.WindowBits = XDEFLATE_WINDOW_MAX;
	pWide = xrtDeflateAll(
		(xbytesview){ Plain, sizeof(Plain) },
		&DeflateConfig,
		&iWideSize
	);
	testRequire(
		pWide != NULL,
		"wide-window Deflate fixture failed"
	);

	xrtInflateConfigInit(&InflateConfig);
	InflateConfig.Format = XINFLATE_RAW;
	InflateConfig.WindowBits = XINFLATE_WINDOW_MIN;
	InflateConfig.OutputLimit = sizeof(Plain);
	{
		size_t iDecoded = 0;

		testRequire(
			(xrtInflateAll(
				(xbytesview){ pWide, iWideSize },
				&InflateConfig,
				&iDecoded
			 ) == NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
			(xrtErrorCode(xrtGetError()) ==
			 XINFLATE_ERROR_DATA),
			"Inflate accepted a match beyond its window"
		);
		xrtClearError();
	}
	xrtFree(pWide);
}



/* 运行跨格式、级别、策略、分片和 Flush 的编码解码回环。 */
int main(void)
{
	static const int32 Levels[] = {
		0, 1, 6, 10
	};
	uint8 Data[131071];
	size_t i;
	size_t j;

	testDeflatePayload(Data, sizeof(Data));
	for ( i = XDEFLATE_RAW;
		i <= XDEFLATE_GZIP;
		i++ ) {
		for ( j = 0;
			j < (sizeof(Levels) / sizeof(Levels[0]));
			j++ ) {
			testDeflateRoundtrip(
				Data,
				sizeof(Data),
				(xdeflateformat)i,
				Levels[j],
				XDEFLATE_STRATEGY_DEFAULT
			);
		}
		for ( j = XDEFLATE_STRATEGY_DEFAULT;
			j <= XDEFLATE_STRATEGY_FIXED;
			j++ ) {
			testDeflateRoundtrip(
				Data,
				8192,
				(xdeflateformat)i,
				6,
				(xdeflatestrategy)j
			);
		}
		testDeflateFlushRoundtrip(
			(xdeflateformat)i
		);
	}
	testDeflateWindows();
	printf("[PASS] deflate_roundtrip\n");
	return 0;
}
