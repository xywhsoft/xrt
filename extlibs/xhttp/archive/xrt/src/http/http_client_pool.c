#include "../internal/xrt_http_client_runtime.h"



#if defined(XRT_FEATURE_HTTP_CLIENT_POOL)

#define XRT_HTTP_POOL_IDLE_DEFAULT 128u
#define XRT_HTTP_POOL_IDLE_ORIGIN_DEFAULT 8u
#define XRT_HTTP_POOL_IDLE_TIMEOUT_DEFAULT UINT64_C(90000000)
#define XRT_HTTP_POOL_SHARDS_MAX 32u



/* 空闲项离开 LRU 后仍可等待所属 Worker 上已经排队的关闭事件。 */
typedef enum __xrt_http_client_idle_state {
	XRT_HTTP_IDLE_LISTED = 0,
	XRT_HTTP_IDLE_TAKEN,
	XRT_HTTP_IDLE_CLOSE_QUEUED,
	XRT_HTTP_IDLE_CLOSING,
	XRT_HTTP_IDLE_CLOSED,
	XRT_HTTP_IDLE_USED
} __xrt_http_client_idle_state;



/* Origin 描述目标与代理路径；TLS 策略由所属 Client 固定。 */
struct __xrt_http_client_origin {
	__xrt_http_client_origin* Next;
	__xrt_http_client_pool_shard* Shard;
	str Host;
	size_t HostSize;
	#if defined(XRT_FEATURE_HTTP_CLIENT_PROXY)
		xnetproxy* Proxy;
	#endif
	uint16 Port;
	size_t Connections;
	size_t Idle;
	size_t Waiting;
	bool Secure;
};



/* 每条空闲连接持有一个 Client 引用和一个传输调用方引用。 */
struct __xrt_http_client_idle {
	__xrt_http_client_idle* Previous;
	__xrt_http_client_idle* Next;
	xhttpclient* Client;
	__xrt_http_client_origin* Origin;
	__xrt_http_client_pool_shard* Shard;
	xnetworker* Worker;
	xnetstream* Tcp;
	#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
		xtlsstream* Tls;
	#endif
	xdeadline Deadline;
	__xrt_http_client_idle_state State;
	bool Released;
};



/* 每个分片独立拥有 Origin 索引、等待 FIFO、空闲 LRU 和清扫 Timer。 */
struct __xrt_http_client_pool_shard {
	xmutex Lock;
	xhttpclient* Client;
	__xrt_http_client_origin* Origins;
	__xrt_http_client_idle* IdleHead;
	__xrt_http_client_idle* IdleTail;
	xhttpcall* WaitHead;
	xhttpcall* WaitTail;
	size_t Connections;
	size_t Idle;
	size_t Closing;
	size_t Waiting;
	uint64 Timer;
	uint32 Index;
	bool Ready;
};



/* 一次加锁阶段收集待投递 Call 和待关闭连接，锁外执行回调网络操作。 */
typedef struct __xrt_http_pool_batch {
	xhttpcall* CallHead;
	xhttpcall* CallTail;
	__xrt_http_client_idle* CloseHead;
	__xrt_http_client_idle* CloseTail;
	xhttpclient* DispatchClient;
	uint32 DispatchStart;
} __xrt_http_pool_batch;



/* 唯一空闲清扫 Timer 的完成入口。 */
static void __xrtHttpPoolTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
);



/* Origin 路由使用稳定的 ASCII 小写哈希，同一 Origin 始终进入同一分片。 */
static uint64 __xrtHttpPoolRouteHash(const xhttpcall* pCall)
{
	const unsigned char* pHost =
		(const unsigned char*)pCall->Host;
	uint64 iHash = UINT64_C(1469598103934665603);
	uintptr_t iProxy = 0;

	while ( *pHost != 0 ) {
		unsigned char iValue = *pHost++;

		if ( (iValue >= 'A') && (iValue <= 'Z') ) {
			iValue = (unsigned char)(iValue + ('a' - 'A'));
		}
		iHash = (iHash ^ iValue) * UINT64_C(1099511628211);
	}
	#if defined(XRT_FEATURE_HTTP_CLIENT_PROXY)
		iProxy = (uintptr_t)pCall->Proxy;
	#endif
	iHash = (iHash ^ pCall->Port) * UINT64_C(1099511628211);
	iHash = (iHash ^ (pCall->Secure ? 1u : 0u)) *
		UINT64_C(1099511628211);
	iHash = (iHash ^ (uint64)iProxy) * UINT64_C(1099511628211);
	return iHash;
}



/* 返回 Call 路由所属分片，并保存索引供取消早期快速定位。 */
static __xrt_http_client_pool_shard* __xrtHttpPoolShardCall(
	xhttpcall* pCall
)
{
	xhttpclient* pClient = pCall->Client;
	uint32 iIndex = (uint32)(
		__xrtHttpPoolRouteHash(pCall) %
		pClient->PoolShardCount
	);

	pCall->PoolShardIndex = iIndex;
	return &pClient->PoolShards[iIndex];
}



/* 在有限全局配额中原子保留一项；零限制表示不设上限。 */
static bool __xrtHttpPoolCountReserve(
	xatomic64* pCount,
	size_t iLimit
)
{
	uint64 iCurrent = xrtAtomic64Load(
		pCount,
		XMEMORY_RELAXED
	);

	for ( ;; ) {
		uint64 iExpected;

		if ( (iLimit != 0) &&
			(iCurrent >= (uint64)iLimit) ) {
			return false;
		}
		if ( iCurrent == UINT64_MAX ) {
			return false;
		}
		iExpected = iCurrent;
		if ( xrtAtomic64CompareExchange(
			pCount,
			&iExpected,
			iCurrent + 1u,
			XMEMORY_ACQ_REL,
			XMEMORY_RELAXED
		) ) {
			return true;
		}
		iCurrent = iExpected;
	}
}



/* 释放一项已经成功取得的全局配额。 */
static void __xrtHttpPoolCountRelease(xatomic64* pCount)
{
	(void)xrtAtomic64FetchSub(
		pCount,
		1,
		XMEMORY_ACQ_REL
	);
}



/* 设置连接池公开操作的结构化错误。 */
static void __xrtHttpPoolSetError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pError = __xrtHttpClientErrorCreate(
		Kind,
		XHTTP_CLIENT_ERROR_POOL,
		sOperation,
		sMessage,
		NULL
	);

	xrtSetError(pError);
	xrtErrorFree(pError);
}



/* 查找与 Call 路由完全一致的 Origin。 */
static __xrt_http_client_origin* __xrtHttpPoolOriginFind(
	__xrt_http_client_pool_shard* pShard,
	const xhttpcall* pCall,
	xstrview Host
)
{
	__xrt_http_client_origin** ppOrigin = &pShard->Origins;

	while ( *ppOrigin != NULL ) {
		__xrt_http_client_origin* pOrigin = *ppOrigin;

		if ( (pOrigin->Port == pCall->Port) &&
			(pOrigin->Secure == pCall->Secure) &&
			#if defined(XRT_FEATURE_HTTP_CLIENT_PROXY)
				(pOrigin->Proxy == pCall->Proxy) &&
			#endif
			__xrtHttpHostEqual(
				(xstrview) {
					pOrigin->Host,
					pOrigin->HostSize
				},
				Host
			) ) {
			if ( ppOrigin != &pShard->Origins ) {
				*ppOrigin = pOrigin->Next;
				pOrigin->Next = pShard->Origins;
				pShard->Origins = pOrigin;
			}
			return pOrigin;
		}
		ppOrigin = &pOrigin->Next;
	}
	return NULL;
}



/* 建立由 Client 拥有的 Origin 键。 */
static __xrt_http_client_origin* __xrtHttpPoolOriginCreate(
	__xrt_http_client_pool_shard* pShard,
	const xhttpcall* pCall,
	xstrview Host
)
{
	__xrt_http_client_origin* pOrigin;

	if ( Host.Size == SIZE_MAX ) {
		__xrtHttpPoolSetError(
			XERR_RANGE,
			"create-http-origin",
			"HTTP origin host is too large"
		);
		return NULL;
	}
	pOrigin = (__xrt_http_client_origin*)xrtCalloc(
		1,
		sizeof(*pOrigin)
	);
	if ( pOrigin == NULL ) {
		return NULL;
	}
	pOrigin->Host = (str)xrtMalloc(Host.Size + 1u);
	if ( pOrigin->Host == NULL ) {
		xrtFree(pOrigin);
		return NULL;
	}
	memcpy(pOrigin->Host, Host.Data, Host.Size);
	pOrigin->Host[Host.Size] = '\0';
	pOrigin->HostSize = Host.Size;
	pOrigin->Shard = pShard;
	#if defined(XRT_FEATURE_HTTP_CLIENT_PROXY)
		if ( pCall->Proxy != NULL ) {
			pOrigin->Proxy = xrtNetProxyRetain(
				pCall->Proxy
			);
			if ( pOrigin->Proxy == NULL ) {
				xrtFree(pOrigin->Host);
				xrtFree(pOrigin);
				return NULL;
			}
		}
	#endif
	pOrigin->Port = pCall->Port;
	pOrigin->Secure = pCall->Secure;
	pOrigin->Next = pShard->Origins;
	pShard->Origins = pOrigin;
	return pOrigin;
}



/* 返回已有 Origin，缺失时按需建立。 */
static __xrt_http_client_origin* __xrtHttpPoolOriginGet(
	__xrt_http_client_pool_shard* pShard,
	const xhttpcall* pCall
)
{
	xstrview Host = {
		pCall->Host,
		strlen(pCall->Host)
	};
	__xrt_http_client_origin* pOrigin =
		__xrtHttpPoolOriginFind(pShard, pCall, Host);

	if ( pOrigin == NULL ) {
		pOrigin = __xrtHttpPoolOriginCreate(
			pShard,
			pCall,
			Host
		);
	}
	return pOrigin;
}



/* 删除已经没有连接和等待者的 Origin。 */
static void __xrtHttpPoolOriginPrune(
	__xrt_http_client_pool_shard* pShard,
	__xrt_http_client_origin* pOrigin
)
{
	__xrt_http_client_origin** ppOrigin;

	if ( (pOrigin == NULL) ||
		(pOrigin->Connections != 0) ||
		(pOrigin->Waiting != 0) ) {
		return;
	}
	ppOrigin = &pShard->Origins;
	while ( (*ppOrigin != NULL) &&
		(*ppOrigin != pOrigin) ) {
		ppOrigin = &(*ppOrigin)->Next;
	}
	if ( *ppOrigin == pOrigin ) {
		*ppOrigin = pOrigin->Next;
		#if defined(XRT_FEATURE_HTTP_CLIENT_PROXY)
			xrtNetProxyRelease(pOrigin->Proxy);
		#endif
		xrtFree(pOrigin->Host);
		xrtFree(pOrigin);
	}
}



/* 判断 Client 与 Origin 的连接硬上限是否都仍有容量。 */
static bool __xrtHttpPoolCanOpen(
	const __xrt_http_client_pool_shard* pShard,
	const __xrt_http_client_origin* pOrigin
)
{
	const xhttpclient* pClient = pShard->Client;
	const xhttpclientpoolconfig* pConfig =
		&pClient->Config.Pool;

	return ((pConfig->MaxConnectionsPerOrigin == 0) ||
			(pOrigin->Connections <
			 pConfig->MaxConnectionsPerOrigin));
}



/* 判断 Client 与 Origin 的等待硬上限是否都仍有容量。 */
static bool __xrtHttpPoolCanWait(
	const __xrt_http_client_pool_shard* pShard,
	const __xrt_http_client_origin* pOrigin
)
{
	const xhttpclient* pClient = pShard->Client;
	const xhttpclientpoolconfig* pConfig =
		&pClient->Config.Pool;

	return ((pConfig->MaxWaitingPerOrigin == 0) ||
			(pOrigin->Waiting <
			 pConfig->MaxWaitingPerOrigin));
}



/*
	判断释放当前连接后指定 Origin 能否取得它留下的配额。
	只计算假设状态，不提前修改任何连接或 Origin 计数。
*/
static bool __xrtHttpPoolCanOpenAfterRelease(
	const __xrt_http_client_pool_shard* pShard,
	const __xrt_http_client_origin* pOrigin,
	const __xrt_http_client_origin* pReleased
)
{
	const xhttpclient* pClient = pShard->Client;
	const xhttpclientpoolconfig* pConfig =
		&pClient->Config.Pool;
	size_t iOriginConnections = pOrigin->Connections;

	if ( (pOrigin == pReleased) &&
		(iOriginConnections != 0) ) {
		iOriginConnections--;
	}
	return ((pConfig->MaxConnectionsPerOrigin == 0) ||
			(iOriginConnections <
			 pConfig->MaxConnectionsPerOrigin));
}



/* 把 Call 追加到锁外投递批次。 */
static void __xrtHttpPoolBatchCall(
	__xrt_http_pool_batch* pBatch,
	xhttpcall* pCall
)
{
	pCall->PoolPrevious = NULL;
	pCall->PoolNext = NULL;
	if ( pBatch->CallTail == NULL ) {
		pBatch->CallHead = pCall;
	} else {
		pBatch->CallTail->PoolNext = pCall;
	}
	pBatch->CallTail = pCall;
}



/* 把已经摘除的空闲项追加到锁外关闭批次。 */
static void __xrtHttpPoolBatchClose(
	__xrt_http_pool_batch* pBatch,
	__xrt_http_client_idle* pIdle
)
{
	pIdle->Previous = NULL;
	pIdle->Next = NULL;
	if ( pBatch->CloseTail == NULL ) {
		pBatch->CloseHead = pIdle;
	} else {
		pBatch->CloseTail->Next = pIdle;
	}
	pBatch->CloseTail = pIdle;
}



/* 记录本批次释放了全局连接配额，锁外从相邻分片开始重新分发。 */
static void __xrtHttpPoolBatchReleased(
	__xrt_http_pool_batch* pBatch,
	__xrt_http_client_pool_shard* pShard
)
{
	pBatch->DispatchClient = pShard->Client;
	pBatch->DispatchStart = (
		pShard->Index + 1u
	) % pShard->Client->PoolShardCount;
}



/* 从全局等待队列摘除一个 Call。 */
static void __xrtHttpPoolWaitRemove(
	__xrt_http_client_pool_shard* pShard,
	xhttpcall* pCall
)
{
	if ( !pCall->PoolWaiting ) {
		return;
	}
	if ( pCall->PoolPrevious == NULL ) {
		pShard->WaitHead = pCall->PoolNext;
	} else {
		pCall->PoolPrevious->PoolNext =
			pCall->PoolNext;
	}
	if ( pCall->PoolNext == NULL ) {
		pShard->WaitTail = pCall->PoolPrevious;
	} else {
		pCall->PoolNext->PoolPrevious =
			pCall->PoolPrevious;
	}
	pCall->PoolPrevious = NULL;
	pCall->PoolNext = NULL;
	pCall->PoolWaiting = false;
	pShard->Waiting--;
	__xrtHttpPoolCountRelease(&pShard->Client->PoolWaiting);
	__xrtHttpPoolCountRelease(&pShard->Client->PoolLive);
	pCall->PoolOrigin->Waiting--;
}



/* 把 Call 追加到全局有界等待队列。 */
static void __xrtHttpPoolWaitAppend(
	__xrt_http_client_pool_shard* pShard,
	xhttpcall* pCall
)
{
	xhttpclient* pClient = pShard->Client;

	pCall->PoolPrevious = pShard->WaitTail;
	pCall->PoolNext = NULL;
	if ( pShard->WaitTail == NULL ) {
		pShard->WaitHead = pCall;
	} else {
		pShard->WaitTail->PoolNext = pCall;
	}
	pShard->WaitTail = pCall;
	pCall->PoolWaiting = true;
	pShard->Waiting++;
	(void)xrtAtomic64FetchAdd(
		&pClient->PoolLive,
		1,
		XMEMORY_RELEASE
	);
	pCall->PoolOrigin->Waiting++;
	(void)xrtAtomic64FetchAdd(
		&pClient->PoolWaits,
		1,
		XMEMORY_RELAXED
	);
}



/* 从空闲 LRU 摘除一项，但不改变物理连接数量。 */
static void __xrtHttpPoolIdleRemove(
	__xrt_http_client_pool_shard* pShard,
	__xrt_http_client_idle* pIdle
)
{
	if ( pIdle->Previous == NULL ) {
		pShard->IdleHead = pIdle->Next;
	} else {
		pIdle->Previous->Next = pIdle->Next;
	}
	if ( pIdle->Next == NULL ) {
		pShard->IdleTail = pIdle->Previous;
	} else {
		pIdle->Next->Previous = pIdle->Previous;
	}
	pIdle->Previous = NULL;
	pIdle->Next = NULL;
	pShard->Idle--;
	__xrtHttpPoolCountRelease(&pShard->Client->PoolIdle);
	pIdle->Origin->Idle--;
}



/* 把新空闲项放在全局 LRU 的最新端。 */
static void __xrtHttpPoolIdleInsert(
	__xrt_http_client_pool_shard* pShard,
	__xrt_http_client_idle* pIdle
)
{
	pIdle->Previous = NULL;
	pIdle->Next = pShard->IdleHead;
	if ( pShard->IdleHead == NULL ) {
		pShard->IdleTail = pIdle;
	} else {
		pShard->IdleHead->Previous = pIdle;
	}
	pShard->IdleHead = pIdle;
	pShard->Idle++;
	pIdle->Origin->Idle++;
}



/* 查找指定 Origin 最近归还的一条空闲连接。 */
static __xrt_http_client_idle* __xrtHttpPoolIdleFind(
	__xrt_http_client_pool_shard* pShard,
	const __xrt_http_client_origin* pOrigin
)
{
	__xrt_http_client_idle* pIdle = pShard->IdleHead;

	while ( (pIdle != NULL) &&
		(pIdle->Origin != pOrigin) ) {
		pIdle = pIdle->Next;
	}
	return pIdle;
}



/* 查找指定 Origin 最久未使用的一条空闲连接。 */
static __xrt_http_client_idle* __xrtHttpPoolIdleFindOldest(
	__xrt_http_client_pool_shard* pShard,
	const __xrt_http_client_origin* pOrigin
)
{
	__xrt_http_client_idle* pIdle = pShard->IdleTail;

	while ( (pIdle != NULL) &&
		(pIdle->Origin != pOrigin) ) {
		pIdle = pIdle->Previous;
	}
	return pIdle;
}



/* 为新拨号保留连接配额。 */
static bool __xrtHttpPoolReserve(
	__xrt_http_client_pool_shard* pShard,
	xhttpcall* pCall
)
{
	xhttpclient* pClient = pShard->Client;

	if ( !__xrtHttpPoolCountReserve(
		&pClient->PoolConnections,
		pClient->Config.Pool.MaxConnections
	) ) {
		return false;
	}
	pShard->Connections++;
	(void)xrtAtomic64FetchAdd(
		&pClient->PoolLive,
		1,
		XMEMORY_RELEASE
	);
	pCall->PoolOrigin->Connections++;
	pCall->PoolReserved = true;
	pCall->PoolOpened = false;
	return true;
}



/* 释放 Call 的连接配额，并按需记录物理关闭。 */
static void __xrtHttpPoolRelease(
	__xrt_http_client_pool_shard* pShard,
	xhttpcall* pCall,
	__xrt_http_pool_batch* pBatch
)
{
	xhttpclient* pClient = pShard->Client;
	__xrt_http_client_origin* pOrigin =
		pCall->PoolOrigin;

	if ( !pCall->PoolReserved ) {
		return;
	}
	pShard->Connections--;
	__xrtHttpPoolCountRelease(&pClient->PoolConnections);
	__xrtHttpPoolCountRelease(&pClient->PoolLive);
	pOrigin->Connections--;
	__xrtHttpPoolBatchReleased(pBatch, pShard);
	if ( pCall->PoolOpened ) {
		(void)xrtAtomic64FetchAdd(
			&pClient->ConnectionsClosed,
			1,
			XMEMORY_RELAXED
		);
	}
	pCall->PoolReserved = false;
	pCall->PoolOpened = false;
	pCall->PoolOrigin = NULL;
	__xrtHttpPoolOriginPrune(pShard, pOrigin);
}



/* 摘除并关闭一条空闲连接，同时立即释放其池配额。 */
static void __xrtHttpPoolEvict(
	__xrt_http_client_pool_shard* pShard,
	__xrt_http_client_idle* pIdle,
	__xrt_http_pool_batch* pBatch
)
{
	xhttpclient* pClient = pShard->Client;
	__xrt_http_client_origin* pOrigin =
		pIdle->Origin;

	__xrtHttpPoolIdleRemove(pShard, pIdle);
	pShard->Connections--;
	__xrtHttpPoolCountRelease(&pClient->PoolConnections);
	pOrigin->Connections--;
	__xrtHttpPoolBatchReleased(pBatch, pShard);
	pIdle->Origin = NULL;
	pIdle->State = XRT_HTTP_IDLE_CLOSE_QUEUED;
	pIdle->Released = true;
	pShard->Closing++;
	(void)xrtAtomic64FetchAdd(
		&pClient->PoolClosing,
		1,
		XMEMORY_RELAXED
	);
	(void)xrtAtomic64FetchAdd(
		&pClient->ConnectionsClosed,
		1,
		XMEMORY_RELAXED
	);
	__xrtHttpPoolBatchClose(pBatch, pIdle);
	__xrtHttpPoolOriginPrune(pShard, pOrigin);
}



/* 把空闲项的传输所有权和原配额交给等待 Call。 */
static void __xrtHttpPoolTakeIdle(
	__xrt_http_client_pool_shard* pShard,
	xhttpcall* pCall,
	__xrt_http_client_idle* pIdle
)
{
	xhttpclient* pClient = pShard->Client;

	__xrtHttpPoolIdleRemove(pShard, pIdle);
	pIdle->State = XRT_HTTP_IDLE_TAKEN;
	pCall->PoolIdle = pIdle;
	pCall->PoolReserved = true;
	pCall->PoolOpened = true;
	pCall->Worker = pIdle->Worker;
	pCall->Affinity = xrtNetWorkerIndex(
		pIdle->Worker
	);
	pCall->PooledTcp = pIdle->Tcp;
	#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
		pCall->PooledTls = pIdle->Tls;
	#endif
	(void)xrtAtomic64FetchAdd(
		&pClient->ConnectionsReused,
		1,
		XMEMORY_RELAXED
	);
}



/* 为等待队列分配当前可用的空闲连接或新连接配额。 */
static void __xrtHttpPoolDispatch(
	__xrt_http_client_pool_shard* pShard,
	__xrt_http_pool_batch* pBatch
)
{
	xhttpcall* pCall = pShard->WaitHead;

	while ( pCall != NULL ) {
		xhttpcall* pNext = pCall->PoolNext;
		__xrt_http_client_origin* pOrigin =
			pCall->PoolOrigin;
		__xrt_http_client_idle* pIdle;

		if ( xrtAtomic32Load(
			&pCall->CancelGate,
			XMEMORY_ACQUIRE
		) ) {
			__xrtHttpPoolWaitRemove(
				pShard,
				pCall
			);
			pCall->PoolOrigin = NULL;
			__xrtHttpPoolBatchCall(
				pBatch,
				pCall
			);
			__xrtHttpPoolOriginPrune(
				pShard,
				pOrigin
			);
			pCall = pNext;
			continue;
		}
		pIdle = __xrtHttpPoolIdleFind(
			pShard,
			pOrigin
		);
		if ( pIdle != NULL ) {
			__xrtHttpPoolWaitRemove(
				pShard,
				pCall
			);
			__xrtHttpPoolTakeIdle(
				pShard,
				pCall,
				pIdle
			);
			__xrtHttpPoolBatchCall(
				pBatch,
				pCall
			);
		} else if ( __xrtHttpPoolCanOpen(
			pShard,
			pOrigin
		) && __xrtHttpPoolReserve(pShard, pCall) ) {
			__xrtHttpPoolWaitRemove(
				pShard,
				pCall
			);
			__xrtHttpPoolBatchCall(
				pBatch,
				pCall
			);
		}
		pCall = pNext;
	}
}



/* 配额释放后逐分片分发，避免有容量时另一分片的等待者长期休眠。 */
static void __xrtHttpPoolDispatchAll(
	xhttpclient* pClient,
	uint32 iStart,
	__xrt_http_pool_batch* pBatch
)
{
	for ( uint32 i = 0; i < pClient->PoolShardCount; i++ ) {
		__xrt_http_client_pool_shard* pShard;
		uint32 iIndex = (iStart + i) %
			pClient->PoolShardCount;

		pShard = &pClient->PoolShards[iIndex];
		(void)xrtMutexLock(&pShard->Lock);
		__xrtHttpPoolDispatch(pShard, pBatch);
		(void)xrtMutexUnlock(&pShard->Lock);
	}
}



/* 在锁外把已分配 Call 唤醒到其目标 Worker。 */
static void __xrtHttpPoolPost(__xrt_http_pool_batch* pBatch)
{
	xhttpcall* pCall = pBatch->CallHead;

	while ( pCall != NULL ) {
		xhttpcall* pNext = pCall->PoolNext;

		pCall->PoolNext = NULL;
		__xrtNetEnginePostInternal(
			pCall->Worker,
			&pCall->PoolCommand,
			__xrtHttpCallPoolReady,
			pCall
		);
		pCall = pNext;
	}
	pBatch->CallHead = NULL;
	pBatch->CallTail = NULL;
}



/* 关闭批次中的传输并释放池持有的调用方引用。 */
static void __xrtHttpPoolClose(__xrt_http_pool_batch* pBatch)
{
	__xrt_http_client_idle* pIdle =
		pBatch->CloseHead;

	while ( pIdle != NULL ) {
		__xrt_http_client_idle* pNext =
			pIdle->Next;
		xhttpclient* pClient = pIdle->Client;
		xnetstream* pTcp = pIdle->Tcp;
		#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
			xtlsstream* pTls = pIdle->Tls;
		#endif
		bool bClosed = false;
		bool bFinished = false;

		pIdle->Next = NULL;
		#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
			if ( pTls != NULL ) {
				bClosed = xrtTlsStreamClose(
					pTls
				);
				if ( !bClosed ) {
					xrtClearError();
					bClosed = xrtTlsStreamAbort(
						pTls
					);
				}
			} else
		#endif
		{
			bClosed = xrtNetStreamClose(pTcp);
			if ( !bClosed ) {
				xrtClearError();
				bClosed = xrtNetStreamAbort(
					pTcp
				);
			}
		}

		/*
			批次持有 CLOSE_QUEUED 项，Close 回调只能把它标为 CLOSED。
			异步关闭成功后再交给回调；终态已经先到达时由当前批次收尾。
		*/
		(void)xrtMutexLock(&pIdle->Shard->Lock);
		if ( pIdle->State == XRT_HTTP_IDLE_CLOSE_QUEUED ) {
			pIdle->State = XRT_HTTP_IDLE_CLOSING;
			if ( !bClosed ) {
				pIdle->Released = false;
			}
		} else if ( pIdle->State == XRT_HTTP_IDLE_CLOSED ) {
			bFinished = true;
		}
		(void)xrtMutexUnlock(&pIdle->Shard->Lock);

		if ( bClosed || bFinished ) {
			#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
				if ( pTls != NULL ) {
					xrtTlsStreamDestroy(pTls);
				} else
			#endif
			{
				xrtNetStreamDestroy(pTcp);
			}
		}
		if ( bFinished ) {
			xrtFree(pIdle);
			__xrtHttpPoolCountRelease(&pClient->PoolLive);
			__xrtHttpClientTryFinish(pClient);
			__xrtHttpClientRelease(pClient);
		}
		if ( !bClosed ) {
			xrtClearError();
		}
		pIdle = pNext;
	}
	pBatch->CloseHead = NULL;
	pBatch->CloseTail = NULL;
}



/* 锁外执行批次，关闭请求先于新拨号，尽快释放系统资源。 */
static void __xrtHttpPoolRun(__xrt_http_pool_batch* pBatch)
{
	if ( pBatch->DispatchClient != NULL ) {
		xhttpclient* pClient = pBatch->DispatchClient;
		uint32 iStart = pBatch->DispatchStart;

		pBatch->DispatchClient = NULL;
		__xrtHttpPoolDispatchAll(
			pClient,
			iStart,
			pBatch
		);
	}
	__xrtHttpPoolClose(pBatch);
	__xrtHttpPoolPost(pBatch);
}



/* 从 LRU 摘除出现额外输入或读端终止的空闲传输。 */
static void __xrtHttpPoolIdleRetire(
	__xrt_http_client_idle* pIdle
)
{
	__xrt_http_client_pool_shard* pShard = pIdle->Shard;
	__xrt_http_pool_batch Batch;

	memset(&Batch, 0, sizeof(Batch));
	(void)xrtMutexLock(&pShard->Lock);
	if ( pIdle->State == XRT_HTTP_IDLE_LISTED ) {
		__xrtHttpPoolEvict(pShard, pIdle, &Batch);
	}
	(void)xrtMutexUnlock(&pShard->Lock);
	__xrtHttpPoolRun(&Batch);
}



/* 在首条可过期空闲连接出现时建立唯一清扫 Timer。 */
static bool __xrtHttpPoolTimerStart(
	__xrt_http_client_pool_shard* pShard
)
{
	xhttpclient* pClient = pShard->Client;
	uint64 Id;

	if ( (pShard->Timer != 0) ||
		(pShard->IdleTail == NULL) ||
		(pClient->Config.Pool.IdleTimeout == 0) ) {
		return true;
	}
	if ( __xrtHttpClientHold(pClient) == NULL ) {
		return false;
	}
	Id = xrtNetEngineSchedule(
		pClient->Engine,
		pShard->Index % xrtNetEngineWorkerCount(pClient->Engine),
		pShard->IdleTail->Deadline,
		__xrtHttpPoolTimer,
		pShard
	);
	if ( Id == 0 ) {
		__xrtHttpClientRelease(pClient);
		return false;
	}
	pShard->Timer = Id;
	(void)xrtAtomic64FetchAdd(
		&pClient->PoolTimersPending,
		1,
		XMEMORY_RELAXED
	);
	(void)xrtAtomic64FetchAdd(
		&pClient->PoolLive,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 空闲传输关闭时按其状态回收配额、调用方引用和项内存。 */
static void __xrtHttpPoolIdleClosed(
	__xrt_http_client_idle* pIdle
)
{
	xhttpclient* pClient = pIdle->Client;
	__xrt_http_client_pool_shard* pShard = pIdle->Shard;
	__xrt_http_pool_batch Batch;
	__xrt_http_client_origin* pOrigin = NULL;
	bool bDestroy = false;
	bool bFree = false;

	memset(&Batch, 0, sizeof(Batch));
	(void)xrtMutexLock(&pShard->Lock);
	if ( pIdle->State == XRT_HTTP_IDLE_LISTED ) {
		pOrigin = pIdle->Origin;
		__xrtHttpPoolIdleRemove(pShard, pIdle);
		pShard->Connections--;
		__xrtHttpPoolCountRelease(&pClient->PoolConnections);
		pOrigin->Connections--;
		__xrtHttpPoolBatchReleased(&Batch, pShard);
		pIdle->Origin = NULL;
		pIdle->State = XRT_HTTP_IDLE_CLOSED;
		(void)xrtAtomic64FetchAdd(
			&pClient->ConnectionsClosed,
			1,
			XMEMORY_RELAXED
		);
		__xrtHttpPoolOriginPrune(
			pShard,
			pOrigin
		);
		bDestroy = !pIdle->Released;
		bFree = true;
	} else if (
		pIdle->State == XRT_HTTP_IDLE_CLOSE_QUEUED
	) {
		pIdle->State = XRT_HTTP_IDLE_CLOSED;
		pShard->Closing--;
		__xrtHttpPoolCountRelease(&pClient->PoolClosing);
	} else if (
		pIdle->State == XRT_HTTP_IDLE_CLOSING
	) {
		pIdle->State = XRT_HTTP_IDLE_CLOSED;
		pShard->Closing--;
		__xrtHttpPoolCountRelease(&pClient->PoolClosing);
		bDestroy = !pIdle->Released;
		bFree = true;
	} else if (
		pIdle->State == XRT_HTTP_IDLE_TAKEN
	) {
		pIdle->State = XRT_HTTP_IDLE_CLOSED;
	}
	(void)xrtMutexUnlock(&pShard->Lock);
	__xrtHttpPoolRun(&Batch);
	if ( bDestroy ) {
		#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
			if ( pIdle->Tls != NULL ) {
				xrtTlsStreamDestroy(pIdle->Tls);
			} else
		#endif
		{
			xrtNetStreamDestroy(pIdle->Tcp);
		}
	}
	if ( bFree ) {
		xrtFree(pIdle);
	}
	if ( bFree ) {
		__xrtHttpPoolCountRelease(&pClient->PoolLive);
	}
	__xrtHttpClientTryFinish(pClient);
	if ( bFree ) {
		__xrtHttpClientRelease(pClient);
	}
}



/* 空闲 TCP 收到任何字节都说明它不能再匹配下一次请求。 */
static void __xrtHttpPoolTcpRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pStream;
	(void)pBuffer;
	__xrtHttpPoolIdleRetire(
		(__xrt_http_client_idle*)pData
	);
}



/* 空闲 TCP 对端半关闭后立即释放池配额。 */
static void __xrtHttpPoolTcpEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttpPoolIdleRetire(
		(__xrt_http_client_idle*)pData
	);
}



/* TCP 空闲连接关闭后完成项和传输引用回收。 */
static void __xrtHttpPoolTcpClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	(void)pStream;
	(void)Result;
	(void)pError;
	__xrtHttpPoolIdleClosed(
		(__xrt_http_client_idle*)pData
	);
}



#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)

#if defined(XRT_FEATURE_HTTP_CLIENT_RESUME)

/* 空闲 TLS 收到晚到 ticket 时立即转移到所属 Client 的有界缓存。 */
static void __xrtHttpPoolTlsTicket(
	xtlsstream* pStream,
	ptr pData
)
{
	__xrt_http_client_idle* pIdle =
		(__xrt_http_client_idle*)pData;
	__xrt_http_resume_route Route;

	memset(&Route, 0, sizeof(Route));
	Route.Client = pIdle->Client;
	Route.Host.Data = pIdle->Origin->Host;
	Route.Host.Size = pIdle->Origin->HostSize;
	Route.Port = pIdle->Origin->Port;
	#if defined(XRT_FEATURE_HTTP_CLIENT_PROXY)
		Route.Proxy = pIdle->Origin->Proxy;
	#endif
	__xrtHttpResumeCollectRoute(&Route, pStream);
}

#endif



/* 空闲 TLS 收到未被上一事务消费的明文后立即退出连接池。 */
static void __xrtHttpPoolTlsRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pStream;
	(void)pBuffer;
	__xrtHttpPoolIdleRetire(
		(__xrt_http_client_idle*)pData
	);
}



/* 空闲 TLS 收到 close_notify 后立即释放池配额。 */
static void __xrtHttpPoolTlsEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttpPoolIdleRetire(
		(__xrt_http_client_idle*)pData
	);
}



/* TLS 空闲连接关闭后完成项和传输引用回收。 */
static void __xrtHttpPoolTlsClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	(void)pStream;
	(void)Result;
	(void)pError;
	__xrtHttpPoolIdleClosed(
		(__xrt_http_client_idle*)pData
	);
}

#endif



/* 把完成传输切换为空闲 Close 观察器。 */
static __xrt_http_client_idle* __xrtHttpPoolIdleCreate(
	xhttpcall* pCall,
	const xhttp1callresult* pResult
)
{
	__xrt_http_client_idle* pIdle;
	xhttpclient* pClient = pCall->Client;

	pIdle = (__xrt_http_client_idle*)xrtCalloc(
		1,
		sizeof(*pIdle)
	);
	if ( pIdle == NULL ) {
		return NULL;
	}
	if ( __xrtHttpClientHold(pClient) == NULL ) {
		xrtFree(pIdle);
		return NULL;
	}
	pIdle->Client = pClient;
	pIdle->Origin = pCall->PoolOrigin;
	pIdle->Shard = pCall->PoolOrigin->Shard;
	pIdle->State = XRT_HTTP_IDLE_LISTED;
	#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
		if ( pResult->Tls != NULL ) {
			xtlsstreamevents Events;
			xnetstream* pTransport =
				xrtTlsStreamTransport(pResult->Tls);

			memset(&Events, 0, sizeof(Events));
			Events.Read = __xrtHttpPoolTlsRead;
			Events.End = __xrtHttpPoolTlsEnd;
			Events.Close = __xrtHttpPoolTlsClose;
			#if defined(XRT_FEATURE_HTTP_CLIENT_RESUME)
				Events.Ticket = __xrtHttpPoolTlsTicket;
			#endif
			pIdle->Tls = pResult->Tls;
			pIdle->Worker = xrtNetStreamWorker(
				pTransport
			);
			if ( !xrtTlsStreamSetEvents(
				pResult->Tls,
				&Events,
				pIdle
			) ) {
				__xrtHttpClientRelease(pClient);
				xrtFree(pIdle);
				return NULL;
			}
			return pIdle;
		}
	#endif
	{
		xnetstreamevents Events;

		memset(&Events, 0, sizeof(Events));
		Events.Read = __xrtHttpPoolTcpRead;
		Events.End = __xrtHttpPoolTcpEnd;
		Events.Close = __xrtHttpPoolTcpClose;
		pIdle->Tcp = pResult->Tcp;
		pIdle->Worker = xrtNetStreamWorker(
			pResult->Tcp
		);
		if ( !xrtNetStreamSetEvents(
			pResult->Tcp,
			&Events,
			pIdle
		) ) {
			__xrtHttpClientRelease(pClient);
			xrtFree(pIdle);
			return NULL;
		}
	}
	return pIdle;
}



/*
	归还失败时在当前 Worker 摘除临时观察器。
	极端关闭竞态下保留项到 Close 回调，传输仍由调用方随后释放。
*/
static void __xrtHttpPoolIdleDiscard(
	__xrt_http_client_idle* pIdle
)
{
	xhttpclient* pClient = pIdle->Client;
	bool bDetached;

	#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
		if ( pIdle->Tls != NULL ) {
			bDetached = xrtTlsStreamSetEvents(
				pIdle->Tls,
				NULL,
				NULL
			);
		} else
	#endif
	{
		bDetached = xrtNetStreamSetEvents(
			pIdle->Tcp,
			NULL,
			NULL
		);
	}
	if ( bDetached ) {
		pIdle->State = XRT_HTTP_IDLE_USED;
		xrtFree(pIdle);
		__xrtHttpClientRelease(pClient);
	} else {
		xrtClearError();
		(void)xrtMutexLock(&pIdle->Shard->Lock);
		pIdle->State = XRT_HTTP_IDLE_CLOSING;
		pIdle->Released = true;
		pIdle->Shard->Closing++;
		(void)xrtAtomic64FetchAdd(
			&pClient->PoolClosing,
			1,
			XMEMORY_RELAXED
		);
		(void)xrtAtomic64FetchAdd(
			&pClient->PoolLive,
			1,
			XMEMORY_RELEASE
		);
		(void)xrtMutexUnlock(&pIdle->Shard->Lock);
	}
}



/* 清扫到期空闲项，并为下一条最早截止项重建唯一 Timer。 */
static void __xrtHttpPoolTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	__xrt_http_client_pool_shard* pShard =
		(__xrt_http_client_pool_shard*)pData;
	xhttpclient* pClient = pShard->Client;
	__xrt_http_pool_batch Batch;
	xdeadline iNow = xrtClock();
	bool bMatched;

	(void)pWorker;
	memset(&Batch, 0, sizeof(Batch));
	(void)xrtMutexLock(&pShard->Lock);
	bMatched = pShard->Timer == Id;
	if ( bMatched ) {
		pShard->Timer = 0;
	}
	__xrtHttpPoolCountRelease(&pClient->PoolTimersPending);
	__xrtHttpPoolCountRelease(&pClient->PoolLive);
	if ( bMatched && (Result == XNET_RESULT_OK) ) {
		while ( (pShard->IdleTail != NULL) &&
			(pShard->IdleTail->Deadline <= iNow) ) {
			__xrtHttpPoolEvict(
				pShard,
				pShard->IdleTail,
				&Batch
			);
		}
		if ( !__xrtHttpPoolTimerStart(pShard) ) {
			xrtClearError();
			while ( pShard->IdleTail != NULL ) {
				__xrtHttpPoolEvict(
					pShard,
					pShard->IdleTail,
					&Batch
				);
			}
		}
	}
	(void)xrtMutexUnlock(&pShard->Lock);
	__xrtHttpPoolRun(&Batch);
	__xrtHttpClientTryFinish(pClient);
	__xrtHttpClientRelease(pClient);
}



/* 初始化公开连接池默认值。 */
XRT_API void xrtHttpClientPoolConfigInit(
	xhttpclientpoolconfig* pConfig
)
{
	const xhttpclientpoolconfig Config = {
		0,
		0,
		0,
		0,
		XRT_HTTP_POOL_IDLE_DEFAULT,
		XRT_HTTP_POOL_IDLE_ORIGIN_DEFAULT,
		XRT_HTTP_POOL_IDLE_TIMEOUT_DEFAULT
	};

	if ( pConfig == NULL ) {
		__xrtHttpPoolSetError(
			XERR_ARGUMENT,
			"init-http-client-pool-config",
			"HTTP client pool config is null"
		);
		return;
	}
	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		__xrtHttpPoolSetError(
			XERR_ARGUMENT,
			"init-http-client-pool-config",
			"HTTP client pool config range is invalid"
		);
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 按 Engine 规模建立固定分片，避免运行期扩容和 Origin 间锁争用。 */
bool __xrtHttpPoolInit(xhttpclient* pClient)
{
	uint32 iWorkers = pClient->Engine == NULL ?
		1u : xrtNetEngineWorkerCount(pClient->Engine);
	uint32 iShards = 1;

	while ( (iShards < iWorkers) &&
		(iShards < XRT_HTTP_POOL_SHARDS_MAX) ) {
		iShards <<= 1u;
	}
	pClient->PoolShards = (__xrt_http_client_pool_shard*)
		xrtCalloc(iShards, sizeof(*pClient->PoolShards));
	if ( pClient->PoolShards == NULL ) {
		return false;
	}
	for ( uint32 i = 0; i < iShards; i++ ) {
		__xrt_http_client_pool_shard* pShard =
			&pClient->PoolShards[i];

		if ( !xrtMutexInit(&pShard->Lock) ) {
			for ( uint32 j = 0; j < i; j++ ) {
				(void)xrtMutexUnit(
					&pClient->PoolShards[j].Lock
				);
			}
			xrtFree(pClient->PoolShards);
			pClient->PoolShards = NULL;
			return false;
		}
		pShard->Client = pClient;
		pShard->Index = i;
		pShard->Ready = true;
	}
	pClient->PoolShardCount = iShards;
	xrtAtomic64Init(&pClient->PoolConnections, 0);
	xrtAtomic64Init(&pClient->PoolIdle, 0);
	xrtAtomic64Init(&pClient->PoolClosing, 0);
	xrtAtomic64Init(&pClient->PoolWaiting, 0);
	xrtAtomic64Init(&pClient->PoolTimersPending, 0);
	xrtAtomic64Init(&pClient->PoolLive, 0);
	xrtAtomic64Init(&pClient->RequestsStarted, 0);
	xrtAtomic64Init(&pClient->RequestsCompleted, 0);
	xrtAtomic64Init(&pClient->ConnectionsOpened, 0);
	xrtAtomic64Init(&pClient->ConnectionsReused, 0);
	xrtAtomic64Init(&pClient->ConnectionsClosed, 0);
	xrtAtomic64Init(&pClient->PoolWaits, 0);
	xrtAtomic64Init(&pClient->PoolRejected, 0);
	#if defined(XRT_FEATURE_HTTP_CLIENT_REDIRECT)
		xrtAtomic64Init(&pClient->RedirectsFollowed, 0);
	#endif
	pClient->PoolReady = true;
	return true;
}



/* 最后一个内部引用只能在池内对象和 Timer 已经排空后释放锁。 */
void __xrtHttpPoolUnit(xhttpclient* pClient)
{
	if ( !pClient->PoolReady ) {
		return;
	}
	pClient->PoolReady = false;
	for ( uint32 i = 0; i < pClient->PoolShardCount; i++ ) {
		__xrt_http_client_pool_shard* pShard =
			&pClient->PoolShards[i];
		__xrt_http_client_origin* pOrigin = pShard->Origins;

		while ( pOrigin != NULL ) {
			__xrt_http_client_origin* pNext =
				pOrigin->Next;

			#if defined(XRT_FEATURE_HTTP_CLIENT_PROXY)
				xrtNetProxyRelease(pOrigin->Proxy);
			#endif
			xrtFree(pOrigin->Host);
			xrtFree(pOrigin);
			pOrigin = pNext;
		}
		pShard->Origins = NULL;
		pShard->Ready = false;
		(void)xrtMutexUnit(&pShard->Lock);
	}
	xrtFree(pClient->PoolShards);
	pClient->PoolShards = NULL;
	pClient->PoolShardCount = 0;
}



/* 申请空闲传输、新连接配额或一个有界等待位置。 */
bool __xrtHttpPoolAcquire(xhttpcall* pCall, bool* pReady)
{
	xhttpclient* pClient = pCall->Client;
	__xrt_http_client_pool_shard* pShard =
		__xrtHttpPoolShardCall(pCall);
	__xrt_http_pool_batch Batch;
	__xrt_http_client_origin* pOrigin;
	__xrt_http_client_idle* pIdle;
	xdeadline iNow;
	bool bResult = true;

	memset(&Batch, 0, sizeof(Batch));
	*pReady = false;
	(void)xrtMutexLock(&pShard->Lock);
	/*
		取消分发可能在 Call 尚未进入池之前完成。
		必须在池锁内重新观察取消门，避免随后把无人再摘除的 Call 放入等待队列。
	*/
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		*pReady = true;
		goto finish;
	}
	pOrigin = __xrtHttpPoolOriginGet(
		pShard,
		pCall
	);
	if ( pOrigin == NULL ) {
		bResult = false;
		goto finish;
	}
	pCall->PoolOrigin = pOrigin;
	if ( pClient->Config.Pool.IdleTimeout != 0 ) {
		iNow = xrtClock();
		while ( (pShard->IdleTail != NULL) &&
			(pShard->IdleTail->Deadline <= iNow) ) {
			__xrtHttpPoolEvict(
				pShard,
				pShard->IdleTail,
				&Batch
			);
		}
	}
	__xrtHttpPoolDispatch(pShard, &Batch);
	pIdle = __xrtHttpPoolIdleFind(
		pShard,
		pOrigin
	);
	if ( pIdle != NULL ) {
		__xrtHttpPoolTakeIdle(
			pShard,
			pCall,
			pIdle
		);
		__xrtHttpPoolBatchCall(
			&Batch,
			pCall
		);
	} else if ( __xrtHttpPoolCanOpen(
		pShard,
		pOrigin
	) && __xrtHttpPoolReserve(pShard, pCall) ) {
		*pReady = true;
	} else if ( __xrtHttpPoolCanWait(
		pShard,
		pOrigin
	) && __xrtHttpPoolCountReserve(
		&pClient->PoolWaiting,
		pClient->Config.Pool.MaxWaiting
	) ) {
		__xrtHttpPoolWaitAppend(
			pShard,
			pCall
		);
	} else {
		(void)xrtAtomic64FetchAdd(
			&pClient->PoolRejected,
			1,
			XMEMORY_RELAXED
		);
		pCall->PoolOrigin = NULL;
		__xrtHttpPoolOriginPrune(
			pShard,
			pOrigin
		);
		__xrtHttpPoolSetError(
			XERR_AGAIN,
			"acquire-http-connection",
			"HTTP connection pool waiting limit reached"
		);
		bResult = false;
	}

finish:
	(void)xrtMutexUnlock(&pShard->Lock);
	__xrtHttpPoolRun(&Batch);
	return bResult;
}



/* 把仍在等待队列中的取消 Call 摘除并唤醒到其原 Worker。 */
bool __xrtHttpPoolCancel(xhttpcall* pCall)
{
	xhttpclient* pClient = pCall->Client;
	__xrt_http_client_pool_shard* pShard =
		&pClient->PoolShards[pCall->PoolShardIndex];
	__xrt_http_pool_batch Batch;
	__xrt_http_client_origin* pOrigin;
	bool bCancelled = false;

	memset(&Batch, 0, sizeof(Batch));
	(void)xrtMutexLock(&pShard->Lock);
	if ( pCall->PoolWaiting ) {
		pOrigin = pCall->PoolOrigin;
		__xrtHttpPoolWaitRemove(pShard, pCall);
		pCall->PoolOrigin = NULL;
		__xrtHttpPoolOriginPrune(
			pShard,
			pOrigin
		);
		__xrtHttpPoolBatchCall(
			&Batch,
			pCall
		);
		bCancelled = true;
	}
	(void)xrtMutexUnlock(&pShard->Lock);
	__xrtHttpPoolRun(&Batch);
	return bCancelled;
}



/* 新拨号成功后把保留配额标记为一条实际连接。 */
void __xrtHttpPoolOpened(xhttpcall* pCall)
{
	xhttpclient* pClient = pCall->Client;
	__xrt_http_client_pool_shard* pShard =
		&pClient->PoolShards[pCall->PoolShardIndex];

	(void)xrtMutexLock(&pShard->Lock);
	if ( pCall->PoolReserved &&
		!pCall->PoolOpened ) {
		pCall->PoolOpened = true;
		(void)xrtAtomic64FetchAdd(
			&pClient->ConnectionsOpened,
			1,
			XMEMORY_RELAXED
		);
	}
	(void)xrtMutexUnlock(&pShard->Lock);
}



/*
	选择释放当前连接后最早可运行的等待者。
	同 Origin 可以直接接管传输；异 Origin 要求归还方释放配额，避免后到
	同源请求持续越过全局队列中的较早请求。
*/
static xhttpcall* __xrtHttpPoolTakeHandoff(
	__xrt_http_client_pool_shard* pShard,
	__xrt_http_client_origin* pOrigin,
	__xrt_http_pool_batch* pBatch,
	bool* pYield
)
{
	xhttpcall* pCall = pShard->WaitHead;

	*pYield = false;
	while ( pCall != NULL ) {
		xhttpcall* pNext = pCall->PoolNext;

		if ( xrtAtomic32Load(
			&pCall->CancelGate,
			XMEMORY_ACQUIRE
		) ) {
			__xrt_http_client_origin* pCancelled =
				pCall->PoolOrigin;

			__xrtHttpPoolWaitRemove(
				pShard,
				pCall
			);
			pCall->PoolOrigin = NULL;
			__xrtHttpPoolBatchCall(
				pBatch,
				pCall
			);
			__xrtHttpPoolOriginPrune(
				pShard,
				pCancelled
			);
		} else if ( __xrtHttpPoolCanOpenAfterRelease(
			pShard,
			pCall->PoolOrigin,
			pOrigin
		) ) {
			if ( pCall->PoolOrigin != pOrigin ) {
				*pYield = true;
				return NULL;
			}
			__xrtHttpPoolWaitRemove(
				pShard,
				pCall
			);
			return pCall;
		}
		pCall = pNext;
	}
	return NULL;
}



/* 把刚完成的传输和原配额直接交给同 Origin 等待者。 */
static void __xrtHttpPoolHandoff(
	xhttpcall* pFrom,
	xhttpcall* pTo,
	const xhttp1callresult* pResult,
	__xrt_http_client_idle* pIdle,
	__xrt_http_pool_batch* pBatch
)
{
	xhttpclient* pClient = pFrom->Client;
	xnetstream* pTransport;

	pFrom->PoolReserved = false;
	pFrom->PoolOpened = false;
	pFrom->PoolOrigin = NULL;
	pTo->PoolReserved = true;
	pTo->PoolOpened = true;
	pTo->PoolIdle = pIdle;
	pTo->PooledTcp = pResult->Tcp;
	#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
		pTo->PooledTls = pResult->Tls;
		if ( pResult->Tls != NULL ) {
			pTransport = xrtTlsStreamTransport(
				pResult->Tls
			);
		} else
	#endif
	{
		pTransport = pResult->Tcp;
	}
	pTo->Worker = xrtNetStreamWorker(pTransport);
	pTo->Affinity = xrtNetWorkerIndex(pTo->Worker);
	if ( pIdle != NULL ) {
		pIdle->State = XRT_HTTP_IDLE_TAKEN;
	}
	(void)xrtAtomic64FetchAdd(
		&pClient->ConnectionsReused,
		1,
		XMEMORY_RELAXED
	);
	__xrtHttpPoolBatchCall(pBatch, pTo);
}



/* 归还可复用传输：优先同源直接交接，否则进入有界 LRU。 */
bool __xrtHttpPoolPut(
	xhttpcall* pCall,
	const xhttp1callresult* pResult
)
{
	xhttpclient* pClient = pCall->Client;
	__xrt_http_client_pool_shard* pShard =
		&pClient->PoolShards[pCall->PoolShardIndex];
	__xrt_http_pool_batch Batch;
	__xrt_http_client_idle* pIdle = NULL;
	xhttpcall* pWait;
	bool bKeep;
	bool bYield = false;

	memset(&Batch, 0, sizeof(Batch));
	(void)xrtMutexLock(&pShard->Lock);
	bKeep = pCall->PoolReserved &&
		pCall->PoolOpened &&
		(xrtAtomic32Load(
			&pClient->State,
			XMEMORY_ACQUIRE
		) == XHTTP_CLIENT_RUNNING);
	if ( bKeep ) {
		pWait = __xrtHttpPoolTakeHandoff(
			pShard,
			pCall->PoolOrigin,
			&Batch,
			&bYield
		);
		if ( pWait != NULL ) {
			__xrtHttpPoolHandoff(
				pCall,
				pWait,
				pResult,
				NULL,
				&Batch
			);
			(void)xrtMutexUnlock(
				&pShard->Lock
			);
			__xrtHttpPoolRun(&Batch);
			return true;
		}
		if ( !bYield &&
			(xrtAtomic64Load(
				&pClient->PoolWaiting,
				XMEMORY_ACQUIRE
			) > pShard->Waiting) ) {
			bYield = true;
		}
	}
	bKeep = bKeep &&
		!bYield &&
		(pClient->Config.Pool.MaxIdle != 0) &&
		(pClient->Config.Pool.MaxIdlePerOrigin != 0);
	(void)xrtMutexUnlock(&pShard->Lock);
	__xrtHttpPoolRun(&Batch);
	if ( !bKeep ) {
		return false;
	}

	pIdle = __xrtHttpPoolIdleCreate(
		pCall,
		pResult
	);
	if ( pIdle == NULL ) {
		return false;
	}
	memset(&Batch, 0, sizeof(Batch));
	(void)xrtMutexLock(&pShard->Lock);
	bKeep = pCall->PoolReserved &&
		pCall->PoolOpened &&
		(xrtAtomic32Load(
			&pClient->State,
			XMEMORY_ACQUIRE
		) == XHTTP_CLIENT_RUNNING);
	if ( bKeep ) {
		bYield = false;
		pWait = __xrtHttpPoolTakeHandoff(
			pShard,
			pCall->PoolOrigin,
			&Batch,
			&bYield
		);
		if ( pWait != NULL ) {
			__xrtHttpPoolHandoff(
				pCall,
				pWait,
				pResult,
				pIdle,
				&Batch
			);
			(void)xrtMutexUnlock(
				&pShard->Lock
			);
			__xrtHttpPoolRun(&Batch);
			return true;
		}
		if ( !bYield &&
			(xrtAtomic64Load(
				&pClient->PoolWaiting,
				XMEMORY_ACQUIRE
			) > pShard->Waiting) ) {
			bYield = true;
		}
	}
	bKeep = bKeep &&
		!bYield &&
		(pClient->Config.Pool.MaxIdle != 0) &&
		(pClient->Config.Pool.MaxIdlePerOrigin != 0);
	if ( bKeep ) {
		while ( pCall->PoolOrigin->Idle >=
			pClient->Config.Pool.MaxIdlePerOrigin ) {
			__xrt_http_client_idle* pOld =
				__xrtHttpPoolIdleFindOldest(
					pShard,
					pCall->PoolOrigin
				);

			if ( pOld == NULL ) {
				break;
			}
			__xrtHttpPoolEvict(
				pShard,
				pOld,
				&Batch
			);
		}
		bKeep = __xrtHttpPoolCountReserve(
			&pClient->PoolIdle,
			pClient->Config.Pool.MaxIdle
		);
		if ( bKeep ) {
			pIdle->Deadline =
				pClient->Config.Pool.IdleTimeout == 0 ?
					XRT_DEADLINE_NEVER :
					xrtDeadlineAfter(
						pClient->Config.Pool.IdleTimeout
					);
			__xrtHttpPoolIdleInsert(pShard, pIdle);
		}
		if ( bKeep ) {
			if ( !__xrtHttpPoolTimerStart(pShard) ) {
				__xrtHttpPoolIdleRemove(
					pShard,
					pIdle
				);
				bKeep = false;
			} else {
				pCall->PoolReserved = false;
				pCall->PoolOpened = false;
				pCall->PoolOrigin = NULL;
			}
		}
	}
	(void)xrtMutexUnlock(&pShard->Lock);
	if ( !bKeep ) {
		__xrtHttpPoolIdleDiscard(pIdle);
	}
	__xrtHttpPoolRun(&Batch);
	return bKeep;
}



/* 成功附加后释放临时空闲项，传输引用已经交给低级 HTTP/1 Call。 */
void __xrtHttpPoolPooledUsed(xhttpcall* pCall)
{
	__xrt_http_client_idle* pIdle =
		pCall->PoolIdle;

	__xrtHttpCallReused(pCall);
	pCall->PoolIdle = NULL;
	pCall->PooledTcp = NULL;
	#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
		pCall->PooledTls = NULL;
	#endif
	if ( pIdle != NULL ) {
		pIdle->State = XRT_HTTP_IDLE_USED;
		xrtFree(pIdle);
		__xrtHttpClientRelease(pCall->Client);
	}
}



/* 在当前 Worker 上摘除失效空闲项并关闭其传输。 */
void __xrtHttpPoolPooledStale(xhttpcall* pCall)
{
	xhttpclient* pClient = pCall->Client;
	__xrt_http_client_pool_shard* pShard =
		&pClient->PoolShards[pCall->PoolShardIndex];
	__xrt_http_client_idle* pIdle =
		pCall->PoolIdle;
	xnetstream* pTcp = pCall->PooledTcp;
	#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
		xtlsstream* pTls = pCall->PooledTls;
	#endif
	xerror* pSaved = xrtTakeError();
	bool bDetached = true;
	bool bClosed;

	pCall->PoolIdle = NULL;
	pCall->PooledTcp = NULL;
	#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
		pCall->PooledTls = NULL;
	#endif
	(void)xrtMutexLock(&pShard->Lock);
	if ( pCall->PoolOpened ) {
		pCall->PoolOpened = false;
		(void)xrtAtomic64FetchAdd(
			&pClient->ConnectionsClosed,
			1,
			XMEMORY_RELAXED
		);
	}
	(void)xrtMutexUnlock(&pShard->Lock);
	if ( pIdle != NULL ) {
		if ( pIdle->State == XRT_HTTP_IDLE_CLOSED ) {
			bDetached = true;
		} else {
			#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
				if ( pTls != NULL ) {
					bDetached =
						xrtTlsStreamSetEvents(
							pTls,
							NULL,
							NULL
						);
				} else
			#endif
			{
				bDetached = xrtNetStreamSetEvents(
					pTcp,
					NULL,
					NULL
				);
			}
		}
	}
	#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
		if ( pTls != NULL ) {
			bClosed = xrtTlsStreamAbort(pTls);
			if ( bClosed || bDetached ) {
				xrtTlsStreamDestroy(pTls);
			}
		} else
	#endif
	{
		bClosed = xrtNetStreamAbort(pTcp);
		if ( bClosed || bDetached ) {
			xrtNetStreamDestroy(pTcp);
		}
	}
	if ( pIdle != NULL ) {
		if ( bDetached ) {
			pIdle->State = XRT_HTTP_IDLE_USED;
			xrtFree(pIdle);
			__xrtHttpClientRelease(pClient);
		} else {
			(void)xrtMutexLock(&pShard->Lock);
			pIdle->State = XRT_HTTP_IDLE_CLOSING;
			pIdle->Released = bClosed;
			pShard->Closing++;
			(void)xrtAtomic64FetchAdd(
				&pClient->PoolClosing,
				1,
				XMEMORY_RELAXED
			);
			(void)xrtAtomic64FetchAdd(
				&pClient->PoolLive,
				1,
				XMEMORY_RELEASE
			);
			(void)xrtMutexUnlock(
				&pShard->Lock
			);
		}
	}
	xrtClearError();
	if ( pSaved != NULL ) {
		xrtSetError(pSaved);
		xrtErrorFree(pSaved);
	}
}



/* 标记升级传输已经转交调用方，随后只释放池配额。 */
void __xrtHttpPoolTransferred(xhttpcall* pCall)
{
	xhttpclient* pClient = pCall->Client;
	__xrt_http_client_pool_shard* pShard =
		&pClient->PoolShards[pCall->PoolShardIndex];

	(void)xrtMutexLock(&pShard->Lock);
	if ( pCall->PoolReserved ) {
		pCall->PoolOpened = false;
	}
	(void)xrtMutexUnlock(&pShard->Lock);
}



/* 唯一终态释放等待位置、连接配额，并唤醒当前可运行的等待者。 */
void __xrtHttpPoolFinish(xhttpcall* pCall)
{
	xhttpclient* pClient = pCall->Client;
	__xrt_http_client_pool_shard* pShard =
		&pClient->PoolShards[pCall->PoolShardIndex];
	__xrt_http_pool_batch Batch;
	__xrt_http_client_origin* pOrigin;

	if ( (pCall->PooledTcp != NULL)
		#if defined(XRT_FEATURE_HTTP_CLIENT_HTTPS)
			|| (pCall->PooledTls != NULL)
		#endif
	) {
		__xrtHttpPoolPooledStale(pCall);
	}
	memset(&Batch, 0, sizeof(Batch));
	(void)xrtMutexLock(&pShard->Lock);
	if ( pCall->PoolWaiting ) {
		pOrigin = pCall->PoolOrigin;
		__xrtHttpPoolWaitRemove(pShard, pCall);
		pCall->PoolOrigin = NULL;
		__xrtHttpPoolOriginPrune(
			pShard,
			pOrigin
		);
	} else {
		/*
			等待位置与连接配额互斥，只有已经取得配额的
			Call 才能递减 Origin 的连接计数。
		*/
		__xrtHttpPoolRelease(
			pShard,
			pCall,
			&Batch
		);
	}
	(void)xrtMutexUnlock(&pShard->Lock);
	__xrtHttpPoolRun(&Batch);
}



/* 关闭当前全部空闲连接，不影响活动 Call 或等待者。 */
XRT_API size_t xrtHttpClientCloseIdle(xhttpclient* pClient)
{
	__xrt_http_pool_batch Batch;
	size_t iClosed = 0;

	if ( pClient == NULL ) {
		__xrtHttpPoolSetError(
			XERR_ARGUMENT,
			"close-http-client-idle",
			"HTTP client is null"
		);
		return 0;
	}
	/* 构造失败回滚可能在 PoolLock 建立前隐式进入 Drain。 */
	if ( !pClient->PoolReady ) {
		return 0;
	}
	memset(&Batch, 0, sizeof(Batch));
	for ( uint32 i = 0; i < pClient->PoolShardCount; i++ ) {
		__xrt_http_client_pool_shard* pShard =
			&pClient->PoolShards[i];
		uint64 Id;

		(void)xrtMutexLock(&pShard->Lock);
		while ( pShard->IdleTail != NULL ) {
			__xrtHttpPoolEvict(
				pShard,
				pShard->IdleTail,
				&Batch
			);
			iClosed++;
		}
		Id = pShard->Timer;
		pShard->Timer = 0;
		__xrtHttpPoolDispatch(pShard, &Batch);
		(void)xrtMutexUnlock(&pShard->Lock);
		if ( (Id != 0) &&
			!__xrtNetEngineTimerCancelLifecycle(
				pClient->Engine,
				Id
			) ) {
			xrtClearError();
		}
	}
	__xrtHttpPoolRun(&Batch);
	__xrtHttpClientTryFinish(pClient);
	return iClosed;
}



/* 读取连接池和生命周期计数的一致池快照。 */
XRT_API bool xrtHttpClientStats(
	const xhttpclient* pClient,
	xhttpclientstats* pStats
)
{
	xhttpclientstats Stats;
	uint64 iConnections;
	uint64 iIdle;

	if ( (pClient == NULL) ||
		!__xrtRangeValid(pStats, sizeof(Stats)) ) {
		__xrtHttpPoolSetError(
			XERR_ARGUMENT,
			"query-http-client-stats",
			"HTTP client and complete stats storage are required"
		);
		return false;
	}
	memset(&Stats, 0, sizeof(Stats));
	iConnections = xrtAtomic64Load(
		&pClient->PoolConnections,
		XMEMORY_ACQUIRE
	);
	iIdle = xrtAtomic64Load(
		&pClient->PoolIdle,
		XMEMORY_ACQUIRE
	);
	Stats.ActiveConnections = (size_t)(
		iConnections >= iIdle ?
			iConnections - iIdle : 0
	);
	Stats.IdleConnections = (size_t)iIdle;
	Stats.ClosingConnections = (size_t)xrtAtomic64Load(
		&pClient->PoolClosing,
		XMEMORY_ACQUIRE
	);
	Stats.WaitingCalls = (size_t)xrtAtomic64Load(
		&pClient->PoolWaiting,
		XMEMORY_ACQUIRE
	);
	Stats.RequestsStarted = xrtAtomic64Load(
		&pClient->RequestsStarted,
		XMEMORY_RELAXED
	);
	Stats.RequestsCompleted = xrtAtomic64Load(
		&pClient->RequestsCompleted,
		XMEMORY_RELAXED
	);
	Stats.ConnectionsOpened = xrtAtomic64Load(
		&pClient->ConnectionsOpened,
		XMEMORY_RELAXED
	);
	Stats.ConnectionsReused = xrtAtomic64Load(
		&pClient->ConnectionsReused,
		XMEMORY_RELAXED
	);
	Stats.ConnectionsClosed = xrtAtomic64Load(
		&pClient->ConnectionsClosed,
		XMEMORY_RELAXED
	);
	Stats.PoolWaits = xrtAtomic64Load(
		&pClient->PoolWaits,
		XMEMORY_RELAXED
	);
	Stats.PoolRejected = xrtAtomic64Load(
		&pClient->PoolRejected,
		XMEMORY_RELAXED
	);
	#if defined(XRT_FEATURE_HTTP_CLIENT_REDIRECT)
		Stats.RedirectsFollowed = xrtAtomic64Load(
			&pClient->RedirectsFollowed,
			XMEMORY_RELAXED
		);
	#endif
	memcpy(pStats, &Stats, sizeof(Stats));
	return true;
}

#endif
