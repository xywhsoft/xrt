#include <stdio.h>

#include <xssh.h>



/* 展示 connection 会话与调用方全局回复存储的最小组合。 */
int main(void)
{
	xsshconnectionsession Session;
	xsshreplyqueue GlobalReplies;
	uint64 arrTokens[8];

	if ( !xrtSshReplyQueueInit(
		&GlobalReplies,
		arrTokens,
		sizeof(arrTokens) / sizeof(arrTokens[0])
	) || !xrtSshConnectionSessionInit(
		&Session,
		XSSH_ROLE_CLIENT,
		NULL,
		NULL,
		&GlobalReplies
	) ) {
		return 1;
	}
	printf(
		"connection-session=%zu channel-core=%zu global-replies=%zu\n",
		sizeof(Session),
		sizeof(xsshchannelcore),
		GlobalReplies.Capacity
	);
	return 0;
}
