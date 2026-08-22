#include "../test.h"



/* 验证本端 open、confirmation、窗口流控和双向关闭。 */
static void testSshChannelCoreOutgoing(void)
{
	unsigned char arrPayload[256];
	unsigned char arrData[40] = { 0u };
	xsshchannelconfirmation Confirmation;
	xsshchanneladjust Adjust;
	xsshchanneldata Data;
	xsshchannelcore Channel;
	xsshwriter Writer;
	uint32 iLocal;
	uint32 iRemote;

	testRequire(xrtSshChannelCoreOpenInit(
		&Channel,
		7u,
		100u,
		64u,
		50u
	) && (xrtSshChannelCorePhase(&Channel) ==
		XSSH_CHANNEL_CORE_OPENING) &&
		!xrtSshChannelCoreOpen(&Channel) &&
		(xrtSshChannelCoreSendLimit(&Channel) == 0u) &&
		!xrtSshChannelCoreIds(&Channel, &iLocal, &iRemote),
		"ssh outgoing channel init mismatch");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelOpenConfirmationWrite(
			&Writer,
			7u,
			12u,
			80u,
			32u,
			(xbytesview){ NULL, 0u }
		) == XSSH_OK) && (xrtSshChannelOpenConfirmationRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Confirmation
		) == XSSH_OK) && (xrtSshChannelCoreConfirmationCommit(
			&Channel,
			&Confirmation
		) == XSSH_OK) && xrtSshChannelCoreOpen(&Channel) &&
		xrtSshChannelCoreIds(&Channel, &iLocal, &iRemote) &&
		(iLocal == 7u) && (iRemote == 12u) &&
		(xrtSshChannelCoreSendLimit(&Channel) == 32u),
		"ssh outgoing channel confirmation failed");

	testRequire((xrtSshChannelCoreDataSendCommit(
		&Channel,
		32u
	) == XSSH_OK) && (Channel.Window.SendWindow == 48u) &&
		(xrtSshChannelCoreDataSendCommit(
			&Channel,
			33u
		) == XSSH_ERROR_ARGUMENT) &&
		xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelDataWrite(
			&Writer,
			7u,
			(xbytesview){ arrData, sizeof(arrData) }
		) == XSSH_OK) && (xrtSshChannelDataRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Data
		) == XSSH_OK) && (xrtSshChannelCoreDataReceiveCommit(
			&Channel,
			Data.Recipient,
			(uint32)Data.Data.Size
		) == XSSH_OK) && (Channel.Window.ReceiveWindow == 60u) &&
		(Channel.Window.ReceiveBuffered == 40u),
		"ssh channel data flow failed");
	testRequire((xrtSshChannelCoreDataConsume(
		&Channel,
		40u
	) == XSSH_OK) && !xrtSshChannelCoreAdjustReady(&Channel) &&
		(xrtSshChannelCoreDataReceiveCommit(
			&Channel,
			7u,
			20u
		) == XSSH_OK) && (xrtSshChannelCoreDataConsume(
			&Channel,
			20u
		) == XSSH_OK) && xrtSshChannelCoreAdjustReady(&Channel) &&
		(xrtSshChannelCoreAdjustLimit(&Channel) == 60u) &&
		(xrtSshChannelCoreAdjustSendCommit(
			&Channel,
			60u
		) == XSSH_OK) && (Channel.Window.ReceiveWindow == 100u),
		"ssh channel receive adjustment failed");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelWindowAdjustWrite(
			&Writer,
			7u,
			100u
		) == XSSH_OK) && (xrtSshChannelWindowAdjustRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Adjust
		) == XSSH_OK) && (xrtSshChannelCoreAdjustReceiveCommit(
			&Channel,
			&Adjust
		) == XSSH_OK) && (Channel.Window.SendWindow == 148u) &&
		(xrtSshChannelCoreDataReceiveCommit(
			&Channel,
			8u,
			1u
		) == XSSH_ERROR_PROTOCOL),
		"ssh channel window adjust or recipient validation failed");

	testRequire(xrtSshChannelCoreCanSendRequest(&Channel) &&
		xrtSshChannelCoreCanReceiveRequest(&Channel) &&
		(xrtSshChannelCoreEofSendCommit(&Channel) == XSSH_OK) &&
		(xrtSshChannelCoreSendLimit(&Channel) == 0u) &&
		xrtSshChannelCoreCanSendRequest(&Channel) &&
		(xrtSshChannelCoreEofReceiveCommit(
			&Channel,
			7u
		) == XSSH_OK) && (xrtSshChannelCoreDataReceiveCommit(
			&Channel,
			7u,
			1u
		) == XSSH_ERROR_PROTOCOL) && (xrtSshChannelCoreCloseReceiveCommit(
			&Channel,
			7u
		) == XSSH_OK) &&
		xrtSshChannelCloseReplyNeeded(&Channel.State) &&
		!xrtSshChannelCoreCanSendRequest(&Channel) &&
		(xrtSshChannelCoreCloseSendCommit(&Channel) == XSSH_OK) &&
		xrtSshChannelCoreClosed(&Channel),
		"ssh channel EOF/CLOSE lifecycle failed");
}



/* 验证 peer open 的接受和拒绝路径。 */
static void testSshChannelCoreIncoming(void)
{
	unsigned char arrPayload[128];
	xsshchannelopen Open;
	xsshchannelcore Channel;
	xsshwriter Writer;
	uint32 iLocal;
	uint32 iRemote;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelOpenWrite(
			&Writer,
			XRT_STR_LITERAL("session"),
			33u,
			64u,
			16u,
			(xbytesview){ NULL, 0u }
		) == XSSH_OK) && (xrtSshChannelOpenRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Open
		) == XSSH_OK), "ssh incoming channel open build failed");
	testRequire(xrtSshChannelCoreAcceptInit(
		&Channel,
		2u,
		&Open,
		128u,
		32u,
		64u
	) && (xrtSshChannelCorePhase(&Channel) ==
		XSSH_CHANNEL_CORE_ACCEPTING) &&
		xrtSshChannelCoreIds(&Channel, &iLocal, &iRemote) &&
		(iLocal == 2u) && (iRemote == 33u) &&
		(xrtSshChannelCoreAcceptCommit(&Channel) == XSSH_OK) &&
		xrtSshChannelCoreOpen(&Channel) &&
		(xrtSshChannelCoreSendLimit(&Channel) == 16u),
		"ssh incoming channel accept failed");

	testRequire(xrtSshChannelCoreAcceptInit(
		&Channel,
		3u,
		&Open,
		128u,
		32u,
		64u
	) && (xrtSshChannelCoreRejectCommit(
		&Channel,
		XSSH_CHANNEL_OPEN_ADMINISTRATIVELY_PROHIBITED
	) == XSSH_OK) && (xrtSshChannelCorePhase(&Channel) ==
		XSSH_CHANNEL_CORE_FAILED) &&
		(Channel.FailureReason ==
		 XSSH_CHANNEL_OPEN_ADMINISTRATIVELY_PROHIBITED),
		"ssh incoming channel reject failed");
}



/* 验证 open failure、非法 confirmation 和清理原子性。 */
static void testSshChannelCoreBoundaries(void)
{
	xsshchannelconfirmation Confirmation = {
		9u, 10u, 100u, 32u, { NULL, 0u }
	};
	xsshchannelopenfailure Failure = {
		7u, XSSH_CHANNEL_OPEN_CONNECT_FAILED,
		XRT_STR_LITERAL("connect failed"), XRT_STR_LITERAL("en")
	};
	xsshchannelcore Channel;

	testRequire(!xrtSshChannelCoreOpenInit(
		NULL,
		0u,
		1u,
		1u,
		1u
	) && xrtSshChannelCoreOpenInit(
		&Channel,
		7u,
		100u,
		64u,
		50u
	) && (xrtSshChannelCoreConfirmationCommit(
		&Channel,
		&Confirmation
	) == XSSH_ERROR_PROTOCOL) &&
		(xrtSshChannelCorePhase(&Channel) ==
		 XSSH_CHANNEL_CORE_OPENING) &&
		(xrtSshChannelCoreFailureCommit(
			&Channel,
			&Failure
		) == XSSH_OK) && (xrtSshChannelCorePhase(&Channel) ==
		XSSH_CHANNEL_CORE_FAILED) &&
		(Channel.FailureReason == XSSH_CHANNEL_OPEN_CONNECT_FAILED),
		"ssh channel open failure boundary failed");
	xrtSshChannelCoreClear(&Channel);
	testRequire((xrtSshChannelCorePhase(&Channel) ==
		XSSH_CHANNEL_CORE_FAILED) && !xrtSshChannelCoreOpen(&Channel),
		"ssh channel core clear failed");
}



/* 运行无缓冲单 channel 组合状态测试。 */
int main(void)
{
	testSshChannelCoreOutgoing();
	testSshChannelCoreIncoming();
	testSshChannelCoreBoundaries();
	return 0;
}
