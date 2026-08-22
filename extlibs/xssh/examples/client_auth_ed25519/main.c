#include <stdio.h>
#include <string.h>

#include <xssh.h>



/* 展示把已解析的 OpenSSH Ed25519 身份直接安装为客户端认证 provider。 */
int main(void)
{
	xsshclientcoreconfig Config;
	xsshed25519identity Identity;

	memset(&Identity, 0, sizeof(Identity));
	if ( !xrtSshClientCoreConfigInit(&Config) ) {
		return 1;
	}
	Config.Authenticate = xrtSshClientEd25519Auth;
	Config.AuthenticateData = &Identity;
	printf("provider=%s\n",
		Config.Authenticate == xrtSshClientEd25519Auth ?
		"ssh-ed25519" : "none");
	return 0;
}
