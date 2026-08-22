#include <stdio.h>
#include <xssh.h>



/* 构建一个客户端 service request。 */
int main(void)
{
	unsigned char arrPayload[64];
	xsshwriter Writer;

	if ( !xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) ||
		(xrtSshServiceRequestWrite(
			&Writer,
			XRT_STR_LITERAL("ssh-userauth")
		) != XSSH_OK) ) {
		return 1;
	}
	printf("service-request=%zu\n", Writer.Size);
	return 0;
}
