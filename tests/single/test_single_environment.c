#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供环境变量基础层和便捷层。 */
int main(void)
{
	str sValue;

	if ( !xrtEnvSet("XRT_SINGLE_ENVIRONMENT", "ready") ) {
		return 1;
	}
	sValue = xrtEnvGet("XRT_SINGLE_ENVIRONMENT");
	if ( (sValue == NULL) || (strcmp(sValue, "ready") != 0) ) {
		xrtFree(sValue);
		return 2;
	}
	xrtFree(sValue);
	return xrtEnvRemove("XRT_SINGLE_ENVIRONMENT") ? 0 : 3;
}
