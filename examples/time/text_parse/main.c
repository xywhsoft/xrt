/*
 * 范例：time/text_parse —— 分解结构文本族 + 协议日期三格式
 * ----------------------------------------------------------------
 * 演示 API：
 *   【分解结构文本】 xrtDateTimeWrite（缓冲版）/ xrtDateTimeFormat
 *                    （分配版）/ xrtDateTimeParse（严格全格式）
 *   【绝对时间解析】 xrtTimeParse（%z 显式偏移，负偏移还原 UTC）
 *   【HTTP 日期】    xrtTimeWriteHTTPDate（IMF-fixdate）/
 *                    xrtTimeParseHTTPDate（三格式收）/
 *                    xrtTimeTryParseHTTPDate（失败不动输出）
 *   【通用识别】    xrtTimeParseAny（RFC3339 / HTTP / 常见数字格式）
 * 模块宏：XRT_MODULE_TIME_TEXT
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/time/text_parse/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   datetime: write="2024-03-10 12:34:56" format=same roundtrip=ok
 *   time-parse: 2024-03-10T08:34:56-04:00 == unix 1710074096
 *   http-date: "Sun, 10 Mar 2024 12:34:56 GMT" roundtrip=ok
 *   try-parse: reject=1 untouched=1
 *   parse-any: rfc3339=1 http=1 compact=1
 *
 * ParseAny 的数字形态按长度+分隔符特征分流——
 *   14 位全数字按 %Y%m%d%H%M%S 解读。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

/* 基准：2024-03-10T12:34:56Z 的 Unix 秒。 */
#define EXAMPLE_UNIX_S	INT64_C(1710074096)



int main(void)
{
	char Buffer[64];
	str sFormatted;
	xdatetime Parts;
	xtime Moment;
	xtime Parsed;
	size_t iSize;

	if ( !xrtTimeFromUnix(EXAMPLE_UNIX_S, &Moment) ) {
		return 1;
	}

	/* DateTimeWrite：缓冲版返回所需字节数（不含结尾零）。 */
	iSize = xrtDateTimeWrite(Buffer, sizeof(Buffer), &(xdatetime){
		.Year = 2024, .Month = 3, .Day = 10,
		.Hour = 12, .Minute = 34, .Second = 56,
		.Microsecond = 0, .Offset = 0
	}, SV("%Y-%m-%d %H:%M:%S"));
	if ( (iSize != 19u) ||
		 (strcmp(Buffer, "2024-03-10 12:34:56") != 0) ) {
		return 2;
	}

	/* DateTimeFormat：分配版，返回值由 xrtFree 释放。 */
	sFormatted = xrtDateTimeFormat(&(xdatetime){
		.Year = 2024, .Month = 3, .Day = 10,
		.Hour = 12, .Minute = 34, .Second = 56,
		.Microsecond = 0, .Offset = 0
	}, SV("%Y-%m-%d %H:%M:%S"));
	printf("datetime: write=\"%s\" format=%s ",
		Buffer, sFormatted ? "same" : "(null)");
	if ( (sFormatted == NULL) ||
		 (strcmp(sFormatted, Buffer) != 0) ) {
		return 3;
	}
	xrtFree(sFormatted);

	/* DateTimeParse：严格全格式 → TimeMake 还原绝对时间。 */
	if ( !xrtDateTimeParse(SV("2024-03-10 12:34:56"),
		SV("%Y-%m-%d %H:%M:%S"), &Parts) ||
		 !xrtTimeMake(&Parts, &Parsed) || (Parsed != Moment) ) {
		return 4;
	}
	printf("roundtrip=ok\n");

	/* TimeParse 偏移：%z 收 ±HHMM，%:z 收 ±HH:MM——都还原同一 UTC 时刻。 */
	if ( !xrtTimeParse(SV("2024-03-10T08:34:56-0400"),
		SV("%Y-%m-%dT%H:%M:%S%z"), &Parsed) || (Parsed != Moment) ) {
		return 5;
	}
	if ( !xrtTimeParse(SV("2024-03-10T08:34:56-04:00"),
		SV("%Y-%m-%dT%H:%M:%S%:z"), &Parsed) || (Parsed != Moment) ) {
		return 5;
	}
	printf("time-parse: 2024-03-10T08:34:56-04:00 == unix %lld\n",
		(long long)xrtTimeUnix(Parsed));

	/* WriteHTTPDate：IMF-fixdate，秒以下丢弃，恒为 GMT。 */
	iSize = xrtTimeWriteHTTPDate(Buffer, sizeof(Buffer), Moment);
	if ( (iSize != 29u) ||
		 (strcmp(Buffer, "Sun, 10 Mar 2024 12:34:56 GMT") != 0) ) {
		return 6;
	}
	printf("http-date: \"%s\" ", Buffer);

	/* ParseHTTPDate：IMF-fixdate / RFC 850 / asctime 三格式都能收。 */
	if ( !xrtTimeParseHTTPDate(SV("Sun, 10 Mar 2024 12:34:56 GMT"),
		&Parsed) || (Parsed != Moment) ||
		 !xrtTimeParseHTTPDate(SV("Sunday, 10-Mar-24 12:34:56 GMT"),
			&Parsed) || (Parsed != Moment) ||
		 !xrtTimeParseHTTPDate(SV("Sun Mar 10 12:34:56 2024"),
			&Parsed) || (Parsed != Moment) ) {
		return 7;
	}
	printf("roundtrip=ok\n");

	/* TryParseHTTPDate：非法文本返回假且不碰输出、不设错误。 */
	Parsed = Moment;  /* 哨兵：失败时必须原样保留 */
	if ( xrtTimeTryParseHTTPDate(SV("not a date at all"), &Parsed) ||
		 (Parsed != Moment) ) {
		return 8;
	}
	printf("try-parse: reject=1 untouched=1\n");

	/* ParseAny：RFC3339 / HTTP-date / 14 位紧凑数字三种形态。 */
	if ( !xrtTimeParseAny(SV("2024-03-10T12:34:56Z"), &Parsed) ||
		 (Parsed != Moment) ) {
		return 9;
	}
	printf("parse-any: rfc3339=%d",
		xrtTimeParseAny(SV("2024-03-10T12:34:56Z"), &Parsed) ? 1 : 0);
	printf(" http=%d",
		xrtTimeParseAny(SV("Sun, 10 Mar 2024 12:34:56 GMT"),
			&Parsed) ? 1 : 0);
	if ( !xrtTimeParseAny(SV("Sun, 10 Mar 2024 12:34:56 GMT"),
		&Parsed) || (Parsed != Moment) ) {
		return 10;
	}
	printf(" compact=%d\n",
		xrtTimeParseAny(SV("20240310123456"), &Parsed) ? 1 : 0);
	if ( !xrtTimeParseAny(SV("20240310123456"), &Parsed) ||
		 (Parsed != Moment) ) {
		return 11;
	}
	return 0;
}
