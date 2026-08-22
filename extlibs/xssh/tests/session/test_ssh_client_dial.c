#include "../test.h"



/* 验证 Dial 薄适配不接受无效客户端，也不改变未附着客户端生命周期。 */
int main(void)
{
	xsshclientconfig Config;
	xsshclient Client;

	testRequire(xrtSshClientDial(
		NULL,
		NULL,
		NULL,
		NULL,
		0u,
		NULL,
		NULL,
		NULL
	) == NULL, "ssh client dial accepted an invalid client");
	testRequire(xrtSshClientConfigInit(&Config) &&
		xrtSshClientInit(&Client, &Config, NULL, NULL),
		"ssh client dial fixture init failed");
	testRequire(xrtSshClientDial(
		&Client,
		NULL,
		NULL,
		"127.0.0.1",
		22u,
		NULL,
		NULL,
		NULL
	) == NULL, "ssh client dial hid invalid XRT dial arguments");
	testRequire((xrtSshClientState(&Client) == XSSH_CLIENT_CREATED) &&
		xrtSshClientClear(&Client),
		"ssh client dial changed the client after submit rejection");
	return 0;
}
