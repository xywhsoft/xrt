#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <string.h>



/* 单头文件必须独立提供安全随机文本层。 */
int main(void)
{
	str sToken = xrtSecureString(32);

	if ( (sToken == NULL) || (strlen(sToken) != 32) ) {
		xrtFree(sToken);
		return 1;
	}
	xrtSecureZero(sToken, 33);
	xrtFree(sToken);
	return 0;
}
