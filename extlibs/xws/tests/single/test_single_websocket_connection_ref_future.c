#define XWS_IMPLEMENTATION
#include "../../include/xws/features.h"
#include "../../../xhttp/include/xhttp/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#define XHTTP_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp.h"
#include "../../../../single/extlibs/xws.h"



/* 单头文件必须公开 Ref Future，并在提交失败时保留调用方所有权。 */
static void testRelease(ptr pContext, cbytes pData, size_t iSize)
{
	uint32* pReleases = (uint32*)pContext;

	(void)pData;
	(void)iSize;
	(*pReleases)++;
}



/* 验证空 Connection 的同步拒绝不会调用 Ref 释放过程。 */
int main(void)
{
	static const uint8 Data[] = { 1 };
	uint32 iReleases = 0;
	xnetref Ref = {
		Data,
		sizeof(Data),
		testRelease,
		&iReleases
	};

	if ( (xrtWsConnBinaryRefAsync(NULL, &Ref) != NULL) ||
		(iReleases != 0) ||
		(xrtErrorCode(xrtGetError()) !=
		 XWS_CONN_ERROR_ARGUMENT) ) {
		return 1;
	}
	return 0;
}
