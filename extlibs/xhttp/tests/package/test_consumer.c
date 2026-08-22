#define XHTTP_MODULE_URL
#include <xhttp.h>



/* 验证静态库和动态库消费者可以直接使用 xhttp 的完整 URL 能力。 */
int main(void)
{
	xurl Url;

	return xrtUrlParse(
		XRT_STR_LITERAL("https://example.com/a?q=1"),
		&Url
	) ? 0 : 1;
}
