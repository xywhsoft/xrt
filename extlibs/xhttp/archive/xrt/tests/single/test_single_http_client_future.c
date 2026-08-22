#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件发布 Future 结果 API 与同步参数错误。 */
int main(void)
{
	xhttpresult* pResult;
	xhttpcallinfo Info;

	xrtClearError();
	pResult = xrtHttpClientDoSync(NULL, NULL, NULL);
	if ( (pResult != NULL) ||
		(xrtGetError() == NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 1;
	}
	xrtClearError();
	if ( (xrtHttpClientDoAsync(NULL, NULL, NULL) != NULL) ||
		(xrtGetError() == NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 2;
	}
	xrtClearError();
	if ( (xrtHttpClientWaitAsync(NULL) != NULL) ||
		(xrtGetError() == NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 3;
	}
	xrtClearError();
	if ( (xrtHttpResultRef(NULL) != NULL) ||
		(xrtGetError() == NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 4;
	}
	xrtClearError();
	if ( xrtHttpResultInfo(NULL, &Info) ||
		(xrtGetError() == NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 5;
	}
	return 0;
}
