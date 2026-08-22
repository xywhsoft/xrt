#include "../test.h"



/* 协商事实应无损转换为 TCP 或 TLS 共用的 Stream 配置。 */
int main(void)
{
	xwsupgrade Upgrade;
	xwsstreamconfig Config;

	memset(&Upgrade, 0, sizeof(Upgrade));
	Upgrade.Protocol = XRT_STR_LITERAL("chat");
	testRequire(
		xrtWsUpgradeStreamConfig(
			&Config,
			XWS_ROLE_CLIENT,
			&Upgrade
		) && (Config.Role == XWS_ROLE_CLIENT) &&
		(Config.Protocol.Size == 4u) &&
		(memcmp(Config.Protocol.Data, "chat", 4u) == 0) &&
		xrtWsStreamConfigValid(&Config),
		"WebSocket Upgrade Stream configuration mismatch"
	);
	return 0;
}
