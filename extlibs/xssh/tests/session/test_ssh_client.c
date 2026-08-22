#include "../test.h"



/* 验证组合客户端默认值、空对象成本和未附着生命周期。 */
int main(void)
{
	xsshclientconfig Config;
	xsshclient Client;
	xsshreplyqueue* pReplies;
	uint64 iToken;
	size_t i;

	testRequire(xrtSshClientConfigInit(&Config) &&
		(Config.Core.Kex.Role == XSSH_ROLE_CLIENT) &&
		(Config.Channels.MaxChannels == XSSH_CHANNELS_MAX_DEFAULT) &&
		(Config.ReadyTimeout == XSSH_CLIENT_READY_TIMEOUT_DEFAULT) &&
		(Config.ControlInitial == XSSH_CLIENT_CONTROL_INITIAL_DEFAULT) &&
		(Config.ControlLimit == XSSH_CLIENT_CONTROL_LIMIT_DEFAULT) &&
		(Config.GlobalReplyLimit ==
		 XSSH_CLIENT_GLOBAL_REPLY_LIMIT_DEFAULT),
		"ssh client defaults failed");
	testRequire(xrtSshClientInit(&Client, &Config, NULL, NULL) &&
		(xrtSshClientState(&Client) == XSSH_CLIENT_CREATED) &&
		(xrtSshClientStream(&Client) != NULL) &&
		(xrtSshClientSession(&Client) == NULL) &&
		(xrtSshClientReader(&Client) == NULL) &&
		(xrtSshClientChannels(&Client) == NULL) &&
		(xrtSshClientGlobalReplies(&Client) == &Client.GlobalReplies) &&
		(xrtSshClientGlobalReplyReserve(&Client, 5u) == XSSH_OK) &&
		(Client.GlobalReplyCapacity >= 5u) &&
		(xrtSshClientNetEvents() == xrtSshSessionStreamNetEvents()) &&
		(xrtSshClientNetData(&Client) == &Client.Stream) &&
		(xrtSshClientGlobalReplyReserve(&Client, 4096u) == XSSH_ERROR_SPACE),
		"ssh client empty lifecycle failed");
	pReplies = xrtSshClientGlobalReplies(&Client);
	for ( i = 0u; i < Config.GlobalReplyLimit; ++i ) {
		testRequire((xrtSshClientGlobalReplyReserve(
			&Client,
			i + 1u
		) == XSSH_OK) && (xrtSshReplyQueuePush(
			pReplies,
			(uint64)i
		) == XSSH_OK), "ssh client global reply growth failed");
	}
	for ( i = 0u; i < Config.GlobalReplyLimit; ++i ) {
		testRequire((xrtSshReplyQueuePop(
			pReplies,
			&iToken
		) == XSSH_OK) && (iToken == (uint64)i),
			"ssh client global reply order mismatch");
	}
	testRequire(xrtSshClientClear(&Client),
		"ssh client empty cleanup failed");

	Config.ControlInitial = 0u;
	testRequire(!xrtSshClientInit(&Client, &Config, NULL, NULL),
		"ssh client accepted zero control budget");
	Config.ControlInitial = 128u;
	Config.ControlLimit = 64u;
	testRequire(!xrtSshClientInit(&Client, &Config, NULL, NULL),
		"ssh client accepted inverted control budget");
	return 0;
}
