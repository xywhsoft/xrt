#include "../test.h"



/* 验证双向 EOF 后仍保持 channel 控制面开放。 */
static void testSshChannelHalfClose(void)
{
	xsshchannelstate State;

	testRequire(xrtSshChannelStateInit(&State) &&
		xrtSshChannelCanSendData(&State) &&
		xrtSshChannelCanReceiveData(&State) &&
		xrtSshChannelCanSendRequest(&State), "ssh channel state init mismatch");
	testRequire((xrtSshChannelLocalEofCommit(&State) == XSSH_OK) &&
		!xrtSshChannelCanSendData(&State) &&
		xrtSshChannelCanReceiveData(&State) &&
		xrtSshChannelCanSendRequest(&State) &&
		(xrtSshChannelLocalEofCommit(&State) == XSSH_ERROR_STATE),
		"ssh channel local EOF mismatch");
	testRequire((xrtSshChannelRemoteEofCommit(&State) == XSSH_OK) &&
		!xrtSshChannelCanReceiveData(&State) &&
		xrtSshChannelCanSendRequest(&State) &&
		(xrtSshChannelRemoteEofCommit(&State) == XSSH_ERROR_PROTOCOL),
		"ssh channel remote EOF mismatch");
}



/* 验证本端先 close 的双向回收与在途接收语义。 */
static void testSshChannelLocalClose(void)
{
	xsshchannelstate State;

	testRequire(xrtSshChannelStateInit(&State) &&
		(xrtSshChannelLocalCloseCommit(&State) == XSSH_OK) &&
		!xrtSshChannelCanSendData(&State) &&
		xrtSshChannelCanReceiveData(&State) &&
		!xrtSshChannelCanSendRequest(&State) &&
		!xrtSshChannelCloseReplyNeeded(&State) &&
		!xrtSshChannelClosed(&State), "ssh channel local close mismatch");
	testRequire((xrtSshChannelRemoteEofCommit(&State) == XSSH_OK) &&
		(xrtSshChannelRemoteCloseCommit(&State) == XSSH_OK) &&
		xrtSshChannelClosed(&State) &&
		(xrtSshChannelLocalCloseCommit(&State) == XSSH_ERROR_STATE) &&
		(xrtSshChannelRemoteCloseCommit(&State) == XSSH_ERROR_PROTOCOL),
		"ssh channel local-first close completion mismatch");
}



/* 验证远端先 close 时必须回复且不能再发送或接收数据。 */
static void testSshChannelRemoteClose(void)
{
	xsshchannelstate State = { false, false, false, false, false };

	testRequire(!xrtSshChannelStateInit(NULL) &&
		!xrtSshChannelCanSendData(&State) &&
		(xrtSshChannelRemoteCloseCommit(&State) == XSSH_ERROR_ARGUMENT),
		"ssh channel invalid state was accepted");
	testRequire(xrtSshChannelStateInit(&State) &&
		(xrtSshChannelRemoteCloseCommit(&State) == XSSH_OK) &&
		xrtSshChannelCloseReplyNeeded(&State) &&
		!xrtSshChannelCanSendData(&State) &&
		!xrtSshChannelCanReceiveData(&State) &&
		!xrtSshChannelCanSendRequest(&State) &&
		(xrtSshChannelLocalEofCommit(&State) == XSSH_ERROR_STATE),
		"ssh channel remote-first close mismatch");
	testRequire((xrtSshChannelLocalCloseCommit(&State) == XSSH_OK) &&
		!xrtSshChannelCloseReplyNeeded(&State) &&
		xrtSshChannelClosed(&State), "ssh channel close reply mismatch");
}



/* 运行 channel EOF/CLOSE 生命周期测试。 */
int main(void)
{
	testSshChannelHalfClose();
	testSshChannelLocalClose();
	testSshChannelRemoteClose();
	return 0;
}
