#define XSSH_MODULE_SSH_WIRE
#include <xssh.h>



/* 验证发布包只安装公共头时仍可使用 SSH wire 原语。 */
int main(void)
{
	xstrview Banner;
	size_t iConsumed;

	return xrtSshBannerRead(
		XRT_STR_LITERAL("SSH-2.0-package\r\n"),
		&Banner,
		&iConsumed
	) == XSSH_OK ? 0 : 1;
}
