#include "../test.h"



/* 收集完整压缩正文，读取上限按调用次数变化以覆盖分片边界。 */
static size_t testHttpBodyDeflateRoundtripRead(
	xhttpbody* pBody,
	uint8* pOutput,
	size_t iCapacity
)
{
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	size_t iSize = 0;
	size_t iStep = 0;

	testRequire(pReader != NULL,
		"HTTP Deflate roundtrip reader open failed");
	for ( ;; ) {
		size_t iMaxBytes = (iStep++ % 997u) + 1u;

		Status = xrtHttpBodyNext(
			pReader, iMaxBytes, &Chunk
		);
		if ( Status == XHTTP_BODY_EOF ) {
			break;
		}
		testRequire((Status == XHTTP_BODY_DATA) &&
			(Chunk.Size <= (iCapacity - iSize)),
			"HTTP Deflate roundtrip body read failed");
		memcpy(pOutput + iSize, Chunk.Data, Chunk.Size);
		iSize += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	xrtHttpBodyReaderDestroy(pReader);
	return iSize;
}



/* 对一个格式和推进粒度执行 Body -> Deflate -> Inflate 完整闭环。 */
static void testHttpBodyDeflateRoundtripOne(
	xbytesview Input,
	xdeflateformat Format,
	size_t iReadSize
)
{
	uint8 Compressed[131072];
	xhttpbodydeflateconfig BodyConfig;
	xinflateconfig InflateConfig;
	xhttpbody* pSource = xrtHttpBodyBorrow(Input);
	xhttpbody* pBody;
	bytes pDecoded;
	size_t iCompressed;
	size_t iDecoded;

	testRequire(pSource != NULL,
		"HTTP Deflate roundtrip source creation failed");
	xrtHttpBodyDeflateConfigInit(&BodyConfig);
	BodyConfig.Deflate.Format = Format;
	BodyConfig.ReadSize = iReadSize;
	pBody = xrtHttpBodyDeflate(pSource, &BodyConfig);
	xrtHttpBodyDestroy(pSource);
	testRequire(pBody != NULL,
		"HTTP Deflate roundtrip body creation failed");
	iCompressed = testHttpBodyDeflateRoundtripRead(
		pBody, Compressed, sizeof(Compressed)
	);
	xrtHttpBodyDestroy(pBody);

	xrtInflateConfigInit(&InflateConfig);
	InflateConfig.Format =
		Format == XDEFLATE_RAW ? XINFLATE_RAW :
		Format == XDEFLATE_ZLIB ? XINFLATE_ZLIB :
		XINFLATE_GZIP;
	pDecoded = xrtInflateAll(
		(xbytesview){ Compressed, iCompressed },
		&InflateConfig,
		&iDecoded
	);
	testRequire((pDecoded != NULL) &&
		(iDecoded == Input.Size) &&
		(memcmp(pDecoded, Input.Data, Input.Size) == 0),
		"HTTP Deflate roundtrip payload mismatch");
	xrtFree(pDecoded);
}



/* 验证空、可压缩和高熵输入在全部格式与推进粒度下闭环。 */
int main(void)
{
	static const size_t ReadSizes[] = {
		1,
		7,
		XHTTP_BODY_DEFLATE_READ_DEFAULT
	};
	static const xdeflateformat Formats[] = {
		XDEFLATE_RAW,
		XDEFLATE_ZLIB,
		XDEFLATE_GZIP
	};
	uint8 Repeated[65536];
	uint8 Random[65536];
	uint32 iState = UINT32_C(0x9E3779B9);
	size_t i;
	size_t j;

	for ( i = 0; i < sizeof(Repeated); i++ ) {
		Repeated[i] = (uint8)("xrt-standard-library"[i % 20u]);
		iState = (iState * UINT32_C(1664525)) +
			UINT32_C(1013904223);
		Random[i] = (uint8)(iState >> 24u);
	}
	for ( i = 0; i <
		(sizeof(Formats) / sizeof(Formats[0])); i++ ) {
		for ( j = 0; j <
			(sizeof(ReadSizes) / sizeof(ReadSizes[0])); j++ ) {
			testHttpBodyDeflateRoundtripOne(
				(xbytesview){ NULL, 0 },
				Formats[i],
				ReadSizes[j]
			);
			testHttpBodyDeflateRoundtripOne(
				(xbytesview){ Repeated, sizeof(Repeated) },
				Formats[i],
				ReadSizes[j]
			);
			testHttpBodyDeflateRoundtripOne(
				(xbytesview){ Random, sizeof(Random) },
				Formats[i],
				ReadSizes[j]
			);
		}
	}
	printf("[PASS] HTTP Deflate body roundtrip\n");
	return 0;
}
