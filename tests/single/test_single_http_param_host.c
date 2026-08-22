#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留参数语义 Host 的无分配验证。 */
int main(void)
{
	static const xhttpparam Param = {
		XRT_STR_INIT("host"),
		XRT_STR_INIT("exa\\mple.com:443"),
		XHTTP_PARAM_HAS_VALUE | XHTTP_PARAM_QUOTED
	};

	return xrtHttpParamHostValid(&Param) ? 0 : 1;
}
