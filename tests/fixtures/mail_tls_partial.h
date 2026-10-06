#include <xrt/detail/wait.h>
#ifndef XRT_TEST_MAIL_TLS_PARTIAL_H
#define XRT_TEST_MAIL_TLS_PARTIAL_H

/* 由调用方先包含邮件 test.h 和 test_tls.h。只读观察公开 TLS/TCP 状态。 */

typedef struct testmailpartialsocket {
	xnetstream* Tcp;
	xatomic32 Done;
	bool Success;
} testmailpartialsocket;

typedef struct testmailpartialsnapshot {
	xtlsstream* Tls;
	xatomic32 Done;
	uint64 Accepted;
	uint64 Received;
	size_t Available;
	size_t AsyncBytes;
	uint32 AsyncCount;
	bool Success;
} testmailpartialsnapshot;

/* Socket 选项在所属 Worker 上设置，不改变 Stream 的 IO 所有权。 */
static inline void testMailPartialSocketTask(xnetworker* pWorker, ptr pData)
{
	testmailpartialsocket* pJob = (testmailpartialsocket*)pData;
	xnetsocket Socket = xrtNetStreamSocket(pJob->Tcp);
	(void)pWorker;
	pJob->Success = Socket != NULL &&
		xrtNetSocketSet(Socket, XNET_OPTION_SEND_BUFFER, 4096) &&
		xrtNetSocketSet(Socket, XNET_OPTION_RECEIVE_BUFFER, 4096);
	xrtAtomic32Store(&pJob->Done, 1u, XMEMORY_RELEASE);
}

static inline void testMailPartialSmallSocket(xnetstream* pTcp, double Deadline)
{
	testmailpartialsocket Job;
	xnetworker* pWorker = xrtNetStreamWorker(pTcp);
	Job.Tcp = pTcp;
	Job.Success = false;
	xrtAtomic32Init(&Job.Done, 0u);
	testRequire(pWorker != NULL && xrtNetEnginePost(
		xrtNetWorkerEngine(pWorker), xrtNetWorkerIndex(pWorker),
		testMailPartialSocketTask, &Job), "mail TLS socket configuration post failed");
	while ( !xrtAtomic32Load(&Job.Done, XMEMORY_ACQUIRE) ) {
		testRequire(!__xrtWaitExpired(Deadline), "mail TLS socket configuration stalled");
		xrtThreadYield();
	}
	testRequire(Job.Success, "mail TLS socket buffer configuration failed");
}

static inline bool testMailPartialWaitFlag(const xatomic32* pFlag, double Deadline)
{
	while ( !xrtAtomic32Load(pFlag, XMEMORY_ACQUIRE) ) {
		if ( __xrtWaitExpired(Deadline) ) return false;
		xrtThreadYield();
	}
	return true;
}

/* 在所属 Worker 上读取组合快照，避免 TCP 已发送计数和两级待发计数之间的竞争。 */
static inline void testMailPartialSnapshotTask(xnetworker* pWorker, ptr pData)
{
	testmailpartialsnapshot* pSnapshot = (testmailpartialsnapshot*)pData;
	xnetstreamstats Stats;
	(void)pWorker;
	pSnapshot->Success = xrtNetStreamStats(xrtTlsStreamTransport(pSnapshot->Tls), &Stats);
	if ( pSnapshot->Success ) {
		pSnapshot->Accepted = Stats.SentBytes + xrtTlsStreamPending(pSnapshot->Tls);
		pSnapshot->Received = Stats.ReceivedBytes;
		pSnapshot->Available = xrtTlsStreamAvailable(pSnapshot->Tls);
		pSnapshot->AsyncBytes = xrtTlsStreamAsyncBytes(pSnapshot->Tls);
		pSnapshot->AsyncCount = xrtTlsStreamAsyncCount(pSnapshot->Tls);
	}
	xrtAtomic32Store(&pSnapshot->Done, 1u, XMEMORY_RELEASE);
}

static inline bool testMailPartialSnapshot(xtlsstream* pTls, double Deadline,
	testmailpartialsnapshot* pSnapshot)
{
	xnetworker* pWorker = xrtNetStreamWorker(xrtTlsStreamTransport(pTls));
	memset(pSnapshot, 0, sizeof(*pSnapshot));
	pSnapshot->Tls = pTls;
	xrtAtomic32Init(&pSnapshot->Done, 0u);
	if ( pWorker == NULL || !xrtNetEnginePost(xrtNetWorkerEngine(pWorker),
		xrtNetWorkerIndex(pWorker), testMailPartialSnapshotTask, pSnapshot) ) return false;
	/* Job 借用栈存储，不能在回调退休前返回。测试总截止时间负责拒绝停滞。 */
	testRequire(testMailPartialWaitFlag(&pSnapshot->Done, Deadline), "mail TLS snapshot task stalled");
	return pSnapshot->Success;
}

/* 第二轮的密文增加且唯一 Future 仍占完整预算，才算同次发送部分推进。 */
static inline bool testMailPartialWaitProgress(xtlsstream* pTls, uint64 PrefixAccepted,
	size_t AsyncBytes, const xatomic32* pReturned, double Deadline)
{
	while ( !__xrtWaitExpired(Deadline) ) {
		testmailpartialsnapshot Snapshot;
		if ( !testMailPartialSnapshot(pTls, Deadline, &Snapshot) ) return false;
		if ( Snapshot.AsyncBytes == AsyncBytes && Snapshot.AsyncCount == 1u &&
			Snapshot.Accepted > PrefixAccepted ) return true;
		if ( xrtAtomic32Load(pReturned, XMEMORY_ACQUIRE) ) return false;
		xrtSleep(1);
	}
	return false;
}

/* 完整密文已收到、明文已取走且新接收仍待定时，客户端正在等缺少的线路结尾。
 * 调用方须在失败返回后另核对协议层待解析字节，确认它确实消费了该前缀。 */
static inline bool testMailPartialWaitRead(xtlsstream* pTls, uint64 ExpectedReceived,
	const xatomic32* pReturned, double Deadline)
{
	while ( !__xrtWaitExpired(Deadline) ) {
		testmailpartialsnapshot Snapshot;
		if ( !testMailPartialSnapshot(pTls, Deadline, &Snapshot) ) return false;
		if ( Snapshot.Received >= ExpectedReceived && Snapshot.Available == 0 &&
			Snapshot.AsyncCount == 1u && Snapshot.AsyncBytes == 0 ) return true;
		if ( xrtAtomic32Load(pReturned, XMEMORY_ACQUIRE) ) return false;
		xrtSleep(1);
	}
	return false;
}

#endif
