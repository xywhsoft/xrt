/*
 * 范例：network/tcp_stream_tour —— Stream 发送/读取/等待/流控全族
 * ----------------------------------------------------------------
 * 演示 API：
 *   【发送族】    xrtNetStreamSendVec（聚集复制）
 *                  xrtNetStreamSendRef / SendRefs（零复制 + 释放回调）
 *                  xrtNetStreamSendTake（接管 XRT 分配）
 *                  xrtNetStreamSendFile（内核文件发送，区间有界）
 *   【发送预算】  xrtNetStreamWriteLimit / Writable / Pending
 *   【读取族】    xrtNetStreamAvailable（并发快照）
 *                  xrtNetStreamBuffer / Read / Consume（仅 Worker 内，
 *                  经 xrtNetPost 投递到 Stream 所属 Worker 执行）
 *   【等待族】    xrtNetStreamWait / WaitAvailable（阻塞式）
 *                  xrtNetStreamWaitAvailableAsync（Future 式）
 *   【流控】      xrtNetStreamPause / Resume
 *   【半关闭】    xrtNetStreamShutdownWrite（对端读到 EOF）
 *   【Worker 族】 xrtNetStreamSocket / SetEvents / SetData（仅 Worker 内）
 *                  xrtNetStreamWorker / Data / Error / Stats / Local
 *   【配置】      xrtNetStreamConfigInit
 * 模块宏：XRT_MODULE_NET_TCP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/tcp_stream_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   stream-tour: config write-limit>=64k writable>=0
 *   sends: vec+ref+refs+take+file = 53 bytes verified
 *   worker-read: buffer=8 read="buf-" consume=4 socket+setevents+setdata=ok
 *   waits: read=1 available=1 available-async=1 write=1
 *   flow: pause=held resume=delivered
 *   shutdown: peer-eof=1 stats(sent=65) error=(none)
 *
 * 五种发送形态拼出 53 字节已知流（不含结尾零），对端整体核对；
 *   读取三件套（Buffer/Read/Consume）与 Socket/SetEvents/
 *   SetData 一样只能在 Stream Worker 上调用——正确入口是
 *   xrtNetPost 投递，主线程只做 Available/Wait 类跨线程观测。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define EXAMPLE_DEADLINE_US	UINT64_C(3000000)

/* 释放回调计数：SendRef/SendRefs 的零复制载荷离队时各执行一次。 */
static uint32 g_Releases = 0;

static void exampleCountRelease(ptr pContext, cbytes pData,
	size_t iSize)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
	g_Releases = g_Releases + 1u;
}

/* Worker 任务上下文。 */
typedef struct examplestreamtask {
	xnetstream* pStream;
	volatile bool bDone;
	size_t iBufferSize;
	size_t iRead;
	size_t iConsume;
	char ReadOut[8];
	bool bSocket;
	bool bSetEvents;
	bool bSetData;
} examplestreamtask;

/* 最小事件集（仅 Close），不改变拉取模式。 */
static void exampleStreamClose(xnetstream* pStream,
	xnetresult Result, const xerror* pError, ptr pData)
{
	(void)pStream;
	(void)Result;
	(void)pError;
	(void)pData;
}

/* Worker 内任务：Buffer 借用 + Read 复制 + Consume 丢弃 + Socket +
 * SetEvents + SetData——六件 Worker 专用操作一批完成。 */
static void exampleStreamTask(xnetworker* pWorker, ptr pData)
{
	examplestreamtask* pTask = (examplestreamtask*)pData;
	const xnetbuf* pBuffer;
	static xnetstreamevents s_Events = { 0 };
	static uint64 s_Tag = 0;

	(void)pWorker;
	s_Events.Close = exampleStreamClose;
	pBuffer = xrtNetStreamBuffer(pTask->pStream);
	pTask->iBufferSize = pBuffer != NULL ? xrtNetBufSize(pBuffer) : 0;
	pTask->iRead = xrtNetStreamRead(pTask->pStream,
		pTask->ReadOut, 4u);
	pTask->iConsume = xrtNetStreamConsume(pTask->pStream, 4u);
	pTask->bSocket = xrtNetStreamSocket(pTask->pStream) != NULL;
	pTask->bSetEvents = xrtNetStreamSetEvents(pTask->pStream,
		&s_Events, NULL);
	pTask->bSetData = xrtNetStreamSetData(pTask->pStream, &s_Tag);
	pTask->bDone = true;
}

static bool exampleWaitDone(volatile bool* pFlag)
{
	xdeadline iDeadline = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);

	while ( !*pFlag ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}

static bool exampleWaitStreamState(xnetstream* pStream,
	xnetstreamstate State)
{
	xdeadline iDeadline = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);

	while ( xrtNetStreamState(pStream) != State ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}

int main(void)
{
	static const char sExpected[] =
		"vec-joinref-zero-copyrefs-arefs-btake-ownedfile-block";
	/* take-owned 与 file-block 各 10 字节，不带结尾零。 */
	xnetengineconfig EngineConfig;
	xnetstreamconfig StreamConfig;
	xnetlistenconfig ListenConfig;
	xnetengine* pEngine = NULL;
	xnetlistener* pListener = NULL;
	xnetstream* pClient = NULL;
	xnetstream* pServer = NULL;
	xnetaddr Address;
	xnetref Refs[2];
	xnetspan Vec[2];
	xnetpost Post;
	examplestreamtask Task;
	xfuture* pAvailable = NULL;
	xnetstreamstats Stats;
	ptr pTake = NULL;
	xfile File;
	uint64 iFileSize = 0;
	int iResult = 1;

	/* 配置：默认值即自适应读取 + 背压水位。 */
	xrtNetStreamConfigInit(&StreamConfig);
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1;
	xrtNetListenConfigInit(&ListenConfig);
	(void)xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0);
	ListenConfig.Address = Address;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		goto Cleanup;
	}
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	if ( (pListener == NULL) ||
		 !xrtNetListenerLocal(pListener, &Address) ) {
		iResult = 2;
		goto Cleanup;
	}
	pClient = xrtNetStreamConnect(pEngine, &Address, 0,
		&StreamConfig, NULL, NULL);
	if ( (pClient == NULL) ||
		 !exampleWaitStreamState(pClient, XNET_STREAM_OPEN) ) {
		iResult = 3;
		goto Cleanup;
	}
	{
		xdeadline iDeadline = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);

		while ( (pServer = xrtNetListenerAccept(pListener)) ==
			NULL ) {
			if ( xrtDeadlineExpired(iDeadline) ) {
				iResult = 4;
				goto Cleanup;
			}
			xrtThreadYield();
		}
	}
	if ( !exampleWaitStreamState(pServer, XNET_STREAM_OPEN) ) {
		iResult = 4;
		goto Cleanup;
	}
	printf("stream-tour: config write-limit>=%s writable>=%s\n",
		xrtNetStreamWriteLimit(pClient) >= 65536u ? "64k" : "?",
		xrtNetStreamWritable(pClient) > 0u ? "0" : "?");

	/* 五种发送形态拼出 55 字节已知流。 */
	Vec[0].Data = "vec-";
	Vec[0].Size = 4;
	Vec[1].Data = "join";
	Vec[1].Size = 4;
	if ( xrtNetStreamSendVec(pClient, Vec, 2) != XNET_RESULT_OK ) {
		goto Cleanup;
	}
	if ( xrtNetStreamSendRef(pClient, "ref-zero-copy", 13,
		exampleCountRelease, &g_Releases) != XNET_RESULT_OK ) {
		goto Cleanup;
	}
	Refs[0].Data = "refs-a";
	Refs[0].Size = 6;
	Refs[0].Release = exampleCountRelease;
	Refs[0].Context = &g_Releases;
	Refs[1].Data = "refs-b";
	Refs[1].Size = 6;
	Refs[1].Release = exampleCountRelease;
	Refs[1].Context = &g_Releases;
	if ( xrtNetStreamSendRefs(pClient, Refs, 2) != XNET_RESULT_OK ) {
		goto Cleanup;
	}
	pTake = xrtMalloc(10);
	if ( pTake == NULL ) {
		goto Cleanup;
	}
	memcpy(pTake, "take-owned", 10);
	if ( xrtNetStreamSendTake(pClient, pTake, 10) ==
		 XNET_RESULT_OK ) {
		pTake = NULL;
	}
	else {
		goto Cleanup;
	}
	/* SendFile：临时文件一段已知区间。 */
	File = xrtOpen("xrt-stream-tour.tmp",
		XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE);
	if ( (File == NULL) ||
		 !xrtWriteFull(File, "file-block", 10, NULL) ) {
		iResult = 5;
		goto Cleanup;
	}
	(void)xrtClose(File);
	File = xrtOpen("xrt-stream-tour.tmp", XFILE_READ);
	if ( (File == NULL) ||
		 (xrtNetStreamSendFile(pClient, File, 0, 10) !=
		  XNET_RESULT_OK) ) {
		iResult = 6;
		goto Cleanup;
	}
	(void)xrtClose(File);
	File = NULL;

	/* 对端整体核对 55 字节。 */
	if ( !xrtNetStreamWaitAvailable(pServer, sizeof(sExpected) - 1u,
		xrtDeadlineAfter(EXAMPLE_DEADLINE_US), NULL) ) {
		iResult = 7;
		goto Cleanup;
	}
	{
		xnetbytes* pBytes = xrtNetStreamRecv(pServer,
			sizeof(sExpected) - 1u,
			xrtDeadlineAfter(EXAMPLE_DEADLINE_US), NULL);
		xbytesview View = xrtNetBytesView(pBytes);
		bool bMatch = (View.Size == sizeof(sExpected) - 1u) &&
			(memcmp(View.Data, sExpected, View.Size) == 0);

		xrtNetBytesDestroy(pBytes);
		if ( !bMatch ) {
			iResult = 8;
			goto Cleanup;
		}
	}
	printf("sends: vec+ref+refs+take+file = %zu bytes verified\n",
		sizeof(sExpected) - 1u);
	/* 发送队列排空：Pending 归零。 */
	{
		xdeadline iDeadline = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);

		while ( xrtNetStreamPending(pClient) != 0u ) {
			if ( xrtDeadlineExpired(iDeadline) ) {
				iResult = 9;
				goto Cleanup;
			}
			xrtThreadYield();
		}
	}

	/* Worker 读取族：先备好 8 字节再投递任务。 */
	if ( xrtNetStreamSend(pClient, "buf-demo", 8) != XNET_RESULT_OK ) {
		goto Cleanup;
	}
	if ( !xrtNetStreamWaitAvailable(pServer, 8u,
		xrtDeadlineAfter(EXAMPLE_DEADLINE_US), NULL) ) {
		iResult = 10;
		goto Cleanup;
	}
	memset(&Task, 0, sizeof(Task));
	Task.pStream = pServer;
	if ( !xrtNetPostInit(&Post) || !xrtNetPost(
		xrtNetStreamWorker(pServer), &Post, exampleStreamTask,
		&Task) || !exampleWaitDone(&Task.bDone) ) {
		iResult = 11;
		goto Cleanup;
	}
	printf("worker-read: buffer=%zu read=\"%.*s\" consume=%zu",
		Task.iBufferSize, (int)Task.iRead, Task.ReadOut,
		Task.iConsume);
	printf(" socket+setevents+setdata=%s\n",
		(Task.bSocket && Task.bSetEvents && Task.bSetData &&
		 (xrtNetStreamData(pServer) != NULL)) ? "ok" : "fail");
	if ( (Task.iBufferSize != 8u) || (Task.iRead != 4u) ||
		 (memcmp(Task.ReadOut, "buf-", 4) != 0) ||
		 (Task.iConsume != 4u) || !Task.bSocket ||
		 !Task.bSetEvents || !Task.bSetData ) {
		iResult = 12;
		goto Cleanup;
	}

	/* 等待族：READ 条件 / 字节数门槛 / Future 门槛 / WRITE 条件。 */
	if ( xrtNetStreamSend(pServer, "wake", 4) != XNET_RESULT_OK ) {
		goto Cleanup;
	}
	printf("waits: read=%d",
		xrtNetStreamWait(pClient, XNET_STREAM_WAIT_READ,
			xrtDeadlineAfter(EXAMPLE_DEADLINE_US), NULL) ? 1 : 0);
	printf(" available=%d",
		xrtNetStreamWaitAvailable(pClient, 4u,
			xrtDeadlineAfter(EXAMPLE_DEADLINE_US), NULL) ? 1 : 0);
	{
		/* BytesRef：接收结果可共享持有——引用与原件各销毁一次。 */
		xnetbytes* pBytes = xrtNetStreamRecv(pClient, 4,
			xrtDeadlineAfter(EXAMPLE_DEADLINE_US), NULL);
		xnetbytes* pShared = xrtNetBytesRef(pBytes);

		xrtNetBytesDestroy(pShared);
		xrtNetBytesDestroy(pBytes);
	}
	if ( xrtNetStreamSend(pServer, "more", 4) != XNET_RESULT_OK ) {
		goto Cleanup;
	}
	pAvailable = xrtNetStreamWaitAvailableAsync(pClient, 4u);
	printf(" available-async=%d",
		(pAvailable != NULL) &&
			(xrtFutureWaitFor(pAvailable, EXAMPLE_DEADLINE_US) ==
			 XWAIT_OK) ? 1 : 0);
	{
		xnetbytes* pBytes = xrtNetStreamRecv(pClient, 4,
			xrtDeadlineAfter(EXAMPLE_DEADLINE_US), NULL);

		xrtNetBytesDestroy(pBytes);
	}
	printf(" write=%d\n",
		xrtNetStreamWait(pClient, XNET_STREAM_WAIT_WRITE,
			xrtDeadlineAfter(EXAMPLE_DEADLINE_US), NULL) ? 1 : 0);

	/* 流控：Pause 后数据滞留内核，Resume 后到达。 */
	xrtNetStreamPause(pServer);
	if ( xrtNetStreamSend(pClient, "held", 4) != XNET_RESULT_OK ) {
		goto Cleanup;
	}
	{
		xdeadline iHold = xrtDeadlineAfter(200000u);
		bool bHeld = true;

		while ( xrtDeadlineExpired(iHold) == false ) {
			if ( xrtNetStreamAvailable(pServer) != 0u ) {
				bHeld = false;
				break;
			}
			xrtThreadYield();
		}
		printf("flow: pause=%s", bHeld ? "held" : "leaked");
	}
	if ( !xrtNetStreamResume(pServer) ||
		 !xrtNetStreamWaitAvailable(pServer, 4u,
			xrtDeadlineAfter(EXAMPLE_DEADLINE_US), NULL) ) {
		iResult = 13;
		goto Cleanup;
	}
	{
		xnetbytes* pBytes = xrtNetStreamRecv(pServer, 4,
			xrtDeadlineAfter(EXAMPLE_DEADLINE_US), NULL);

		xrtNetBytesDestroy(pBytes);
		printf(" resume=delivered\n");
	}

	/* 半关闭：客户端 ShutdownWrite → 服务端读到 EOF（Recv 空 + 终态）。 */
	if ( !xrtNetStreamShutdownWrite(pClient) ) {
		iResult = 14;
		goto Cleanup;
	}
	{
		xnetbytes* pBytes = xrtNetStreamRecv(pServer, 0,
			xrtDeadlineAfter(EXAMPLE_DEADLINE_US), NULL);
		xbytesview View = xrtNetBytesView(pBytes);
		int iEof = (View.Size == 0u);

		xrtNetBytesDestroy(pBytes);
		if ( !iEof ) {
			iResult = 15;
			goto Cleanup;
		}
		printf("shutdown: peer-eof=1");
	}

	/* 统计与终态错误。 */
	if ( !xrtNetStreamStats(pClient, &Stats) ||
		 (Stats.SentBytes < 63u) ) {
		iResult = 16;
		goto Cleanup;
	}
	printf(" stats(sent=%llu)",
		(unsigned long long)Stats.SentBytes);
	printf(" error=%s\n",
		xrtNetStreamError(pClient) == NULL ? "(none)" : "err");
	(void)xrtNetStreamLocal(pClient, &Address);
	(void)iFileSize;
	iResult = 0;

Cleanup:
	xrtFree(pTake);
	xrtFutureDestroy(pAvailable);
	(void)xrtFileDelete("xrt-stream-tour.tmp");
	if ( pClient != NULL ) {
		(void)xrtNetStreamAbort(pClient);
		(void)exampleWaitStreamState(pClient, XNET_STREAM_CLOSED);
	}
	if ( pServer != NULL ) {
		(void)xrtNetStreamAbort(pServer);
		(void)exampleWaitStreamState(pServer, XNET_STREAM_CLOSED);
	}
	if ( pListener != NULL ) {
		xdeadline iEnd = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);

		(void)xrtNetListenerClose(pListener);
		while ( xrtNetListenerState(pListener) !=
			XNET_LISTENER_CLOSED ) {
			if ( xrtDeadlineExpired(iEnd) ) {
				break;
			}
			xrtThreadYield();
		}
	}
	xrtNetStreamDestroy(pClient);
	xrtNetStreamDestroy(pServer);
	xrtNetListenerDestroy(pListener);
	if ( (pEngine != NULL) && !xrtNetEngineDestroy(pEngine) &&
		 (iResult == 0) ) {
		iResult = 17;
	}
	return iResult;
}
