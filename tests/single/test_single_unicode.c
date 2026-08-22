#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供严格 Unicode 转换。 */
int main(void)
{
	uint16* pText = xrtUtf8To16("A", NULL);
	uint16* pCopy;

	if ( (pText == NULL) || (pText[0] != (uint16)'A') || (pText[1] != 0) ) {
		return 1;
	}
	pCopy = xrtUtf16Dup(pText);
	if ( (pCopy == NULL) || (pCopy[0] != (uint16)'A') || (pCopy[1] != 0) ) {
		xrtFree(pText);
		xrtFree(pCopy);
		return 2;
	}
	xrtFree(pCopy);
	xrtFree(pText);
	return 0;
}
