#define XREGEX_MODULE_REGEX_MATCH
#include <xregex.h>



/* 验证发布包只安装公共头时，最小正则消费者仍可直接链接。 */
int main(void)
{
	xregexresult Result = xrtRegexFullMatch(
		XRT_STR_LITERAL("^[a-z]+$"),
		XRT_STR_LITERAL("xlang")
	);

	return (Result == XREGEX_MATCH) ? 0 : 1;
}
