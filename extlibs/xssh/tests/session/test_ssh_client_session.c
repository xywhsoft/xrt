#include "../test.h"



/* 验证 session 应用层不会绕过客户端 worker 与 channel 所有权。 */
int main(void)
{
	xsshclientconfig Config;
	xsshclient Client;
	xsshchannel* pChannel = (xsshchannel*)1;

	testRequire(xrtSshClientConfigInit(&Config) &&
		xrtSshClientInit(&Client, &Config, NULL, NULL),
		"ssh session client setup failed");
	testRequire((xrtSshClientSessionOpen(
		&Client,
		&pChannel
	) == XSSH_ERROR_STATE) && (pChannel == NULL) &&
		(xrtSshClientSessionExec(
			&Client,
			NULL,
			XRT_BYTES_LITERAL("true"),
			true,
			7u
		) == XSSH_ERROR_STATE) &&
		!xrtSshClientIsCurrent(&Client) &&
		!xrtSshClientOwnsChannel(&Client, NULL),
		"ssh session client accepted detached operation");
	testRequire(xrtSshClientClear(&Client),
		"ssh session client clear failed");
	return 0;
}
