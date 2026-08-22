#include <stdio.h>

#include <xssh.h>



/* 演示远端先 close 时的强制回复路径。 */
int main(void)
{
	xsshchannelstate State;

	if ( !xrtSshChannelStateInit(&State) ||
		(xrtSshChannelRemoteCloseCommit(&State) != XSSH_OK) ) {
		return 1;
	}
	printf("reply-close=%d\n", xrtSshChannelCloseReplyNeeded(&State));
	return 0;
}
