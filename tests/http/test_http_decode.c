#include "../test.h"

#include <xrt/http_decode.h>



static const uint8 TestGzip[] = {
	0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0xFF, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x57,
	0x48, 0xCE, 0xCF, 0x2D, 0x28, 0x4A, 0x2D, 0x2E,
	0x4E, 0x4D, 0x51, 0x28, 0xCF, 0x2F, 0xCA, 0x49,
	0x01, 0x00, 0xA1, 0x2D, 0x94, 0x53, 0x16, 0x00,
	0x00, 0x00
};



static const uint8 TestDeflate[] = {
	0x78, 0x9C, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x57,
	0x48, 0xCE, 0xCF, 0x2D, 0x28, 0x4A, 0x2D, 0x2E,
	0x4E, 0x4D, 0x51, 0x28, 0xCF, 0x2F, 0xCA, 0x49,
	0x01, 0x00, 0x63, 0x85, 0x08, 0xB2
};



static const uint8 TestGzipTwice[] = {
	0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x0A, 0x93, 0xEF, 0xE6, 0x60, 0x00, 0x01,
	0x26, 0xAE, 0xD3, 0x1E, 0x67, 0x4F, 0x9E, 0x0C,
	0xF7, 0x38, 0x77, 0x5E, 0x57, 0xC3, 0x4B, 0x57,
	0xCF, 0xCF, 0x37, 0x50, 0xE3, 0xBC, 0xFE, 0x29,
	0x4F, 0x46, 0x86, 0x85, 0xBA, 0x53, 0x82, 0xC5,
	0x80, 0x2A, 0x00, 0x0E, 0x6A, 0xF6, 0x01, 0x2A,
	0x00, 0x00, 0x00
};



static const char TestPlain[] = "hello compressed world";



/* 测试输出使用固定容量，确保 HTTP 解码路径本身不需要正文缓冲。 */
typedef struct test_http_decode_output {
	uint8 Data[128];
	size_t Size;
} test_http_decode_output;



/* 收集同步回调输出。 */
static bool testHttpDecodeOutput(xbytesview Data, ptr pData)
{
	test_http_decode_output* pOutput =
		(test_http_decode_output*)pData;

	if ( Data.Size > (sizeof(pOutput->Data) - pOutput->Size) ) {
		return false;
	}
	memcpy(pOutput->Data + pOutput->Size, Data.Data, Data.Size);
	pOutput->Size += Data.Size;
	return true;
}



/* 主动拒绝第一段输出，验证回调停止不会发布部分计数。 */
static bool testHttpDecodeStop(xbytesview Data, ptr pData)
{
	(void)Data;
	(void)pData;
	return false;
}



/* 验证 gzip 任意分片、计数、终态和同对象复位。 */
static void testHttpDecodeGzip(void)
{
	static const xhttpfield Gzip[] = {
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("gzip")
		}
	};
	static const xhttpfield Deflate[] = {
		{
			XRT_STR_INIT("content-encoding"),
			XRT_STR_INIT("deflate")
		}
	};
	static const xhttpfield GzipTwice[] = {
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("gzip, gzip")
		}
	};
	test_http_decode_output Output;
	xhttpdecode* pDecode;
	size_t i;

	memset(&Output, 0, sizeof(Output));
	pDecode = xrtHttpDecodeCreate(Gzip, 1, NULL);
	testRequire(
		(pDecode != NULL) &&
		(xrtHttpDecodeMode(pDecode) == XHTTP_DECODE_CONTENT),
		"gzip HTTP decoder create failed"
	);
	for ( i = 0; i < sizeof(TestGzip); i++ ) {
		testRequire(
			xrtHttpDecodeWrite(
				pDecode,
				(xbytesview){ TestGzip + i, 1 },
				i == (sizeof(TestGzip) - 1u),
				testHttpDecodeOutput,
				&Output
			),
			"fragmented gzip HTTP decode failed"
		);
	}
	testRequire(
		xrtHttpDecodeDone(pDecode) &&
		(xrtHttpDecodeInputSize(pDecode) == sizeof(TestGzip)) &&
		(xrtHttpDecodeOutputSize(pDecode) == (sizeof(TestPlain) - 1u)) &&
		(Output.Size == (sizeof(TestPlain) - 1u)) &&
		(memcmp(Output.Data, TestPlain, Output.Size) == 0),
		"gzip HTTP decode result mismatch"
	);

	memset(&Output, 0, sizeof(Output));
	testRequire(
		xrtHttpDecodeReset(pDecode, Deflate, 1, NULL) &&
		xrtHttpDecodeWrite(
			pDecode,
			(xbytesview){ TestDeflate, sizeof(TestDeflate) },
			true,
			testHttpDecodeOutput,
			&Output
		) &&
		xrtHttpDecodeDone(pDecode) &&
		(Output.Size == (sizeof(TestPlain) - 1u)) &&
		(memcmp(Output.Data, TestPlain, Output.Size) == 0),
		"deflate HTTP decoder reset failed"
	);

	memset(&Output, 0, sizeof(Output));
	testRequire(
		xrtHttpDecodeReset(pDecode, GzipTwice, 1, NULL) &&
		xrtHttpDecodeWrite(
			pDecode,
			(xbytesview){ TestGzipTwice, sizeof(TestGzipTwice) },
			true,
			testHttpDecodeOutput,
			&Output
		) &&
		xrtHttpDecodeDone(pDecode) &&
		(Output.Size == (sizeof(TestPlain) - 1u)) &&
		(memcmp(Output.Data, TestPlain, Output.Size) == 0),
		"nested gzip HTTP decoder reset failed"
	);
	xrtHttpDecodeDestroy(pDecode);
}



/* 验证 identity 零拷贝路径和未知编码的显式原样回退。 */
static void testHttpDecodePassthrough(void)
{
	static const xhttpfield Unknown[] = {
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("br")
		}
	};
	test_http_decode_output Output;
	xhttpdecodeconfig Config;
	xhttpdecode* pDecode;

	memset(&Output, 0, sizeof(Output));
	pDecode = xrtHttpDecodeCreate(NULL, 0, NULL);
	testRequire(
		(pDecode != NULL) &&
		(xrtHttpDecodeMode(pDecode) == XHTTP_DECODE_IDENTITY) &&
		xrtHttpDecodeWrite(
			pDecode,
			(xbytesview){ (const uint8*)TestPlain, sizeof(TestPlain) - 1u },
			true,
			testHttpDecodeOutput,
			&Output
		) &&
		xrtHttpDecodeDone(pDecode) &&
		(Output.Size == (sizeof(TestPlain) - 1u)),
		"identity HTTP decode path failed"
	);

	xrtClearError();
	testRequire(
		!xrtHttpDecodeReset(pDecode, Unknown, 1, NULL) &&
		(xrtGetError() != NULL),
		"strict HTTP decoder accepted unknown coding"
	);
	xrtHttpDecodeConfigInit(&Config);
	Config.Flags = XHTTP_DECODE_ALLOW_RAW;
	memset(&Output, 0, sizeof(Output));
	testRequire(
		xrtHttpDecodeReset(pDecode, Unknown, 1, &Config) &&
		(xrtHttpDecodeMode(pDecode) == XHTTP_DECODE_RAW) &&
		xrtHttpDecodeWrite(
			pDecode,
			(xbytesview){ TestGzip, sizeof(TestGzip) },
			true,
			testHttpDecodeOutput,
			&Output
		) &&
		(Output.Size == sizeof(TestGzip)) &&
		(memcmp(Output.Data, TestGzip, sizeof(TestGzip)) == 0),
		"unknown HTTP coding raw fallback failed"
	);
	xrtHttpDecodeDestroy(pDecode);
}



/* 验证编码层数、明文上限、畸形语法和失败终态。 */
static void testHttpDecodeLimits(void)
{
	static const xhttpfield Gzip[] = {
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("gzip")
		}
	};
	static const xhttpfield TooMany[] = {
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("gzip, gzip, gzip, gzip, gzip")
		}
	};
	static const xhttpfield Invalid[] = {
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("gzip;")
		}
	};
	test_http_decode_output Output;
	xhttpdecodeconfig Config;
	xhttpdecode* pDecode;

	xrtClearError();
	testRequire(
		(xrtHttpDecodeCreate(TooMany, 1, NULL) == NULL) &&
		(xrtGetError() != NULL),
		"HTTP decoder accepted too many coding layers"
	);
	xrtClearError();
	testRequire(
		(xrtHttpDecodeCreate(Invalid, 1, NULL) == NULL) &&
		(xrtGetError() != NULL),
		"HTTP decoder accepted malformed Content-Encoding"
	);

	xrtHttpDecodeConfigInit(&Config);
	Config.OutputLimit = 5;
	memset(&Output, 0, sizeof(Output));
	pDecode = xrtHttpDecodeCreate(Gzip, 1, &Config);
	testRequire(
		(pDecode != NULL) &&
		!xrtHttpDecodeWrite(
			pDecode,
			(xbytesview){ TestGzip, sizeof(TestGzip) },
			true,
			testHttpDecodeOutput,
			&Output
		) &&
		!xrtHttpDecodeDone(pDecode),
		"HTTP decoder ignored decoded body limit"
	);
	xrtHttpDecodeDestroy(pDecode);
}



/* 验证未对齐字段描述符和输出回调失败都保持公开终态。 */
static void testHttpDecodeEdges(void)
{
	static const xhttpfield Gzip = {
		XRT_STR_INIT("Content-Encoding"),
		XRT_STR_INIT("gzip")
	};
	uint8 Storage[sizeof(xhttpfield) + 1u];
	test_http_decode_output Output;
	xhttpdecode* pDecode;

	memcpy(Storage + 1u, &Gzip, sizeof(Gzip));
	memset(&Output, 0, sizeof(Output));
	pDecode = xrtHttpDecodeCreate(
		(const xhttpfield*)(Storage + 1u),
		1,
		NULL
	);
	testRequire(
		(pDecode != NULL) &&
		xrtHttpDecodeWrite(
			pDecode,
			(xbytesview){ TestGzip, sizeof(TestGzip) },
			true,
			testHttpDecodeOutput,
			&Output
		) &&
		xrtHttpDecodeDone(pDecode) &&
		(Output.Size == (sizeof(TestPlain) - 1u)),
		"unaligned HTTP decode field failed"
	);

	testRequire(
		xrtHttpDecodeReset(pDecode, NULL, 0, NULL),
		"HTTP decoder did not reset after unaligned field"
	);
	xrtClearError();
	testRequire(
		!xrtHttpDecodeWrite(
			pDecode,
			(xbytesview){ (const uint8*)TestPlain, sizeof(TestPlain) - 1u },
			true,
			testHttpDecodeStop,
			NULL
		) &&
		!xrtHttpDecodeDone(pDecode) &&
		(xrtHttpDecodeInputSize(pDecode) == 0) &&
		(xrtHttpDecodeOutputSize(pDecode) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_CANCELLED) &&
		(xrtErrorCode(xrtGetError()) == XHTTP_DECODE_ERROR_OUTPUT),
		"HTTP decode output rejection terminal state mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtHttpDecodeWrite(
			pDecode,
			(xbytesview){ NULL, 0 },
			true,
			NULL,
			NULL
		) &&
		(xrtErrorCode(xrtGetError()) == XHTTP_DECODE_ERROR_STATE),
		"failed HTTP decoder accepted more input"
	);
	testRequire(
		xrtHttpDecodeReset(pDecode, NULL, 0, NULL) &&
		xrtHttpDecodeWrite(
			pDecode,
			(xbytesview){ NULL, 0 },
			true,
			NULL,
			NULL
		) &&
		xrtHttpDecodeDone(pDecode),
		"HTTP decoder did not recover through explicit reset"
	);
	xrtHttpDecodeDestroy(pDecode);
}



/* 运行 HTTP 正文自动解码回归。 */
int main(void)
{
	testHttpDecodeGzip();
	testHttpDecodePassthrough();
	testHttpDecodeLimits();
	testHttpDecodeEdges();
	printf("[PASS] http_decode\n");
	return 0;
}
