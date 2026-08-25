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
	pChannel = (xsshchannel*)1;
	testRequire((xrtSshClientDirectTcpipOpen(
		&Client,
		XRT_BYTES_LITERAL("127.0.0.1"),
		65536u,
		XRT_BYTES_LITERAL("127.0.0.1"),
		50000u,
		&pChannel
	) == XSSH_ERROR_ARGUMENT) && (pChannel == NULL) &&
		(xrtSshClientDirectTcpipOpen(
			&Client,
			XRT_BYTES_LITERAL("127.0.0.1"),
			80u,
			XRT_BYTES_LITERAL("127.0.0.1"),
			UINT32_MAX,
			&pChannel
		) == XSSH_ERROR_ARGUMENT) && (pChannel == NULL) &&
		(xrtSshClientTcpipForward(
			&Client,
			XRT_BYTES_LITERAL("127.0.0.1"),
			65536u,
			9u
		) == XSSH_ERROR_ARGUMENT) &&
		(xrtSshClientTcpipForwardCancel(
			&Client,
			XRT_BYTES_LITERAL("127.0.0.1"),
			UINT32_MAX,
			10u
		) == XSSH_ERROR_ARGUMENT),
		"ssh forward client accepted out-of-range TCP port");
	testRequire(xrtSshClientClear(&Client),
		"ssh forward client clear failed");
	return 0;
}
