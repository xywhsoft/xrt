#include "../test.h"



/* 记录一次公共调用触发的错误通知数量。 */
static void testTimeErrorHandler(const xerror* pError, ptr pUserData)
{
	int* pCount = (int*)pUserData;

	(void)pError;
	(*pCount)++;
}



/* 自定义格式必须支持计长、有界写入和超过旧版固定上限的长格式。 */
static void testTimeFormatBuffers(void)
{
	char arrAlias[32];
	char arrSmall[8];
	char arrFormat[300];
	xdatetime tDateTime;
	xstrview tFormat;
	xtime iTime;
	size_t iSize;
	str sText;

	testRequire(xrtDateTime(2024, 2, 29, 23, 58, 57, 654321, &iTime),
		"format source construction failed");
	iSize = xrtTimeWrite(NULL, 0, iTime, 8 * 3600,
		XRT_STR_LITERAL("%F %T.%f %:z"));
	testRequire(iSize == 33, "format size query is wrong");
	testRequire(xrtTimeWrite(arrSmall, sizeof(arrSmall), iTime, 8 * 3600,
		XRT_STR_LITERAL("%F %T.%f %:z")) == iSize,
		"bounded format returned the wrong size");
	testRequire((arrSmall[sizeof(arrSmall) - 1] == '\0') &&
		(strcmp(arrSmall, "2024-03") == 0), "bounded format was not terminated");
	sText = xrtTimeFormat(iTime, 0,
		XRT_STR_LITERAL("%Y/%-m/%-d %-H:%-M:%-S %P"));
	testRequire((sText != NULL) &&
		(strcmp(sText, "2024/2/29 23:58:57 pm") == 0),
		"no-padding or lowercase AM/PM format failed");
	xrtFree(sText);

	for ( size_t i = 0; i < 100; i++ ) {
		arrFormat[(i * 3)] = '%';
		arrFormat[(i * 3) + 1] = 'Y';
		arrFormat[(i * 3) + 2] = '|';
	}
	tFormat.Data = arrFormat;
	tFormat.Size = sizeof(arrFormat);
	sText = xrtTimeFormat(iTime, 0, tFormat);
	testRequire((sText != NULL) && (strlen(sText) == 500),
		"long format retained a legacy token or output limit");
	xrtFree(sText);

	testRequire(xrtTimeSplitAt(iTime, 1, &tDateTime),
		"second offset split failed");
	arrSmall[0] = 'x';
	testRequire(xrtDateTimeWrite(arrSmall, sizeof(arrSmall), &tDateTime,
		XRT_STR_LITERAL("%z")) == XRT_NPOS,
		"lossy second offset format was accepted");
	testRequire(arrSmall[0] == '\0', "failed format left partial output");

	arrSmall[0] = 'x';
	testRequire(xrtTimeWrite(arrSmall, sizeof(arrSmall), iTime, 0,
		XRT_STR_LITERAL("%:Y")) == XRT_NPOS,
		"invalid colon modifier was accepted");
	testRequire(arrSmall[0] == '\0', "invalid format left partial output");

	arrSmall[0] = 'x';
	testRequire(xrtTimeWrite(arrSmall, sizeof(arrSmall), iTime, 0,
		XRT_STR_LITERAL("%-p")) == XRT_NPOS,
		"invalid no-padding modifier was accepted");
	testRequire(arrSmall[0] == '\0',
		"invalid no-padding format left partial output");

	memcpy(arrAlias, "%F", 3);
	testRequire(xrtTimeWrite(arrAlias, sizeof(arrAlias), iTime, 0,
		(xstrview){ arrAlias, 2 }) == XRT_NPOS,
		"aliased time format and output were accepted");
	testRequire(strcmp(arrAlias, "%F") == 0,
		"alias rejection modified the format buffer");

	memcpy(arrAlias, "%Y", 3);
	testRequire(xrtDateTimeWrite(arrAlias, sizeof(arrAlias), &tDateTime,
		(xstrview){ arrAlias, 2 }) == XRT_NPOS,
		"aliased date-time format and output were accepted");
	testRequire(strcmp(arrAlias, "%Y") == 0,
		"date-time alias rejection modified the format buffer");
}



/* 自定义解析必须完整消费输入，并验证派生日期字段。 */
static void testTimeCustomParse(void)
{
	xdatetime tDateTime;
	xtime iTime = 17;
	xtime iExpected;
	xtime iExtended;
	xtime iParsedExtended;
	int iHandlerCount = 0;
	str sExtended;

	testRequire(xrtTimeParse(XRT_STR_LITERAL("2024-02-29 11:58:57 PM +08:30"),
		XRT_STR_LITERAL("%F %I:%M:%S %p %:z"), &iTime),
		"custom time parse failed");
	testRequire(xrtDateTime(2024, 2, 29, 15, 28, 57, 0, &iExpected) &&
		(iTime == iExpected), "12-hour or offset parse is wrong");
	testRequire(xrtTimeParse(XRT_STR_LITERAL("2024/2/9 3:4:5 pm"),
		XRT_STR_LITERAL("%Y/%-m/%-d %-I:%-M:%-S %P"), &iTime),
		"no-padding time parse failed");
	testRequire(xrtDateTime(2024, 2, 9, 15, 4, 5, 0, &iExpected) &&
		(iTime == iExpected), "no-padding time parse value is wrong");

	testRequire(xrtDateTimeParse(XRT_STR_LITERAL("Thu 2024-060 Q1"),
		XRT_STR_LITERAL("%a %Y-%j Q%q"), &tDateTime),
		"derived date parse failed");
	testRequire((tDateTime.Month == 2) && (tDateTime.Day == 29) &&
		(tDateTime.Weekday == XTIME_THURSDAY), "derived fields are wrong");

	testRequire(xrtDate(-12345, 6, 7, &iExtended),
		"extended-year source construction failed");
	sExtended = xrtTimeFormat(
		iExtended, 0, XRT_STR_LITERAL("%F"));
	testRequire((sExtended != NULL) &&
		(strcmp(sExtended, "-12345-06-07") == 0),
		"extended-year %F format failed");
	testRequire(xrtTimeParse(
		(xstrview){ sExtended, strlen(sExtended) },
		XRT_STR_LITERAL("%F"), &iParsedExtended) &&
		(iParsedExtended == iExtended),
		"extended-year %F did not roundtrip");
	xrtFree(sExtended);

	xrtSetErrorHandler(testTimeErrorHandler, &iHandlerCount);
	testRequire(!xrtTimeParse(XRT_STR_LITERAL("2024-02-30"),
		XRT_STR_LITERAL("%F"), &iTime), "invalid complete input was accepted");
	xrtSetErrorHandler(NULL, NULL);
	testRequire((iTime == iExpected) && (iHandlerCount == 1),
		"parse failure modified output or notified more than once");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XTIME_ERROR_PARSE),
		"custom parse reported the wrong final error");

	iHandlerCount = 0;
	xrtSetErrorHandler(testTimeErrorHandler, &iHandlerCount);
	testRequire(!xrtTimeParse(XRT_STR_LITERAL("2024"),
		XRT_STR_LITERAL("%:Y"), &iTime),
		"malformed parse format was accepted");
	xrtSetErrorHandler(NULL, NULL);
	testRequire((iTime == iExpected) && (iHandlerCount == 1),
		"malformed format modified output or notified more than once");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XTIME_ERROR_FORMAT),
		"malformed parse format reported a text mismatch");
}



/* RFC 3339 必须无损处理偏移和微秒，并严格拒绝协议外字段。 */
static void testRFC3339(void)
{
	char arrText[64];
	xtime iTime;
	xtime iExpected;
	str sText;

	testRequire(xrtTimeParseRFC3339(
		XRT_STR_LITERAL("1994-11-06T08:49:37Z"), &iTime),
		"RFC 3339 known vector failed");
	testRequire(xrtDateTime(1994, 11, 6, 8, 49, 37, 0, &iExpected) &&
		(iTime == iExpected), "RFC 3339 UTC value is wrong");

	testRequire(xrtTimeParseRFC3339(
		XRT_STR_LITERAL("2024-01-02T03:04:05.123456789+08:30"), &iTime),
		"RFC 3339 fractional offset parse failed");
	testRequire(xrtDateTime(2024, 1, 1, 18, 34, 5, 123456, &iExpected) &&
		(iTime == iExpected), "RFC 3339 fraction truncation is wrong");
	sText = xrtTimeRFC3339(iTime, 8 * 3600 + 30 * 60);
	testRequire((sText != NULL) &&
		(strcmp(sText, "2024-01-02T03:04:05.123456+08:30") == 0),
		"RFC 3339 canonical format is wrong");
	xrtFree(sText);

	iTime = 71;
	testRequire(!xrtTimeParseRFC3339(
		XRT_STR_LITERAL("2016-12-31T23:59:60Z"), &iTime) && (iTime == 71),
		"unsupported leap second was accepted");
	testRequire(xrtTimeWriteRFC3339(arrText, sizeof(arrText), 0, 1) == XRT_NPOS,
		"RFC 3339 silently discarded offset seconds");
}



/* HTTP-date 必须覆盖规范格式和两种历史兼容格式，并校验星期。 */
static void testHTTPDate(void)
{
	const xtime iKnown = INT64_C(784111777) * XRT_TIME_SECOND;
	xtime iTime = 0;
	str sText = xrtTimeHTTPDate(iKnown + 654321);

	testRequire((sText != NULL) &&
		(strcmp(sText, "Sun, 06 Nov 1994 08:49:37 GMT") == 0),
		"HTTP-date canonical format is wrong");
	xrtFree(sText);
	testRequire(xrtTimeParseHTTPDate(
		XRT_STR_LITERAL("Sun, 06 Nov 1994 08:49:37 GMT"), &iTime) &&
		(iTime == iKnown), "IMF-fixdate parse failed");
	testRequire(xrtTimeParseHTTPDate(
		XRT_STR_LITERAL("Sunday, 06-Nov-94 08:49:37 GMT"), &iTime) &&
		(iTime == iKnown), "RFC 850 date parse failed");
	testRequire(xrtTimeParseHTTPDate(
		XRT_STR_LITERAL("Sun Nov  6 08:49:37 1994"), &iTime) &&
		(iTime == iKnown), "asctime date parse failed");
	testRequire(xrtTimeTryParseHTTPDate(
		XRT_STR_LITERAL("Sun, 06 Nov 1994 08:49:37 GMT"), &iTime) &&
		(iTime == iKnown), "HTTP-date try-parse failed");

	iTime = 23;
	xrtClearError();
	testRequire(!xrtTimeTryParseHTTPDate(
		XRT_STR_LITERAL("invalid"), &iTime
	) && (iTime == 23) && (xrtGetError() == NULL),
		"HTTP-date try-parse changed output or thread error");
	testRequire(!xrtTimeParseHTTPDate(
		XRT_STR_LITERAL("Mon, 06 Nov 1994 08:49:37 GMT"), &iTime) &&
		(iTime == 23), "HTTP-date weekday mismatch was accepted");
	testRequire(!xrtTimeParseHTTPDate(
		XRT_STR_LITERAL("sun, 06 Nov 1994 08:49:37 GMT"), &iTime) &&
		(iTime == 23), "HTTP-date accepted a lowercase weekday");
	testRequire(!xrtTimeParseHTTPDate(
		XRT_STR_LITERAL("Sun, 06 nov 1994 08:49:37 GMT"), &iTime) &&
		(iTime == 23), "HTTP-date accepted a lowercase month");
	testRequire(!xrtTimeParseHTTPDate(
		XRT_STR_LITERAL("Sun, 06 Nov 1994 08:49:37 gmt"), &iTime) &&
		(iTime == 23), "HTTP-date accepted a lowercase GMT literal");
}



/* 便捷解析只按明确形状分派，不接受前缀或后缀。 */
static void testTimeParseAnyShapes(void)
{
	xtime iTime;
	xtime iExpected;

	testRequire(xrtDateTime(2024, 2, 29, 23, 58, 57, 0, &iExpected),
		"parse-any expected value failed");
	testRequire(xrtTimeParseAny(XRT_STR_LITERAL("20240229235857"), &iTime) &&
		(iTime == iExpected), "compact parse-any failed");
	testRequire(xrtTimeParseAny(XRT_STR_LITERAL("2024/02/29 23:58:57"), &iTime) &&
		(iTime == iExpected), "slash parse-any failed");
	testRequire(xrtTimeParseAny(XRT_STR_LITERAL("2024.02.29 23:58:57"), &iTime) &&
		(iTime == iExpected), "dot parse-any failed");
	testRequire(xrtTimeParseAny(XRT_STR_LITERAL("20240229 235857"), &iTime) &&
		(iTime == iExpected), "split compact parse-any failed");
	testRequire(!xrtTimeParseAny(XRT_STR_LITERAL("2024-02-29 trailing"), &iTime),
		"parse-any accepted an unsupported suffix");
}



/* 执行格式化、严格解析与协议日期测试。 */
int main(void)
{
	testTimeFormatBuffers();
	testTimeCustomParse();
	testRFC3339();
	testHTTPDate();
	testTimeParseAnyShapes();
	return 0;
}
