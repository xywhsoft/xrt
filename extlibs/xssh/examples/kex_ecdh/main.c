#include <stdio.h>
#include <xssh.h>



/* 构建一个 ECDH init payload。 */
int main(void)
{
	unsigned char arrPublic[32] = { 9u };
	unsigned char arrPayload[64];
	xsshwriter Writer;

	if ( !xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) ||
		(xrtSshEcdhInitWrite(
			&Writer,
			(xbytesview){ arrPublic, sizeof(arrPublic) }
		) != XSSH_OK) ) {
		return 1;
	}
	printf("ecdh-init=%zu\n", Writer.Size);
	return 0;
}
