#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件安全随机 FormData 主路径。 */
int main(void)
{
	xformdata* pForm = xrtFormDataCreate(NULL);
	xmultipartboundary Boundary;
	xhttpbody* pBody;

	if ( (pForm == NULL) || !xrtFormDataAppendText(
		pForm, XRT_STR_LITERAL("a"), XRT_STR_LITERAL("b")
	) ) {
		xrtFormDataDestroy(pForm);
		return 1;
	}
	pBody = xrtFormDataBodyRandom(pForm, &Boundary);
	xrtFormDataDestroy(pForm);
	if ( (pBody == NULL) || (Boundary.Size != 45) ) {
		xrtHttpBodyDestroy(pBody);
		return 1;
	}
	xrtHttpBodyDestroy(pBody);
	return 0;
}
