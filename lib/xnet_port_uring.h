#ifndef XRT_XNET_PORT_URING_H
#define XRT_XNET_PORT_URING_H

#if defined(__linux__)
	/*
	    XRT mainline Linux network backend.

	    This header owns the Linux worker port used by xnet:
	      - native io_uring ring setup, submission, and completion harvest
	      - worker wake plus control-event integration
	      - timer harvest for the native Linux backend

	    Mainline Linux transport now requires a native io_uring ring. If the ring
	    cannot be initialized, backend init fails instead of falling back to a
	    historical poll-based socket-watch transport.
	*/

	#define __XNET_IORING_OFF_SQ_RING   0ULL
	#define __XNET_IORING_OFF_CQ_RING   0x8000000ULL
	#define __XNET_IORING_OFF_SQES      0x10000000ULL

	#define __XNET_IORING_SETUP_IOPOLL        (1u << 0)
	#define __XNET_IORING_SETUP_SQPOLL        (1u << 1)
	#define __XNET_IORING_SETUP_SQ_AFF        (1u << 2)
	#define __XNET_IORING_SETUP_CQSIZE        (1u << 3)
	#define __XNET_IORING_SETUP_CLAMP         (1u << 4)
	#define __XNET_IORING_SETUP_ATTACH_WQ     (1u << 5)

	#define __XNET_IORING_FEAT_SINGLE_MMAP    (1u << 0)
	#define __XNET_IORING_ENTER_GETEVENTS     (1u << 0)

	#define __XNET_IORING_OP_NOP              0u
	#define __XNET_IORING_OP_READV            1u
	#define __XNET_IORING_OP_WRITEV           2u
	#define __XNET_IORING_OP_FSYNC            3u
	#define __XNET_IORING_OP_READ_FIXED       4u
	#define __XNET_IORING_OP_WRITE_FIXED      5u
	#define __XNET_IORING_OP_POLL_ADD         6u
	#define __XNET_IORING_OP_POLL_REMOVE      7u
	#define __XNET_IORING_OP_SENDMSG          9u
	#define __XNET_IORING_OP_RECVMSG          10u
	#define __XNET_IORING_OP_TIMEOUT         11u
	#define __XNET_IORING_OP_ACCEPT          13u
	#define __XNET_IORING_OP_ASYNC_CANCEL    14u
	#define __XNET_IORING_OP_CONNECT         16u
	#define __XNET_IORING_OP_CLOSE           19u
	#define __XNET_IORING_OP_SEND            26u
	#define __XNET_IORING_OP_RECV            27u

	#define __XNET_URING_INLINE_RECV         8192u
	#define __XNET_URING_MAX_IOVEC           16u

	typedef struct {
		uint32 iHead;
		uint32 iTail;
		uint32 iRingMask;
		uint32 iRingEntries;
		uint32 iFlags;
		uint32 iDropped;
		uint32 iArray;
		uint32 iResv1;
		uint64 iResv2;
	} __xnet_io_uring_sqring_offsets;

	typedef struct {
		uint32 iHead;
		uint32 iTail;
		uint32 iRingMask;
		uint32 iRingEntries;
		uint32 iOverflow;
		uint32 iCqes;
		uint64 iResv[2];
	} __xnet_io_uring_cqring_offsets;

	typedef struct {
		uint32 iSqEntries;
		uint32 iCqEntries;
		uint32 iFlags;
		uint32 iSqThreadCpu;
		uint32 iSqThreadIdle;
		uint32 iFeatures;
		uint32 iWqFd;
		uint32 iResv[3];
		__xnet_io_uring_sqring_offsets tSqOff;
		__xnet_io_uring_cqring_offsets tCqOff;
	} __xnet_io_uring_params;

	typedef struct {
		uint8 iOpcode;
		uint8 iFlags;
		uint16 iIoPrio;
		int32_t iFd;
		union {
			uint64 iOff;
			uint64 iAddr2;
		};
		uint64 iAddr;
		uint32 iLen;
		union {
			uint32 iRwFlags;
			uint32 iPollEvents;
			uint32 iTimeoutFlags;
			uint32 iAcceptFlags;
			uint32 iCancelFlags;
		};
		uint64 iUserData;
		union {
			struct {
				uint16 iBufIndex;
				uint16 iPersonality;
			};
			uint64 iPad3[3];
		};
	} __xnet_io_uring_sqe;

	typedef struct {
		uint64 iUserData;
		int32_t iRes;
		uint32 iFlags;
	} __xnet_io_uring_cqe;

	typedef struct {
		int hRingFd;
		void* pSqRingBase;
		size_t iSqRingMapLen;
		void* pCqRingBase;
		size_t iCqRingMapLen;
		void* pSqesBase;
		size_t iSqesMapLen;
		uint32* pSqHead;
		uint32* pSqTail;
		uint32* pSqMask;
		uint32* pSqEntries;
		uint32* pSqFlags;
		uint32* pSqDropped;
		uint32* pSqArray;
		uint32* pCqHead;
		uint32* pCqTail;
		uint32* pCqMask;
		uint32* pCqEntries;
		uint32* pCqOverflow;
		__xnet_io_uring_cqe* pCqes;
		__xnet_io_uring_sqe* pSqes;
		__xnet_io_uring_params tParams;
		bool bSingleMmap;
		bool bReady;
	} __xnet_uring_native_ring;

	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
		static uint32 __xnetPortUringAtomicLoadAcquireU32(const volatile uint32* pValue)
		{
			return __xrtAtomicLoadU32(pValue);
		}

		static uint32 __xnetPortUringAtomicLoadRelaxedU32(const volatile uint32* pValue)
		{
			return *(const volatile uint32*)pValue;
		}

		static void __xnetPortUringAtomicStoreReleaseU32(volatile uint32* pValue, uint32 iValue)
		{
			__xrtAtomicStoreU32(pValue, iValue);
		}
	#else
		static uint32 __xnetPortUringAtomicLoadAcquireU32(const volatile uint32* pValue)
		{
			return __atomic_load_n(pValue, __ATOMIC_ACQUIRE);
		}

		static uint32 __xnetPortUringAtomicLoadRelaxedU32(const volatile uint32* pValue)
		{
			return __atomic_load_n(pValue, __ATOMIC_RELAXED);
		}

		static void __xnetPortUringAtomicStoreReleaseU32(volatile uint32* pValue, uint32 iValue)
		{
			__atomic_store_n(pValue, iValue, __ATOMIC_RELEASE);
		}
	#endif

	typedef struct __xnet_uring_io {
		struct __xnet_uring_io* pNext;
		uint16 iOpType;
		uint16 iFlags;
		uint32 iBufCount;
		uint64 iOpId;
		intptr_t hSocket;
		ptr pUserData;
		xnetaddr tAddr;
		struct sockaddr_storage tAddrStorage;
		socklen_t iAddrLen;
		struct iovec arrIov[__XNET_URING_MAX_IOVEC];
		struct msghdr tMsg;
		char aRecvBuf[__XNET_URING_INLINE_RECV];
	} __xnet_uring_io;

	typedef struct __xnet_uring_timer {
		struct __xnet_uring_timer* pNext;
		uint64 iTimerId;
		uint64 iDueMs;
	} __xnet_uring_timer;

	typedef struct __xnet_uring_post {
		struct __xnet_uring_post* pNext;
		xnetportevent tEvent;
	} __xnet_uring_post;

	typedef struct {
		int hRing;
		int hWakeFd;
		__xnet_uring_native_ring tNativeRing;
		__xnet_uring_timer* pTimers;
		__xnet_uring_post* pPostedHead;
		__xnet_uring_post* pPostedTail;
		__xnet_uring_io* pActiveIo;
		pthread_mutex_t tPostedLock;
		pthread_mutex_t tIoLock;
		pthread_mutex_t tRingLock;
	} __xnet_uring_ctx;

	static bool __xnetPortUringHasNativeRing(const xnetport* pPort)
	{
		const __xnet_uring_ctx* pCtx = pPort ? (const __xnet_uring_ctx*)pPort->pCtx : NULL;
		return pCtx && pCtx->tNativeRing.bReady;
	}

	static uint16 __xnetPortUringEventType(uint16 iOpType);

	static int __xnetPortUringSysSetup(uint32 iEntries, __xnet_io_uring_params* pParams)
	{
		#if defined(SYS_io_uring_setup)
			return (int)syscall(SYS_io_uring_setup, iEntries, pParams);
		#elif defined(__NR_io_uring_setup)
			return (int)syscall(__NR_io_uring_setup, iEntries, pParams);
		#else
			(void)iEntries;
			(void)pParams;
			errno = ENOSYS;
			return -1;
		#endif
	}

	static int __xnetPortUringSysEnter(int hRingFd, uint32 iToSubmit, uint32 iMinComplete, uint32 iFlags)
	{
		#if defined(SYS_io_uring_enter)
			return (int)syscall(SYS_io_uring_enter, hRingFd, iToSubmit, iMinComplete, iFlags, NULL, 0);
		#elif defined(__NR_io_uring_enter)
			return (int)syscall(__NR_io_uring_enter, hRingFd, iToSubmit, iMinComplete, iFlags, NULL, 0);
		#else
			(void)hRingFd;
			(void)iToSubmit;
			(void)iMinComplete;
			(void)iFlags;
			errno = ENOSYS;
			return -1;
		#endif
	}

	static int __xnetPortUringSysRegister(int hRingFd, uint32 iOpcode, const void* pArg, uint32 iNrArgs)
	{
		#if defined(SYS_io_uring_register)
			return (int)syscall(SYS_io_uring_register, hRingFd, iOpcode, pArg, iNrArgs);
		#elif defined(__NR_io_uring_register)
			return (int)syscall(__NR_io_uring_register, hRingFd, iOpcode, pArg, iNrArgs);
		#else
			(void)hRingFd;
			(void)iOpcode;
			(void)pArg;
			(void)iNrArgs;
			errno = ENOSYS;
			return -1;
		#endif
	}

	static void __xnetPortUringNativeRingUnit(__xnet_uring_native_ring* pRing)
	{
		if ( !pRing ) return;
		if ( pRing->pSqesBase && pRing->iSqesMapLen > 0 ) {
			munmap(pRing->pSqesBase, pRing->iSqesMapLen);
		}
		if ( pRing->pSqRingBase && pRing->iSqRingMapLen > 0 ) {
			munmap(pRing->pSqRingBase, pRing->iSqRingMapLen);
		}
		if ( pRing->pCqRingBase && pRing->pCqRingBase != pRing->pSqRingBase && pRing->iCqRingMapLen > 0 ) {
			munmap(pRing->pCqRingBase, pRing->iCqRingMapLen);
		}
		if ( pRing->hRingFd >= 0 ) {
			close(pRing->hRingFd);
		}
		memset(pRing, 0, sizeof(*pRing));
		pRing->hRingFd = -1;
	}

	static bool __xnetPortUringNativeRingInit(__xnet_uring_native_ring* pRing, uint32 iEntries)
	{
		size_t iSqRingLen;
		size_t iCqRingLen;
		size_t iSqesLen;
		size_t iSharedLen;
		void* pSharedMap = MAP_FAILED;
		if ( !pRing ) return false;
		memset(pRing, 0, sizeof(*pRing));
		pRing->hRingFd = -1;
		memset(&pRing->tParams, 0, sizeof(pRing->tParams));

		pRing->hRingFd = __xnetPortUringSysSetup(iEntries ? iEntries : 256u, &pRing->tParams);
		if ( pRing->hRingFd < 0 ) {
			return false;
		}

		iSqRingLen = pRing->tParams.tSqOff.iArray + (size_t)pRing->tParams.iSqEntries * sizeof(uint32);
		iCqRingLen = pRing->tParams.tCqOff.iCqes + (size_t)pRing->tParams.iCqEntries * sizeof(__xnet_io_uring_cqe);
		iSqesLen = (size_t)pRing->tParams.iSqEntries * sizeof(__xnet_io_uring_sqe);
		pRing->bSingleMmap = (pRing->tParams.iFeatures & __XNET_IORING_FEAT_SINGLE_MMAP) != 0;

		if ( pRing->bSingleMmap ) {
			iSharedLen = (iSqRingLen > iCqRingLen) ? iSqRingLen : iCqRingLen;
			pSharedMap = mmap(NULL, iSharedLen, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, pRing->hRingFd, __XNET_IORING_OFF_SQ_RING);
			if ( pSharedMap == MAP_FAILED ) {
				__xnetPortUringNativeRingUnit(pRing);
				return false;
			}
			pRing->pSqRingBase = pSharedMap;
			pRing->pCqRingBase = pSharedMap;
			pRing->iSqRingMapLen = iSharedLen;
			pRing->iCqRingMapLen = iSharedLen;
		} else {
			pRing->pSqRingBase = mmap(NULL, iSqRingLen, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, pRing->hRingFd, __XNET_IORING_OFF_SQ_RING);
			if ( pRing->pSqRingBase == MAP_FAILED ) {
				pRing->pSqRingBase = NULL;
				__xnetPortUringNativeRingUnit(pRing);
				return false;
			}
			pRing->pCqRingBase = mmap(NULL, iCqRingLen, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, pRing->hRingFd, __XNET_IORING_OFF_CQ_RING);
			if ( pRing->pCqRingBase == MAP_FAILED ) {
				pRing->pCqRingBase = NULL;
				__xnetPortUringNativeRingUnit(pRing);
				return false;
			}
			pRing->iSqRingMapLen = iSqRingLen;
			pRing->iCqRingMapLen = iCqRingLen;
		}

		pRing->pSqesBase = mmap(NULL, iSqesLen, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, pRing->hRingFd, __XNET_IORING_OFF_SQES);
		if ( pRing->pSqesBase == MAP_FAILED ) {
			pRing->pSqesBase = NULL;
			__xnetPortUringNativeRingUnit(pRing);
			return false;
		}
		pRing->iSqesMapLen = iSqesLen;

		pRing->pSqHead = (uint32*)((uint8*)pRing->pSqRingBase + pRing->tParams.tSqOff.iHead);
		pRing->pSqTail = (uint32*)((uint8*)pRing->pSqRingBase + pRing->tParams.tSqOff.iTail);
		pRing->pSqMask = (uint32*)((uint8*)pRing->pSqRingBase + pRing->tParams.tSqOff.iRingMask);
		pRing->pSqEntries = (uint32*)((uint8*)pRing->pSqRingBase + pRing->tParams.tSqOff.iRingEntries);
		pRing->pSqFlags = (uint32*)((uint8*)pRing->pSqRingBase + pRing->tParams.tSqOff.iFlags);
		pRing->pSqDropped = (uint32*)((uint8*)pRing->pSqRingBase + pRing->tParams.tSqOff.iDropped);
		pRing->pSqArray = (uint32*)((uint8*)pRing->pSqRingBase + pRing->tParams.tSqOff.iArray);
		pRing->pCqHead = (uint32*)((uint8*)pRing->pCqRingBase + pRing->tParams.tCqOff.iHead);
		pRing->pCqTail = (uint32*)((uint8*)pRing->pCqRingBase + pRing->tParams.tCqOff.iTail);
		pRing->pCqMask = (uint32*)((uint8*)pRing->pCqRingBase + pRing->tParams.tCqOff.iRingMask);
		pRing->pCqEntries = (uint32*)((uint8*)pRing->pCqRingBase + pRing->tParams.tCqOff.iRingEntries);
		pRing->pCqOverflow = (uint32*)((uint8*)pRing->pCqRingBase + pRing->tParams.tCqOff.iOverflow);
		pRing->pCqes = (__xnet_io_uring_cqe*)((uint8*)pRing->pCqRingBase + pRing->tParams.tCqOff.iCqes);
		pRing->pSqes = (__xnet_io_uring_sqe*)pRing->pSqesBase;
		pRing->bReady = true;
		return true;
	}

	static xnetchain* __xnetPortUringAllocEventChain(const void* pData, size_t iLen)
	{
		xnetchain* pChain = NULL;
		if ( (!pData && iLen > 0) || iLen > UINT32_MAX ) return NULL;
		pChain = (xnetchain*)XNET_ALLOC(sizeof(xnetchain));
		if ( !pChain ) return NULL;
		xrtNetChainInit(pChain);
		if ( iLen > 0 && !xrtNetChainAppendCopy(pChain, pData, iLen) ) {
			xrtNetChainClear(pChain);
			XNET_FREE(pChain);
			return NULL;
		}
		return pChain;
	}

	static void __xnetPortUringFreeEventChain(xnetchain* pChain)
	{
		if ( !pChain ) return;
		xrtNetChainClear(pChain);
		XNET_FREE(pChain);
	}

	static void __xnetPortUringTrackIo(__xnet_uring_ctx* pCtx, __xnet_uring_io* pIo)
	{
		if ( !pCtx || !pIo ) return;
		pthread_mutex_lock(&pCtx->tIoLock);
		pIo->pNext = pCtx->pActiveIo;
		pCtx->pActiveIo = pIo;
		pthread_mutex_unlock(&pCtx->tIoLock);
	}

	static void __xnetPortUringUntrackIo(__xnet_uring_ctx* pCtx, __xnet_uring_io* pIo)
	{
		__xnet_uring_io** ppCur;
		if ( !pCtx || !pIo ) return;
		pthread_mutex_lock(&pCtx->tIoLock);
		ppCur = &pCtx->pActiveIo;
		while ( *ppCur ) {
			if ( *ppCur == pIo ) {
				*ppCur = pIo->pNext;
				break;
			}
			ppCur = &(*ppCur)->pNext;
		}
		pthread_mutex_unlock(&pCtx->tIoLock);
	}

	static void __xnetPortUringFreeActiveIo(__xnet_uring_ctx* pCtx)
	{
		__xnet_uring_io* pList = NULL;
		if ( !pCtx ) return;
		pthread_mutex_lock(&pCtx->tIoLock);
		pList = pCtx->pActiveIo;
		pCtx->pActiveIo = NULL;
		pthread_mutex_unlock(&pCtx->tIoLock);
		while ( pList ) {
			__xnet_uring_io* pNext = pList->pNext;
			XNET_FREE(pList);
			pList = pNext;
		}
	}

	static bool __xnetPortUringNativeCanUse(const __xnet_uring_ctx* pCtx, const xnetportsubmit* pOp)
	{
		if ( !pCtx || !pOp || !pCtx->tNativeRing.bReady ) return false;
		if ( pOp->hSocket == (intptr_t)XNET_SOCKET_INVALID || pOp->hSocket == 0 ) return false;
		switch ( pOp->iOpType ) {
			case XNET_PORT_OP_ACCEPT:
				/*
				    Accept has two roles in mainline:
				      1. listener-owned native accept submissions
				      2. accepted-stream synthetic open events

				    Only the listener form should reach native io_uring accept.
				    Accepted-stream open events already carry a resolved remote
				    address, while listener submissions leave tAddr zeroed.
				*/
				return (pOp->iFlags & XNET_PORT_EVENT_F_ACCEPTED_OPEN) == 0 &&
					pOp->iOpId != 0 && pOp->tAddr.iFamily == 0;
			case XNET_PORT_OP_CONNECT:
				return pOp->tAddr.iFamily != 0;
			case XNET_PORT_OP_RECV:
				return pOp->pChain == NULL && !(pOp->pVec && pOp->iVecCount > 0);
			case XNET_PORT_OP_SEND:
				return (pOp->pVec && pOp->iVecCount > 0) || pOp->pChain != NULL;
			case XNET_PORT_OP_RECVFROM:
				return pOp->pChain == NULL && !(pOp->pVec && pOp->iVecCount > 0);
			case XNET_PORT_OP_SENDTO:
				return (pOp->pVec && pOp->iVecCount > 0) || pOp->pChain != NULL;
			default:
				return false;
		}
	}

	static __xnet_io_uring_sqe* __xnetPortUringNativeGetSqe(__xnet_uring_native_ring* pRing, uint32* pTail, uint32* pSlot)
	{
		uint32 iHead;
		uint32 iTail;
		uint32 iEntries;
		if ( !pRing || !pRing->bReady || !pTail || !pSlot ) return NULL;
		iHead = __xnetPortUringAtomicLoadAcquireU32(pRing->pSqHead);
		iTail = __xnetPortUringAtomicLoadRelaxedU32(pRing->pSqTail);
		iEntries = __xnetPortUringAtomicLoadRelaxedU32(pRing->pSqEntries);
		if ( (iTail - iHead) >= iEntries ) return NULL;
		*pTail = iTail;
		*pSlot = iTail & __xnetPortUringAtomicLoadRelaxedU32(pRing->pSqMask);
		memset(&pRing->pSqes[*pSlot], 0, sizeof(__xnet_io_uring_sqe));
		return &pRing->pSqes[*pSlot];
	}

	static void __xnetPortUringNativeCommitSqe(__xnet_uring_native_ring* pRing, uint32 iTail, uint32 iSlot)
	{
		if ( !pRing || !pRing->bReady ) return;
		pRing->pSqArray[iTail & __xnetPortUringAtomicLoadRelaxedU32(pRing->pSqMask)] = iSlot;
		__xnetPortUringAtomicStoreReleaseU32(pRing->pSqTail, iTail + 1u);
	}

	static xnet_result __xnetPortUringNativeEnter(__xnet_uring_native_ring* pRing, uint32 iToSubmit, uint32 iMinComplete, uint32 iFlags)
	{
		int iRet;
		if ( !pRing || !pRing->bReady ) return XRT_NET_ERROR;
		do {
			iRet = __xnetPortUringSysEnter(pRing->hRingFd, iToSubmit, iMinComplete, iFlags);
		} while ( iRet < 0 && errno == EINTR );
		return (iRet >= 0) ? XRT_NET_OK : XRT_NET_ERROR;
	}

	static __xnet_uring_io* __xnetPortUringAllocIO(const xnetportsubmit* pOp)
	{
		__xnet_uring_io* pIo;
		if ( !pOp ) return NULL;
		pIo = (__xnet_uring_io*)XNET_ALLOC(sizeof(__xnet_uring_io));
		if ( !pIo ) return NULL;
		memset(pIo, 0, sizeof(__xnet_uring_io));
		pIo->iOpType = pOp->iOpType;
		pIo->iFlags = pOp->iFlags;
		pIo->iOpId = pOp->iOpId;
		pIo->hSocket = pOp->hSocket;
		pIo->pUserData = pOp->pUserData;
		pIo->tAddr = pOp->tAddr;
		pIo->iAddrLen = (socklen_t)sizeof(pIo->tAddrStorage);
		return pIo;
	}

	static bool __xnetPortUringBuildBufsFromSubmit(__xnet_uring_io* pIo, const xnetportsubmit* pOp)
	{
		if ( !pIo || !pOp ) return false;
		if ( pOp->pVec && pOp->iVecCount > 0 ) {
			if ( pOp->iVecCount > __XNET_URING_MAX_IOVEC ) return false;
			for ( uint32 i = 0; i < pOp->iVecCount; ++i ) {
				pIo->arrIov[i].iov_base = (void*)pOp->pVec[i].pData;
				pIo->arrIov[i].iov_len = (size_t)pOp->pVec[i].iLen;
			}
			pIo->iBufCount = pOp->iVecCount;
			return true;
		}
		if ( pOp->pChain ) {
			xnetspan arrSpan[__XNET_URING_MAX_IOVEC];
			uint32 iSpanCount = xrtNetChainGetSpans(pOp->pChain, arrSpan, __XNET_URING_MAX_IOVEC);
			if ( iSpanCount == 0 ) return false;
			for ( uint32 i = 0; i < iSpanCount; ++i ) {
				pIo->arrIov[i].iov_base = (void*)arrSpan[i].pData;
				pIo->arrIov[i].iov_len = (size_t)arrSpan[i].iLen;
			}
			pIo->iBufCount = iSpanCount;
			return true;
		}
		return false;
	}

	static xnet_result __xnetPortUringSubmitNative(__xnet_uring_ctx* pCtx, const xnetportsubmit* pOp)
	{
		__xnet_uring_io* pIo = NULL;
		__xnet_io_uring_sqe* pSqe = NULL;
		uint32 iTail = 0;
		uint32 iSlot = 0;
		if ( !pCtx || !pOp || !pCtx->tNativeRing.bReady ) return XRT_NET_ERROR;
		pIo = __xnetPortUringAllocIO(pOp);
		if ( !pIo ) return XRT_NET_ERROR;

		switch ( pOp->iOpType ) {
			case XNET_PORT_OP_ACCEPT:
				break;

			case XNET_PORT_OP_CONNECT:
				if ( !__xnetAddrToSockAddr(&pOp->tAddr, &pIo->tAddrStorage, &pIo->iAddrLen) ) {
					XNET_FREE(pIo);
					return XRT_NET_ERROR;
				}
				break;

			case XNET_PORT_OP_RECV:
				break;

			case XNET_PORT_OP_SEND:
				if ( !__xnetPortUringBuildBufsFromSubmit(pIo, pOp) ) {
					XNET_FREE(pIo);
					return XRT_NET_ERROR;
				}
				memset(&pIo->tMsg, 0, sizeof(pIo->tMsg));
				pIo->tMsg.msg_iov = pIo->arrIov;
				pIo->tMsg.msg_iovlen = pIo->iBufCount;
				break;

			case XNET_PORT_OP_RECVFROM:
				memset(&pIo->tMsg, 0, sizeof(pIo->tMsg));
				pIo->arrIov[0].iov_base = pIo->aRecvBuf;
				pIo->arrIov[0].iov_len = sizeof(pIo->aRecvBuf);
				pIo->iBufCount = 1;
				pIo->tMsg.msg_name = &pIo->tAddrStorage;
				pIo->tMsg.msg_namelen = sizeof(pIo->tAddrStorage);
				pIo->tMsg.msg_iov = pIo->arrIov;
				pIo->tMsg.msg_iovlen = 1;
				break;

			case XNET_PORT_OP_SENDTO:
				if ( !__xnetAddrToSockAddr(&pOp->tAddr, &pIo->tAddrStorage, &pIo->iAddrLen) ||
					!__xnetPortUringBuildBufsFromSubmit(pIo, pOp) ) {
					XNET_FREE(pIo);
					return XRT_NET_ERROR;
				}
				memset(&pIo->tMsg, 0, sizeof(pIo->tMsg));
				pIo->tMsg.msg_name = &pIo->tAddrStorage;
				pIo->tMsg.msg_namelen = pIo->iAddrLen;
				pIo->tMsg.msg_iov = pIo->arrIov;
				pIo->tMsg.msg_iovlen = pIo->iBufCount;
				break;

			default:
				XNET_FREE(pIo);
				return XRT_NET_ERROR;
		}

		pthread_mutex_lock(&pCtx->tRingLock);
		pSqe = __xnetPortUringNativeGetSqe(&pCtx->tNativeRing, &iTail, &iSlot);
		if ( !pSqe ) {
			pthread_mutex_unlock(&pCtx->tRingLock);
			XNET_FREE(pIo);
			return XRT_NET_ERROR;
		}

		pSqe->iFd = (int)pOp->hSocket;
		pSqe->iUserData = (uint64)(uintptr_t)pIo;
		switch ( pOp->iOpType ) {
			case XNET_PORT_OP_ACCEPT:
				pSqe->iOpcode = __XNET_IORING_OP_ACCEPT;
				pSqe->iAddr = (uint64)(uintptr_t)&pIo->tAddrStorage;
				pSqe->iOff = (uint64)(uintptr_t)&pIo->iAddrLen;
				break;

			case XNET_PORT_OP_CONNECT:
				pSqe->iOpcode = __XNET_IORING_OP_CONNECT;
				pSqe->iAddr = (uint64)(uintptr_t)&pIo->tAddrStorage;
				pSqe->iOff = (uint64)pIo->iAddrLen;
				break;

			case XNET_PORT_OP_RECV:
				pSqe->iOpcode = __XNET_IORING_OP_RECV;
				pSqe->iAddr = (uint64)(uintptr_t)pIo->aRecvBuf;
				pSqe->iLen = (uint32)sizeof(pIo->aRecvBuf);
				break;

			case XNET_PORT_OP_SEND:
				pSqe->iOpcode = __XNET_IORING_OP_SENDMSG;
				pSqe->iAddr = (uint64)(uintptr_t)&pIo->tMsg;
				pSqe->iLen = 1u;
				break;

			case XNET_PORT_OP_RECVFROM:
				pSqe->iOpcode = __XNET_IORING_OP_RECVMSG;
				pSqe->iAddr = (uint64)(uintptr_t)&pIo->tMsg;
				pSqe->iLen = 1u;
				break;

			case XNET_PORT_OP_SENDTO:
				pSqe->iOpcode = __XNET_IORING_OP_SENDMSG;
				pSqe->iAddr = (uint64)(uintptr_t)&pIo->tMsg;
				pSqe->iLen = 1u;
				break;
		}

		__xnetPortUringTrackIo(pCtx, pIo);
		__xnetPortUringNativeCommitSqe(&pCtx->tNativeRing, iTail, iSlot);
		if ( __xnetPortUringNativeEnter(&pCtx->tNativeRing, 1u, 0u, 0u) != XRT_NET_OK ) {
			pthread_mutex_unlock(&pCtx->tRingLock);
			return XRT_NET_ERROR;
		}
		pthread_mutex_unlock(&pCtx->tRingLock);
		return XRT_NET_OK;
	}

	static bool __xnetPortUringBuildIoEvent(__xnet_uring_io* pIo, int iRes, xnetportevent* pEvent)
	{
		int iErr = (iRes < 0) ? -iRes : 0;
		if ( !pIo || !pEvent ) return false;
		memset(pEvent, 0, sizeof(xnetportevent));
		pEvent->iType = __xnetPortUringEventType(pIo->iOpType);
		pEvent->iStatus = (iRes >= 0) ? XRT_NET_OK : XRT_NET_ERROR;
		pEvent->iOpId = pIo->iOpId;
		pEvent->hSocket = pIo->hSocket;
		pEvent->pUserData = pIo->pUserData;
		pEvent->tAddr = pIo->tAddr;
		pEvent->iBytes = (iRes > 0) ? (uint32)iRes : 0u;

		if ( pIo->iOpType == XNET_PORT_OP_ACCEPT ) {
			if ( iRes >= 0 ) {
				pEvent->hSocket = (intptr_t)iRes;
				(void)__xnetAddrFromSockAddr(&pEvent->tAddr, (const struct sockaddr*)&pIo->tAddrStorage);
			}
			return true;
		}

		if ( pIo->iOpType == XNET_PORT_OP_CONNECT ) {
			return true;
		}

		if ( pIo->iOpType == XNET_PORT_OP_RECV ) {
			if ( iRes > 0 ) {
				pEvent->pChain = __xnetPortUringAllocEventChain(pIo->aRecvBuf, (size_t)iRes);
				if ( !pEvent->pChain ) {
					pEvent->iStatus = XRT_NET_ERROR;
					pEvent->iBytes = 0;
				}
			} else if ( iRes == 0 || iErr == ECONNRESET || iErr == ENOTCONN || iErr == EBADF ) {
				pEvent->iStatus = XRT_NET_CLOSED;
				pEvent->iFlags |= XNET_PORT_EVENT_F_EOF;
			}
			return true;
		}

		if ( pIo->iOpType == XNET_PORT_OP_RECVFROM ) {
			if ( iRes >= 0 ) {
				pEvent->pChain = __xnetPortUringAllocEventChain(pIo->aRecvBuf, (size_t)iRes);
				if ( !pEvent->pChain ) {
					pEvent->iStatus = XRT_NET_ERROR;
					pEvent->iBytes = 0;
				}
				(void)__xnetAddrFromSockAddr(&pEvent->tAddr, (const struct sockaddr*)&pIo->tAddrStorage);
			}
			return true;
		}

		return true;
	}

	static uint32 __xnetPortUringDrainNative(__xnet_uring_ctx* pCtx, xnetportevent* pEvents, uint32 iMaxEvents)
	{
		uint32 iCount = 0;
		uint32 iHead;
		uint32 iTail;
		if ( !pCtx || !pEvents || iMaxEvents == 0 || !pCtx->tNativeRing.bReady ) return 0;
		iHead = __xnetPortUringAtomicLoadAcquireU32(pCtx->tNativeRing.pCqHead);
		iTail = __xnetPortUringAtomicLoadAcquireU32(pCtx->tNativeRing.pCqTail);
		while ( iHead != iTail && iCount < iMaxEvents ) {
			__xnet_io_uring_cqe* pCqe = &pCtx->tNativeRing.pCqes[iHead & __xnetPortUringAtomicLoadRelaxedU32(pCtx->tNativeRing.pCqMask)];
			__xnet_uring_io* pIo = (__xnet_uring_io*)(uintptr_t)pCqe->iUserData;
			if ( pIo ) {
				__xnetPortUringUntrackIo(pCtx, pIo);
				if ( __xnetPortUringBuildIoEvent(pIo, pCqe->iRes, &pEvents[iCount]) ) {
					++iCount;
				}
				XNET_FREE(pIo);
			}
			++iHead;
		}
		__xnetPortUringAtomicStoreReleaseU32(pCtx->tNativeRing.pCqHead, iHead);
		return iCount;
	}

	static xnet_result __xnetPortUringWake(xnetport* pPort);

	static uint16 __xnetPortUringEventType(uint16 iOpType)
	{
		switch ( iOpType ) {
			case XNET_PORT_OP_ACCEPT: return XNET_PORT_EVENT_ACCEPT;
			case XNET_PORT_OP_CONNECT: return XNET_PORT_EVENT_CONNECT;
			case XNET_PORT_OP_RECV: return XNET_PORT_EVENT_RECV;
			case XNET_PORT_OP_SEND: return XNET_PORT_EVENT_SEND;
			case XNET_PORT_OP_RECVFROM: return XNET_PORT_EVENT_RECVFROM;
			case XNET_PORT_OP_SENDTO: return XNET_PORT_EVENT_SENDTO;
			case XNET_PORT_OP_TIMER: return XNET_PORT_EVENT_TIMER;
			case XNET_PORT_OP_WAKE: return XNET_PORT_EVENT_WAKE;
			case XNET_PORT_OP_CLOSE: return XNET_PORT_EVENT_CLOSE;
			default: return XNET_PORT_EVENT_NONE;
		}
	}

	static uint32 __xnetPortUringSubmitBytes(const xnetportsubmit* pOp)
	{
		uint64 iBytes = 0;
		if ( !pOp ) return 0;
		if ( pOp->pVec && pOp->iVecCount > 0 ) {
			for ( uint32 i = 0; i < pOp->iVecCount; ++i ) {
				iBytes += pOp->pVec[i].iLen;
			}
		} else if ( pOp->pChain ) {
			iBytes = xrtNetChainBytes(pOp->pChain);
		}
		if ( iBytes > 0xffffffffu ) iBytes = 0xffffffffu;
		return (uint32)iBytes;
	}

	static uint64 __xnetPortUringNowMs(void)
	{
		struct timespec tNow;
		clock_gettime(CLOCK_MONOTONIC, &tNow);
		return (uint64)tNow.tv_sec * 1000u + (uint64)(tNow.tv_nsec / 1000000u);
	}

	static bool __xnetPortUringValidOp(uint16 iType)
	{
		switch ( iType ) {
			case XNET_PORT_OP_ACCEPT:
			case XNET_PORT_OP_CONNECT:
			case XNET_PORT_OP_RECV:
			case XNET_PORT_OP_SEND:
			case XNET_PORT_OP_RECVFROM:
			case XNET_PORT_OP_SENDTO:
			case XNET_PORT_OP_TIMER:
			case XNET_PORT_OP_WAKE:
			case XNET_PORT_OP_CLOSE:
				return true;
			default:
				return false;
		}
	}

	static void __xnetPortUringFreeTimers(__xnet_uring_ctx* pCtx)
	{
		while ( pCtx && pCtx->pTimers ) {
			__xnet_uring_timer* pNext = pCtx->pTimers->pNext;
			XNET_FREE(pCtx->pTimers);
			pCtx->pTimers = pNext;
		}
	}

	static void __xnetPortUringFreePosts(__xnet_uring_ctx* pCtx)
	{
		if ( !pCtx ) return;
		while ( pCtx->pPostedHead ) {
			__xnet_uring_post* pNext = pCtx->pPostedHead->pNext;
			XNET_FREE(pCtx->pPostedHead);
			pCtx->pPostedHead = pNext;
		}
		pCtx->pPostedTail = NULL;
	}

	static bool __xnetPortUringQueueEvent(__xnet_uring_ctx* pCtx, const xnetportevent* pEvent)
	{
		__xnet_uring_post* pPost;
		if ( !pCtx || !pEvent ) return false;
		pPost = (__xnet_uring_post*)XNET_ALLOC(sizeof(__xnet_uring_post));
		if ( !pPost ) return false;
		memset(pPost, 0, sizeof(__xnet_uring_post));
		pPost->tEvent = *pEvent;
		pthread_mutex_lock(&pCtx->tPostedLock);
		if ( pCtx->pPostedTail ) {
			pCtx->pPostedTail->pNext = pPost;
		} else {
			pCtx->pPostedHead = pPost;
		}
		pCtx->pPostedTail = pPost;
		pthread_mutex_unlock(&pCtx->tPostedLock);
		return true;
	}

	static uint32 __xnetPortUringDrainPosted(__xnet_uring_ctx* pCtx, xnetportevent* pEvents, uint32 iMaxEvents)
	{
		uint32 iCount = 0;
		__xnet_uring_post* pHead;
		__xnet_uring_post* pTail;
		if ( !pCtx || !pEvents || iMaxEvents == 0 ) return 0;
		pthread_mutex_lock(&pCtx->tPostedLock);
		pHead = pCtx->pPostedHead;
		pTail = pCtx->pPostedTail;
		pCtx->pPostedHead = NULL;
		pCtx->pPostedTail = NULL;
		pthread_mutex_unlock(&pCtx->tPostedLock);
		(void)pTail;

		while ( pHead && iCount < iMaxEvents ) {
			__xnet_uring_post* pNext = pHead->pNext;
			pEvents[iCount++] = pHead->tEvent;
			XNET_FREE(pHead);
			pHead = pNext;
		}

		while ( pHead ) {
			__xnet_uring_post* pNext = pHead->pNext;
			pthread_mutex_lock(&pCtx->tPostedLock);
			if ( pCtx->pPostedTail ) {
				pCtx->pPostedTail->pNext = pHead;
			} else {
				pCtx->pPostedHead = pHead;
			}
			pCtx->pPostedTail = pHead;
			pHead->pNext = NULL;
			pthread_mutex_unlock(&pCtx->tPostedLock);
			pHead = pNext;
		}

		return iCount;
	}

	static uint32 __xnetPortUringHarvestTimers(__xnet_uring_ctx* pCtx, xnetportevent* pEvents, uint32 iMaxEvents)
	{
		uint32 iCount = 0;
		uint64 iNowMs = __xnetPortUringNowMs();
		__xnet_uring_timer** ppCur = pCtx ? &pCtx->pTimers : NULL;
		while ( ppCur && *ppCur && iCount < iMaxEvents ) {
			__xnet_uring_timer* pNode = *ppCur;
			if ( pNode->iDueMs > iNowMs ) {
				ppCur = &pNode->pNext;
				continue;
			}
			memset(&pEvents[iCount], 0, sizeof(xnetportevent));
			pEvents[iCount].iType = XNET_PORT_EVENT_TIMER;
			pEvents[iCount].iStatus = XRT_NET_OK;
			pEvents[iCount].iOpId = pNode->iTimerId;
			iCount++;
			*ppCur = pNode->pNext;
			XNET_FREE(pNode);
		}
		return iCount;
	}

	static xnet_result __xnetPortUringInit(xnetport* pPort, const xnetportconfig* pCfg, ptr pOwner)
	{
		__xnet_uring_ctx* pCtx;
		(void)pOwner;
		if ( !pPort || !pCfg ) return XRT_NET_ERROR;
		pCtx = (__xnet_uring_ctx*)XNET_ALLOC(sizeof(__xnet_uring_ctx));
		if ( !pCtx ) return XRT_NET_ERROR;
		memset(pCtx, 0, sizeof(__xnet_uring_ctx));
		pCtx->hRing = -1;
		pCtx->hWakeFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
		if ( pCtx->hWakeFd < 0 ) {
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		if ( pthread_mutex_init(&pCtx->tPostedLock, NULL) != 0 ) {
			close(pCtx->hWakeFd);
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		if ( pthread_mutex_init(&pCtx->tIoLock, NULL) != 0 ) {
			pthread_mutex_destroy(&pCtx->tPostedLock);
			close(pCtx->hWakeFd);
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		if ( pthread_mutex_init(&pCtx->tRingLock, NULL) != 0 ) {
			pthread_mutex_destroy(&pCtx->tIoLock);
			pthread_mutex_destroy(&pCtx->tPostedLock);
			close(pCtx->hWakeFd);
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		if ( !__xnetPortUringNativeRingInit(&pCtx->tNativeRing, pCfg->iSqEntries ? pCfg->iSqEntries : 256u) ) {
			pthread_mutex_destroy(&pCtx->tRingLock);
			pthread_mutex_destroy(&pCtx->tIoLock);
			pthread_mutex_destroy(&pCtx->tPostedLock);
			close(pCtx->hWakeFd);
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}
		pCtx->hRing = pCtx->tNativeRing.hRingFd;
		pPort->pCtx = pCtx;
		return XRT_NET_OK;
	}

	static void __xnetPortUringUnit(xnetport* pPort)
	{
		__xnet_uring_ctx* pCtx = pPort ? (__xnet_uring_ctx*)pPort->pCtx : NULL;
		if ( !pCtx ) return;
		__xnetPortUringFreeTimers(pCtx);
		__xnetPortUringFreePosts(pCtx);
		__xnetPortUringFreeActiveIo(pCtx);
		__xnetPortUringNativeRingUnit(&pCtx->tNativeRing);
		pthread_mutex_destroy(&pCtx->tPostedLock);
		pthread_mutex_destroy(&pCtx->tIoLock);
		pthread_mutex_destroy(&pCtx->tRingLock);
		if ( pCtx->hWakeFd >= 0 ) {
			close(pCtx->hWakeFd);
		}
		XNET_FREE(pCtx);
		if ( pPort ) pPort->pCtx = NULL;
	}

	static xnet_result __xnetPortUringSubmit(xnetport* pPort, const xnetportsubmit* pOps, uint32 iCount)
	{
		uint32 i;
		bool bNeedWake = false;
		__xnet_uring_ctx* pCtx = pPort ? (__xnet_uring_ctx*)pPort->pCtx : NULL;
		if ( !pCtx || !pOps || iCount == 0 ) return XRT_NET_ERROR;
		for ( i = 0; i < iCount; ++i ) {
			xnetportevent tEvent;
			if ( !__xnetPortUringValidOp(pOps[i].iOpType) ) {
				return XRT_NET_ERROR;
			}
			if ( pOps[i].iOpType == XNET_PORT_OP_CLOSE ) {
				continue;
			}
			if ( __xnetPortUringNativeCanUse(pCtx, &pOps[i]) ) {
				if ( __xnetPortUringSubmitNative(pCtx, &pOps[i]) != XRT_NET_OK ) {
					return XRT_NET_ERROR;
				}
				continue;
			}
			memset(&tEvent, 0, sizeof(tEvent));
			tEvent.iType = __xnetPortUringEventType(pOps[i].iOpType);
			tEvent.iStatus = XRT_NET_OK;
			tEvent.iFlags = pOps[i].iFlags;
			tEvent.iBytes = __xnetPortUringSubmitBytes(&pOps[i]);
			tEvent.iOpId = pOps[i].iOpId;
			tEvent.hSocket = pOps[i].hSocket;
			tEvent.pUserData = pOps[i].pUserData;
			tEvent.pChain = pOps[i].pChain;
			tEvent.tAddr = pOps[i].tAddr;
			if ( tEvent.iType == XNET_PORT_EVENT_NONE || !__xnetPortUringQueueEvent(pCtx, &tEvent) ) {
				return XRT_NET_ERROR;
			}
			bNeedWake = true;
		}
		if ( bNeedWake ) {
			(void)__xnetPortUringWake(pPort);
		}
		return XRT_NET_OK;
	}

	static uint32 __xnetPortUringHarvest(xnetport* pPort, xnetportevent* pEvents, uint32 iMaxEvents, uint32 iTimeoutMs)
	{
		uint32 iCount = 0;
		__xnet_uring_ctx* pCtx = pPort ? (__xnet_uring_ctx*)pPort->pCtx : NULL;
		if ( !pCtx || !pEvents || iMaxEvents == 0 ) return 0;

		iCount += __xnetPortUringHarvestTimers(pCtx, pEvents + iCount, iMaxEvents - iCount);
		if ( iCount >= iMaxEvents ) return iCount;

		iCount += __xnetPortUringDrainPosted(pCtx, pEvents + iCount, iMaxEvents - iCount);
		if ( iCount >= iMaxEvents ) return iCount;

		iCount += __xnetPortUringDrainNative(pCtx, pEvents + iCount, iMaxEvents - iCount);
		if ( iCount >= iMaxEvents ) return iCount;

		{
			struct pollfd arrPoll[2];
			nfds_t iPollCount = 0;
			int iWakeIndex = -1;
			int iRingIndex = -1;
			int iPollRet;

			memset(arrPoll, 0, sizeof(arrPoll));
			if ( pCtx->hWakeFd >= 0 ) {
				iWakeIndex = (int)iPollCount;
				arrPoll[iPollCount].fd = pCtx->hWakeFd;
				arrPoll[iPollCount].events = POLLIN;
				++iPollCount;
			}
			iRingIndex = (int)iPollCount;
			arrPoll[iPollCount].fd = pCtx->tNativeRing.hRingFd;
			arrPoll[iPollCount].events = POLLIN;
			++iPollCount;

			iPollRet = poll(arrPoll, iPollCount, (iCount > 0) ? 0 : (int)iTimeoutMs);
			if ( iPollRet > 0 ) {
				if ( iWakeIndex >= 0 && (arrPoll[iWakeIndex].revents & POLLIN) ) {
					uint64 iWakeCount = 0;
					if ( read(pCtx->hWakeFd, &iWakeCount, sizeof(iWakeCount)) == (ssize_t)sizeof(iWakeCount) ) {
						uint32 iPosted = __xnetPortUringDrainPosted(pCtx, pEvents + iCount, iMaxEvents - iCount);
						iCount += iPosted;
						if ( iPosted == 0 && iCount < iMaxEvents ) {
							memset(&pEvents[iCount], 0, sizeof(xnetportevent));
							pEvents[iCount].iType = XNET_PORT_EVENT_WAKE;
							pEvents[iCount].iStatus = XRT_NET_OK;
							pEvents[iCount].iBytes = (uint32)((iWakeCount > 0xffffffffu) ? 0xffffffffu : iWakeCount);
							++iCount;
						}
					}
				}
				if ( iCount < iMaxEvents && iRingIndex >= 0 &&
					(arrPoll[iRingIndex].revents & (POLLIN | POLLERR | POLLHUP)) ) {
					iCount += __xnetPortUringDrainNative(pCtx, pEvents + iCount, iMaxEvents - iCount);
				}
			}
		}

		return iCount;
	}

	static xnet_result __xnetPortUringWake(xnetport* pPort)
	{
		__xnet_uring_ctx* pCtx = pPort ? (__xnet_uring_ctx*)pPort->pCtx : NULL;
		uint64 iWakeCount = 1;
		if ( !pCtx || pCtx->hWakeFd < 0 ) return XRT_NET_ERROR;
		if ( write(pCtx->hWakeFd, &iWakeCount, sizeof(iWakeCount)) != (ssize_t)sizeof(iWakeCount) ) {
			if ( errno == EAGAIN ) return XRT_NET_OK;
			return XRT_NET_ERROR;
		}
		return XRT_NET_OK;
	}

	static xnet_result __xnetPortUringArmTimer(xnetport* pPort, uint64 iTimerId, uint32 iDelayMs)
	{
		__xnet_uring_ctx* pCtx = pPort ? (__xnet_uring_ctx*)pPort->pCtx : NULL;
		__xnet_uring_timer* pNode;
		if ( !pCtx ) return XRT_NET_ERROR;
		pNode = (__xnet_uring_timer*)XNET_ALLOC(sizeof(__xnet_uring_timer));
		if ( !pNode ) return XRT_NET_ERROR;
		memset(pNode, 0, sizeof(__xnet_uring_timer));
		pNode->iTimerId = iTimerId;
		pNode->iDueMs = __xnetPortUringNowMs() + (uint64)iDelayMs;
		pNode->pNext = pCtx->pTimers;
		pCtx->pTimers = pNode;
		return XRT_NET_OK;
	}

	static xnet_result __xnetPortUringCancelTimer(xnetport* pPort, uint64 iTimerId)
	{
		__xnet_uring_ctx* pCtx = pPort ? (__xnet_uring_ctx*)pPort->pCtx : NULL;
		__xnet_uring_timer** ppCur = pCtx ? &pCtx->pTimers : NULL;
		if ( !ppCur ) return XRT_NET_ERROR;
		while ( *ppCur ) {
			__xnet_uring_timer* pNode = *ppCur;
			if ( pNode->iTimerId == iTimerId ) {
				*ppCur = pNode->pNext;
				XNET_FREE(pNode);
				return XRT_NET_OK;
			}
			ppCur = &pNode->pNext;
		}
		return XRT_NET_ERROR;
	}

	static const xnetportops __g_xnetPortUringOps = {
		"xnet-port-uring",
		__xnetPortUringInit,
		__xnetPortUringUnit,
		__xnetPortUringSubmit,
		__xnetPortUringHarvest,
		__xnetPortUringWake,
		__xnetPortUringArmTimer,
		__xnetPortUringCancelTimer
	};

	static const xnetportops* xrtNetPortUringOps(void)
	{
		return &__g_xnetPortUringOps;
	}

#else

	static bool __xnetPortUringHasNativeRing(const xnetport* pPort)
	{
		(void)pPort;
		return false;
	}

	static const xnetportops* xrtNetPortUringOps(void)
	{
		return NULL;
	}

#endif


#endif
