#include <stdio.h>
#include <xssh.h>



/* 生成本端 identification，解析对端 identification 并选择共同算法。 */
int main(void)
{
	unsigned char arrLocal[64];
	xstrview Banner;
	xstrview Algorithm;
	xsshwriter Writer;
	size_t iConsumed;

	if ( !xrtSshWriterInit(&Writer, arrLocal, sizeof(arrLocal)) ||
		(xrtSshBannerWrite(
			&Writer,
			XRT_STR_LITERAL("SSH-2.0-xssh_example")
		) != XSSH_OK) ) {
		return 1;
	}
	if ( xrtSshBannerRead(
		XRT_STR_LITERAL("notice\r\nSSH-2.0-example\r\n"),
		&Banner,
		&iConsumed
	) != XSSH_OK ) {
		return 2;
	}
	if ( xrtSshNameListFirstMatch(
		XRT_STR_LITERAL("curve25519-sha256,diffie-hellman-group14-sha256"),
		XRT_STR_LITERAL("diffie-hellman-group14-sha256"),
		&Algorithm
	) != XSSH_OK ) {
		return 3;
	}
	fwrite(arrLocal, 1u, Writer.Size, stdout);
	printf(
		"peer=%.*s algorithm=%.*s consumed=%zu\n",
		(int)Banner.Size,
		Banner.Data,
		(int)Algorithm.Size,
		Algorithm.Data,
		iConsumed
	);
	return 0;
}
