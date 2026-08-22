#include <stdio.h>
#include <xssh.h>



/* 查询 OpenSSH 私钥 PEM 所需二进制缓冲长度。 */
int main(void)
{
	static const char sPem[] =
		"-----BEGIN OPENSSH PRIVATE KEY-----\n"
		"b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAADAAAAAhleGFt"
		"cGxlAAAAA3ByaQ==\n"
		"-----END OPENSSH PRIVATE KEY-----\n";
	size_t iBinarySize;
	xsshcode Code = xrtSshPrivateKeyPemRead(
		XRT_STR_LITERAL(sPem),
		NULL,
		0u,
		&iBinarySize,
		NULL
	);

	printf("binary-size=%zu\n", Code == XSSH_OK ? iBinarySize : 0u);
	return Code == XSSH_OK ? 0 : 1;
}
