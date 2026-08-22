#include "../test.h"



#define TEST_SSH_CHANNEL_STRESS_COUNT 64u
#define TEST_SSH_CHANNEL_STRESS_BYTES 256u
#define TEST_SSH_CHANNEL_STRESS_WINDOW 32u
#define TEST_SSH_CHANNEL_STRESS_PACKET 8u



/* 使用小预算建立可预测的动态 channel 集合。 */
static void testSshChannelsInit(
	xsshchannels* pChannels,
	size_t iMaxChannels
)
{
	xsshchannelsconfig Config;

	xrtSshChannelsConfigInit(&Config);
	Config.MaxChannels = iMaxChannels;
	Config.ReplyLimit = 4u;
	Config.ReceiveWindow = 64u;
	Config.ReceiveMaxPacket = 16u;
	Config.AdjustThreshold = 32u;
	Config.Io.ReceiveLimit = 64u;
	Config.Io.SendLimit = 128u;
	testRequire(
		xrtSshChannelsInit(pChannels, NULL, &Config),
		"ssh channels init failed"
	);
}



/* 验证本端编号、容量硬上限、稳定查询和 resolver。 */
static void testSshChannelsOpen(void)
{
	xsshchannelcore* pCore = NULL;
	xsshreplyqueue* pReplies = NULL;
	xsshchannels Channels;
	xsshchannel* pFirst = NULL;
	xsshchannel* pSecond = NULL;
	xsshchannel* pOverflow = (xsshchannel*)1;

	testSshChannelsInit(&Channels, 2u);
	testRequire(
		(xrtSshChannelsOpen(&Channels, &pFirst) == XSSH_OK) &&
		(pFirst != NULL) && (pFirst->Core.Local == 0u) &&
		!pFirst->Incoming &&
		(xrtSshChannelsOpen(&Channels, &pSecond) == XSSH_OK) &&
		(pSecond != NULL) && (pSecond->Core.Local == 1u) &&
		(xrtSshChannelsCount(&Channels) == 2u),
		"ssh channels local open mismatch"
	);
	testRequire(
		(xrtSshChannelsOpen(&Channels, &pOverflow) ==
		 XSSH_ERROR_SPACE) && (pOverflow == NULL) &&
		(xrtSshChannelsGet(&Channels, 0u) == pFirst) &&
		(xrtSshChannelsConstGet(&Channels, 1u) == pSecond) &&
		xrtSshChannelsResolve(
			&Channels,
			0u,
			&pCore,
			&pReplies
		) && (pCore == &pFirst->Core) &&
		(pReplies == &pFirst->Replies),
		"ssh channels resolver mismatch"
	);
	xrtSshChannelsClear(&Channels);
}



/* 验证回复队列扩容不会改变已有 token 的顺序。 */
static void testSshChannelsReplies(void)
{
	xsshchannels Channels;
	xsshchannel* pChannel = NULL;
	uint64 iToken = 0u;

	testSshChannelsInit(&Channels, 1u);
	testRequire(
		(xrtSshChannelsOpen(&Channels, &pChannel) == XSSH_OK) &&
		(xrtSshChannelReplyReserve(pChannel, 2u) == XSSH_OK) &&
		(xrtSshReplyQueuePush(&pChannel->Replies, 11u) == XSSH_OK) &&
		(xrtSshReplyQueuePush(&pChannel->Replies, 22u) == XSSH_OK) &&
		(xrtSshChannelReplyReserve(pChannel, 4u) == XSSH_OK) &&
		(pChannel->ReplyCapacity == 4u),
		"ssh channels reply reserve failed"
	);
	testRequire(
		(xrtSshReplyQueuePop(&pChannel->Replies, &iToken) == XSSH_OK) &&
		(iToken == 11u) &&
		(xrtSshReplyQueuePop(&pChannel->Replies, &iToken) == XSSH_OK) &&
		(iToken == 22u) &&
		(xrtSshChannelReplyReserve(pChannel, 5u) ==
		 XSSH_ERROR_SPACE),
		"ssh channels reply order or limit mismatch"
	);
	xrtSshChannelsClear(&Channels);
}



/* 验证 uint32 编号回绕后跳过占用项，并继续保持单调分配。 */
static void testSshChannelsIdWrap(void)
{
	xsshchannels Channels;
	xsshchannel* pChannels[4];
	size_t i;

	memset(pChannels, 0, sizeof(pChannels));
	testSshChannelsInit(&Channels, 4u);
	Channels.NextLocal = UINT32_MAX - 1u;
	testRequire(
		(xrtSshChannelsOpen(&Channels, &pChannels[0]) == XSSH_OK) &&
		(pChannels[0]->Core.Local == (UINT32_MAX - 1u)) &&
		(xrtSshChannelsOpen(&Channels, &pChannels[1]) == XSSH_OK) &&
		(pChannels[1]->Core.Local == UINT32_MAX) &&
		(xrtSshChannelsOpen(&Channels, &pChannels[2]) == XSSH_OK) &&
		(pChannels[2]->Core.Local == 0u),
		"ssh channels id wrap mismatch"
	);
	Channels.NextLocal = UINT32_MAX - 1u;
	testRequire(
		(xrtSshChannelsOpen(&Channels, &pChannels[3]) == XSSH_OK) &&
		(pChannels[3]->Core.Local == 1u) &&
		(xrtSshChannelsCount(&Channels) == 4u),
		"ssh channels occupied id skip mismatch"
	);
	for ( i = 0u; i < 4u; ++i ) {
		testRequire(xrtSshChannelsDiscard(
			&Channels,
			pChannels[i]->Core.Local
		), "ssh channels wrapped id discard failed");
	}
	testRequire(xrtSshChannelsCount(&Channels) == 0u,
		"ssh channels wrapped id cleanup mismatch");
	xrtSshChannelsClear(&Channels);
}



/* 验证大量未完成请求按需扩容、环绕和释放。 */
static void testSshChannelsReplyStress(void)
{
	xsshchannelsconfig Config;
	xsshchannels Channels;
	xsshchannel* pChannel = NULL;
	uint64 iToken;
	size_t i;

	xrtSshChannelsConfigInit(&Config);
	Config.MaxChannels = 1u;
	Config.ReplyLimit = 4096u;
	Config.ReceiveWindow = 64u;
	Config.ReceiveMaxPacket = 16u;
	Config.AdjustThreshold = 32u;
	Config.Io.ReceiveLimit = 64u;
	Config.Io.SendLimit = 128u;
	testRequire(xrtSshChannelsInit(&Channels, NULL, &Config) &&
		(xrtSshChannelsOpen(&Channels, &pChannel) == XSSH_OK),
		"ssh channels reply stress init failed");
	for ( i = 0u; i < 4096u; ++i ) {
		testRequire((xrtSshChannelReplyReserve(
			pChannel,
			i + 1u
		) == XSSH_OK) && (xrtSshReplyQueuePush(
			&pChannel->Replies,
			(uint64)i
		) == XSSH_OK), "ssh channels reply stress grow failed");
	}
	testRequire((pChannel->ReplyCapacity == 4096u) &&
		(xrtSshReplyQueueCount(&pChannel->Replies) == 4096u) &&
		(xrtSshChannelReplyReserve(pChannel, 4097u) == XSSH_ERROR_SPACE),
		"ssh channels reply stress limit mismatch");
	for ( i = 0u; i < 2048u; ++i ) {
		testRequire((xrtSshReplyQueuePop(
			&pChannel->Replies,
			&iToken
		) == XSSH_OK) && (iToken == (uint64)i),
			"ssh channels reply stress prefix mismatch");
	}
	for ( i = 4096u; i < 6144u; ++i ) {
		testRequire(xrtSshReplyQueuePush(
			&pChannel->Replies,
			(uint64)i
		) == XSSH_OK, "ssh channels reply stress wrap failed");
	}
	for ( i = 2048u; i < 6144u; ++i ) {
		testRequire((xrtSshReplyQueuePop(
			&pChannel->Replies,
			&iToken
		) == XSSH_OK) && (iToken == (uint64)i),
			"ssh channels reply stress order mismatch");
	}
	testRequire(xrtSshReplyQueueCount(&pChannel->Replies) == 0u,
		"ssh channels reply stress drain mismatch");
	xrtSshChannelsClear(&Channels);
}



/* 验证多 channel 独立耗尽窗口、阻塞并在 WINDOW_ADJUST 后恢复。 */
static void testSshChannelsBackpressure(void)
{
	unsigned char arrData[TEST_SSH_CHANNEL_STRESS_BYTES];
	unsigned char arrPayload[64];
	xsshchannelsconfig Config;
	xsshchannels Channels;
	xsshchannel* arrChannels[TEST_SSH_CHANNEL_STRESS_COUNT];
	xsshchannelconfirmation Confirmation;
	xsshchanneladjust Adjust;
	xsshchanneldata Data;
	xsshwriter Writer;
	xbytesview Payload;
	size_t i;
	size_t iRound;

	for ( i = 0u; i < sizeof(arrData); ++i ) {
		arrData[i] = (unsigned char)((i * 37u) + 11u);
	}
	memset(arrChannels, 0, sizeof(arrChannels));
	xrtSshChannelsConfigInit(&Config);
	Config.MaxChannels = TEST_SSH_CHANNEL_STRESS_COUNT;
	Config.ReplyLimit = 4u;
	Config.ReceiveWindow = 64u;
	Config.ReceiveMaxPacket = 16u;
	Config.AdjustThreshold = 32u;
	Config.Io.ReceiveLimit = 64u;
	Config.Io.SendLimit = TEST_SSH_CHANNEL_STRESS_BYTES;
	testRequire(xrtSshChannelsInit(&Channels, NULL, &Config),
		"ssh channels backpressure init failed");
	for ( i = 0u; i < TEST_SSH_CHANNEL_STRESS_COUNT; ++i ) {
		testRequire(xrtSshChannelsOpen(
			&Channels,
			&arrChannels[i]
		) == XSSH_OK, "ssh channels backpressure open failed");
		memset(&Confirmation, 0, sizeof(Confirmation));
		Confirmation.Recipient = arrChannels[i]->Core.Local;
		Confirmation.Sender = (uint32)(1000u + i);
		Confirmation.Window = TEST_SSH_CHANNEL_STRESS_WINDOW;
		Confirmation.MaxPacket = TEST_SSH_CHANNEL_STRESS_PACKET;
		testRequire((xrtSshChannelCoreConfirmationCommit(
			&arrChannels[i]->Core,
			&Confirmation
		) == XSSH_OK) && (xrtSshChannelIoWrite(
			&arrChannels[i]->Io,
			XSSH_CHANNEL_IO_DATA,
			arrData,
			sizeof(arrData)
		) == XSSH_OK) && (xrtSshChannelIoWrite(
			&arrChannels[i]->Io,
			XSSH_CHANNEL_IO_DATA,
			arrData,
			1u
		) == XSSH_ERROR_SPACE),
			"ssh channels backpressure queue failed");
	}
	for ( iRound = 0u; iRound <
		(TEST_SSH_CHANNEL_STRESS_BYTES / TEST_SSH_CHANNEL_STRESS_WINDOW);
		++iRound ) {
		for ( i = 0u; i < TEST_SSH_CHANNEL_STRESS_COUNT; ++i ) {
			size_t iPacket;

			for ( iPacket = 0u; iPacket <
				(TEST_SSH_CHANNEL_STRESS_WINDOW /
				 TEST_SSH_CHANNEL_STRESS_PACKET); ++iPacket ) {
				testRequire(xrtSshWriterInit(
					&Writer,
					arrPayload,
					sizeof(arrPayload)
				) && (xrtSshChannelIoSendPrepare(
					&arrChannels[i]->Io,
					XSSH_CHANNEL_IO_DATA,
					&Writer,
					&Payload
				) == XSSH_OK) && (xrtSshChannelDataRead(
					Payload,
					&Data
				) == XSSH_OK) &&
					(Data.Recipient == (uint32)(1000u + i)) &&
					(Data.Data.Size == TEST_SSH_CHANNEL_STRESS_PACKET) &&
					(memcmp(
						Data.Data.Data,
						arrData + (iRound * TEST_SSH_CHANNEL_STRESS_WINDOW) +
							(iPacket * TEST_SSH_CHANNEL_STRESS_PACKET),
						TEST_SSH_CHANNEL_STRESS_PACKET
					) == 0) && (xrtSshChannelCoreDataSendCommit(
					&arrChannels[i]->Core,
					TEST_SSH_CHANNEL_STRESS_PACKET
				) == XSSH_OK) && (xrtSshChannelIoSendCommit(
					&arrChannels[i]->Io
				) == XSSH_OK), "ssh channels backpressure send failed");
			}
			testRequire((xrtSshChannelIoSendLimit(
				&arrChannels[i]->Io,
				XSSH_CHANNEL_IO_DATA
			) == 0u) && (arrChannels[i]->Core.Window.SendWindow == 0u),
				"ssh channels backpressure did not block");
		}
		for ( i = TEST_SSH_CHANNEL_STRESS_COUNT; i != 0u; --i ) {
			xsshchannel* pChannel = arrChannels[i - 1u];

			Adjust.Recipient = pChannel->Core.Local;
			Adjust.Bytes = TEST_SSH_CHANNEL_STRESS_WINDOW;
			testRequire(xrtSshChannelCoreAdjustReceiveCommit(
				&pChannel->Core,
				&Adjust
			) == XSSH_OK, "ssh channels backpressure adjust failed");
		}
	}
	for ( i = 0u; i < TEST_SSH_CHANNEL_STRESS_COUNT; ++i ) {
		testRequire((xrtSshChannelIoQueued(
			&arrChannels[i]->Io,
			XSSH_CHANNEL_IO_DATA
		) == 0u) && (xrtSshChannelIoWritable(
			&arrChannels[i]->Io
		) == TEST_SSH_CHANNEL_STRESS_BYTES),
			"ssh channels backpressure drain mismatch");
	}
	for ( iRound = 0u; iRound <
		(64u / TEST_SSH_CHANNEL_STRESS_PACKET); ++iRound ) {
		for ( i = 0u; i < TEST_SSH_CHANNEL_STRESS_COUNT; ++i ) {
			testRequire((xrtSshChannelIoReceivePrepare(
				&arrChannels[i]->Io,
				XSSH_CHANNEL_IO_DATA,
				arrChannels[i]->Core.Local,
				(xbytesview){
					arrData + (iRound * TEST_SSH_CHANNEL_STRESS_PACKET),
					TEST_SSH_CHANNEL_STRESS_PACKET
				}
			) == XSSH_OK) && (xrtSshChannelCoreDataReceiveCommit(
				&arrChannels[i]->Core,
				arrChannels[i]->Core.Local,
				TEST_SSH_CHANNEL_STRESS_PACKET
			) == XSSH_OK) && (xrtSshChannelIoReceiveCommit(
				&arrChannels[i]->Io
			) == XSSH_OK), "ssh channels receive pressure failed");
		}
	}
	for ( i = 0u; i < TEST_SSH_CHANNEL_STRESS_COUNT; ++i ) {
		testRequire((xrtSshChannelIoReadable(
			&arrChannels[i]->Io,
			XSSH_CHANNEL_IO_DATA
		) == 64u) && (xrtSshChannelIoReceivePrepare(
			&arrChannels[i]->Io,
			XSSH_CHANNEL_IO_DATA,
			arrChannels[i]->Core.Local,
			(xbytesview){ arrData, 1u }
		) == XSSH_ERROR_SPACE),
			"ssh channels receive limit did not block");
	}
	for ( i = TEST_SSH_CHANNEL_STRESS_COUNT; i != 0u; --i ) {
		xsshchannel* pChannel = arrChannels[i - 1u];

		testRequire((xrtSshChannelIoConsume(
			&pChannel->Io,
			XSSH_CHANNEL_IO_DATA,
			64u
		) == XSSH_OK) && xrtSshChannelCoreAdjustReady(
			&pChannel->Core
		) && (xrtSshChannelCoreAdjustLimit(
			&pChannel->Core
		) == 64u) && (xrtSshChannelCoreAdjustSendCommit(
			&pChannel->Core,
			64u
		) == XSSH_OK) && (xrtSshChannelIoReceivePrepare(
			&pChannel->Io,
			XSSH_CHANNEL_IO_DATA,
			pChannel->Core.Local,
			(xbytesview){ arrData, TEST_SSH_CHANNEL_STRESS_PACKET }
		) == XSSH_OK) && (xrtSshChannelCoreDataReceiveCommit(
			&pChannel->Core,
			pChannel->Core.Local,
			TEST_SSH_CHANNEL_STRESS_PACKET
		) == XSSH_OK) && (xrtSshChannelIoReceiveCommit(
			&pChannel->Io
		) == XSSH_OK) && (xrtSshChannelIoConsume(
			&pChannel->Io,
			XSSH_CHANNEL_IO_DATA,
			TEST_SSH_CHANNEL_STRESS_PACKET
		) == XSSH_OK), "ssh channels receive resume failed");
	}
	xrtSshChannelsClear(&Channels);
}



/* 验证 peer open、迭代和仅结束 channel 可安全删除。 */
static void testSshChannelsLifecycle(void)
{
	xsshchannelopen Open;
	xsshchannelopenfailure Failure;
	xsshchannelsiter Iterator;
	xsshchannels Channels;
	xsshchannel* pLocal = NULL;
	xsshchannel* pIncoming = NULL;
	xsshchannel* pItem;
	uint32 iLocal = UINT32_MAX;
	size_t iCount = 0u;

	memset(&Open, 0, sizeof(Open));
	Open.Type = XRT_STR_LITERAL("session");
	Open.Sender = 77u;
	Open.Window = 128u;
	Open.MaxPacket = 32u;
	testSshChannelsInit(&Channels, 3u);
	testRequire(
		(xrtSshChannelsOpen(&Channels, &pLocal) == XSSH_OK) &&
		(xrtSshChannelsAccept(&Channels, &Open, &pIncoming) == XSSH_OK) &&
		pIncoming->Incoming && (pIncoming->Core.Remote == 77u) &&
		!xrtSshChannelsRemove(&Channels, pLocal->Core.Local),
		"ssh channels lifecycle setup mismatch"
	);
	testRequire(
		xrtSshChannelsIterBegin(&Channels, &Iterator),
		"ssh channels iterator begin failed"
	);
	while ( (pItem = xrtSshChannelsIterNext(&Iterator, &iLocal)) != NULL ) {
		testRequire(
			pItem->Core.Local == iLocal,
			"ssh channels iterator key mismatch"
		);
		iCount++;
	}
	xrtSshChannelsIterEnd(&Iterator);
	testRequire(iCount == 2u, "ssh channels iterator count mismatch");

	memset(&Failure, 0, sizeof(Failure));
	Failure.Recipient = pLocal->Core.Local;
	Failure.Reason = XSSH_CHANNEL_OPEN_CONNECT_FAILED;
	testRequire(
		(xrtSshChannelCoreFailureCommit(
			&pLocal->Core,
			&Failure
		) == XSSH_OK) &&
		xrtSshChannelsRemove(&Channels, pLocal->Core.Local) &&
		(xrtSshChannelsCount(&Channels) == 1u) &&
		xrtSshChannelsDiscard(&Channels, pIncoming->Core.Local) &&
		(xrtSshChannelsCount(&Channels) == 0u),
		"ssh channels remove or discard mismatch"
	);
	xrtSshChannelsClear(&Channels);
}



int main(void)
{
	testSshChannelsOpen();
	testSshChannelsReplies();
	testSshChannelsIdWrap();
	testSshChannelsReplyStress();
	testSshChannelsBackpressure();
	testSshChannelsLifecycle();
	return 0;
}
