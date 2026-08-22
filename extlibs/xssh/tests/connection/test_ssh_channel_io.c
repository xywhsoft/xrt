#include "../test.h"



typedef struct testsshchanneliorelease {
	size_t Count;
	size_t Bytes;
} testsshchanneliorelease;



/* 建立数据面已打开、窗口尺寸可预测的测试 channel。 */
static void testSshChannelIoOpen(
	xsshchannelcore* pChannel,
	uint32 iReceiveWindow,
	uint32 iReceiveMaxPacket,
	uint32 iSendWindow,
	uint32 iSendMaxPacket
)
{
	xsshchannelconfirmation Confirmation;

	testRequire(xrtSshChannelCoreOpenInit(
		pChannel,
		7u,
		iReceiveWindow,
		iReceiveMaxPacket,
		4u
	), "ssh channel I/O local open failed");
	memset(&Confirmation, 0, sizeof(Confirmation));
	Confirmation.Recipient = 7u;
	Confirmation.Sender = 41u;
	Confirmation.Window = iSendWindow;
	Confirmation.MaxPacket = iSendMaxPacket;
	testRequire(xrtSshChannelCoreConfirmationCommit(
		pChannel,
		&Confirmation
	) == XSSH_OK, "ssh channel I/O confirmation failed");
}



/* 统计带引用发送块只释放一次。 */
static void testSshChannelIoRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	testsshchanneliorelease* pRelease =
		(testsshchanneliorelease*)pContext;

	(void)pData;
	pRelease->Count++;
	pRelease->Bytes += iSize;
}



/* 验证发送共享预算、远端窗口分片以及提交和回滚边界。 */
static void testSshChannelIoSend(void)
{
	static const unsigned char arrData[] = "abcdefgh";
	static const unsigned char arrError[] = "WXYZ";
	unsigned char arrPayload[64];
	xsshchannelioconfig Config;
	xsshchanneldata Data;
	xsshchannelextendeddata Extended;
	xsshchannelcore Channel;
	xsshchannelio Io;
	xsshwriter Writer;
	xbytesview Payload;

	testSshChannelIoOpen(&Channel, 16u, 8u, 12u, 5u);
	xrtSshChannelIoConfigInit(&Config);
	Config.ReceiveLimit = 16u;
	Config.SendLimit = 12u;
	testRequire(xrtSshChannelIoInit(
		&Io,
		NULL,
		&Channel,
		&Config
	), "ssh channel I/O send init failed");
	testRequire((xrtSshChannelIoWrite(
		&Io,
		XSSH_CHANNEL_IO_DATA,
		arrData,
		6u
	) == XSSH_OK) && (xrtSshChannelIoWriteBorrow(
		&Io,
		XSSH_CHANNEL_IO_DATA,
		arrData + 6u,
		2u
	) == XSSH_OK) && (xrtSshChannelIoWrite(
		&Io,
		XSSH_CHANNEL_IO_STDERR,
		arrError,
		4u
	) == XSSH_OK) && (xrtSshChannelIoWritable(&Io) == 0u) &&
		(xrtSshChannelIoWrite(
			&Io,
			XSSH_CHANNEL_IO_DATA,
			"x",
			1u
		) == XSSH_ERROR_SPACE) &&
		(xrtSshChannelIoQueued(&Io, XSSH_CHANNEL_IO_DATA) == 8u) &&
		(xrtSshChannelIoQueued(&Io, XSSH_CHANNEL_IO_STDERR) == 4u) &&
		(xrtSshChannelIoSendLimit(&Io, XSSH_CHANNEL_IO_DATA) == 5u),
		"ssh channel I/O send queue budget failed");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelIoSendPrepare(
			&Io,
			XSSH_CHANNEL_IO_DATA,
			&Writer,
			&Payload
		) == XSSH_OK) &&
		(Payload.Size == 14u) &&
		(xrtSshChannelDataRead(Payload, &Data) == XSSH_OK) &&
		(Data.Recipient == 41u) && testSshBytesEqual(
			Data.Data,
			(xbytesview){ arrData, 5u }
		) && (xrtSshChannelIoSendCommit(&Io) == XSSH_ERROR_STATE) &&
		(xrtSshChannelCoreDataSendCommit(&Channel, 5u) == XSSH_OK) &&
		(xrtSshChannelIoSendCommit(&Io) == XSSH_OK) &&
		(Channel.Window.SendWindow == 7u) &&
		(xrtSshChannelIoQueued(&Io, XSSH_CHANNEL_IO_DATA) == 3u),
		"ssh channel I/O send commit failed");

	/* Abort 不扣减远端窗口，也不消费动态队首。 */
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelIoSendPrepare(
			&Io,
			XSSH_CHANNEL_IO_DATA,
			&Writer,
			&Payload
		) == XSSH_OK) &&
		(Payload.Size == 10u) &&
		(xrtSshChannelIoWrite(
			&Io,
			XSSH_CHANNEL_IO_DATA,
			"q",
			1u
		) == XSSH_ERROR_STATE) &&
		(xrtSshChannelIoSendAbort(&Io) == XSSH_OK) &&
		(Channel.Window.SendWindow == 7u) &&
		(xrtSshChannelIoQueued(&Io, XSSH_CHANNEL_IO_DATA) == 3u),
		"ssh channel I/O send abort failed");

	/* Writer 空间只影响本次切片，不改变队列。 */
	testRequire(xrtSshWriterInit(&Writer, arrPayload, 9u) &&
		(xrtSshChannelIoSendPrepare(
			&Io,
			XSSH_CHANNEL_IO_DATA,
			&Writer,
			&Payload
		) == XSSH_ERROR_SPACE) && (Writer.Size == 0u) &&
		(xrtSshChannelIoQueued(&Io, XSSH_CHANNEL_IO_DATA) == 3u),
		"ssh channel I/O writer space changed queue");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshChannelIoSendPrepare(
			&Io,
			XSSH_CHANNEL_IO_STDERR,
			&Writer,
			&Payload
		) == XSSH_OK) &&
		(xrtSshChannelExtendedDataRead(Payload, &Extended) == XSSH_OK) &&
		(Extended.Recipient == 41u) &&
		(Extended.Type == XSSH_CHANNEL_EXTENDED_DATA_STDERR) &&
		testSshBytesEqual(
			Extended.Data,
			(xbytesview){ arrError, sizeof(arrError) - 1u }
		) && (xrtSshChannelCoreDataSendCommit(&Channel, 4u) == XSSH_OK) &&
		(xrtSshChannelIoSendCommit(&Io) == XSSH_OK) &&
		(xrtSshChannelIoQueued(&Io, XSSH_CHANNEL_IO_STDERR) == 0u),
		"ssh channel I/O stderr send failed");
	xrtSshChannelIoClear(&Io);
	xrtSshChannelCoreClear(&Channel);
}



/* 验证接收预分配、可读发布、零复制消费和窗口返还计数。 */
static void testSshChannelIoReceive(void)
{
	static const unsigned char arrData[] = "hello";
	static const unsigned char arrError[] = "err";
	unsigned char arrRead[8];
	xsshchannelioconfig Config;
	xsshchannelcore Channel;
	xsshchannelio Io;
	const xnetbuf* pBuffer;
	xnetspan Span;
	size_t iRead = SIZE_MAX;
	uint32 iAdjust;

	testSshChannelIoOpen(&Channel, 16u, 8u, 12u, 5u);
	xrtSshChannelIoConfigInit(&Config);
	Config.ReceiveLimit = 16u;
	Config.SendLimit = 0u;
	testRequire(xrtSshChannelIoInit(
		&Io,
		NULL,
		&Channel,
		&Config
	), "ssh channel I/O receive init failed");
	testRequire((xrtSshChannelIoReceivePrepare(
		&Io,
		XSSH_CHANNEL_IO_DATA,
		7u,
		(xbytesview){ arrData, sizeof(arrData) - 1u }
	) == XSSH_OK) &&
		(xrtSshChannelIoReadable(&Io, XSSH_CHANNEL_IO_DATA) == 0u) &&
		(xrtSshChannelIoReceiveCommit(&Io) == XSSH_ERROR_STATE) &&
		(xrtSshChannelCoreDataReceiveCommit(
			&Channel,
			7u,
			(uint32)(sizeof(arrData) - 1u)
		) == XSSH_OK) &&
		(xrtSshChannelIoReceiveCommit(&Io) == XSSH_OK) &&
		(xrtSshChannelIoReadable(&Io, XSSH_CHANNEL_IO_DATA) == 5u),
		"ssh channel I/O receive commit failed");
	testRequire((xrtSshChannelIoReceivePrepare(
		&Io,
		XSSH_CHANNEL_IO_STDERR,
		7u,
		(xbytesview){ arrError, sizeof(arrError) - 1u }
	) == XSSH_OK) &&
		(xrtSshChannelCoreDataReceiveCommit(
			&Channel,
			7u,
			(uint32)(sizeof(arrError) - 1u)
		) == XSSH_OK) &&
		(xrtSshChannelIoReceiveCommit(&Io) == XSSH_OK) &&
		(Channel.Window.ReceiveWindow == 8u) &&
		(Channel.Window.ReceiveBuffered == 8u),
		"ssh channel I/O stderr receive failed");

	/* 未提交接收可以释放预分配，不影响已发布数据。 */
	testRequire((xrtSshChannelIoReceivePrepare(
		&Io,
		XSSH_CHANNEL_IO_DATA,
		7u,
		(xbytesview){ (const unsigned char*)"z", 1u }
	) == XSSH_OK) &&
		(xrtSshChannelIoRead(
			&Io,
			XSSH_CHANNEL_IO_DATA,
			arrRead,
			sizeof(arrRead),
			&iRead
		) == XSSH_ERROR_STATE) &&
		(xrtSshChannelIoReceiveAbort(&Io) == XSSH_OK) &&
		(Channel.Window.ReceiveBuffered == 8u),
		"ssh channel I/O receive abort failed");
	testRequire((xrtSshChannelIoRead(
		&Io,
		XSSH_CHANNEL_IO_DATA,
		arrRead,
		2u,
		&iRead
	) == XSSH_OK) && (iRead == 2u) &&
		(memcmp(arrRead, "he", 2u) == 0) &&
		(Channel.Window.ReceiveBuffered == 6u) &&
		(Channel.Window.ReceivePending == 2u),
		"ssh channel I/O copied read failed");
	pBuffer = xrtSshChannelIoReadBuffer(&Io, XSSH_CHANNEL_IO_DATA);
	testRequire((pBuffer != NULL) && xrtNetBufFront(pBuffer, &Span) &&
		(Span.Size == 3u) && (memcmp(Span.Data, "llo", 3u) == 0) &&
		(xrtSshChannelIoConsume(
			&Io,
			XSSH_CHANNEL_IO_DATA,
			3u
		) == XSSH_OK) && (xrtSshChannelIoRead(
			&Io,
			XSSH_CHANNEL_IO_STDERR,
			arrRead,
			sizeof(arrRead),
			&iRead
		) == XSSH_OK) && (iRead == 3u) &&
		(memcmp(arrRead, arrError, 3u) == 0) &&
		(Channel.Window.ReceiveBuffered == 0u) &&
		(Channel.Window.ReceivePending == 8u) &&
		xrtSshChannelCoreAdjustReady(&Channel),
		"ssh channel I/O zero-copy consume failed");
	iAdjust = xrtSshChannelCoreAdjustLimit(&Channel);
	testRequire((iAdjust == 8u) &&
		(xrtSshChannelCoreAdjustSendCommit(
			&Channel,
			iAdjust
		) == XSSH_OK) && (Channel.Window.ReceiveWindow == 16u) &&
		(Channel.Window.ReceivePending == 0u),
		"ssh channel I/O window return failed");
	xrtSshChannelIoClear(&Io);
	xrtSshChannelCoreClear(&Channel);
}



/* 验证复制、借用、接管、引用和缓冲移动共享同一硬预算与释放语义。 */
static void testSshChannelIoOwnership(void)
{
	static const unsigned char arrBorrow[] = "aa";
	static const unsigned char arrReference[] = "cc";
	testsshchanneliorelease Release;
	xsshchannelioconfig Config;
	xsshchannelcore Channel;
	xnetbufpool* pPool;
	xnetbuf Buffer;
	xsshchannelio Io;
	bytes pTaken;

	memset(&Release, 0, sizeof(Release));
	pPool = xrtNetBufPoolCreate(NULL);
	testRequire(pPool != NULL, "ssh channel I/O pool create failed");
	testSshChannelIoOpen(&Channel, 16u, 8u, 100u, 100u);
	xrtSshChannelIoConfigInit(&Config);
	Config.ReceiveLimit = 16u;
	Config.SendLimit = 8u;
	testRequire(xrtSshChannelIoInit(
		&Io,
		pPool,
		&Channel,
		&Config
	) && xrtNetBufInit(&Buffer, pPool) &&
		xrtNetBufAppend(&Buffer, "dd", 2u),
		"ssh channel I/O ownership init failed");
	pTaken = (bytes)xrtMalloc(2u);
	testRequire(pTaken != NULL, "ssh channel I/O take allocation failed");
	memcpy(pTaken, "bb", 2u);
	testRequire((xrtSshChannelIoWriteBorrow(
		&Io,
		XSSH_CHANNEL_IO_DATA,
		arrBorrow,
		2u
	) == XSSH_OK) && (xrtSshChannelIoWriteTake(
		&Io,
		XSSH_CHANNEL_IO_DATA,
		pTaken,
		2u
	) == XSSH_OK) && (xrtSshChannelIoWriteRef(
		&Io,
		XSSH_CHANNEL_IO_STDERR,
		arrReference,
		2u,
		testSshChannelIoRelease,
		&Release
	) == XSSH_OK) && (xrtSshChannelIoWriteBuffer(
		&Io,
		XSSH_CHANNEL_IO_STDERR,
		&Buffer
	) == XSSH_OK) && xrtNetBufEmpty(&Buffer) &&
		(xrtSshChannelIoWritable(&Io) == 0u) &&
		(xrtSshChannelIoWriteBorrow(
			&Io,
			XSSH_CHANNEL_IO_DATA,
			NULL,
			0u
		) == XSSH_OK),
		"ssh channel I/O ownership queue failed");
	xrtNetBufClear(&Buffer);
	xrtSshChannelIoClear(&Io);
	testRequire((Release.Count == 1u) && (Release.Bytes == 2u) &&
		xrtNetBufPoolDestroy(pPool),
		"ssh channel I/O ownership release failed");
	xrtSshChannelCoreClear(&Channel);
}



/* 验证配置不能声明小于已通告接收窗口的实际缓冲预算。 */
static void testSshChannelIoBoundary(void)
{
	xsshchannelioconfig Config;
	xsshchannelcore Channel;
	xsshchannelio Io;

	testSshChannelIoOpen(&Channel, 16u, 8u, 12u, 5u);
	xrtSshChannelIoConfigInit(&Config);
	Config.ReceiveLimit = 15u;
	testRequire(!xrtSshChannelIoInit(
		&Io,
		NULL,
		&Channel,
		&Config
	), "ssh channel I/O accepted undersized receive budget");
	Config.ReceiveLimit = 16u;
	Config.SendLimit = 1u;
	testRequire(xrtSshChannelIoInit(
		&Io,
		NULL,
		&Channel,
		&Config
	) && (xrtSshChannelIoReceivePrepare(
		&Io,
		XSSH_CHANNEL_IO_DATA,
		8u,
		(xbytesview){ (const unsigned char*)"bad", 3u }
	) == XSSH_ERROR_PROTOCOL) &&
		(xrtSshChannelIoWrite(
			&Io,
			XSSH_CHANNEL_IO_DATA,
			"ab",
			2u
		) == XSSH_ERROR_SPACE),
		"ssh channel I/O boundary checks failed");
	printf("channel-io=%zu default-limit=%u\n",
		sizeof(Io), XSSH_CHANNEL_IO_LIMIT_DEFAULT);
	xrtSshChannelIoClear(&Io);
	xrtSshChannelCoreClear(&Channel);
}



/* 运行动态 channel 收发、所有权和事务边界测试。 */
int main(void)
{
	testSshChannelIoSend();
	testSshChannelIoReceive();
	testSshChannelIoOwnership();
	testSshChannelIoBoundary();
	return 0;
}
