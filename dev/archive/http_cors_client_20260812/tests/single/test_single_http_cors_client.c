#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证 CORS 客户端模块可单头文件独立使用。 */
int main(void)
{
	xhttpcorspreflightplan Plan;

	return xrtHttpCorsPreflightPlan(
		XRT_STR_LITERAL("GET"), NULL, 0, false, &Plan
	) && (Plan.Flags == 0) ? 0 : 1;
}
