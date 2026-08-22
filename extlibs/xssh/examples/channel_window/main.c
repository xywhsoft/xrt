#include <stdio.h>

#include <xssh.h>



/* 演示按远端窗口和 max-packet 对待发送数据分片。 */
int main(void)
{
	xsshchannelwindow Window;
	uint32 iChunk;

	if ( !xrtSshChannelWindowInit(
		&Window,
		100000u,
		32768u,
		100000u,
		32768u,
		50000u
	) ) {
		return 1;
	}
	iChunk = xrtSshChannelSendLimit(&Window);
	if ( xrtSshChannelSendCommit(&Window, iChunk) != XSSH_OK ) {
		return 1;
	}
	printf("chunk=%u remaining=%u\n", iChunk, Window.SendWindow);
	return 0;
}
