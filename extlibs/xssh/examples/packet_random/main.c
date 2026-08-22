#include <inttypes.h>
#include <stdio.h>
#include <xssh.h>



/* 使用系统安全随机源构建可直接发送的 plain SSH packet。 */
int main(void)
{
	unsigned char arrWire[64];
	xsshwriter Writer;
	uint32 iSequence = 0u;

	if ( !xrtSshWriterInit(&Writer, arrWire, sizeof(arrWire)) ||
		(xrtSshPacketWriteSecure(
			&Writer,
			XRT_BYTES_LITERAL("\2keepalive"),
			8u,
			&iSequence
		) != XSSH_OK) ) {
		return 1;
	}
	printf("wire=%zu next-sequence=%" PRIu32 "\n", Writer.Size, iSequence);
	return 0;
}
