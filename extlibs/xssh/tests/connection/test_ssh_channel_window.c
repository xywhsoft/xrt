#include "../test.h"



/* 验证初始化、发送切片、扣减与远端窗口更新。 */
static void testSshChannelSendWindow(void)
{
	xsshchannelwindow Window = { 9u, 9u, 9u, 9u, 9u, 9u, 9u };

	testRequire(!xrtSshChannelWindowInit(
		NULL,
		1u,
		1u,
		1u,
		1u,
		1u
	) && !xrtSshChannelWindowInit(
		&Window,
		1u,
		0u,
		1u,
		1u,
		1u
	) && (Window.SendWindow == 9u), "ssh channel invalid init changed state");

	testRequire(xrtSshChannelWindowInit(
		&Window,
		100u,
		32u,
		128u,
		64u,
		64u
	) && (xrtSshChannelSendLimit(&Window) == 32u),
		"ssh channel send init mismatch");
	testRequire((xrtSshChannelSendCommit(&Window, 32u) == XSSH_OK) &&
		(Window.SendWindow == 68u) &&
		(xrtSshChannelSendCommit(&Window, 33u) == XSSH_ERROR_ARGUMENT) &&
		(Window.SendWindow == 68u), "ssh channel max-packet was not enforced");
	testRequire((xrtSshChannelSendCommit(&Window, 32u) == XSSH_OK) &&
		(xrtSshChannelSendCommit(&Window, 32u) == XSSH_OK) &&
		(xrtSshChannelSendLimit(&Window) == 4u) &&
		(xrtSshChannelSendCommit(&Window, 5u) == XSSH_ERROR_SPACE) &&
		(Window.SendWindow == 4u), "ssh channel send window was not enforced");
	testRequire((xrtSshChannelSendAdjust(&Window, UINT32_MAX - 4u) ==
		XSSH_OK) && (Window.SendWindow == UINT32_MAX) &&
		(xrtSshChannelSendAdjust(&Window, 1u) == XSSH_ERROR_PROTOCOL) &&
		(Window.SendWindow == UINT32_MAX),
		"ssh channel send adjust overflow mismatch");
}



/* 验证接收窗口、消费、阈值与返还提交。 */
static void testSshChannelReceiveWindow(void)
{
	xsshchannelwindow Window;
	uint32 iAdjust;

	testRequire(xrtSshChannelWindowInit(
		&Window,
		0u,
		32u,
		100u,
		64u,
		50u
	), "ssh channel receive init failed");
	testRequire((xrtSshChannelReceiveCommit(&Window, 64u) == XSSH_OK) &&
		(Window.ReceiveWindow == 36u) && (Window.ReceiveBuffered == 64u) &&
		(xrtSshChannelReceiveCommit(&Window, 65u) == XSSH_ERROR_PROTOCOL) &&
		(Window.ReceiveWindow == 36u) && (Window.ReceiveBuffered == 64u),
		"ssh channel receive max-packet was not enforced");
	testRequire((xrtSshChannelReceiveConsume(&Window, 49u) == XSSH_OK) &&
		!xrtSshChannelReceiveAdjustReady(&Window) &&
		(xrtSshChannelReceiveConsume(&Window, 1u) == XSSH_OK) &&
		xrtSshChannelReceiveAdjustReady(&Window) &&
		(Window.ReceiveBuffered == 14u) && (Window.ReceivePending == 50u),
		"ssh channel receive threshold mismatch");
	iAdjust = xrtSshChannelReceiveAdjustLimit(&Window);
	testRequire((iAdjust == 50u) &&
		(xrtSshChannelReceiveAdjustCommit(&Window, iAdjust) == XSSH_OK) &&
		(Window.ReceiveWindow == 86u) && (Window.ReceivePending == 0u) &&
		(xrtSshChannelReceiveConsume(&Window, 15u) == XSSH_ERROR_STATE) &&
		(Window.ReceiveBuffered == 14u),
		"ssh channel receive adjust commit mismatch");
}



/* 验证零窗口恢复、动态 grant 和 uint32 回绕边界。 */
static void testSshChannelReceiveGrant(void)
{
	xsshchannelwindow Window;

	testRequire(xrtSshChannelWindowInit(
		&Window,
		0u,
		1u,
		0u,
		16u,
		8u
	) && (xrtSshChannelReceiveGrantCommit(&Window, 16u) == XSSH_OK) &&
		(Window.ReceiveWindow == 16u) &&
		(xrtSshChannelReceiveCommit(&Window, 16u) == XSSH_OK) &&
		(Window.ReceiveWindow == 0u) &&
		(xrtSshChannelReceiveConsume(&Window, 1u) == XSSH_OK) &&
		xrtSshChannelReceiveAdjustReady(&Window),
		"ssh channel zero-window recovery mismatch");
	testRequire((xrtSshChannelReceiveAdjustCommit(&Window, 1u) == XSSH_OK) &&
		(Window.ReceiveWindow == 1u) &&
		(xrtSshChannelReceiveGrantCommit(&Window, UINT32_MAX - 1u) == XSSH_OK) &&
		(Window.ReceiveWindow == UINT32_MAX) &&
		(xrtSshChannelReceiveGrantCommit(&Window, 1u) == XSSH_ERROR_STATE) &&
		(Window.ReceiveWindow == UINT32_MAX),
		"ssh channel receive grant overflow mismatch");
}



/* 运行 channel 双向流控状态测试。 */
int main(void)
{
	testSshChannelSendWindow();
	testSshChannelReceiveWindow();
	testSshChannelReceiveGrant();
	return 0;
}
