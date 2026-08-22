#include <stdio.h>
#include <xssh.h>



/* 解析并匹配一条带否定 pattern 的 known_hosts 记录。 */
int main(void)
{
	static const char sLine[] =
		"*.example.com,!blocked.example.com ssh-ed25519 "
		"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	xsshknownhostline KnownHost;
	xsshknownhostmatch Match;

	if ( (xrtSshKnownHostLineRead(
		XRT_STR_LITERAL(sLine),
		&KnownHost
	) != XSSH_OK) || (xrtSshKnownHostLineMatch(
		&KnownHost,
		XRT_STR_LITERAL("server.example.com"),
		22u,
		&Match
	) != XSSH_OK) ) {
		return 1;
	}
	printf("host-match=%d\n", (int)Match);
	return Match == XSSH_KNOWN_HOST_MATCH ? 0 : 1;
}
