#include <stdio.h>

#include <xrt.h>



/* 用一个调用构造带动态上下文的结构化错误。 */
int main(void)
{
	xrtSetErrorFormat(
		XERR_NOT_FOUND,
		"example.config",
		1,
		"file does not exist: %s",
		"app.json"
	);
	printf("%s\n", xrtErrorMessage(xrtGetError()));
	xrtClearError();
	return 0;
}
