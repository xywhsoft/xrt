#ifndef XRT_XNET_PORT_IOCP_H
#define XRT_XNET_PORT_IOCP_H

#include "xnet_port.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <mswsock.h>
	#if defined(XNET_DEBUG_IOCP_NATIVE)
		#include <stdio.h>
	#endif

	/*
	    XRT mainline Windows network backend.

	    This header implements the worker port used on Windows:
	      - IOCP-backed wake and completion harvest
	      - overlapped accept / recv / send / datagram submissions
	      - worker timer and synthetic control events that share the same
	        completion queue

	    It is the production Windows transport path used by xnet workers.
	*/

	#define __XNET_IOCP_KIND_POST         1u
	#define __XNET_IOCP_KIND_IO           2u
	#define __XNET_IOCP_INLINE_RECV       8192u
	#define __XNET_IOCP_MAX_IOVEC         16u
	#define __XNET_IOCP_ACCEPT_ADDR_SPACE ((DWORD)(sizeof(struct sockaddr_storage) + 16))

	typedef struct {
		OVERLAPPED tOverlapped;
		uint32 iKind;
	} __xnet_iocp_header;

	typedef struct __xnet_iocp_post {
		OVERLAPPED tOverlapped;
		uint32 iKind;
		xnetportevent tEvent;
	} __xnet_iocp_post;

	typedef struct __xnet_iocp_timer {
		struct __xnet_iocp_timer* pNext;
		uint64 iTimerId;
		uint64 iDueMs;
	} __xnet_iocp_timer;

	typedef struct __xnet_iocp_bound_socket {
		struct __xnet_iocp_bound_socket* pNext;
		SOCKET hSocket;
	} __xnet_iocp_bound_socket;

	typedef struct __xnet_iocp_io {
		OVERLAPPED tOverlapped;
		uint32 iKind;
		uint16 iOpType;
		uint16 iFlags;
		uint32 iBufCount;
		uint64 iOpId;
		intptr_t hSocket;
		intptr_t hAuxSocket;
		ptr pUserData;
		xnetaddr tAddr;
		WSABUF arrBuf[__XNET_IOCP_MAX_IOVEC];
		struct sockaddr_storage tAddrStorage;
		int iAddrLen;
		DWORD iRecvFlags;
		SOCKET hAcceptSocket;
		char aAcceptBuf[__XNET_IOCP_ACCEPT_ADDR_SPACE * 2u];
		char aRecvBuf[__XNET_IOCP_INLINE_RECV];
	} __xnet_iocp_io;

	typedef struct {
		HANDLE hIOCP;
		__xnet_iocp_timer* pTimers;
		__xnet_iocp_bound_socket* pBoundSockets;
		LPFN_ACCEPTEX pfnAcceptEx;
		LPFN_CONNECTEX pfnConnectEx;
		CRITICAL_SECTION tExtLock;
	} __xnet_iocp_ctx;

	static uint16 __xnetPortIOCPEventType(uint16 iOpType)
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

	static uint32 __xnetPortIOCPSubmitBytes(const xnetportsubmit* pOp)
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

	static uint64 __xnetPortIOCPNowMs(void)
	{
		return (uint64)GetTickCount64();
	}

	static bool __xnetPortIOCPValidOp(uint16 iType)
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

	static bool __xnetPortIOCPHasNativeSocket(const xnetportsubmit* pOp)
	{
		return pOp && pOp->hSocket != (intptr_t)XNET_SOCKET_INVALID && pOp->hSocket != 0;
	}

	static __xnet_iocp_post* __xnetPortIOCPAllocPost(uint16 iType, uint16 iFlags, xnet_result iStatus, uint32 iBytes, uint64 iOpId)
	{
		__xnet_iocp_post* pPost = (__xnet_iocp_post*)XNET_ALLOC(sizeof(__xnet_iocp_post));
		if ( !pPost ) return NULL;
		memset(pPost, 0, sizeof(__xnet_iocp_post));
		pPost->iKind = __XNET_IOCP_KIND_POST;
		pPost->tEvent.iType = iType;
		pPost->tEvent.iFlags = iFlags;
		pPost->tEvent.iStatus = iStatus;
		pPost->tEvent.iBytes = iBytes;
		pPost->tEvent.iOpId = iOpId;
		return pPost;
	}

	static xnetchain* __xnetPortIOCPAllocEventChain(const void* pData, size_t iLen)
	{
		xnetchain* pChain = NULL;

		if ( (!pData && iLen > 0) || iLen > UINT32_MAX ) {
			return NULL;
		}

		pChain = (xnetchain*)XNET_ALLOC(sizeof(xnetchain));
		if ( !pChain ) {
			return NULL;
		}

		xrtNetChainInit(pChain);
		if ( iLen > 0 && !xrtNetChainAppendCopy(pChain, pData, iLen) ) {
			xrtNetChainClear(pChain);
			XNET_FREE(pChain);
			return NULL;
		}
		return pChain;
	}

	static void __xnetPortIOCPFreeEventChain(xnetchain* pChain)
	{
		if ( !pChain ) return;
		xrtNetChainClear(pChain);
		XNET_FREE(pChain);
	}

	static bool __xnetPortIOCPBindSocket(__xnet_iocp_ctx* pCtx, SOCKET hSocket)
	{
		HANDLE hAssoc;
		__xnet_iocp_bound_socket* pNode = NULL;
		bool bBound = false;
		if ( !pCtx || !pCtx->hIOCP || hSocket == XNET_SOCKET_INVALID ) return false;
		EnterCriticalSection(&pCtx->tExtLock);
		for ( pNode = pCtx->pBoundSockets; pNode; pNode = pNode->pNext ) {
			if ( pNode->hSocket == hSocket ) {
				LeaveCriticalSection(&pCtx->tExtLock);
				return true;
			}
		}
		pNode = (__xnet_iocp_bound_socket*)XNET_ALLOC(sizeof(__xnet_iocp_bound_socket));
		if ( pNode ) {
			memset(pNode, 0, sizeof(__xnet_iocp_bound_socket));
			pNode->hSocket = hSocket;
		}
		hAssoc = CreateIoCompletionPort((HANDLE)hSocket, pCtx->hIOCP, 0, 0);
		if ( hAssoc == pCtx->hIOCP ) {
			if ( pNode ) {
				pNode->pNext = pCtx->pBoundSockets;
				pCtx->pBoundSockets = pNode;
				pNode = NULL;
			}
			bBound = true;
		}
		LeaveCriticalSection(&pCtx->tExtLock);
		if ( pNode ) {
			XNET_FREE(pNode);
		}
		return bBound;
	}

	static void __xnetPortIOCPUnbindSocket(__xnet_iocp_ctx* pCtx, SOCKET hSocket)
	{
		__xnet_iocp_bound_socket** ppCur;
		if ( !pCtx || hSocket == XNET_SOCKET_INVALID ) return;
		EnterCriticalSection(&pCtx->tExtLock);
		ppCur = &pCtx->pBoundSockets;
		while ( *ppCur ) {
			__xnet_iocp_bound_socket* pNode = *ppCur;
			if ( pNode->hSocket == hSocket ) {
				*ppCur = pNode->pNext;
				XNET_FREE(pNode);
				break;
			}
			ppCur = &pNode->pNext;
		}
		LeaveCriticalSection(&pCtx->tExtLock);
	}

	static int __xnetPortIOCPGetSocketFamily(SOCKET hSocket)
	{
		struct sockaddr_storage tStorage;
		int iLen = (int)sizeof(tStorage);

		memset(&tStorage, 0, sizeof(tStorage));
		if ( getsockname(hSocket, (struct sockaddr*)&tStorage, &iLen) == 0 ) {
			return ((struct sockaddr*)&tStorage)->sa_family;
		}
		return AF_INET;
	}

	static LPFN_ACCEPTEX __xnetPortIOCPGetAcceptEx(__xnet_iocp_ctx* pCtx, SOCKET hListenSocket)
	{
		DWORD iBytes = 0;
		GUID tGuid = WSAID_ACCEPTEX;

		if ( !pCtx || hListenSocket == XNET_SOCKET_INVALID ) return NULL;
		if ( pCtx->pfnAcceptEx ) return pCtx->pfnAcceptEx;

		EnterCriticalSection(&pCtx->tExtLock);
		if ( pCtx->pfnAcceptEx == NULL ) {
			LPFN_ACCEPTEX pfnAcceptEx = NULL;
			if ( WSAIoctl(hListenSocket,
				SIO_GET_EXTENSION_FUNCTION_POINTER,
				&tGuid,
				sizeof(tGuid),
				&pfnAcceptEx,
				sizeof(pfnAcceptEx),
				&iBytes,
				NULL,
				NULL) == 0 ) {
				pCtx->pfnAcceptEx = pfnAcceptEx;
			}
		}
		LeaveCriticalSection(&pCtx->tExtLock);

		return pCtx->pfnAcceptEx;
	}

	static LPFN_CONNECTEX __xnetPortIOCPGetConnectEx(__xnet_iocp_ctx* pCtx, SOCKET hSocket)
	{
		DWORD iBytes = 0;
		GUID tGuid = WSAID_CONNECTEX;

		if ( !pCtx || hSocket == INVALID_SOCKET ) return NULL;
		if ( pCtx->pfnConnectEx ) return pCtx->pfnConnectEx;

		EnterCriticalSection(&pCtx->tExtLock);
		if ( pCtx->pfnConnectEx == NULL ) {
			LPFN_CONNECTEX pfnConnectEx = NULL;
			if ( WSAIoctl(hSocket,
				SIO_GET_EXTENSION_FUNCTION_POINTER,
				&tGuid,
				sizeof(tGuid),
				&pfnConnectEx,
				sizeof(pfnConnectEx),
				&iBytes,
				NULL,
				NULL) == 0 ) {
				pCtx->pfnConnectEx = pfnConnectEx;
			}
		}
		LeaveCriticalSection(&pCtx->tExtLock);
		return pCtx->pfnConnectEx;
	}

	static bool __xnetPortIOCPBindConnectSocketLocal(SOCKET hSocket, int iFamily)
	{
		if ( hSocket == INVALID_SOCKET ) return false;
		if ( iFamily == AF_INET6 ) {
			struct sockaddr_in6 tAddr6;
			memset(&tAddr6, 0, sizeof(tAddr6));
			tAddr6.sin6_family = AF_INET6;
			return bind(hSocket, (const struct sockaddr*)&tAddr6, sizeof(tAddr6)) == 0;
		}
		if ( iFamily == AF_INET ) {
			struct sockaddr_in tAddr4;
			memset(&tAddr4, 0, sizeof(tAddr4));
			tAddr4.sin_family = AF_INET;
			tAddr4.sin_addr.s_addr = htonl(INADDR_ANY);
			return bind(hSocket, (const struct sockaddr*)&tAddr4, sizeof(tAddr4)) == 0;
		}
		return false;
	}

	static bool __xnetPortIOCPBuildBufsFromSubmit(__xnet_iocp_io* pIo, const xnetportsubmit* pOp)
	{
		if ( !pIo || !pOp ) return false;

		if ( pOp->pVec && pOp->iVecCount > 0 ) {
			if ( pOp->iVecCount > __XNET_IOCP_MAX_IOVEC ) {
				return false;
			}
			for ( uint32 i = 0; i < pOp->iVecCount; ++i ) {
				pIo->arrBuf[i].buf = (CHAR*)pOp->pVec[i].pData;
				pIo->arrBuf[i].len = (ULONG)pOp->pVec[i].iLen;
			}
			pIo->iBufCount = pOp->iVecCount;
			return true;
		}

		if ( pOp->pChain ) {
			xnetspan arrSpan[__XNET_IOCP_MAX_IOVEC];
			uint32 iSpanCount = xrtNetChainGetSpans(pOp->pChain, arrSpan, __XNET_IOCP_MAX_IOVEC);
			if ( iSpanCount == 0 ) {
				return false;
			}
			for ( uint32 i = 0; i < iSpanCount; ++i ) {
				pIo->arrBuf[i].buf = (CHAR*)arrSpan[i].pData;
				pIo->arrBuf[i].len = (ULONG)arrSpan[i].iLen;
			}
			pIo->iBufCount = iSpanCount;
			return true;
		}

		return false;
	}

	static __xnet_iocp_io* __xnetPortIOCPAllocIO(const xnetportsubmit* pOp)
	{
		__xnet_iocp_io* pIo = NULL;

		if ( !pOp ) {
			return NULL;
		}

		pIo = (__xnet_iocp_io*)XNET_ALLOC(sizeof(__xnet_iocp_io));
		if ( !pIo ) {
			return NULL;
		}

		memset(pIo, 0, sizeof(__xnet_iocp_io));
		pIo->iKind = __XNET_IOCP_KIND_IO;
		pIo->iOpType = pOp->iOpType;
		pIo->iFlags = pOp->iFlags;
		pIo->iOpId = pOp->iOpId;
		pIo->hSocket = pOp->hSocket;
		pIo->pUserData = pOp->pUserData;
		pIo->tAddr = pOp->tAddr;
		pIo->iAddrLen = (int)sizeof(pIo->tAddrStorage);
		return pIo;
	}

	static xnet_result __xnetPortIOCPSubmitSynthetic(__xnet_iocp_ctx* pCtx, const xnetportsubmit* pOp)
	{
		__xnet_iocp_post* pPost = NULL;
		uint16 iEventType;

		if ( !pCtx || !pCtx->hIOCP || !pOp ) return XRT_NET_ERROR;
		iEventType = __xnetPortIOCPEventType(pOp->iOpType);
		if ( iEventType == XNET_PORT_EVENT_NONE ) return XRT_NET_ERROR;

		pPost = __xnetPortIOCPAllocPost(iEventType, XNET_PORT_EVENT_F_NONE, XRT_NET_OK,
			__xnetPortIOCPSubmitBytes(pOp), pOp->iOpId);
		if ( !pPost ) return XRT_NET_ERROR;

		pPost->tEvent.iFlags |= pOp->iFlags;
		pPost->tEvent.hSocket = pOp->hSocket;
		pPost->tEvent.pUserData = pOp->pUserData;
		pPost->tEvent.pChain = pOp->pChain;
		pPost->tEvent.tAddr = pOp->tAddr;

		if ( !PostQueuedCompletionStatus(pCtx->hIOCP, pPost->tEvent.iBytes, 0, &pPost->tOverlapped) ) {
			XNET_FREE(pPost);
			return XRT_NET_ERROR;
		}
		return XRT_NET_OK;
	}

	static xnet_result __xnetPortIOCPSubmitAccept(__xnet_iocp_ctx* pCtx, const xnetportsubmit* pOp)
	{
		__xnet_iocp_io* pIo = NULL;
		LPFN_ACCEPTEX pfnAcceptEx = NULL;
		DWORD iBytes = 0;
		int iFamily;
		BOOL bOk;

		if ( !pCtx || !pOp || pOp->hSocket == (intptr_t)XNET_SOCKET_INVALID ) return XRT_NET_ERROR;
		if ( !__xnetPortIOCPBindSocket(pCtx, (SOCKET)pOp->hSocket) ) return XRT_NET_ERROR;

		pfnAcceptEx = __xnetPortIOCPGetAcceptEx(pCtx, (SOCKET)pOp->hSocket);
		if ( !pfnAcceptEx ) return XRT_NET_ERROR;

		pIo = __xnetPortIOCPAllocIO(pOp);
		if ( !pIo ) return XRT_NET_ERROR;

		iFamily = __xnetPortIOCPGetSocketFamily((SOCKET)pOp->hSocket);
		pIo->hAuxSocket = pOp->hSocket;
		pIo->hAcceptSocket = WSASocket(iFamily, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if ( pIo->hAcceptSocket == XNET_SOCKET_INVALID ) {
			XNET_FREE(pIo);
			return XRT_NET_ERROR;
		}

		bOk = pfnAcceptEx((SOCKET)pOp->hSocket,
			pIo->hAcceptSocket,
			pIo->aAcceptBuf,
			0,
			__XNET_IOCP_ACCEPT_ADDR_SPACE,
			__XNET_IOCP_ACCEPT_ADDR_SPACE,
			&iBytes,
			&pIo->tOverlapped);
		if ( !bOk ) {
			int iErr = WSAGetLastError();
			if ( iErr != ERROR_IO_PENDING ) {
				closesocket(pIo->hAcceptSocket);
				XNET_FREE(pIo);
				return XRT_NET_ERROR;
			}
		}

		return XRT_NET_OK;
	}

	static xnet_result __xnetPortIOCPSubmitRecv(__xnet_iocp_ctx* pCtx, const xnetportsubmit* pOp)
	{
		__xnet_iocp_io* pIo = NULL;
		DWORD iBytes = 0;
		DWORD iFlags = 0;
		int iRet;

		if ( !pCtx || !pOp || pOp->hSocket == (intptr_t)XNET_SOCKET_INVALID ) return XRT_NET_ERROR;
		if ( !__xnetPortIOCPBindSocket(pCtx, (SOCKET)pOp->hSocket) ) return XRT_NET_ERROR;

		pIo = __xnetPortIOCPAllocIO(pOp);
		if ( !pIo ) return XRT_NET_ERROR;

		pIo->arrBuf[0].buf = pIo->aRecvBuf;
		pIo->arrBuf[0].len = (ULONG)sizeof(pIo->aRecvBuf);
		pIo->iBufCount = 1;
		pIo->iRecvFlags = 0;

		iRet = WSARecv((SOCKET)pOp->hSocket, pIo->arrBuf, 1, &iBytes, &iFlags, &pIo->tOverlapped, NULL);
		if ( iRet == SOCKET_ERROR ) {
			int iErr = WSAGetLastError();
			if ( iErr != WSA_IO_PENDING ) {
				#if defined(XNET_DEBUG_IOCP_NATIVE)
					fprintf(stderr, "[IOCP] WSARecv submit fail socket=%p err=%d op=%llu\n",
						(void*)(uintptr_t)pOp->hSocket, iErr, (unsigned long long)pOp->iOpId);
				#endif
				XNET_FREE(pIo);
				return XRT_NET_ERROR;
			}
		}

		return XRT_NET_OK;
	}

	static xnet_result __xnetPortIOCPSubmitConnect(__xnet_iocp_ctx* pCtx, const xnetportsubmit* pOp)
	{
		__xnet_iocp_io* pIo = NULL;
		LPFN_CONNECTEX pfnConnectEx = NULL;
		struct sockaddr_storage tStorage;
		socklen_t iAddrLen = 0;
		BOOL bOk;
		int iFamily;

		if ( !pCtx || !pOp || pOp->hSocket == (intptr_t)XNET_SOCKET_INVALID ) return XRT_NET_ERROR;
		if ( !__xnetAddrToSockAddr(&pOp->tAddr, &tStorage, &iAddrLen) ) return XRT_NET_ERROR;
		if ( !__xnetPortIOCPBindSocket(pCtx, (SOCKET)pOp->hSocket) ) return XRT_NET_ERROR;

		iFamily = pOp->tAddr.iFamily ? pOp->tAddr.iFamily : __xnetPortIOCPGetSocketFamily((SOCKET)pOp->hSocket);
		if ( !__xnetPortIOCPBindConnectSocketLocal((SOCKET)pOp->hSocket, iFamily) ) {
			return XRT_NET_ERROR;
		}

		pfnConnectEx = __xnetPortIOCPGetConnectEx(pCtx, (SOCKET)pOp->hSocket);
		if ( !pfnConnectEx ) return XRT_NET_ERROR;

		pIo = __xnetPortIOCPAllocIO(pOp);
		if ( !pIo ) return XRT_NET_ERROR;

		bOk = pfnConnectEx((SOCKET)pOp->hSocket,
			(const struct sockaddr*)&tStorage,
			(int)iAddrLen,
			NULL,
			0,
			NULL,
			&pIo->tOverlapped);
		if ( !bOk ) {
			int iErr = WSAGetLastError();
			if ( iErr != ERROR_IO_PENDING ) {
				XNET_FREE(pIo);
				return XRT_NET_ERROR;
			}
		}

		return XRT_NET_OK;
	}

	static xnet_result __xnetPortIOCPSubmitRecvFrom(__xnet_iocp_ctx* pCtx, const xnetportsubmit* pOp)
	{
		__xnet_iocp_io* pIo = NULL;
		DWORD iBytes = 0;
		DWORD iFlags = 0;
		int iRet;

		if ( !pCtx || !pOp || pOp->hSocket == (intptr_t)XNET_SOCKET_INVALID ) return XRT_NET_ERROR;
		if ( !__xnetPortIOCPBindSocket(pCtx, (SOCKET)pOp->hSocket) ) return XRT_NET_ERROR;

		pIo = __xnetPortIOCPAllocIO(pOp);
		if ( !pIo ) return XRT_NET_ERROR;

		pIo->arrBuf[0].buf = pIo->aRecvBuf;
		pIo->arrBuf[0].len = (ULONG)sizeof(pIo->aRecvBuf);
		pIo->iBufCount = 1;
		pIo->iRecvFlags = 0;

		iRet = WSARecvFrom((SOCKET)pOp->hSocket,
			pIo->arrBuf,
			1,
			&iBytes,
			&iFlags,
			(struct sockaddr*)&pIo->tAddrStorage,
			&pIo->iAddrLen,
			&pIo->tOverlapped,
			NULL);
		if ( iRet == SOCKET_ERROR ) {
			int iErr = WSAGetLastError();
			if ( iErr != WSA_IO_PENDING ) {
				#if defined(XNET_DEBUG_IOCP_NATIVE)
					fprintf(stderr, "[IOCP] WSARecvFrom submit fail socket=%p err=%d op=%llu\n",
						(void*)(uintptr_t)pOp->hSocket, iErr, (unsigned long long)pOp->iOpId);
				#endif
				XNET_FREE(pIo);
				return XRT_NET_ERROR;
			}
		}

		return XRT_NET_OK;
	}

	static xnet_result __xnetPortIOCPSubmitSend(__xnet_iocp_ctx* pCtx, const xnetportsubmit* pOp)
	{
		__xnet_iocp_io* pIo = NULL;
		DWORD iBytes = 0;
		int iRet;

		if ( !pCtx || !pOp || pOp->hSocket == (intptr_t)XNET_SOCKET_INVALID ) return XRT_NET_ERROR;
		if ( !__xnetPortIOCPBindSocket(pCtx, (SOCKET)pOp->hSocket) ) {
			#if defined(XNET_DEBUG_IOCP_NATIVE)
				fprintf(stderr, "[IOCP] bind send socket fail socket=%p err=%lu\n",
					(void*)(uintptr_t)pOp->hSocket, (unsigned long)GetLastError());
			#endif
			return XRT_NET_ERROR;
		}

		pIo = __xnetPortIOCPAllocIO(pOp);
		if ( !pIo ) return XRT_NET_ERROR;
		if ( !__xnetPortIOCPBuildBufsFromSubmit(pIo, pOp) ) {
			#if defined(XNET_DEBUG_IOCP_NATIVE)
				fprintf(stderr, "[IOCP] build send bufs fail socket=%p vec=%u chain=%p\n",
					(void*)(uintptr_t)pOp->hSocket, pOp->iVecCount, (void*)pOp->pChain);
			#endif
			XNET_FREE(pIo);
			return XRT_NET_ERROR;
		}

		iRet = WSASend((SOCKET)pOp->hSocket, pIo->arrBuf, pIo->iBufCount, &iBytes, 0, &pIo->tOverlapped, NULL);
		if ( iRet == SOCKET_ERROR ) {
			int iErr = WSAGetLastError();
			if ( iErr != WSA_IO_PENDING ) {
				#if defined(XNET_DEBUG_IOCP_NATIVE)
					fprintf(stderr, "[IOCP] WSASend submit fail socket=%p err=%d op=%llu bufcount=%u\n",
						(void*)(uintptr_t)pOp->hSocket, iErr, (unsigned long long)pOp->iOpId, pIo->iBufCount);
				#endif
				XNET_FREE(pIo);
				return XRT_NET_ERROR;
			}
		}

		return XRT_NET_OK;
	}

	static xnet_result __xnetPortIOCPSubmitSendTo(__xnet_iocp_ctx* pCtx, const xnetportsubmit* pOp)
	{
		__xnet_iocp_io* pIo = NULL;
		DWORD iBytes = 0;
		int iRet;
		struct sockaddr_storage tStorage;
		socklen_t iAddrLen = 0;

		if ( !pCtx || !pOp || pOp->hSocket == (intptr_t)XNET_SOCKET_INVALID ) return XRT_NET_ERROR;
		if ( !__xnetPortIOCPBindSocket(pCtx, (SOCKET)pOp->hSocket) ) return XRT_NET_ERROR;
		if ( !__xnetAddrToSockAddr(&pOp->tAddr, &tStorage, &iAddrLen) ) return XRT_NET_ERROR;

		pIo = __xnetPortIOCPAllocIO(pOp);
		if ( !pIo ) return XRT_NET_ERROR;
		if ( !__xnetPortIOCPBuildBufsFromSubmit(pIo, pOp) ) {
			XNET_FREE(pIo);
			return XRT_NET_ERROR;
		}

		memcpy(&pIo->tAddrStorage, &tStorage, sizeof(tStorage));
		pIo->iAddrLen = (int)iAddrLen;

		iRet = WSASendTo((SOCKET)pOp->hSocket,
			pIo->arrBuf,
			pIo->iBufCount,
			&iBytes,
			0,
			(const struct sockaddr*)&pIo->tAddrStorage,
			pIo->iAddrLen,
			&pIo->tOverlapped,
			NULL);
		if ( iRet == SOCKET_ERROR ) {
			int iErr = WSAGetLastError();
			if ( iErr != WSA_IO_PENDING ) {
				#if defined(XNET_DEBUG_IOCP_NATIVE)
					fprintf(stderr, "[IOCP] WSASendTo submit fail socket=%p err=%d op=%llu bufcount=%u\n",
						(void*)(uintptr_t)pOp->hSocket, iErr, (unsigned long long)pOp->iOpId, pIo->iBufCount);
				#endif
				XNET_FREE(pIo);
				return XRT_NET_ERROR;
			}
		}

		return XRT_NET_OK;
	}

	static void __xnetPortIOCPFreeTimers(__xnet_iocp_ctx* pCtx)
	{
		while ( pCtx && pCtx->pTimers ) {
			__xnet_iocp_timer* pNext = pCtx->pTimers->pNext;
			XNET_FREE(pCtx->pTimers);
			pCtx->pTimers = pNext;
		}
	}

	static void __xnetPortIOCPFreeBoundSockets(__xnet_iocp_ctx* pCtx)
	{
		while ( pCtx && pCtx->pBoundSockets ) {
			__xnet_iocp_bound_socket* pNext = pCtx->pBoundSockets->pNext;
			XNET_FREE(pCtx->pBoundSockets);
			pCtx->pBoundSockets = pNext;
		}
	}

	static uint32 __xnetPortIOCPHarvestTimers(__xnet_iocp_ctx* pCtx, xnetportevent* pEvents, uint32 iMaxEvents)
	{
		uint32 iCount = 0;
		uint64 iNowMs = __xnetPortIOCPNowMs();
		__xnet_iocp_timer** ppCur = pCtx ? &pCtx->pTimers : NULL;
		while ( ppCur && *ppCur && iCount < iMaxEvents ) {
			__xnet_iocp_timer* pNode = *ppCur;
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

	static bool __xnetPortIOCPBuildIoEvent(__xnet_iocp_io* pIo, BOOL bOk, DWORD iBytes, xnetportevent* pEvent)
	{
		if ( !pIo || !pEvent ) return false;

		memset(pEvent, 0, sizeof(xnetportevent));
		pEvent->iType = __xnetPortIOCPEventType(pIo->iOpType);
		pEvent->iStatus = bOk ? XRT_NET_OK : XRT_NET_ERROR;
		pEvent->iOpId = pIo->iOpId;
		pEvent->hSocket = pIo->hSocket;
		pEvent->pUserData = pIo->pUserData;
		pEvent->iBytes = (uint32)iBytes;
		pEvent->tAddr = pIo->tAddr;

		if ( pIo->iOpType == XNET_PORT_OP_ACCEPT ) {
			if ( bOk ) {
				SOCKET hListenSocket = (SOCKET)pIo->hAuxSocket;
				(void)setsockopt(pIo->hAcceptSocket,
					SOL_SOCKET,
					SO_UPDATE_ACCEPT_CONTEXT,
					(const char*)&hListenSocket,
					sizeof(hListenSocket));
				pEvent->hSocket = (intptr_t)pIo->hAcceptSocket;
			} else if ( pIo->hAcceptSocket != XNET_SOCKET_INVALID ) {
				closesocket(pIo->hAcceptSocket);
			}
			return true;
		}

		if ( pIo->iOpType == XNET_PORT_OP_CONNECT ) {
			if ( bOk ) {
				SOCKET hSocket = (SOCKET)pIo->hSocket;
				(void)setsockopt(hSocket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
			}
			return true;
		}

		if ( pIo->iOpType == XNET_PORT_OP_RECV ) {
			if ( bOk && iBytes > 0 ) {
				pEvent->pChain = __xnetPortIOCPAllocEventChain(pIo->aRecvBuf, (size_t)iBytes);
				if ( pEvent->pChain == NULL ) {
					pEvent->iStatus = XRT_NET_ERROR;
					pEvent->iBytes = 0;
				}
			} else if ( bOk ) {
				pEvent->iStatus = XRT_NET_CLOSED;
				pEvent->iFlags |= XNET_PORT_EVENT_F_EOF;
			}
			return true;
		}

		if ( pIo->iOpType == XNET_PORT_OP_RECVFROM ) {
			if ( bOk ) {
				pEvent->pChain = __xnetPortIOCPAllocEventChain(pIo->aRecvBuf, (size_t)iBytes);
				if ( pEvent->pChain == NULL ) {
					pEvent->iStatus = XRT_NET_ERROR;
					pEvent->iBytes = 0;
				}
				(void)__xnetAddrFromSockAddr(&pEvent->tAddr, (const struct sockaddr*)&pIo->tAddrStorage);
			}
			return true;
		}

		return true;
	}

	static uint32 __xnetPortIOCPHarvestPosted(__xnet_iocp_ctx* pCtx, xnetportevent* pEvents, uint32 iMaxEvents, uint32 iWaitMs)
	{
		uint32 iCount = 0;

		if ( !pCtx || !pEvents || iMaxEvents == 0 ) return 0;

		for ( ;; ) {
			DWORD iBytes = 0;
			ULONG_PTR iKey = 0;
			LPOVERLAPPED pOv = NULL;
			DWORD iWait = (iCount > 0) ? 0u : iWaitMs;
			BOOL bOk = GetQueuedCompletionStatus(pCtx->hIOCP, &iBytes, &iKey, &pOv, iWait);
			__xnet_iocp_header* pHdr;

			(void)iKey;
			if ( !bOk && pOv == NULL ) break;
			if ( pOv == NULL ) break;

			pHdr = (__xnet_iocp_header*)pOv;
			if ( pHdr->iKind == __XNET_IOCP_KIND_POST ) {
				__xnet_iocp_post* pPost = (__xnet_iocp_post*)pOv;
				if ( !bOk && pPost->tEvent.iStatus == XRT_NET_OK ) {
					pPost->tEvent.iStatus = XRT_NET_ERROR;
				}
				pPost->tEvent.iBytes = iBytes;
				pEvents[iCount++] = pPost->tEvent;
				XNET_FREE(pPost);
			} else if ( pHdr->iKind == __XNET_IOCP_KIND_IO ) {
				__xnet_iocp_io* pIo = (__xnet_iocp_io*)pOv;
				if ( __xnetPortIOCPBuildIoEvent(pIo, bOk, iBytes, &pEvents[iCount]) ) {
					iCount++;
				}
				XNET_FREE(pIo);
			}

			if ( iCount >= iMaxEvents ) break;
		}

		return iCount;
	}

	static void __xnetPortIOCPDrainPosted(__xnet_iocp_ctx* pCtx)
	{
		xnetportevent arrEvents[32];
		uint32 iCount;
		memset(arrEvents, 0, sizeof(arrEvents));
		while ( (iCount = __xnetPortIOCPHarvestPosted(pCtx, arrEvents, 32, 0)) > 0 ) {
			for ( uint32 i = 0; i < iCount; ++i ) {
				if ( arrEvents[i].pChain ) {
					__xnetPortIOCPFreeEventChain(arrEvents[i].pChain);
				}
			}
			memset(arrEvents, 0, sizeof(arrEvents));
		}
	}

	static xnet_result __xnetPortIOCPInit(xnetport* pPort, const xnetportconfig* pCfg, ptr pOwner)
	{
		(void)pOwner;
		if ( !pPort || !pCfg ) return XRT_NET_ERROR;

		__xnet_iocp_ctx* pCtx = (__xnet_iocp_ctx*)XNET_ALLOC(sizeof(__xnet_iocp_ctx));
		if ( !pCtx ) return XRT_NET_ERROR;

		memset(pCtx, 0, sizeof(__xnet_iocp_ctx));
		InitializeCriticalSection(&pCtx->tExtLock);
		pCtx->hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
		if ( !pCtx->hIOCP ) {
			DeleteCriticalSection(&pCtx->tExtLock);
			XNET_FREE(pCtx);
			return XRT_NET_ERROR;
		}

		pPort->pCtx = pCtx;
		return XRT_NET_OK;
	}

	static void __xnetPortIOCPUnit(xnetport* pPort)
	{
		__xnet_iocp_ctx* pCtx = pPort ? (__xnet_iocp_ctx*)pPort->pCtx : NULL;
		if ( !pCtx ) return;

		__xnetPortIOCPDrainPosted(pCtx);
		__xnetPortIOCPFreeTimers(pCtx);
		__xnetPortIOCPFreeBoundSockets(pCtx);
		if ( pCtx->hIOCP ) {
			CloseHandle(pCtx->hIOCP);
		}
		DeleteCriticalSection(&pCtx->tExtLock);
		XNET_FREE(pCtx);
		if ( pPort ) pPort->pCtx = NULL;
	}

	static xnet_result __xnetPortIOCPSumbit(xnetport* pPort, const xnetportsubmit* pOps, uint32 iCount)
	{
		__xnet_iocp_ctx* pCtx = pPort ? (__xnet_iocp_ctx*)pPort->pCtx : NULL;

		if ( !pCtx || !pCtx->hIOCP || !pOps || iCount == 0 ) return XRT_NET_ERROR;

		for ( uint32 i = 0; i < iCount; ++i ) {
			const xnetportsubmit* pOp = &pOps[i];

			if ( !__xnetPortIOCPValidOp(pOp->iOpType) ) {
				return XRT_NET_ERROR;
			}

			switch ( pOp->iOpType ) {
				case XNET_PORT_OP_ACCEPT:
					if ( __xnetPortIOCPHasNativeSocket(pOp) &&
						(pOp->iFlags & XNET_PORT_EVENT_F_ACCEPTED_OPEN) == 0 &&
						pOp->iOpId != 0 ) {
						if ( __xnetPortIOCPSubmitAccept(pCtx, pOp) != XRT_NET_OK ) return XRT_NET_ERROR;
						break;
					}
					if ( __xnetPortIOCPSubmitSynthetic(pCtx, pOp) != XRT_NET_OK ) return XRT_NET_ERROR;
					break;

				case XNET_PORT_OP_RECV:
					if ( __xnetPortIOCPHasNativeSocket(pOp) && pOp->iOpId != 0 &&
						pOp->pChain == NULL && !(pOp->pVec && pOp->iVecCount > 0) ) {
						if ( __xnetPortIOCPSubmitRecv(pCtx, pOp) != XRT_NET_OK ) return XRT_NET_ERROR;
						break;
					}
					if ( __xnetPortIOCPSubmitSynthetic(pCtx, pOp) != XRT_NET_OK ) return XRT_NET_ERROR;
					break;

				case XNET_PORT_OP_RECVFROM:
					if ( __xnetPortIOCPHasNativeSocket(pOp) && pOp->iOpId != 0 &&
						pOp->pChain == NULL && !(pOp->pVec && pOp->iVecCount > 0) ) {
						if ( __xnetPortIOCPSubmitRecvFrom(pCtx, pOp) != XRT_NET_OK ) return XRT_NET_ERROR;
						break;
					}
					if ( __xnetPortIOCPSubmitSynthetic(pCtx, pOp) != XRT_NET_OK ) return XRT_NET_ERROR;
					break;

				case XNET_PORT_OP_SEND:
					if ( __xnetPortIOCPHasNativeSocket(pOp) &&
						((pOp->pVec && pOp->iVecCount > 0) || pOp->pChain != NULL) ) {
						if ( __xnetPortIOCPSubmitSend(pCtx, pOp) != XRT_NET_OK ) return XRT_NET_ERROR;
						break;
					}
					if ( __xnetPortIOCPSubmitSynthetic(pCtx, pOp) != XRT_NET_OK ) return XRT_NET_ERROR;
					break;

				case XNET_PORT_OP_SENDTO:
					if ( __xnetPortIOCPHasNativeSocket(pOp) &&
						((pOp->pVec && pOp->iVecCount > 0) || pOp->pChain != NULL) ) {
						if ( __xnetPortIOCPSubmitSendTo(pCtx, pOp) != XRT_NET_OK ) return XRT_NET_ERROR;
						break;
					}
					if ( __xnetPortIOCPSubmitSynthetic(pCtx, pOp) != XRT_NET_OK ) return XRT_NET_ERROR;
					break;

				case XNET_PORT_OP_CONNECT:
					if ( __xnetPortIOCPHasNativeSocket(pOp) && pOp->tAddr.iFamily != 0 ) {
						if ( __xnetPortIOCPSubmitConnect(pCtx, pOp) != XRT_NET_OK ) return XRT_NET_ERROR;
						break;
					}
					if ( __xnetPortIOCPSubmitSynthetic(pCtx, pOp) != XRT_NET_OK ) return XRT_NET_ERROR;
					break;

				case XNET_PORT_OP_WAKE:
					if ( __xnetPortIOCPSubmitSynthetic(pCtx, pOp) != XRT_NET_OK ) return XRT_NET_ERROR;
					break;

				case XNET_PORT_OP_CLOSE:
					if ( __xnetPortIOCPHasNativeSocket(pOp) ) {
						__xnetPortIOCPUnbindSocket(pCtx, (SOCKET)pOp->hSocket);
					}
					break;

				case XNET_PORT_OP_TIMER:
				default:
					return XRT_NET_ERROR;
			}
		}

		return XRT_NET_OK;
	}

	static uint32 __xnetPortIOCPHarvest(xnetport* pPort, xnetportevent* pEvents, uint32 iMaxEvents, uint32 iTimeoutMs)
	{
		uint32 iCount = 0;
		__xnet_iocp_ctx* pCtx = pPort ? (__xnet_iocp_ctx*)pPort->pCtx : NULL;

		if ( !pCtx || !pEvents || iMaxEvents == 0 ) return 0;

		iCount += __xnetPortIOCPHarvestTimers(pCtx, pEvents + iCount, iMaxEvents - iCount);
		if ( iCount >= iMaxEvents ) return iCount;

		iCount += __xnetPortIOCPHarvestPosted(pCtx, pEvents + iCount, iMaxEvents - iCount, iCount > 0 ? 0u : iTimeoutMs);
		return iCount;
	}

	static xnet_result __xnetPortIOCPWake(xnetport* pPort)
	{
		__xnet_iocp_ctx* pCtx = pPort ? (__xnet_iocp_ctx*)pPort->pCtx : NULL;
		__xnet_iocp_post* pPost;

		if ( !pCtx || !pCtx->hIOCP ) return XRT_NET_ERROR;
		pPost = __xnetPortIOCPAllocPost(XNET_PORT_EVENT_WAKE, XNET_PORT_EVENT_F_NONE, XRT_NET_OK, 0, 0);
		if ( !pPost ) return XRT_NET_ERROR;
		if ( !PostQueuedCompletionStatus(pCtx->hIOCP, 0, 0, &pPost->tOverlapped) ) {
			XNET_FREE(pPost);
			return XRT_NET_ERROR;
		}
		return XRT_NET_OK;
	}

	static xnet_result __xnetPortIOCPArmTimer(xnetport* pPort, uint64 iTimerId, uint32 iDelayMs)
	{
		__xnet_iocp_ctx* pCtx = pPort ? (__xnet_iocp_ctx*)pPort->pCtx : NULL;
		__xnet_iocp_timer* pNode;

		if ( !pCtx ) return XRT_NET_ERROR;
		pNode = (__xnet_iocp_timer*)XNET_ALLOC(sizeof(__xnet_iocp_timer));
		if ( !pNode ) return XRT_NET_ERROR;
		memset(pNode, 0, sizeof(__xnet_iocp_timer));
		pNode->iTimerId = iTimerId;
		pNode->iDueMs = __xnetPortIOCPNowMs() + (uint64)iDelayMs;
		pNode->pNext = pCtx->pTimers;
		pCtx->pTimers = pNode;
		return XRT_NET_OK;
	}

	static xnet_result __xnetPortIOCPCancelTimer(xnetport* pPort, uint64 iTimerId)
	{
		__xnet_iocp_ctx* pCtx = pPort ? (__xnet_iocp_ctx*)pPort->pCtx : NULL;
		__xnet_iocp_timer** ppCur = pCtx ? &pCtx->pTimers : NULL;

		if ( !ppCur ) return XRT_NET_ERROR;
		while ( *ppCur ) {
			__xnet_iocp_timer* pNode = *ppCur;
			if ( pNode->iTimerId == iTimerId ) {
				*ppCur = pNode->pNext;
				XNET_FREE(pNode);
				return XRT_NET_OK;
			}
			ppCur = &pNode->pNext;
		}
		return XRT_NET_ERROR;
	}

	static const xnetportops __g_xnetPortIOCPOps = {
		"xnet-port-iocp",
		__xnetPortIOCPInit,
		__xnetPortIOCPUnit,
		__xnetPortIOCPSumbit,
		__xnetPortIOCPHarvest,
		__xnetPortIOCPWake,
		__xnetPortIOCPArmTimer,
		__xnetPortIOCPCancelTimer
	};

	static const xnetportops* xrtNetPortIOCPOps(void)
	{
		return &__g_xnetPortIOCPOps;
	}

#else

	static const xnetportops* xrtNetPortIOCPOps(void)
	{
		return NULL;
	}

#endif


#endif
