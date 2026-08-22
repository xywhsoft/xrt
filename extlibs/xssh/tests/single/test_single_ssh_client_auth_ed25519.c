#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_CLIENT_AUTH_ED25519
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 单头裁剪闭包必须只启用 Ed25519 客户端认证所需的协议与密码依赖。 */
int main(void)
{
	xsshclientauth Auth;
	xsshed25519identity Identity;
	unsigned char arrOutput[256];
	xsshwriter Writer;

	#if !defined(XSSH_FEATURE_CLIENT_AUTH_ED25519) || \
		!defined(XSSH_FEATURE_AUTH_PUBLICKEY) || \
		!defined(XSSH_FEATURE_PRIVATE_KEY_ED25519)
		#error "SSH client Ed25519 auth dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_FILE)
		#error "SSH client Ed25519 auth unexpectedly enabled file support"
	#endif

	memset(&Auth, 0, sizeof(Auth));
	memset(&Identity, 0, sizeof(Identity));
	Auth.User = XRT_STR_LITERAL("alice");
	Auth.Methods = XRT_STR_LITERAL("password");
	Auth.SessionId = XRT_BYTES_LITERAL("id");
	if ( !xrtSshWriterInit(&Writer, arrOutput, sizeof(arrOutput)) ) {
		return 1;
	}
	return xrtSshClientEd25519Auth(
		NULL,
		&Writer,
		&Auth,
		&Identity
	) == XSSH_ERROR_AUTHENTICATION ? 0 : 1;
}
