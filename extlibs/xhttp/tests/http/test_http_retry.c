#include "../test.h"

#include <xrt/http_retry.h>



/* 验证相对秒数、兼容日期、OWS 和严格非法值边界。 */
static void testHttpRetryAfterParse(void)
{
	static const xstrview Invalid[] = {
		XRT_STR_INIT(""),
		XRT_STR_INIT(" "),
		XRT_STR_INIT("+1"),
		XRT_STR_INIT("1.0"),
		XRT_STR_INIT("1 second"),
		XRT_STR_INIT("18446744073709551616"),
		XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 UTC")
	};
	xhttpretryafter Retry;
	size_t i;

	testRequire(
		xrtHttpRetryAfterParse(
			XRT_STR_LITERAL("\t120 "), &Retry
		) && (Retry.Kind == XHTTP_RETRY_AFTER_DELAY) &&
		(Retry.Seconds == UINT64_C(120)) &&
		(Retry.Date == 0),
		"Retry-After delay parse mismatch"
	);
	testRequire(
		xrtHttpRetryAfterParse(
			XRT_STR_LITERAL(
				"Sun, 06 Nov 1994 08:49:37 GMT"
			),
			&Retry
		) && (Retry.Kind == XHTTP_RETRY_AFTER_DATE) &&
		(Retry.Seconds == 0) &&
		(Retry.Date == INT64_C(784111777000000)),
		"Retry-After HTTP-date parse mismatch"
	);
	testRequire(
		xrtHttpRetryAfterParse(
			XRT_STR_LITERAL(
				"Sunday, 06-Nov-94 08:49:37 GMT"
			),
			&Retry
		) && (Retry.Kind == XHTTP_RETRY_AFTER_DATE) &&
		(Retry.Date == INT64_C(784111777000000)) &&
		xrtHttpRetryAfterParse(
			XRT_STR_LITERAL(
				"Sun Nov  6 08:49:37 1994"
			),
			&Retry
		) && (Retry.Kind == XHTTP_RETRY_AFTER_DATE) &&
		(Retry.Date == INT64_C(784111777000000)),
		"Retry-After obsolete HTTP-date compatibility mismatch"
	);
	for ( i = 0; i < (sizeof(Invalid) / sizeof(Invalid[0])); i++ ) {
		memset(&Retry, 0xA5, sizeof(Retry));
		testRequire(
			!xrtHttpRetryAfterParse(Invalid[i], &Retry) &&
			(Retry.Kind == XHTTP_RETRY_AFTER_NONE) &&
			(Retry.Seconds == 0) && (Retry.Date == 0),
			"Retry-After accepted malformed value"
		);
		xrtClearError();
	}
}



/* 验证墙钟换算、过去日期和秒到微秒的溢出保护。 */
static void testHttpRetryAfterDelay(void)
{
	xhttpretryafter Retry;
	uint64 iDelay = UINT64_C(0xA5A5A5A5);

	Retry.Kind = XHTTP_RETRY_AFTER_DELAY;
	Retry.Seconds = 2;
	Retry.Date = 0;
	testRequire(
		xrtHttpRetryAfterDelay(&Retry, 0, &iDelay) &&
		(iDelay == UINT64_C(2000000)),
		"Retry-After relative delay mismatch"
	);
	Retry.Kind = XHTTP_RETRY_AFTER_DATE;
	Retry.Date = INT64_C(784111777000000);
	testRequire(
		xrtHttpRetryAfterDelay(
			&Retry, INT64_C(784111772000000), &iDelay
		) && (iDelay == UINT64_C(5000000)),
		"Retry-After future date delay mismatch"
	);
	testRequire(
		xrtHttpRetryAfterDelay(
			&Retry, INT64_C(784111778000000), &iDelay
		) && (iDelay == 0),
		"Retry-After past date was not immediate"
	);
	Retry.Date = INT64_MAX;
	testRequire(
		xrtHttpRetryAfterDelay(
			&Retry, INT64_MIN, &iDelay
		) && (iDelay == UINT64_MAX),
		"Retry-After signed date span mismatch"
	);
	Retry.Kind = XHTTP_RETRY_AFTER_DELAY;
	Retry.Seconds = (UINT64_MAX / (uint64)XRT_TIME_SECOND) + 1u;
	iDelay = UINT64_C(0xA5A5A5A5);
	testRequire(
		!xrtHttpRetryAfterDelay(&Retry, 0, &iDelay) &&
		(iDelay == UINT64_C(0xA5A5A5A5)) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"Retry-After delay overflow was not atomic"
	);
	xrtClearError();
}



/* 验证字段缺失、唯一、重复和非法值具有稳定三态结果。 */
static void testHttpRetryAfterFields(void)
{
	static const xhttpfield Valid[] = {
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		},
		{
			XRT_STR_INIT("Retry-After"),
			XRT_STR_INIT("3")
		}
	};
	static const xhttpfield Duplicate[] = {
		{
			XRT_STR_INIT("Retry-After"),
			XRT_STR_INIT("1")
		},
		{
			XRT_STR_INIT("retry-after"),
			XRT_STR_INIT("2")
		}
	};
	static const xhttpfield Invalid[] = {
		{
			XRT_STR_INIT("Retry-After"),
			XRT_STR_INIT("later")
		}
	};
	uint64 iDelay = UINT64_C(99);

	testRequire(
		(xrtHttpRetryAfterFields(
			NULL, 0, 0, &iDelay
		 ) == XHTTP_NEXT_END) && (iDelay == 0),
		"Retry-After missing field mismatch"
	);
	testRequire(
		(xrtHttpRetryAfterFields(
			Valid, 2, 0, &iDelay
		 ) == XHTTP_NEXT_ITEM) &&
		(iDelay == UINT64_C(3000000)),
		"Retry-After unique field mismatch"
	);
	iDelay = UINT64_C(99);
	testRequire(
		(xrtHttpRetryAfterFields(
			Duplicate, 2, 0, &iDelay
		 ) == XHTTP_NEXT_ERROR) && (iDelay == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"Retry-After duplicate field mismatch"
	);
	xrtClearError();
	iDelay = UINT64_C(99);
	testRequire(
		(xrtHttpRetryAfterFields(
			Invalid, 1, 0, &iDelay
		 ) == XHTTP_NEXT_ERROR) && (iDelay == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"Retry-After malformed field mismatch"
	);
	xrtClearError();
}



/* 验证直接写出、长度查询、规范日期、短缓冲和分配便利层。 */
static void testHttpRetryAfterWrite(void)
{
	xhttpretryafter Retry = {
		XHTTP_RETRY_AFTER_DELAY,
		UINT64_C(120),
		0
	};
	char Output[32];
	char Before[32];
	size_t iSize;
	str sBuilt;

	testRequire(
		xrtHttpRetryAfterWrite(
			&Retry, NULL, 0, &iSize
		) && (iSize == 3u),
		"Retry-After length query mismatch"
	);
	memset(Output, 0x5A, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	testRequire(
		!xrtHttpRetryAfterWrite(
			&Retry, Output, 2u, &iSize
		) && (iSize == 3u) &&
		(memcmp(Output, Before, sizeof(Output)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"Retry-After short write was not atomic"
	);
	xrtClearError();
	testRequire(
		xrtHttpRetryAfterWrite(
			&Retry, Output, sizeof(Output), &iSize
		) && (iSize == 3u) &&
		(memcmp(Output, "120", 3u) == 0),
		"Retry-After delay write mismatch"
	);
	Retry.Seconds = UINT64_MAX;
	testRequire(
		xrtHttpRetryAfterWrite(
			&Retry, Output, sizeof(Output), &iSize
		) && (iSize == 20u) &&
		(memcmp(Output, "18446744073709551615", 20u) == 0),
		"Retry-After maximum delay write mismatch"
	);
	Retry.Seconds = UINT64_C(120);
	sBuilt = xrtHttpRetryAfterBuild(&Retry, &iSize);
	testRequire(
		(sBuilt != NULL) && (iSize == 3u) &&
		(strcmp(sBuilt, "120") == 0),
		"Retry-After delay build mismatch"
	);
	xrtFree(sBuilt);

	Retry.Kind = XHTTP_RETRY_AFTER_DATE;
	Retry.Seconds = 0;
	Retry.Date = INT64_C(784111777000000);
	testRequire(
		xrtHttpRetryAfterWrite(
			&Retry, Output, sizeof(Output), &iSize
		) && (iSize == 29u) &&
		(memcmp(
			Output,
			"Sun, 06 Nov 1994 08:49:37 GMT",
			29u
		) == 0),
		"Retry-After date write mismatch"
	);
}



/* 验证未对齐描述符、回绕范围、别名与失败输出契约。 */
static void testHttpRetryBoundaries(void)
{
	union {
		uintptr_t Align;
		uint8 Bytes[sizeof(xhttpretryafter) + 1u];
	} RetryStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(uint64) + 1u];
	} DelayStorage;
	xhttpretryafter Retry;
	xhttpretryafter Before;
	xhttpretryafter* pUnaligned =
		(xhttpretryafter*)(void*)(RetryStorage.Bytes + 1u);
	uint64 iDelay;
	size_t iSize = 91u;
	char Output[32];
	char OutputBefore[32];
	xstrview Wrapped = {
		(const char*)(uintptr_t)(UINTPTR_MAX - 1u),
		4u
	};

	testRequire(
		xrtHttpRetryAfterParse(
			XRT_STR_LITERAL("2"), pUnaligned
		) && xrtHttpRetryAfterDelay(
			pUnaligned,
			0,
			(uint64*)(void*)(DelayStorage.Bytes + 1u)
		),
		"Retry-After rejected unaligned storage"
	);
	memcpy(&iDelay, DelayStorage.Bytes + 1u, sizeof(iDelay));
	testRequire(
		(iDelay == UINT64_C(2000000)),
		"Retry-After unaligned result mismatch"
	);

	memset(&Retry, 0xA5, sizeof(Retry));
	Before = Retry;
	testRequire(
		!xrtHttpRetryAfterParse(Wrapped, &Retry) &&
		(memcmp(&Retry, &Before, sizeof(Retry)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Retry-After accepted a wrapped input range"
	);
	xrtClearError();
	Retry.Kind = XHTTP_RETRY_AFTER_DELAY;
	Retry.Seconds = 1;
	Retry.Date = 0;
	testRequire(
		!xrtHttpRetryAfterWrite(
			&Retry,
			(void*)(uintptr_t)(UINTPTR_MAX - 1u),
			4u,
			&iSize
		) && (iSize == 91u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Retry-After accepted a wrapped output capacity"
	);
	xrtClearError();
	Before = Retry;
	testRequire(
		!xrtHttpRetryAfterWrite(
			&Retry, &Retry, sizeof(Retry), &iSize
		) && (memcmp(&Retry, &Before, sizeof(Retry)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Retry-After accepted overlapping input and output"
	);
	xrtClearError();
	Retry.Kind = XHTTP_RETRY_AFTER_NONE;
	memset(Output, 0x5A, sizeof(Output));
	memcpy(OutputBefore, Output, sizeof(Output));
	memcpy(&Before, &Retry, sizeof(Before));
	testRequire(
		!xrtHttpRetryAfterWrite(
			&Retry, Output, sizeof(Output), &iSize
		) && (memcmp(&Retry, &Before, sizeof(Retry)) == 0) &&
		(memcmp(Output, OutputBefore, sizeof(Output)) == 0) &&
		(iSize == 91u) &&
		(xrtErrorKind(xrtGetError()) == XERR_VALUE),
		"Retry-After accepted an invalid descriptor kind"
	);
	xrtClearError();
	testRequire(
		!xrtHttpRetryBackoff(
			1,
			2,
			0,
			(uint64*)(uintptr_t)(UINTPTR_MAX - 3u)
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP retry backoff accepted a wrapped output"
	);
	xrtClearError();
}



/* 验证默认状态集合和封顶指数退避的精确边界。 */
static void testHttpRetryPolicy(void)
{
	static const uint16 Retry[] = {
		408, 421, 425, 429, 500, 502, 503, 504
	};
	static const uint16 NoRetry[] = {
		200, 301, 400, 401, 404, 409, 501, 505
	};
	uint64 iDelay;
	size_t i;

	for ( i = 0; i < (sizeof(Retry) / sizeof(Retry[0])); i++ ) {
		testRequire(
			xrtHttpRetryStatusDefault(Retry[i]),
			"temporary status was not retryable"
		);
	}
	for ( i = 0; i < (sizeof(NoRetry) / sizeof(NoRetry[0])); i++ ) {
		testRequire(
			!xrtHttpRetryStatusDefault(NoRetry[i]),
			"permanent status was retryable"
		);
	}
	testRequire(
		xrtHttpRetryBackoff(100000, 1600000, 0, &iDelay) &&
		(iDelay == 100000) &&
		xrtHttpRetryBackoff(100000, 1600000, 3, &iDelay) &&
		(iDelay == 800000) &&
		xrtHttpRetryBackoff(100000, 1600000, 4, &iDelay) &&
		(iDelay == 1600000) &&
		xrtHttpRetryBackoff(100000, 1600000, UINT32_MAX, &iDelay) &&
		(iDelay == 1600000) &&
		xrtHttpRetryBackoff(1, UINT64_MAX, UINT32_MAX, &iDelay) &&
		(iDelay == UINT64_MAX) &&
		xrtHttpRetryBackoff(0, 1600000, UINT32_MAX, &iDelay) &&
		(iDelay == 0),
		"HTTP retry backoff mismatch"
	);
	iDelay = UINT64_C(77);
	testRequire(
		!xrtHttpRetryBackoff(2, 1, 0, &iDelay) &&
		(iDelay == UINT64_C(77)) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP retry invalid backoff was not atomic"
	);
	xrtClearError();
}



/* 运行 HTTP 重试协议层全部边界测试。 */
int main(void)
{
	testHttpRetryAfterParse();
	testHttpRetryAfterDelay();
	testHttpRetryAfterFields();
	testHttpRetryAfterWrite();
	testHttpRetryBoundaries();
	testHttpRetryPolicy();
	puts("[PASS] HTTP retry protocol");
	return 0;
}


