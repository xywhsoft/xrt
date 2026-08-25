#include "../internal/xrt_net_port.h"
#include "../internal/xrt_atomic.h"



#if defined(XRT_FEATURE_NET_PORT_URING)

#if defined(__linux__)

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>



/* 严格 C 模式可能隐藏 libc 的可变参数 syscall 声明。 */
extern long syscall(long iNumber, ...);



/* io_uring 的稳定 UAPI 数值，避免要求调用方额外链接 liburing。 */
#define XRT_NET_URING_OFF_SQ_RING UINT64_C(0)
#define XRT_NET_URING_OFF_CQ_RING UINT64_C(0x8000000)
#define XRT_NET_URING_OFF_SQES UINT64_C(0x10000000)

#define XRT_NET_URING_SETUP_CQSIZE (1u << 3)
#define XRT_NET_URING_SETUP_CLAMP (1u << 4)

#define XRT_NET_URING_FEAT_SINGLE_MMAP (1u << 0)
#define XRT_NET_URING_FEAT_NODROP (1u << 1)
#define XRT_NET_URING_FEAT_FAST_POLL (1u << 5)

#define XRT_NET_URING_OP_READV 1u
#define XRT_NET_URING_OP_WRITEV 2u
#define XRT_NET_URING_OP_POLL_ADD 6u
#define XRT_NET_URING_OP_SENDMSG 9u
#define XRT_NET_URING_OP_RECVMSG 10u
#define XRT_NET_URING_OP_ACCEPT 13u
#define XRT_NET_URING_OP_ASYNC_CANCEL 14u
#define XRT_NET_URING_OP_CONNECT 16u
#define XRT_NET_URING_OP_SPLICE 30u

#define XRT_NET_URING_REGISTER_PROBE 8u
#define XRT_NET_URING_OP_SUPPORTED (1u << 0)

#define XRT_NET_URING_ENTRY_MIN 64u
#define XRT_NET_URING_ENTRY_MAX 32768u
#define XRT_NET_URING_PROBE_COUNT 32u
#define XRT_NET_URING_CONTROL_CANCEL UINT64_C(1)
#define XRT_NET_URING_SPLICE_F_MOVE 1u
#define XRT_NET_URING_PIPE_TARGET (1024u * 1024u)

#if !defined(F_GETPIPE_SZ)
	#define F_GETPIPE_SZ 1032
#endif

#if !defined(F_SETPIPE_SZ)
	#define F_SETPIPE_SZ 1031
#endif



typedef struct __xrt_net_uring_sq_offsets {
	uint32 Head;
	uint32 Tail;
	uint32 RingMask;
	uint32 RingEntries;
	uint32 Flags;
	uint32 Dropped;
	uint32 Array;
	uint32 Reserved;
	uint64 UserAddress;
} __xrt_net_uring_sq_offsets;



typedef struct __xrt_net_uring_cq_offsets {
	uint32 Head;
	uint32 Tail;
	uint32 RingMask;
	uint32 RingEntries;
	uint32 Overflow;
	uint32 CQEs;
	uint32 Flags;
	uint32 Reserved;
	uint64 UserAddress;
} __xrt_net_uring_cq_offsets;



/* 该结构与 Linux io_uring_params 的稳定 120 字节 UAPI 布局一致。 */
typedef struct __xrt_net_uring_params {
	uint32 SQEntries;
	uint32 CQEntries;
	uint32 Flags;
	uint32 SQThreadCPU;
	uint32 SQThreadIdle;
	uint32 Features;
	uint32 WorkQueueFd;
	uint32 Reserved[3];
	__xrt_net_uring_sq_offsets SQOffset;
	__xrt_net_uring_cq_offsets CQOffset;
} __xrt_net_uring_params;



/* 该结构只使用 io_uring_sqe 自 Linux 5.1 起稳定的前 64 字节。 */
typedef struct __xrt_net_uring_sqe {
	uint8 Opcode;
	uint8 Flags;
	uint16 Priority;
	int32 Fd;
	union {
		uint64 Offset;
		uint64 Address2;
	};
	uint64 Address;
	uint32 Length;
	union {
		uint32 ReadWriteFlags;
		uint32 PollEvents;
		uint32 MessageFlags;
		uint32 AcceptFlags;
		uint32 CancelFlags;
	};
	uint64 UserData;
	union {
		struct {
			uint16 BufferIndex;
			uint16 Personality;
			int32 SpliceFdIn;
			uint64 Reserved[2];
		} Tail;
		uint64 Padding[3];
	};
} __xrt_net_uring_sqe;



typedef struct __xrt_net_uring_cqe {
	uint64 UserData;
	int32 Result;
	uint32 Flags;
} __xrt_net_uring_cqe;



typedef struct __xrt_net_uring_probe_op {
	uint8 Opcode;
	uint8 Reserved;
	uint16 Flags;
	uint32 Reserved2;
} __xrt_net_uring_probe_op;



typedef struct __xrt_net_uring_probe {
	uint8 LastOpcode;
	uint8 OperationCount;
	uint16 Reserved;
	uint32 Reserved2[3];
	__xrt_net_uring_probe_op Operations[XRT_NET_URING_PROBE_COUNT];
} __xrt_net_uring_probe;



typedef struct __xrt_net_uring_ring {
	int Fd;
	void* SQRing;
	void* CQRing;
	void* SQEsMap;
	size_t SQRingSize;
	size_t CQRingSize;
	size_t SQEsSize;
	volatile uint32* SQHead;
	volatile uint32* SQTail;
	uint32* SQMask;
	uint32* SQEntries;
	volatile uint32* SQDropped;
	uint32* SQArray;
	volatile uint32* CQHead;
	volatile uint32* CQTail;
	uint32* CQMask;
	uint32* CQEntries;
	volatile uint32* CQOverflow;
	__xrt_net_uring_sqe* SQEs;
	__xrt_net_uring_cqe* CQEs;
	__xrt_net_uring_params Parameters;
	bool SharedMap;
	bool Splice;
	bool FileIO;
} __xrt_net_uring_ring;



typedef struct __xrt_net_uring_operation __xrt_net_uring_operation;



/* RECV_MSG 操作按需在 iovec 后附带一个对齐的控制消息缓冲。 */
typedef union __xrt_net_uring_control {
	uint64 Align;
	unsigned char Data[XRT_NET_SOCKET_DGRAM_CONTROL_SIZE];
} __xrt_net_uring_control;



/* SEND_MSG 尾部同时保存控制值和内核读取的对齐控制缓冲。 */
typedef struct __xrt_net_uring_send_control {
	xnetdgramcontrol Value;
	__xrt_net_uring_control Buffer;
} __xrt_net_uring_send_control;



/* 操作只复制描述符和地址，不拥有 Socket、发送载荷或接收缓冲。 */
struct __xrt_net_uring_operation {
	__xrt_net_uring_operation* ActiveNext;
	__xrt_net_uring_operation* ActivePrevious;
	__xrt_net_uring_operation* HashNext;
	__xrt_net_uring_operation* HashPrevious;
	__xrt_net_uring_operation* RetiredNext;
	__xrt_net_uring_operation* RetiredPrevious;
	size_t AllocationSize;
	xnetporteventtype Type;
	xnetsocket Socket;
	uint64 Id;
	ptr User;
	size_t Capacity;
	struct sockaddr_storage NativeAddress;
	socklen_t NativeAddressSize;
	xnetaddr Address;
	struct msghdr Message;
	bool CancelRequested;
	bool CancelCompleted;
	bool OperationCompleted;
	intptr_t File;
	uint64 FileOffset;
	size_t FileSize;
	size_t FileBytes;
	int PipeRead;
	int PipeWrite;
	size_t PipeBytes;
	bool FileOutput;
};



/* 每个 Worker 独占 ring 与操作表，只有 WakeFd 接受跨线程写入。 */
typedef struct __xrt_net_uring_context {
	__xrt_net_uring_ring Ring;
	int WakeFd;
	__xrt_net_uring_operation* OperationHead;
	__xrt_net_uring_operation* RetiredHead;
	__xrt_net_uring_operation** OperationBuckets;
	size_t OperationCount;
	size_t OperationBucketCount;
	size_t OperationBucketLimit;
	size_t PendingSubmissions;
	size_t PendingCancels;
	__xrt_net_port_cache OperationCache;
} __xrt_net_uring_context;



/* 编译期约束防止手写 UAPI 布局被维护修改破坏。 */
typedef char __xrt_net_uring_params_size[
	(sizeof(__xrt_net_uring_params) == 120u) ? 1 : -1
];
typedef char __xrt_net_uring_sqe_size[
	(sizeof(__xrt_net_uring_sqe) == 64u) ? 1 : -1
];
typedef char __xrt_net_uring_sqe_splice_fd_offset[
	(offsetof(__xrt_net_uring_sqe, Tail.SpliceFdIn) == 44u) ? 1 : -1
];
typedef char __xrt_net_uring_cqe_size[
	(sizeof(__xrt_net_uring_cqe) == 16u) ? 1 : -1
];



/* 调用 Linux io_uring_setup；不经过 glibc 或外部运行库包装。 */
static int __xrtNetUringSetup(
	uint32 iEntries,
	__xrt_net_uring_params* pParameters
)
{
	#if defined(SYS_io_uring_setup)
		return (int)syscall(SYS_io_uring_setup, iEntries, pParameters);
	#elif defined(__NR_io_uring_setup)
		return (int)syscall(__NR_io_uring_setup, iEntries, pParameters);
	#else
		(void)iEntries;
		(void)pParameters;
		errno = ENOSYS;
		return -1;
	#endif
}



/* 调用 Linux io_uring_enter，并把信号中断留给上层重试。 */
static int __xrtNetUringEnter(
	int iRingFd,
	uint32 iSubmit,
	uint32 iMinimum,
	uint32 iFlags
)
{
	#if defined(SYS_io_uring_enter)
		return (int)syscall(
			SYS_io_uring_enter,
			iRingFd,
			iSubmit,
			iMinimum,
			iFlags,
			NULL,
			0u
		);
	#elif defined(__NR_io_uring_enter)
		return (int)syscall(
			__NR_io_uring_enter,
			iRingFd,
			iSubmit,
			iMinimum,
			iFlags,
			NULL,
			0u
		);
	#else
		(void)iRingFd;
		(void)iSubmit;
		(void)iMinimum;
		(void)iFlags;
		errno = ENOSYS;
		return -1;
	#endif
}



/* 调用 Linux io_uring_register，用于启动期探测稳定操作集合。 */
static int __xrtNetUringRegister(
	int iRingFd,
	uint32 iOpcode,
	const void* pArgument,
	uint32 iCount
)
{
	#if defined(SYS_io_uring_register)
		return (int)syscall(
			SYS_io_uring_register,
			iRingFd,
			iOpcode,
			pArgument,
			iCount
		);
	#elif defined(__NR_io_uring_register)
		return (int)syscall(
			__NR_io_uring_register,
			iRingFd,
			iOpcode,
			pArgument,
			iCount
		);
	#else
		(void)iRingFd;
		(void)iOpcode;
		(void)pArgument;
		(void)iCount;
		errno = ENOSYS;
		return -1;
	#endif
}



/* 关闭并解除 ring 的全部映射；允许初始化中途调用。 */
static void __xrtNetUringRingUnit(__xrt_net_uring_ring* pRing)
{
	if ( pRing->SQEsMap != NULL ) {
		(void)munmap(pRing->SQEsMap, pRing->SQEsSize);
	}
	if ( pRing->SQRing != NULL ) {
		(void)munmap(pRing->SQRing, pRing->SQRingSize);
	}
	if ( (pRing->CQRing != NULL) && !pRing->SharedMap ) {
		(void)munmap(pRing->CQRing, pRing->CQRingSize);
	}
	if ( pRing->Fd >= 0 ) {
		(void)close(pRing->Fd);
	}

	memset(pRing, 0, sizeof(*pRing));
	pRing->Fd = -1;
}



/* 检查内核是否实现本端口承诺的全部完成式 Socket 操作。 */
static bool __xrtNetUringProbe(__xrt_net_uring_ring* pRing)
{
	__xrt_net_uring_probe Probe;
	uint32 iOperationCount;
	const uint8 Required[] = {
		XRT_NET_URING_OP_POLL_ADD,
		XRT_NET_URING_OP_SENDMSG,
		XRT_NET_URING_OP_RECVMSG,
		XRT_NET_URING_OP_ACCEPT,
		XRT_NET_URING_OP_ASYNC_CANCEL,
		XRT_NET_URING_OP_CONNECT
	};

	memset(&Probe, 0, sizeof(Probe));
	if ( __xrtNetUringRegister(
		pRing->Fd,
		XRT_NET_URING_REGISTER_PROBE,
		&Probe,
		XRT_NET_URING_PROBE_COUNT
	) < 0 ) {
		__xrtNetSetError(
			XERR_UNSUPPORTED,
			XNET_ERROR_PORT_CREATE,
			"create-port",
			"io_uring operation probing is unavailable",
			errno
		);
		return false;
	}
	iOperationCount = Probe.OperationCount;
	if ( iOperationCount > XRT_NET_URING_PROBE_COUNT ) {
		iOperationCount = XRT_NET_URING_PROBE_COUNT;
	}

	for ( size_t i = 0; i < sizeof(Required); i++ ) {
		bool bFound = false;

		for ( uint32 j = 0; j < iOperationCount; j++ ) {
			if ( (Probe.Operations[j].Opcode == Required[i]) &&
				 ((Probe.Operations[j].Flags &
					XRT_NET_URING_OP_SUPPORTED) != 0) ) {
				bFound = true;
				break;
			}
		}
		if ( !bFound ) {
			__xrtNetSetError(
				XERR_UNSUPPORTED,
				XNET_ERROR_PORT_CREATE,
				"create-port",
				"io_uring lacks a required network operation",
				0
			);
			return false;
		}
	}
	for ( uint32 i = 0; i < iOperationCount; i++ ) {
		if ( (Probe.Operations[i].Opcode == XRT_NET_URING_OP_SPLICE) &&
			((Probe.Operations[i].Flags &
				XRT_NET_URING_OP_SUPPORTED) != 0) ) {
			pRing->Splice = true;
			break;
		}
	}
	{
		bool bRead = false;
		bool bWrite = false;

		for ( uint32 i = 0; i < iOperationCount; i++ ) {
			if ( (Probe.Operations[i].Flags &
				XRT_NET_URING_OP_SUPPORTED) == 0 ) {
				continue;
			}
			if ( Probe.Operations[i].Opcode == XRT_NET_URING_OP_READV ) {
				bRead = true;
			}
			if ( Probe.Operations[i].Opcode == XRT_NET_URING_OP_WRITEV ) {
				bWrite = true;
			}
		}
		pRing->FileIO = bRead && bWrite;
	}
	return true;
}



/* 安全累加一个映射区间，避免 32 位构建中的偏移和数组尺寸回绕。 */
static bool __xrtNetUringMapExtent(
	size_t iOffset,
	size_t iCount,
	size_t iItemSize,
	size_t* pSize
)
{
	size_t iEnd;

	if ( (iItemSize != 0) &&
		 (iCount > ((SIZE_MAX - iOffset) / iItemSize)) ) {
		errno = EOVERFLOW;
		return false;
	}
	iEnd = iOffset + (iCount * iItemSize);
	if ( iEnd > *pSize ) {
		*pSize = iEnd;
	}
	return true;
}



/* 把 setup 返回的偏移映射为单生产者 SQ 与单消费者 CQ。 */
static bool __xrtNetUringRingMap(__xrt_net_uring_ring* pRing)
{
	size_t iSharedSize;

	if ( !__xrtNetUringMapExtent(
		pRing->Parameters.SQOffset.Head,
		1u,
		sizeof(uint32),
		&pRing->SQRingSize
	) || !__xrtNetUringMapExtent(
		pRing->Parameters.SQOffset.Tail,
		1u,
		sizeof(uint32),
		&pRing->SQRingSize
	) || !__xrtNetUringMapExtent(
		pRing->Parameters.SQOffset.RingMask,
		1u,
		sizeof(uint32),
		&pRing->SQRingSize
	) || !__xrtNetUringMapExtent(
		pRing->Parameters.SQOffset.RingEntries,
		1u,
		sizeof(uint32),
		&pRing->SQRingSize
	) || !__xrtNetUringMapExtent(
		pRing->Parameters.SQOffset.Dropped,
		1u,
		sizeof(uint32),
		&pRing->SQRingSize
	) || !__xrtNetUringMapExtent(
		pRing->Parameters.SQOffset.Array,
		pRing->Parameters.SQEntries,
		sizeof(uint32),
		&pRing->SQRingSize
	) || !__xrtNetUringMapExtent(
		pRing->Parameters.CQOffset.Head,
		1u,
		sizeof(uint32),
		&pRing->CQRingSize
	) || !__xrtNetUringMapExtent(
		pRing->Parameters.CQOffset.Tail,
		1u,
		sizeof(uint32),
		&pRing->CQRingSize
	) || !__xrtNetUringMapExtent(
		pRing->Parameters.CQOffset.RingMask,
		1u,
		sizeof(uint32),
		&pRing->CQRingSize
	) || !__xrtNetUringMapExtent(
		pRing->Parameters.CQOffset.RingEntries,
		1u,
		sizeof(uint32),
		&pRing->CQRingSize
	) || !__xrtNetUringMapExtent(
		pRing->Parameters.CQOffset.Overflow,
		1u,
		sizeof(uint32),
		&pRing->CQRingSize
	) || !__xrtNetUringMapExtent(
		pRing->Parameters.CQOffset.CQEs,
		pRing->Parameters.CQEntries,
		sizeof(__xrt_net_uring_cqe),
		&pRing->CQRingSize
	) || !__xrtNetUringMapExtent(
		0,
		pRing->Parameters.SQEntries,
		sizeof(__xrt_net_uring_sqe),
		&pRing->SQEsSize
	) ) {
		return false;
	}
	pRing->SharedMap = (pRing->Parameters.Features &
		XRT_NET_URING_FEAT_SINGLE_MMAP) != 0;

	if ( pRing->SharedMap ) {
		iSharedSize = (pRing->SQRingSize > pRing->CQRingSize) ?
			pRing->SQRingSize : pRing->CQRingSize;
		pRing->SQRing = mmap(
			NULL,
			iSharedSize,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			pRing->Fd,
			(off_t)XRT_NET_URING_OFF_SQ_RING
		);
		if ( pRing->SQRing == MAP_FAILED ) {
			pRing->SQRing = NULL;
			return false;
		}
		pRing->CQRing = pRing->SQRing;
		pRing->SQRingSize = iSharedSize;
		pRing->CQRingSize = iSharedSize;
	} else {
		pRing->SQRing = mmap(
			NULL,
			pRing->SQRingSize,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			pRing->Fd,
			(off_t)XRT_NET_URING_OFF_SQ_RING
		);
		if ( pRing->SQRing == MAP_FAILED ) {
			pRing->SQRing = NULL;
			return false;
		}
		pRing->CQRing = mmap(
			NULL,
			pRing->CQRingSize,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			pRing->Fd,
			(off_t)XRT_NET_URING_OFF_CQ_RING
		);
		if ( pRing->CQRing == MAP_FAILED ) {
			pRing->CQRing = NULL;
			return false;
		}
	}

	pRing->SQEsMap = mmap(
		NULL,
		pRing->SQEsSize,
		PROT_READ | PROT_WRITE,
		MAP_SHARED,
		pRing->Fd,
		(off_t)XRT_NET_URING_OFF_SQES
	);
	if ( pRing->SQEsMap == MAP_FAILED ) {
		pRing->SQEsMap = NULL;
		return false;
	}

	pRing->SQHead = (volatile uint32*)((bytes)pRing->SQRing +
		pRing->Parameters.SQOffset.Head);
	pRing->SQTail = (volatile uint32*)((bytes)pRing->SQRing +
		pRing->Parameters.SQOffset.Tail);
	pRing->SQMask = (uint32*)((bytes)pRing->SQRing +
		pRing->Parameters.SQOffset.RingMask);
	pRing->SQEntries = (uint32*)((bytes)pRing->SQRing +
		pRing->Parameters.SQOffset.RingEntries);
	pRing->SQDropped = (volatile uint32*)((bytes)pRing->SQRing +
		pRing->Parameters.SQOffset.Dropped);
	pRing->SQArray = (uint32*)((bytes)pRing->SQRing +
		pRing->Parameters.SQOffset.Array);
	pRing->CQHead = (volatile uint32*)((bytes)pRing->CQRing +
		pRing->Parameters.CQOffset.Head);
	pRing->CQTail = (volatile uint32*)((bytes)pRing->CQRing +
		pRing->Parameters.CQOffset.Tail);
	pRing->CQMask = (uint32*)((bytes)pRing->CQRing +
		pRing->Parameters.CQOffset.RingMask);
	pRing->CQEntries = (uint32*)((bytes)pRing->CQRing +
		pRing->Parameters.CQOffset.RingEntries);
	pRing->CQOverflow = (volatile uint32*)((bytes)pRing->CQRing +
		pRing->Parameters.CQOffset.Overflow);
	pRing->SQEs = (__xrt_net_uring_sqe*)pRing->SQEsMap;
	pRing->CQEs = (__xrt_net_uring_cqe*)((bytes)pRing->CQRing +
		pRing->Parameters.CQOffset.CQEs);
	if ( (*pRing->SQEntries == 0) ||
		 ((*pRing->SQEntries & (*pRing->SQEntries - 1u)) != 0) ||
		 (*pRing->SQEntries != pRing->Parameters.SQEntries) ||
		 (*pRing->SQMask != (*pRing->SQEntries - 1u)) ||
		 (*pRing->CQEntries == 0) ||
		 ((*pRing->CQEntries & (*pRing->CQEntries - 1u)) != 0) ||
		 (*pRing->CQEntries != pRing->Parameters.CQEntries) ||
		 (*pRing->CQMask != (*pRing->CQEntries - 1u)) ) {
		errno = EIO;
		return false;
	}
	return true;
}



/* 创建满足完成不丢失和内核快速轮询要求的原生 ring。 */
static bool __xrtNetUringRingInit(
	__xrt_net_uring_ring* pRing,
	size_t iOperationLimit
)
{
	uint32 iEntries = (uint32)iOperationLimit;
	int iFlags;
	int iCode;

	memset(pRing, 0, sizeof(*pRing));
	pRing->Fd = -1;
	if ( iEntries < XRT_NET_URING_ENTRY_MIN ) {
		iEntries = XRT_NET_URING_ENTRY_MIN;
	}

	pRing->Parameters.Flags =
		XRT_NET_URING_SETUP_CQSIZE |
		XRT_NET_URING_SETUP_CLAMP;
	pRing->Parameters.CQEntries = iEntries * 2u;
	pRing->Fd = __xrtNetUringSetup(iEntries, &pRing->Parameters);
	if ( pRing->Fd < 0 ) {
		__xrtNetSocketSetSystemError(
			XNET_ERROR_PORT_CREATE,
			"create-port",
			"creating io_uring failed",
			errno
		);
		return false;
	}
	if ( (size_t)pRing->Parameters.CQEntries <
		(iOperationLimit * 2u) ) {
		__xrtNetUringRingUnit(pRing);
		__xrtNetSetError(
			XERR_UNSUPPORTED,
			XNET_ERROR_PORT_CREATE,
			"create-port",
			"io_uring completion queue is smaller than the operation contract",
			0
		);
		return false;
	}

	iFlags = fcntl(pRing->Fd, F_GETFD, 0);
	if ( (iFlags < 0) ||
		 (fcntl(pRing->Fd, F_SETFD, iFlags | FD_CLOEXEC) != 0) ) {
		iCode = errno;
		__xrtNetUringRingUnit(pRing);
		__xrtNetSocketSetSystemError(
			XNET_ERROR_PORT_CREATE,
			"create-port",
			"protecting io_uring descriptor inheritance failed",
			iCode
		);
		return false;
	}
	if ( (pRing->Parameters.Features & XRT_NET_URING_FEAT_NODROP) == 0 ) {
		__xrtNetUringRingUnit(pRing);
		__xrtNetSetError(
			XERR_UNSUPPORTED,
			XNET_ERROR_PORT_CREATE,
			"create-port",
			"io_uring cannot guarantee completion delivery",
			0
		);
		return false;
	}
	if ( (pRing->Parameters.Features & XRT_NET_URING_FEAT_FAST_POLL) == 0 ) {
		__xrtNetUringRingUnit(pRing);
		__xrtNetSetError(
			XERR_UNSUPPORTED,
			XNET_ERROR_PORT_CREATE,
			"create-port",
			"io_uring has no fast socket polling",
			0
		);
		return false;
	}
	if ( !__xrtNetUringProbe(pRing) ) {
		__xrtNetUringRingUnit(pRing);
		return false;
	}
	if ( !__xrtNetUringRingMap(pRing) ) {
		iCode = errno;
		__xrtNetUringRingUnit(pRing);
		__xrtNetSocketSetSystemError(
			XNET_ERROR_PORT_CREATE,
			"create-port",
			"mapping io_uring queues failed",
			iCode
		);
		return false;
	}
	return true;
}



/* 创建非阻塞、禁止继承的 eventfd，兼容不接受原子标志的旧内核。 */
static int __xrtNetUringWakeOpen(void)
{
	int iFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

	if ( (iFd < 0) && (errno == EINVAL) ) {
		int iFlags;

		iFd = eventfd(0, 0);
		if ( iFd < 0 ) {
			return -1;
		}
		iFlags = fcntl(iFd, F_GETFL, 0);
		if ( (iFlags < 0) ||
			 (fcntl(iFd, F_SETFL, iFlags | O_NONBLOCK) != 0) ) {
			int iCode = errno;

			(void)close(iFd);
			errno = iCode;
			return -1;
		}
		iFlags = fcntl(iFd, F_GETFD, 0);
		if ( (iFlags < 0) ||
			 (fcntl(iFd, F_SETFD, iFlags | FD_CLOEXEC) != 0) ) {
			int iCode = errno;

			(void)close(iFd);
			errno = iCode;
			return -1;
		}
	}
	return iFd;
}



/* 对 64 位操作 ID 做完整混合，32 位构建也保留高位熵。 */
static size_t __xrtNetUringHash(
	const __xrt_net_uring_context* pContext,
	uint64 Id
)
{
	return __xrtNetPortHashId(Id, pContext->OperationBucketCount);
}



/* 按公开操作 ID 查找仍在等待唯一终态的节点。 */
static __xrt_net_uring_operation* __xrtNetUringFind(
	__xrt_net_uring_context* pContext,
	uint64 Id
)
{
	__xrt_net_uring_operation* pOperation =
		pContext->OperationBuckets[__xrtNetUringHash(pContext, Id)];

	while ( pOperation != NULL ) {
		if ( pOperation->Id == Id ) {
			return pOperation;
		}
		pOperation = pOperation->HashNext;
	}
	return NULL;
}



/* 在发布新 SQE 前按活动数量扩展 ID 索引。 */
static bool __xrtNetUringIndexGrow(__xrt_net_uring_context* pContext)
{
	size_t iNewCount = __xrtNetPortBucketNext(
		pContext->OperationCount + 1u,
		pContext->OperationBucketCount,
		pContext->OperationBucketLimit
	);
	__xrt_net_uring_operation** pBuckets;
	__xrt_net_uring_operation* pOperation;

	if ( iNewCount == pContext->OperationBucketCount ) {
		return true;
	}
	pBuckets = (__xrt_net_uring_operation**)xrtCalloc(
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



/* 把已发布给内核的操作加入 owner 线程独占索引。 */
static void __xrtNetUringTrack(
	__xrt_net_uring_context* pContext,
	__xrt_net_uring_operation* pOperation
)
{
	size_t iBucket = __xrtNetUringHash(pContext, pOperation->Id);

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



/* 从活跃链和 ID 哈希中以 O(1) 平均复杂度移除终态操作。 */
static void __xrtNetUringUntrack(
	__xrt_net_uring_context* pContext,
	__xrt_net_uring_operation* pOperation
)
{
	size_t iBucket = __xrtNetUringHash(pContext, pOperation->Id);

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



/* 暂存已经公开完成、但取消控制 CQE 尚未到达的描述符。 */
static void __xrtNetUringRetire(
	__xrt_net_uring_context* pContext,
	__xrt_net_uring_operation* pOperation
)
{
	pOperation->RetiredNext = pContext->RetiredHead;
	if ( pContext->RetiredHead != NULL ) {
		pContext->RetiredHead->RetiredPrevious = pOperation;
	}
	pContext->RetiredHead = pOperation;
}



/* 在配对的取消控制 CQE 到达后从暂存链 O(1) 移除描述符。 */
static void __xrtNetUringUnretire(
	__xrt_net_uring_context* pContext,
	__xrt_net_uring_operation* pOperation
)
{
	if ( pOperation->RetiredPrevious != NULL ) {
		pOperation->RetiredPrevious->RetiredNext =
			pOperation->RetiredNext;
	} else {
		pContext->RetiredHead = pOperation->RetiredNext;
	}
	if ( pOperation->RetiredNext != NULL ) {
		pOperation->RetiredNext->RetiredPrevious =
			pOperation->RetiredPrevious;
	}
	pOperation->RetiredNext = NULL;
	pOperation->RetiredPrevious = NULL;
}



/* 只有原操作和已发布的取消请求都完成后，描述符才允许复用。 */
static void __xrtNetUringOperationRelease(
	__xrt_net_uring_context* pContext,
	__xrt_net_uring_operation* pOperation
)
{
	if ( pOperation->PipeRead >= 0 ) {
		(void)close(pOperation->PipeRead);
	}
	if ( pOperation->PipeWrite >= 0 ) {
		(void)close(pOperation->PipeWrite);
	}
	__xrtNetPortCacheFree(
		&pContext->OperationCache,
		pOperation,
		pOperation->AllocationSize
	);
}



/* 创建禁止继承的私有管道，并返回本次可安全填满的容量。 */
static bool __xrtNetUringPipeOpen(
	int* pRead,
	int* pWrite,
	size_t* pCapacity
)
{
	int Pipe[2];
	int iCapacity;

	if ( pipe(Pipe) != 0 ) {
		return false;
	}
	for ( size_t i = 0; i < 2; i++ ) {
		int iFlags = fcntl(Pipe[i], F_GETFD, 0);

		if ( (iFlags < 0) ||
			 (fcntl(Pipe[i], F_SETFD, iFlags | FD_CLOEXEC) != 0) ) {
			int iCode = errno;

			(void)close(Pipe[0]);
			(void)close(Pipe[1]);
			errno = iCode;
			return false;
		}
	}
	/* 内核会按系统上限裁剪；权限或限额失败时继续使用默认容量。 */
	(void)fcntl(Pipe[1], F_SETPIPE_SZ, XRT_NET_URING_PIPE_TARGET);
	iCapacity = fcntl(Pipe[0], F_GETPIPE_SZ, 0);
	if ( iCapacity <= 0 ) {
		int iCode = (iCapacity < 0) ? errno : EIO;

		(void)close(Pipe[0]);
		(void)close(Pipe[1]);
		errno = iCode;
		return false;
	}
	*pRead = Pipe[0];
	*pWrite = Pipe[1];
	*pCapacity = (size_t)iCapacity;
	return true;
}



/* 返回操作尾部按需分配的 iovec 描述符数组。 */
static struct iovec* __xrtNetUringVectors(
	__xrt_net_uring_operation* pOperation
)
{
	return (struct iovec*)(pOperation + 1);
}



/* 返回 RECV_MSG 操作在 iovec 数组后的控制消息缓冲。 */
static __xrt_net_uring_control* __xrtNetUringControl(
	__xrt_net_uring_operation* pOperation
)
{
	return (__xrt_net_uring_control*)(
		__xrtNetUringVectors(pOperation) +
		pOperation->Message.msg_iovlen
	);
}



/* 返回 SEND_MSG 操作在 iovec 数组后的控制状态。 */
static __xrt_net_uring_send_control* __xrtNetUringSendControl(
	__xrt_net_uring_operation* pOperation
)
{
	return (__xrt_net_uring_send_control*)(
		__xrtNetUringVectors(pOperation) +
		pOperation->Message.msg_iovlen
	);
}



/* 按事件类别返回结构化错误中的稳定操作名。 */
static cstr __xrtNetUringOperationName(xnetporteventtype Type)
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
		case XNET_PORT_EVENT_RECV_ERROR:
			return "receive-datagram-error";
		case XNET_PORT_EVENT_SEND_TO:
			return "send-to";
		case XNET_PORT_EVENT_SEND_MSG:
			return "send-message";
		default:
			return "submit";
	}
}



/* 分配操作并复制 Span 描述符与地址，载荷始终由调用方保有。 */
static __xrt_net_uring_operation* __xrtNetUringOperationCreate(
	xnetport* pPort,
	const __xrt_net_port_submit* pSubmit
)
{
	__xrt_net_uring_context* pContext =
		(__xrt_net_uring_context*)pPort->Context;
	__xrt_net_uring_operation* pOperation;
	struct iovec* pVectors;
	size_t iExtra = pSubmit->SpanCount * sizeof(struct iovec);
	size_t iAllocation;

	if ( pContext->OperationCount >= pPort->Config.OperationLimit ) {
		__xrtNetSetError(
			XERR_AGAIN,
			XNET_ERROR_PORT_SUBMIT,
			__xrtNetUringOperationName(pSubmit->Type),
			"network completion operation limit reached",
			0
		);
		return NULL;
	}
	if ( __xrtNetUringFind(pContext, pSubmit->Id) != NULL ) {
		__xrtNetSetError(
			XERR_EXISTS,
			XNET_ERROR_PORT_SUBMIT,
			__xrtNetUringOperationName(pSubmit->Type),
			"network completion operation id is already active",
			0
		);
		return NULL;
	}
	if ( !__xrtNetUringIndexGrow(pContext) ) {
		return NULL;
	}

	if ( pSubmit->Type == XNET_PORT_EVENT_RECV_MSG ) {
		iExtra += sizeof(__xrt_net_uring_control);
	} else if ( pSubmit->Type == XNET_PORT_EVENT_SEND_MSG ) {
		iExtra += sizeof(__xrt_net_uring_send_control);
	}
	pOperation = (__xrt_net_uring_operation*)__xrtNetPortCacheAlloc(
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
	pOperation->File = pSubmit->File;
	pOperation->FileOffset = pSubmit->FileOffset;
	pOperation->FileSize = pSubmit->FileSize;
	pOperation->PipeRead = -1;
	pOperation->PipeWrite = -1;
	if ( pSubmit->Type == XNET_PORT_EVENT_SEND_FILE ) {
		size_t iPipeCapacity;

		if ( !__xrtNetUringPipeOpen(
			&pOperation->PipeRead,
			&pOperation->PipeWrite,
			&iPipeCapacity
		) ) {
			int iCode = errno;

			__xrtNetUringOperationRelease(pContext, pOperation);
			__xrtNetSocketSetSystemError(
				XNET_ERROR_PORT_SUBMIT,
				"send-file",
				"creating io_uring send-file pipe failed",
				iCode
			);
			return NULL;
		}
		pOperation->Capacity = iPipeCapacity;
	}
	pOperation->NativeAddressSize =
		(socklen_t)sizeof(pOperation->NativeAddress);

	pVectors = __xrtNetUringVectors(pOperation);
	for ( size_t i = 0; i < pSubmit->SpanCount; i++ ) {
		if ( pSubmit->ReadSpans != NULL ) {
			pVectors[i].iov_base = pSubmit->ReadSpans[i].Data;
			pVectors[i].iov_len = pSubmit->ReadSpans[i].Size;
			pOperation->Capacity += pSubmit->ReadSpans[i].Size;
		} else {
			pVectors[i].iov_base = (void*)pSubmit->WriteSpans[i].Data;
			pVectors[i].iov_len = pSubmit->WriteSpans[i].Size;
			pOperation->Capacity += pSubmit->WriteSpans[i].Size;
		}
	}

	if ( pSubmit->Address != NULL ) {
		size_t iAddressSize = sizeof(pOperation->NativeAddress);

		pOperation->Address = *pSubmit->Address;
		if ( !xrtNetAddrToNative(
			pSubmit->Address,
			&pOperation->NativeAddress,
			&iAddressSize
		) ) {
			__xrtNetUringOperationRelease(pContext, pOperation);
			__xrtNetSetError(
				XERR_ARGUMENT,
				XNET_ERROR_PORT_SUBMIT,
				__xrtNetUringOperationName(pSubmit->Type),
				"invalid native network operation address",
				0
			);
			return NULL;
		}
		pOperation->NativeAddressSize = (socklen_t)iAddressSize;
	}

	pOperation->Message.msg_iov = pVectors;
	pOperation->Message.msg_iovlen = pSubmit->SpanCount;
	if ( (pSubmit->Type == XNET_PORT_EVENT_RECV_FROM) ||
		 (pSubmit->Type == XNET_PORT_EVENT_RECV_MSG) ) {
		pOperation->Message.msg_name = &pOperation->NativeAddress;
		pOperation->Message.msg_namelen =
			(socklen_t)sizeof(pOperation->NativeAddress);
		if ( pSubmit->Type == XNET_PORT_EVENT_RECV_MSG ) {
			__xrt_net_uring_control* pControl =
				__xrtNetUringControl(pOperation);

			pOperation->Message.msg_control = pControl->Data;
			pOperation->Message.msg_controllen = sizeof(pControl->Data);
		}
	} else if ( (pSubmit->Type == XNET_PORT_EVENT_SEND_TO) ||
		 (pSubmit->Type == XNET_PORT_EVENT_SEND_MSG) ) {
		if ( pSubmit->Address != NULL ) {
			pOperation->Message.msg_name = &pOperation->NativeAddress;
			pOperation->Message.msg_namelen =
				pOperation->NativeAddressSize;
		}
		if ( pSubmit->Type == XNET_PORT_EVENT_SEND_MSG ) {
			__xrt_net_uring_send_control* pControl =
				__xrtNetUringSendControl(pOperation);
			size_t iControlSize = 0;

			pControl->Value = *pSubmit->Control;
			if ( !__xrtNetSocketDgramControlBuild(
				pOperation->Socket,
				&pControl->Value,
				pOperation->Capacity,
				pControl->Buffer.Data,
				sizeof(pControl->Buffer.Data),
				&iControlSize,
				XNET_ERROR_PORT_SUBMIT,
				"send-message"
			) ) {
				__xrtNetUringOperationRelease(pContext, pOperation);
				return NULL;
			}
			pOperation->Message.msg_control = pControl->Buffer.Data;
			pOperation->Message.msg_controllen = iControlSize;
		}
	}
	return pOperation;
}



/* 获取尚未发布的 SQE；ring 满时不改变共享队列。 */
static __xrt_net_uring_sqe* __xrtNetUringSQE(
	__xrt_net_uring_ring* pRing,
	uint32* pTail,
	uint32* pSlot
)
{
	uint32 iHead = __xrtAtomic32LoadValue(
		pRing->SQHead,
		XMEMORY_ACQUIRE
	);
	uint32 iTail = __xrtAtomic32LoadValue(
		pRing->SQTail,
		XMEMORY_RELAXED
	);

	if ( (iTail - iHead) >= *pRing->SQEntries ) {
		return NULL;
	}
	*pTail = iTail;
	*pSlot = iTail & *pRing->SQMask;
	memset(&pRing->SQEs[*pSlot], 0, sizeof(pRing->SQEs[*pSlot]));
	return &pRing->SQEs[*pSlot];
}



/* 以 release 顺序发布一个已经完整填写的 SQE。 */
static void __xrtNetUringSQECommit(
	__xrt_net_uring_ring* pRing,
	uint32 iTail,
	uint32 iSlot
)
{
	pRing->SQArray[iTail & *pRing->SQMask] = iSlot;
	__xrtAtomic32StoreValue(
		pRing->SQTail,
		iTail + 1u,
		XMEMORY_RELEASE
	);
}



/* 仅在内核确认尚未消费时撤销最后一个单生产者 SQE。 */
static bool __xrtNetUringSQERollback(
	__xrt_net_uring_ring* pRing,
	uint32 iTail,
	uint32 iSlot
)
{
	uint32 iHead = __xrtAtomic32LoadValue(
		pRing->SQHead,
		XMEMORY_ACQUIRE
	);

	if ( iHead != iTail ) {
		return false;
	}
	memset(&pRing->SQEs[iSlot], 0, sizeof(pRing->SQEs[iSlot]));
	pRing->SQArray[iTail & *pRing->SQMask] = 0;
	__xrtAtomic32StoreValue(pRing->SQTail, iTail, XMEMORY_RELEASE);
	return true;
}



/* 通知内核消费一个 SQE；信号中断不会让提交结果变成歧义。 */
static bool __xrtNetUringPublish(
	__xrt_net_uring_ring* pRing,
	uint32 iTail,
	uint32 iSlot,
	bool* pConsumed
)
{
	int iResult;
	int iCode = EIO;

	*pConsumed = false;
	for ( ;; ) {
		iResult = __xrtNetUringEnter(pRing->Fd, 1u, 0u, 0u);
		if ( iResult > 0 ) {
			*pConsumed = true;
			return true;
		}
		if ( (iResult < 0) && (errno == EINTR) ) {
			continue;
		}
		if ( iResult < 0 ) {
			iCode = errno;
		}
		break;
	}

	if ( !__xrtNetUringSQERollback(pRing, iTail, iSlot) ) {
		*pConsumed = true;
		return true;
	}
	errno = iCode;
	return false;
}



/* 一次系统调用发布当前批次的全部普通 SQE，并处理内核的部分消费。 */
static bool __xrtNetUringFlush(__xrt_net_uring_context* pContext)
{
	while ( pContext->PendingSubmissions != 0 ) {
		uint32 iSubmit = (uint32)pContext->PendingSubmissions;
		int iResult = __xrtNetUringEnter(
			pContext->Ring.Fd,
			iSubmit,
			0u,
			0u
		);

		if ( iResult > 0 ) {
			if ( (size_t)iResult > pContext->PendingSubmissions ) {
				errno = EIO;
				return false;
			}
			pContext->PendingSubmissions -= (size_t)iResult;
			continue;
		}
		if ( (iResult < 0) && (errno == EINTR) ) {
			continue;
		}
		if ( iResult == 0 ) {
			errno = EIO;
		}
		return false;
	}
	return true;
}



/* 按公共完成操作类型填写一个原生 Socket SQE。 */
static bool __xrtNetUringPrepare(
	__xrt_net_uring_operation* pOperation,
	__xrt_net_uring_sqe* pSQE
)
{
	pSQE->Fd = ((pOperation->Type == XNET_PORT_EVENT_FILE_READ) ||
		(pOperation->Type == XNET_PORT_EVENT_FILE_WRITE)) ?
		(int)pOperation->File : (int)pOperation->Socket->Native;
	pSQE->UserData = (uint64)(uintptr_t)pOperation;

	switch ( pOperation->Type ) {
		case XNET_PORT_EVENT_ACCEPT:
			pSQE->Opcode = XRT_NET_URING_OP_ACCEPT;
			pSQE->Address =
				(uint64)(uintptr_t)&pOperation->NativeAddress;
			pSQE->Offset =
				(uint64)(uintptr_t)&pOperation->NativeAddressSize;
			pSQE->AcceptFlags = SOCK_CLOEXEC;
			if ( (pOperation->Socket->Flags &
				XNET_SOCKET_NONBLOCK) != 0 ) {
				pSQE->AcceptFlags |= SOCK_NONBLOCK;
			}
			return true;

		case XNET_PORT_EVENT_CONNECT:
			pSQE->Opcode = XRT_NET_URING_OP_CONNECT;
			pSQE->Address =
				(uint64)(uintptr_t)&pOperation->NativeAddress;
			pSQE->Offset = (uint64)pOperation->NativeAddressSize;
			return true;

		case XNET_PORT_EVENT_READ_PROBE:
			pSQE->Opcode = XRT_NET_URING_OP_POLL_ADD;
			pSQE->PollEvents = POLLIN | POLLPRI | POLLERR | POLLHUP;
			#if defined(POLLRDHUP)
				pSQE->PollEvents |= POLLRDHUP;
			#endif
			return true;

		case XNET_PORT_EVENT_RECV_ERROR:
			pSQE->Opcode = XRT_NET_URING_OP_POLL_ADD;
			pSQE->PollEvents = POLLERR;
			return true;

		case XNET_PORT_EVENT_RECV:
		case XNET_PORT_EVENT_RECV_FROM:
		case XNET_PORT_EVENT_RECV_MSG:
			pSQE->Opcode = XRT_NET_URING_OP_RECVMSG;
			pSQE->Address =
				(uint64)(uintptr_t)&pOperation->Message;
			pSQE->Length = 1u;
			if ( pOperation->Socket->Type == XNET_SOCKET_DGRAM ) {
				pSQE->MessageFlags = MSG_TRUNC;
			}
			return true;

		case XNET_PORT_EVENT_SEND:
		case XNET_PORT_EVENT_SEND_TO:
		case XNET_PORT_EVENT_SEND_MSG:
			pSQE->Opcode = XRT_NET_URING_OP_SENDMSG;
			pSQE->Address =
				(uint64)(uintptr_t)&pOperation->Message;
			pSQE->Length = 1u;
			pSQE->MessageFlags = MSG_NOSIGNAL;
			return true;

		case XNET_PORT_EVENT_SEND_FILE:
		{
			size_t iRemaining;

			if ( (pOperation->FileBytes > pOperation->FileSize) ||
				(pOperation->PipeBytes >
				 (pOperation->FileSize - pOperation->FileBytes)) ) {
				return false;
			}
			pSQE->Opcode = XRT_NET_URING_OP_SPLICE;
			pSQE->Offset = UINT64_MAX;
			pSQE->ReadWriteFlags = XRT_NET_URING_SPLICE_F_MOVE;
			if ( pOperation->FileOutput ) {
				if ( (pOperation->PipeBytes == 0) ||
					(pOperation->PipeBytes > (size_t)UINT32_MAX) ) {
					return false;
				}
				pSQE->Fd = (int)pOperation->Socket->Native;
				pSQE->Address = UINT64_MAX;
				pSQE->Length = (uint32)pOperation->PipeBytes;
				pSQE->Tail.SpliceFdIn = pOperation->PipeRead;
			} else {
				iRemaining = pOperation->FileSize -
					pOperation->FileBytes;
				if ( iRemaining == 0 ) {
					return false;
				}
				pSQE->Fd = pOperation->PipeWrite;
				pSQE->Address = pOperation->FileOffset;
				if ( iRemaining > pOperation->Capacity ) {
					iRemaining = pOperation->Capacity;
				}
				pSQE->Length = iRemaining > (size_t)UINT32_MAX ?
					UINT32_MAX : (uint32)iRemaining;
				pSQE->Tail.SpliceFdIn = (int32)pOperation->File;
			}
			/* splice_fd_in 位于 SQE 尾部稳定 UAPI 字段。 */
			return true;
		}

		case XNET_PORT_EVENT_FILE_READ:
			pSQE->Opcode = XRT_NET_URING_OP_READV;
			pSQE->Offset = pOperation->FileOffset;
			pSQE->Address = (uint64)(uintptr_t)__xrtNetUringVectors(pOperation);
			pSQE->Length = (uint32)pOperation->Message.msg_iovlen;
			return true;

		case XNET_PORT_EVENT_FILE_WRITE:
			pSQE->Opcode = XRT_NET_URING_OP_WRITEV;
			pSQE->Offset = pOperation->FileOffset;
			pSQE->Address = (uint64)(uintptr_t)__xrtNetUringVectors(pOperation);
			pSQE->Length = (uint32)pOperation->Message.msg_iovlen;
			return true;

		default:
			return false;
	}
}



/* 提交一个完成式操作；失败路径不留下活跃节点或幽灵 CQE。 */
static bool __xrtNetUringSubmit(
	xnetport* pPort,
	const __xrt_net_port_submit* pSubmit
)
{
	__xrt_net_uring_context* pContext =
		(__xrt_net_uring_context*)pPort->Context;
	__xrt_net_uring_operation* pOperation;
	__xrt_net_uring_sqe* pSQE;
	uint32 iTail;
	uint32 iSlot;

	if ( (pSubmit->Type == XNET_PORT_EVENT_CONNECT) &&
		 pSubmit->Socket->Connecting ) {
		__xrtNetSetError(
			XERR_AGAIN,
			XNET_ERROR_PORT_SUBMIT,
			"connect",
			"socket already has an active connect",
			0
		);
		return false;
	}
	if ( (pSubmit->Type == XNET_PORT_EVENT_SEND_FILE) &&
		!pContext->Ring.Splice ) {
		__xrtNetSetError(
			XERR_UNSUPPORTED,
			XNET_ERROR_PORT_SUBMIT,
			"send-file",
			"io_uring kernel has no splice operation",
			0
		);
		return false;
	}
	if ( ((pSubmit->Type == XNET_PORT_EVENT_FILE_READ) ||
		 (pSubmit->Type == XNET_PORT_EVENT_FILE_WRITE)) &&
		 !pContext->Ring.FileIO ) {
		__xrtNetSetError(
			XERR_UNSUPPORTED,
			XNET_ERROR_PORT_SUBMIT,
			__xrtNetUringOperationName(pSubmit->Type),
			"io_uring kernel has no positioned file I/O operations",
			0
		);
		return false;
	}
	pOperation = __xrtNetUringOperationCreate(pPort, pSubmit);
	if ( pOperation == NULL ) {
		return false;
	}
	pSQE = __xrtNetUringSQE(&pContext->Ring, &iTail, &iSlot);
	if ( pSQE == NULL ) {
		if ( !__xrtNetUringFlush(pContext) ) {
			__xrtNetUringOperationRelease(pContext, pOperation);
			__xrtNetSocketSetSystemError(
				XNET_ERROR_PORT_SUBMIT,
				__xrtNetUringOperationName(pSubmit->Type),
				"flushing io_uring submissions failed",
				errno
			);
			return false;
		}
		pSQE = __xrtNetUringSQE(&pContext->Ring, &iTail, &iSlot);
		if ( pSQE == NULL ) {
			__xrtNetUringOperationRelease(pContext, pOperation);
			__xrtNetSetError(
				XERR_AGAIN,
				XNET_ERROR_PORT_SUBMIT,
				__xrtNetUringOperationName(pSubmit->Type),
				"io_uring submission queue is full",
				0
			);
			return false;
		}
	}
	if ( !__xrtNetUringPrepare(pOperation, pSQE) ) {
		__xrtNetUringOperationRelease(pContext, pOperation);
		__xrtNetSetError(
			XERR_ARGUMENT,
			XNET_ERROR_PORT_SUBMIT,
			"submit",
			"unsupported io_uring operation type",
			0
		);
		return false;
	}

	if ( pOperation->Type == XNET_PORT_EVENT_CONNECT ) {
		pOperation->Socket->Connecting = true;
	}
	__xrtNetUringTrack(pContext, pOperation);
	__xrtNetUringSQECommit(&pContext->Ring, iTail, iSlot);
	pContext->PendingSubmissions++;
	return true;
}



/* 在私有管道内持续推进完整文件区间，只向上层发布一次终态。 */
static bool __xrtNetUringFileAdvance(
	__xrt_net_uring_context* pContext,
	__xrt_net_uring_operation* pOperation,
	int32 iStage,
	int32* pFinal
)
{
	__xrt_net_uring_sqe* pSQE;
	uint32 iTail;
	uint32 iSlot;
	bool bConsumed;

	if ( pOperation->Type != XNET_PORT_EVENT_SEND_FILE ) {
		return false;
	}
	if ( (pOperation->FileBytes > pOperation->FileSize) ||
		(pOperation->PipeBytes >
		 (pOperation->FileSize - pOperation->FileBytes)) ) {
		*pFinal = pOperation->FileBytes != 0 ?
			(int32)pOperation->FileBytes : -EIO;
		return false;
	}
	if ( pOperation->CancelRequested ) {
		*pFinal = pOperation->FileBytes != 0 ?
			(int32)pOperation->FileBytes : -ECANCELED;
		return false;
	}
	if ( iStage <= 0 ) {
		*pFinal = pOperation->FileBytes != 0 ?
			(int32)pOperation->FileBytes :
			(iStage == 0 ? -EIO : iStage);
		return false;
	}
	if ( pOperation->FileOutput ) {
		if ( (size_t)iStage > pOperation->PipeBytes ) {
			*pFinal = pOperation->FileBytes != 0 ?
				(int32)pOperation->FileBytes : -EIO;
			return false;
		}
		pOperation->PipeBytes -= (size_t)iStage;
		pOperation->FileBytes += (size_t)iStage;
		if ( pOperation->PipeBytes == 0 ) {
			pOperation->FileOutput = false;
		}
	} else {
		if ( ((size_t)iStage > pOperation->Capacity) ||
			((size_t)iStage >
			 (pOperation->FileSize - pOperation->FileBytes)) ) {
			*pFinal = pOperation->FileBytes != 0 ?
				(int32)pOperation->FileBytes : -EIO;
			return false;
		}
		pOperation->PipeBytes = (size_t)iStage;
		pOperation->FileOffset += (uint64)(size_t)iStage;
		pOperation->FileOutput = true;
	}
	if ( (pOperation->FileBytes == pOperation->FileSize) &&
		(pOperation->PipeBytes == 0) ) {
		*pFinal = (int32)pOperation->FileBytes;
		return false;
	}
	pSQE = __xrtNetUringSQE(&pContext->Ring, &iTail, &iSlot);
	if ( (pSQE == NULL) || !__xrtNetUringPrepare(pOperation, pSQE) ) {
		*pFinal = pOperation->FileBytes != 0 ?
			(int32)pOperation->FileBytes : -EAGAIN;
		return false;
	}
	__xrtNetUringSQECommit(&pContext->Ring, iTail, iSlot);
	if ( !__xrtNetUringPublish(
		&pContext->Ring,
		iTail,
		iSlot,
		&bConsumed
	) ) {
		(void)bConsumed;
		*pFinal = pOperation->FileBytes != 0 ?
			(int32)pOperation->FileBytes : -errno;
		return false;
	}
	return true;
}



/* 发布一个按原操作 user_data 精确匹配的异步取消请求。 */
static bool __xrtNetUringCancelOperation(
	__xrt_net_uring_context* pContext,
	__xrt_net_uring_operation* pOperation
)
{
	__xrt_net_uring_sqe* pSQE;
	uint32 iTail;
	uint32 iSlot;
	bool bConsumed;

	if ( pOperation->CancelRequested ) {
		return true;
	}
	if ( !__xrtNetUringFlush(pContext) ) {
		__xrtNetSocketSetSystemError(
			XNET_ERROR_PORT_CANCEL,
			"cancel",
			"flushing io_uring submissions before cancellation failed",
			errno
		);
		return false;
	}
	pSQE = __xrtNetUringSQE(&pContext->Ring, &iTail, &iSlot);
	if ( pSQE == NULL ) {
		__xrtNetSetError(
			XERR_AGAIN,
			XNET_ERROR_PORT_CANCEL,
			"cancel",
			"io_uring submission queue is full",
			0
		);
		return false;
	}
	pSQE->Opcode = XRT_NET_URING_OP_ASYNC_CANCEL;
	pSQE->Fd = -1;
	pSQE->Address = (uint64)(uintptr_t)pOperation;
	pSQE->UserData = (uint64)(uintptr_t)pOperation |
		XRT_NET_URING_CONTROL_CANCEL;
	__xrtNetUringSQECommit(&pContext->Ring, iTail, iSlot);
	if ( !__xrtNetUringPublish(
		&pContext->Ring,
		iTail,
		iSlot,
		&bConsumed
	) ) {
		(void)bConsumed;
		__xrtNetSocketSetSystemError(
			XNET_ERROR_PORT_CANCEL,
			"cancel",
			"submitting io_uring cancellation failed",
			errno
		);
		return false;
	}
	pOperation->CancelRequested = true;
	pContext->PendingCancels++;
	return true;
}



/* 请求取消指定 ID；原操作或自然完成仍负责唯一公开终态。 */
static bool __xrtNetUringCancel(xnetport* pPort, uint64 Id)
{
	__xrt_net_uring_context* pContext =
		(__xrt_net_uring_context*)pPort->Context;
	__xrt_net_uring_operation* pOperation =
		__xrtNetUringFind(pContext, Id);

	if ( pOperation == NULL ) {
		__xrtNetSetError(
			XERR_NOT_FOUND,
			XNET_ERROR_PORT_CANCEL,
			"cancel",
			"network completion operation id was not found",
			0
		);
		return false;
	}
	return __xrtNetUringCancelOperation(pContext, pOperation);
}



/* 把成功 accept 返回的 fd 包装为具有明确所有权的 Socket。 */
static bool __xrtNetUringAccept(
	__xrt_net_uring_operation* pOperation,
	int iAccepted,
	xnetportevent* pEvent
)
{
	pEvent->Accepted = __xrtNetSocketAdopt(
		(uintptr_t)iAccepted,
		pOperation->Socket->Family,
		XNET_SOCKET_STREAM,
		pOperation->Socket->Flags
	);
	if ( pEvent->Accepted == NULL ) {
		const xerror* pError = xrtGetError();

		pEvent->SystemCode = (pError != NULL) ?
			xrtErrorSystemCode(pError) : 0;
		return false;
	}
	if ( !xrtNetAddrFromNative(
		&pEvent->Address,
		&pOperation->NativeAddress,
		(size_t)pOperation->NativeAddressSize
	) ) {
		const xerror* pError = xrtGetError();

		pEvent->SystemCode = (pError != NULL) ?
			xrtErrorSystemCode(pError) : 0;
		(void)xrtNetSocketClose(pEvent->Accepted);
		pEvent->Accepted = NULL;
		return false;
	}
	return true;
}



/* 将一个 CQE 转换为稳定结果，不把操作失败误报为端口等待失败。 */
static void __xrtNetUringEvent(
	__xrt_net_uring_operation* pOperation,
	int32 iResult,
	xnetportevent* pEvent
)
{
	int iCode = (iResult < 0) ? -iResult : 0;

	memset(pEvent, 0, sizeof(*pEvent));
	pEvent->Type = pOperation->Type;
	pEvent->Result = XNET_RESULT_OK;
	pEvent->SystemCode = iCode;
	pEvent->Id = pOperation->Id;
	pEvent->Socket = pOperation->Socket;
	pEvent->Address = pOperation->Address;
	pEvent->User = pOperation->User;
	if ( iResult > 0 ) {
		pEvent->Bytes = (size_t)iResult;
	}

	if ( pOperation->Type == XNET_PORT_EVENT_CONNECT ) {
		pOperation->Socket->Connecting = false;
	}
	if ( iResult < 0 ) {
		if ( (iCode == ECANCELED) ||
			 ((iCode == EINTR) && pOperation->CancelRequested) ) {
			pEvent->Result = XNET_RESULT_CANCELLED;
		} else {
			pEvent->Result = XNET_RESULT_ERROR;
			pEvent->Flags |= XNET_PORT_EVENT_ERROR;
		}
		return;
	}

	if ( pOperation->Type == XNET_PORT_EVENT_ACCEPT ) {
		pEvent->Bytes = 0;
		if ( !__xrtNetUringAccept(pOperation, iResult, pEvent) ) {
			pEvent->Result = XNET_RESULT_ERROR;
			pEvent->Flags |= XNET_PORT_EVENT_ERROR;
		}
		return;
	}

	if ( pOperation->Type == XNET_PORT_EVENT_READ_PROBE ) {
		uint32 iPoll = (uint32)iResult;

		pEvent->Bytes = 0;
		if ( (iPoll & (POLLIN | POLLPRI)) != 0 ) {
			pEvent->Flags |= XNET_PORT_EVENT_READ;
		}
		if ( (iPoll & (POLLERR | POLLNVAL)) != 0 ) {
			pEvent->Flags |= XNET_PORT_EVENT_ERROR;
		}
		if ( (iPoll & POLLHUP) != 0 ) {
			pEvent->Flags |= XNET_PORT_EVENT_HANGUP;
		}
		#if defined(POLLRDHUP)
			if ( (iPoll & POLLRDHUP) != 0 ) {
				pEvent->Flags |= XNET_PORT_EVENT_HANGUP;
			}
		#endif
		if ( pEvent->Flags == 0 ) {
			pEvent->Result = XNET_RESULT_ERROR;
			pEvent->SystemCode = EIO;
			pEvent->Flags = XNET_PORT_EVENT_ERROR;
		}
		return;
	}
	if ( pOperation->Type == XNET_PORT_EVENT_RECV_ERROR ) {
		struct iovec* pVector = __xrtNetUringVectors(pOperation);
		size_t iReceived = 0;

		pEvent->Bytes = 0;
		pEvent->SystemCode = 0;
		pEvent->Result = xrtNetSocketDgramRecvError(
			pOperation->Socket,
			pVector[0].iov_base,
			pVector[0].iov_len,
			&iReceived,
			&pEvent->DgramError
		);
		pEvent->Bytes = iReceived;
		if ( pEvent->Result == XNET_RESULT_ERROR ) {
			const xerror* pError = xrtGetError();

			pEvent->Flags |= XNET_PORT_EVENT_ERROR;
			pEvent->SystemCode = (pError != NULL) ?
				xrtErrorSystemCode(pError) : 0;
		}
		return;
	}

	if ( ((pOperation->Type == XNET_PORT_EVENT_RECV) ||
		 (pOperation->Type == XNET_PORT_EVENT_RECV_FROM) ||
		 (pOperation->Type == XNET_PORT_EVENT_RECV_MSG)) &&
		 (pOperation->Socket->Type == XNET_SOCKET_DGRAM) &&
		 (((pOperation->Message.msg_flags & MSG_TRUNC) != 0) ||
			((size_t)iResult > pOperation->Capacity)) ) {
		pEvent->Result = XNET_RESULT_TRUNCATED;
		pEvent->SystemCode = EMSGSIZE;
		pEvent->Bytes = pOperation->Capacity;
	}

	if ( (pOperation->Type == XNET_PORT_EVENT_RECV) &&
		 (pOperation->Socket->Type == XNET_SOCKET_STREAM) &&
		 (iResult == 0) ) {
		pEvent->Result = XNET_RESULT_CLOSED;
		pEvent->Flags |= XNET_PORT_EVENT_EOF;
	}
	if ( (pOperation->Type == XNET_PORT_EVENT_RECV_FROM) ||
		 (pOperation->Type == XNET_PORT_EVENT_RECV_MSG) ) {
		if ( pOperation->Type == XNET_PORT_EVENT_RECV_MSG ) {
			__xrtNetSocketDgramMetaParse(
				pOperation->Socket,
				&pEvent->Meta,
				pOperation->Message.msg_control,
				pOperation->Message.msg_controllen,
				(uint32)pOperation->Message.msg_flags
			);
		}
		if ( !xrtNetAddrFromNative(
			&pEvent->Address,
			&pOperation->NativeAddress,
			(size_t)pOperation->Message.msg_namelen
		) ) {
			const xerror* pError = xrtGetError();

			pEvent->Result = XNET_RESULT_ERROR;
			pEvent->Flags |= XNET_PORT_EVENT_ERROR;
			pEvent->SystemCode = (pError != NULL) ?
				xrtErrorSystemCode(pError) : 0;
		}
	}
}



/* 消费 CQ；控制 CQE 不占公开容量，销毁模式直接关闭 accept 结果。 */
static bool __xrtNetUringDrain(
	__xrt_net_uring_context* pContext,
	xnetportevent* pEvents,
	size_t iCapacity,
	size_t* pCount,
	bool bDiscard
)
{
	__xrt_net_uring_ring* pRing = &pContext->Ring;
	uint32 iHead = __xrtAtomic32LoadValue(
		pRing->CQHead,
		XMEMORY_RELAXED
	);
	uint32 iTail = __xrtAtomic32LoadValue(
		pRing->CQTail,
		XMEMORY_ACQUIRE
	);
	size_t iCount = 0;

	while ( iHead != iTail ) {
		__xrt_net_uring_cqe* pCQE =
			&pRing->CQEs[iHead & *pRing->CQMask];

		if ( (pCQE->UserData & XRT_NET_URING_CONTROL_CANCEL) != 0 ) {
			__xrt_net_uring_operation* pOperation =
				(__xrt_net_uring_operation*)(uintptr_t)
				(pCQE->UserData & ~XRT_NET_URING_CONTROL_CANCEL);

			if ( !pOperation->CancelCompleted ) {
				pOperation->CancelCompleted = true;
				pContext->PendingCancels--;
			}
			if ( pOperation->OperationCompleted ) {
				__xrtNetUringUnretire(pContext, pOperation);
				__xrtNetUringOperationRelease(pContext, pOperation);
			}
			iHead++;
			continue;
		}
		if ( !bDiscard && (iCount == iCapacity) ) {
			break;
		}

		{
			__xrt_net_uring_operation* pOperation =
				(__xrt_net_uring_operation*)(uintptr_t)pCQE->UserData;
			int32 iResult = pCQE->Result;

			if ( !bDiscard && __xrtNetUringFileAdvance(
				pContext,
				pOperation,
				iResult,
				&iResult
			) ) {
				iHead++;
				continue;
			}

			__xrtNetUringUntrack(pContext, pOperation);
			if ( bDiscard ) {
				if ( (pOperation->Type == XNET_PORT_EVENT_ACCEPT) &&
					 (iResult >= 0) ) {
					(void)close(iResult);
				}
				if ( pOperation->Type == XNET_PORT_EVENT_CONNECT ) {
					pOperation->Socket->Connecting = false;
				}
			} else {
				__xrtNetUringEvent(
					pOperation,
					iResult,
					&pEvents[iCount++]
				);
			}
			pOperation->OperationCompleted = true;
			if ( pOperation->CancelRequested &&
				 !pOperation->CancelCompleted ) {
				__xrtNetUringRetire(pContext, pOperation);
			} else {
				__xrtNetUringOperationRelease(pContext, pOperation);
			}
		}
		iHead++;
	}

	__xrtAtomic32StoreValue(pRing->CQHead, iHead, XMEMORY_RELEASE);
	*pCount = iCount;
	if ( (__xrtAtomic32LoadValue(
		pRing->SQDropped,
		XMEMORY_ACQUIRE
	) != 0) || (__xrtAtomic32LoadValue(
		pRing->CQOverflow,
		XMEMORY_ACQUIRE
	) != 0) ) {
		__xrtNetSetError(
			XERR_IO,
			XNET_ERROR_PORT_WAIT,
			"wait",
			"io_uring reported a queue overflow",
			0
		);
		return false;
	}
	return true;
}



/* 排空 eventfd 计数；EAGAIN 表示本轮唤醒已经完全消费。 */
static bool __xrtNetUringWakeDrain(__xrt_net_uring_context* pContext)
{
	uint64 iValue;

	for ( ;; ) {
		ssize_t iResult = read(
			pContext->WakeFd,
			&iValue,
			sizeof(iValue)
		);

		if ( iResult == (ssize_t)sizeof(iValue) ) {
			continue;
		}
		if ( (iResult < 0) && (errno == EINTR) ) {
			continue;
		}
		if ( (iResult < 0) &&
			 ((errno == EAGAIN) || (errno == EWOULDBLOCK)) ) {
			return true;
		}
		__xrtNetSocketSetSystemError(
			XNET_ERROR_PORT_WAIT,
			"wait",
			"draining io_uring wake event failed",
			(iResult < 0) ? errno : EIO
		);
		return false;
	}
}



/* 把微秒等待向上取整为 poll 毫秒，并保留无限等待。 */
static int __xrtNetUringTimeout(uint64 iTimeout)
{
	uint64 iMilliseconds;

	if ( iTimeout == UINT64_MAX ) {
		return -1;
	}
	iMilliseconds = (iTimeout / 1000u) +
		(((iTimeout % 1000u) != 0) ? 1u : 0u);
	if ( iMilliseconds > INT_MAX ) {
		return INT_MAX;
	}
	return (int)iMilliseconds;
}



/* 等待 CQ 或跨线程唤醒，并批量提取调用方容量允许的终态。 */
static xnetresult __xrtNetUringWait(
	xnetport* pPort,
	xnetportevent* pEvents,
	size_t iCapacity,
	uint64 iTimeout,
	size_t* pCount
)
{
	__xrt_net_uring_context* pContext =
		(__xrt_net_uring_context*)pPort->Context;
	struct pollfd Poll[2];
	xdeadline Deadline = (iTimeout == UINT64_MAX) ?
		XRT_DEADLINE_NEVER : xrtDeadlineAfter(iTimeout);

	*pCount = 0;
	if ( !__xrtNetUringFlush(pContext) ) {
		__xrtNetSocketSetSystemError(
			XNET_ERROR_PORT_WAIT,
			"wait",
			"flushing io_uring submissions failed",
			errno
		);
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtNetUringDrain(
		pContext,
		pEvents,
		iCapacity,
		pCount,
		false
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( *pCount != 0 ) {
		return XNET_RESULT_OK;
	}

	memset(Poll, 0, sizeof(Poll));
	Poll[0].fd = pContext->Ring.Fd;
	Poll[0].events = POLLIN;
	Poll[1].fd = pContext->WakeFd;
	Poll[1].events = POLLIN;

	for ( ;; ) {
		uint64 iRemaining = (Deadline == XRT_DEADLINE_NEVER) ?
			UINT64_MAX : xrtDeadlineRemaining(Deadline);
		int iResult = poll(Poll, 2, __xrtNetUringTimeout(iRemaining));

		if ( iResult == 0 ) {
			if ( (Deadline != XRT_DEADLINE_NEVER) &&
				 !xrtDeadlineExpired(Deadline) ) {
				continue;
			}
			return iTimeout == 0 ?
				XNET_RESULT_OK : XNET_RESULT_TIMEOUT;
		}
		if ( iResult < 0 ) {
			if ( errno == EINTR ) {
				if ( (Deadline != XRT_DEADLINE_NEVER) &&
					 xrtDeadlineExpired(Deadline) ) {
					return iTimeout == 0 ?
						XNET_RESULT_OK : XNET_RESULT_TIMEOUT;
				}
				continue;
			}
			__xrtNetSocketSetSystemError(
				XNET_ERROR_PORT_WAIT,
				"wait",
				"waiting on io_uring failed",
				errno
			);
			return XNET_RESULT_ERROR;
		}
		if ( (Poll[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0 ) {
			__xrtNetSetError(
				XERR_IO,
				XNET_ERROR_PORT_WAIT,
				"wait",
				"io_uring descriptor entered a terminal state",
				0
			);
			return XNET_RESULT_ERROR;
		}
		if ( (Poll[1].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0 ) {
			__xrtNetSetError(
				XERR_IO,
				XNET_ERROR_PORT_WAIT,
				"wait",
				"io_uring wake descriptor entered a terminal state",
				0
			);
			return XNET_RESULT_ERROR;
		}
		if ( (Poll[1].revents & POLLIN) != 0 ) {
			if ( !__xrtNetUringWakeDrain(pContext) ) {
				return XNET_RESULT_ERROR;
			}
		}
		if ( !__xrtNetUringDrain(
			pContext,
			pEvents,
			iCapacity,
			pCount,
			false
		) ) {
			return XNET_RESULT_ERROR;
		}
		return XNET_RESULT_OK;
	}
}



/* 跨线程写入可合并 eventfd；计数饱和表示已有充分唤醒。 */
static bool __xrtNetUringWake(xnetport* pPort)
{
	__xrt_net_uring_context* pContext =
		(__xrt_net_uring_context*)pPort->Context;
	uint64 iValue = 1;

	for ( ;; ) {
		ssize_t iResult = write(
			pContext->WakeFd,
			&iValue,
			sizeof(iValue)
		);

		if ( iResult == (ssize_t)sizeof(iValue) ) {
			return true;
		}
		if ( (iResult < 0) && (errno == EINTR) ) {
			continue;
		}
		if ( (iResult < 0) &&
			 ((errno == EAGAIN) || (errno == EWOULDBLOCK)) ) {
			return true;
		}
		__xrtNetSocketSetSystemError(
			XNET_ERROR_PORT_POST,
			"wake",
			"posting io_uring wake event failed",
			(iResult < 0) ? errno : EIO
		);
		return false;
	}
}



/* 初始化 ring、唤醒通道和有界操作 ID 索引。 */
static bool __xrtNetUringInit(xnetport* pPort)
{
	__xrt_net_uring_context* pContext;

	if ( pPort->Config.OperationLimit > XRT_NET_URING_ENTRY_MAX ) {
		__xrtNetSetError(
			XERR_RANGE,
			XNET_ERROR_PORT_CREATE,
			"create-port",
			"io_uring operation limit exceeds 32768",
			0
		);
		return false;
	}
	pContext = (__xrt_net_uring_context*)xrtCalloc(
		1,
		sizeof(*pContext)
	);
	if ( pContext == NULL ) {
		return false;
	}
	pContext->Ring.Fd = -1;
	pContext->WakeFd = -1;
	pContext->OperationCache.Limit = pPort->Config.OperationCache;
	if ( !__xrtNetUringRingInit(
		&pContext->Ring,
		pPort->Config.OperationLimit
	) ) {
		xrtFree(pContext);
		return false;
	}
	if ( !pContext->Ring.Splice ) {
		pPort->Capabilities &= ~((uint32)XNET_PORT_CAP_SEND_FILE);
	}
	if ( !pContext->Ring.FileIO ) {
		pPort->Capabilities &= ~((uint32)XNET_PORT_CAP_FILE_IO);
	}

	pContext->WakeFd = __xrtNetUringWakeOpen();
	if ( pContext->WakeFd < 0 ) {
		int iCode = errno;

		__xrtNetUringRingUnit(&pContext->Ring);
		xrtFree(pContext);
		__xrtNetSocketSetSystemError(
			XNET_ERROR_PORT_CREATE,
			"create-port",
			"creating io_uring wake event failed",
			iCode
		);
		return false;
	}
	pContext->OperationBucketCount = XRT_NET_PORT_BUCKET_MIN;
	pContext->OperationBucketLimit =
		__xrtNetPortBucketCount(pPort->Config.OperationLimit);
	pContext->OperationBuckets =
		(__xrt_net_uring_operation**)xrtCalloc(
			pContext->OperationBucketCount,
			sizeof(*pContext->OperationBuckets)
		);
	if ( pContext->OperationBuckets == NULL ) {
		(void)close(pContext->WakeFd);
		__xrtNetUringRingUnit(&pContext->Ring);
		xrtFree(pContext);
		return false;
	}
	pPort->Context = pContext;
	return true;
}



/* 销毁前取消并提取全部在途操作，确保返回后不再引用调用方缓冲。 */
static bool __xrtNetUringUnit(xnetport* pPort)
{
	__xrt_net_uring_context* pContext =
		(__xrt_net_uring_context*)pPort->Context;
	bool bResult = true;
	int iCode = 0;

	if ( pContext == NULL ) {
		return true;
	}

	for ( __xrt_net_uring_operation* pOperation =
		pContext->OperationHead;
		pOperation != NULL;
		pOperation = pOperation->ActiveNext ) {
		if ( !__xrtNetUringCancelOperation(pContext, pOperation) ) {
			const xerror* pError = xrtGetError();

			bResult = false;
			iCode = (pError != NULL) ?
				xrtErrorSystemCode(pError) : EIO;
			break;
		}
	}

	while ( bResult && ((pContext->OperationCount != 0) ||
		(pContext->PendingCancels != 0)) ) {
		struct pollfd Poll;
		size_t iDiscarded = 0;
		int iPoll;

		if ( !__xrtNetUringDrain(
			pContext,
			NULL,
			0,
			&iDiscarded,
			true
		) ) {
			const xerror* pError = xrtGetError();

			bResult = false;
			iCode = (pError != NULL) ?
				xrtErrorSystemCode(pError) : EIO;
			break;
		}
		if ( (pContext->OperationCount == 0) &&
			 (pContext->PendingCancels == 0) ) {
			break;
		}
		Poll.fd = pContext->Ring.Fd;
		Poll.events = POLLIN;
		Poll.revents = 0;
		do {
			iPoll = poll(&Poll, 1, -1);
		} while ( (iPoll < 0) && (errno == EINTR) );
		if ( iPoll <= 0 ) {
			bResult = false;
			iCode = (iPoll < 0) ? errno : EIO;
		} else if ( (Poll.revents &
			(POLLERR | POLLHUP | POLLNVAL)) != 0 ) {
			bResult = false;
			iCode = EIO;
		}
	}

	/* 异常路径先尽力回收竞态到达的完成，尤其是已经产生的 accept fd。 */
	if ( !bResult && (pContext->Ring.Fd >= 0) ) {
		size_t iDiscarded = 0;

		(void)__xrtNetUringDrain(
			pContext,
			NULL,
			0,
			&iDiscarded,
			true
		);

		/*
		 * mmap 会持有 ring 文件引用；必须完整解除映射并关闭 fd，
		 * 等内核撤销剩余请求后，才能释放调用方仍可能被引用的描述符。
		 */
		__xrtNetUringRingUnit(&pContext->Ring);
	}
	while ( pContext->OperationHead != NULL ) {
		__xrt_net_uring_operation* pOperation =
			pContext->OperationHead;

		__xrtNetUringUntrack(pContext, pOperation);
		if ( pOperation->Type == XNET_PORT_EVENT_CONNECT ) {
			pOperation->Socket->Connecting = false;
		}
		__xrtNetUringOperationRelease(pContext, pOperation);
	}
	while ( pContext->RetiredHead != NULL ) {
		__xrt_net_uring_operation* pOperation =
			pContext->RetiredHead;

		__xrtNetUringUnretire(pContext, pOperation);
		__xrtNetUringOperationRelease(pContext, pOperation);
	}

	if ( pContext->WakeFd >= 0 ) {
		(void)close(pContext->WakeFd);
	}
	__xrtNetUringRingUnit(&pContext->Ring);
	xrtFree(pContext->OperationBuckets);
	__xrtNetPortCacheUnit(&pContext->OperationCache);
	xrtFree(pContext);
	pPort->Context = NULL;
	if ( !bResult ) {
		__xrtNetSetError(
			XERR_IO,
			XNET_ERROR_PORT_CLOSE,
			"destroy-port",
			"quiescing io_uring failed",
			iCode
		);
	}
	return bResult;
}



/* io_uring 只公开经过探测的真实完成式能力。 */
static const __xrt_net_port_driver __xrtNetUringDriver = {
	"io_uring",
	XNET_PORT_URING,
	XNET_PORT_CAP_COMPLETION |
		XNET_PORT_CAP_BATCH |
		XNET_PORT_CAP_WAKE |
		XNET_PORT_CAP_POST |
		XNET_PORT_CAP_CANCEL |
		XNET_PORT_CAP_READ_PROBE |
		XNET_PORT_CAP_DGRAM_ERROR |
		XNET_PORT_CAP_SEND_FILE |
		XNET_PORT_CAP_FILE_IO,
	__xrtNetUringInit,
	__xrtNetUringUnit,
	NULL,
	NULL,
	__xrtNetUringSubmit,
	__xrtNetUringCancel,
	__xrtNetUringWait,
	__xrtNetUringWake
};



/* 返回 Linux io_uring 完成式网络端口驱动。 */
const __xrt_net_port_driver* __xrtNetPortUringDriver(void)
{
	return &__xrtNetUringDriver;
}

#else

/* 非 Linux 构建保留裁剪符号，但明确报告后端不可用。 */
const __xrt_net_port_driver* __xrtNetPortUringDriver(void)
{
	return NULL;
}

#endif

#endif
