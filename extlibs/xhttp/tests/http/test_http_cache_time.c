#include "../test.h"

#include <xrt/http_cache_time.h>



/* 验证 Date、Age 和 Expires 的字段事实与历史日期格式复用。 */
static void testHttpCacheTimeMetadata(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		},
		{
			XRT_STR_INIT("Age"),
			XRT_STR_INIT(" 10, 99 ")
		},
		{
			XRT_STR_INIT("age"),
			XRT_STR_INIT("123")
		},
		{
			XRT_STR_INIT("Expires"),
			XRT_STR_INIT("Sunday, 06-Nov-94 08:50:37 GMT")
		}
	};
	static const xhttpfield Invalid[] = {
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("not-a-date")
		},
		{
			XRT_STR_INIT("date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		},
		{
			XRT_STR_INIT("Age"),
			XRT_STR_INIT("x, 20")
		},
		{
			XRT_STR_INIT("Expires"),
			XRT_STR_INIT("")
		}
	};
	static const xhttpfield EmptyAge[] = {
		{
			XRT_STR_INIT("Age"),
			XRT_STR_INIT(" , \t, ")
		}
	};
	xhttpcachetime Time;

	testRequire(
		xrtHttpCacheTimeParse(
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			&Time
		) &&
		xrtHttpCacheTimeValid(&Time) &&
		(Time.DateCount == 1) &&
		(Time.AgeCount == 2) &&
		(Time.AgeMemberCount == 3) &&
		(Time.ExpiresCount == 1) &&
		(Time.Age == 10) &&
		((Time.Expires - Time.Date) ==
		 (60 * XRT_TIME_SECOND)) &&
		((Time.Flags & (
			XHTTP_CACHE_TIME_DATE |
			XHTTP_CACHE_TIME_AGE |
			XHTTP_CACHE_TIME_EXPIRES |
			XHTTP_CACHE_TIME_AGE_EXTRA
		 )) == (
			XHTTP_CACHE_TIME_DATE |
			XHTTP_CACHE_TIME_AGE |
			XHTTP_CACHE_TIME_EXPIRES |
			XHTTP_CACHE_TIME_AGE_EXTRA
		 )),
		"HTTP cache time metadata mismatch"
	);
	testRequire(
		xrtHttpCacheTimeParse(
			Invalid,
			sizeof(Invalid) / sizeof(Invalid[0]),
			&Time
		) &&
		xrtHttpCacheTimeValid(&Time) &&
		(Time.DateCount == 2) &&
		(Time.AgeCount == 1) &&
		(Time.AgeMemberCount == 2) &&
		(Time.Age == 0) &&
		((Time.Flags & (
			XHTTP_CACHE_TIME_DATE_DUPLICATE |
			XHTTP_CACHE_TIME_DATE_INVALID |
			XHTTP_CACHE_TIME_AGE_EXTRA |
			XHTTP_CACHE_TIME_AGE_INVALID |
			XHTTP_CACHE_TIME_EXPIRES_INVALID
		 )) == (
			XHTTP_CACHE_TIME_DATE_DUPLICATE |
			XHTTP_CACHE_TIME_DATE_INVALID |
			XHTTP_CACHE_TIME_AGE_EXTRA |
			XHTTP_CACHE_TIME_AGE_INVALID |
			XHTTP_CACHE_TIME_EXPIRES_INVALID
		 )),
		"invalid HTTP cache time facts were hidden"
	);
	testRequire(
		xrtHttpCacheTimeParse(
			EmptyAge, 1, &Time
		) &&
		xrtHttpCacheTimeValid(&Time) &&
		(Time.AgeMemberCount == 0) &&
		((Time.Flags &
		  XHTTP_CACHE_TIME_AGE_INVALID) != 0),
		"empty Age list was accepted"
	);
}



/* 验证 current_age 公式、Age 容错、乱序时钟和饱和。 */
static void testHttpCacheCurrentAge(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		},
		{
			XRT_STR_INIT("Age"),
			XRT_STR_INIT("10, 200")
		}
	};
	static const xhttpfield InvalidAge[] = {
		{
			XRT_STR_INIT("Age"),
			XRT_STR_INIT("invalid")
		}
	};
	static const xhttpfield InvalidDate[] = {
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("invalid")
		}
	};
	static const xhttpfield DuplicateDate[] = {
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:38 GMT")
		}
	};
	static const xhttpfield HugeAge[] = {
		{
			XRT_STR_INIT("Age"),
			XRT_STR_INIT("999999999999999999999")
		}
	};
	xhttpcachetime Time;
	xhttpcacheage Age;
	xhttpcacheage Before;
	xhttpcachefreshness Freshness;
	xtime iResponseTime;

	testRequire(
		xrtHttpCacheTimeParse(Fields, 2, &Time),
		"current age metadata setup failed"
	);
	iResponseTime = Time.Date + (4 * XRT_TIME_SECOND);
	testRequire(
		xrtHttpCacheCurrentAge(
			&Time,
			iResponseTime,
			UINT64_C(1000000),
			UINT64_C(3000000),
			UINT64_C(8000000),
			&Age
		) == XHTTP_CACHE_CALC_READY &&
		(Age.ApparentAge ==
		 UINT64_C(4000000)) &&
		(Age.ResponseDelay ==
		 UINT64_C(2000000)) &&
		(Age.CorrectedAgeValue ==
		 UINT64_C(12000000)) &&
		(Age.CorrectedInitialAge ==
		 UINT64_C(12000000)) &&
		(Age.ResidentTime ==
		 UINT64_C(5000000)) &&
		(Age.CurrentAge ==
		 UINT64_C(17000000)) &&
		(Age.CurrentAgeSeconds == 17),
		"RFC current age formula mismatch"
	);
	Freshness.Lifetime = UINT64_C(17000000);
	Freshness.Source = XHTTP_CACHE_FRESHNESS_MAX_AGE;
	testRequire(
		!xrtHttpCacheFresh(&Age, &Freshness),
		"age equal to lifetime was considered fresh"
	);
	Freshness.Lifetime++;
	testRequire(
		xrtHttpCacheFresh(&Age, &Freshness),
		"younger response was considered stale"
	);
	Freshness.Source =
		XHTTP_CACHE_FRESHNESS_HEURISTIC;
	testRequire(
		xrtHttpCacheAgeValid(&Age) &&
		xrtHttpCacheFreshnessValid(&Freshness) &&
		xrtHttpCacheFresh(&Age, &Freshness),
		"heuristic freshness could not enter policy"
	);
	Freshness.Source =
		XHTTP_CACHE_FRESHNESS_MAX_AGE;
	testRequire(
		xrtHttpCacheTimeParse(
			InvalidAge, 1, &Time
		) &&
		(xrtHttpCacheCurrentAge(
			&Time,
			0,
			UINT64_C(1000000),
			UINT64_C(3000000),
			UINT64_C(8000000),
			&Age
		 ) == XHTTP_CACHE_CALC_READY) &&
		(Age.CurrentAge == UINT64_C(7000000)),
		"invalid Age was not ignored"
	);
	testRequire(
		xrtHttpCacheTimeParse(
			InvalidDate, 1, &Time
		) &&
		(xrtHttpCacheCurrentAge(
			&Time, 99, 1, 2, 3, &Age
		 ) == XHTTP_CACHE_CALC_READY) &&
		(Age.ApparentAge == 0) &&
		(Age.CurrentAge == 2),
		"invalid Date did not use response time"
	);
	testRequire(
		xrtHttpCacheTimeParse(
			DuplicateDate, 2, &Time
		),
		"duplicate Date setup failed"
	);
	memset(&Before, 0x5A, sizeof(Before));
	Age = Before;
	testRequire(
		(xrtHttpCacheCurrentAge(
			&Time, 0, 1, 2, 3, &Age
		 ) == XHTTP_CACHE_CALC_INVALID) &&
		(memcmp(&Age, &Before, sizeof(Age)) == 0),
		"duplicate Date changed age output"
	);
	xrtHttpCacheTimeInit(&Time);
	Age = Before;
	testRequire(
		(xrtHttpCacheCurrentAge(
			&Time, 0, 3, 2, 4, &Age
		 ) == XHTTP_CACHE_CALC_ERROR) &&
		(memcmp(&Age, &Before, sizeof(Age)) == 0),
		"out-of-order cache clocks changed output"
	);
	xrtClearError();
	testRequire(
		xrtHttpCacheTimeParse(HugeAge, 1, &Time) &&
		(xrtHttpCacheCurrentAge(
			&Time,
			0,
			0,
			UINT64_MAX,
			UINT64_MAX,
			&Age
		 ) == XHTTP_CACHE_CALC_READY) &&
		(Age.CorrectedAgeValue == UINT64_MAX) &&
		(Age.CurrentAge == UINT64_MAX) &&
		(Age.CurrentAgeSeconds ==
		 XHTTP_CACHE_DELTA_MAX),
		"current age saturation mismatch"
	);
	xrtHttpCacheTimeInit(&Time);
	Time.Date = INT64_MIN;
	Time.DateCount = 1;
	Time.Flags = XHTTP_CACHE_TIME_DATE;
	testRequire(
		xrtHttpCacheTimeValid(&Time) &&
		(xrtHttpCacheCurrentAge(
			&Time,
			INT64_MAX,
			0,
			0,
			0,
			&Age
		 ) == XHTTP_CACHE_CALC_READY) &&
		(Age.ApparentAge == UINT64_MAX) &&
		(Age.CurrentAge == UINT64_MAX),
		"full signed Date span overflowed"
	);
}



/* 验证共享和私有缓存的显式寿命优先级及保守失效。 */
static void testHttpCacheFreshness(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT("max-age=60, s-maxage=30")
		},
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		},
		{
			XRT_STR_INIT("Expires"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:51:37 GMT")
		}
	};
	static const xhttpfield InvalidPrivate[] = {
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT("s-maxage=20, max-age=x")
		}
	};
	static const xhttpfield Duplicate[] = {
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT("max-age=10, max-age=20")
		}
	};
	static const xhttpfield ExpiresOnly[] = {
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:49:37 GMT")
		},
		{
			XRT_STR_INIT("Expires"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:50:37 GMT")
		}
	};
	static const xhttpfield InvalidExpires[] = {
		{
			XRT_STR_INIT("Expires"),
			XRT_STR_INIT("invalid")
		}
	};
	static const xhttpfield DuplicateExpires[] = {
		{
			XRT_STR_INIT("Expires"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:50:37 GMT")
		},
		{
			XRT_STR_INIT("Expires"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:51:37 GMT")
		}
	};
	static const xhttpfield InvalidDateExpires[] = {
		{
			XRT_STR_INIT("Date"),
			XRT_STR_INIT("invalid")
		},
		{
			XRT_STR_INIT("Expires"),
			XRT_STR_INIT("Sun, 06 Nov 1994 08:50:37 GMT")
		}
	};
	xhttpcachecontrol Control;
	xhttpcachetime Time;
	xhttpcachefreshness Freshness;
	xhttpcachefreshness Before;

	testRequire(
		xrtHttpCacheControlParse(
			Fields, 3, &Control
		) &&
		xrtHttpCacheTimeParse(Fields, 3, &Time) &&
		(xrtHttpCacheFreshness(
			&Control, &Time, Time.Date, true,
			&Freshness
		 ) == XHTTP_CACHE_CALC_READY) &&
		(Freshness.Source ==
		 XHTTP_CACHE_FRESHNESS_S_MAXAGE) &&
		(Freshness.Lifetime ==
		 UINT64_C(30000000)),
		"shared cache s-maxage precedence mismatch"
	);
	testRequire(
		(xrtHttpCacheFreshness(
			&Control, &Time, Time.Date, false,
			&Freshness
		 ) == XHTTP_CACHE_CALC_READY) &&
		(Freshness.Source ==
		 XHTTP_CACHE_FRESHNESS_MAX_AGE) &&
		(Freshness.Lifetime ==
		 UINT64_C(60000000)),
		"private cache max-age precedence mismatch"
	);
	testRequire(
		xrtHttpCacheControlParse(
			InvalidPrivate, 1, &Control
		) &&
		xrtHttpCacheTimeParse(
			InvalidPrivate, 1, &Time
		) &&
		(xrtHttpCacheFreshness(
			&Control, &Time, 0, true,
			&Freshness
		 ) == XHTTP_CACHE_CALC_READY) &&
		(Freshness.Lifetime ==
		 UINT64_C(20000000)),
		"irrelevant invalid max-age polluted s-maxage"
	);
	Before = Freshness;
	testRequire(
		(xrtHttpCacheFreshness(
			&Control, &Time, 0, false,
			&Freshness
		 ) == XHTTP_CACHE_CALC_INVALID) &&
		(memcmp(
			&Freshness, &Before, sizeof(Freshness)
		 ) == 0),
		"selected invalid max-age changed output"
	);
	testRequire(
		xrtHttpCacheControlParse(
			Duplicate, 1, &Control
		) &&
		xrtHttpCacheTimeParse(
			Duplicate, 1, &Time
		) &&
		(xrtHttpCacheFreshness(
			&Control, &Time, 0, false,
			&Freshness
		 ) == XHTTP_CACHE_CALC_INVALID),
		"duplicate max-age was treated as reliable"
	);
	testRequire(
		xrtHttpCacheControlParse(
			ExpiresOnly, 2, &Control
		) &&
		xrtHttpCacheTimeParse(
			ExpiresOnly, 2, &Time
		) &&
		(xrtHttpCacheFreshness(
			&Control, &Time, Time.Date, false,
			&Freshness
		 ) == XHTTP_CACHE_CALC_READY) &&
		(Freshness.Source ==
		 XHTTP_CACHE_FRESHNESS_EXPIRES) &&
		(Freshness.Lifetime ==
		 UINT64_C(60000000)),
		"Expires lifetime mismatch"
	);
	testRequire(
		xrtHttpCacheTimeParse(
			&ExpiresOnly[1], 1, &Time
		) &&
		(xrtHttpCacheFreshness(
			&Control,
			&Time,
			Time.Expires -
				(45 * XRT_TIME_SECOND),
			false,
			&Freshness
		 ) == XHTTP_CACHE_CALC_READY) &&
		(Freshness.Lifetime ==
		 UINT64_C(45000000)),
		"missing Date response-time fallback mismatch"
	);
	testRequire(
		(xrtHttpCacheFreshness(
			&Control,
			&Time,
			Time.Expires + XRT_TIME_SECOND,
			false,
			&Freshness
		 ) == XHTTP_CACHE_CALC_READY) &&
		(Freshness.Lifetime == 0),
		"past Expires did not produce zero lifetime"
	);
	testRequire(
		xrtHttpCacheTimeParse(
			InvalidExpires, 1, &Time
		) &&
		(xrtHttpCacheFreshness(
			&Control, &Time, 0, false,
			&Freshness
		 ) == XHTTP_CACHE_CALC_READY) &&
		(Freshness.Source ==
		 XHTTP_CACHE_FRESHNESS_EXPIRES) &&
		(Freshness.Lifetime == 0),
		"invalid Expires was not treated as expired"
	);
	Before = Freshness;
	testRequire(
		xrtHttpCacheTimeParse(
			DuplicateExpires, 2, &Time
		) &&
		(xrtHttpCacheFreshness(
			&Control, &Time, 0, false,
			&Freshness
		 ) == XHTTP_CACHE_CALC_INVALID) &&
		(memcmp(
			&Freshness, &Before, sizeof(Freshness)
		 ) == 0),
		"duplicate Expires changed output"
	);
	testRequire(
		xrtHttpCacheTimeParse(
			InvalidDateExpires, 2, &Time
		) &&
		(xrtHttpCacheFreshness(
			&Control,
			&Time,
			Time.Expires -
				(45 * XRT_TIME_SECOND),
			false,
			&Freshness
		 ) == XHTTP_CACHE_CALC_READY) &&
		(Freshness.Source ==
		 XHTTP_CACHE_FRESHNESS_EXPIRES) &&
		(Freshness.Lifetime ==
		 UINT64_C(45000000)),
		"invalid Date did not use response time"
	);
	xrtHttpCacheControlInit(&Control);
	xrtHttpCacheTimeInit(&Time);
	Before = Freshness;
	testRequire(
		(xrtHttpCacheFreshness(
			&Control, &Time, 0, false,
			&Freshness
		 ) == XHTTP_CACHE_CALC_NONE) &&
		(memcmp(
			&Freshness, &Before, sizeof(Freshness)
		 ) == 0),
		"missing explicit freshness changed output"
	);
	Time.Expires = INT64_MAX;
	Time.ExpiresCount = 1;
	Time.Flags = XHTTP_CACHE_TIME_EXPIRES;
	testRequire(
		xrtHttpCacheTimeValid(&Time) &&
		(xrtHttpCacheFreshness(
			&Control,
			&Time,
			INT64_MIN,
			false,
			&Freshness
		 ) == XHTTP_CACHE_CALC_READY) &&
		(Freshness.Lifetime == UINT64_MAX),
		"full signed Expires span overflowed"
	);
}



/* 验证公共结构、delta-seconds 和失败原子性边界。 */
static void testHttpCacheTimeEdges(void)
{
	static const xhttpfield InvalidArray[] = {
		{
			{ NULL, 1 },
			XRT_STR_INIT("value")
		}
	};
	xhttpcachetime Time;
	xhttpcachetime Before;
	uint64 iSeconds = 77;

	testRequire(
		xrtHttpCacheDeltaParse(
			XRT_STR_LITERAL(
				" \t999999999999999999999\t "
			),
			&iSeconds
		) &&
		(iSeconds == XHTTP_CACHE_DELTA_MAX),
		"strict delta-seconds saturation mismatch"
	);
	iSeconds = 77;
	testRequire(
		!xrtHttpCacheDeltaParse(
			XRT_STR_LITERAL("1 2"), &iSeconds
		) &&
		(iSeconds == 77),
		"invalid strict delta-seconds changed output"
	);
	xrtClearError();
	xrtHttpCacheTimeInit(&Time);
	testRequire(
		xrtHttpCacheTimeValid(&Time),
		"empty cache time metadata is invalid"
	);
	memset(&Before, 0xA5, sizeof(Before));
	Time = Before;
	testRequire(
		!xrtHttpCacheTimeParse(
			InvalidArray, 1, &Time
		) &&
		(memcmp(&Time, &Before, sizeof(Time)) == 0),
		"invalid field array changed cache time output"
	);
	xrtClearError();
	xrtHttpCacheTimeInit(&Time);
	Time.Flags = UINT32_MAX;
	testRequire(
		!xrtHttpCacheTimeValid(&Time),
		"cache time validator accepted unknown flags"
	);
}



/* 运行 HTTP 缓存时间协议和计算边界。 */
int main(void)
{
	testHttpCacheTimeMetadata();
	testHttpCacheCurrentAge();
	testHttpCacheFreshness();
	testHttpCacheTimeEdges();
	printf("[PASS] http_cache_time\n");
	return 0;
}
