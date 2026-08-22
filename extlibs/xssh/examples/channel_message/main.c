#include <stdio.h>

#include <xssh.h>



/* 构建并读取一个零附加字段的 session channel open。 */
int main(void)
{
	unsigned char arrPayload[64];
	xsshchannelopen Open;
	xsshwriter Writer;

	if ( !xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) ||
		(xrtSshChannelOpenWrite(
			&Writer,
			XRT_STR_LITERAL("session"),
			0u,
			1024u * 1024u,
			32768u,
			(xbytesview){ NULL, 0u }
		) != XSSH_OK) || (xrtSshChannelOpenRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Open
		) != XSSH_OK) ) {
		return 1;
	}
	printf(
		"channel=%.*s window=%u max-packet=%u\n",
		(int)Open.Type.Size,
		Open.Type.Data,
		(unsigned int)Open.Window,
		(unsigned int)Open.MaxPacket
	);
	return 0;
}
