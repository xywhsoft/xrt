#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件保留 request-target 形式与 authority 选择。 */
int main(void)
{
	xhttptarget Target;
	xhttpauthority Authority;

	return xrtHttpTargetParse(
		XRT_STR_LITERAL("CONNECT"),
		XRT_STR_LITERAL("example.test:443"),
		&Target
	) && (Target.Form == XHTTP_TARGET_AUTHORITY) &&
		xrtHttpTargetAuthority(
			&Target,
			XRT_STR_LITERAL("ignored.test"),
			&Authority
		) && (Authority.Port == 443) ? 0 : 1;
}
