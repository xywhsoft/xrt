#include "../test.h"



/* 验证 forwarding 发起端拒绝未附着客户端。 */
int main(void)
{
	xsshclientconfig Config;
	xsshclient Client;
	xsshchannel* pChannel = (xsshchannel*)1;

	testRequire(xrtSshClientConfigInit(&Config) &&
		xrtSshClientInit(&Client, &Config, NULL, NULL),
		"ssh forward client setup failed");
	testRequire((xrtSshClientDirectTcpipOpen(
		&Client,
		XRT_BYTES_LITERAL("127.0.0.1"),
		80u,
		XRT_BYTES_LITERAL("127.0.0.1"),
		50000u,
		&pChannel
	) == XSSH_ERROR_STATE) && (pChannel == NULL) &&
		(xrtSshClientTcpipForward(
			&Client,
			XRT_BYTES_LITERAL("127.0.0.1"),
			0u,
			9u
		) == XSSH_ERROR_STATE),
		"ssh forward client accepted detached operation");
	testRequire(xrtSshClientClear(&Client),
		"ssh forward client clear failed");
	return 0;
}
