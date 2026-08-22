#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件发布包含请求 Trailer 的拥有型容器 API。 */
int main(void)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("https://example.test/upload")
	);
	xhttprequest* pClone;
	const xhttpfield* pField;
	bool bPass;

	if ( (pRequest == NULL) ||
		!xrtHttpRequestAddTrailer(
			pRequest,
			XRT_STR_LITERAL("Digest"),
			XRT_STR_LITERAL("value")
		) ) {
		xrtHttpRequestDestroy(pRequest);
		return 1;
	}
	pClone = xrtHttpRequestClone(pRequest);
	xrtHttpRequestDestroy(pRequest);
	pField = xrtHttpRequestTrailer(
		pClone, XRT_STR_LITERAL("Digest")
	);
	bPass = (pField != NULL) &&
		(pField->Value.Size == 5u) &&
		(memcmp(pField->Value.Data, "value", 5u) == 0);
	xrtHttpRequestDestroy(pClone);
	return bPass ? 0 : 2;
}
