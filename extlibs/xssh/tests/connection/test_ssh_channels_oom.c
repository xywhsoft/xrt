#include "../test.h"
#include "../../../../tests/test_fault_allocator.h"



/* 验证节点和回复存储分配失败都保持集合可重试且不泄漏。 */
int main(void)
{
	static testfaultallocator State = { 0u, SIZE_MAX, 0u, false };
	xallocator Allocator = testFaultAllocator(&State);
	xsshchannelsconfig Config;
	xsshchannels Channels;
	xsshchannel* pChannel = (xsshchannel*)1;
	xsshcode Code;

	testRequire(
		xrtSetAllocator(&Allocator),
		"ssh channels OOM allocator install failed"
	);
	xrtSshChannelsConfigInit(&Config);
	Config.MaxChannels = 2u;
	Config.ReplyLimit = 4u;
	testRequire(
		xrtSshChannelsInit(&Channels, NULL, &Config),
		"ssh channels OOM init failed"
	);

	State.FailAt = State.Calls + 2u;
	State.Hit = false;
	Code = xrtSshChannelsOpen(&Channels, &pChannel);
	testRequire(
		(Code == XSSH_ERROR_SPACE) && State.Hit &&
		(pChannel == NULL) &&
		(xrtSshChannelsCount(&Channels) == 0u),
		"ssh channels OOM left a partial map entry"
	);
	xrtClearError();

	State.FailAt = SIZE_MAX;
	testRequire(
		(xrtSshChannelsOpen(&Channels, &pChannel) == XSSH_OK) &&
		(pChannel != NULL) && (pChannel->ReplyCapacity == 0u),
		"ssh channels did not recover after node OOM"
	);
	State.FailAt = State.Calls + 1u;
	State.Hit = false;
	testRequire(
		(xrtSshChannelReplyReserve(pChannel, 2u) ==
		 XSSH_ERROR_SPACE) && State.Hit &&
		(pChannel->ReplyCapacity == 0u) &&
		(xrtSshReplyQueueCount(&pChannel->Replies) == 0u),
		"ssh channels reply OOM changed queue state"
	);
	xrtClearError();

	State.FailAt = SIZE_MAX;
	testRequire(
		(xrtSshChannelReplyReserve(pChannel, 2u) == XSSH_OK) &&
		(pChannel->ReplyCapacity >= 2u),
		"ssh channels reply queue did not recover"
	);
	xrtSshChannelsClear(&Channels);
	return 0;
}
