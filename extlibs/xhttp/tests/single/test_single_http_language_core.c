#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_LANGUAGE_CORE
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证内部语言标签校验可以形成最小、无分配的单头闭包。 */
int main(void)
{
	return __xrtHttpLanguageTextValid(
		XRT_STR_LITERAL("en-US"), false, false, NULL
	) ? 0 : 1;
}
