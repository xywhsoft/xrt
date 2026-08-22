#include <stdio.h>

#include <xssh.h>



/* 展示按需创建 channel，并把集合直接绑定到 connection resolver。 */
int main(void)
{
	xsshconnectionsession Session;
	xsshreplyqueue GlobalReplies;
	xsshchannels Channels;
	xsshchannel* pChannel = NULL;

	if ( !xrtSshReplyQueueInit(&GlobalReplies, NULL, 0u) ||
		!xrtSshChannelsInit(&Channels, NULL, NULL) ||
		!xrtSshConnectionSessionInit(
			&Session,
			XSSH_ROLE_CLIENT,
			xrtSshChannelsResolve,
			&Channels,
			&GlobalReplies
		) ||
		(xrtSshChannelsOpen(&Channels, &pChannel) != XSSH_OK) ) {
		return 1;
	}
	printf(
		"local=%u channels=%zu allocated_reply_tokens=%zu\n",
		pChannel->Core.Local,
		xrtSshChannelsCount(&Channels),
		pChannel->ReplyCapacity
	);
	xrtSshConnectionSessionClear(&Session);
	xrtSshChannelsClear(&Channels);
	return 0;
}
