#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_SERVER_FORM_DATA
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证单头 multipart 请求辅助入口。 */
int main(void)
{
	return xrtHttpServerRequestFormData(
		NULL,
		NULL,
		NULL,
		NULL
	) == NULL ? 0 : 1;
}

