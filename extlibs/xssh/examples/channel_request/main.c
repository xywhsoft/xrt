#include <stdio.h>

#include <xssh.h>



/* 构建并读取一个 exec request。 */
int main(void)
{
	unsigned char arrPayload[64];
	xsshchannelrequest Request;
	xbytesview Command;
	xsshwriter Writer;

	if ( !xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) ||
		(xrtSshChannelExecWrite(
			&Writer,
			0u,
			true,
			XRT_BYTES_LITERAL("printf ok")
		) != XSSH_OK) || (xrtSshChannelRequestRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Request
		) != XSSH_OK) || (xrtSshChannelExecRead(
			&Request,
			&Command
		) != XSSH_OK) ) {
		return 1;
	}
	printf("command=%.*s\n", (int)Command.Size, (const char*)Command.Data);
	return 0;
}
