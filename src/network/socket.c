#if defined(__linux__) && !defined(_GNU_SOURCE) && \
	!defined(XRT_SINGLE_HEADER)
	#define _GNU_SOURCE 1
#endif

#include "../internal/xrt_net_socket.h"

#include <errno.h>
#include <limits.h>

#if !defined(_WIN32) && !defined(_WIN64)
	#include <fcntl.h>
	#include <netinet/tcp.h>
	#include <sys/ioctl.h>
	#include <sys/uio.h>
	#include <unistd.h>
	#if defined(__linux__)
		#include <linux/errqueue.h>
		#include <sys/syscall.h>
		#ifndef SOL_UDP
			#define SOL_UDP 17
		#endif
		#ifndef UDP_SEGMENT
			#define UDP_SEGMENT 103
		#endif
		#ifndef UDP_GRO
			#define UDP_GRO 104
		#endif
		/* Linux 的 MSG_ERRQUEUE 可能是枚举标识，不能用宏 fallback 覆盖。 */
		#define XRT_NET_MSG_ERRQUEUE 0x2000
		/* 严格 C 单头模式可能隐藏 libc 的可变参数 syscall 声明。 */
		extern long syscall(long iNumber, ...);
	#endif
	#if defined(IP_RECVIF) && (defined(__APPLE__) || defined(__FreeBSD__) || \
		defined(__NetBSD__) || defined(__OpenBSD__))
		#include <net/if_dl.h>
	#endif
#endif

#if defined(_WIN32) || defined(_WIN64)
	#ifndef WSA_FLAG_NO_HANDLE_INHERIT
		#define WSA_FLAG_NO_HANDLE_INHERIT 0x80
	#endif
	#ifndef IPV6_UNICAST_HOPS
		#define IPV6_UNICAST_HOPS 4
	#endif
	#ifndef IPV6_V6ONLY
		#define IPV6_V6ONLY 27
	#endif
	#ifndef IPV6_TCLASS
		#define IPV6_TCLASS 39
	#endif
	#ifndef UDP_SEND_MSG_SIZE
		#define UDP_SEND_MSG_SIZE 2
	#endif
	#ifndef UDP_RECV_MAX_COALESCED_SIZE
		#define UDP_RECV_MAX_COALESCED_SIZE 3
	#endif
	#ifndef UDP_COALESCED_INFO
		#define UDP_COALESCED_INFO 3
	#endif
#endif



#if defined(XRT_FEATURE_NET_SOCKET)

#if defined(__linux__) && \
	(!defined(XRT_SINGLE_HEADER) || defined(_GNU_SOURCE))
	#define XRT_NET_SOCKET_NATIVE_DGRAM_BATCH 1
	#define XRT_NET_SOCKET_RECV_BATCH_STACK 16u
#endif

#if defined(_WIN32) || defined(_WIN64)
	typedef SOCKET __xrt_netsocket_native;
	#define __XRT_NET_SOCKET_INVALID INVALID_SOCKET
#else
typedef int __xrt_netsocket_native;
	#define __XRT_NET_SOCKET_INVALID (-1)
#endif



/* 控制消息缓冲按整数边界对齐，避免把 char 数组直接解释为平台头部。 */
typedef union __xrt_net_dgram_control {
	uint64 Align;
	unsigned char Data[XRT_NET_SOCKET_DGRAM_CONTROL_SIZE];
} __xrt_net_dgram_control;



/* 返回对象中的平台句柄。 */
static __xrt_netsocket_native __xrtNetSocketHandle(xnetsocket Socket)
{
	return (__xrt_netsocket_native)Socket->Native;
}



/* 判断对象及其平台句柄是否仍然有效。 */
static bool __xrtNetSocketValid(xnetsocket Socket)
{
	return (Socket != NULL) &&
		(__xrtNetSocketHandle(Socket) != __XRT_NET_SOCKET_INVALID);
}



/* 读取当前线程的 Socket 错误代码。 */
static int __xrtNetSocketLastError(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return WSAGetLastError();
	#else
		return errno;
	#endif
}



/* 判断一次非阻塞 Socket 操作是否需要等待后重试。 */
static bool __xrtNetSocketWouldBlock(int iCode)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (iCode == WSAEWOULDBLOCK) || (iCode == WSAEINPROGRESS) ||
			(iCode == WSAEALREADY);
	#else
		return (iCode == EAGAIN) || (iCode == EWOULDBLOCK) ||
			(iCode == EINPROGRESS) || (iCode == EALREADY);
	#endif
}



/* 判断非阻塞连接是否已经由其他路径完成。 */
static bool __xrtNetSocketIsConnected(int iCode)
{
	#if defined(_WIN32) || defined(_WIN64)
		return iCode == WSAEISCONN;
	#else
		return iCode == EISCONN;
	#endif
}



/* 把 Winsock 错误补充映射到 XRT 稳定错误类别。 */
xerrkind __xrtNetSocketErrorKind(int iCode)
{
	#if defined(_WIN32) || defined(_WIN64)
		if ( iCode == WSAEACCES ) {
			return XERR_PERMISSION;
		}
		if ( iCode == WSAEADDRINUSE ) {
			return XERR_EXISTS;
		}
		if ( (iCode == WSAEINVAL) || (iCode == WSAEAFNOSUPPORT) ||
			 (iCode == WSAEPROTOTYPE) ) {
			return XERR_ARGUMENT;
		}
		if ( (iCode == WSAENOBUFS) || (iCode == WSA_NOT_ENOUGH_MEMORY) ) {
			return XERR_MEMORY;
		}
		if ( iCode == WSAETIMEDOUT ) {
			return XERR_TIMEOUT;
		}
		if ( (iCode == WSAEOPNOTSUPP) || (iCode == WSAEPROTONOSUPPORT) ) {
			return XERR_UNSUPPORTED;
		}
		if ( __xrtNetSocketWouldBlock(iCode) ) {
			return XERR_AGAIN;
		}
		return XERR_IO;
	#else
		return __xrtSystemErrorKind(iCode);
	#endif
}



/* 设置一个保留平台错误代码的结构化 Socket 错误。 */
void __xrtNetSocketSetSystemError(xneterror Code,
	cstr sOperation, cstr sMessage, int iSystemCode)
{
	__xrtNetSetError(__xrtNetSocketErrorKind(iSystemCode), Code,
		sOperation, sMessage, iSystemCode);
}



/* 设置不依赖平台错误代码的 Socket 契约错误。 */
static void __xrtNetSocketSetError(xerrkind Kind, xneterror Code,
	cstr sOperation, cstr sMessage)
{
	__xrtNetSetError(Kind, Code, sOperation, sMessage, 0);
}



/* 检查对象并保留具体操作名。 */
static bool __xrtNetSocketRequire(xnetsocket Socket,
	xneterror Code, cstr sOperation)
{
	if ( __xrtNetSocketValid(Socket) ) {
		return true;
	}

	__xrtNetSocketSetError(XERR_ARGUMENT, Code,
		sOperation, "invalid socket object");
	return false;
}



/* 关闭一个尚未包装或正在回滚的平台句柄。 */
static bool __xrtNetSocketCloseNative(__xrt_netsocket_native hSocket)
{
	#if defined(_WIN32) || defined(_WIN64)
		return closesocket(hSocket) == 0;
	#else
		return close(hSocket) == 0;
	#endif
}



/* 禁止 Socket 句柄泄漏到后续创建的子进程。 */
static bool __xrtNetSocketNoInherit(__xrt_netsocket_native hSocket)
{
	#if defined(_WIN32) || defined(_WIN64)
		return SetHandleInformation((HANDLE)hSocket,
			HANDLE_FLAG_INHERIT, 0) != 0;
	#else
		int iFlags = fcntl(hSocket, F_GETFD, 0);

		if ( iFlags < 0 ) {
			return false;
		}
		return fcntl(hSocket, F_SETFD, iFlags | FD_CLOEXEC) == 0;
	#endif
}



/* 切换平台句柄的阻塞模式。 */
static bool __xrtNetSocketSetNonblockNative(
	__xrt_netsocket_native hSocket, bool bEnabled)
{
	#if defined(_WIN32) || defined(_WIN64)
		u_long iMode = bEnabled ? 1u : 0u;

		return ioctlsocket(hSocket, FIONBIO, &iMode) == 0;
	#else
		int iFlags = fcntl(hSocket, F_GETFL, 0);

		if ( iFlags < 0 ) {
			return false;
		}
		if ( bEnabled ) {
			iFlags |= O_NONBLOCK;
		} else {
			iFlags &= ~O_NONBLOCK;
		}
		return fcntl(hSocket, F_SETFL, iFlags) == 0;
	#endif
}



/* 创建时原子设置不可继承和非阻塞属性；旧系统不支持时退回二次设置。 */
static __xrt_netsocket_native __xrtNetSocketOpenNative(
	int iFamily, int iType, int iProtocol, uint32 iFlags)
{
	__xrt_netsocket_native hSocket;

	#if defined(_WIN32) || defined(_WIN64)
		DWORD iNativeFlags = WSA_FLAG_OVERLAPPED |
			WSA_FLAG_NO_HANDLE_INHERIT;

		(void)iFlags;
		hSocket = WSASocket(iFamily, iType, iProtocol,
			NULL, 0, iNativeFlags);
		if ( (hSocket == __XRT_NET_SOCKET_INVALID) &&
			 (WSAGetLastError() == WSAEINVAL) ) {
			hSocket = WSASocket(iFamily, iType, iProtocol,
				NULL, 0, WSA_FLAG_OVERLAPPED);
		}
	#else
		int iNativeType = iType;

		#if defined(SOCK_CLOEXEC)
			iNativeType |= SOCK_CLOEXEC;
		#endif
		#if defined(SOCK_NONBLOCK)
			if ( (iFlags & XNET_SOCKET_NONBLOCK) != 0 ) {
				iNativeType |= SOCK_NONBLOCK;
			}
		#endif

		hSocket = socket(iFamily, iNativeType, iProtocol);
		if ( (hSocket == __XRT_NET_SOCKET_INVALID) &&
			 (iNativeType != iType) && (errno == EINVAL) ) {
			hSocket = socket(iFamily, iType, iProtocol);
		}
	#endif

	return hSocket;
}



/* 包装一个新句柄；失败时关闭句柄，不留下半初始化对象。 */
xnetsocket __xrtNetSocketAdopt(uintptr_t iNative,
	xnetfamily Family, xnetsockettype Type, uint32 iFlags)
{
	__xrt_netsocket_native hSocket = (__xrt_netsocket_native)iNative;
	xnetsocket Socket;

	Socket = (xnetsocket)xrtMalloc(sizeof(*Socket));
	if ( Socket == NULL ) {
		(void)__xrtNetSocketCloseNative(hSocket);
		return NULL;
	}

	memset(Socket, 0, sizeof(*Socket));
	Socket->Native = (uintptr_t)hSocket;
	Socket->Family = Family;
	Socket->Type = Type;
	Socket->Flags = iFlags;

	if ( !__xrtNetSocketNoInherit(hSocket) ) {
		#if defined(_WIN32) || defined(_WIN64)
			int iCode = (int)GetLastError();
		#else
			int iCode = __xrtNetSocketLastError();
		#endif

		(void)__xrtNetSocketCloseNative(hSocket);
		Socket->Native = (uintptr_t)__XRT_NET_SOCKET_INVALID;
		xrtFree(Socket);
		__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_OPEN,
			"open", "disabling socket inheritance failed", iCode);
		return NULL;
	}
	if ( ((iFlags & XNET_SOCKET_NONBLOCK) != 0) &&
		 !__xrtNetSocketSetNonblockNative(hSocket, true) ) {
		int iCode = __xrtNetSocketLastError();

		(void)__xrtNetSocketCloseNative(hSocket);
		Socket->Native = (uintptr_t)__XRT_NET_SOCKET_INVALID;
		xrtFree(Socket);
		__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_OPEN,
			"open", "setting initial nonblocking mode failed", iCode);
		return NULL;
	}

	#if defined(SO_NOSIGPIPE)
		{
			int iEnabled = 1;

			(void)setsockopt(hSocket, SOL_SOCKET, SO_NOSIGPIPE,
				(const char*)&iEnabled, (socklen_t)sizeof(iEnabled));
		}
	#endif

	return Socket;
}



/* 校验地址与 Socket 地址族一致，并构造平台地址。 */
static bool __xrtNetSocketAddress(xnetsocket Socket,
	const xnetaddr* pAddress, void* pNative, size_t* pSize,
	xneterror Code, cstr sOperation)
{
	if ( (pAddress == NULL) || (pAddress->Family != Socket->Family) ) {
		__xrtNetSocketSetError(XERR_ARGUMENT, Code,
			sOperation, "socket address family mismatch");
		return false;
	}
	return xrtNetAddrToNative(pAddress, pNative, pSize);
}



/* 校验单次 IO 的缓冲区和平台长度范围。 */
static bool __xrtNetSocketBuffer(const void* pData, size_t iSize,
	xneterror Code, cstr sOperation)
{
	if ( ((pData == NULL) && (iSize != 0)) || (iSize > (size_t)INT_MAX) ) {
		__xrtNetSocketSetError(XERR_ARGUMENT, Code,
			sOperation, "invalid socket buffer or size");
		return false;
	}
	return true;
}



/* 设置一个 int 类型的平台 Socket 选项。 */
static bool __xrtNetSocketSetInt(xnetsocket Socket,
	int iLevel, int iName, int64 iValue)
{
	int iOption;

	if ( (iValue < INT_MIN) || (iValue > INT_MAX) ) {
		__xrtNetSocketSetError(XERR_RANGE, XNET_ERROR_SOCKET_OPTION,
			"set-option", "socket option value is out of range");
		return false;
	}
	iOption = (int)iValue;
	if ( setsockopt(__xrtNetSocketHandle(Socket), iLevel, iName,
		(const char*)&iOption, (socklen_t)sizeof(iOption)) != 0 ) {
		int iCode = __xrtNetSocketLastError();

		__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_OPTION,
			"set-option", "setting socket option failed", iCode);
		return false;
	}
	return true;
}



/* 查询一个 int 类型的平台 Socket 选项。 */
static bool __xrtNetSocketGetInt(xnetsocket Socket,
	int iLevel, int iName, int64* pValue)
{
	int iOption = 0;

	#if defined(_WIN32) || defined(_WIN64)
		int iSize = (int)sizeof(iOption);
	#else
		socklen_t iSize = (socklen_t)sizeof(iOption);
	#endif

	if ( getsockopt(__xrtNetSocketHandle(Socket), iLevel, iName,
		(char*)&iOption, &iSize) != 0 ) {
		int iCode = __xrtNetSocketLastError();

		__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_OPTION,
			"get-option", "querying socket option failed", iCode);
		return false;
	}
	*pValue = (int64)iOption;
	return true;
}



/* 报告当前平台没有对应的稳定选项语义。 */
static bool __xrtNetSocketUnsupportedOption(cstr sOperation)
{
	__xrtNetSocketSetError(XERR_UNSUPPORTED, XNET_ERROR_SOCKET_OPTION,
		sOperation, "socket option is not supported on this platform");
	return false;
}



/* 校验仅适用于流式或数据报 Socket 的选项。 */
static bool __xrtNetSocketRequireType(xnetsocket Socket,
	xnetsockettype Type, cstr sOperation, cstr sMessage)
{
	if ( Socket->Type == Type ) {
		return true;
	}
	__xrtNetSocketSetError(XERR_ARGUMENT, XNET_ERROR_SOCKET_OPTION,
		sOperation, sMessage);
	return false;
}



/* 返回当前地址族的路径 MTU 模式和查询选项。 */
static bool __xrtNetSocketMtuOption(
	xnetsocket Socket,
	bool bQuery,
	int* pLevel,
	int* pName
)
{
	#if defined(_WIN32) || defined(_WIN64)
		*pLevel = (Socket->Family == XNET_FAMILY_IPV6) ?
			IPPROTO_IPV6 : IPPROTO_IP;
		if ( bQuery ) {
			*pName = (Socket->Family == XNET_FAMILY_IPV6) ? 72 : 73;
		} else {
			*pName = 71;
		}
		return true;
	#elif defined(__linux__)
		*pLevel = (Socket->Family == XNET_FAMILY_IPV6) ?
			IPPROTO_IPV6 : IPPROTO_IP;
		if ( bQuery ) {
			*pName = (Socket->Family == XNET_FAMILY_IPV6) ?
				IPV6_MTU : IP_MTU;
		} else {
			*pName = (Socket->Family == XNET_FAMILY_IPV6) ?
				IPV6_MTU_DISCOVER : IP_MTU_DISCOVER;
		}
		return true;
	#else
		(void)Socket;
		(void)bQuery;
		(void)pLevel;
		(void)pName;
		return __xrtNetSocketUnsupportedOption("path-mtu");
	#endif
}



/* 把稳定 PMTU 模式转换为平台选项值。 */
static bool __xrtNetSocketMtuModeNative(
	xnetpmtumode Mode,
	int* pValue
)
{
	#if defined(_WIN32) || defined(_WIN64)
		static const int aModes[] = { 0, 1, 2, 3 };
	#elif defined(__linux__)
		static const int aModes[] = {
			IP_PMTUDISC_WANT,
			IP_PMTUDISC_DO,
			IP_PMTUDISC_DONT,
			IP_PMTUDISC_PROBE
		};
	#else
		(void)Mode;
		(void)pValue;
		return __xrtNetSocketUnsupportedOption("path-mtu-mode");
	#endif

	#if defined(_WIN32) || defined(_WIN64) || defined(__linux__)
		if ( (Mode < XNET_PMTU_SYSTEM) || (Mode > XNET_PMTU_PROBE) ) {
			__xrtNetSocketSetError(
				XERR_ARGUMENT,
				XNET_ERROR_SOCKET_OPTION,
				"set-option",
				"invalid path MTU mode"
			);
			return false;
		}
		*pValue = aModes[(size_t)Mode];
		return true;
	#endif
}



/* 把平台 PMTU 模式还原为稳定值。 */
static bool __xrtNetSocketMtuModeStable(
	int iValue,
	xnetpmtumode* pMode
)
{
	#if defined(_WIN32) || defined(_WIN64)
		if ( (iValue >= 0) && (iValue <= 3) ) {
			*pMode = (xnetpmtumode)iValue;
			return true;
		}
	#elif defined(__linux__)
		if ( iValue == IP_PMTUDISC_WANT ) {
			*pMode = XNET_PMTU_SYSTEM;
			return true;
		}
		if ( iValue == IP_PMTUDISC_DO ) {
			*pMode = XNET_PMTU_DISCOVER;
			return true;
		}
		if ( iValue == IP_PMTUDISC_DONT ) {
			*pMode = XNET_PMTU_FRAGMENT;
			return true;
		}
		if ( iValue == IP_PMTUDISC_PROBE ) {
			*pMode = XNET_PMTU_PROBE;
			return true;
		}
	#else
		(void)iValue;
		(void)pMode;
		return __xrtNetSocketUnsupportedOption("path-mtu-mode");
	#endif

	#if defined(_WIN32) || defined(_WIN64) || defined(__linux__)
		__xrtNetSocketSetError(
			XERR_VALUE,
			XNET_ERROR_SOCKET_OPTION,
			"get-option",
			"platform returned an unknown path MTU mode"
		);
		return false;
	#endif
}



/* 返回 Linux 数据报扩展错误队列选项。 */
static bool __xrtNetSocketDgramErrorOption(
	xnetsocket Socket,
	int* pLevel,
	int* pName
)
{
	#if defined(__linux__)
		*pLevel = (Socket->Family == XNET_FAMILY_IPV6) ?
			IPPROTO_IPV6 : IPPROTO_IP;
		*pName = (Socket->Family == XNET_FAMILY_IPV6) ?
			IPV6_RECVERR : IP_RECVERR;
		return true;
	#else
		(void)Socket;
		(void)pLevel;
		(void)pName;
		return __xrtNetSocketUnsupportedOption("datagram-errors");
	#endif
}



/* 打开一个流式或数据报 Socket；成功返回的对象拥有原生句柄。 */
XRT_API xnetsocket xrtNetSocketOpen(xnetfamily Family,
	xnetsockettype Type, uint32 iFlags)
{
	__xrt_netsocket_native hSocket;
	int iFamily;
	int iType;
	int iProtocol;

	if ( ((Family != XNET_FAMILY_IPV4) && (Family != XNET_FAMILY_IPV6)) ||
		 ((Type != XNET_SOCKET_STREAM) && (Type != XNET_SOCKET_DGRAM)) ||
		 ((iFlags & ~((uint32)XNET_SOCKET_NONBLOCK)) != 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtNetEnsure() ) {
		return NULL;
	}

	iFamily = (Family == XNET_FAMILY_IPV4) ? AF_INET : AF_INET6;
	iType = (Type == XNET_SOCKET_STREAM) ? SOCK_STREAM : SOCK_DGRAM;
	iProtocol = (Type == XNET_SOCKET_STREAM) ? IPPROTO_TCP : IPPROTO_UDP;

	hSocket = __xrtNetSocketOpenNative(
		iFamily, iType, iProtocol, iFlags);
	if ( hSocket == __XRT_NET_SOCKET_INVALID ) {
		int iCode = __xrtNetSocketLastError();

		__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_OPEN,
			"open", "opening socket failed", iCode);
		return NULL;
	}

	return __xrtNetSocketAdopt((uintptr_t)hSocket, Family, Type, iFlags);
}



/* 关闭原生句柄并销毁对象；即使系统关闭失败，对象也立即失效。 */
XRT_API bool xrtNetSocketClose(xnetsocket Socket)
{
	__xrt_netsocket_native hSocket;
	bool bResult;
	int iCode = 0;

	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_CLOSE, "close") ) {
		return false;
	}
	hSocket = __xrtNetSocketHandle(Socket);
	Socket->Native = (uintptr_t)__XRT_NET_SOCKET_INVALID;
	bResult = __xrtNetSocketCloseNative(hSocket);
	if ( !bResult ) {
		iCode = __xrtNetSocketLastError();
	}
	xrtFree(Socket);

	if ( !bResult ) {
		__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_CLOSE,
			"close", "closing socket failed", iCode);
	}
	return bResult;
}



/* 返回借用的原生句柄，调用方不得自行关闭。 */
XRT_API intptr_t xrtNetSocketNative(xnetsocket Socket)
{
	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_NATIVE, "native") ) {
		return (intptr_t)-1;
	}
	return (intptr_t)Socket->Native;
}



/* 返回 Socket 创建时确定的地址族。 */
XRT_API xnetfamily xrtNetSocketFamily(xnetsocket Socket)
{
	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_NATIVE, "family") ) {
		return XNET_FAMILY_UNSPEC;
	}
	return Socket->Family;
}



/* 返回 Socket 创建时确定的类型。 */
XRT_API xnetsockettype xrtNetSocketType(xnetsocket Socket)
{
	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_NATIVE, "type") ) {
		return (xnetsockettype)0;
	}
	return Socket->Type;
}



/* 查询平台接收队列，不创建或覆盖当前线程的 XRT 错误对象。 */
bool __xrtNetSocketAvailableNative(xnetsocket Socket,
	size_t* pSize, int* pSystemCode)
{
	size_t iSize;

	#if defined(_WIN32) || defined(_WIN64)
		{
			u_long iAvailable = 0;

			if ( ioctlsocket(__xrtNetSocketHandle(Socket),
				FIONREAD, &iAvailable) != 0 ) {
				if ( pSystemCode != NULL ) {
					*pSystemCode = __xrtNetSocketLastError();
				}
				return false;
			}
			iSize = (size_t)iAvailable;
		}
	#else
		{
			int iAvailable = 0;

			if ( ioctl(__xrtNetSocketHandle(Socket),
				FIONREAD, &iAvailable) != 0 ) {
				if ( pSystemCode != NULL ) {
					*pSystemCode = __xrtNetSocketLastError();
				}
				return false;
			}
			iSize = (iAvailable > 0) ? (size_t)iAvailable : 0;
		}
	#endif

	if ( pSystemCode != NULL ) {
		*pSystemCode = 0;
	}
	*pSize = iSize;
	return true;
}



/* 查询当前可立即读取的字节数；数据报 Socket 返回下一报文可读长度。 */
XRT_API bool xrtNetSocketAvailable(xnetsocket Socket, size_t* pSize)
{
	int iCode = 0;

	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_READ, "available") || (pSize == NULL) ) {
		if ( pSize == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	if ( !__xrtNetSocketAvailableNative(Socket, pSize, &iCode) ) {
		__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_READ,
			"available", "querying available socket data failed", iCode);
		return false;
	}
	return true;
}



/* 设置一个通用 Socket 选项。 */
XRT_API bool xrtNetSocketSet(xnetsocket Socket,
	xnetoption Option, int64 iValue)
{
	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_OPTION, "set-option") ) {
		return false;
	}

	switch ( Option ) {
		case XNET_OPTION_NONBLOCK:
			if ( !__xrtNetSocketSetNonblockNative(
				__xrtNetSocketHandle(Socket), iValue != 0) ) {
				int iCode = __xrtNetSocketLastError();

				__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_OPTION,
					"set-option", "setting nonblocking mode failed", iCode);
				return false;
			}
			if ( iValue != 0 ) {
				Socket->Flags |= XNET_SOCKET_NONBLOCK;
			} else {
				Socket->Flags &= ~((uint32)XNET_SOCKET_NONBLOCK);
			}
			return true;

		case XNET_OPTION_REUSE_ADDRESS:
			return __xrtNetSocketSetInt(Socket,
				SOL_SOCKET, SO_REUSEADDR, iValue != 0);

		case XNET_OPTION_REUSE_PORT:
			#if defined(SO_REUSEPORT) && !defined(_WIN32) && !defined(_WIN64)
				return __xrtNetSocketSetInt(Socket,
					SOL_SOCKET, SO_REUSEPORT, iValue != 0);
			#else
				return __xrtNetSocketUnsupportedOption("set-option");
			#endif

		case XNET_OPTION_EXCLUSIVE_ADDRESS:
			#if defined(_WIN32) || defined(_WIN64)
				return __xrtNetSocketSetInt(Socket,
					SOL_SOCKET, SO_EXCLUSIVEADDRUSE, iValue != 0);
			#else
				return __xrtNetSocketUnsupportedOption("set-option");
			#endif

		case XNET_OPTION_NO_DELAY:
			if ( !__xrtNetSocketRequireType(Socket,
				XNET_SOCKET_STREAM, "set-option",
				"no-delay requires a stream socket") ) { return false; }
			return __xrtNetSocketSetInt(Socket,
				IPPROTO_TCP, TCP_NODELAY, iValue != 0);

		case XNET_OPTION_KEEP_ALIVE:
			if ( !__xrtNetSocketRequireType(Socket,
				XNET_SOCKET_STREAM, "set-option",
				"keep-alive requires a stream socket") ) { return false; }
			return __xrtNetSocketSetInt(Socket,
				SOL_SOCKET, SO_KEEPALIVE, iValue != 0);

		case XNET_OPTION_BROADCAST:
			if ( !__xrtNetSocketRequireType(Socket,
				XNET_SOCKET_DGRAM, "set-option",
				"broadcast requires a datagram socket") ) { return false; }
			return __xrtNetSocketSetInt(Socket,
				SOL_SOCKET, SO_BROADCAST, iValue != 0);

		case XNET_OPTION_IPV6_ONLY:
			if ( Socket->Family != XNET_FAMILY_IPV6 ) {
				__xrtNetSocketSetError(XERR_ARGUMENT,
					XNET_ERROR_SOCKET_OPTION, "set-option",
					"IPv6-only option requires an IPv6 socket");
				return false;
			}
			return __xrtNetSocketSetInt(Socket,
				IPPROTO_IPV6, IPV6_V6ONLY, iValue != 0);

		case XNET_OPTION_RECEIVE_BUFFER:
			return __xrtNetSocketSetInt(Socket,
				SOL_SOCKET, SO_RCVBUF, iValue);

		case XNET_OPTION_SEND_BUFFER:
			return __xrtNetSocketSetInt(Socket,
				SOL_SOCKET, SO_SNDBUF, iValue);

		case XNET_OPTION_LINGER:
			{
				struct linger Linger;

				if ( !__xrtNetSocketRequireType(Socket,
					XNET_SOCKET_STREAM, "set-option",
					"linger requires a stream socket") ) { return false; }
				if ( iValue > INT_MAX ) {
					__xrtNetSocketSetError(XERR_RANGE,
						XNET_ERROR_SOCKET_OPTION, "set-option",
						"linger value is out of range");
					return false;
				}
				Linger.l_onoff = (iValue < 0) ? 0 : 1;
				Linger.l_linger = (iValue < 0) ? 0 : (int)iValue;
				if ( setsockopt(__xrtNetSocketHandle(Socket),
					SOL_SOCKET, SO_LINGER, (const char*)&Linger,
					(socklen_t)sizeof(Linger)) != 0 ) {
					int iCode = __xrtNetSocketLastError();

					__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_OPTION,
						"set-option", "setting linger failed", iCode);
					return false;
				}
				return true;
			}

		case XNET_OPTION_HOP_LIMIT:
			return (Socket->Family == XNET_FAMILY_IPV6) ?
				__xrtNetSocketSetInt(Socket,
					IPPROTO_IPV6, IPV6_UNICAST_HOPS, iValue) :
				__xrtNetSocketSetInt(Socket,
					IPPROTO_IP, IP_TTL, iValue);

		case XNET_OPTION_TRAFFIC_CLASS:
			if ( Socket->Family == XNET_FAMILY_IPV4 ) {
				return __xrtNetSocketSetInt(Socket,
					IPPROTO_IP, IP_TOS, iValue);
			}
			#if defined(IPV6_TCLASS)
				return __xrtNetSocketSetInt(Socket,
					IPPROTO_IPV6, IPV6_TCLASS, iValue);
			#else
				return __xrtNetSocketUnsupportedOption("set-option");
			#endif

		case XNET_OPTION_PATH_MTU_MODE:
			{
				int iLevel;
				int iName;
				int iNative;

				if ( (iValue < XNET_PMTU_SYSTEM) ||
					 (iValue > XNET_PMTU_PROBE) ||
					 !__xrtNetSocketMtuModeNative(
						(xnetpmtumode)iValue,
						&iNative
					 ) || !__xrtNetSocketMtuOption(
						Socket,
						false,
						&iLevel,
						&iName
					 ) ) {
					if ( (iValue < XNET_PMTU_SYSTEM) ||
						 (iValue > XNET_PMTU_PROBE) ) {
						__xrtNetSocketSetError(
							XERR_ARGUMENT,
							XNET_ERROR_SOCKET_OPTION,
							"set-option",
							"invalid path MTU mode"
						);
					}
					return false;
				}
				return __xrtNetSocketSetInt(
					Socket,
					iLevel,
					iName,
					iNative
				);
			}

		case XNET_OPTION_PATH_MTU:
			__xrtNetSocketSetError(
				XERR_ARGUMENT,
				XNET_ERROR_SOCKET_OPTION,
				"set-option",
				"path MTU is a read-only option"
			);
			return false;

		case XNET_OPTION_DGRAM_ERRORS:
			{
				int iLevel;
				int iName;

				if ( !__xrtNetSocketRequireType(
					Socket,
					XNET_SOCKET_DGRAM,
					"set-option",
					"datagram errors require a datagram socket"
				) || !__xrtNetSocketDgramErrorOption(
					Socket,
					&iLevel,
					&iName
				) ) {
					return false;
				}
				return __xrtNetSocketSetInt(
					Socket,
					iLevel,
					iName,
					iValue != 0
				);
			}

		case XNET_OPTION_ERROR:
			__xrtNetSocketSetError(XERR_ARGUMENT,
				XNET_ERROR_SOCKET_OPTION, "set-option",
				"socket error is a read-only option");
			return false;
	}

	__xrtNetSocketSetError(XERR_ARGUMENT, XNET_ERROR_SOCKET_OPTION,
		"set-option", "unknown socket option");
	return false;
}



/* 查询一个通用 Socket 选项，成功才修改输出。 */
XRT_API bool xrtNetSocketGet(xnetsocket Socket,
	xnetoption Option, int64* pValue)
{
	int64 iValue;

	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_OPTION, "get-option") || (pValue == NULL) ) {
		if ( pValue == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}

	switch ( Option ) {
		case XNET_OPTION_NONBLOCK:
			iValue = ((Socket->Flags & XNET_SOCKET_NONBLOCK) != 0) ? 1 : 0;
			break;

		case XNET_OPTION_REUSE_ADDRESS:
			if ( !__xrtNetSocketGetInt(Socket,
				SOL_SOCKET, SO_REUSEADDR, &iValue) ) { return false; }
			break;

		case XNET_OPTION_REUSE_PORT:
			#if defined(SO_REUSEPORT) && !defined(_WIN32) && !defined(_WIN64)
				if ( !__xrtNetSocketGetInt(Socket,
					SOL_SOCKET, SO_REUSEPORT, &iValue) ) { return false; }
				break;
			#else
				return __xrtNetSocketUnsupportedOption("get-option");
			#endif

		case XNET_OPTION_EXCLUSIVE_ADDRESS:
			#if defined(_WIN32) || defined(_WIN64)
				if ( !__xrtNetSocketGetInt(Socket,
					SOL_SOCKET, SO_EXCLUSIVEADDRUSE, &iValue) ) { return false; }
				break;
			#else
				return __xrtNetSocketUnsupportedOption("get-option");
			#endif

		case XNET_OPTION_NO_DELAY:
			if ( !__xrtNetSocketRequireType(Socket,
				XNET_SOCKET_STREAM, "get-option",
				"no-delay requires a stream socket") ) { return false; }
			if ( !__xrtNetSocketGetInt(Socket,
				IPPROTO_TCP, TCP_NODELAY, &iValue) ) { return false; }
			break;

		case XNET_OPTION_KEEP_ALIVE:
			if ( !__xrtNetSocketRequireType(Socket,
				XNET_SOCKET_STREAM, "get-option",
				"keep-alive requires a stream socket") ) { return false; }
			if ( !__xrtNetSocketGetInt(Socket,
				SOL_SOCKET, SO_KEEPALIVE, &iValue) ) { return false; }
			break;

		case XNET_OPTION_BROADCAST:
			if ( !__xrtNetSocketRequireType(Socket,
				XNET_SOCKET_DGRAM, "get-option",
				"broadcast requires a datagram socket") ) { return false; }
			if ( !__xrtNetSocketGetInt(Socket,
				SOL_SOCKET, SO_BROADCAST, &iValue) ) { return false; }
			break;

		case XNET_OPTION_IPV6_ONLY:
			if ( Socket->Family != XNET_FAMILY_IPV6 ) {
				__xrtNetSocketSetError(XERR_ARGUMENT,
					XNET_ERROR_SOCKET_OPTION, "get-option",
					"IPv6-only option requires an IPv6 socket");
				return false;
			}
			if ( !__xrtNetSocketGetInt(Socket,
				IPPROTO_IPV6, IPV6_V6ONLY, &iValue) ) { return false; }
			break;

		case XNET_OPTION_RECEIVE_BUFFER:
			if ( !__xrtNetSocketGetInt(Socket,
				SOL_SOCKET, SO_RCVBUF, &iValue) ) { return false; }
			break;

		case XNET_OPTION_SEND_BUFFER:
			if ( !__xrtNetSocketGetInt(Socket,
				SOL_SOCKET, SO_SNDBUF, &iValue) ) { return false; }
			break;

		case XNET_OPTION_LINGER:
			{
				struct linger Linger;

				if ( !__xrtNetSocketRequireType(Socket,
					XNET_SOCKET_STREAM, "get-option",
					"linger requires a stream socket") ) { return false; }
				#if defined(_WIN32) || defined(_WIN64)
					int iSize = (int)sizeof(Linger);
				#else
					socklen_t iSize = (socklen_t)sizeof(Linger);
				#endif

				if ( getsockopt(__xrtNetSocketHandle(Socket),
					SOL_SOCKET, SO_LINGER, (char*)&Linger, &iSize) != 0 ) {
					int iCode = __xrtNetSocketLastError();

					__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_OPTION,
						"get-option", "querying linger failed", iCode);
					return false;
				}
				iValue = (Linger.l_onoff == 0) ? -1 : (int64)Linger.l_linger;
				break;
			}

		case XNET_OPTION_HOP_LIMIT:
			if ( Socket->Family == XNET_FAMILY_IPV6 ) {
				if ( !__xrtNetSocketGetInt(Socket,
					IPPROTO_IPV6, IPV6_UNICAST_HOPS, &iValue) ) { return false; }
			} else if ( !__xrtNetSocketGetInt(Socket,
				IPPROTO_IP, IP_TTL, &iValue) ) {
				return false;
			}
			break;

		case XNET_OPTION_TRAFFIC_CLASS:
			if ( Socket->Family == XNET_FAMILY_IPV4 ) {
				if ( !__xrtNetSocketGetInt(Socket,
					IPPROTO_IP, IP_TOS, &iValue) ) { return false; }
				break;
			}
			#if defined(IPV6_TCLASS)
				if ( !__xrtNetSocketGetInt(Socket,
					IPPROTO_IPV6, IPV6_TCLASS, &iValue) ) { return false; }
				break;
			#else
				return __xrtNetSocketUnsupportedOption("get-option");
			#endif

		case XNET_OPTION_PATH_MTU_MODE:
			{
				int iLevel;
				int iName;
				xnetpmtumode Mode;

				if ( !__xrtNetSocketMtuOption(
					Socket,
					false,
					&iLevel,
					&iName
				) || !__xrtNetSocketGetInt(
					Socket,
					iLevel,
					iName,
					&iValue
				) || !__xrtNetSocketMtuModeStable(
					(int)iValue,
					&Mode
				) ) {
					return false;
				}
				iValue = (int64)Mode;
				break;
			}

		case XNET_OPTION_PATH_MTU:
			{
				int iLevel;
				int iName;

				if ( !__xrtNetSocketMtuOption(
					Socket,
					true,
					&iLevel,
					&iName
				) || !__xrtNetSocketGetInt(
					Socket,
					iLevel,
					iName,
					&iValue
				) ) {
					return false;
				}
				break;
			}

		case XNET_OPTION_DGRAM_ERRORS:
			{
				int iLevel;
				int iName;

				if ( !__xrtNetSocketRequireType(
					Socket,
					XNET_SOCKET_DGRAM,
					"get-option",
					"datagram errors require a datagram socket"
				) || !__xrtNetSocketDgramErrorOption(
					Socket,
					&iLevel,
					&iName
				) || !__xrtNetSocketGetInt(
					Socket,
					iLevel,
					iName,
					&iValue
				) ) {
					return false;
				}
				break;
			}

		case XNET_OPTION_ERROR:
			if ( !__xrtNetSocketGetInt(Socket,
				SOL_SOCKET, SO_ERROR, &iValue) ) { return false; }
			break;

		default:
			__xrtNetSocketSetError(XERR_ARGUMENT,
				XNET_ERROR_SOCKET_OPTION, "get-option",
				"unknown socket option");
			return false;
	}

	*pValue = iValue;
	return true;
}



/* 返回一个控制消息整数；平台可能用 byte、int 或 DWORD 表达同一字段。 */
static int __xrtNetSocketDgramMetaInt(const void* pData, size_t iSize)
{
	int iValue = 0;

	if ( iSize >= sizeof(iValue) ) {
		memcpy(&iValue, pData, sizeof(iValue));
	} else if ( iSize != 0 ) {
		iValue = (int)*(const unsigned char*)pData;
	}
	return iValue;
}



/* 写入一个不带端口的 IPv4 目标地址。 */
static void __xrtNetSocketDgramMetaIPv4(
	xnetdgrammeta* pMeta,
	const void* pAddress
)
{
	memset(&pMeta->Destination, 0, sizeof(pMeta->Destination));
	pMeta->Destination.Family = XNET_FAMILY_IPV4;
	memcpy(pMeta->Destination.Address, pAddress, 4);
	pMeta->Flags |= XNET_DGRAM_META_DESTINATION;
}



/* 写入一个不带端口的 IPv6 目标地址及其接收接口。 */
static void __xrtNetSocketDgramMetaIPv6(
	xnetdgrammeta* pMeta,
	const void* pAddress,
	uint32 iInterface
)
{
	memset(&pMeta->Destination, 0, sizeof(pMeta->Destination));
	pMeta->Destination.Family = XNET_FAMILY_IPV6;
	pMeta->Destination.Scope = iInterface;
	memcpy(pMeta->Destination.Address, pAddress, 16);
	pMeta->Flags |= XNET_DGRAM_META_DESTINATION;
}



/* 清零并解析平台控制消息，只发布 Socket 明确启用的字段。 */
void __xrtNetSocketDgramMetaParse(
	xnetsocket Socket,
	xnetdgrammeta* pMeta,
	const void* pControl,
	size_t iControl,
	uint32 iMessageFlags
)
{
	uint32 iEnabled = (Socket != NULL) ? Socket->DgramMeta : 0;

	if ( pMeta == NULL ) {
		return;
	}
	memset(pMeta, 0, sizeof(*pMeta));
	#if defined(MSG_CTRUNC)
		if ( (iMessageFlags & MSG_CTRUNC) != 0 ) {
			pMeta->Flags |= XNET_DGRAM_META_TRUNCATED;
		}
	#else
		(void)iMessageFlags;
	#endif
	if ( (pControl == NULL) || (iControl == 0) || (iEnabled == 0) ) {
		return;
	}

	#if defined(_WIN32) || defined(_WIN64)
		WSAMSG Message;
		WSACMSGHDR* pHeader;

		memset(&Message, 0, sizeof(Message));
		Message.Control.buf = (CHAR*)pControl;
		Message.Control.len = (ULONG)iControl;
		for ( pHeader = WSA_CMSG_FIRSTHDR(&Message);
			pHeader != NULL;
			pHeader = WSA_CMSG_NXTHDR(&Message, pHeader) ) {
			const void* pData = WSA_CMSG_DATA(pHeader);
			size_t iHeader = WSA_CMSG_LEN(0);
			size_t iSize = (pHeader->cmsg_len >= iHeader) ?
				(size_t)pHeader->cmsg_len - iHeader : 0;

			if ( (pHeader->cmsg_level == IPPROTO_UDP) &&
				 (pHeader->cmsg_type == UDP_COALESCED_INFO) &&
				 (iSize >= sizeof(uint32)) &&
				 ((iEnabled & XNET_DGRAM_META_SEGMENT_SIZE) != 0) ) {
				uint32 iSegment = 0;

				memcpy(&iSegment, pData, sizeof(iSegment));
				if ( iSegment != 0 ) {
					pMeta->SegmentSize = iSegment;
					pMeta->Flags |= XNET_DGRAM_META_SEGMENT_SIZE;
				}
			} else if ( (pHeader->cmsg_level == IPPROTO_IP) &&
				 (pHeader->cmsg_type == IP_PKTINFO) &&
				 (iSize >= sizeof(IN_PKTINFO)) ) {
				const IN_PKTINFO* pInfo = (const IN_PKTINFO*)pData;

				if ( (iEnabled & XNET_DGRAM_META_DESTINATION) != 0 ) {
					__xrtNetSocketDgramMetaIPv4(
						pMeta,
						&pInfo->ipi_addr
					);
				}
				if ( (iEnabled & XNET_DGRAM_META_INTERFACE) != 0 ) {
					pMeta->Interface = (uint32)pInfo->ipi_ifindex;
					pMeta->Flags |= XNET_DGRAM_META_INTERFACE;
				}
			} else if ( (pHeader->cmsg_level == IPPROTO_IPV6) &&
				 (pHeader->cmsg_type == IPV6_PKTINFO) &&
				 (iSize >= sizeof(IN6_PKTINFO)) ) {
				const IN6_PKTINFO* pInfo = (const IN6_PKTINFO*)pData;

				if ( (iEnabled & XNET_DGRAM_META_DESTINATION) != 0 ) {
					__xrtNetSocketDgramMetaIPv6(
						pMeta,
						&pInfo->ipi6_addr,
						(uint32)pInfo->ipi6_ifindex
					);
				}
				if ( (iEnabled & XNET_DGRAM_META_INTERFACE) != 0 ) {
					pMeta->Interface = (uint32)pInfo->ipi6_ifindex;
					pMeta->Flags |= XNET_DGRAM_META_INTERFACE;
				}
			} else if ( (iEnabled & XNET_DGRAM_META_HOP_LIMIT) != 0 ) {
				if ( (pHeader->cmsg_level == IPPROTO_IP) &&
					 ((pHeader->cmsg_type == IP_TTL)
					 #if defined(IP_RECVTTL)
						 || (pHeader->cmsg_type == IP_RECVTTL)
					 #endif
					 ) ) {
					pMeta->HopLimit = __xrtNetSocketDgramMetaInt(pData, iSize);
					pMeta->Flags |= XNET_DGRAM_META_HOP_LIMIT;
				} else if ( (pHeader->cmsg_level == IPPROTO_IPV6) &&
					 (pHeader->cmsg_type == IPV6_HOPLIMIT) ) {
					pMeta->HopLimit = __xrtNetSocketDgramMetaInt(pData, iSize);
					pMeta->Flags |= XNET_DGRAM_META_HOP_LIMIT;
				}
			}
			if ( (iEnabled & XNET_DGRAM_META_TRAFFIC_CLASS) != 0 ) {
				if ( (pHeader->cmsg_level == IPPROTO_IP) &&
					 ((pHeader->cmsg_type == IP_TOS)
					 #if defined(IP_RECVTOS)
						 || (pHeader->cmsg_type == IP_RECVTOS)
					 #endif
					 ) ) {
					pMeta->TrafficClass =
						__xrtNetSocketDgramMetaInt(pData, iSize) & 0xFF;
					pMeta->Flags |= XNET_DGRAM_META_TRAFFIC_CLASS;
				} else if ( (pHeader->cmsg_level == IPPROTO_IPV6) &&
					 (pHeader->cmsg_type == IPV6_TCLASS) ) {
					pMeta->TrafficClass =
						__xrtNetSocketDgramMetaInt(pData, iSize) & 0xFF;
					pMeta->Flags |= XNET_DGRAM_META_TRAFFIC_CLASS;
				}
			}
		}
	#else
		struct msghdr Message;
		struct cmsghdr* pHeader;

		memset(&Message, 0, sizeof(Message));
		Message.msg_control = (void*)pControl;
		Message.msg_controllen = iControl;
		for ( pHeader = CMSG_FIRSTHDR(&Message);
			pHeader != NULL;
			pHeader = CMSG_NXTHDR(&Message, pHeader) ) {
			const void* pData = CMSG_DATA(pHeader);
			size_t iHeader = CMSG_LEN(0);
			size_t iSize = (pHeader->cmsg_len >= iHeader) ?
				(size_t)pHeader->cmsg_len - iHeader : 0;

			#if defined(__linux__)
				if ( (pHeader->cmsg_level == SOL_UDP) &&
					 (pHeader->cmsg_type == UDP_GRO) &&
					 (iSize >= sizeof(uint16)) &&
					 ((iEnabled & XNET_DGRAM_META_SEGMENT_SIZE) != 0) ) {
					uint16 iSegment = 0;

					memcpy(&iSegment, pData, sizeof(iSegment));
					if ( iSegment != 0 ) {
						pMeta->SegmentSize = (uint32)iSegment;
						pMeta->Flags |= XNET_DGRAM_META_SEGMENT_SIZE;
					}
					continue;
				}
			#endif

			#if defined(IP_PKTINFO)
				if ( (pHeader->cmsg_level == IPPROTO_IP) &&
					 (pHeader->cmsg_type == IP_PKTINFO) ) {
					#if defined(__linux__)
						uint32 iInterface = 0;
						const unsigned char* pAddress;

						if ( iSize < 12u ) {
							continue;
						}
						memcpy(&iInterface, pData, sizeof(iInterface));
						pAddress = (const unsigned char*)pData + 8;
					#else
						const struct in_pktinfo* pInfo =
							(const struct in_pktinfo*)pData;
						uint32 iInterface;
						const void* pAddress;

						if ( iSize < sizeof(*pInfo) ) {
							continue;
						}
						iInterface = (uint32)pInfo->ipi_ifindex;
						pAddress = &pInfo->ipi_addr;
					#endif

					if ( (iEnabled & XNET_DGRAM_META_DESTINATION) != 0 ) {
						__xrtNetSocketDgramMetaIPv4(
							pMeta,
							pAddress
						);
					}
					if ( (iEnabled & XNET_DGRAM_META_INTERFACE) != 0 ) {
						pMeta->Interface = iInterface;
						pMeta->Flags |= XNET_DGRAM_META_INTERFACE;
					}
					continue;
				}
			#endif
			#if defined(IP_RECVDSTADDR)
				if ( (pHeader->cmsg_level == IPPROTO_IP) &&
					 (pHeader->cmsg_type == IP_RECVDSTADDR) &&
					 (iSize >= sizeof(struct in_addr)) &&
					 ((iEnabled & XNET_DGRAM_META_DESTINATION) != 0) ) {
					__xrtNetSocketDgramMetaIPv4(pMeta, pData);
					continue;
				}
			#endif
			#if defined(IP_RECVIF) && (defined(__APPLE__) || \
				defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__))
				if ( (pHeader->cmsg_level == IPPROTO_IP) &&
					 (pHeader->cmsg_type == IP_RECVIF) &&
					 (iSize >= sizeof(struct sockaddr_dl)) &&
					 ((iEnabled & XNET_DGRAM_META_INTERFACE) != 0) ) {
					const struct sockaddr_dl* pLink =
						(const struct sockaddr_dl*)pData;

					pMeta->Interface = (uint32)pLink->sdl_index;
					pMeta->Flags |= XNET_DGRAM_META_INTERFACE;
					continue;
				}
			#endif
			#if defined(IPV6_PKTINFO)
				if ( (pHeader->cmsg_level == IPPROTO_IPV6) &&
					 (pHeader->cmsg_type == IPV6_PKTINFO) &&
					 (iSize >= 20u) ) {
					uint32 iInterface = 0;
					const unsigned char* pInfo =
						(const unsigned char*)pData;

					memcpy(&iInterface, pInfo + 16, sizeof(iInterface));

					if ( (iEnabled & XNET_DGRAM_META_DESTINATION) != 0 ) {
						__xrtNetSocketDgramMetaIPv6(
							pMeta,
							pInfo,
							iInterface
						);
					}
					if ( (iEnabled & XNET_DGRAM_META_INTERFACE) != 0 ) {
						pMeta->Interface = iInterface;
						pMeta->Flags |= XNET_DGRAM_META_INTERFACE;
					}
					continue;
				}
			#endif
			if ( (iEnabled & XNET_DGRAM_META_HOP_LIMIT) != 0 ) {
				#if defined(IP_TTL)
					if ( (pHeader->cmsg_level == IPPROTO_IP) &&
						 (pHeader->cmsg_type == IP_TTL) ) {
						pMeta->HopLimit =
							__xrtNetSocketDgramMetaInt(pData, iSize);
						pMeta->Flags |= XNET_DGRAM_META_HOP_LIMIT;
						continue;
					}
				#endif
				#if defined(IPV6_HOPLIMIT)
					if ( (pHeader->cmsg_level == IPPROTO_IPV6) &&
						 (pHeader->cmsg_type == IPV6_HOPLIMIT) ) {
						pMeta->HopLimit =
							__xrtNetSocketDgramMetaInt(pData, iSize);
						pMeta->Flags |= XNET_DGRAM_META_HOP_LIMIT;
						continue;
					}
				#endif
			}
			if ( (iEnabled & XNET_DGRAM_META_TRAFFIC_CLASS) != 0 ) {
				#if defined(IP_TOS)
					if ( (pHeader->cmsg_level == IPPROTO_IP) &&
						 (pHeader->cmsg_type == IP_TOS) ) {
						pMeta->TrafficClass =
							__xrtNetSocketDgramMetaInt(pData, iSize) & 0xFF;
						pMeta->Flags |= XNET_DGRAM_META_TRAFFIC_CLASS;
						continue;
					}
				#endif
				#if defined(IPV6_TCLASS)
					if ( (pHeader->cmsg_level == IPPROTO_IPV6) &&
						 (pHeader->cmsg_type == IPV6_TCLASS) ) {
						pMeta->TrafficClass =
							__xrtNetSocketDgramMetaInt(pData, iSize) & 0xFF;
						pMeta->Flags |= XNET_DGRAM_META_TRAFFIC_CLASS;
					}
				#endif
			}
		}
	#endif
}



#if defined(_WIN32) || defined(_WIN64)

/* 延迟加载一个 Winsock 扩展入口，并以整数保存函数指针的原始位。 */
static uintptr_t __xrtNetSocketMessageLoad(
	xnetsocket Socket,
	uintptr_t* pCached,
	const void* pId,
	size_t iIdSize,
	bool bReport,
	cstr sOperation,
	cstr sMessage
)
{
	uintptr_t iFunction = 0;
	DWORD iBytes = 0;

	if ( *pCached != 0 ) {
		return *pCached;
	}
	if ( WSAIoctl(
		__xrtNetSocketHandle(Socket),
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		(void*)pId,
		(DWORD)iIdSize,
		&iFunction,
		(DWORD)sizeof(iFunction),
		&iBytes,
		NULL,
		NULL
	) != 0 ) {
		int iCode = __xrtNetSocketLastError();

		if ( bReport ) {
			__xrtNetSocketSetSystemError(
				XNET_ERROR_SOCKET_OPTION,
				sOperation,
				sMessage,
				iCode
			);
		}
		return 0;
	}
	*pCached = iFunction;
	return iFunction;
}

#endif



/* 延迟加载同步和 IOCP 共用的 WSARecvMsg 入口。 */
static uintptr_t __xrtNetSocketReceiveMessageLoad(
	xnetsocket Socket,
	bool bReport
)
{
	#if defined(_WIN32) || defined(_WIN64)
		GUID Id = WSAID_WSARECVMSG;

		return __xrtNetSocketMessageLoad(
			Socket,
			&Socket->ReceiveMessage,
			&Id,
			sizeof(Id),
			bReport,
			"load-recv-message",
			"loading WSARecvMsg failed"
		);
	#else
		(void)Socket;
		(void)bReport;
		return 0;
	#endif
}



/* 返回已缓存的 WSARecvMsg，失败时保留完整平台错误。 */
uintptr_t __xrtNetSocketReceiveMessage(xnetsocket Socket)
{
	return __xrtNetSocketReceiveMessageLoad(Socket, true);
}



/* 延迟加载同步和 IOCP 共用的 WSASendMsg 入口。 */
static uintptr_t __xrtNetSocketSendMessageLoad(
	xnetsocket Socket,
	bool bReport
)
{
	#if defined(_WIN32) || defined(_WIN64)
		GUID Id = WSAID_WSASENDMSG;

		return __xrtNetSocketMessageLoad(
			Socket,
			&Socket->SendMessage,
			&Id,
			sizeof(Id),
			bReport,
			"load-send-message",
			"loading WSASendMsg failed"
		);
	#else
		(void)Socket;
		(void)bReport;
		return 0;
	#endif
}



/* 返回已经缓存或刚加载的 WSASendMsg 入口。 */
uintptr_t __xrtNetSocketSendMessage(xnetsocket Socket)
{
	return __xrtNetSocketSendMessageLoad(Socket, true);
}



/* 无错误副作用地探测一个原生 int Socket 选项。 */
static bool __xrtNetSocketDgramMetaHasOption(
	xnetsocket Socket,
	int iLevel,
	int iOption
)
{
	int iValue = 0;

	#if defined(_WIN32) || defined(_WIN64)
		int iSize = (int)sizeof(iValue);
	#else
		socklen_t iSize = (socklen_t)sizeof(iValue);
	#endif

	return getsockopt(
		__xrtNetSocketHandle(Socket),
		iLevel,
		iOption,
		(char*)&iValue,
		&iSize
	) == 0;
}



/* 返回地址族可配置的数据报元数据位。 */
XRT_API uint32 xrtNetSocketDgramMetaAvailable(xnetsocket Socket)
{
	uint32 iFlags = 0;

	if ( !__xrtNetSocketRequire(
		Socket,
		XNET_ERROR_SOCKET_OPTION,
		"query-datagram-metadata"
	) || !__xrtNetSocketRequireType(
		Socket,
		XNET_SOCKET_DGRAM,
		"query-datagram-metadata",
		"datagram metadata requires a datagram socket"
	) ) {
		return 0;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( __xrtNetSocketReceiveMessageLoad(Socket, false) == 0 ) {
			return 0;
		}
	#endif

	if ( Socket->Family == XNET_FAMILY_IPV4 ) {
		#if defined(IP_PKTINFO)
			if ( __xrtNetSocketDgramMetaHasOption(
				Socket, IPPROTO_IP, IP_PKTINFO
			) ) {
				iFlags |= XNET_DGRAM_META_DESTINATION |
					XNET_DGRAM_META_INTERFACE;
			}
		#else
			#if defined(IP_RECVDSTADDR)
				if ( __xrtNetSocketDgramMetaHasOption(
					Socket, IPPROTO_IP, IP_RECVDSTADDR
				) ) {
					iFlags |= XNET_DGRAM_META_DESTINATION;
				}
			#endif
			#if defined(IP_RECVIF)
				if ( __xrtNetSocketDgramMetaHasOption(
					Socket, IPPROTO_IP, IP_RECVIF
				) ) {
					iFlags |= XNET_DGRAM_META_INTERFACE;
				}
			#endif
		#endif
		#if defined(IP_RECVTTL)
			if ( __xrtNetSocketDgramMetaHasOption(
				Socket, IPPROTO_IP, IP_RECVTTL
			) ) {
				iFlags |= XNET_DGRAM_META_HOP_LIMIT;
			}
		#endif
		#if defined(IP_RECVTOS)
			if ( __xrtNetSocketDgramMetaHasOption(
				Socket, IPPROTO_IP, IP_RECVTOS
			) ) {
				iFlags |= XNET_DGRAM_META_TRAFFIC_CLASS;
			}
		#endif
	} else {
		#if defined(IPV6_PKTINFO) && \
			(defined(IPV6_RECVPKTINFO) || defined(_WIN32) || defined(_WIN64))
			#if defined(_WIN32) || defined(_WIN64)
				#define XRT_NET_SOCKET_IPV6_META_OPTION IPV6_PKTINFO
			#else
				#define XRT_NET_SOCKET_IPV6_META_OPTION IPV6_RECVPKTINFO
			#endif
			if ( __xrtNetSocketDgramMetaHasOption(
				Socket, IPPROTO_IPV6, XRT_NET_SOCKET_IPV6_META_OPTION
			) ) {
				iFlags |= XNET_DGRAM_META_DESTINATION |
					XNET_DGRAM_META_INTERFACE;
			}
			#undef XRT_NET_SOCKET_IPV6_META_OPTION
		#endif
		#if defined(_WIN32) || defined(_WIN64) || defined(IPV6_RECVHOPLIMIT)
			#if defined(_WIN32) || defined(_WIN64)
				#define XRT_NET_SOCKET_IPV6_HOP_OPTION IPV6_HOPLIMIT
			#else
				#define XRT_NET_SOCKET_IPV6_HOP_OPTION IPV6_RECVHOPLIMIT
			#endif
			if ( __xrtNetSocketDgramMetaHasOption(
				Socket, IPPROTO_IPV6, XRT_NET_SOCKET_IPV6_HOP_OPTION
			) ) {
				iFlags |= XNET_DGRAM_META_HOP_LIMIT;
			}
			#undef XRT_NET_SOCKET_IPV6_HOP_OPTION
		#endif
		#if defined(IPV6_RECVTCLASS)
			if ( __xrtNetSocketDgramMetaHasOption(
				Socket, IPPROTO_IPV6, IPV6_RECVTCLASS
			) ) {
				iFlags |= XNET_DGRAM_META_TRAFFIC_CLASS;
			}
		#endif
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( __xrtNetSocketDgramMetaHasOption(
			Socket, IPPROTO_UDP, UDP_RECV_MAX_COALESCED_SIZE
		) ) {
			iFlags |= XNET_DGRAM_META_SEGMENT_SIZE;
		}
	#elif defined(__linux__)
		if ( __xrtNetSocketDgramMetaHasOption(
			Socket, SOL_UDP, UDP_GRO
		) ) {
			iFlags |= XNET_DGRAM_META_SEGMENT_SIZE;
		}
	#endif
	return iFlags;
}



/* 返回当前已经启用的数据报元数据位。 */
XRT_API uint32 xrtNetSocketDgramMetaEnabled(xnetsocket Socket)
{
	if ( !__xrtNetSocketRequire(
		Socket,
		XNET_ERROR_SOCKET_OPTION,
		"query-datagram-metadata"
	) || !__xrtNetSocketRequireType(
		Socket,
		XNET_SOCKET_DGRAM,
		"query-datagram-metadata",
		"datagram metadata requires a datagram socket"
	) ) {
		return 0;
	}
	return Socket->DgramMeta;
}



/* 设置一个原生接收元数据选项，并同步对象中的有效位。 */
static bool __xrtNetSocketDgramMetaOption(
	xnetsocket Socket,
	int iLevel,
	int iOption,
	uint32 iMask,
	uint32 iRequested
)
{
	bool bEnabled = (iRequested & iMask) != 0;

	if ( !__xrtNetSocketSetInt(Socket, iLevel, iOption, bEnabled ? 1 : 0) ) {
		return false;
	}
	Socket->DgramMeta = (Socket->DgramMeta & ~iMask) |
		(iRequested & iMask);
	return true;
}



/* 成功后精确配置元数据；失败时对象状态反映已经生效的平台选项。 */
XRT_API bool xrtNetSocketDgramMetaSet(xnetsocket Socket, uint32 iFlags)
{
	const uint32 iFields = XNET_DGRAM_META_DESTINATION |
		XNET_DGRAM_META_INTERFACE |
		XNET_DGRAM_META_HOP_LIMIT |
		XNET_DGRAM_META_TRAFFIC_CLASS |
		XNET_DGRAM_META_SEGMENT_SIZE;
	uint32 iAvailable;

	if ( !__xrtNetSocketRequire(
		Socket,
		XNET_ERROR_SOCKET_OPTION,
		"set-datagram-metadata"
	) || !__xrtNetSocketRequireType(
		Socket,
		XNET_SOCKET_DGRAM,
		"set-datagram-metadata",
		"datagram metadata requires a datagram socket"
	) ) {
		return false;
	}
	if ( (iFlags & ~iFields) != 0 ) {
		__xrtNetSocketSetError(
			XERR_ARGUMENT,
			XNET_ERROR_SOCKET_OPTION,
			"set-datagram-metadata",
			"unknown datagram metadata flag"
		);
		return false;
	}
	iAvailable = xrtNetSocketDgramMetaAvailable(Socket);
	if ( (iFlags & ~iAvailable) != 0 ) {
		__xrtNetSocketSetError(
			XERR_UNSUPPORTED,
			XNET_ERROR_SOCKET_OPTION,
			"set-datagram-metadata",
			"requested datagram metadata is unavailable"
		);
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( (iFlags != 0) &&
			 (__xrtNetSocketReceiveMessage(Socket) == 0) ) {
			return false;
		}
	#endif

	if ( Socket->Family == XNET_FAMILY_IPV4 ) {
		#if defined(IP_PKTINFO)
			if ( !__xrtNetSocketDgramMetaOption(
				Socket,
				IPPROTO_IP,
				IP_PKTINFO,
				XNET_DGRAM_META_DESTINATION |
					XNET_DGRAM_META_INTERFACE,
				iFlags
			) ) {
				return false;
			}
		#else
			#if defined(IP_RECVDSTADDR)
				if ( !__xrtNetSocketDgramMetaOption(
					Socket, IPPROTO_IP, IP_RECVDSTADDR,
					XNET_DGRAM_META_DESTINATION, iFlags
				) ) {
					return false;
				}
			#endif
			#if defined(IP_RECVIF)
				if ( !__xrtNetSocketDgramMetaOption(
					Socket, IPPROTO_IP, IP_RECVIF,
					XNET_DGRAM_META_INTERFACE, iFlags
				) ) {
					return false;
				}
			#endif
		#endif
		#if defined(IP_RECVTTL)
			if ( !__xrtNetSocketDgramMetaOption(
				Socket, IPPROTO_IP, IP_RECVTTL,
				XNET_DGRAM_META_HOP_LIMIT, iFlags
			) ) {
				return false;
			}
		#endif
		#if defined(IP_RECVTOS)
			if ( !__xrtNetSocketDgramMetaOption(
				Socket, IPPROTO_IP, IP_RECVTOS,
				XNET_DGRAM_META_TRAFFIC_CLASS, iFlags
			) ) {
				return false;
			}
		#endif
	} else {
		#if defined(IPV6_PKTINFO) && defined(_WIN32)
			if ( !__xrtNetSocketDgramMetaOption(
				Socket, IPPROTO_IPV6, IPV6_PKTINFO,
				XNET_DGRAM_META_DESTINATION |
					XNET_DGRAM_META_INTERFACE,
				iFlags
			) ) {
				return false;
			}
		#elif defined(IPV6_PKTINFO) && defined(IPV6_RECVPKTINFO)
			if ( !__xrtNetSocketDgramMetaOption(
				Socket, IPPROTO_IPV6, IPV6_RECVPKTINFO,
				XNET_DGRAM_META_DESTINATION |
					XNET_DGRAM_META_INTERFACE,
				iFlags
			) ) {
				return false;
			}
		#endif
		#if defined(_WIN32) || defined(_WIN64)
			if ( !__xrtNetSocketDgramMetaOption(
				Socket, IPPROTO_IPV6, IPV6_HOPLIMIT,
				XNET_DGRAM_META_HOP_LIMIT, iFlags
			) ) {
				return false;
			}
		#elif defined(IPV6_RECVHOPLIMIT)
			if ( !__xrtNetSocketDgramMetaOption(
				Socket, IPPROTO_IPV6, IPV6_RECVHOPLIMIT,
				XNET_DGRAM_META_HOP_LIMIT, iFlags
			) ) {
				return false;
			}
		#endif
		#if defined(IPV6_RECVTCLASS)
			if ( !__xrtNetSocketDgramMetaOption(
				Socket, IPPROTO_IPV6, IPV6_RECVTCLASS,
				XNET_DGRAM_META_TRAFFIC_CLASS, iFlags
			) ) {
				return false;
			}
		#endif
	}
	#if defined(__linux__)
		if ( (((iFlags | Socket->DgramMeta) &
			XNET_DGRAM_META_SEGMENT_SIZE) != 0) &&
			 !__xrtNetSocketDgramMetaOption(
				Socket,
				SOL_UDP,
				UDP_GRO,
				XNET_DGRAM_META_SEGMENT_SIZE,
				iFlags
			) ) {
			return false;
		}
	#elif defined(_WIN32) || defined(_WIN64)
		if ( (((iFlags | Socket->DgramMeta) &
			XNET_DGRAM_META_SEGMENT_SIZE) != 0) &&
			 !__xrtNetSocketSetInt(
				Socket,
				IPPROTO_UDP,
				UDP_RECV_MAX_COALESCED_SIZE,
				(iFlags & XNET_DGRAM_META_SEGMENT_SIZE) != 0 ?
					65535 : 0
			) ) {
			return false;
		}
		Socket->DgramMeta = (Socket->DgramMeta &
			~XNET_DGRAM_META_SEGMENT_SIZE) |
			(iFlags & XNET_DGRAM_META_SEGMENT_SIZE);
	#endif
	return true;
}



/* 返回地址族当前可构建的逐数据报发送控制位。 */
XRT_API uint32 xrtNetSocketDgramControlAvailable(xnetsocket Socket)
{
	uint32 iFlags = 0;

	if ( !__xrtNetSocketRequire(
		Socket,
		XNET_ERROR_SOCKET_OPTION,
		"query-datagram-control"
	) || !__xrtNetSocketRequireType(
		Socket,
		XNET_SOCKET_DGRAM,
		"query-datagram-control",
		"datagram control requires a datagram socket"
	) ) {
		return 0;
	}

	#if defined(_WIN32) || defined(_WIN64)
		if ( __xrtNetSocketSendMessageLoad(Socket, false) != 0 ) {
			iFlags = XNET_DGRAM_CONTROL_SOURCE |
				XNET_DGRAM_CONTROL_INTERFACE;
			if ( __xrtNetSocketDgramMetaHasOption(
				Socket, IPPROTO_UDP, UDP_SEND_MSG_SIZE
			) ) {
				iFlags |= XNET_DGRAM_CONTROL_SEGMENT_SIZE;
			}
		}
	#elif defined(__linux__)
		if ( __xrtNetSocketDgramMetaHasOption(
			Socket, SOL_UDP, UDP_SEGMENT
		) ) {
			iFlags |= XNET_DGRAM_CONTROL_SEGMENT_SIZE;
		}
		if ( Socket->Family == XNET_FAMILY_IPV4 ) {
			#if defined(IP_PKTINFO)
				iFlags |= XNET_DGRAM_CONTROL_SOURCE |
					XNET_DGRAM_CONTROL_INTERFACE;
			#endif
			#if defined(IP_TTL)
				iFlags |= XNET_DGRAM_CONTROL_HOP_LIMIT;
			#endif
			#if defined(IP_TOS)
				iFlags |= XNET_DGRAM_CONTROL_TRAFFIC_CLASS;
			#endif
		} else {
			#if defined(IPV6_PKTINFO)
				iFlags |= XNET_DGRAM_CONTROL_SOURCE |
					XNET_DGRAM_CONTROL_INTERFACE;
			#endif
			#if defined(IPV6_HOPLIMIT)
				iFlags |= XNET_DGRAM_CONTROL_HOP_LIMIT;
			#endif
			#if defined(IPV6_TCLASS)
				iFlags |= XNET_DGRAM_CONTROL_TRAFFIC_CLASS;
			#endif
		}
	#else
		if ( Socket->Family == XNET_FAMILY_IPV4 ) {
			#if defined(IP_SENDSRCADDR)
				iFlags |= XNET_DGRAM_CONTROL_SOURCE;
			#endif
		} else {
			#if defined(IPV6_PKTINFO)
				iFlags |= XNET_DGRAM_CONTROL_SOURCE |
					XNET_DGRAM_CONTROL_INTERFACE;
			#endif
			#if defined(IPV6_HOPLIMIT)
				iFlags |= XNET_DGRAM_CONTROL_HOP_LIMIT;
			#endif
			#if defined(IPV6_TCLASS)
				iFlags |= XNET_DGRAM_CONTROL_TRAFFIC_CLASS;
			#endif
		}
	#endif
	return iFlags;
}



/* 返回不改变 Socket 状态即可判定的数据报高级能力。 */
XRT_API uint32 xrtNetSocketDgramCapabilities(xnetsocket Socket)
{
	uint32 iCapabilities;

	if ( !__xrtNetSocketRequire(
		Socket,
		XNET_ERROR_SOCKET_OPTION,
		"query-datagram-capabilities"
	) || !__xrtNetSocketRequireType(
		Socket,
		XNET_SOCKET_DGRAM,
		"query-datagram-capabilities",
		"datagram capabilities require a datagram socket"
	) ) {
		return 0;
	}

	#if defined(_WIN32) || defined(_WIN64)
		iCapabilities = XNET_DGRAM_CAP_PATH_MTU_MODE |
			XNET_DGRAM_CAP_PATH_MTU_QUERY;
		if ( (__xrtNetSocketSendMessageLoad(Socket, false) != 0) &&
			 __xrtNetSocketDgramMetaHasOption(
				Socket, IPPROTO_UDP, UDP_SEND_MSG_SIZE
			 ) ) {
			iCapabilities |= XNET_DGRAM_CAP_SEGMENT_SEND;
		}
		if ( (__xrtNetSocketReceiveMessageLoad(Socket, false) != 0) &&
			 __xrtNetSocketDgramMetaHasOption(
				Socket, IPPROTO_UDP, UDP_RECV_MAX_COALESCED_SIZE
			 ) ) {
			iCapabilities |= XNET_DGRAM_CAP_SEGMENT_RECEIVE;
		}
	#elif defined(__linux__)
		iCapabilities = XNET_DGRAM_CAP_PATH_MTU_MODE |
			XNET_DGRAM_CAP_PATH_MTU_QUERY |
			XNET_DGRAM_CAP_ERROR_QUEUE;
		if ( __xrtNetSocketDgramMetaHasOption(
			Socket, SOL_UDP, UDP_SEGMENT
		) ) {
			iCapabilities |= XNET_DGRAM_CAP_SEGMENT_SEND;
		}
		if ( __xrtNetSocketDgramMetaHasOption(
			Socket, SOL_UDP, UDP_GRO
		) ) {
			iCapabilities |= XNET_DGRAM_CAP_SEGMENT_RECEIVE;
		}
	#else
		iCapabilities = 0;
	#endif
	return iCapabilities;
}



/* 向对齐控制缓冲追加一个平台控制消息。 */
static bool __xrtNetSocketDgramControlAppend(
	void* pBuffer,
	size_t iCapacity,
	size_t* pOffset,
	int iLevel,
	int iType,
	const void* pData,
	size_t iSize
)
{
	#if defined(_WIN32) || defined(_WIN64)
		size_t iSpace = (size_t)WSA_CMSG_SPACE(iSize);
		WSACMSGHDR* pHeader;

		if ( (*pOffset > iCapacity) ||
			 (iSpace > (iCapacity - *pOffset)) ) {
			return false;
		}
		pHeader = (WSACMSGHDR*)((bytes)pBuffer + *pOffset);
		pHeader->cmsg_len = WSA_CMSG_LEN(iSize);
		pHeader->cmsg_level = iLevel;
		pHeader->cmsg_type = iType;
		memcpy(WSA_CMSG_DATA(pHeader), pData, iSize);
	#else
		size_t iSpace = (size_t)CMSG_SPACE(iSize);
		struct cmsghdr* pHeader;

		if ( (*pOffset > iCapacity) ||
			 (iSpace > (iCapacity - *pOffset)) ) {
			return false;
		}
		pHeader = (struct cmsghdr*)((bytes)pBuffer + *pOffset);
		pHeader->cmsg_len = CMSG_LEN(iSize);
		pHeader->cmsg_level = iLevel;
		pHeader->cmsg_type = iType;
		memcpy(CMSG_DATA(pHeader), pData, iSize);
	#endif
	*pOffset += iSpace;
	return true;
}



/* 校验逐包覆盖值，保证失败发生在 sendmsg 产生任何副作用之前。 */
static bool __xrtNetSocketDgramControlCheck(
	xnetsocket Socket,
	const xnetdgramcontrol* pControl,
	size_t iPayload,
	xneterror Code,
	cstr sOperation
)
{
	const uint32 iFields = XNET_DGRAM_CONTROL_SOURCE |
		XNET_DGRAM_CONTROL_INTERFACE |
		XNET_DGRAM_CONTROL_HOP_LIMIT |
		XNET_DGRAM_CONTROL_TRAFFIC_CLASS |
		XNET_DGRAM_CONTROL_SEGMENT_SIZE;
	uint32 iAvailable;

	if ( (pControl == NULL) || (pControl->Flags == 0) ) {
		return true;
	}
	if ( (pControl->Flags & ~iFields) != 0 ) {
		__xrtNetSocketSetError(
			XERR_ARGUMENT,
			Code,
			sOperation,
			"unknown datagram control flag"
		);
		return false;
	}
	if ( ((pControl->Flags & XNET_DGRAM_CONTROL_SOURCE) != 0) &&
		 ((pControl->Source.Family != Socket->Family) ||
		  (pControl->Source.Port != 0) ||
		  xrtNetAddrIsUnspecified(&pControl->Source)) ) {
		__xrtNetSocketSetError(
			XERR_ARGUMENT,
			Code,
			sOperation,
			"datagram source address, family or port is invalid"
		);
		return false;
	}
	if ( ((pControl->Flags & XNET_DGRAM_CONTROL_HOP_LIMIT) != 0) &&
		 ((pControl->HopLimit < 0) || (pControl->HopLimit > 255)) ) {
		__xrtNetSocketSetError(
			XERR_RANGE,
			Code,
			sOperation,
			"datagram hop limit is out of range"
		);
		return false;
	}
	if ( ((pControl->Flags & XNET_DGRAM_CONTROL_TRAFFIC_CLASS) != 0) &&
		 ((pControl->TrafficClass < 0) ||
		  (pControl->TrafficClass > 255)) ) {
		__xrtNetSocketSetError(
			XERR_RANGE,
			Code,
			sOperation,
			"datagram traffic class is out of range"
		);
		return false;
	}
	if ( ((pControl->Flags & XNET_DGRAM_CONTROL_SEGMENT_SIZE) != 0) &&
		 ((pControl->SegmentSize == 0) ||
		  (pControl->SegmentSize > 65535u) ||
		  (iPayload == 0) ||
		  (iPayload > ((size_t)pControl->SegmentSize *
			(size_t)XNET_DGRAM_BATCH_MAX))) ) {
		__xrtNetSocketSetError(
			XERR_RANGE,
			Code,
			sOperation,
			"datagram segmentation size or segment count is out of range"
		);
		return false;
	}

	iAvailable = xrtNetSocketDgramControlAvailable(Socket);
	if ( (pControl->Flags & ~iAvailable) != 0 ) {
		__xrtNetSocketSetError(
			XERR_UNSUPPORTED,
			Code,
			sOperation,
			"requested datagram control is unavailable"
		);
		return false;
	}
	return true;
}



/* 校验并构建一次发送使用的全部平台控制消息。 */
bool __xrtNetSocketDgramControlBuild(
	xnetsocket Socket,
	const xnetdgramcontrol* pControl,
	size_t iPayload,
	void* pBuffer,
	size_t iCapacity,
	size_t* pSize,
	xneterror Code,
	cstr sOperation
)
{
	size_t iOffset = 0;
	uint32 iFlags = (pControl != NULL) ? pControl->Flags : 0;
	bool bBuilt = true;

	if ( pSize != NULL ) {
		*pSize = 0;
	}
	if ( (pBuffer == NULL) || (pSize == NULL) ||
		 !__xrtNetSocketDgramControlCheck(
			Socket,
			pControl,
			iPayload,
			Code,
			sOperation
		) ) {
		if ( (pBuffer == NULL) || (pSize == NULL) ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	memset(pBuffer, 0, iCapacity);
	if ( iFlags == 0 ) {
		return true;
	}

	if ( Socket->Family == XNET_FAMILY_IPV4 ) {
		if ( (iFlags & (XNET_DGRAM_CONTROL_SOURCE |
			XNET_DGRAM_CONTROL_INTERFACE)) != 0 ) {
			#if defined(_WIN32) || defined(_WIN64)
				IN_PKTINFO Info;

				memset(&Info, 0, sizeof(Info));
				if ( (iFlags & XNET_DGRAM_CONTROL_SOURCE) != 0 ) {
					memcpy(&Info.ipi_addr, pControl->Source.Address, 4);
				}
				if ( (iFlags & XNET_DGRAM_CONTROL_INTERFACE) != 0 ) {
					Info.ipi_ifindex = pControl->Interface;
				}
				bBuilt = __xrtNetSocketDgramControlAppend(
					pBuffer, iCapacity, &iOffset,
					IPPROTO_IP, IP_PKTINFO, &Info, sizeof(Info)
				);
			#elif defined(__linux__) && defined(IP_PKTINFO)
				unsigned char Info[12];

				memset(Info, 0, sizeof(Info));
				if ( (iFlags & XNET_DGRAM_CONTROL_INTERFACE) != 0 ) {
					memcpy(Info, &pControl->Interface, sizeof(uint32));
				}
				if ( (iFlags & XNET_DGRAM_CONTROL_SOURCE) != 0 ) {
					memcpy(Info + 4, pControl->Source.Address, 4);
				}
				bBuilt = __xrtNetSocketDgramControlAppend(
					pBuffer, iCapacity, &iOffset,
					IPPROTO_IP, IP_PKTINFO, Info, sizeof(Info)
				);
			#elif defined(IP_SENDSRCADDR)
				bBuilt = __xrtNetSocketDgramControlAppend(
					pBuffer, iCapacity, &iOffset,
					IPPROTO_IP, IP_SENDSRCADDR,
					pControl->Source.Address, 4
				);
			#endif
		}
		#if !defined(_WIN32) && !defined(_WIN64)
			#if defined(IP_TTL)
				if ( bBuilt && ((iFlags &
					XNET_DGRAM_CONTROL_HOP_LIMIT) != 0) ) {
					bBuilt = __xrtNetSocketDgramControlAppend(
						pBuffer, iCapacity, &iOffset,
						IPPROTO_IP, IP_TTL,
						&pControl->HopLimit, sizeof(int)
					);
				}
			#endif
			#if defined(IP_TOS)
				if ( bBuilt && ((iFlags &
					XNET_DGRAM_CONTROL_TRAFFIC_CLASS) != 0) ) {
					bBuilt = __xrtNetSocketDgramControlAppend(
						pBuffer, iCapacity, &iOffset,
						IPPROTO_IP, IP_TOS,
						&pControl->TrafficClass, sizeof(int)
					);
				}
			#endif
		#endif
	} else {
		if ( (iFlags & (XNET_DGRAM_CONTROL_SOURCE |
			XNET_DGRAM_CONTROL_INTERFACE)) != 0 ) {
			#if defined(_WIN32) || defined(_WIN64)
				IN6_PKTINFO Info;

				memset(&Info, 0, sizeof(Info));
				if ( (iFlags & XNET_DGRAM_CONTROL_SOURCE) != 0 ) {
					memcpy(&Info.ipi6_addr, pControl->Source.Address, 16);
				}
				Info.ipi6_ifindex =
					((iFlags & XNET_DGRAM_CONTROL_INTERFACE) != 0) ?
					pControl->Interface : pControl->Source.Scope;
				bBuilt = __xrtNetSocketDgramControlAppend(
					pBuffer, iCapacity, &iOffset,
					IPPROTO_IPV6, IPV6_PKTINFO, &Info, sizeof(Info)
				);
			#elif defined(IPV6_PKTINFO)
				unsigned char Info[20];
				uint32 iInterface =
					((iFlags & XNET_DGRAM_CONTROL_INTERFACE) != 0) ?
					pControl->Interface : pControl->Source.Scope;

				memset(Info, 0, sizeof(Info));
				if ( (iFlags & XNET_DGRAM_CONTROL_SOURCE) != 0 ) {
					memcpy(Info, pControl->Source.Address, 16);
				}
				memcpy(Info + 16, &iInterface, sizeof(iInterface));
				bBuilt = __xrtNetSocketDgramControlAppend(
					pBuffer, iCapacity, &iOffset,
					IPPROTO_IPV6, IPV6_PKTINFO, Info, sizeof(Info)
				);
			#endif
		}
		#if !defined(_WIN32) && !defined(_WIN64)
			#if defined(IPV6_HOPLIMIT)
				if ( bBuilt && ((iFlags &
					XNET_DGRAM_CONTROL_HOP_LIMIT) != 0) ) {
					bBuilt = __xrtNetSocketDgramControlAppend(
						pBuffer, iCapacity, &iOffset,
						IPPROTO_IPV6, IPV6_HOPLIMIT,
						&pControl->HopLimit, sizeof(int)
					);
				}
			#endif
			#if defined(IPV6_TCLASS)
				if ( bBuilt && ((iFlags &
					XNET_DGRAM_CONTROL_TRAFFIC_CLASS) != 0) ) {
					bBuilt = __xrtNetSocketDgramControlAppend(
						pBuffer, iCapacity, &iOffset,
						IPPROTO_IPV6, IPV6_TCLASS,
						&pControl->TrafficClass, sizeof(int)
					);
				}
			#endif
		#endif
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( bBuilt && ((iFlags &
			XNET_DGRAM_CONTROL_SEGMENT_SIZE) != 0) ) {
			uint32 iSegment = pControl->SegmentSize;

			bBuilt = __xrtNetSocketDgramControlAppend(
				pBuffer,
				iCapacity,
				&iOffset,
				IPPROTO_UDP,
				UDP_SEND_MSG_SIZE,
				&iSegment,
				sizeof(iSegment)
			);
		}
	#elif defined(__linux__)
		if ( bBuilt && ((iFlags &
			XNET_DGRAM_CONTROL_SEGMENT_SIZE) != 0) ) {
			uint16 iSegment = (uint16)pControl->SegmentSize;

			bBuilt = __xrtNetSocketDgramControlAppend(
				pBuffer,
				iCapacity,
				&iOffset,
				SOL_UDP,
				UDP_SEGMENT,
				&iSegment,
				sizeof(iSegment)
			);
		}
	#endif

	if ( !bBuilt ) {
		__xrtNetSocketSetError(
			XERR_INTERNAL,
			Code,
			sOperation,
			"datagram control buffer is too small"
		);
		return false;
	}
	*pSize = iOffset;
	return true;
}



/* 加入或离开一个 IPv4 或 IPv6 多播组。 */
static bool __xrtNetSocketMulticastMembership(
	xnetsocket Socket,
	const xnetaddr* pGroup,
	const xnetaddr* pInterface,
	bool bJoin
)
{
	int iResult;

	if ( !__xrtNetSocketRequire(
		Socket,
		XNET_ERROR_SOCKET_OPTION,
		bJoin ? "join-multicast" : "leave-multicast"
	) || !__xrtNetSocketRequireType(
		Socket,
		XNET_SOCKET_DGRAM,
		bJoin ? "join-multicast" : "leave-multicast",
		"multicast membership requires a datagram socket"
	) ) {
		return false;
	}
	if ( (pGroup == NULL) ||
		 (pGroup->Family != Socket->Family) ||
		 !xrtNetAddrIsMulticast(pGroup) ||
		 ((pInterface != NULL) &&
		  (pInterface->Family != Socket->Family)) ) {
		__xrtNetSocketSetError(
			XERR_ARGUMENT,
			XNET_ERROR_SOCKET_OPTION,
			bJoin ? "join-multicast" : "leave-multicast",
			"invalid multicast group or interface"
		);
		return false;
	}

	if ( Socket->Family == XNET_FAMILY_IPV4 ) {
		/* 内核 ABI 是两个连续 IPv4 地址，不依赖 libc 特性宏公开 ip_mreq。 */
		struct {
			unsigned char Group[4];
			unsigned char Interface[4];
		} Request;

		memset(&Request, 0, sizeof(Request));
		memcpy(Request.Group, pGroup->Address, 4);
		if ( pInterface != NULL ) {
			memcpy(Request.Interface, pInterface->Address, 4);
		}
		iResult = setsockopt(
			__xrtNetSocketHandle(Socket),
			IPPROTO_IP,
			bJoin ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP,
			(const char*)&Request,
			(socklen_t)sizeof(Request)
		);
	} else {
		/* IPv6 成员参数是 16 字节地址和一个原生接口索引。 */
		struct {
			unsigned char Group[16];
			uint32 Interface;
		} Request;

		memset(&Request, 0, sizeof(Request));
		memcpy(Request.Group, pGroup->Address, 16);
		Request.Interface = pInterface != NULL ?
			pInterface->Scope : 0;
		iResult = setsockopt(
			__xrtNetSocketHandle(Socket),
			IPPROTO_IPV6,
			bJoin ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP,
			(const char*)&Request,
			(socklen_t)sizeof(Request)
		);
	}
	if ( iResult != 0 ) {
		int iCode = __xrtNetSocketLastError();

		__xrtNetSocketSetSystemError(
			XNET_ERROR_SOCKET_OPTION,
			bJoin ? "join-multicast" : "leave-multicast",
			bJoin ? "joining multicast group failed" :
				"leaving multicast group failed",
			iCode
		);
		return false;
	}
	return true;
}



/* 加入一个多播组。 */
XRT_API bool xrtNetSocketMulticastJoin(
	xnetsocket Socket,
	const xnetaddr* pGroup,
	const xnetaddr* pInterface
)
{
	return __xrtNetSocketMulticastMembership(
		Socket,
		pGroup,
		pInterface,
		true
	);
}



/* 离开一个多播组。 */
XRT_API bool xrtNetSocketMulticastLeave(
	xnetsocket Socket,
	const xnetaddr* pGroup,
	const xnetaddr* pInterface
)
{
	return __xrtNetSocketMulticastMembership(
		Socket,
		pGroup,
		pInterface,
		false
	);
}



/* 设置多播回环。 */
XRT_API bool xrtNetSocketMulticastLoop(
	xnetsocket Socket,
	bool bEnabled
)
{
	if ( !__xrtNetSocketRequire(
		Socket,
		XNET_ERROR_SOCKET_OPTION,
		"set-multicast-loop"
	) || !__xrtNetSocketRequireType(
		Socket,
		XNET_SOCKET_DGRAM,
		"set-multicast-loop",
		"multicast loop requires a datagram socket"
	) ) {
		return false;
	}
	if ( Socket->Family == XNET_FAMILY_IPV4 ) {
		uint8 iEnabled = bEnabled ? 1 : 0;

		if ( setsockopt(
			__xrtNetSocketHandle(Socket),
			IPPROTO_IP,
			IP_MULTICAST_LOOP,
			(const char*)&iEnabled,
			(socklen_t)sizeof(iEnabled)
		) == 0 ) {
			return true;
		}
	} else if ( __xrtNetSocketSetInt(
		Socket,
		IPPROTO_IPV6,
		IPV6_MULTICAST_LOOP,
		bEnabled ? 1 : 0
	) ) {
		return true;
	} else {
		return false;
	}
	{
		int iCode = __xrtNetSocketLastError();

		__xrtNetSocketSetSystemError(
			XNET_ERROR_SOCKET_OPTION,
			"set-multicast-loop",
			"setting multicast loop failed",
			iCode
		);
	}
	return false;
}



/* 设置多播跳数。 */
XRT_API bool xrtNetSocketMulticastHopLimit(
	xnetsocket Socket,
	int iHopLimit
)
{
	if ( !__xrtNetSocketRequire(
		Socket,
		XNET_ERROR_SOCKET_OPTION,
		"set-multicast-hop-limit"
	) || !__xrtNetSocketRequireType(
		Socket,
		XNET_SOCKET_DGRAM,
		"set-multicast-hop-limit",
		"multicast hop limit requires a datagram socket"
	) ) {
		return false;
	}
	if ( (iHopLimit < 0) || (iHopLimit > 255) ) {
		__xrtNetSocketSetError(
			XERR_RANGE,
			XNET_ERROR_SOCKET_OPTION,
			"set-multicast-hop-limit",
			"multicast hop limit is out of range"
		);
		return false;
	}
	if ( Socket->Family == XNET_FAMILY_IPV4 ) {
		uint8 iValue = (uint8)iHopLimit;

		if ( setsockopt(
			__xrtNetSocketHandle(Socket),
			IPPROTO_IP,
			IP_MULTICAST_TTL,
			(const char*)&iValue,
			(socklen_t)sizeof(iValue)
		) == 0 ) {
			return true;
		}
	} else if ( __xrtNetSocketSetInt(
		Socket,
		IPPROTO_IPV6,
		IPV6_MULTICAST_HOPS,
		iHopLimit
	) ) {
		return true;
	} else {
		return false;
	}
	{
		int iCode = __xrtNetSocketLastError();

		__xrtNetSocketSetSystemError(
			XNET_ERROR_SOCKET_OPTION,
			"set-multicast-hop-limit",
			"setting multicast hop limit failed",
			iCode
		);
	}
	return false;
}



/* 选择 IPv4 地址或 IPv6 接口索引作为多播发送接口。 */
XRT_API bool xrtNetSocketMulticastInterface(
	xnetsocket Socket,
	const xnetaddr* pInterface
)
{
	int iResult;

	if ( !__xrtNetSocketRequire(
		Socket,
		XNET_ERROR_SOCKET_OPTION,
		"set-multicast-interface"
	) || !__xrtNetSocketRequireType(
		Socket,
		XNET_SOCKET_DGRAM,
		"set-multicast-interface",
		"multicast interface requires a datagram socket"
	) ) {
		return false;
	}
	if ( (pInterface != NULL) &&
		 (pInterface->Family != Socket->Family) ) {
		__xrtNetSocketSetError(
			XERR_ARGUMENT,
			XNET_ERROR_SOCKET_OPTION,
			"set-multicast-interface",
			"multicast interface family does not match the socket"
		);
		return false;
	}
	if ( Socket->Family == XNET_FAMILY_IPV4 ) {
		struct in_addr Address;

		memset(&Address, 0, sizeof(Address));
		if ( pInterface != NULL ) {
			memcpy(&Address, pInterface->Address, 4);
		}
		iResult = setsockopt(
			__xrtNetSocketHandle(Socket),
			IPPROTO_IP,
			IP_MULTICAST_IF,
			(const char*)&Address,
			(socklen_t)sizeof(Address)
		);
	} else {
		uint32 iInterface = pInterface != NULL ?
			pInterface->Scope : 0;

		iResult = setsockopt(
			__xrtNetSocketHandle(Socket),
			IPPROTO_IPV6,
			IPV6_MULTICAST_IF,
			(const char*)&iInterface,
			(socklen_t)sizeof(iInterface)
		);
	}
	if ( iResult != 0 ) {
		int iCode = __xrtNetSocketLastError();

		__xrtNetSocketSetSystemError(
			XNET_ERROR_SOCKET_OPTION,
			"set-multicast-interface",
			"setting multicast interface failed",
			iCode
		);
		return false;
	}
	return true;
}



/* 把 Socket 绑定到本地地址，端口为零时由系统分配端口。 */
XRT_API bool xrtNetSocketBind(xnetsocket Socket, const xnetaddr* pAddress)
{
	struct sockaddr_storage Storage;
	size_t iSize = sizeof(Storage);

	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_BIND, "bind") ) {
		return false;
	}
	if ( !__xrtNetSocketAddress(Socket, pAddress, &Storage, &iSize,
		XNET_ERROR_SOCKET_BIND, "bind") ) {
		return false;
	}
	if ( bind(__xrtNetSocketHandle(Socket),
		(struct sockaddr*)&Storage, (socklen_t)iSize) != 0 ) {
		int iCode = __xrtNetSocketLastError();

		__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_BIND,
			"bind", "binding socket failed", iCode);
		return false;
	}
	return true;
}



/* 把已绑定的流式 Socket 转为监听状态。 */
XRT_API bool xrtNetSocketListen(xnetsocket Socket, int iBacklog)
{
	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_LISTEN, "listen") ) {
		return false;
	}
	if ( (Socket->Type != XNET_SOCKET_STREAM) || (iBacklog <= 0) ) {
		__xrtNetSocketSetError(XERR_ARGUMENT,
			XNET_ERROR_SOCKET_LISTEN, "listen",
			"listen requires a stream socket and positive backlog");
		return false;
	}
	if ( listen(__xrtNetSocketHandle(Socket), iBacklog) != 0 ) {
		int iCode = __xrtNetSocketLastError();

		__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_LISTEN,
			"listen", "listening on socket failed", iCode);
		return false;
	}
	return true;
}



/* 接受一个连接；非阻塞 Socket 暂无连接时返回 AGAIN。 */
XRT_API xnetresult xrtNetSocketAccept(xnetsocket Socket,
	xnetsocket* pClient, xnetaddr* pRemote)
{
	struct sockaddr_storage Storage;
	__xrt_netsocket_native hClient;
	xnetsocket Client;
	xnetaddr Remote;

	#if defined(_WIN32) || defined(_WIN64)
		int iSize = (int)sizeof(Storage);
	#else
		socklen_t iSize = (socklen_t)sizeof(Storage);
	#endif

	if ( pClient != NULL ) {
		*pClient = NULL;
	}
	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_ACCEPT, "accept") || (pClient == NULL) ) {
		if ( pClient == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return XNET_RESULT_ERROR;
	}
	if ( Socket->Type != XNET_SOCKET_STREAM ) {
		__xrtNetSocketSetError(XERR_ARGUMENT,
			XNET_ERROR_SOCKET_ACCEPT, "accept",
			"accept requires a stream socket");
		return XNET_RESULT_ERROR;
	}

	memset(&Storage, 0, sizeof(Storage));
	for ( ;; ) {
		#if defined(__linux__) && defined(SOCK_CLOEXEC) && \
			defined(SYS_accept4)
			int iAcceptFlags = SOCK_CLOEXEC;
			socklen_t iOriginalSize = iSize;

			#if defined(SOCK_NONBLOCK)
				if ( (Socket->Flags & XNET_SOCKET_NONBLOCK) != 0 ) {
					iAcceptFlags |= SOCK_NONBLOCK;
				}
			#endif
			hClient = (__xrt_netsocket_native)syscall(
				SYS_accept4,
				__xrtNetSocketHandle(Socket),
				(struct sockaddr*)&Storage,
				&iSize,
				iAcceptFlags
			);
			if ( (hClient == __XRT_NET_SOCKET_INVALID) &&
				 ((errno == ENOSYS) || (errno == EINVAL)) ) {
				iSize = iOriginalSize;
				hClient = accept(__xrtNetSocketHandle(Socket),
					(struct sockaddr*)&Storage, &iSize);
			}
		#else
			hClient = accept(__xrtNetSocketHandle(Socket),
				(struct sockaddr*)&Storage, &iSize);
		#endif
		if ( hClient != __XRT_NET_SOCKET_INVALID ) {
			break;
		}
		{
			int iCode = __xrtNetSocketLastError();

			#if !defined(_WIN32) && !defined(_WIN64)
				if ( iCode == EINTR ) {
					continue;
				}
			#endif
			if ( __xrtNetSocketWouldBlock(iCode) ) {
				return XNET_RESULT_AGAIN;
			}
			__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_ACCEPT,
				"accept", "accepting socket failed", iCode);
			return XNET_RESULT_ERROR;
		}
	}

	if ( pRemote != NULL ) {
		if ( !xrtNetAddrFromNative(&Remote, &Storage, (size_t)iSize) ) {
			(void)__xrtNetSocketCloseNative(hClient);
			return XNET_RESULT_ERROR;
		}
	}
	Client = __xrtNetSocketAdopt((uintptr_t)hClient, Socket->Family,
		XNET_SOCKET_STREAM, Socket->Flags & XNET_SOCKET_NONBLOCK);
	if ( Client == NULL ) {
		return XNET_RESULT_ERROR;
	}

	*pClient = Client;
	if ( pRemote != NULL ) {
		*pRemote = Remote;
	}
	return XNET_RESULT_OK;
}



/* 发起连接；非阻塞连接尚未完成时返回 AGAIN。 */
XRT_API xnetresult xrtNetSocketConnect(xnetsocket Socket,
	const xnetaddr* pRemote)
{
	struct sockaddr_storage Storage;
	size_t iSize = sizeof(Storage);
	int iResult;

	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_CONNECT, "connect") ) {
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtNetSocketAddress(Socket, pRemote, &Storage, &iSize,
		XNET_ERROR_SOCKET_CONNECT, "connect") ) {
		return XNET_RESULT_ERROR;
	}

	iResult = connect(__xrtNetSocketHandle(Socket),
		(struct sockaddr*)&Storage, (socklen_t)iSize);
	if ( iResult == 0 ) {
		Socket->Connecting = false;
		return XNET_RESULT_OK;
	}
	{
		int iCode = __xrtNetSocketLastError();

		if ( __xrtNetSocketIsConnected(iCode) ) {
			Socket->Connecting = false;
			return XNET_RESULT_OK;
		}
		if ( __xrtNetSocketWouldBlock(iCode) ) {
			Socket->Connecting = true;
			return XNET_RESULT_AGAIN;
		}
		#if !defined(_WIN32) && !defined(_WIN64)
			if ( iCode == EINTR ) {
				Socket->Connecting = true;
				return XNET_RESULT_AGAIN;
			}
		#endif
		Socket->Connecting = false;
		__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_CONNECT,
			"connect", "connecting socket failed", iCode);
		return XNET_RESULT_ERROR;
	}
}



/* 在可写事件到达后读取 SO_ERROR，完成非阻塞连接判定。 */
XRT_API xnetresult xrtNetSocketFinishConnect(xnetsocket Socket)
{
	int iCode = 0;
	int iResult;

	#if defined(_WIN32) || defined(_WIN64)
		int iSize = (int)sizeof(iCode);
	#else
		socklen_t iSize = (socklen_t)sizeof(iCode);
	#endif

	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_CONNECT, "finish-connect") ) {
		return XNET_RESULT_ERROR;
	}
	if ( !Socket->Connecting ) {
		__xrtNetSocketSetError(XERR_ARGUMENT,
			XNET_ERROR_SOCKET_CONNECT, "finish-connect",
			"socket has no pending connection");
		return XNET_RESULT_ERROR;
	}
	iResult = getsockopt(__xrtNetSocketHandle(Socket),
		SOL_SOCKET, SO_ERROR, (char*)&iCode, &iSize);
	if ( iResult != 0 ) {
		int iSystemCode = __xrtNetSocketLastError();

		__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_CONNECT,
			"finish-connect", "querying socket connection failed", iSystemCode);
		Socket->Connecting = false;
		return XNET_RESULT_ERROR;
	}
	if ( iCode == 0 ) {
		Socket->Connecting = false;
		return XNET_RESULT_OK;
	}
	if ( __xrtNetSocketWouldBlock(iCode) ) {
		return XNET_RESULT_AGAIN;
	}
	Socket->Connecting = false;
	__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_CONNECT,
		"finish-connect", "socket connection failed", iCode);
	return XNET_RESULT_ERROR;
}



/* 半关闭指定方向，不销毁 Socket 对象。 */
XRT_API bool xrtNetSocketShutdown(xnetsocket Socket, xnetshutdown Direction)
{
	int iDirection;

	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_SHUTDOWN, "shutdown") ) {
		return false;
	}
	if ( Direction == XNET_SHUTDOWN_READ ) {
		#if defined(_WIN32) || defined(_WIN64)
			iDirection = SD_RECEIVE;
		#else
			iDirection = SHUT_RD;
		#endif
	} else if ( Direction == XNET_SHUTDOWN_WRITE ) {
		#if defined(_WIN32) || defined(_WIN64)
			iDirection = SD_SEND;
		#else
			iDirection = SHUT_WR;
		#endif
	} else if ( Direction == XNET_SHUTDOWN_BOTH ) {
		#if defined(_WIN32) || defined(_WIN64)
			iDirection = SD_BOTH;
		#else
			iDirection = SHUT_RDWR;
		#endif
	} else {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	if ( shutdown(__xrtNetSocketHandle(Socket), iDirection) != 0 ) {
		int iCode = __xrtNetSocketLastError();

		__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_SHUTDOWN,
			"shutdown", "shutting down socket failed", iCode);
		return false;
	}
	return true;
}



/* 查询一个 Socket 地址，成功才修改输出。 */
static bool __xrtNetSocketName(xnetsocket Socket,
	xnetaddr* pAddress, bool bRemote)
{
	struct sockaddr_storage Storage;
	xnetaddr Address;
	int iResult;

	#if defined(_WIN32) || defined(_WIN64)
		int iSize = (int)sizeof(Storage);
	#else
		socklen_t iSize = (socklen_t)sizeof(Storage);
	#endif

	if ( !__xrtNetSocketRequire(Socket, XNET_ERROR_NATIVE,
		bRemote ? "remote-address" : "local-address") ||
		 (pAddress == NULL) ) {
		if ( pAddress == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}

	iResult = bRemote ?
		getpeername(__xrtNetSocketHandle(Socket),
			(struct sockaddr*)&Storage, &iSize) :
		getsockname(__xrtNetSocketHandle(Socket),
			(struct sockaddr*)&Storage, &iSize);
	if ( iResult != 0 ) {
		int iCode = __xrtNetSocketLastError();

		__xrtNetSocketSetSystemError(XNET_ERROR_NATIVE,
			bRemote ? "remote-address" : "local-address",
			"querying socket address failed", iCode);
		return false;
	}
	if ( !xrtNetAddrFromNative(&Address, &Storage, (size_t)iSize) ) {
		return false;
	}
	*pAddress = Address;
	return true;
}



/* 查询实际本地地址，成功才修改输出。 */
XRT_API bool xrtNetSocketLocal(xnetsocket Socket, xnetaddr* pAddress)
{
	return __xrtNetSocketName(Socket, pAddress, false);
}



/* 查询已连接的对端地址，成功才修改输出。 */
XRT_API bool xrtNetSocketRemote(xnetsocket Socket, xnetaddr* pAddress)
{
	return __xrtNetSocketName(Socket, pAddress, true);
}



/* 判断平台错误是否表示数据报已被接收缓冲截断。 */
static bool __xrtNetSocketMessageTooLarge(int iCode)
{
	#if defined(_WIN32) || defined(_WIN64)
		return iCode == WSAEMSGSIZE;
	#else
		return iCode == EMSGSIZE;
	#endif
}



/* 处理接收失败，并把非阻塞和数据报截断保留为正常控制结果。 */
static xnetresult __xrtNetSocketRecvError(int iCode,
	size_t iTruncated, size_t* pReceived, bool bDatagram, cstr sOperation)
{
	if ( __xrtNetSocketWouldBlock(iCode) ) {
		return XNET_RESULT_AGAIN;
	}
	if ( bDatagram && __xrtNetSocketMessageTooLarge(iCode) ) {
		*pReceived = iTruncated;
		return XNET_RESULT_TRUNCATED;
	}
	__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_READ,
		sOperation, "receiving socket data failed", iCode);
	return XNET_RESULT_ERROR;
}



/* 处理接收系统调用的统一结果。 */
static xnetresult __xrtNetSocketRecvResult(int iResult,
	size_t* pReceived, bool bDatagram, cstr sOperation)
{
	if ( iResult > 0 ) {
		*pReceived = (size_t)iResult;
		return XNET_RESULT_OK;
	}
	if ( iResult == 0 ) {
		return bDatagram ? XNET_RESULT_OK : XNET_RESULT_CLOSED;
	}
	{
		int iCode = __xrtNetSocketLastError();

		return __xrtNetSocketRecvError(iCode, 0,
			pReceived, bDatagram, sOperation);
	}
}



/* 处理发送系统调用的统一结果。 */
static xnetresult __xrtNetSocketSendResult(int iResult,
	size_t* pSent, cstr sOperation)
{
	if ( iResult >= 0 ) {
		*pSent = (size_t)iResult;
		return XNET_RESULT_OK;
	}
	{
		int iCode = __xrtNetSocketLastError();

		if ( __xrtNetSocketWouldBlock(iCode) ) {
			return XNET_RESULT_AGAIN;
		}
		__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_WRITE,
			sOperation, "sending socket data failed", iCode);
		return XNET_RESULT_ERROR;
	}
}



/* 单次接收；流式 Socket 正常 EOF 返回 CLOSED，非阻塞无数据返回 AGAIN。 */
XRT_API xnetresult xrtNetSocketRecv(xnetsocket Socket,
	void* pData, size_t iSize, size_t* pReceived)
{
	int iResult;

	if ( pReceived != NULL ) {
		*pReceived = 0;
	}
	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_READ, "recv") || (pReceived == NULL) ||
		 !__xrtNetSocketBuffer(pData, iSize,
			XNET_ERROR_SOCKET_READ, "recv") ) {
		if ( pReceived == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return XNET_RESULT_ERROR;
	}
	if ( Socket->Type == XNET_SOCKET_DGRAM ) {
		return xrtNetSocketRecvFrom(Socket,
			pData, iSize, pReceived, NULL);
	}
	if ( iSize == 0 ) {
		return XNET_RESULT_OK;
	}

	do {
		iResult = recv(__xrtNetSocketHandle(Socket),
			(char*)pData, (int)iSize, 0);
	#if !defined(_WIN32) && !defined(_WIN64)
	} while ( (iResult < 0) && (errno == EINTR) );
	#else
	} while ( false );
	#endif
	return __xrtNetSocketRecvResult(iResult, pReceived, false, "recv");
}



/* 单次发送；允许成功短写，非阻塞无法推进时返回 AGAIN。 */
XRT_API xnetresult xrtNetSocketSend(xnetsocket Socket,
	const void* pData, size_t iSize, size_t* pSent)
{
	unsigned char iDummy = 0;
	int iResult;
	int iFlags = 0;

	if ( pSent != NULL ) {
		*pSent = 0;
	}
	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_WRITE, "send") || (pSent == NULL) ||
		 !__xrtNetSocketBuffer(pData, iSize,
			XNET_ERROR_SOCKET_WRITE, "send") ) {
		if ( pSent == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return XNET_RESULT_ERROR;
	}
	if ( (iSize == 0) && (Socket->Type == XNET_SOCKET_STREAM) ) {
		return XNET_RESULT_OK;
	}

	#if defined(MSG_NOSIGNAL)
		iFlags = MSG_NOSIGNAL;
	#endif
	do {
		iResult = send(__xrtNetSocketHandle(Socket),
			(const char*)((pData != NULL) ? pData : &iDummy),
			(int)iSize, iFlags);
	#if !defined(_WIN32) && !defined(_WIN64)
	} while ( (iResult < 0) && (errno == EINTR) );
	#else
	} while ( false );
	#endif
	return __xrtNetSocketSendResult(iResult, pSent, "send");
}



/* 校验向量并计算总长度，避免描述符和长度溢出。 */
static bool __xrtNetSocketCheckVec(const void* pSpans,
	size_t iCount, bool bWritable, size_t* pTotal, cstr sOperation)
{
	size_t i;
	size_t iTotal = 0;

	if ( (iCount > XRT_NET_SOCKET_VECTOR_LIMIT) ||
		 ((pSpans == NULL) && (iCount != 0)) ) {
		__xrtNetSocketSetError(XERR_ARGUMENT,
			bWritable ? XNET_ERROR_SOCKET_READ : XNET_ERROR_SOCKET_WRITE,
			sOperation, "invalid socket vector");
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		const void* pData;
		size_t iSize;

		if ( bWritable ) {
			const xnetwspan* pVec = (const xnetwspan*)pSpans;

			pData = pVec[i].Data;
			iSize = pVec[i].Size;
		} else {
			const xnetspan* pVec = (const xnetspan*)pSpans;

			pData = pVec[i].Data;
			iSize = pVec[i].Size;
		}
		if ( ((pData == NULL) && (iSize != 0)) ||
			 (iSize > (size_t)INT_MAX) ||
			 (iTotal > (SIZE_MAX - iSize)) ) {
			__xrtNetSocketSetError(XERR_RANGE,
				bWritable ? XNET_ERROR_SOCKET_READ : XNET_ERROR_SOCKET_WRITE,
				sOperation, "socket vector size is out of range");
			return false;
		}
		iTotal += iSize;
	}
	if ( iTotal > (size_t)INT_MAX ) {
		__xrtNetSocketSetError(XERR_RANGE,
			bWritable ? XNET_ERROR_SOCKET_READ : XNET_ERROR_SOCKET_WRITE,
			sOperation, "socket vector total is out of range");
		return false;
	}
	*pTotal = iTotal;
	return true;
}



#if defined(_WIN32) || defined(_WIN64)

/* 构造 Windows 可写向量描述符。 */
static void __xrtNetSocketBuildReadVec(WSABUF* pNative,
	const xnetwspan* pSpans, size_t iCount)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		pNative[i].buf = (char*)pSpans[i].Data;
		pNative[i].len = (ULONG)pSpans[i].Size;
	}
}



/* 构造 Windows 只读向量描述符。 */
static void __xrtNetSocketBuildWriteVec(WSABUF* pNative,
	const xnetspan* pSpans, size_t iCount)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		pNative[i].buf = (char*)pSpans[i].Data;
		pNative[i].len = (ULONG)pSpans[i].Size;
	}
}

#else

/* 构造 POSIX 可写向量描述符。 */
static void __xrtNetSocketBuildReadVec(struct iovec* pNative,
	const xnetwspan* pSpans, size_t iCount)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		pNative[i].iov_base = pSpans[i].Data;
		pNative[i].iov_len = pSpans[i].Size;
	}
}



/* 构造 POSIX 只读向量描述符。 */
static void __xrtNetSocketBuildWriteVec(struct iovec* pNative,
	const xnetspan* pSpans, size_t iCount)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		pNative[i].iov_base = (void*)pSpans[i].Data;
		pNative[i].iov_len = pSpans[i].Size;
	}
}

#endif



/* 单次分散接收，Span 数量不能超过 64。 */
XRT_API xnetresult xrtNetSocketRecvVec(xnetsocket Socket,
	xnetwspan* pSpans, size_t iCount, size_t* pReceived)
{
	size_t iTotal;
	int iResult;

	if ( pReceived != NULL ) {
		*pReceived = 0;
	}
	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_READ, "recv-vec") || (pReceived == NULL) ||
		 !__xrtNetSocketCheckVec(pSpans, iCount, true,
			&iTotal, "recv-vec") ) {
		if ( pReceived == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return XNET_RESULT_ERROR;
	}
	if ( Socket->Type == XNET_SOCKET_DGRAM ) {
		return xrtNetSocketRecvFromVec(Socket,
			pSpans, iCount, pReceived, NULL);
	}
	if ( iTotal == 0 ) {
		return XNET_RESULT_OK;
	}

	#if defined(_WIN32) || defined(_WIN64)
		WSABUF Native[XRT_NET_SOCKET_VECTOR_LIMIT];
		DWORD iBytes = 0;
		DWORD iFlags = 0;

		__xrtNetSocketBuildReadVec(Native, pSpans, iCount);
		iResult = WSARecv(__xrtNetSocketHandle(Socket), Native,
			(DWORD)iCount, &iBytes, &iFlags, NULL, NULL);
		if ( iResult == 0 ) {
			iResult = (int)iBytes;
		}
	#else
		struct iovec Native[XRT_NET_SOCKET_VECTOR_LIMIT];
		struct msghdr Message;
		ssize_t iBytes;

		__xrtNetSocketBuildReadVec(Native, pSpans, iCount);
		memset(&Message, 0, sizeof(Message));
		Message.msg_iov = Native;
		Message.msg_iovlen = iCount;
		do {
			iBytes = recvmsg(__xrtNetSocketHandle(Socket), &Message, 0);
		} while ( (iBytes < 0) && (errno == EINTR) );
		iResult = (iBytes > INT_MAX) ? INT_MAX : (int)iBytes;
	#endif
	return __xrtNetSocketRecvResult(iResult, pReceived, false, "recv-vec");
}



/* 单次聚集发送，Span 数量不能超过 64。 */
XRT_API xnetresult xrtNetSocketSendVec(xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, size_t* pSent)
{
	size_t iTotal;
	int iResult;

	if ( pSent != NULL ) {
		*pSent = 0;
	}
	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_WRITE, "send-vec") || (pSent == NULL) ||
		 !__xrtNetSocketCheckVec(pSpans, iCount, false,
			&iTotal, "send-vec") ) {
		if ( pSent == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return XNET_RESULT_ERROR;
	}
	if ( (iTotal == 0) && (Socket->Type == XNET_SOCKET_DGRAM) ) {
		return xrtNetSocketSend(Socket, NULL, 0, pSent);
	}
	if ( iTotal == 0 ) {
		return XNET_RESULT_OK;
	}

	#if defined(_WIN32) || defined(_WIN64)
		WSABUF Native[XRT_NET_SOCKET_VECTOR_LIMIT];
		DWORD iBytes = 0;

		__xrtNetSocketBuildWriteVec(Native, pSpans, iCount);
		iResult = WSASend(__xrtNetSocketHandle(Socket), Native,
			(DWORD)iCount, &iBytes, 0, NULL, NULL);
		if ( iResult == 0 ) {
			iResult = (int)iBytes;
		}
	#else
		struct iovec Native[XRT_NET_SOCKET_VECTOR_LIMIT];
		struct msghdr Message;
		ssize_t iBytes;
		int iFlags = 0;

		__xrtNetSocketBuildWriteVec(Native, pSpans, iCount);
		memset(&Message, 0, sizeof(Message));
		Message.msg_iov = Native;
		Message.msg_iovlen = iCount;
		#if defined(MSG_NOSIGNAL)
			iFlags = MSG_NOSIGNAL;
		#endif
		do {
			iBytes = sendmsg(__xrtNetSocketHandle(Socket), &Message, iFlags);
		} while ( (iBytes < 0) && (errno == EINTR) );
		iResult = (iBytes > INT_MAX) ? INT_MAX : (int)iBytes;
	#endif
	return __xrtNetSocketSendResult(iResult, pSent, "send-vec");
}



/* 单次接收数据报；零长度数据报仍返回 OK。 */
XRT_API xnetresult xrtNetSocketRecvFrom(xnetsocket Socket,
	void* pData, size_t iSize, size_t* pReceived, xnetaddr* pRemote)
{
	struct sockaddr_storage Storage;
	unsigned char iDummy = 0;
	int iResult;
	xnetresult Result;

	#if defined(_WIN32) || defined(_WIN64)
		int iAddressSize = (int)sizeof(Storage);
	#else
		socklen_t iAddressSize = (socklen_t)sizeof(Storage);
	#endif

	if ( pReceived != NULL ) {
		*pReceived = 0;
	}
	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_READ, "recv-from") || (pReceived == NULL) ||
		 !__xrtNetSocketBuffer(pData, iSize,
			XNET_ERROR_SOCKET_READ, "recv-from") ) {
		if ( pReceived == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return XNET_RESULT_ERROR;
	}
	if ( Socket->Type != XNET_SOCKET_DGRAM ) {
		__xrtNetSocketSetError(XERR_ARGUMENT,
			XNET_ERROR_SOCKET_READ, "recv-from",
			"recv-from requires a datagram socket");
		return XNET_RESULT_ERROR;
	}

	#if defined(_WIN32) || defined(_WIN64)
		{
			WSABUF Buffer;
			DWORD iBytes = 0;
			DWORD iFlags = 0;

			Buffer.buf = (char*)((pData != NULL) ? pData : &iDummy);
			Buffer.len = (ULONG)iSize;
			iResult = WSARecvFrom(__xrtNetSocketHandle(Socket),
				&Buffer, 1, &iBytes, &iFlags, (struct sockaddr*)&Storage,
				&iAddressSize, NULL, NULL);
			if ( iResult == 0 ) {
				*pReceived = (size_t)iBytes;
				Result = XNET_RESULT_OK;
			} else {
				int iCode = __xrtNetSocketLastError();

				Result = __xrtNetSocketRecvError(iCode,
					(size_t)iBytes, pReceived, true, "recv-from");
			}
		}
	#else
		{
			struct iovec Buffer;
			struct msghdr Message;
			ssize_t iBytes;

			Buffer.iov_base = (pData != NULL) ? pData : &iDummy;
			Buffer.iov_len = iSize;
			memset(&Message, 0, sizeof(Message));
			Message.msg_name = &Storage;
			Message.msg_namelen = iAddressSize;
			Message.msg_iov = &Buffer;
			Message.msg_iovlen = 1;
			do {
				iBytes = recvmsg(__xrtNetSocketHandle(Socket), &Message, 0);
			} while ( (iBytes < 0) && (errno == EINTR) );
			iAddressSize = Message.msg_namelen;
			iResult = (iBytes > INT_MAX) ? INT_MAX : (int)iBytes;
			Result = __xrtNetSocketRecvResult(iResult,
				pReceived, true, "recv-from");
			#if defined(MSG_TRUNC)
				if ( (Result == XNET_RESULT_OK) &&
					 ((Message.msg_flags & MSG_TRUNC) != 0) ) {
					Result = XNET_RESULT_TRUNCATED;
				}
			#endif
		}
	#endif

	if ( ((Result == XNET_RESULT_OK) ||
		 (Result == XNET_RESULT_TRUNCATED)) && (pRemote != NULL) &&
		 !xrtNetAddrFromNative(pRemote, &Storage, (size_t)iAddressSize) ) {
		*pReceived = 0;
		return XNET_RESULT_ERROR;
	}
	return Result;
}



/* 单次发送数据报；允许发送零长度数据报。 */
XRT_API xnetresult xrtNetSocketSendTo(xnetsocket Socket,
	const void* pData, size_t iSize, size_t* pSent, const xnetaddr* pRemote)
{
	struct sockaddr_storage Storage;
	unsigned char iDummy = 0;
	size_t iAddressSize = sizeof(Storage);
	int iResult;
	int iFlags = 0;

	if ( pSent != NULL ) {
		*pSent = 0;
	}
	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_WRITE, "send-to") || (pSent == NULL) ||
		 !__xrtNetSocketBuffer(pData, iSize,
			XNET_ERROR_SOCKET_WRITE, "send-to") ) {
		if ( pSent == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return XNET_RESULT_ERROR;
	}
	if ( Socket->Type != XNET_SOCKET_DGRAM ) {
		__xrtNetSocketSetError(XERR_ARGUMENT,
			XNET_ERROR_SOCKET_WRITE, "send-to",
			"send-to requires a datagram socket");
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtNetSocketAddress(Socket, pRemote, &Storage,
		&iAddressSize, XNET_ERROR_SOCKET_WRITE, "send-to") ) {
		return XNET_RESULT_ERROR;
	}

	#if defined(MSG_NOSIGNAL)
		iFlags = MSG_NOSIGNAL;
	#endif
	do {
		iResult = sendto(__xrtNetSocketHandle(Socket),
			(const char*)((pData != NULL) ? pData : &iDummy), (int)iSize,
			iFlags, (struct sockaddr*)&Storage, (socklen_t)iAddressSize);
	#if !defined(_WIN32) && !defined(_WIN64)
	} while ( (iResult < 0) && (errno == EINTR) );
	#else
	} while ( false );
	#endif
	return __xrtNetSocketSendResult(iResult, pSent, "send-to");
}



/* 单次分散接收数据报，Span 数量不能超过 64。 */
XRT_API xnetresult xrtNetSocketRecvFromVec(xnetsocket Socket,
	xnetwspan* pSpans, size_t iCount, size_t* pReceived, xnetaddr* pRemote)
{
	struct sockaddr_storage Storage;
	size_t iTotal;
	int iResult;
	xnetresult Result;

	if ( pReceived != NULL ) {
		*pReceived = 0;
	}
	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_READ, "recv-from-vec") || (pReceived == NULL) ||
		 !__xrtNetSocketCheckVec(pSpans, iCount, true,
			&iTotal, "recv-from-vec") ) {
		if ( pReceived == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return XNET_RESULT_ERROR;
	}
	if ( Socket->Type != XNET_SOCKET_DGRAM ) {
		__xrtNetSocketSetError(XERR_ARGUMENT,
			XNET_ERROR_SOCKET_READ, "recv-from-vec",
			"recv-from-vec requires a datagram socket");
		return XNET_RESULT_ERROR;
	}
	if ( iTotal == 0 ) {
		return xrtNetSocketRecvFrom(Socket,
			NULL, 0, pReceived, pRemote);
	}

	#if defined(_WIN32) || defined(_WIN64)
		WSABUF Native[XRT_NET_SOCKET_VECTOR_LIMIT];
		DWORD iBytes = 0;
		DWORD iFlags = 0;
		int iAddressSize = (int)sizeof(Storage);

		__xrtNetSocketBuildReadVec(Native, pSpans, iCount);
		iResult = WSARecvFrom(__xrtNetSocketHandle(Socket), Native,
			(DWORD)iCount, &iBytes, &iFlags, (struct sockaddr*)&Storage,
			&iAddressSize, NULL, NULL);
		if ( iResult == 0 ) {
			*pReceived = (size_t)iBytes;
			Result = XNET_RESULT_OK;
		} else {
			int iCode = __xrtNetSocketLastError();

			Result = __xrtNetSocketRecvError(iCode,
				(size_t)iBytes, pReceived, true, "recv-from-vec");
		}
	#else
		struct iovec Native[XRT_NET_SOCKET_VECTOR_LIMIT];
		struct msghdr Message;
		ssize_t iBytes;
		socklen_t iAddressSize;

		__xrtNetSocketBuildReadVec(Native, pSpans, iCount);
		memset(&Message, 0, sizeof(Message));
		Message.msg_name = &Storage;
		Message.msg_namelen = (socklen_t)sizeof(Storage);
		Message.msg_iov = Native;
		Message.msg_iovlen = iCount;
		do {
			iBytes = recvmsg(__xrtNetSocketHandle(Socket), &Message, 0);
		} while ( (iBytes < 0) && (errno == EINTR) );
		iAddressSize = Message.msg_namelen;
		iResult = (iBytes > INT_MAX) ? INT_MAX : (int)iBytes;
		Result = __xrtNetSocketRecvResult(iResult,
			pReceived, true, "recv-from-vec");
		#if defined(MSG_TRUNC)
			if ( (Result == XNET_RESULT_OK) &&
				 ((Message.msg_flags & MSG_TRUNC) != 0) ) {
				Result = XNET_RESULT_TRUNCATED;
			}
		#endif
	#endif

	if ( ((Result == XNET_RESULT_OK) ||
		 (Result == XNET_RESULT_TRUNCATED)) && (pRemote != NULL) &&
		 !xrtNetAddrFromNative(pRemote, &Storage, (size_t)iAddressSize) ) {
		*pReceived = 0;
		return XNET_RESULT_ERROR;
	}
	return Result;
}



/* 接收一个连续数据报及其已启用元数据。 */
XRT_API xnetresult xrtNetSocketRecvMsg(
	xnetsocket Socket,
	void* pData,
	size_t iSize,
	size_t* pReceived,
	xnetaddr* pRemote,
	xnetdgrammeta* pMeta
)
{
	xnetwspan Span;

	Span.Data = (bytes)pData;
	Span.Size = iSize;
	return xrtNetSocketRecvMsgVec(
		Socket,
		&Span,
		1,
		pReceived,
		pRemote,
		pMeta
	);
}



/* 分散接收数据报，并从同一次系统调用解析控制消息。 */
XRT_API xnetresult xrtNetSocketRecvMsgVec(
	xnetsocket Socket,
	xnetwspan* pSpans,
	size_t iCount,
	size_t* pReceived,
	xnetaddr* pRemote,
	xnetdgrammeta* pMeta
)
{
	struct sockaddr_storage Storage;
	__xrt_net_dgram_control Control;
	size_t iTotal;
	xnetresult Result;

	if ( pReceived != NULL ) {
		*pReceived = 0;
	}
	if ( pMeta != NULL ) {
		memset(pMeta, 0, sizeof(*pMeta));
	}
	if ( !__xrtNetSocketRequire(
		Socket,
		XNET_ERROR_SOCKET_READ,
		"recv-message"
	) || (pReceived == NULL) || (pMeta == NULL) ||
		 !__xrtNetSocketCheckVec(
			pSpans,
			iCount,
			true,
			&iTotal,
			"recv-message"
		) ) {
		if ( (pReceived == NULL) || (pMeta == NULL) ) {
			__xrtErrorSetInvalidArgument();
		}
		return XNET_RESULT_ERROR;
	}
	if ( Socket->Type != XNET_SOCKET_DGRAM ) {
		__xrtNetSocketSetError(
			XERR_ARGUMENT,
			XNET_ERROR_SOCKET_READ,
			"recv-message",
			"recv-message requires a datagram socket"
		);
		return XNET_RESULT_ERROR;
	}
	if ( Socket->DgramMeta == 0 ) {
		return xrtNetSocketRecvFromVec(
			Socket,
			pSpans,
			iCount,
			pReceived,
			pRemote
		);
	}

	memset(&Storage, 0, sizeof(Storage));
	memset(&Control, 0, sizeof(Control));
	#if defined(_WIN32) || defined(_WIN64)
		WSABUF Native[XRT_NET_SOCKET_VECTOR_LIMIT];
		WSAMSG Message;
		LPFN_WSARECVMSG pReceive = NULL;
		uintptr_t iReceive = __xrtNetSocketReceiveMessage(Socket);
		DWORD iBytes = 0;
		int iResult;

		if ( iReceive == 0 ) {
			return XNET_RESULT_ERROR;
		}
		memcpy(&pReceive, &iReceive, sizeof(pReceive));
		__xrtNetSocketBuildReadVec(Native, pSpans, iCount);
		memset(&Message, 0, sizeof(Message));
		Message.name = (struct sockaddr*)&Storage;
		Message.namelen = (int)sizeof(Storage);
		Message.lpBuffers = Native;
		Message.dwBufferCount = (DWORD)iCount;
		Message.Control.buf = (CHAR*)Control.Data;
		Message.Control.len = (ULONG)sizeof(Control.Data);
		iResult = pReceive(
			__xrtNetSocketHandle(Socket),
			&Message,
			&iBytes,
			NULL,
			NULL
		);
		if ( iResult == 0 ) {
			*pReceived = (size_t)iBytes;
			Result = XNET_RESULT_OK;
		} else {
			int iCode = __xrtNetSocketLastError();

			Result = __xrtNetSocketRecvError(
				iCode,
				(size_t)iBytes,
				pReceived,
				true,
				"recv-message"
			);
		}
		if ( (Result == XNET_RESULT_OK) &&
			 ((Message.dwFlags & MSG_TRUNC) != 0) ) {
			Result = XNET_RESULT_TRUNCATED;
		}
		if ( (Result == XNET_RESULT_OK) ||
			 (Result == XNET_RESULT_TRUNCATED) ) {
			__xrtNetSocketDgramMetaParse(
				Socket,
				pMeta,
				Message.Control.buf,
				(size_t)Message.Control.len,
				(uint32)Message.dwFlags
			);
			if ( (pRemote != NULL) && !xrtNetAddrFromNative(
				pRemote,
				&Storage,
				(size_t)Message.namelen
			) ) {
				*pReceived = 0;
				memset(pMeta, 0, sizeof(*pMeta));
				return XNET_RESULT_ERROR;
			}
		}
	#else
		struct iovec Native[XRT_NET_SOCKET_VECTOR_LIMIT];
		struct msghdr Message;
		ssize_t iBytes;
		int iResult;

		__xrtNetSocketBuildReadVec(Native, pSpans, iCount);
		memset(&Message, 0, sizeof(Message));
		Message.msg_name = &Storage;
		Message.msg_namelen = (socklen_t)sizeof(Storage);
		Message.msg_iov = Native;
		Message.msg_iovlen = iCount;
		Message.msg_control = Control.Data;
		Message.msg_controllen = sizeof(Control.Data);
		do {
			iBytes = recvmsg(__xrtNetSocketHandle(Socket), &Message, 0);
		} while ( (iBytes < 0) && (errno == EINTR) );
		iResult = (iBytes > INT_MAX) ? INT_MAX : (int)iBytes;
		Result = __xrtNetSocketRecvResult(
			iResult,
			pReceived,
			true,
			"recv-message"
		);
		#if defined(MSG_TRUNC)
			if ( (Result == XNET_RESULT_OK) &&
				 ((Message.msg_flags & MSG_TRUNC) != 0) ) {
				Result = XNET_RESULT_TRUNCATED;
			}
		#endif
		if ( (Result == XNET_RESULT_OK) ||
			 (Result == XNET_RESULT_TRUNCATED) ) {
			__xrtNetSocketDgramMetaParse(
				Socket,
				pMeta,
				Message.msg_control,
				Message.msg_controllen,
				(uint32)Message.msg_flags
			);
			if ( (pRemote != NULL) && !xrtNetAddrFromNative(
				pRemote,
				&Storage,
				(size_t)Message.msg_namelen
			) ) {
				*pReceived = 0;
				memset(pMeta, 0, sizeof(*pMeta));
				return XNET_RESULT_ERROR;
			}
		}
	#endif
	return Result;
}



#if defined(__linux__)

/* 把错误队列地址转换为稳定地址；空地址不产生字段。 */
static bool __xrtNetSocketDgramErrorAddress(
	xnetaddr* pTarget,
	uint32* pFlags,
	uint32 iFlag,
	const struct sockaddr* pAddress,
	size_t iSize
)
{
	size_t iRequired;

	if ( (pAddress == NULL) || (iSize < sizeof(pAddress->sa_family)) ||
		 (pAddress->sa_family == AF_UNSPEC) ) {
		return true;
	}
	if ( pAddress->sa_family == AF_INET ) {
		iRequired = sizeof(struct sockaddr_in);
	} else if ( pAddress->sa_family == AF_INET6 ) {
		iRequired = sizeof(struct sockaddr_in6);
	} else {
		return true;
	}
	if ( iSize < iRequired ) {
		return true;
	}
	if ( !xrtNetAddrFromNative(pTarget, pAddress, iRequired) ) {
		return false;
	}
	*pFlags |= iFlag;
	return true;
}



/* 解析 Linux IP_RECVERR/IPV6_RECVERR 控制消息。 */
static bool __xrtNetSocketDgramErrorParse(
	xnetdgramerror* pError,
	const struct msghdr* pMessage
)
{
	struct cmsghdr* pHeader;

	if ( !__xrtNetSocketDgramErrorAddress(
		&pError->Remote,
		&pError->Flags,
		XNET_DGRAM_ERROR_REMOTE,
		(const struct sockaddr*)pMessage->msg_name,
		(size_t)pMessage->msg_namelen
	) ) {
		return false;
	}
	for ( pHeader = CMSG_FIRSTHDR((struct msghdr*)pMessage);
		pHeader != NULL;
		pHeader = CMSG_NXTHDR((struct msghdr*)pMessage, pHeader) ) {
		const struct sock_extended_err* pNative;
		const struct sockaddr* pOffender;
		size_t iHeader = CMSG_LEN(0);
		size_t iData;
		bool bError;

		if ( pHeader->cmsg_len < iHeader ) {
			continue;
		}
		iData = (size_t)pHeader->cmsg_len - iHeader;
		bError = ((pHeader->cmsg_level == IPPROTO_IP) &&
			(pHeader->cmsg_type == IP_RECVERR)) ||
			((pHeader->cmsg_level == IPPROTO_IPV6) &&
			(pHeader->cmsg_type == IPV6_RECVERR));
		if ( !bError || (iData < sizeof(*pNative)) ) {
			continue;
		}

		pNative = (const struct sock_extended_err*)CMSG_DATA(pHeader);
		pError->SystemCode = (int)pNative->ee_errno;
		pError->Kind = (pNative->ee_errno != 0) ?
			__xrtNetSocketErrorKind((int)pNative->ee_errno) : XERR_NONE;
		pError->Type = (int)pNative->ee_type;
		pError->Code = (int)pNative->ee_code;
		pError->Info = (uint32)pNative->ee_info;
		pError->Data = (uint32)pNative->ee_data;
		if ( pNative->ee_origin == SO_EE_ORIGIN_LOCAL ) {
			pError->Origin = XNET_DGRAM_ERROR_LOCAL;
		} else if ( pNative->ee_origin == SO_EE_ORIGIN_ICMP ) {
			pError->Origin = XNET_DGRAM_ERROR_ICMP;
		} else if ( pNative->ee_origin == SO_EE_ORIGIN_ICMP6 ) {
			pError->Origin = XNET_DGRAM_ERROR_ICMP6;
		}
		if ( (pNative->ee_errno == EMSGSIZE) &&
			 (pNative->ee_info != 0) ) {
			pError->PathMtu = (size_t)pNative->ee_info;
			pError->Flags |= XNET_DGRAM_ERROR_PATH_MTU;
		}

		pOffender = SO_EE_OFFENDER(pNative);
		if ( !__xrtNetSocketDgramErrorAddress(
			&pError->Offender,
			&pError->Flags,
			XNET_DGRAM_ERROR_OFFENDER,
			pOffender,
			iData - sizeof(*pNative)
		) ) {
			return false;
		}
		return true;
	}
	return true;
}

#endif



/* 非阻塞读取一个异步数据报错误及原数据报负载前缀。 */
XRT_API xnetresult xrtNetSocketDgramRecvError(
	xnetsocket Socket,
	void* pData,
	size_t iSize,
	size_t* pReceived,
	xnetdgramerror* pError
)
{
	if ( pReceived != NULL ) {
		*pReceived = 0;
	}
	if ( pError != NULL ) {
		memset(pError, 0, sizeof(*pError));
	}
	if ( !__xrtNetSocketRequire(
		Socket,
		XNET_ERROR_SOCKET_DGRAM_ERROR,
		"receive-datagram-error"
	) || (pReceived == NULL) || (pError == NULL) ||
		 !__xrtNetSocketBuffer(
			pData,
			iSize,
			XNET_ERROR_SOCKET_DGRAM_ERROR,
			"receive-datagram-error"
		 ) ) {
		if ( (pReceived == NULL) || (pError == NULL) ) {
			__xrtErrorSetInvalidArgument();
		}
		return XNET_RESULT_ERROR;
	}
	if ( Socket->Type != XNET_SOCKET_DGRAM ) {
		__xrtNetSocketSetError(
			XERR_ARGUMENT,
			XNET_ERROR_SOCKET_DGRAM_ERROR,
			"receive-datagram-error",
			"datagram error queue requires a datagram socket"
		);
		return XNET_RESULT_ERROR;
	}

	#if defined(__linux__)
		{
			struct sockaddr_storage Storage;
			__xrt_net_dgram_control Control;
			struct iovec Buffer;
			struct msghdr Message;
			unsigned char iDummy = 0;
			ssize_t iReceived;

			memset(&Storage, 0, sizeof(Storage));
			memset(&Control, 0, sizeof(Control));
			memset(&Message, 0, sizeof(Message));
			Buffer.iov_base = (pData != NULL) ? pData : &iDummy;
			Buffer.iov_len = iSize;
			Message.msg_name = &Storage;
			Message.msg_namelen = (socklen_t)sizeof(Storage);
			Message.msg_iov = &Buffer;
			Message.msg_iovlen = 1;
			Message.msg_control = Control.Data;
			Message.msg_controllen = sizeof(Control.Data);
			do {
				iReceived = recvmsg(
					__xrtNetSocketHandle(Socket),
					&Message,
					XRT_NET_MSG_ERRQUEUE | MSG_DONTWAIT
				);
			} while ( (iReceived < 0) && (errno == EINTR) );
			if ( iReceived < 0 ) {
				int iCode = errno;

				if ( __xrtNetSocketWouldBlock(iCode) ) {
					return XNET_RESULT_AGAIN;
				}
				__xrtNetSocketSetSystemError(
					XNET_ERROR_SOCKET_DGRAM_ERROR,
					"receive-datagram-error",
					"receiving datagram error queue failed",
					iCode
				);
				return XNET_RESULT_ERROR;
			}
			*pReceived = ((size_t)iReceived < iSize) ?
				(size_t)iReceived : iSize;
			if ( (Message.msg_flags & MSG_TRUNC) != 0 ) {
				pError->Flags |= XNET_DGRAM_ERROR_PAYLOAD_TRUNCATED;
			}
			#if defined(MSG_CTRUNC)
				if ( (Message.msg_flags & MSG_CTRUNC) != 0 ) {
					pError->Flags |= XNET_DGRAM_ERROR_META_TRUNCATED;
				}
			#endif
			if ( !__xrtNetSocketDgramErrorParse(pError, &Message) ) {
				*pReceived = 0;
				memset(pError, 0, sizeof(*pError));
				return XNET_RESULT_ERROR;
			}
			return ((Message.msg_flags & MSG_TRUNC) != 0) ?
				XNET_RESULT_TRUNCATED : XNET_RESULT_OK;
		}
	#else
		__xrtNetSocketSetError(
			XERR_UNSUPPORTED,
			XNET_ERROR_SOCKET_DGRAM_ERROR,
			"receive-datagram-error",
			"datagram error queue is not supported on this platform"
		);
		return XNET_RESULT_ERROR;
	#endif
}



/* 单次聚集发送数据报，Span 数量不能超过 64。 */
XRT_API xnetresult xrtNetSocketSendToVec(xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, size_t* pSent,
	const xnetaddr* pRemote)
{
	struct sockaddr_storage Storage;
	size_t iAddressSize = sizeof(Storage);
	size_t iTotal;
	int iResult;

	if ( pSent != NULL ) {
		*pSent = 0;
	}
	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_WRITE, "send-to-vec") || (pSent == NULL) ||
		 !__xrtNetSocketCheckVec(pSpans, iCount, false,
			&iTotal, "send-to-vec") ) {
		if ( pSent == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return XNET_RESULT_ERROR;
	}
	if ( Socket->Type != XNET_SOCKET_DGRAM ) {
		__xrtNetSocketSetError(XERR_ARGUMENT,
			XNET_ERROR_SOCKET_WRITE, "send-to-vec",
			"send-to-vec requires a datagram socket");
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtNetSocketAddress(Socket, pRemote, &Storage,
		&iAddressSize, XNET_ERROR_SOCKET_WRITE, "send-to-vec") ) {
		return XNET_RESULT_ERROR;
	}
	if ( iTotal == 0 ) {
		return xrtNetSocketSendTo(Socket, NULL, 0, pSent, pRemote);
	}

	#if defined(_WIN32) || defined(_WIN64)
		WSABUF Native[XRT_NET_SOCKET_VECTOR_LIMIT];
		DWORD iBytes = 0;

		__xrtNetSocketBuildWriteVec(Native, pSpans, iCount);
		iResult = WSASendTo(__xrtNetSocketHandle(Socket), Native,
			(DWORD)iCount, &iBytes, 0, (struct sockaddr*)&Storage,
			(int)iAddressSize, NULL, NULL);
		if ( iResult == 0 ) {
			iResult = (int)iBytes;
		}
	#else
		struct iovec Native[XRT_NET_SOCKET_VECTOR_LIMIT];
		struct msghdr Message;
		ssize_t iBytes;
		int iFlags = 0;

		__xrtNetSocketBuildWriteVec(Native, pSpans, iCount);
		memset(&Message, 0, sizeof(Message));
		Message.msg_name = &Storage;
		Message.msg_namelen = (socklen_t)iAddressSize;
		Message.msg_iov = Native;
		Message.msg_iovlen = iCount;
		#if defined(MSG_NOSIGNAL)
			iFlags = MSG_NOSIGNAL;
		#endif
		do {
			iBytes = sendmsg(__xrtNetSocketHandle(Socket), &Message, iFlags);
		} while ( (iBytes < 0) && (errno == EINTR) );
		iResult = (iBytes > INT_MAX) ? INT_MAX : (int)iBytes;
	#endif
	return __xrtNetSocketSendResult(iResult, pSent, "send-to-vec");
}



/* 发送一个带逐包控制的连续数据报。 */
XRT_API xnetresult xrtNetSocketSendMsg(
	xnetsocket Socket,
	const void* pData,
	size_t iSize,
	size_t* pSent,
	const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl
)
{
	xnetspan Span;

	Span.Data = (cbytes)pData;
	Span.Size = iSize;
	return xrtNetSocketSendMsgVec(
		Socket,
		&Span,
		1,
		pSent,
		pRemote,
		pControl
	);
}



/* 聚集发送一个带逐包控制的数据报。 */
XRT_API xnetresult xrtNetSocketSendMsgVec(
	xnetsocket Socket,
	const xnetspan* pSpans,
	size_t iCount,
	size_t* pSent,
	const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl
)
{
	struct sockaddr_storage Storage;
	__xrt_net_dgram_control Control;
	size_t iAddressSize = sizeof(Storage);
	size_t iControlSize = 0;
	size_t iTotal;
	int iResult;

	if ( pSent != NULL ) {
		*pSent = 0;
	}
	if ( !__xrtNetSocketRequire(
		Socket,
		XNET_ERROR_SOCKET_WRITE,
		"send-message"
	) || (pSent == NULL) || !__xrtNetSocketCheckVec(
		pSpans,
		iCount,
		false,
		&iTotal,
		"send-message"
	) ) {
		if ( pSent == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return XNET_RESULT_ERROR;
	}
	if ( Socket->Type != XNET_SOCKET_DGRAM ) {
		__xrtNetSocketSetError(
			XERR_ARGUMENT,
			XNET_ERROR_SOCKET_WRITE,
			"send-message",
			"send-message requires a datagram socket"
		);
		return XNET_RESULT_ERROR;
	}
	if ( (pControl == NULL) || (pControl->Flags == 0) ) {
		return (pRemote != NULL) ?
			xrtNetSocketSendToVec(
				Socket, pSpans, iCount, pSent, pRemote
			) : xrtNetSocketSendVec(
				Socket, pSpans, iCount, pSent
			);
	}
	if ( (pRemote != NULL) && !__xrtNetSocketAddress(
		Socket,
		pRemote,
		&Storage,
		&iAddressSize,
		XNET_ERROR_SOCKET_WRITE,
		"send-message"
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtNetSocketDgramControlBuild(
		Socket,
		pControl,
		iTotal,
		Control.Data,
		sizeof(Control.Data),
		&iControlSize,
		XNET_ERROR_SOCKET_WRITE,
		"send-message"
	) ) {
		return XNET_RESULT_ERROR;
	}

	#if defined(_WIN32) || defined(_WIN64)
		WSABUF Native[XRT_NET_SOCKET_VECTOR_LIMIT];
		WSAMSG Message;
		LPFN_WSASENDMSG pSend = NULL;
		uintptr_t iSend = __xrtNetSocketSendMessage(Socket);
		DWORD iBytes = 0;

		if ( iSend == 0 ) {
			return XNET_RESULT_ERROR;
		}
		memcpy(&pSend, &iSend, sizeof(pSend));
		__xrtNetSocketBuildWriteVec(Native, pSpans, iCount);
		memset(&Message, 0, sizeof(Message));
		if ( pRemote != NULL ) {
			Message.name = (struct sockaddr*)&Storage;
			Message.namelen = (int)iAddressSize;
		}
		Message.lpBuffers = Native;
		Message.dwBufferCount = (DWORD)iCount;
		Message.Control.buf = (CHAR*)Control.Data;
		Message.Control.len = (ULONG)iControlSize;
		iResult = pSend(
			__xrtNetSocketHandle(Socket),
			&Message,
			0,
			&iBytes,
			NULL,
			NULL
		);
		if ( iResult == 0 ) {
			iResult = (int)iBytes;
		}
	#else
		struct iovec Native[XRT_NET_SOCKET_VECTOR_LIMIT];
		struct msghdr Message;
		ssize_t iBytes;
		int iFlags = 0;

		__xrtNetSocketBuildWriteVec(Native, pSpans, iCount);
		memset(&Message, 0, sizeof(Message));
		if ( pRemote != NULL ) {
			Message.msg_name = &Storage;
			Message.msg_namelen = (socklen_t)iAddressSize;
		}
		Message.msg_iov = Native;
		Message.msg_iovlen = iCount;
		Message.msg_control = Control.Data;
		Message.msg_controllen = iControlSize;
		#if defined(MSG_NOSIGNAL)
			iFlags = MSG_NOSIGNAL;
		#endif
		do {
			iBytes = sendmsg(
				__xrtNetSocketHandle(Socket),
				&Message,
				iFlags
			);
		} while ( (iBytes < 0) && (errno == EINTR) );
		iResult = (iBytes > INT_MAX) ? INT_MAX : (int)iBytes;
	#endif
	return __xrtNetSocketSendResult(iResult, pSent, "send-message");
}



/* 校验批量接收项，并在系统调用前初始化全部输出字段。 */
static bool __xrtNetSocketCheckRecvBatch(xnetdgramrecv* pItems,
	size_t iCapacity)
{
	size_t i;

	if ( (iCapacity > XNET_DGRAM_BATCH_MAX) ||
		 ((pItems == NULL) && (iCapacity != 0)) ) {
		__xrtNetSocketSetError(XERR_ARGUMENT, XNET_ERROR_SOCKET_READ,
			"recv-batch", "invalid datagram receive batch");
		return false;
	}
	for ( i = 0; i < iCapacity; i++ ) {
		if ( !__xrtNetSocketBuffer(pItems[i].Data, pItems[i].Capacity,
			XNET_ERROR_SOCKET_READ, "recv-batch") ) {
			return false;
		}
	}
	for ( i = 0; i < iCapacity; i++ ) {
		memset(&pItems[i].Remote, 0, sizeof(pItems[i].Remote));
		memset(&pItems[i].Meta, 0, sizeof(pItems[i].Meta));
		pItems[i].Size = 0;
		pItems[i].Result = XNET_RESULT_AGAIN;
	}
	return true;
}



/* 校验批量发送的全部数据与地址，保证无效输入不会造成部分发送。 */
static bool __xrtNetSocketCheckSendBatch(xnetsocket Socket,
	const xnetdgramsend* pItems, size_t iCount,
	struct sockaddr_storage* pAddresses, size_t* pAddressSizes)
{
	size_t i;

	if ( (iCount > XNET_DGRAM_BATCH_MAX) ||
		 ((pItems == NULL) && (iCount != 0)) ) {
		__xrtNetSocketSetError(XERR_ARGUMENT, XNET_ERROR_SOCKET_WRITE,
			"send-batch", "invalid datagram send batch");
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( !__xrtNetSocketBuffer(pItems[i].Data, pItems[i].Size,
			XNET_ERROR_SOCKET_WRITE, "send-batch") ) {
			return false;
		}
		pAddressSizes[i] = sizeof(pAddresses[i]);
		if ( (pItems[i].Remote != NULL) &&
			 !__xrtNetSocketAddress(Socket, pItems[i].Remote,
				&pAddresses[i], &pAddressSizes[i],
				XNET_ERROR_SOCKET_WRITE, "send-batch") ) {
			return false;
		}
	}
	return true;
}



#if !defined(XRT_NET_SOCKET_NATIVE_DGRAM_BATCH)

/* 可移植批量接收回退；阻塞 Socket 只接收首个报文，避免隐藏的二次等待。 */
static xnetresult __xrtNetSocketRecvBatchPortable(xnetsocket Socket,
	xnetdgramrecv* pItems, size_t iCapacity, size_t* pReceived)
{
	size_t iLimit = ((Socket->Flags & XNET_SOCKET_NONBLOCK) != 0) ?
		iCapacity : 1;
	size_t i;

	for ( i = 0; i < iLimit; i++ ) {
		xnetresult Result;

		if ( Socket->DgramMeta != 0 ) {
			Result = xrtNetSocketRecvMsg(
				Socket,
				pItems[i].Data,
				pItems[i].Capacity,
				&pItems[i].Size,
				&pItems[i].Remote,
				&pItems[i].Meta
			);
		} else {
			Result = xrtNetSocketRecvFrom(
				Socket,
				pItems[i].Data,
				pItems[i].Capacity,
				&pItems[i].Size,
				&pItems[i].Remote
			);
		}

		pItems[i].Result = Result;
		if ( (Result == XNET_RESULT_OK) ||
			 (Result == XNET_RESULT_TRUNCATED) ) {
			*pReceived = i + 1;
			continue;
		}
		if ( Result == XNET_RESULT_AGAIN ) {
			return (*pReceived != 0) ? XNET_RESULT_OK : XNET_RESULT_AGAIN;
		}
		return Result;
	}
	return XNET_RESULT_OK;
}

#endif



/* 接收一批独立数据报；Linux 模块构建使用 recvmmsg 减少系统调用。 */
XRT_API xnetresult xrtNetSocketRecvBatch(xnetsocket Socket,
	xnetdgramrecv* pItems, size_t iCapacity, size_t* pReceived)
{
	if ( pReceived != NULL ) {
		*pReceived = 0;
	}
	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_READ, "recv-batch") || (pReceived == NULL) ||
		 !__xrtNetSocketCheckRecvBatch(pItems, iCapacity) ) {
		if ( pReceived == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return XNET_RESULT_ERROR;
	}
	if ( Socket->Type != XNET_SOCKET_DGRAM ) {
		__xrtNetSocketSetError(XERR_ARGUMENT,
			XNET_ERROR_SOCKET_READ, "recv-batch",
			"recv-batch requires a datagram socket");
		return XNET_RESULT_ERROR;
	}
	if ( iCapacity == 0 ) {
		return XNET_RESULT_OK;
	}

	#if defined(XRT_NET_SOCKET_NATIVE_DGRAM_BATCH)
		{
			struct mmsghdr Messages[XRT_NET_SOCKET_RECV_BATCH_STACK];
			struct iovec Vectors[XRT_NET_SOCKET_RECV_BATCH_STACK];
			struct sockaddr_storage Addresses[
				XRT_NET_SOCKET_RECV_BATCH_STACK
			];
			__xrt_net_dgram_control Controls[
				XRT_NET_SOCKET_RECV_BATCH_STACK
			];
			unsigned char Dummy[XRT_NET_SOCKET_RECV_BATCH_STACK];
			int iFlags = 0;
			int iResult;
			size_t iBatch = iCapacity <
				XRT_NET_SOCKET_RECV_BATCH_STACK ?
				iCapacity : XRT_NET_SOCKET_RECV_BATCH_STACK;
			size_t i;
			bool bAddressError = false;

			memset(Messages, 0, sizeof(Messages));
			if ( Socket->DgramMeta != 0 ) {
				memset(Controls, 0, sizeof(Controls));
			}
			for ( i = 0; i < iBatch; i++ ) {
				Vectors[i].iov_base = (pItems[i].Data != NULL) ?
					pItems[i].Data : &Dummy[i];
				Vectors[i].iov_len = pItems[i].Capacity;
				Messages[i].msg_hdr.msg_name = &Addresses[i];
				Messages[i].msg_hdr.msg_namelen = sizeof(Addresses[i]);
				Messages[i].msg_hdr.msg_iov = &Vectors[i];
				Messages[i].msg_hdr.msg_iovlen = 1;
				if ( Socket->DgramMeta != 0 ) {
					Messages[i].msg_hdr.msg_control = Controls[i].Data;
					Messages[i].msg_hdr.msg_controllen =
						sizeof(Controls[i].Data);
				}
			}
			#if defined(MSG_TRUNC)
				iFlags |= MSG_TRUNC;
			#endif
			#if defined(MSG_WAITFORONE)
				iFlags |= MSG_WAITFORONE;
			#endif
			do {
				iResult = recvmmsg(__xrtNetSocketHandle(Socket), Messages,
					(unsigned int)iBatch, iFlags, NULL);
			} while ( (iResult < 0) && (errno == EINTR) );
			if ( iResult < 0 ) {
				int iCode = __xrtNetSocketLastError();

				return __xrtNetSocketRecvError(iCode, 0,
					pReceived, true, "recv-batch");
			}
			if ( iResult == 0 ) {
				return XNET_RESULT_AGAIN;
			}
			for ( i = 0; i < (size_t)iResult; i++ ) {
				size_t iPacketSize = (size_t)Messages[i].msg_len;

				pItems[i].Size = (iPacketSize < pItems[i].Capacity) ?
					iPacketSize : pItems[i].Capacity;
				pItems[i].Result = XNET_RESULT_OK;
				#if defined(MSG_TRUNC)
					if ( ((Messages[i].msg_hdr.msg_flags & MSG_TRUNC) != 0) ||
						 (iPacketSize > pItems[i].Capacity) ) {
						pItems[i].Result = XNET_RESULT_TRUNCATED;
					}
				#endif
				if ( !xrtNetAddrFromNative(&pItems[i].Remote,
					&Addresses[i], Messages[i].msg_hdr.msg_namelen) ) {
					pItems[i].Size = 0;
					pItems[i].Result = XNET_RESULT_ERROR;
					bAddressError = true;
				}
				if ( Socket->DgramMeta != 0 ) {
					__xrtNetSocketDgramMetaParse(
						Socket,
						&pItems[i].Meta,
						Messages[i].msg_hdr.msg_control,
						Messages[i].msg_hdr.msg_controllen,
						(uint32)Messages[i].msg_hdr.msg_flags
					);
				}
			}
			*pReceived = (size_t)iResult;
			return bAddressError ? XNET_RESULT_ERROR : XNET_RESULT_OK;
		}
	#else
		return __xrtNetSocketRecvBatchPortable(Socket,
			pItems, iCapacity, pReceived);
	#endif
}



#if !defined(XRT_NET_SOCKET_NATIVE_DGRAM_BATCH)

/* 可移植批量发送回退；每个成功项都必须完整发送一个报文。 */
static xnetresult __xrtNetSocketSendBatchPortable(xnetsocket Socket,
	const xnetdgramsend* pItems, size_t iCount, size_t* pSent)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		size_t iBytes = 0;
		xnetresult Result;

		if ( pItems[i].Remote != NULL ) {
			Result = xrtNetSocketSendTo(Socket,
				pItems[i].Data, pItems[i].Size,
				&iBytes, pItems[i].Remote);
		} else {
			Result = xrtNetSocketSend(Socket,
				pItems[i].Data, pItems[i].Size, &iBytes);
		}

		if ( Result == XNET_RESULT_OK ) {
			if ( iBytes != pItems[i].Size ) {
				__xrtNetSocketSetError(XERR_IO,
					XNET_ERROR_SOCKET_WRITE, "send-batch",
					"datagram batch item was only partially sent");
				return XNET_RESULT_ERROR;
			}
			*pSent = i + 1;
			continue;
		}
		if ( Result == XNET_RESULT_AGAIN ) {
			return (*pSent != 0) ? XNET_RESULT_OK : XNET_RESULT_AGAIN;
		}
		return Result;
	}
	return XNET_RESULT_OK;
}

#endif



/* 发送一批独立数据报；Linux 模块构建使用 sendmmsg 减少系统调用。 */
XRT_API xnetresult xrtNetSocketSendBatch(xnetsocket Socket,
	const xnetdgramsend* pItems, size_t iCount, size_t* pSent)
{
	struct sockaddr_storage Addresses[XNET_DGRAM_BATCH_MAX];
	size_t AddressSizes[XNET_DGRAM_BATCH_MAX];

	if ( pSent != NULL ) {
		*pSent = 0;
	}
	if ( !__xrtNetSocketRequire(Socket,
		XNET_ERROR_SOCKET_WRITE, "send-batch") || (pSent == NULL) ) {
		if ( pSent == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return XNET_RESULT_ERROR;
	}
	if ( Socket->Type != XNET_SOCKET_DGRAM ) {
		__xrtNetSocketSetError(XERR_ARGUMENT,
			XNET_ERROR_SOCKET_WRITE, "send-batch",
			"send-batch requires a datagram socket");
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtNetSocketCheckSendBatch(Socket, pItems,
		iCount, Addresses, AddressSizes) ) {
		return XNET_RESULT_ERROR;
	}
	if ( iCount == 0 ) {
		return XNET_RESULT_OK;
	}

	#if defined(XRT_NET_SOCKET_NATIVE_DGRAM_BATCH)
		{
			struct mmsghdr Messages[XNET_DGRAM_BATCH_MAX];
			struct iovec Vectors[XNET_DGRAM_BATCH_MAX];
			unsigned char Dummy[XNET_DGRAM_BATCH_MAX];
			int iFlags = 0;
			int iResult;
			size_t i;

			memset(Messages, 0, sizeof(Messages));
			for ( i = 0; i < iCount; i++ ) {
				Vectors[i].iov_base = (void*)((pItems[i].Data != NULL) ?
					pItems[i].Data : &Dummy[i]);
				Vectors[i].iov_len = pItems[i].Size;
				if ( pItems[i].Remote != NULL ) {
					Messages[i].msg_hdr.msg_name = &Addresses[i];
					Messages[i].msg_hdr.msg_namelen =
						(socklen_t)AddressSizes[i];
				}
				Messages[i].msg_hdr.msg_iov = &Vectors[i];
				Messages[i].msg_hdr.msg_iovlen = 1;
			}
			#if defined(MSG_NOSIGNAL)
				iFlags |= MSG_NOSIGNAL;
			#endif
			do {
				iResult = sendmmsg(__xrtNetSocketHandle(Socket), Messages,
					(unsigned int)iCount, iFlags);
			} while ( (iResult < 0) && (errno == EINTR) );
			if ( iResult < 0 ) {
				int iCode = __xrtNetSocketLastError();

				if ( __xrtNetSocketWouldBlock(iCode) ) {
					return XNET_RESULT_AGAIN;
				}
				__xrtNetSocketSetSystemError(XNET_ERROR_SOCKET_WRITE,
					"send-batch", "sending datagram batch failed", iCode);
				return XNET_RESULT_ERROR;
			}
			for ( i = 0; i < (size_t)iResult; i++ ) {
				if ( (size_t)Messages[i].msg_len != pItems[i].Size ) {
					*pSent = i;
					__xrtNetSocketSetError(XERR_IO,
						XNET_ERROR_SOCKET_WRITE, "send-batch",
						"datagram batch item was only partially sent");
					return XNET_RESULT_ERROR;
				}
			}
			*pSent = (size_t)iResult;
			return (iResult != 0) ? XNET_RESULT_OK : XNET_RESULT_AGAIN;
		}
	#else
		return __xrtNetSocketSendBatchPortable(Socket,
			pItems, iCount, pSent);
	#endif
}

#if defined(XRT_NET_SOCKET_NATIVE_DGRAM_BATCH)
	#undef XRT_NET_SOCKET_NATIVE_DGRAM_BATCH
#endif

#endif
