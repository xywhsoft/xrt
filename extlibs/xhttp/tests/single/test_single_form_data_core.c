#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_FORM_DATA
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



#if defined(XHTTP_FEATURE_MULTIPART)
	#error "base FormData unexpectedly selected multipart"
#endif



/* 验证基础 FormData 单头裁剪只保留容器与固定正文能力。 */
int main(void)
{
	xformdata* pForm = xrtFormDataCreate(NULL);
	xformdatapart Part;
	xbytesview Value;

	if ( (pForm == NULL) || !xrtFormDataAppendText(
		pForm, XRT_STR_LITERAL("name"), XRT_STR_LITERAL("value")
	) || !xrtFormDataGet(
		pForm, XRT_STR_LITERAL("name"), &Part
	) || !xrtHttpBodyView(Part.Body, &Value) ||
		(Value.Size != 5u) ||
		(memcmp(Value.Data, "value", 5u) != 0) ) {
		xrtFormDataDestroy(pForm);
		return 1;
	}
	xrtFormDataDestroy(pForm);
	return 0;
}

