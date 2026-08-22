#include "../test.h"



/* 验证 PTY 应用层拒绝未附着客户端。 */
int main(void)
{
	xsshclientconfig Config;
	xsshclient Client;

	testRequire(xrtSshClientConfigInit(&Config) &&
		xrtSshClientInit(&Client, &Config, NULL, NULL),
		"ssh PTY client setup failed");
	testRequire(xrtSshClientSessionPty(
		&Client,
		NULL,
		XRT_BYTES_LITERAL("xterm-256color"),
		80u,
		24u,
		0u,
		0u,
		XRT_BYTES_LITERAL("\0"),
		true,
		1u
	) == XSSH_ERROR_STATE, "ssh PTY accepted detached operation");
	testRequire(xrtSshClientClear(&Client), "ssh PTY clear failed");
	return 0;
}
