#include <stdio.h>

#include <xrt.h>



/* 展示缺失、空值和一行读取三种环境变量路径。 */
int main(void)
{
	str sValue;

	if ( !xrtEnvSet("XRT_ENVIRONMENT_EXAMPLE", "hello") ) {
		return 1;
	}
	sValue = xrtEnvGet("XRT_ENVIRONMENT_EXAMPLE");
	if ( sValue == NULL ) {
		return 2;
	}
	printf("value=%s\n", sValue);
	xrtFree(sValue);
	return xrtEnvRemove("XRT_ENVIRONMENT_EXAMPLE") ? 0 : 3;
}
