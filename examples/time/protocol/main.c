#include <xrt.h>

#include <stdio.h>



/* 展示 RFC 3339 与 HTTP-date 的专用协议路径。 */
int main(void)
{
	xtime iTime;
	str sRFC3339;
	str sHTTPDate;

	if ( !xrtTimeParseRFC3339(
		XRT_STR_LITERAL("1994-11-06T08:49:37Z"), &iTime) ) {
		return 1;
	}
	sRFC3339 = xrtTimeRFC3339(iTime, 8 * 3600);
	sHTTPDate = xrtTimeHTTPDate(iTime);
	if ( (sRFC3339 == NULL) || (sHTTPDate == NULL) ) {
		xrtFree(sRFC3339);
		xrtFree(sHTTPDate);
		return 1;
	}
	printf("RFC 3339: %s\n", sRFC3339);
	printf("HTTP-date: %s\n", sHTTPDate);
	xrtFree(sRFC3339);
	xrtFree(sHTTPDate);
	return 0;
}
