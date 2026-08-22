#include "../test.h"



static const uint8 TestZlib[] = {
	0x78, 0x9C, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x57,
	0x48, 0xCE, 0xCF, 0x2D, 0x28, 0x4A, 0x2D, 0x2E,
	0x4E, 0x4D, 0x51, 0x28, 0xCF, 0x2F, 0xCA, 0x49,
	0x01, 0x00, 0x63, 0x85, 0x08, 0xB2
};

static const uint8 TestRaw[] = {
	0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x57, 0x48, 0xCE,
	0xCF, 0x2D, 0x28, 0x4A, 0x2D, 0x2E, 0x4E, 0x4D,
	0x51, 0x28, 0xCF, 0x2F, 0xCA, 0x49, 0x01, 0x00
};

static const uint8 TestGzip[] = {
	0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0xFF, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x57,
	0x48, 0xCE, 0xCF, 0x2D, 0x28, 0x4A, 0x2D, 0x2E,
	0x4E, 0x4D, 0x51, 0x28, 0xCF, 0x2F, 0xCA, 0x49,
	0x01, 0x00, 0xA1, 0x2D, 0x94, 0x53, 0x16, 0x00,
	0x00, 0x00
};

static const uint8 TestGzipMembers[] = {
	0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0xFF, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x57,
	0x00, 0x00, 0xF6, 0xF9, 0x81, 0xED, 0x06, 0x00,
	0x00, 0x00, 0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x02, 0xFF, 0x4B, 0xCE, 0xCF, 0x2D,
	0x28, 0x4A, 0x2D, 0x2E, 0x4E, 0x4D, 0x51, 0x28,
	0xCF, 0x2F, 0xCA, 0x49, 0x01, 0x00, 0x73, 0xB6,
	0xFE, 0x4A, 0x10, 0x00, 0x00, 0x00
};

static const uint8 TestGzipOptional[] = {
	0x1F, 0x8B, 0x08, 0x1E, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xFF, 0x03, 0x00, 0x41, 0x42, 0x43, 0x6E,
	0x61, 0x6D, 0x65, 0x00, 0x63, 0x6F, 0x6D, 0x6D,
	0x65, 0x6E, 0x74, 0x00, 0x43, 0x25, 0xCB, 0x48,
	0xCD, 0xC9, 0xC9, 0x57, 0x48, 0xCE, 0xCF, 0x2D,
	0x28, 0x4A, 0x2D, 0x2E, 0x4E, 0x4D, 0x51, 0x28,
	0xCF, 0x2F, 0xCA, 0x49, 0x01, 0x00, 0xA1, 0x2D,
	0x94, 0x53, 0x16, 0x00, 0x00, 0x00
};

static const char TestPlain[] = "hello compressed world";



/* 流式测试输出使用固定容量，避免把解码器行为和分配器混在一起。 */
typedef struct test_inflate_output {
	uint8 Data[128];
	size_t Size;
	bool Stop;
} test_inflate_output;



/* 回调重入探针记录同一解码器的危险生命周期操作。 */
typedef struct test_inflate_reentry {
	xinflate* Inflate;
	bool Called;
	bool WriteRejected;
	bool ResetRejected;
	bool DestroyRejected;
} test_inflate_reentry;



/* 收集解码分片，或按测试要求拒绝第一段输出。 */
static bool testInflateOutput(xbytesview Data, ptr pData)
{
	test_inflate_output* pOutput =
		(test_inflate_output*)pData;

	if ( pOutput->Stop ) {
		return false;
	}
	testRequire(
		Data.Size <=
			(sizeof(pOutput->Data) - pOutput->Size),
		"Inflate fixture output overflowed"
	);
	memcpy(
		pOutput->Data + pOutput->Size,
		Data.Data,
		Data.Size
	);
	pOutput->Size += Data.Size;
	return true;
}



/* 从输出回调尝试重入、复位和销毁同一解码器。 */
static bool testInflateReentryOutput(
	xbytesview Data,
	ptr pData
)
{
	test_inflate_reentry* pReentry =
		(test_inflate_reentry*)pData;

	(void)Data;
	if ( pReentry->Called ) {
		return true;
	}
	pReentry->Called = true;
	xrtClearError();
	pReentry->WriteRejected =
		!xrtInflateWrite(
			pReentry->Inflate,
			(xbytesview){ NULL, 0 },
			false,
			NULL,
			NULL
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XINFLATE_ERROR_STATE);
	xrtClearError();
	pReentry->ResetRejected =
		!xrtInflateReset(
			pReentry->Inflate,
			NULL
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XINFLATE_ERROR_STATE);
	xrtClearError();
	xrtInflateDestroy(pReentry->Inflate);
	pReentry->DestroyRejected =
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XINFLATE_ERROR_STATE);
	return true;
}



/* 用指定分片大小完成一次流式解码并校验输出。 */
static bool testInflateStream(
	xinflateformat Format,
	const uint8* pInput,
	size_t iInputSize,
	size_t iChunk
)
{
	xinflateconfig Config;
	test_inflate_output Output;
	xinflate* pInflate;
	size_t iOffset = 0;
	bool bSuccess = true;

	memset(&Output, 0, sizeof(Output));
	xrtInflateConfigInit(&Config);
	Config.Format = Format;
	Config.OutputLimit = sizeof(Output.Data);
	pInflate = xrtInflateCreate(&Config);
	if ( pInflate == NULL ) {
		return false;
	}
	while ( iOffset < iInputSize ) {
		size_t iSize = iInputSize - iOffset;

		if ( iSize > iChunk ) {
			iSize = iChunk;
		}
		if ( !xrtInflateWrite(
			pInflate,
			(xbytesview){ pInput + iOffset, iSize },
			(iOffset + iSize) == iInputSize,
			testInflateOutput,
			&Output
		) ) {
			bSuccess = false;
			break;
		}
		iOffset += iSize;
	}
	bSuccess = bSuccess &&
		xrtInflateDone(pInflate) &&
		(xrtInflateOutputSize(pInflate) ==
		 (sizeof(TestPlain) - 1u)) &&
		(Output.Size == (sizeof(TestPlain) - 1u)) &&
		(memcmp(
			Output.Data,
			TestPlain,
			Output.Size
		) == 0);
	xrtInflateDestroy(pInflate);
	return bSuccess;
}



/* 验证输出回调不能破坏正在执行的解码器生命周期。 */
static void testInflateReentry(void)
{
	test_inflate_reentry Reentry;
	xinflateconfig Config;
	xinflate* pInflate;

	memset(&Reentry, 0, sizeof(Reentry));
	xrtInflateConfigInit(&Config);
	Config.Format = XINFLATE_GZIP;
	pInflate = xrtInflateCreate(&Config);
	testRequire(
		pInflate != NULL,
		"Inflate reentry fixture create failed"
	);
	Reentry.Inflate = pInflate;
	testRequire(
		xrtInflateWrite(
			pInflate,
			(xbytesview){ TestGzip, sizeof(TestGzip) },
			true,
			testInflateReentryOutput,
			&Reentry
		) &&
		xrtInflateDone(pInflate) &&
		Reentry.Called &&
		Reentry.WriteRejected &&
		Reentry.ResetRejected &&
		Reentry.DestroyRejected,
		"Inflate callback reentry guard failed"
	);
	xrtInflateDestroy(pInflate);
	xrtClearError();
}



/* 验证旧版已经压实的四种包装与任意小分片边界。 */
static void testInflateFormats(void)
{
	testRequire(
		testInflateStream(
			XINFLATE_ZLIB,
			TestZlib,
			sizeof(TestZlib),
			1
		),
		"zlib streaming Inflate failed"
	);
	testRequire(
		testInflateStream(
			XINFLATE_RAW,
			TestRaw,
			sizeof(TestRaw),
			2
		),
		"raw DEFLATE streaming Inflate failed"
	);
	testRequire(
		testInflateStream(
			XINFLATE_DEFLATE,
			TestZlib,
			sizeof(TestZlib),
			3
		) &&
		testInflateStream(
			XINFLATE_DEFLATE,
			TestRaw,
			sizeof(TestRaw),
			3
		),
		"HTTP deflate compatibility detection failed"
	);
	testRequire(
		testInflateStream(
			XINFLATE_GZIP,
			TestGzip,
			sizeof(TestGzip),
			1
		) &&
		testInflateStream(
			XINFLATE_GZIP,
			TestGzipMembers,
			sizeof(TestGzipMembers),
			3
		),
		"gzip stream or concatenated members failed"
	);
}



/* 验证一次性便捷函数的拥有型结果和失败原子长度。 */
static void testInflateAll(void)
{
	xinflateconfig Config;
	bytes pOutput;
	size_t iSize = 91;

	xrtInflateConfigInit(&Config);
	Config.Format = XINFLATE_GZIP;
	pOutput = xrtInflateAll(
		(xbytesview){ TestGzip, sizeof(TestGzip) },
		&Config,
		&iSize
	);
	testRequire(
		(pOutput != NULL) &&
		(iSize == (sizeof(TestPlain) - 1u)) &&
		(memcmp(pOutput, TestPlain, iSize) == 0) &&
		(pOutput[iSize] == 0),
		"whole-buffer Inflate result mismatch"
	);
	xrtFree(pOutput);

	Config.OutputLimit = 5;
	iSize = 91;
	testRequire(
		(xrtInflateAll(
			(xbytesview){ TestGzip, sizeof(TestGzip) },
			&Config,
			&iSize
		) == NULL) &&
		(iSize == 91) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XINFLATE_ERROR_LIMIT),
		"Inflate output limit or failure atomicity mismatch"
	);
	xrtClearError();
}



/* 验证 gzip 可选字段、Header CRC、任意分片和精确 Header 上限。 */
static void testInflateGzipHeader(void)
{
	uint8 BadHeader[sizeof(TestGzipOptional)];
	xinflateconfig Config;
	bytes pOutput;
	size_t iSize = 91;

	testRequire(
		testInflateStream(
			XINFLATE_GZIP,
			TestGzipOptional,
			sizeof(TestGzipOptional),
			1
		),
		"gzip optional Header streaming failed"
	);
	xrtInflateConfigInit(&Config);
	Config.Format = XINFLATE_GZIP;
	Config.GzipHeaderLimit = 30;
	pOutput = xrtInflateAll(
		(xbytesview){
			TestGzipOptional,
			sizeof(TestGzipOptional)
		},
		&Config,
		&iSize
	);
	testRequire(
		(pOutput != NULL) &&
		(iSize == (sizeof(TestPlain) - 1u)) &&
		(memcmp(pOutput, TestPlain, iSize) == 0),
		"gzip exact Header limit failed"
	);
	xrtFree(pOutput);

	memcpy(
		BadHeader,
		TestGzipOptional,
		sizeof(BadHeader)
	);
	BadHeader[29] ^= 1u;
	iSize = 91;
	testRequire(
		(xrtInflateAll(
			(xbytesview){ BadHeader, sizeof(BadHeader) },
			&Config,
			&iSize
		 ) == NULL) &&
		(iSize == 91) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XINFLATE_ERROR_DATA),
		"gzip invalid Header CRC was accepted"
	);
	xrtClearError();

	Config.GzipHeaderLimit = 29;
	testRequire(
		(xrtInflateAll(
			(xbytesview){
				TestGzipOptional,
				sizeof(TestGzipOptional)
			},
			&Config,
			&iSize
		 ) == NULL) &&
		(iSize == 91) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XINFLATE_ERROR_LIMIT),
		"gzip Header limit accepted one excess byte"
	);
	xrtClearError();
}



/* 验证校验、截断、尾随数据、回调中止和终态约束。 */
static void testInflateFailures(void)
{
	static const uint8 InvalidDistance[] = {
		0x03, 0x02, 0x00
	};
	xinflateconfig Config;
	test_inflate_output Output;
	uint8 BadGzip[sizeof(TestGzip)];
	uint8 Trailing[sizeof(TestZlib) + 1u];
	xinflate* pInflate;

	memcpy(BadGzip, TestGzip, sizeof(BadGzip));
	BadGzip[sizeof(BadGzip) - 8u] ^= 1u;
	xrtInflateConfigInit(&Config);
	Config.Format = XINFLATE_GZIP;
	pInflate = xrtInflateCreate(&Config);
	testRequire(pInflate != NULL, "gzip failure decoder create failed");
	testRequire(
		!xrtInflateWrite(
			pInflate,
			(xbytesview){ BadGzip, sizeof(BadGzip) },
			true,
			NULL,
			NULL
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XINFLATE_ERROR_DATA) &&
		!xrtInflateWrite(
			pInflate,
			(xbytesview){ NULL, 0 },
			true,
			NULL,
			NULL
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"gzip checksum or failed terminal state mismatch"
	);
	xrtInflateDestroy(pInflate);
	xrtClearError();

	pInflate = xrtInflateCreate(&Config);
	testRequire(
		(pInflate != NULL) &&
		!xrtInflateWrite(
			pInflate,
			(xbytesview){
				TestGzip,
				sizeof(TestGzip) - 1u
			},
			true,
			NULL,
			NULL
		),
		"truncated gzip stream was accepted"
	);
	xrtInflateDestroy(pInflate);
	xrtClearError();

	memcpy(Trailing, TestZlib, sizeof(TestZlib));
	Trailing[sizeof(TestZlib)] = 0;
	Config.Format = XINFLATE_ZLIB;
	pInflate = xrtInflateCreate(&Config);
	testRequire(
		(pInflate != NULL) &&
		!xrtInflateWrite(
			pInflate,
			(xbytesview){ Trailing, sizeof(Trailing) },
			true,
			NULL,
			NULL
		),
		"zlib trailing data was accepted"
	);
	xrtInflateDestroy(pInflate);
	xrtClearError();

	memset(&Output, 0, sizeof(Output));
	Config.Format = XINFLATE_RAW;
	pInflate = xrtInflateCreate(&Config);
	testRequire(
		(pInflate != NULL) &&
		xrtInflateWrite(
			pInflate,
			(xbytesview){ TestRaw, sizeof(TestRaw) },
			true,
			NULL,
			NULL
		) &&
		xrtInflateReset(pInflate, &Config) &&
		!xrtInflateWrite(
			pInflate,
			(xbytesview){
				InvalidDistance,
				sizeof(InvalidDistance)
			},
			true,
			testInflateOutput,
			&Output
		) &&
		(Output.Size == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
		(xrtErrorCode(xrtGetError()) ==
		 XINFLATE_ERROR_DATA),
		"Inflate accepted a match before available history"
	);
	xrtInflateDestroy(pInflate);
	xrtClearError();

	memset(&Output, 0, sizeof(Output));
	Output.Stop = true;
	Config.Format = XINFLATE_GZIP;
	pInflate = xrtInflateCreate(&Config);
	testRequire(
		(pInflate != NULL) &&
		!xrtInflateWrite(
			pInflate,
			(xbytesview){ TestGzip, sizeof(TestGzip) },
			true,
			testInflateOutput,
			&Output
		) &&
		(xrtErrorCode(xrtGetError()) ==
		 XINFLATE_ERROR_OUTPUT),
		"Inflate output callback stop mismatch"
	);
	xrtInflateDestroy(pInflate);
	xrtClearError();
}



/* 验证复位复用、gzip Header 上限和配置验证。 */
static void testInflateReset(void)
{
	xinflateconfig Config;
	test_inflate_output Output;
	xinflate* pInflate;

	memset(&Output, 0, sizeof(Output));
	xrtInflateConfigInit(&Config);
	testRequire(
		(Config.WindowBits == XINFLATE_WINDOW_MAX),
		"Inflate default window mismatch"
	);
	Config.Format = XINFLATE_GZIP;
	pInflate = xrtInflateCreate(&Config);
	testRequire(
		(pInflate != NULL) &&
		xrtInflateWrite(
			pInflate,
			(xbytesview){ TestGzip, sizeof(TestGzip) },
			true,
			testInflateOutput,
			&Output
		),
		"Inflate pre-reset stream failed"
	);
	memset(&Output, 0, sizeof(Output));
	Config.Format = XINFLATE_RAW;
	testRequire(
		xrtInflateReset(pInflate, &Config) &&
		xrtInflateWrite(
			pInflate,
			(xbytesview){ TestRaw, sizeof(TestRaw) },
			true,
			testInflateOutput,
			&Output
		) &&
		(Output.Size == (sizeof(TestPlain) - 1u)),
		"Inflate reset and reuse failed"
	);
	xrtInflateDestroy(pInflate);

	xrtInflateConfigInit(&Config);
	Config.GzipHeaderLimit = 9;
	testRequire(
		(xrtInflateCreate(&Config) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XINFLATE_ERROR_CONFIG),
		"invalid gzip Header limit was accepted"
	);
	xrtClearError();

	xrtInflateConfigInit(&Config);
	Config.WindowBits = XINFLATE_WINDOW_MAX + 1u;
	testRequire(
		(xrtInflateCreate(&Config) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XINFLATE_ERROR_CONFIG),
		"invalid Inflate window was accepted"
	);
	xrtClearError();
}



/* 验证无对齐配置、地址回绕和参数失败原子性。 */
static void testInflateRanges(void)
{
	uint8 Storage[sizeof(xinflateconfig) + 1u];
	uint8 SizeStorage[sizeof(size_t) + 1u];
	xinflateconfig Config;
	xinflateconfig* pUnaligned =
		(xinflateconfig*)(Storage + 1u);
	size_t* pUnalignedSize =
		(size_t*)(SizeStorage + 1u);
	const xinflateconfig* pWrapping =
		(const xinflateconfig*)(uintptr_t)(UINTPTR_MAX - 1u);
	xinflate* pInflate;
	bytes pOutput;
	size_t iUnalignedSize = 91;
	size_t iSize = 91;

	xrtInflateConfigInit(pUnaligned);
	memcpy(&Config, pUnaligned, sizeof(Config));
	testRequire(
		(Config.Format == XINFLATE_DEFLATE) &&
		(Config.GzipHeaderLimit ==
		 XINFLATE_GZIP_HEADER_DEFAULT) &&
		(Config.WindowBits == XINFLATE_WINDOW_MAX),
		"unaligned Inflate config initialization failed"
	);
	Config.Format = XINFLATE_GZIP;
	memcpy(pUnaligned, &Config, sizeof(Config));
	pInflate = xrtInflateCreate(pUnaligned);
	testRequire(
		(pInflate != NULL) &&
		xrtInflateReset(pInflate, pUnaligned),
		"unaligned Inflate config snapshot failed"
	);

	testRequire(
		!xrtInflateWrite(
			pInflate,
			(xbytesview){
				(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
				4
			},
			true,
			NULL,
			NULL
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XINFLATE_ERROR_ARGUMENT),
		"Inflate accepted a wrapping input view"
	);
	xrtClearError();
	testRequire(
		xrtInflateWrite(
			pInflate,
			(xbytesview){ TestGzip, sizeof(TestGzip) },
			true,
			NULL,
			NULL
		) &&
		xrtInflateDone(pInflate),
		"invalid Inflate input changed the stream state"
	);
	xrtInflateDestroy(pInflate);
	memcpy(
		pUnalignedSize,
		&iUnalignedSize,
		sizeof(iUnalignedSize)
	);
	pOutput = xrtInflateAll(
		(xbytesview){ TestGzip, sizeof(TestGzip) },
		pUnaligned,
		pUnalignedSize
	);
	memcpy(
		&iUnalignedSize,
		pUnalignedSize,
		sizeof(iUnalignedSize)
	);
	testRequire(
		(pOutput != NULL) &&
		(iUnalignedSize == (sizeof(TestPlain) - 1u)) &&
		(memcmp(
			pOutput,
			TestPlain,
			iUnalignedSize
		 ) == 0),
		"Inflate rejected an unaligned output length"
	);
	xrtFree(pOutput);

	xrtInflateConfigInit(
		(xinflateconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XINFLATE_ERROR_ARGUMENT),
		"Inflate initialized a wrapping config range"
	);
	xrtClearError();
	testRequire(
		(xrtInflateCreate(pWrapping) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XINFLATE_ERROR_ARGUMENT),
		"Inflate accepted a wrapping config range"
	);
	xrtClearError();

	testRequire(
		(xrtInflateAll(
			(xbytesview){
				(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
				4
			},
			NULL,
			&iSize
		 ) == NULL) &&
		(iSize == 91) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XINFLATE_ERROR_ARGUMENT),
		"Inflate All changed length for a wrapping input"
	);
	xrtClearError();
	testRequire(
		(xrtInflateAll(
			(xbytesview){ TestGzip, sizeof(TestGzip) },
			NULL,
			(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XINFLATE_ERROR_ARGUMENT),
		"Inflate All accepted a wrapping output length"
	);
	xrtClearError();

	testRequire(
		!xrtInflateDone(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XINFLATE_ERROR_ARGUMENT),
		"Inflate Done null error mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtInflateOutputSize(NULL) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XINFLATE_ERROR_ARGUMENT),
		"Inflate output size null error mismatch"
	);
	xrtClearError();
}



/* 验证公开配置校验不会创建算法对象。 */
static void testInflateConfigValidation(void)
{
	xinflateconfig Config;

	xrtInflateConfigInit(&Config);
	testRequire(
		xrtInflateConfigValid(&Config),
		"default Inflate config was rejected"
	);
	Config.GzipHeaderLimit = 9;
	testRequire(
		!xrtInflateConfigValid(&Config) &&
		(xrtErrorCode(xrtGetError()) == XINFLATE_ERROR_CONFIG),
		"invalid Inflate config was accepted"
	);
	xrtClearError();
	testRequire(
		!xrtInflateConfigValid(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null Inflate config error mismatch"
	);
	xrtClearError();
}



/* 运行 Inflate 格式、流式、完整性、限额和复用边界。 */
int main(void)
{
	testInflateConfigValidation();
	testInflateReentry();
	testInflateFormats();
	testInflateAll();
	testInflateGzipHeader();
	testInflateFailures();
	testInflateReset();
	testInflateRanges();
	printf("[PASS] streaming Inflate\n");
	return 0;
}
