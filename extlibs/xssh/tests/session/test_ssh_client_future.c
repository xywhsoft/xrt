#include "../test.h"



/* Future 创建必须拒绝无效参数和非 Worker 调用，不改变客户端状态。 */
int main(void)
{
	xsshclientconfig Config;
	xsshclient Client;

	testRequire(xrtSshClientConfigInit(&Config) &&
		xrtSshClientInit(&Client, &Config, NULL, NULL),
		"ssh client Future fixture init failed");
	testRequire((xrtSshClientWaitAsync(
		&Client,
		XSSH_CLIENT_WAIT_READY
	) == NULL) &&
		(xrtSshClientChannelWaitAsync(
			&Client,
			NULL,
			XSSH_CLIENT_CHANNEL_WAIT_OPEN
		) == NULL) &&
		(xrtSshClientState(&Client) == XSSH_CLIENT_CREATED),
		"ssh client Future accepted an invalid execution context");
	testRequire(xrtSshClientClear(&Client),
		"ssh client Future fixture clear failed");
	return 0;
}
