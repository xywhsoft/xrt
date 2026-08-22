#include "../test.h"

#include <xrt/http_cache_time.h>



/* 验证 Age 对全部单字节输入保持严格数值语义。 */
static void testHttpCacheTimeAgeBytes(void)
{
	xhttpfield Field;
	xhttpcachetime Time;
	unsigned int i;

	Field.Name = XRT_STR_LITERAL("Age");
	Field.Value.Data = NULL;
	Field.Value.Size = 1;
	for ( i = 0; i <= UINT8_MAX; i++ ) {
		char iByte = (char)(uint8)i;
		bool bDigit = (i >= (unsigned int)'0') &&
			(i <= (unsigned int)'9');

		Field.Value.Data = &iByte;
		testRequire(
			xrtHttpCacheTimeParse(
				&Field, 1, &Time
			) &&
			xrtHttpCacheTimeValid(&Time) &&
			(Time.AgeCount == 1) &&
			(((Time.Flags &
			   XHTTP_CACHE_TIME_AGE_INVALID) == 0) ==
			 bDigit) &&
			(!bDigit ||
			 (Time.Age == (uint64)(
				i - (unsigned int)'0'
			 ))),
			"Age single-byte mutation mismatch"
		);
	}
}



/* 验证日期字段对全部单字节输入只记录非法事实。 */
static void testHttpCacheTimeDateBytes(void)
{
	static const xstrview Names[] = {
		XRT_STR_INIT("Date"),
		XRT_STR_INIT("Expires")
	};
	size_t iName;
	unsigned int i;

	for ( iName = 0;
		iName < (sizeof(Names) / sizeof(Names[0]));
		iName++ ) {
		for ( i = 0; i <= UINT8_MAX; i++ ) {
			char iByte = (char)(uint8)i;
			xhttpfield Field;
			xhttpcachetime Time;
			uint32 iInvalid;

			Field.Name = Names[iName];
			Field.Value.Data = &iByte;
			Field.Value.Size = 1;
			iInvalid = iName == 0 ?
				XHTTP_CACHE_TIME_DATE_INVALID :
				XHTTP_CACHE_TIME_EXPIRES_INVALID;
			testRequire(
				xrtHttpCacheTimeParse(
					&Field, 1, &Time
				) &&
				xrtHttpCacheTimeValid(&Time) &&
				((Time.Flags & iInvalid) != 0),
				"HTTP date single-byte mutation mismatch"
			);
		}
	}
}



/* 验证被丢弃的 Age 后续成员不污染第一个成员。 */
static void testHttpCacheTimeDiscardedAge(void)
{
	static const char Value[] = {
		'7', ',', '\0', ',', 'x'
	};
	xhttpfield Field;
	xhttpcachetime Time;

	Field.Name = XRT_STR_LITERAL("Age");
	Field.Value.Data = Value;
	Field.Value.Size = sizeof(Value);
	testRequire(
		xrtHttpCacheTimeParse(
			&Field, 1, &Time
		) &&
		xrtHttpCacheTimeValid(&Time) &&
		(Time.Age == 7) &&
		(Time.AgeMemberCount == 3) &&
		((Time.Flags &
		  XHTTP_CACHE_TIME_AGE_EXTRA) != 0) &&
		((Time.Flags &
		  XHTTP_CACHE_TIME_AGE_INVALID) == 0),
		"discarded Age members polluted first value"
	);
}



/* 运行缓存时间字段的字节级变异边界。 */
int main(void)
{
	testHttpCacheTimeAgeBytes();
	testHttpCacheTimeDateBytes();
	testHttpCacheTimeDiscardedAge();
	printf("[PASS] http_cache_time_mutation\n");
	return 0;
}
