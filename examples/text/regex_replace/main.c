#include <stdio.h>

#include <xrt.h>



/* 演示用命名捕获直接重写配置项。 */
int main(void)
{
	xregex* pRegex = xrtRegexCompile(
		XRT_STR_LITERAL("(?<name>[A-Za-z_]+)=(?<value>\\d+)")
	);
	str sResult;

	if ( pRegex == NULL ) {
		return 1;
	}
	sResult = xrtRegexReplace(
		pRegex,
		XRT_STR_LITERAL("width=128 height=72"),
		XRT_STR_LITERAL("${name}: $2")
	);
	xrtRegexRelease(pRegex);
	if ( sResult == NULL ) {
		return 2;
	}
	puts(sResult);
	xrtFree(sResult);
	return 0;
}
