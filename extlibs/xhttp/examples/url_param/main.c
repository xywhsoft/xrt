#include <stdio.h>
#include <string.h>

#include <xhttp.h>



/* 验证 HTTP 参数解码后的 URI-reference，不创建临时字符串。 */
int main(void)
{
	xhttpparam Param;

	memset(&Param, 0, sizeof(Param));
	Param.Value = XRT_STR_LITERAL("../items?page=2");
	Param.Flags = XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED;
	printf("valid: %s\n", xrtUrlParamValid(&Param) ? "yes" : "no");
	return 0;
}
