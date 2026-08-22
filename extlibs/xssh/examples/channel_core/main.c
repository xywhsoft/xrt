#include <stdio.h>

#include <xssh.h>



/* 建立一个等待 confirmation 且不携带数据缓冲的本端 channel。 */
int main(void)
{
	xsshchannelcore Channel;

	if ( !xrtSshChannelCoreOpenInit(
		&Channel,
		3u,
		1024u * 1024u,
		32u * 1024u,
		512u * 1024u
	) ) {
		return 1;
	}
	printf(
		"channel=%zu local=%u phase=%d\n",
		sizeof(Channel),
		Channel.Local,
		(int)xrtSshChannelCorePhase(&Channel)
	);
	return 0;
}
