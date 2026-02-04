#ifndef XRT_NET_IOCP_H
#define XRT_NET_IOCP_H

#if defined(_WIN32) || defined(_WIN64)

#include "xrt_net_types.h"
#include "xrt_net_platform.h"

#include <windows.h>

#define XRT_IOCP_MAX_EVENTS 64
#define XRT_IOCP_BUFFER_SIZE 8192

typedef struct xrt_iocp_operation {
	WSAOVERLAPPED overlapped;
	xrt_net_connection* conn;
	char buffer[XRT_IOCP_BUFFER_SIZE];
	WSABUF wsabuf;
	DWORD bytes;
	DWORD flags;
	int op_type;
	bool in_use;
} xrt_iocp_operation;

typedef struct xrt_iocp_data {
	HANDLE iocp_handle;
	HANDLE events[XRT_IOCP_MAX_EVENTS];
	CRITICAL_SECTION lock;
	int event_count;
	xrt_iocp_operation* operations;
	int operation_count;
} xrt_iocp_data;

xrt_net_platform* xrt_net_iocp_platform_get();

#endif

#endif
