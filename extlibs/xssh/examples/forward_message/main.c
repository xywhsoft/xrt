#include <stdio.h>

#include <xssh.h>



/* 构建一个 direct-tcpip channel open。 */
int main(void)
{
	unsigned char arrPayload[128];
	xsshwriter Writer;

	if ( !xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) ||
		(xrtSshDirectTcpipOpenWrite(
			&Writer,
			0u,
			1024u * 1024u,
			32768u,
			XRT_BYTES_LITERAL("db.internal"),
			5432u,
			XRT_BYTES_LITERAL("127.0.0.1"),
			50000u
		) != XSSH_OK) ) {
		return 1;
	}
	printf("direct-tcpip-payload=%zu\n", Writer.Size);
	return 0;
}
