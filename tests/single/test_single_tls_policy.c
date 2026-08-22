#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>



/* 验证单头文件中的 TLS 策略默认值与校验器。 */
int main(void)
{
	xtlspolicy Policy;

	xrtTlsPolicyInit(&Policy);
	if ( !xrtTlsPolicyValid(&Policy) ||
		(Policy.VersionCount != 2u) ||
		(Policy.GroupCount != 4u) ) {
		return 1;
	}
	printf("[PASS] single-tls-policy\n");
	return 0;
}
