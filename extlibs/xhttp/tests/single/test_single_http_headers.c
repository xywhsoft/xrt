#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_HEADERS
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证拥有型 Header 容器的裁剪闭包与长值无隐藏固定上限。 */
int main(void)
{
	xhttpheaders* pHeaders = xrtHttpHeadersCreate(NULL);
	char Value[12000];
	const xhttpfield* pField;
	int iResult = 0;

	if ( pHeaders == NULL ) {
		return 1;
	}
	memset(Value, 'v', sizeof(Value));
	if ( !xrtHttpHeadersAdd(
		pHeaders,
		XRT_STR_LITERAL("X-Long"),
		(xstrview){ Value, sizeof(Value) }
	) ) {
		iResult = 2;
	}
	pField = xrtHttpHeadersGet(
		pHeaders, XRT_STR_LITERAL("x-long")
	);
	if ( (pField == NULL) || (pField->Value.Size != sizeof(Value)) ) {
		iResult = 3;
	}
	xrtHttpHeadersDestroy(pHeaders);
	return iResult;
}
