#include "../internal/xrt_net_port.h"



#if defined(XRT_FEATURE_NET_PORT_IOCP)

#if defined(_WIN32) || defined(_WIN64)

/* AcceptEx 为本地和远端地址各要求额外保留 16 字节。 */
#define XRT_NET_IOCP_ACCEPT_ADDRESS_SIZE \
	((DWORD)(sizeof(struct sockaddr_storage) + 16u))



/* 完成端口 owner 标识在进程生命周期内单调分配，零保留为未绑定。 */
static xatomic64 __xrtNetIOCPOwner = XRT_ATOMIC64_INIT(0);



typedef struct __xrt_net_iocp_operation __xrt_net_iocp_operation;



/* Accept 操作独占新 Socket，其他操作不承担这项状态。 */
typedef struct __xrt_net_iocp_accept_state {
	xnetsocket Accepted;
} __xrt_net_iocp_accept_state;



/* 地址操作保存平台地址、稳定地址和接收标志。 */
typedef struct __xrt_net_iocp_address_state {
	struct sockaddr_storage NativeAddress;
	int NativeAddressSize;
	DWORD ReceiveFlags;
	xnetaddr Address;
} __xrt_net_iocp_address_state;



/* 元数据接收只在显式 RECV_MSG 操作中保留 WSAMSG 和控制缓冲。 */
typedef struct __xrt_net_iocp_message_state {
	__xrt_net_iocp_address_state Address;
	WSAMSG Message;
	union {
		uint64 Align;
		unsigned char Data[XRT_NET_SOCKET_DGRAM_CONTROL_SIZE];
	} Control;
} __xrt_net_iocp_message_state;



/* 元数据发送额外保存控制值，保证提交返回后不再借用调用方描述符。 */
typedef struct __xrt_net_iocp_send_message_state {
	__xrt_net_iocp_address_state Address;
	xnetdgramcontrol SendControl;
	WSAMSG Message;
	union {
		uint64 Align;
		unsigned char Data[XRT_NET_SOCKET_DGRAM_CONTROL_SIZE];
	} Control;
} __xrt_net_iocp_send_message_state;



/* 流接收标志必须存活到异步完成。 */
typedef struct __xrt_net_iocp_receive_state {
	DWORD Flags;
	uint32 Reserved;
} __xrt_net_iocp_receive_state;



/* TransmitFile 状态借用 Stream 已复制的文件句柄直到完成。 */
typedef struct __xrt_net_iocp_file_state {
	HANDLE File;
	uint64 Offset;
	size_t Size;
} __xrt_net_iocp_file_state;



/* 一个操作只拥有描述符副本和平台状态，不拥有调用方 Socket 与数据缓冲。 */
struct __xrt_net_iocp_operation {
	OVERLAPPED Overlapped;
	__xrt_net_iocp_operation* ActiveNext;
	__xrt_net_iocp_operation* ActivePrevious;
	__xrt_net_iocp_operation* HashNext;
	__xrt_net_iocp_operation* HashPrevious;
	size_t AllocationSize;
	xnetporteventtype Type;
	xnetsocket Socket;
	uint64 Id;
	ptr User;
	size_t Capacity;
};



/* 空闲读探针必须稳定落入最小操作缓存尺寸类。 */
typedef char __xrt_net_iocp_operation_size[
	(sizeof(__xrt_net_iocp_operation) <= XRT_NET_PORT_CACHE_CLASS_MIN) ? 1 : -1
];



/* IOCP 上下文只维护完成端口、扩展入口和当前所有在途操作。 */
typedef struct __xrt_net_iocp_context {
	HANDLE Port;
	uint64 Owner;
	OVERLAPPED Wake;
	__xrt_net_iocp_operation* OperationHead;
	__xrt_net_iocp_operation** OperationBuckets;
	size_t OperationCount;
	size_t OperationBucketCount;
	size_t OperationBucketLimit;
	__xrt_net_port_cache OperationCache;
	LPFN_ACCEPTEX AcceptEx;
	LPFN_CONNECTEX ConnectEx;
	LPFN_TRANSMITFILE TransmitFile;
} __xrt_net_iocp_context;



/* 分配一个非零完成端口 owner 标识。 */
static uint64 __xrtNetIOCPOwnerId(void)
{
	uint64 Id;

	do {
		Id = xrtAtomic64FetchAdd(
			&__xrtNetIOCPOwner,
			1,
			XMEMORY_RELAXED
		) + 1u;
	} while ( Id == 0 );
	return Id;
}



/* 返回当前操作类型专属尾部状态的字节数。 */
static size_t __xrtNetIOCPOperationStateSize(xnetporteventtype Type)
{
	switch ( Type ) {
		case XNET_PORT_EVENT_ACCEPT:
			return sizeof(__xrt_net_iocp_accept_state);
		case XNET_PORT_EVENT_CONNECT:
		case XNET_PORT_EVENT_RECV_FROM:
		case XNET_PORT_EVENT_SEND_TO:
			return sizeof(__xrt_net_iocp_address_state);
		case XNET_PORT_EVENT_RECV_MSG:
			return sizeof(__xrt_net_iocp_message_state);
		case XNET_PORT_EVENT_SEND_MSG:
			return sizeof(__xrt_net_iocp_send_message_state);
		case XNET_PORT_EVENT_RECV:
			return sizeof(__xrt_net_iocp_receive_state);
		case XNET_PORT_EVENT_SEND_FILE:
		case XNET_PORT_EVENT_FILE_READ:
		case XNET_PORT_EVENT_FILE_WRITE:
			return sizeof(__xrt_net_iocp_file_state);
		default:
			return 0;
	}
}



/* 返回 Accept 操作独占的尾部状态。 */
static __xrt_net_iocp_accept_state* __xrtNetIOCPAcceptState(
	__xrt_net_iocp_operation* pOperation
)
{
	return (__xrt_net_iocp_accept_state*)(pOperation + 1);
}



/* 返回地址类操作独占的尾部状态。 */
static __xrt_net_iocp_address_state* __xrtNetIOCPAddressState(
	__xrt_net_iocp_operation* pOperation
)
{
	return (__xrt_net_iocp_address_state*)(pOperation + 1);
}



/* 返回元数据接收操作独占的 WSAMSG 状态。 */
static __xrt_net_iocp_message_state* __xrtNetIOCPMessageState(
	__xrt_net_iocp_operation* pOperation
)
{
	return (__xrt_net_iocp_message_state*)(pOperation + 1);
}



/* 返回元数据发送操作独占的 WSAMSG 和控制值状态。 */
static __xrt_net_iocp_send_message_state* __xrtNetIOCPSendMessageState(
	__xrt_net_iocp_operation* pOperation
)
{
	return (__xrt_net_iocp_send_message_state*)(pOperation + 1);
}



/* 返回流接收操作独占的尾部状态。 */
static __xrt_net_iocp_receive_state* __xrtNetIOCPReceiveState(
	__xrt_net_iocp_operation* pOperation
)
{
	return (__xrt_net_iocp_receive_state*)(pOperation + 1);
}



/* 返回文件发送操作的稳定尾部状态。 */
static __xrt_net_iocp_file_state* __xrtNetIOCPFileState(
	__xrt_net_iocp_operation* pOperation
)
{
	return (__xrt_net_iocp_file_state*)(pOperation + 1);
}



/* 返回操作尾部按需分配的 WSABUF 数组。 */
static WSABUF* __xrtNetIOCPBuffers(__xrt_net_iocp_operation* pOperation)
{
	return (WSABUF*)((bytes)(pOperation + 1) +
		__xrtNetIOCPOperationStateSize(pOperation->Type));
}



/* 返回 AcceptEx 操作尾部的地址缓冲。 */
static void* __xrtNetIOCPAcceptBuffer(__xrt_net_iocp_operation* pOperation)
{
	return (void*)((bytes)(pOperation + 1) +
		sizeof(__xrt_net_iocp_accept_state));
}



/* 混合顺序和随机 ID 的高低位，避免常见标识模式集中到少数桶。 */
static size_t __xrtNetIOCPHash(
	const __xrt_net_iocp_context* pContext, uint64 Id)
{
	return __xrtNetPortHashId(Id, pContext->OperationBucketCount);
}



/* 按操作标识从哈希桶查找在途节点。 */
static __xrt_net_iocp_operation* __xrtNetIOCPFind(
	__xrt_net_iocp_context* pContext, uint64 Id)
{
	__xrt_net_iocp_operation* pOperation =
		pContext->OperationBuckets[__xrtNetIOCPHash(pContext, Id)];

	while ( pOperation != NULL ) {
		if ( pOperation->Id == Id ) {
			return pOperation;
		}
		pOperation = pOperation->HashNext;
	}
	return NULL;
}



/* 在提交新操作前按活动数量扩展 ID 索引。 */
static bool __xrtNetIOCPIndexGrow(__xrt_net_iocp_context* pContext)
{
	size_t iNewCount = __xrtNetPortBucketNext(
		pContext->OperationCount + 1u,
		pContext->OperationBucketCount,
		pContext->OperationBucketLimit
	);
	__xrt_net_iocp_operation** pBuckets;
	__xrt_net_iocp_operation* pOperation;

	if ( iNewCount == pContext->OperationBucketCount ) {
		return true;
	}
	pBuckets = (__xrt_net_iocp_operation**)xrtCalloc(
		iNewCount,
		sizeof(*pBuckets)
	);
	if ( pBuckets == NULL ) {
		return false;
	}

	pOperation = pContext->OperationHead;
	while ( pOperation != NULL ) {
		size_t iBucket = __xrtNetPortHashId(pOperation->Id, iNewCount);

		pOperation->HashPrevious = NULL;
		pOperation->HashNext = pBuckets[iBucket];
		if ( pOperation->HashNext != NULL ) {
			pOperation->HashNext->HashPrevious = pOperation;
		}
		pBuckets[iBucket] = pOperation;
		pOperation = pOperation->ActiveNext;
	}
	xrtFree(pContext->OperationBuckets);
	pContext->OperationBuckets = pBuckets;
	pContext->OperationBucketCount = iNewCount;
	return true;
}



/* 把操作加入 owner 线程独占的活跃双链和 ID 哈希桶。 */
static void __xrtNetIOCPTrack(__xrt_net_iocp_context* pContext,
	__xrt_net_iocp_operation* pOperation)
{
	size_t iBucket = __xrtNetIOCPHash(pContext, pOperation->Id);

	pOperation->ActiveNext = pContext->OperationHead;
	if ( pContext->OperationHead != NULL ) {
		pContext->OperationHead->ActivePrevious = pOperation;
	}
	pContext->OperationHead = pOperation;

	pOperation->HashNext = pContext->OperationBuckets[iBucket];
	if ( pOperation->HashNext != NULL ) {
		pOperation->HashNext->HashPrevious = pOperation;
	}
	pContext->OperationBuckets[iBucket] = pOperation;
	pContext->OperationCount++;
}



/* 从活跃双链和哈希桶 O(1) 移除已经产生终态的操作。 */
static void __xrtNetIOCPUntrack(__xrt_net_iocp_context* pContext,
	__xrt_net_iocp_operation* pOperation)
{
	size_t iBucket = __xrtNetIOCPHash(pContext, pOperation->Id);

	if ( pOperation->ActivePrevious != NULL ) {
		pOperation->ActivePrevious->ActiveNext = pOperation->ActiveNext;
	} else {
		pContext->OperationHead = pOperation->ActiveNext;
	}
	if ( pOperation->ActiveNext != NULL ) {
		pOperation->ActiveNext->ActivePrevious =
			pOperation->ActivePrevious;
	}

	if ( pOperation->HashPrevious != NULL ) {
		pOperation->HashPrevious->HashNext = pOperation->HashNext;
	} else {
		pContext->OperationBuckets[iBucket] = pOperation->HashNext;
	}
	if ( pOperation->HashNext != NULL ) {
		pOperation->HashNext->HashPrevious = pOperation->HashPrevious;
	}
	pContext->OperationCount--;
}



/* 用统一的提交错误保留 Winsock 或 Win32 系统码。 */
static void __xrtNetIOCPSubmitError(cstr sOperation,
	cstr sMessage, int iSystemCode)
{
	__xrtNetSocketSetSystemError(XNET_ERROR_PORT_SUBMIT,
		sOperation, sMessage, iSystemCode);
}



/* 按事件类别返回稳定的提交操作名。 */
static cstr __xrtNetIOCPOperationName(xnetporteventtype Type)
{
	switch ( Type ) {
		case XNET_PORT_EVENT_ACCEPT:
			return "accept";
		case XNET_PORT_EVENT_CONNECT:
			return "connect";
		case XNET_PORT_EVENT_READ_PROBE:
			return "read-probe";
		case XNET_PORT_EVENT_RECV:
			return "recv";
		case XNET_PORT_EVENT_SEND:
			return "send";
		case XNET_PORT_EVENT_SEND_FILE:
			return "send-file";
		case XNET_PORT_EVENT_FILE_READ:
			return "read-file";
		case XNET_PORT_EVENT_FILE_WRITE:
			return "write-file";
		case XNET_PORT_EVENT_RECV_FROM:
			return "recv-from";
		case XNET_PORT_EVENT_RECV_MSG:
			return "recv-message";
		case XNET_PORT_EVENT_SEND_TO:
			return "send-to";
		case XNET_PORT_EVENT_SEND_MSG:
			return "send-message";
		default:
			return "submit";
	}
}



/* 计算操作尾部实际需要的描述符或 AcceptEx 地址空间。 */
static size_t __xrtNetIOCPOperationExtra(
	const __xrt_net_port_submit* pSubmit)
{
	size_t iExtra = __xrtNetIOCPOperationStateSize(pSubmit->Type);

	if ( pSubmit->Type == XNET_PORT_EVENT_ACCEPT ) {
		iExtra += (size_t)XRT_NET_IOCP_ACCEPT_ADDRESS_SIZE * 2u;
	}
	if ( pSubmit->SpanCount != 0 ) {
		iExtra += pSubmit->SpanCount * sizeof(WSABUF);
	}
	return iExtra;
}



/* 分配操作并复制 Span 描述符和远端地址，不复制任何载荷。 */
static __xrt_net_iocp_operation* __xrtNetIOCPOperationCreate(
	xnetport* pPort, const __xrt_net_port_submit* pSubmit)
{
	__xrt_net_iocp_context* pContext =
		(__xrt_net_iocp_context*)pPort->Context;
	__xrt_net_iocp_operation* pOperation;
	WSABUF* pBuffers;
	size_t iExtra;
	size_t iAllocation;

	if ( pContext->OperationCount >= pPort->Config.OperationLimit ) {
		__xrtNetSetError(XERR_AGAIN, XNET_ERROR_PORT_SUBMIT,
			__xrtNetIOCPOperationName(pSubmit->Type),
			"network completion operation limit reached", 0);
		return NULL;
	}
	if ( __xrtNetIOCPFind(pContext, pSubmit->Id) != NULL ) {
		__xrtNetSetError(XERR_EXISTS, XNET_ERROR_PORT_SUBMIT,
			__xrtNetIOCPOperationName(pSubmit->Type),
			"network completion operation id is already active", 0);
		return NULL;
	}
	if ( !__xrtNetIOCPIndexGrow(pContext) ) {
		return NULL;
	}

	iExtra = __xrtNetIOCPOperationExtra(pSubmit);
	pOperation = (__xrt_net_iocp_operation*)__xrtNetPortCacheAlloc(
		&pContext->OperationCache,
		sizeof(*pOperation) + iExtra,
		&iAllocation
	);
	if ( pOperation == NULL ) {
		return NULL;
	}
	pOperation->AllocationSize = iAllocation;
	pOperation->Type = pSubmit->Type;
	pOperation->Socket = pSubmit->Socket;
	pOperation->Id = pSubmit->Id;
	pOperation->User = pSubmit->User;
	if ( (pSubmit->Type == XNET_PORT_EVENT_SEND_FILE) ||
		 (pSubmit->Type == XNET_PORT_EVENT_FILE_READ) ||
		 (pSubmit->Type == XNET_PORT_EVENT_FILE_WRITE) ) {
		__xrt_net_iocp_file_state* pFile =
			__xrtNetIOCPFileState(pOperation);

		pFile->File = (HANDLE)(uintptr_t)pSubmit->File;
		pFile->Offset = pSubmit->FileOffset;
		pFile->Size = (pSubmit->Type == XNET_PORT_EVENT_SEND_FILE) ?
			pSubmit->FileSize : 0;
		if ( pSubmit->Type == XNET_PORT_EVENT_SEND_FILE ) {
			pOperation->Capacity = pSubmit->FileSize;
		}
		pOperation->Overlapped.Offset = (DWORD)pSubmit->FileOffset;
		pOperation->Overlapped.OffsetHigh =
			(DWORD)(pSubmit->FileOffset >> 32);
	}

	/* WSABUF 只保存调用方内存地址，生命周期由公共契约约束。 */
	pBuffers = __xrtNetIOCPBuffers(pOperation);
	for ( size_t i = 0; i < pSubmit->SpanCount; i++ ) {
		if ( pSubmit->ReadSpans != NULL ) {
			pBuffers[i].buf = (CHAR*)pSubmit->ReadSpans[i].Data;
			pBuffers[i].len = (ULONG)pSubmit->ReadSpans[i].Size;
			pOperation->Capacity += pSubmit->ReadSpans[i].Size;
		} else {
			pBuffers[i].buf = (CHAR*)pSubmit->WriteSpans[i].Data;
			pBuffers[i].len = (ULONG)pSubmit->WriteSpans[i].Size;
			pOperation->Capacity += pSubmit->WriteSpans[i].Size;
		}
	}

	/* Connect 与 SendTo 在提交时复制地址，调用方随后可以复用原对象。 */
	if ( pSubmit->Address != NULL ) {
		__xrt_net_iocp_address_state* pAddress =
			__xrtNetIOCPAddressState(pOperation);
		size_t iSize = sizeof(pAddress->NativeAddress);

		pAddress->Address = *pSubmit->Address;
		if ( !xrtNetAddrToNative(pSubmit->Address,
			&pAddress->NativeAddress, &iSize) ) {
			__xrtNetPortCacheFree(&pContext->OperationCache,
				pOperation, pOperation->AllocationSize);
			__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_PORT_SUBMIT,
				__xrtNetIOCPOperationName(pSubmit->Type),
				"invalid native network operation address", 0);
			return NULL;
		}
		pAddress->NativeAddressSize = (int)iSize;
	}
	if ( pSubmit->Type == XNET_PORT_EVENT_SEND_MSG ) {
		__xrtNetIOCPSendMessageState(pOperation)->SendControl =
			*pSubmit->Control;
	}
	return pOperation;
}



/* 把 Socket 永久关联到当前完成端口；重复关联到同一端口是幂等的。 */
static bool __xrtNetIOCPAssociate(__xrt_net_iocp_context* pContext,
	xnetsocket Socket, cstr sOperation)
{
	HANDLE hResult;

	if ( Socket->CompletionOwner == pContext->Owner ) {
		return true;
	}
	if ( Socket->CompletionOwner != 0 ) {
		__xrtNetSetError(XERR_EXISTS, XNET_ERROR_PORT_SUBMIT,
			sOperation, "socket is already associated with another IOCP", 0);
		return false;
	}
	hResult = CreateIoCompletionPort((HANDLE)(uintptr_t)Socket->Native,
		pContext->Port, 0, 0);

	if ( hResult == pContext->Port ) {
		Socket->CompletionOwner = pContext->Owner;
		return true;
	}
	__xrtNetIOCPSubmitError(sOperation,
		"associating socket with IOCP failed", (int)GetLastError());
	return false;
}



/* 把完成式文件句柄关联到当前端口；重复关联到同一端口由 Windows 幂等接受。 */
static bool __xrtNetIOCPAssociateFile(
	__xrt_net_iocp_context* pContext,
	HANDLE hFile,
	cstr sOperation
)
{
	HANDLE hResult = CreateIoCompletionPort(hFile, pContext->Port, 0, 0);

	if ( hResult == pContext->Port ) {
		return true;
	}
	__xrtNetIOCPSubmitError(
		sOperation,
		"associating file with IOCP failed",
		(int)GetLastError()
	);
	return false;
}



/* 从任意 Winsock Socket 获取指定扩展函数。 */
static bool __xrtNetIOCPExtension(SOCKET hSocket, const GUID* pId,
	void* pFunction, DWORD iFunctionSize, cstr sOperation)
{
	DWORD iBytes = 0;

	if ( WSAIoctl(hSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
		(void*)pId, sizeof(*pId), pFunction, iFunctionSize,
		&iBytes, NULL, NULL) == 0 ) {
		return true;
	}
	__xrtNetIOCPSubmitError(sOperation,
		"loading Winsock extension function failed", WSAGetLastError());
	return false;
}



/* 延迟加载 AcceptEx，避免没有服务端操作时引入额外初始化。 */
static LPFN_ACCEPTEX __xrtNetIOCPAcceptEx(
	__xrt_net_iocp_context* pContext, SOCKET hSocket)
{
	if ( pContext->AcceptEx == NULL ) {
		GUID Id = WSAID_ACCEPTEX;

		if ( !__xrtNetIOCPExtension(hSocket, &Id,
			&pContext->AcceptEx, sizeof(pContext->AcceptEx), "accept") ) {
			return NULL;
		}
	}
	return pContext->AcceptEx;
}



/* 延迟加载 ConnectEx，客户端未使用时不做扩展查询。 */
static LPFN_CONNECTEX __xrtNetIOCPConnectEx(
	__xrt_net_iocp_context* pContext, SOCKET hSocket)
{
	if ( pContext->ConnectEx == NULL ) {
		GUID Id = WSAID_CONNECTEX;

		if ( !__xrtNetIOCPExtension(hSocket, &Id,
			&pContext->ConnectEx, sizeof(pContext->ConnectEx), "connect") ) {
			return NULL;
		}
	}
	return pContext->ConnectEx;
}



/* 延迟加载 TransmitFile，普通网络路径不承担扩展查询成本。 */
static LPFN_TRANSMITFILE __xrtNetIOCPTransmitFile(
	__xrt_net_iocp_context* pContext,
	SOCKET hSocket
)
{
	if ( pContext->TransmitFile == NULL ) {
		GUID Id = WSAID_TRANSMITFILE;

		if ( !__xrtNetIOCPExtension(
			hSocket,
			&Id,
			&pContext->TransmitFile,
			sizeof(pContext->TransmitFile),
			"send-file"
		) ) {
			return NULL;
		}
	}
	return pContext->TransmitFile;
}



/* ConnectEx 要求尚未绑定的 Socket 先绑定同地址族的任意本地地址。 */
static bool __xrtNetIOCPBindConnect(xnetsocket Socket)
{
	SOCKET hSocket = (SOCKET)Socket->Native;
	struct sockaddr_storage Storage;
	int iSize = sizeof(Storage);
	int iCode;

	memset(&Storage, 0, sizeof(Storage));
	if ( getsockname(hSocket, (struct sockaddr*)&Storage, &iSize) == 0 ) {
		return true;
	}
	iCode = WSAGetLastError();
	if ( iCode != WSAEINVAL ) {
		__xrtNetIOCPSubmitError("connect",
			"querying local connect address failed", iCode);
		return false;
	}

	memset(&Storage, 0, sizeof(Storage));
	if ( Socket->Family == XNET_FAMILY_IPV4 ) {
		((struct sockaddr_in*)&Storage)->sin_family = AF_INET;
		iSize = sizeof(struct sockaddr_in);
	} else {
		((struct sockaddr_in6*)&Storage)->sin6_family = AF_INET6;
		iSize = sizeof(struct sockaddr_in6);
	}
	if ( bind(hSocket, (struct sockaddr*)&Storage, iSize) == 0 ) {
		return true;
	}
	__xrtNetIOCPSubmitError("connect",
		"binding ConnectEx socket failed", WSAGetLastError());
	return false;
}



/* 判断 Overlapped API 返回值是否代表操作已经被系统接管。 */
static bool __xrtNetIOCPIssued(bool bImmediate,
	cstr sOperation, cstr sMessage)
{
	int iCode;

	if ( bImmediate ) {
		return true;
	}
	iCode = WSAGetLastError();
	if ( iCode == WSA_IO_PENDING ) {
		return true;
	}
	__xrtNetIOCPSubmitError(sOperation, sMessage, iCode);
	return false;
}



/* 发起一次原生 AcceptEx，接受 Socket 由操作持有直到终态。 */
static bool __xrtNetIOCPIssueAccept(__xrt_net_iocp_context* pContext,
	__xrt_net_iocp_operation* pOperation)
{
	__xrt_net_iocp_accept_state* pAccept =
		__xrtNetIOCPAcceptState(pOperation);
	SOCKET hListen = (SOCKET)pOperation->Socket->Native;
	LPFN_ACCEPTEX pAcceptEx = __xrtNetIOCPAcceptEx(pContext, hListen);
	DWORD iBytes = 0;
	BOOL bResult;

	if ( pAcceptEx == NULL ) {
		return false;
	}
	pAccept->Accepted = xrtNetSocketOpen(
		pOperation->Socket->Family, XNET_SOCKET_STREAM,
		pOperation->Socket->Flags);
	if ( pAccept->Accepted == NULL ) {
		return false;
	}
	bResult = pAcceptEx(hListen,
		(SOCKET)pAccept->Accepted->Native,
		__xrtNetIOCPAcceptBuffer(pOperation), 0,
		XRT_NET_IOCP_ACCEPT_ADDRESS_SIZE,
		XRT_NET_IOCP_ACCEPT_ADDRESS_SIZE,
		&iBytes, &pOperation->Overlapped);
	return __xrtNetIOCPIssued(bResult != FALSE,
		"accept", "submitting AcceptEx failed");
}



/* 发起一次原生 ConnectEx，并标记 Socket 正在连接。 */
static bool __xrtNetIOCPIssueConnect(__xrt_net_iocp_context* pContext,
	__xrt_net_iocp_operation* pOperation)
{
	__xrt_net_iocp_address_state* pAddress =
		__xrtNetIOCPAddressState(pOperation);
	SOCKET hSocket = (SOCKET)pOperation->Socket->Native;
	LPFN_CONNECTEX pConnectEx;
	BOOL bResult;

	if ( pOperation->Socket->Connecting ) {
		__xrtNetSetError(XERR_AGAIN, XNET_ERROR_PORT_SUBMIT,
			"connect", "socket already has an active connect", 0);
		return false;
	}
	if ( !__xrtNetIOCPBindConnect(pOperation->Socket) ) {
		return false;
	}
	pConnectEx = __xrtNetIOCPConnectEx(pContext, hSocket);
	if ( pConnectEx == NULL ) {
		return false;
	}

	pOperation->Socket->Connecting = true;
	bResult = pConnectEx(hSocket,
		(const struct sockaddr*)&pAddress->NativeAddress,
		pAddress->NativeAddressSize, NULL, 0, NULL,
		&pOperation->Overlapped);
	if ( !__xrtNetIOCPIssued(bResult != FALSE,
		"connect", "submitting ConnectEx failed") ) {
		pOperation->Socket->Connecting = false;
		return false;
	}
	return true;
}



/* 用零字节 WSARecv 等待流 Socket 可读，不占用载荷缓冲。 */
static bool __xrtNetIOCPIssueReadProbe(
	__xrt_net_iocp_operation* pOperation
)
{
	WSABUF Buffer;
	DWORD iBytes = 0;
	DWORD iFlags = 0;
	int iResult;

	memset(&Buffer, 0, sizeof(Buffer));
	iResult = WSARecv(
		(SOCKET)pOperation->Socket->Native,
		&Buffer,
		1,
		&iBytes,
		&iFlags,
		&pOperation->Overlapped,
		NULL
	);
	return __xrtNetIOCPIssued(
		iResult == 0,
		"read-probe",
		"submitting zero-byte WSARecv failed"
	);
}



/* 直接把 WSARecv 写入调用方 Span。 */
static bool __xrtNetIOCPIssueRecv(__xrt_net_iocp_operation* pOperation,
	DWORD iCount)
{
	__xrt_net_iocp_receive_state* pReceive =
		__xrtNetIOCPReceiveState(pOperation);
	DWORD iBytes = 0;
	int iResult;

	pReceive->Flags = 0;
	iResult = WSARecv((SOCKET)pOperation->Socket->Native,
		__xrtNetIOCPBuffers(pOperation), iCount, &iBytes,
		&pReceive->Flags, &pOperation->Overlapped, NULL);
	return __xrtNetIOCPIssued(iResult == 0,
		"recv", "submitting WSARecv failed");
}



/* 直接从调用方 Span 发起 WSASend。 */
static bool __xrtNetIOCPIssueSend(__xrt_net_iocp_operation* pOperation,
	DWORD iCount)
{
	DWORD iBytes = 0;
	int iResult = WSASend((SOCKET)pOperation->Socket->Native,
		__xrtNetIOCPBuffers(pOperation), iCount, &iBytes,
		0, &pOperation->Overlapped, NULL);

	return __xrtNetIOCPIssued(iResult == 0,
		"send", "submitting WSASend failed");
}



/* 由 Socket 的 IOCP 关联接收 TransmitFile 唯一完成包。 */
static bool __xrtNetIOCPIssueSendFile(
	__xrt_net_iocp_context* pContext,
	__xrt_net_iocp_operation* pOperation
)
{
	__xrt_net_iocp_file_state* pFile =
		__xrtNetIOCPFileState(pOperation);
	LPFN_TRANSMITFILE pTransmit = __xrtNetIOCPTransmitFile(
		pContext,
		(SOCKET)pOperation->Socket->Native
	);
	DWORD iSize = pFile->Size > (size_t)UINT32_MAX ?
		UINT32_MAX : (DWORD)pFile->Size;
	BOOL bResult;

	if ( pTransmit == NULL ) {
		return false;
	}
	bResult = pTransmit(
		(SOCKET)pOperation->Socket->Native,
		pFile->File,
		iSize,
		0,
		&pOperation->Overlapped,
		NULL,
		0
	);

	return __xrtNetIOCPIssued(
		bResult != 0,
		"send-file",
		"submitting TransmitFile failed"
	);
}



/* 从绝对偏移直接读取到调用方缓冲。 */
static bool __xrtNetIOCPIssueFileRead(
	__xrt_net_iocp_operation* pOperation
)
{
	__xrt_net_iocp_file_state* pFile =
		__xrtNetIOCPFileState(pOperation);
	WSABUF* pBuffer = __xrtNetIOCPBuffers(pOperation);
	BOOL bResult = ReadFile(
		pFile->File,
		pBuffer[0].buf,
		pBuffer[0].len,
		NULL,
		&pOperation->Overlapped
	);
	DWORD iCode;

	if ( bResult != 0 ) {
		return true;
	}
	iCode = GetLastError();
	if ( iCode == ERROR_IO_PENDING ) {
		return true;
	}
	__xrtNetIOCPSubmitError(
		"read-file",
		"submitting ReadFile failed",
		(int)iCode
	);
	return false;
}



/* 从调用方缓冲向绝对偏移直接写入。 */
static bool __xrtNetIOCPIssueFileWrite(
	__xrt_net_iocp_operation* pOperation
)
{
	__xrt_net_iocp_file_state* pFile =
		__xrtNetIOCPFileState(pOperation);
	WSABUF* pBuffer = __xrtNetIOCPBuffers(pOperation);
	BOOL bResult = WriteFile(
		pFile->File,
		pBuffer[0].buf,
		pBuffer[0].len,
		NULL,
		&pOperation->Overlapped
	);
	DWORD iCode;

	if ( bResult != 0 ) {
		return true;
	}
	iCode = GetLastError();
	if ( iCode == ERROR_IO_PENDING ) {
		return true;
	}
	__xrtNetIOCPSubmitError(
		"write-file",
		"submitting WriteFile failed",
		(int)iCode
	);
	return false;
}



/* 直接把 WSARecvFrom 写入调用方 Span，并由操作保存远端地址。 */
static bool __xrtNetIOCPIssueRecvFrom(
	__xrt_net_iocp_operation* pOperation, DWORD iCount)
{
	__xrt_net_iocp_address_state* pAddress =
		__xrtNetIOCPAddressState(pOperation);
	DWORD iBytes = 0;
	int iResult;

	pAddress->NativeAddressSize = sizeof(pAddress->NativeAddress);
	pAddress->ReceiveFlags = 0;
	iResult = WSARecvFrom((SOCKET)pOperation->Socket->Native,
		__xrtNetIOCPBuffers(pOperation), iCount, &iBytes,
		&pAddress->ReceiveFlags,
		(struct sockaddr*)&pAddress->NativeAddress,
		&pAddress->NativeAddressSize,
		&pOperation->Overlapped, NULL);
	return __xrtNetIOCPIssued(iResult == 0,
		"recv-from", "submitting WSARecvFrom failed");
}



/* 通过 WSARecvMsg 异步接收地址和可选控制消息。 */
static bool __xrtNetIOCPIssueRecvMsg(
	__xrt_net_iocp_operation* pOperation,
	DWORD iCount
)
{
	__xrt_net_iocp_message_state* pState =
		__xrtNetIOCPMessageState(pOperation);
	LPFN_WSARECVMSG pReceive = NULL;
	uintptr_t iReceive = __xrtNetSocketReceiveMessage(pOperation->Socket);
	DWORD iBytes = 0;
	int iResult;

	if ( iReceive == 0 ) {
		return false;
	}
	memcpy(&pReceive, &iReceive, sizeof(pReceive));
	pState->Address.NativeAddressSize =
		(int)sizeof(pState->Address.NativeAddress);
	memset(&pState->Message, 0, sizeof(pState->Message));
	memset(&pState->Control, 0, sizeof(pState->Control));
	pState->Message.name =
		(struct sockaddr*)&pState->Address.NativeAddress;
	pState->Message.namelen = pState->Address.NativeAddressSize;
	pState->Message.lpBuffers = __xrtNetIOCPBuffers(pOperation);
	pState->Message.dwBufferCount = iCount;
	pState->Message.Control.buf = (CHAR*)pState->Control.Data;
	pState->Message.Control.len = (ULONG)sizeof(pState->Control.Data);
	iResult = pReceive(
		(SOCKET)pOperation->Socket->Native,
		&pState->Message,
		&iBytes,
		&pOperation->Overlapped,
		NULL
	);
	return __xrtNetIOCPIssued(
		iResult == 0,
		"recv-message",
		"submitting WSARecvMsg failed"
	);
}



/* 直接从调用方 Span 和已复制地址发起 WSASendTo。 */
static bool __xrtNetIOCPIssueSendTo(
	__xrt_net_iocp_operation* pOperation, DWORD iCount)
{
	__xrt_net_iocp_address_state* pAddress =
		__xrtNetIOCPAddressState(pOperation);
	DWORD iBytes = 0;
	int iResult = WSASendTo((SOCKET)pOperation->Socket->Native,
		__xrtNetIOCPBuffers(pOperation), iCount, &iBytes, 0,
		(const struct sockaddr*)&pAddress->NativeAddress,
		pAddress->NativeAddressSize,
		&pOperation->Overlapped, NULL);

	return __xrtNetIOCPIssued(iResult == 0,
		"send-to", "submitting WSASendTo failed");
}



/* 通过 WSASendMsg 异步发送已复制的地址和逐包控制值。 */
static bool __xrtNetIOCPIssueSendMsg(
	__xrt_net_iocp_operation* pOperation,
	DWORD iCount
)
{
	__xrt_net_iocp_send_message_state* pState =
		__xrtNetIOCPSendMessageState(pOperation);
	LPFN_WSASENDMSG pSend = NULL;
	uintptr_t iSend = __xrtNetSocketSendMessage(pOperation->Socket);
	size_t iControlSize = 0;
	DWORD iBytes = 0;
	int iResult;

	if ( (iSend == 0) || !__xrtNetSocketDgramControlBuild(
		pOperation->Socket,
		&pState->SendControl,
		pOperation->Capacity,
		pState->Control.Data,
		sizeof(pState->Control.Data),
		&iControlSize,
		XNET_ERROR_PORT_SUBMIT,
		"send-message"
	) ) {
		return false;
	}
	memcpy(&pSend, &iSend, sizeof(pSend));
	memset(&pState->Message, 0, sizeof(pState->Message));
	if ( pState->Address.NativeAddressSize != 0 ) {
		pState->Message.name =
			(struct sockaddr*)&pState->Address.NativeAddress;
		pState->Message.namelen = pState->Address.NativeAddressSize;
	}
	pState->Message.lpBuffers = __xrtNetIOCPBuffers(pOperation);
	pState->Message.dwBufferCount = iCount;
	pState->Message.Control.buf = (CHAR*)pState->Control.Data;
	pState->Message.Control.len = (ULONG)iControlSize;
	iResult = pSend(
		(SOCKET)pOperation->Socket->Native,
		&pState->Message,
		0,
		&iBytes,
		&pOperation->Overlapped,
		NULL
	);
	return __xrtNetIOCPIssued(
		iResult == 0,
		"send-message",
		"submitting WSASendMsg failed"
	);
}



/* 初始化原生完成端口和有界操作 ID 索引。 */
static bool __xrtNetIOCPInit(xnetport* pPort)
{
	__xrt_net_iocp_context* pContext;

	pContext = (__xrt_net_iocp_context*)xrtMalloc(sizeof(*pContext));
	if ( pContext == NULL ) {
		return false;
	}
	memset(pContext, 0, sizeof(*pContext));
	pContext->Owner = __xrtNetIOCPOwnerId();
	pContext->OperationCache.Limit = pPort->Config.OperationCache;
	pContext->Port = CreateIoCompletionPort(
		INVALID_HANDLE_VALUE, NULL, 0, 0);
	if ( pContext->Port == NULL ) {
		int iCode = (int)GetLastError();

		xrtFree(pContext);
		__xrtNetSocketSetSystemError(XNET_ERROR_PORT_CREATE,
			"create-port", "creating IOCP failed", iCode);
		return false;
	}
	pContext->OperationBucketCount = XRT_NET_PORT_BUCKET_MIN;
	pContext->OperationBucketLimit =
		__xrtNetPortBucketCount(pPort->Config.OperationLimit);
	pContext->OperationBuckets =
		(__xrt_net_iocp_operation**)xrtCalloc(
			pContext->OperationBucketCount,
			sizeof(*pContext->OperationBuckets));
	if ( pContext->OperationBuckets == NULL ) {
		(void)CloseHandle(pContext->Port);
		xrtFree(pContext);
		return false;
	}
	pPort->Context = pContext;
	return true;
}



/* 提交一个完成式操作；失败不会留下在途节点或终态事件。 */
static bool __xrtNetIOCPSubmit(xnetport* pPort,
	const __xrt_net_port_submit* pSubmit)
{
	__xrt_net_iocp_context* pContext =
		(__xrt_net_iocp_context*)pPort->Context;
	__xrt_net_iocp_operation* pOperation;
	bool bResult = false;
	cstr sOperation = __xrtNetIOCPOperationName(pSubmit->Type);

	pOperation = __xrtNetIOCPOperationCreate(pPort, pSubmit);
	if ( pOperation == NULL ) {
		return false;
	}
	if ( (pOperation->Type == XNET_PORT_EVENT_FILE_READ) ||
		 (pOperation->Type == XNET_PORT_EVENT_FILE_WRITE) ) {
		if ( !*pSubmit->FileAssociated ) {
			if ( !__xrtNetIOCPAssociateFile(
				pContext,
				__xrtNetIOCPFileState(pOperation)->File,
				sOperation
			) ) {
				__xrtNetPortCacheFree(&pContext->OperationCache,
					pOperation, pOperation->AllocationSize);
				return false;
			}
			*pSubmit->FileAssociated = true;
		}
	} else if ( !__xrtNetIOCPAssociate(
		pContext,
		pOperation->Socket,
		sOperation
	) ) {
		__xrtNetPortCacheFree(&pContext->OperationCache,
			pOperation, pOperation->AllocationSize);
		return false;
	}

	/* 先进入在途链，保证立即完成的系统操作也具有可提取身份。 */
	__xrtNetIOCPTrack(pContext, pOperation);
	switch ( pOperation->Type ) {
		case XNET_PORT_EVENT_ACCEPT:
			bResult = __xrtNetIOCPIssueAccept(pContext, pOperation);
			break;
		case XNET_PORT_EVENT_CONNECT:
			bResult = __xrtNetIOCPIssueConnect(pContext, pOperation);
			break;
		case XNET_PORT_EVENT_READ_PROBE:
			bResult = __xrtNetIOCPIssueReadProbe(pOperation);
			break;
		case XNET_PORT_EVENT_RECV:
			bResult = __xrtNetIOCPIssueRecv(pOperation,
				(DWORD)pSubmit->SpanCount);
			break;
		case XNET_PORT_EVENT_SEND:
			bResult = __xrtNetIOCPIssueSend(pOperation,
				(DWORD)pSubmit->SpanCount);
			break;
		case XNET_PORT_EVENT_SEND_FILE:
			bResult = __xrtNetIOCPIssueSendFile(pContext, pOperation);
			break;
		case XNET_PORT_EVENT_FILE_READ:
			bResult = __xrtNetIOCPIssueFileRead(pOperation);
			break;
		case XNET_PORT_EVENT_FILE_WRITE:
			bResult = __xrtNetIOCPIssueFileWrite(pOperation);
			break;
		case XNET_PORT_EVENT_RECV_FROM:
			bResult = __xrtNetIOCPIssueRecvFrom(pOperation,
				(DWORD)pSubmit->SpanCount);
			break;
		case XNET_PORT_EVENT_RECV_MSG:
			bResult = __xrtNetIOCPIssueRecvMsg(pOperation,
				(DWORD)pSubmit->SpanCount);
			break;
		case XNET_PORT_EVENT_SEND_TO:
			bResult = __xrtNetIOCPIssueSendTo(pOperation,
				(DWORD)pSubmit->SpanCount);
			break;
		case XNET_PORT_EVENT_SEND_MSG:
			bResult = __xrtNetIOCPIssueSendMsg(pOperation,
				(DWORD)pSubmit->SpanCount);
			break;
		default:
			__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_PORT_SUBMIT,
				"submit", "unsupported IOCP operation type", 0);
			break;
	}
	if ( bResult ) {
		return true;
	}

	__xrtNetIOCPUntrack(pContext, pOperation);
	if ( pOperation->Type == XNET_PORT_EVENT_ACCEPT ) {
		__xrt_net_iocp_accept_state* pAccept =
			__xrtNetIOCPAcceptState(pOperation);

		if ( pAccept->Accepted != NULL ) {
			(void)xrtNetSocketClose(pAccept->Accepted);
		}
	}
	if ( pOperation->Type == XNET_PORT_EVENT_CONNECT ) {
		pOperation->Socket->Connecting = false;
	}
	__xrtNetPortCacheFree(&pContext->OperationCache,
		pOperation, pOperation->AllocationSize);
	return false;
}



/* 请求取消指定操作；无论取消是否抢先，原操作仍只产生一个终态。 */
static bool __xrtNetIOCPCancel(xnetport* pPort, uint64 Id)
{
	__xrt_net_iocp_context* pContext =
		(__xrt_net_iocp_context*)pPort->Context;
	__xrt_net_iocp_operation* pOperation =
		__xrtNetIOCPFind(pContext, Id);
	DWORD iCode;

	if ( pOperation == NULL ) {
		__xrtNetSetError(XERR_NOT_FOUND, XNET_ERROR_PORT_CANCEL,
			"cancel", "network completion operation id was not found", 0);
		return false;
	}
	if ( CancelIoEx(
		((pOperation->Type == XNET_PORT_EVENT_FILE_READ) ||
		 (pOperation->Type == XNET_PORT_EVENT_FILE_WRITE)) ?
			__xrtNetIOCPFileState(pOperation)->File :
			(HANDLE)(uintptr_t)pOperation->Socket->Native,
		&pOperation->Overlapped) != 0 ) {
		return true;
	}
	iCode = GetLastError();
	if ( iCode == ERROR_NOT_FOUND ) {
		return true;
	}
	__xrtNetSocketSetSystemError(XNET_ERROR_PORT_CANCEL,
		"cancel", "cancelling IOCP operation failed", (int)iCode);
	return false;
}



/* 把微秒等待向上取整为毫秒，避免有限截止时间被提前截断。 */
static DWORD __xrtNetIOCPTimeout(uint64 iTimeout)
{
	uint64 iMilliseconds;

	if ( iTimeout == UINT64_MAX ) {
		return INFINITE;
	}
	iMilliseconds = (iTimeout / 1000u) +
		((iTimeout % 1000u) != 0 ? 1u : 0u);
	if ( iMilliseconds >= (uint64)INFINITE ) {
		return INFINITE - 1u;
	}
	return (DWORD)iMilliseconds;
}



/* 把接受成功后的原生句柄转为拥有明确所有权的 xnetsocket。 */
static bool __xrtNetIOCPFinishAccept(
	__xrt_net_iocp_operation* pOperation, xnetportevent* pEvent)
{
	__xrt_net_iocp_accept_state* pAccept =
		__xrtNetIOCPAcceptState(pOperation);
	SOCKET hListen = (SOCKET)pOperation->Socket->Native;
	xnetsocket Accepted = pAccept->Accepted;
	SOCKET hAccepted = (SOCKET)Accepted->Native;

	if ( setsockopt(hAccepted, SOL_SOCKET,
		SO_UPDATE_ACCEPT_CONTEXT, (const char*)&hListen,
		sizeof(hListen)) != 0 ) {
		pEvent->SystemCode = WSAGetLastError();
		(void)xrtNetSocketClose(Accepted);
		pAccept->Accepted = NULL;
		return false;
	}
	if ( !xrtNetSocketRemote(Accepted, &pEvent->Address) ) {
		const xerror* pError = xrtGetError();

		pEvent->SystemCode = pError != NULL ?
			xrtErrorSystemCode(pError) : 0;
		(void)xrtNetSocketClose(Accepted);
		pAccept->Accepted = NULL;
		return false;
	}
	pEvent->Accepted = Accepted;
	pAccept->Accepted = NULL;
	return true;
}



/* 完成 ConnectEx 的 Socket 上下文更新。 */
static bool __xrtNetIOCPFinishConnect(
	__xrt_net_iocp_operation* pOperation, xnetportevent* pEvent)
{
	pOperation->Socket->Connecting = false;
	if ( setsockopt((SOCKET)pOperation->Socket->Native,
		SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0) == 0 ) {
		return true;
	}
	pEvent->SystemCode = WSAGetLastError();
	return false;
}



/* 将一次系统完成包转换为稳定端口事件。 */
static void __xrtNetIOCPEvent(__xrt_net_iocp_operation* pOperation,
	BOOL bSuccess, DWORD iBytes, int iSystemCode, xnetportevent* pEvent)
{
	memset(pEvent, 0, sizeof(*pEvent));
	pEvent->Type = pOperation->Type;
	pEvent->Result = XNET_RESULT_OK;
	pEvent->SystemCode = bSuccess ? 0 : iSystemCode;
	pEvent->Bytes = (size_t)iBytes;
	pEvent->Id = pOperation->Id;
	pEvent->Socket = pOperation->Socket;
	pEvent->User = pOperation->User;
	if ( (pOperation->Type == XNET_PORT_EVENT_CONNECT) ||
		 (pOperation->Type == XNET_PORT_EVENT_SEND_TO) ||
		 (pOperation->Type == XNET_PORT_EVENT_SEND_MSG) ) {
		pEvent->Address = __xrtNetIOCPAddressState(pOperation)->Address;
	}

	if ( !bSuccess ) {
		if ( (iSystemCode == ERROR_OPERATION_ABORTED) ||
			 (iSystemCode == WSA_OPERATION_ABORTED) ) {
			pEvent->Result = XNET_RESULT_CANCELLED;
		} else if ( ((pOperation->Type == XNET_PORT_EVENT_RECV_FROM) ||
				(pOperation->Type == XNET_PORT_EVENT_RECV_MSG) ||
				 (pOperation->Type == XNET_PORT_EVENT_RECV)) &&
			(pOperation->Socket->Type == XNET_SOCKET_DGRAM) &&
			((iSystemCode == WSAEMSGSIZE) ||
			 (iSystemCode == ERROR_MORE_DATA)) ) {
			pEvent->Result = XNET_RESULT_TRUNCATED;
			if ( pEvent->Bytes == 0 ) {
				pEvent->Bytes = pOperation->Capacity;
			}
		} else {
			pEvent->Result = XNET_RESULT_ERROR;
			pEvent->Flags |= XNET_PORT_EVENT_ERROR;
		}
	}
	if ( !bSuccess && (pOperation->Type == XNET_PORT_EVENT_ACCEPT) ) {
		__xrt_net_iocp_accept_state* pAccept =
			__xrtNetIOCPAcceptState(pOperation);

		if ( pAccept->Accepted != NULL ) {
			(void)xrtNetSocketClose(pAccept->Accepted);
			pAccept->Accepted = NULL;
		}
	}

	/* 成功接收零字节在流上是 EOF，在数据报上仍是合法报文。 */
	if ( bSuccess && (pOperation->Type == XNET_PORT_EVENT_RECV) &&
		(pOperation->Socket->Type == XNET_SOCKET_STREAM) &&
		(iBytes == 0) ) {
		pEvent->Result = XNET_RESULT_CLOSED;
		pEvent->Flags |= XNET_PORT_EVENT_EOF;
	}

	/* 截断数据报仍应返回已经复制到缓冲区的远端地址。 */
	if ( ((pOperation->Type == XNET_PORT_EVENT_RECV_FROM) ||
		 (pOperation->Type == XNET_PORT_EVENT_RECV_MSG)) &&
		(bSuccess || (pEvent->Result == XNET_RESULT_TRUNCATED)) ) {
		__xrt_net_iocp_address_state* pAddress =
			(pOperation->Type == XNET_PORT_EVENT_RECV_MSG) ?
			&__xrtNetIOCPMessageState(pOperation)->Address :
			__xrtNetIOCPAddressState(pOperation);

		if ( pOperation->Type == XNET_PORT_EVENT_RECV_MSG ) {
			__xrt_net_iocp_message_state* pMessage =
				__xrtNetIOCPMessageState(pOperation);

			pAddress->NativeAddressSize = pMessage->Message.namelen;
			__xrtNetSocketDgramMetaParse(
				pOperation->Socket,
				&pEvent->Meta,
				pMessage->Message.Control.buf,
				(size_t)pMessage->Message.Control.len,
				(uint32)pMessage->Message.dwFlags
			);
			if ( (pEvent->Result == XNET_RESULT_OK) &&
				 ((pMessage->Message.dwFlags & MSG_TRUNC) != 0) ) {
				pEvent->Result = XNET_RESULT_TRUNCATED;
			}
		}

		if ( !xrtNetAddrFromNative(&pEvent->Address,
			&pAddress->NativeAddress,
			(size_t)pAddress->NativeAddressSize) ) {
			pEvent->Result = XNET_RESULT_ERROR;
			pEvent->Flags |= XNET_PORT_EVENT_ERROR;
		}
	}

	if ( bSuccess && (pOperation->Type == XNET_PORT_EVENT_ACCEPT) &&
		!__xrtNetIOCPFinishAccept(pOperation, pEvent) ) {
		pEvent->Result = XNET_RESULT_ERROR;
		pEvent->Flags |= XNET_PORT_EVENT_ERROR;
	}
	if ( pOperation->Type == XNET_PORT_EVENT_CONNECT ) {
		if ( bSuccess && !__xrtNetIOCPFinishConnect(pOperation, pEvent) ) {
			pEvent->Result = XNET_RESULT_ERROR;
			pEvent->Flags |= XNET_PORT_EVENT_ERROR;
		} else if ( !bSuccess ) {
			pOperation->Socket->Connecting = false;
		}
	}
}



/* 等待并批量提取完成；后端唤醒包不伪装成公开完成事件。 */
static xnetresult __xrtNetIOCPWait(xnetport* pPort,
	xnetportevent* pEvents, size_t iCapacity,
	uint64 iTimeout, size_t* pCount)
{
	__xrt_net_iocp_context* pContext =
		(__xrt_net_iocp_context*)pPort->Context;
	size_t iCount = 0;

	*pCount = 0;
	while ( iCount < iCapacity ) {
		DWORD iBytes = 0;
		ULONG_PTR iKey = 0;
		OVERLAPPED* pOverlapped = NULL;
		DWORD iWait = (iCount == 0) ?
			__xrtNetIOCPTimeout(iTimeout) : 0;
		BOOL bSuccess = GetQueuedCompletionStatus(pContext->Port,
			&iBytes, &iKey, &pOverlapped, iWait);
		int iSystemCode = bSuccess ? 0 : (int)GetLastError();

		(void)iKey;
		if ( pOverlapped == NULL ) {
			if ( iSystemCode == WAIT_TIMEOUT ) {
				*pCount = iCount;
				return (iCount == 0) ?
					(iTimeout == 0 ? XNET_RESULT_OK :
					 XNET_RESULT_TIMEOUT) : XNET_RESULT_OK;
			}
			__xrtNetSetError(XERR_IO, XNET_ERROR_PORT_WAIT,
				"wait", "waiting on IOCP failed", iSystemCode);
			return XNET_RESULT_ERROR;
		}
		if ( pOverlapped == &pContext->Wake ) {
			*pCount = iCount;
			return XNET_RESULT_OK;
		}

		{
			__xrt_net_iocp_operation* pOperation =
				(__xrt_net_iocp_operation*)pOverlapped;

			__xrtNetIOCPUntrack(pContext, pOperation);
			__xrtNetIOCPEvent(pOperation, bSuccess,
				iBytes, iSystemCode, &pEvents[iCount++]);
			__xrtNetPortCacheFree(&pContext->OperationCache,
				pOperation, pOperation->AllocationSize);
		}
	}
	*pCount = iCount;
	return XNET_RESULT_OK;
}



/* 使用上下文内固定哨兵唤醒等待，不为每次通知分配内存。 */
static bool __xrtNetIOCPWake(xnetport* pPort)
{
	__xrt_net_iocp_context* pContext =
		(__xrt_net_iocp_context*)pPort->Context;

	if ( PostQueuedCompletionStatus(pContext->Port,
		0, 0, &pContext->Wake) != 0 ) {
		return true;
	}
	__xrtNetSocketSetSystemError(XNET_ERROR_PORT_POST,
		"wake", "posting IOCP wake packet failed", (int)GetLastError());
	return false;
}



/* 销毁期间取消并提取全部在途操作，返回后调用方缓冲不再被系统引用。 */
static bool __xrtNetIOCPUnit(xnetport* pPort)
{
	__xrt_net_iocp_context* pContext =
		(__xrt_net_iocp_context*)pPort->Context;
	int iSystemCode = 0;
	bool bResult = true;

	if ( pContext == NULL ) {
		return true;
	}

	for ( __xrt_net_iocp_operation* pOperation = pContext->OperationHead;
		pOperation != NULL; pOperation = pOperation->ActiveNext ) {
		HANDLE hOwner = ((pOperation->Type == XNET_PORT_EVENT_FILE_READ) ||
			(pOperation->Type == XNET_PORT_EVENT_FILE_WRITE)) ?
			__xrtNetIOCPFileState(pOperation)->File :
			(HANDLE)(uintptr_t)pOperation->Socket->Native;

		if ( CancelIoEx(hOwner,
			&pOperation->Overlapped) == 0 ) {
			DWORD iCode = GetLastError();

			if ( iCode != ERROR_NOT_FOUND ) {
				bResult = false;
				if ( iSystemCode == 0 ) {
					iSystemCode = (int)iCode;
				}
			}
		}
	}

	/* CancelIoEx 只发起取消；必须消费每个终态包后才能释放描述符。 */
	while ( pContext->OperationCount != 0 ) {
		DWORD iBytes = 0;
		ULONG_PTR iKey = 0;
		OVERLAPPED* pOverlapped = NULL;
		BOOL bSuccess = GetQueuedCompletionStatus(pContext->Port,
			&iBytes, &iKey, &pOverlapped, INFINITE);
		DWORD iCode = bSuccess ? ERROR_SUCCESS : GetLastError();

		(void)iBytes;
		(void)iKey;
		if ( pOverlapped == &pContext->Wake ) {
			continue;
		}
		if ( pOverlapped == NULL ) {
			bResult = false;
			if ( iSystemCode == 0 ) {
				iSystemCode = iCode != ERROR_SUCCESS ?
					(int)iCode : (int)ERROR_GEN_FAILURE;
			}
			continue;
		}

		{
			__xrt_net_iocp_operation* pOperation =
				(__xrt_net_iocp_operation*)pOverlapped;

			__xrtNetIOCPUntrack(pContext, pOperation);
			if ( pOperation->Type == XNET_PORT_EVENT_ACCEPT ) {
				__xrt_net_iocp_accept_state* pAccept =
					__xrtNetIOCPAcceptState(pOperation);

				if ( pAccept->Accepted != NULL ) {
					(void)xrtNetSocketClose(pAccept->Accepted);
				}
			}
			if ( pOperation->Type == XNET_PORT_EVENT_CONNECT ) {
				pOperation->Socket->Connecting = false;
			}
			__xrtNetPortCacheFree(&pContext->OperationCache,
				pOperation, pOperation->AllocationSize);
		}
	}

	if ( CloseHandle(pContext->Port) == 0 ) {
		bResult = false;
		if ( iSystemCode == 0 ) {
			iSystemCode = (int)GetLastError();
		}
	}
	xrtFree(pContext->OperationBuckets);
	__xrtNetPortCacheUnit(&pContext->OperationCache);
	xrtFree(pContext);
	pPort->Context = NULL;
	if ( !bResult ) {
		__xrtNetSetError(XERR_IO, XNET_ERROR_PORT_CLOSE,
			"destroy-port", "quiescing IOCP failed", iSystemCode);
	}
	return bResult;
}



/* IOCP 是 Windows Tier A 后端，只公开真实完成式 IO 能力。 */
static const __xrt_net_port_driver __xrtNetIOCPDriver = {
	"iocp",
	XNET_PORT_IOCP,
	XNET_PORT_CAP_COMPLETION |
		XNET_PORT_CAP_BATCH |
		XNET_PORT_CAP_WAKE |
		XNET_PORT_CAP_POST |
		XNET_PORT_CAP_CANCEL |
		XNET_PORT_CAP_READ_PROBE |
		XNET_PORT_CAP_SEND_FILE |
		XNET_PORT_CAP_FILE_IO,
	__xrtNetIOCPInit,
	__xrtNetIOCPUnit,
	NULL,
	NULL,
	__xrtNetIOCPSubmit,
	__xrtNetIOCPCancel,
	__xrtNetIOCPWait,
	__xrtNetIOCPWake
};



/* 返回 IOCP 后端驱动。 */
const __xrt_net_port_driver* __xrtNetPortIOCPDriver(void)
{
	return &__xrtNetIOCPDriver;
}

#else

/* 非 Windows 构建保留可裁剪符号，但不会把 IOCP 作为可用后端。 */
const __xrt_net_port_driver* __xrtNetPortIOCPDriver(void)
{
	return NULL;
}

#endif

#endif
