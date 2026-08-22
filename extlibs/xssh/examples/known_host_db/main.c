#include <stdio.h>
#include <xssh.h>



/* 对调用方已经读取的 known_hosts 文本执行常见主机信任判定。 */
int main(void)
{
	static const char sSource[] =
		"host.example ssh-ed25519 "
		"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n";
	static const char sKey[] =
		"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	unsigned char arrKey[64];
	size_t iKeySize = 0u;
	xsshknownhostcheck Check;

	if ( !xrtBase64Decode(
		sKey,
		sizeof(sKey) - 1u,
		arrKey,
		sizeof(arrKey),
		&iKeySize,
		NULL
	) || (xrtSshKnownHostDbCheck(
		XRT_STR_LITERAL(sSource),
		XRT_STR_LITERAL("host.example"),
		22u,
		(xbytesview){ arrKey, iKeySize },
		0u,
		&Check
	) != XSSH_OK) ) {
		return 1;
	}
	printf("trust=%d line=%zu\n", (int)Check.Trust, Check.Entry.LineNumber);
	return Check.Trust == XSSH_KNOWN_HOST_TRUST_MATCH ? 0 : 1;
}
