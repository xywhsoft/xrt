#include "../test.h"



/* 读取完整嵌套解码结果。 */
static size_t testHttpBodyDecodeRoundtripRead(
	xhttpbody* pBody,
	uint8* pOutput,
	size_t iCapacity
)
{
	xhttpbodyreader* pReader = xrtHttpBodyOpen(pBody);
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	size_t iSize = 0;

	testRequire(pReader != NULL,
		"HTTP Body nested decode reader open failed");
	while ( (Status = xrtHttpBodyNext(
		pReader, 7, &Chunk
	)) == XHTTP_BODY_DATA ) {
		testRequire(
			(iSize <= iCapacity) &&
			(Chunk.Size <= (iCapacity - iSize)),
			"HTTP Body nested decode overflow");
		memcpy(pOutput + iSize, Chunk.Data, Chunk.Size);
		iSize += Chunk.Size;
		xrtHttpBodyChunkRelease(&Chunk);
	}
	testRequire(Status == XHTTP_BODY_EOF,
		"HTTP Body nested decode did not finish");
	xrtHttpBodyReaderDestroy(pReader);
	return iSize;
}



/* 验证重复字段中的 gzip、deflate 按应用顺序的反向解码。 */
int main(void)
{
	static const uint8 Plain[] =
		"nested content coding nested content coding";
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("gzip")
		},
		{
			XRT_STR_INIT("content-encoding"),
			XRT_STR_INIT("deflate")
		}
	};
	xdeflateconfig Deflate;
	bytes pGzip;
	bytes pNested;
	size_t iGzip;
	size_t iNested;
	xhttpbody* pSource;
	xhttpbody* pBody = NULL;
	uint8 Output[sizeof(Plain) - 1u];
	size_t iOutput;

	xrtDeflateConfigInit(&Deflate);
	Deflate.Format = XDEFLATE_GZIP;
	pGzip = xrtDeflateAll(
		(xbytesview){ Plain, sizeof(Plain) - 1u },
		&Deflate,
		&iGzip
	);
	testRequire(pGzip != NULL,
		"HTTP Body nested gzip fixture failed");
	Deflate.Format = XDEFLATE_ZLIB;
	pNested = xrtDeflateAll(
		(xbytesview){ pGzip, iGzip },
		&Deflate,
		&iNested
	);
	xrtFree(pGzip);
	testRequire(pNested != NULL,
		"HTTP Body nested deflate fixture failed");
	pSource = xrtHttpBodyTake(pNested, iNested);
	testRequire(
		(pSource != NULL) &&
		(xrtHttpBodyDecodeFields(
			pSource,
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			NULL,
			&pBody
		 ) == XHTTP_BODY_DECODE_APPLIED) &&
		(pBody != NULL),
		"HTTP Body nested decode setup failed"
	);
	xrtHttpBodyDestroy(pSource);
	iOutput = testHttpBodyDecodeRoundtripRead(
		pBody, Output, sizeof(Output)
	);
	testRequire(
		(iOutput == sizeof(Output)) &&
		(memcmp(Output, Plain, sizeof(Output)) == 0),
		"HTTP Body nested decode output mismatch"
	);
	xrtHttpBodyDestroy(pBody);
	printf("[PASS] HTTP Body nested content decode\n");
	return 0;
}

