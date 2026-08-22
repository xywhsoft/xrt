#include <stdio.h>

#include <xssh.h>



/* 演示显式主机信任和 password 构建器的客户端核心配置。 */
static xsshclienthostdecision exampleHostKey(
	xsshclientcore* pClient,
	const xsshclienthost* pHost,
	ptr pUserData
)
{
	(void)pClient;
	(void)pUserData;
	printf("host-key bytes=%zu\n", pHost->Key.Size);
	return XSSH_CLIENT_HOST_DEFER;
}



int main(void)
{
	xsshclientcoreconfig Config;
	xsshclientcore Client;
	xstrview Password = XRT_STR_LITERAL("secret");

	if ( !xrtSshClientCoreConfigInit(&Config) ) {
		return 1;
	}
	Config.User = XRT_STR_LITERAL("alice");
	Config.HostKey = exampleHostKey;
	Config.Authenticate = xrtSshClientPasswordAuth;
	Config.AuthenticateData = &Password;
	if ( !xrtSshClientCoreInit(&Client, &Config) ) {
		return 1;
	}
	printf(
		"version=%.*s output-limit=%zu\n",
		(int)Config.Version.Size,
		Config.Version.Data,
		Config.OutputLimit
	);
	xrtSshClientCoreClear(&Client);
	return 0;
}
