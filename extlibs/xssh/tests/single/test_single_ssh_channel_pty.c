#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_CHANNEL_PTY
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* PTY 单头闭包只增加 request 与 terminal mode 能力。 */
int main(void)
{
	unsigned char arrModes[8];
	xsshwriter Writer;

	#if !defined(XSSH_FEATURE_CHANNEL_PTY) || \
		!defined(XSSH_FEATURE_CHANNEL_REQUEST) || \
		!defined(XSSH_FEATURE_CHANNEL_MESSAGE)
		#error "SSH channel PTY dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_NETWORK) || defined(XRT_FEATURE_CRYPTO) || \
		defined(XRT_FEATURE_RANDOM_SECURE)
		#error "SSH channel PTY unexpectedly enabled network or crypto"
	#endif

	return xrtSshWriterInit(&Writer, arrModes, sizeof(arrModes)) &&
		(xrtSshTerminalModeWrite(
			&Writer,
			XSSH_TTY_OP_ECHO,
			0u
		) == XSSH_OK) && (xrtSshTerminalModeEnd(&Writer) == XSSH_OK) ? 0 : 1;
}
