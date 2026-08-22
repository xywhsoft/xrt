#include "../test.h"



/* 固定输出收集器避免把编码器行为和动态分配混在一起。 */
typedef struct test_deflate_output {
	uint8 Data[4096];
	size_t Size;
	bool Stop;
} test_deflate_output;



/* 回调重入探针记录三个危险生命周期操作是否都被状态机拒绝。 */
typedef struct test_deflate_reentry {
	xdeflate* Deflate;
	bool Called;
	bool WriteRejected;
	bool ResetRejected;
	bool DestroyRejected;
} test_deflate_reentry;



/* 收集完整编码片段，或按测试要求拒绝第一段输出。 */
static bool testDeflateOutput(
	xbytesview Data,
	ptr pData
)
{
	test_deflate_output* pOutput =
		(test_deflate_output*)pData;

	if ( pOutput->Stop ) {
		return false;
	}
	testRequire(
		Data.Size <=
			(sizeof(pOutput->Data) - pOutput->Size),
		"Deflate fixture output overflowed"
	);
	memcpy(
		pOutput->Data + pOutput->Size,
		Data.Data,
		Data.Size
	);
	pOutput->Size += Data.Size;
	return true;
}



/* 从输出回调尝试重入、复位和销毁同一编码器。 */
static bool testDeflateReentryOutput(
	xbytesview Data,
	ptr pData
)
{
	test_deflate_reentry* pReentry =
		(test_deflate_reentry*)pData;

	(void)Data;
	if ( pReentry->Called ) {
		return true;
	}
	pReentry->Called = true;
	xrtClearError();
	pReentry->WriteRejected =
		!xrtDeflateWrite(
			pReentry->Deflate,
			(xbytesview){ NULL, 0 },
			XDEFLATE_FLUSH_NONE,
			NULL,
			NULL
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XDEFLATE_ERROR_STATE);
	xrtClearError();
	pReentry->ResetRejected =
		!xrtDeflateReset(
			pReentry->Deflate,
			NULL
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XDEFLATE_ERROR_STATE);
	xrtClearError();
	xrtDeflateDestroy(pReentry->Deflate);
	pReentry->DestroyRejected =
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XDEFLATE_ERROR_STATE);
	return true;
}



/* 用自定义结构化错误中止输出，验证编码器不覆盖消费者原因。 */
static bool testDeflateOutputError(
	xbytesview Data,
	ptr pData
)
{
	xerror* pError;

	(void)Data;
	(void)pData;
	pError = xrtErrorCreate(
		XERR_IO,
		"test.deflate.output",
		91,
		"custom output failure"
	);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 验证输出回调不能破坏正在执行的编码器生命周期。 */
static void testDeflateReentry(void)
{
	static const char Plain[] = "callback reentry";
	test_deflate_reentry Reentry;
	xdeflate* pDeflate;

	memset(&Reentry, 0, sizeof(Reentry));
	pDeflate = xrtDeflateCreate(NULL);
	testRequire(
		pDeflate != NULL,
		"Deflate reentry fixture create failed"
	);
	Reentry.Deflate = pDeflate;
	testRequire(
		xrtDeflateWrite(
			pDeflate,
			(xbytesview){
				(cbytes)Plain,
				sizeof(Plain) - 1u
			},
			XDEFLATE_FLUSH_FINISH,
			testDeflateReentryOutput,
			&Reentry
		) &&
		xrtDeflateDone(pDeflate) &&
		Reentry.Called &&
		Reentry.WriteRejected &&
		Reentry.ResetRejected &&
		Reentry.DestroyRejected,
		"Deflate callback reentry guard failed"
	);
	xrtDeflateDestroy(pDeflate);
	xrtClearError();
}



/* 验证三种空数据流的稳定线路表示和一次性结果所有权。 */
static void testDeflateEmpty(void)
{
	static const uint8 Raw[] = {
		0x03, 0x00
	};
	static const uint8 Zlib[] = {
		0x78, 0x9C, 0x03, 0x00,
		0x00, 0x00, 0x00, 0x01
	};
	static const uint8 Gzip[] = {
		0x1F, 0x8B, 0x08, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0xFF, 0x03, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00
	};
	const uint8* Expected[] = {
		Raw,
		Zlib,
		Gzip
	};
	const size_t Sizes[] = {
		sizeof(Raw),
		sizeof(Zlib),
		sizeof(Gzip)
	};
	xdeflateconfig Config;
	size_t i;

	for ( i = 0; i < 3u; i++ ) {
		bytes pOutput;
		size_t iSize = 77;

		xrtDeflateConfigInit(&Config);
		Config.Format = (xdeflateformat)i;
		pOutput = xrtDeflateAll(
			(xbytesview){ NULL, 0 },
			&Config,
			&iSize
		);
		testRequire(
			(pOutput != NULL) &&
			(iSize == Sizes[i]) &&
			(memcmp(
				pOutput,
				Expected[i],
				iSize
			 ) == 0) &&
			(pOutput[iSize] == 0),
			"Deflate empty stream mismatch"
		);
		xrtFree(pOutput);
	}
}



/* 验证分片 gzip 的确定性 Header、CRC32、ISIZE 和终态复用。 */
static void testDeflateGzip(void)
{
	static const char Plain[] =
		"hello compressed world";
	static const uint8 Trailer[] = {
		0xA1, 0x2D, 0x94, 0x53,
		0x16, 0x00, 0x00, 0x00
	};
	xdeflateconfig Config;
	test_deflate_output Output;
	xdeflate* pDeflate;

	memset(&Output, 0, sizeof(Output));
	xrtDeflateConfigInit(&Config);
	pDeflate = xrtDeflateCreate(&Config);
	testRequire(
		pDeflate != NULL,
		"gzip Deflate create failed"
	);
	testRequire(
		xrtDeflateWrite(
			pDeflate,
			(xbytesview){
				(cbytes)Plain,
				5
			},
			XDEFLATE_FLUSH_NONE,
			testDeflateOutput,
			&Output
		) &&
		xrtDeflateWrite(
			pDeflate,
			(xbytesview){
				(cbytes)Plain + 5u,
				sizeof(Plain) - 1u - 5u
			},
			XDEFLATE_FLUSH_FINISH,
			testDeflateOutput,
			&Output
		),
		"gzip streaming Deflate failed"
	);
	testRequire(
		(Output.Size > 18u) &&
		(memcmp(
			Output.Data,
			"\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff",
			10
		 ) == 0) &&
		(memcmp(
			Output.Data + Output.Size -
				sizeof(Trailer),
			Trailer,
			sizeof(Trailer)
		 ) == 0) &&
		xrtDeflateDone(pDeflate) &&
		(xrtDeflateOutputSize(pDeflate) ==
		 Output.Size),
		"gzip framing or counters mismatch"
	);
	testRequire(
		!xrtDeflateWrite(
			pDeflate,
			(xbytesview){ NULL, 0 },
			XDEFLATE_FLUSH_FINISH,
			NULL,
			NULL
		) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_STATE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XDEFLATE_ERROR_STATE),
		"Deflate terminal state accepted another write"
	);
	xrtClearError();
	Config.Format = XDEFLATE_RAW;
	testRequire(
		xrtDeflateReset(pDeflate, &Config) &&
		!xrtDeflateDone(pDeflate) &&
		(xrtDeflateOutputSize(pDeflate) == 0) &&
		xrtDeflateWrite(
			pDeflate,
			(xbytesview){ NULL, 0 },
			XDEFLATE_FLUSH_FINISH,
			NULL,
			NULL
		) &&
		xrtDeflateDone(pDeflate),
		"Deflate reset reuse failed"
	);
	xrtDeflateDestroy(pDeflate);
}



/* 验证 SYNC/FULL 建立可继续的数据流边界和标准空块尾部。 */
static void testDeflateFlushes(void)
{
	static const uint8 Tail[4] = {
		0x00, 0x00, 0xFF, 0xFF
	};
	xdeflateconfig Config;
	test_deflate_output Output;
	xdeflate* pDeflate;
	size_t iFirst;
	size_t iSecond;

	memset(&Output, 0, sizeof(Output));
	xrtDeflateConfigInit(&Config);
	Config.Format = XDEFLATE_RAW;
	pDeflate = xrtDeflateCreate(&Config);
	testRequire(
		(pDeflate != NULL) &&
		xrtDeflateWrite(
			pDeflate,
			XRT_BYTES_LITERAL("alpha"),
			XDEFLATE_FLUSH_SYNC,
			testDeflateOutput,
			&Output
		),
		"Deflate SYNC flush failed"
	);
	iFirst = Output.Size;
	testRequire(
		(iFirst >= sizeof(Tail)) &&
		(memcmp(
			Output.Data + iFirst - sizeof(Tail),
			Tail,
			sizeof(Tail)
		 ) == 0),
		"Deflate SYNC flush tail mismatch"
	);
	testRequire(
		xrtDeflateWrite(
			pDeflate,
			XRT_BYTES_LITERAL("beta"),
			XDEFLATE_FLUSH_FULL,
			testDeflateOutput,
			&Output
		),
		"Deflate FULL flush failed"
	);
	iSecond = Output.Size;
	testRequire(
		(iSecond > iFirst) &&
		(iSecond >= sizeof(Tail)) &&
		(memcmp(
			Output.Data + iSecond - sizeof(Tail),
			Tail,
			sizeof(Tail)
		 ) == 0),
		"Deflate FULL flush tail mismatch"
	);
	testRequire(
		xrtDeflateWrite(
			pDeflate,
			XRT_BYTES_LITERAL("gamma"),
			XDEFLATE_FLUSH_FINISH,
			testDeflateOutput,
			&Output
		) &&
		xrtDeflateDone(pDeflate),
		"Deflate continuation after Flush failed"
	);
	xrtDeflateDestroy(pDeflate);
}



/* 验证配置、输出限额、回调拒绝和失败终态。 */
static void testDeflateFailures(void)
{
	xdeflateconfig Config;
	xdeflateconfig Invalid;
	test_deflate_output Output;
	xdeflate* pDeflate;
	size_t iSize = 91;

	xrtDeflateConfigInit(&Config);
	testRequire(
		(Config.WindowBits == XDEFLATE_WINDOW_MAX),
		"Deflate default window mismatch"
	);
	Config.Level = 11;
	testRequire(
		(xrtDeflateCreate(&Config) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(strcmp(
			xrtErrorDomain(xrtGetError()),
			"xrt.deflate"
		 ) == 0) &&
		(xrtErrorCode(xrtGetError()) ==
		 XDEFLATE_ERROR_CONFIG),
		"Deflate invalid config error mismatch"
	);
	xrtClearError();

	xrtDeflateConfigInit(&Config);
	Config.WindowBits = XDEFLATE_WINDOW_MIN - 1u;
	testRequire(
		(xrtDeflateCreate(&Config) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XDEFLATE_ERROR_CONFIG),
		"Deflate accepted an invalid window"
	);
	xrtClearError();

	xrtDeflateConfigInit(&Config);
	Config.OutputLimit = 9;
	testRequire(
		(xrtDeflateAll(
			XRT_BYTES_LITERAL("limited"),
			&Config,
			&iSize
		 ) == NULL) &&
		(iSize == 91) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(xrtErrorCode(xrtGetError()) ==
		 XDEFLATE_ERROR_LIMIT),
		"Deflate output limit or failure atomicity mismatch"
	);
	xrtClearError();

	memset(&Output, 0, sizeof(Output));
	Output.Stop = true;
	xrtDeflateConfigInit(&Config);
	Config.Format = XDEFLATE_RAW;
	pDeflate = xrtDeflateCreate(&Config);
	testRequire(
		(pDeflate != NULL) &&
		!xrtDeflateWrite(
			pDeflate,
			XRT_BYTES_LITERAL("stopped output"),
			XDEFLATE_FLUSH_FINISH,
			testDeflateOutput,
			&Output
		) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_CANCELLED) &&
		(xrtErrorCode(xrtGetError()) ==
		 XDEFLATE_ERROR_OUTPUT) &&
		!xrtDeflateWrite(
			pDeflate,
			(xbytesview){ NULL, 0 },
			XDEFLATE_FLUSH_FINISH,
			NULL,
			NULL
		),
		"Deflate callback failure contract mismatch"
	);
	xrtClearError();
	testRequire(
		xrtDeflateReset(pDeflate, &Config),
		"Deflate failed state did not reset"
	);
	xrtDeflateDestroy(pDeflate);

	pDeflate = xrtDeflateCreate(&Config);
	Invalid = Config;
	Invalid.Level = -1;
	testRequire(
		(pDeflate != NULL) &&
		!xrtDeflateReset(
			pDeflate,
			&Invalid
		) &&
		(xrtErrorCode(xrtGetError()) ==
		 XDEFLATE_ERROR_CONFIG),
		"Deflate invalid Reset was accepted"
	);
	xrtClearError();
	testRequire(
		xrtDeflateWrite(
			pDeflate,
			XRT_BYTES_LITERAL("reset atomic"),
			XDEFLATE_FLUSH_FINISH,
			NULL,
			NULL
		) &&
		xrtDeflateDone(pDeflate),
		"Deflate invalid Reset changed the active stream"
	);
	xrtDeflateDestroy(pDeflate);

	pDeflate = xrtDeflateCreate(&Config);
	testRequire(
		(pDeflate != NULL) &&
		!xrtDeflateWrite(
			pDeflate,
			XRT_BYTES_LITERAL("custom error"),
			XDEFLATE_FLUSH_FINISH,
			testDeflateOutputError,
			NULL
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_IO) &&
		(strcmp(
			xrtErrorDomain(xrtGetError()),
			"test.deflate.output"
		 ) == 0) &&
		(xrtErrorCode(xrtGetError()) == 91),
		"Deflate replaced the output callback error"
	);
	xrtClearError();
	xrtDeflateDestroy(pDeflate);
}



/* 验证无对齐配置、地址回绕和参数失败原子性。 */
static void testDeflateRanges(void)
{
	uint8 Storage[sizeof(xdeflateconfig) + 1u];
	uint8 SizeStorage[sizeof(size_t) + 1u];
	xdeflateconfig Config;
	xdeflateconfig* pUnaligned =
		(xdeflateconfig*)(Storage + 1u);
	size_t* pUnalignedSize =
		(size_t*)(SizeStorage + 1u);
	const xdeflateconfig* pWrapping =
		(const xdeflateconfig*)(uintptr_t)(UINTPTR_MAX - 1u);
	xdeflate* pDeflate;
	bytes pOutput;
	size_t iUnalignedSize = 91;
	size_t iSize = 91;

	xrtDeflateConfigInit(pUnaligned);
	memcpy(&Config, pUnaligned, sizeof(Config));
	testRequire(
		(Config.Format == XDEFLATE_GZIP) &&
		(Config.Level == XDEFLATE_LEVEL_DEFAULT) &&
		(Config.WindowBits == XDEFLATE_WINDOW_MAX),
		"unaligned Deflate config initialization failed"
	);
	pDeflate = xrtDeflateCreate(pUnaligned);
	testRequire(
		(pDeflate != NULL) &&
		xrtDeflateReset(pDeflate, pUnaligned),
		"unaligned Deflate config snapshot failed"
	);

	testRequire(
		!xrtDeflateWrite(
			pDeflate,
			(xbytesview){
				(cbytes)(uintptr_t)(UINTPTR_MAX - 1u),
				4
			},
			XDEFLATE_FLUSH_FINISH,
			NULL,
			NULL
		) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XDEFLATE_ERROR_ARGUMENT),
		"Deflate accepted a wrapping input view"
	);
	xrtClearError();
	testRequire(
		xrtDeflateWrite(
			pDeflate,
			XRT_BYTES_LITERAL("range state"),
			XDEFLATE_FLUSH_FINISH,
			NULL,
			NULL
		) &&
		xrtDeflateDone(pDeflate),
		"invalid Deflate input changed the stream state"
	);
	xrtDeflateDestroy(pDeflate);
	memcpy(
		pUnalignedSize,
		&iUnalignedSize,
		sizeof(iUnalignedSize)
	);
	pOutput = xrtDeflateAll(
		XRT_BYTES_LITERAL("unaligned output length"),
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
		(iUnalignedSize > 0),
		"Deflate rejected an unaligned output length"
	);
	xrtFree(pOutput);

	xrtDeflateConfigInit(
		(xdeflateconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XDEFLATE_ERROR_ARGUMENT),
		"Deflate initialized a wrapping config range"
	);
	xrtClearError();
	testRequire(
		(xrtDeflateCreate(pWrapping) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XDEFLATE_ERROR_ARGUMENT),
		"Deflate accepted a wrapping config range"
	);
	xrtClearError();

	testRequire(
		(xrtDeflateAll(
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
		 XDEFLATE_ERROR_ARGUMENT),
		"Deflate All changed length for a wrapping input"
	);
	xrtClearError();
	testRequire(
		(xrtDeflateAll(
			XRT_BYTES_LITERAL("range output"),
			NULL,
			(size_t*)(uintptr_t)(UINTPTR_MAX - 1u)
		 ) == NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 XDEFLATE_ERROR_ARGUMENT),
		"Deflate All accepted a wrapping output length"
	);
	xrtClearError();
}



/* 验证全部压缩级别和策略都能建立并结束合法数据流。 */
static void testDeflateOptions(void)
{
	static const int32 Levels[] = {
		0, 1, 6, 10
	};
	xdeflateconfig Config;
	size_t i;
	size_t j;

	for ( i = 0;
		i < (sizeof(Levels) / sizeof(Levels[0]));
		i++ ) {
		for ( j = XDEFLATE_STRATEGY_DEFAULT;
			j <= XDEFLATE_STRATEGY_FIXED;
			j++ ) {
			xdeflate* pDeflate;

			xrtDeflateConfigInit(&Config);
			Config.Format = XDEFLATE_ZLIB;
			Config.Level = Levels[i];
			Config.Strategy =
				(xdeflatestrategy)j;
			pDeflate = xrtDeflateCreate(&Config);
			testRequire(
				(pDeflate != NULL) &&
				xrtDeflateWrite(
					pDeflate,
					XRT_BYTES_LITERAL(
						"option matrix option matrix"
					),
					XDEFLATE_FLUSH_FINISH,
					NULL,
					NULL
				) &&
				xrtDeflateDone(pDeflate),
				"Deflate level or strategy failed"
			);
			xrtDeflateDestroy(pDeflate);
		}
	}
}



/* 验证公开配置校验不会创建算法对象。 */
static void testDeflateConfigValidation(void)
{
	xdeflateconfig Config;

	xrtDeflateConfigInit(&Config);
	testRequire(
		xrtDeflateConfigValid(&Config),
		"default Deflate config was rejected"
	);
	Config.Level = 11;
	testRequire(
		!xrtDeflateConfigValid(&Config) &&
		(xrtErrorCode(xrtGetError()) == XDEFLATE_ERROR_CONFIG),
		"invalid Deflate config was accepted"
	);
	xrtClearError();
	testRequire(
		!xrtDeflateConfigValid(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"null Deflate config error mismatch"
	);
	xrtClearError();
}



/* 运行独立 Deflate 的包装、Flush、状态和边界回归。 */
int main(void)
{
	testDeflateConfigValidation();
	testDeflateReentry();
	testDeflateEmpty();
	testDeflateGzip();
	testDeflateFlushes();
	testDeflateFailures();
	testDeflateRanges();
	testDeflateOptions();
	printf("[PASS] deflate\n");
	return 0;
}
