#include "../bench_common.h"

#define XRT_MODULE_HTTP1_MESSAGE
#define XRT_MODULE_HTTP_DECODE
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



static const uint8 BenchGzip[] = {
	0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0xFF, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x57,
	0x48, 0xCE, 0xCF, 0x2D, 0x28, 0x4A, 0x2D, 0x2E,
	0x4E, 0x4D, 0x51, 0x28, 0xCF, 0x2F, 0xCA, 0x49,
	0x01, 0x00, 0xA1, 0x2D, 0x94, 0x53, 0x16, 0x00,
	0x00, 0x00
};



/* 解码基准只累计输出元数据，避免额外正文缓冲改变测量对象。 */
typedef struct bench_http_output {
	uint64 Checksum;
	size_t Size;
} bench_http_output;



/* 消费流式明文并阻止编译器删除解码过程。 */
static bool benchHttpOutput(xbytesview Data, ptr pData)
{
	bench_http_output* pOutput = (bench_http_output*)pData;

	pOutput->Size += Data.Size;
	if ( Data.Size != 0 ) {
		pOutput->Checksum += (uint64)Data.Data[0] + (uint64)Data.Size;
	}
	return true;
}



/* 分别测量完整 HTTP/1 消息解析、Header 写出和流式 gzip 解码。 */
int main(int argc, char** argv)
{
	static const uint8 Request[] =
		"POST /v1/items?q=1 HTTP/1.1\r\n"
		"Host: example.test\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: 15\r\n"
		"Connection: keep-alive\r\n\r\n"
		"{\"value\":12345}";
	static const xhttpfield ResponseFields[] = {
		{ XRT_STR_INIT("Content-Type"), XRT_STR_INIT("application/json") },
		{ XRT_STR_INIT("Content-Length"), XRT_STR_INIT("16") },
		{ XRT_STR_INIT("Connection"), XRT_STR_INIT("keep-alive") }
	};
	static const xhttpfield GzipField[] = {
		{ XRT_STR_INIT("Content-Encoding"), XRT_STR_INIT("gzip") }
	};
	bench_http_output DecodeOutput;
	xhttpdecode* pDecode;
	xbenchtimer Timer;
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 200000u);
	uint64 iParseElapsed;
	uint64 iWriteElapsed;
	uint64 iDecodeElapsed;
	uint64 iChecksum = 0;
	uint8 Output[256];
	size_t iOutputSize = 0;

	if ( iIterations == 0 ) {
		fprintf(stderr, "iteration count must be non-zero.\n");
		return 1;
	}
	pDecode = xrtHttpDecodeCreate(GzipField, 1, NULL);
	if ( pDecode == NULL ) {
		return 2;
	}

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xhttpfield Fields[8];
		xhttp1message Message;

		xrtHttp1MessageInit(&Message, Fields, 8, NULL, 0);
		if (
			xrtHttp1RequestMessageParse(
				(xbytesview){ Request, sizeof(Request) - 1u },
				false,
				&Message,
				NULL,
				NULL,
				NULL
			) != XHTTP1_READY
		) {
			xrtHttpDecodeDestroy(pDecode);
			return 3;
		}
		iChecksum += (uint64)Message.Wire.Size + Message.BodyBytes;
	}
	xbenchTimerStop(&Timer);
	iParseElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		if (
			!xrtHttp1ResponseWrite(
				XHTTP_VERSION_1_1,
				200,
				XRT_STR_LITERAL("OK"),
				ResponseFields,
				3,
				Output,
				sizeof(Output),
				&iOutputSize
			)
		) {
			xrtHttpDecodeDestroy(pDecode);
			return 4;
		}
		iChecksum += (uint64)iOutputSize + (uint64)Output[0];
	}
	xbenchTimerStop(&Timer);
	iWriteElapsed = xbenchTimerElapsedNs(&Timer);

	memset(&DecodeOutput, 0, sizeof(DecodeOutput));
	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		if (
			!xrtHttpDecodeReset(pDecode, GzipField, 1, NULL) ||
			!xrtHttpDecodeWrite(
				pDecode,
				(xbytesview){ BenchGzip, sizeof(BenchGzip) },
				true,
				benchHttpOutput,
				&DecodeOutput
			)
		) {
			xrtHttpDecodeDestroy(pDecode);
			return 5;
		}
	}
	xbenchTimerStop(&Timer);
	iDecodeElapsed = xbenchTimerElapsedNs(&Timer);
	iChecksum += DecodeOutput.Checksum + (uint64)DecodeOutput.Size;

	printf("xrt HTTP core benchmark\n");
	printf("iterations=%" PRIu32 "\n", iIterations);
	xbenchPrintMetricDouble(
		"http_parse_ops_per_sec",
		xbenchSafeRate(iIterations, iParseElapsed)
	);
	xbenchPrintMetricDouble(
		"http_write_ops_per_sec",
		xbenchSafeRate(iIterations, iWriteElapsed)
	);
	xbenchPrintMetricDouble(
		"http_gzip_decode_mib_per_sec",
		xbenchSafeRate(
			(uint64)iIterations * (uint64)sizeof(BenchGzip),
			iDecodeElapsed
		) / (1024.0 * 1024.0)
	);
	xbenchPrintMetricU64("checksum", iChecksum);

	xrtHttpDecodeDestroy(pDecode);
	return 0;
}
