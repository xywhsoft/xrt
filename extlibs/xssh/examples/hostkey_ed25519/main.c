#include <xssh.h>



/* 展示验证入口；生产代码应先执行独立的主机身份信任策略。 */
int main(void)
{
	return xrtSshEd25519HostKeyVerify(
		XRT_BYTES_LITERAL("invalid-key"),
		XRT_BYTES_LITERAL("invalid-signature"),
		XRT_BYTES_LITERAL("message")
	) == XSSH_OK ? 1 : 0;
}
