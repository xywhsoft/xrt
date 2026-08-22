#include "../test.h"



/* 完整读取解压正文，并严格限制调用方输出容量。 */
static size_t testHttpBodyInflateRoundtripRead(
	xhttpbody* pBody,
	uint8* pOutput,
	size_t iCapacity,
	size_t iMaxBytes
)
{
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	size_t iSize = 0;

	testRequire(pReader != NULL,
		"HTTP Inflate roundtrip reader open failed");
	while ( (Status = xrtHttpBodyNext(
		pReader, iMaxBytes, &Chunk
	)) == XHTTP_BODY_DATA ) {
		testRequire(Chunk.Size <= (iCapacity - iSize),
			"HTTP Inflate roundtrip output overflow");
		memcpy(pOutput + iSize, Chunk.Data, Chunk.Size);
		iSize += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	testRequire(Status == XHTTP_BODY_EOF,
		"HTTP Inflate roundtrip did not end");
	xrtHttpBodyReaderDestroy(pReader);
	return iSize;
}



/* 使用公开 Deflate 生成线路表示，再通过 Body 完整解码。 */
static void testHttpBodyInflateRoundtripOne(
	xbytesview Plain,
	xdeflateformat DeflateFormat,
	xinflateformat InflateFormat,
	size_t iReadSize,
	size_t iMaxBytes
)
{
	xdeflateconfig DeflateConfig;
	xhttpbodyinflateconfig InflateConfig;
	xhttpbody* pSource;
	xhttpbody* pBody;
	bytes pEncoded;
	bytes pOutput = NULL;
	size_t iEncoded;
	size_t iOutput;

	xrtDeflateConfigInit(&DeflateConfig);
	DeflateConfig.Format = DeflateFormat;
	pEncoded = xrtDeflateAll(
		Plain,
		&DeflateConfig,
		&iEncoded
	);
	testRequire(pEncoded != NULL,
		"HTTP Inflate roundtrip encoding failed");
	pSource = xrtHttpBodyBorrow(
		(xbytesview){ pEncoded, iEncoded }
	);
	xrtHttpBodyInflateConfigInit(&InflateConfig);
	InflateConfig.Inflate.Format = InflateFormat;
	InflateConfig.ReadSize = iReadSize;
	pBody = xrtHttpBodyInflate(pSource, &InflateConfig);
	xrtHttpBodyDestroy(pSource);
	testRequire(pBody != NULL,
		"HTTP Inflate roundtrip body creation failed");
	if ( Plain.Size != 0 ) {
		pOutput = (bytes)xrtMalloc(Plain.Size);
		testRequire(pOutput != NULL,
			"HTTP Inflate roundtrip output allocation failed");
	}
	iOutput = testHttpBodyInflateRoundtripRead(
		pBody,
		pOutput,
		Plain.Size,
		iMaxBytes
	);
	testRequire((iOutput == Plain.Size) &&
		((Plain.Size == 0) ||
		 (memcmp(pOutput, Plain.Data, Plain.Size) == 0)),
		"HTTP Inflate roundtrip output mismatch");
	xrtFree(pOutput);
	xrtHttpBodyDestroy(pBody);
	xrtFree(pEncoded);
}



/* 验证全部包装、HTTP deflate 自动识别、空流和大块高熵输入。 */
static void testHttpBodyInflateRoundtripFormats(void)
{
	static uint8 Large[131072];
	static const uint8 Small[] =
		"body inflate roundtrip body inflate roundtrip";
	uint32 iRandom = UINT32_C(0x9e3779b9);
	size_t i;

	testHttpBodyInflateRoundtripOne(
		(xbytesview){ NULL, 0 },
		XDEFLATE_GZIP,
		XINFLATE_GZIP,
		1,
		1
	);
	testHttpBodyInflateRoundtripOne(
		(xbytesview){ Small, sizeof(Small) - 1u },
		XDEFLATE_RAW,
		XINFLATE_RAW,
		1,
		1
	);
	testHttpBodyInflateRoundtripOne(
		(xbytesview){ Small, sizeof(Small) - 1u },
		XDEFLATE_ZLIB,
		XINFLATE_ZLIB,
		2,
		7
	);
	testHttpBodyInflateRoundtripOne(
		(xbytesview){ Small, sizeof(Small) - 1u },
		XDEFLATE_RAW,
		XINFLATE_DEFLATE,
		3,
		13
	);
	testHttpBodyInflateRoundtripOne(
		(xbytesview){ Small, sizeof(Small) - 1u },
		XDEFLATE_ZLIB,
		XINFLATE_DEFLATE,
		5,
		31
	);
	testHttpBodyInflateRoundtripOne(
		(xbytesview){ Small, sizeof(Small) - 1u },
		XDEFLATE_GZIP,
		XINFLATE_GZIP,
		7,
		17
	);

	for ( i = 0; i < sizeof(Large); i++ ) {
		iRandom = (iRandom * UINT32_C(1664525)) +
			UINT32_C(1013904223);
		Large[i] = (uint8)(iRandom >> 24u);
	}
	testHttpBodyInflateRoundtripOne(
		(xbytesview){ Large, sizeof(Large) },
		XDEFLATE_GZIP,
		XINFLATE_GZIP,
		4093,
		257
	);
}



/* 验证 gzip 拼接 member 跨来源分片后按顺序形成一个正文。 */
static void testHttpBodyInflateConcatenatedGzip(void)
{
	static const uint8 First[] = "first gzip member:";
	static const uint8 Second[] = "second gzip member";
	xdeflateconfig DeflateConfig;
	xhttpbodyinflateconfig InflateConfig;
	bytes pFirst;
	bytes pSecond;
	bytes pJoined;
	xhttpbody* pSource;
	xhttpbody* pBody;
	uint8 Output[128];
	size_t iFirst;
	size_t iSecond;
	size_t iOutput;

	xrtDeflateConfigInit(&DeflateConfig);
	DeflateConfig.Format = XDEFLATE_GZIP;
	pFirst = xrtDeflateAll(
		(xbytesview){ First, sizeof(First) - 1u },
		&DeflateConfig,
		&iFirst
	);
	pSecond = xrtDeflateAll(
		(xbytesview){ Second, sizeof(Second) - 1u },
		&DeflateConfig,
		&iSecond
	);
	testRequire((pFirst != NULL) && (pSecond != NULL),
		"concatenated gzip encoding failed");
	pJoined = (bytes)xrtMalloc(iFirst + iSecond);
	testRequire(pJoined != NULL,
		"concatenated gzip allocation failed");
	memcpy(pJoined, pFirst, iFirst);
	memcpy(pJoined + iFirst, pSecond, iSecond);
	xrtFree(pFirst);
	xrtFree(pSecond);

	pSource = xrtHttpBodyTake(pJoined, iFirst + iSecond);
	xrtHttpBodyInflateConfigInit(&InflateConfig);
	InflateConfig.Inflate.Format = XINFLATE_GZIP;
	InflateConfig.ReadSize = iFirst;
	pBody = xrtHttpBodyInflate(pSource, &InflateConfig);
	xrtHttpBodyDestroy(pSource);
	testRequire(pBody != NULL,
		"concatenated gzip Body creation failed");
	iOutput = testHttpBodyInflateRoundtripRead(
		pBody,
		Output,
		sizeof(Output),
		3
	);
	testRequire((iOutput ==
		 ((sizeof(First) - 1u) + (sizeof(Second) - 1u))) &&
		(memcmp(Output, First, sizeof(First) - 1u) == 0) &&
		(memcmp(
			Output + sizeof(First) - 1u,
			Second,
			sizeof(Second) - 1u
		) == 0),
		"concatenated gzip Body output mismatch");
	xrtHttpBodyDestroy(pBody);
}



/* 执行 HTTP Inflate 正文格式闭环测试。 */
int main(void)
{
	testHttpBodyInflateRoundtripFormats();
	testHttpBodyInflateConcatenatedGzip();
	printf("[PASS] HTTP Inflate body roundtrip\n");
	return 0;
}

