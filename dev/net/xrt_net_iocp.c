#if defined(_WIN32) || defined(_WIN64)

#include "xrt_net_iocp.h"
#include "../mempool.h"

#define XRT_IOCP_OP_ACCEPT 1
#define XRT_IOCP_OP_RECV 2
#define XRT_IOCP_OP_SEND 3

typedef struct xrt_iocp_poller {
	xrt_net_poller base;
	xrt_iocp_data* data;
	xrt_net_events* events;
} xrt_iocp_poller;

static xrt_net_result iocp_poll(xrt_net_poller* poller, int timeout_ms)
{
	xrt_iocp_poller* iocp_poller = (xrt_iocp_poller*)poller;
	xrt_iocp_data* data = iocp_poller->data;

	DWORD bytes_transferred;
	ULONG_PTR completion_key;
	LPOVERLAPPED overlapped;

	BOOL success = GetQueuedCompletionStatus(
		data->iocp_handle,
		&bytes_transferred,
		&completion_key,
		&overlapped,
		timeout_ms
	);

	if ( !success && !overlapped ) {
		if ( GetLastError() == WAIT_TIMEOUT ) {
			return XRT_NET_TIMEOUT;
		}
		return XRT_NET_ERROR;
	}

	xrt_iocp_operation* op = CONTAINING_RECORD(overlapped, xrt_iocp_operation, overlapped);
	xrt_net_connection* conn = op->conn;

	if ( bytes_transferred == 0 && op->op_type != XRT_IOCP_OP_ACCEPT ) {
		if ( iocp_poller->events && iocp_poller->events->on_close ) {
			iocp_poller->events->on_close(poller, conn);
		}
		return XRT_NET_CLOSED;
	}

	switch ( op->op_type ) {
		case XRT_IOCP_OP_RECV:
			if ( iocp_poller->events && iocp_poller->events->on_recv ) {
				iocp_poller->events->on_recv(poller, conn, op->buffer, bytes_transferred);
			}
			break;

		case XRT_IOCP_OP_SEND:
			if ( iocp_poller->events && iocp_poller->events->on_send ) {
				iocp_poller->events->on_send(poller, conn, bytes_transferred);
			}
			break;

		case XRT_IOCP_OP_ACCEPT:
			if ( iocp_poller->events && iocp_poller->events->on_accept ) {
				iocp_poller->events->on_accept(poller, conn);
			}
			break;
	}

	op->in_use = false;
	return XRT_NET_OK;
}

static xrt_net_result iocp_add(xrt_net_poller* poller, xrt_net_connection* conn)
{
	xrt_iocp_poller* iocp_poller = (xrt_iocp_poller*)poller;
	xrt_iocp_data* data = iocp_poller->data;

	HANDLE handle = CreateIoCompletionPort((HANDLE)conn->fd, data->iocp_handle, (ULONG_PTR)conn, 0);
	if ( !handle ) {
		return XRT_NET_ERROR;
	}

	if ( conn->type == XRT_NET_TCP ) {
		DWORD flags = 0;
		xrt_iocp_operation* op = &data->operations[data->operation_count++];
		memset(op, 0, sizeof(xrt_iocp_operation));
		op->conn = conn;
		op->wsabuf.buf = op->buffer;
		op->wsabuf.len = XRT_IOCP_BUFFER_SIZE;
		op->op_type = XRT_IOCP_OP_RECV;
		op->in_use = true;

		int result = WSARecv(conn->fd, &op->wsabuf, 1, &op->bytes, &flags, &op->overlapped, NULL);
		if ( result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING ) {
			op->in_use = false;
			return XRT_NET_ERROR;
		}
	}

	return XRT_NET_OK;
}

static xrt_net_result iocp_remove(xrt_net_poller* poller, xrt_net_connection* conn)
{
	xrt_iocp_poller* iocp_poller = (xrt_iocp_poller*)poller;
	xrt_iocp_data* data = iocp_poller->data;

	for ( int i = 0; i < data->operation_count; i++ ) {
		if ( data->operations[i].conn == conn && data->operations[i].in_use ) {
			data->operations[i].in_use = false;
		}
	}

	return XRT_NET_OK;
}

static void iocp_destroy(xrt_net_poller* poller)
{
	xrt_iocp_poller* iocp_poller = (xrt_iocp_poller*)poller;
	xrt_iocp_data* data = iocp_poller->data;

	if ( data->iocp_handle != NULL ) {
		CloseHandle(data->iocp_handle);
	}

	if ( data->operations ) {
		xrtFree(data->operations);
	}

	DeleteCriticalSection(&data->lock);
	xrtFree(data);
	xrtFree(iocp_poller);
}

static void iocp_wakeup(xrt_net_poller* poller)
{
	PostQueuedCompletionStatus(((xrt_iocp_poller*)poller)->data->iocp_handle, 0, 0, NULL);
}

static const xrt_net_platform_ops iocp_ops = {
	.poll = iocp_poll,
	.add = iocp_add,
	.remove = iocp_remove,
	.destroy = iocp_destroy,
	.wakeup = iocp_wakeup,
};

xrt_net_platform* xrt_net_iocp_platform_get()
{
	static xrt_net_platform platform = {
		.name = "IOCP",
		.ops = &iocp_ops,
	};
	return &platform;
}

xrt_net_poller* xrt_net_iocp_poller_create(const xrt_net_config* config)
{
	xrt_iocp_poller* iocp_poller = (xrt_iocp_poller*)xrtCalloc(1, sizeof(xrt_iocp_poller));
	if ( !iocp_poller ) {
		return NULL;
	}

	xrt_iocp_data* data = (xrt_iocp_data*)xrtCalloc(1, sizeof(xrt_iocp_data));
	if ( !data ) {
		xrtFree(iocp_poller);
		return NULL;
	}

	data->iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, config ? config->thread_count : 1);
	if ( !data->iocp_handle ) {
		xrtFree(data);
		xrtFree(iocp_poller);
		return NULL;
	}

	InitializeCriticalSection(&data->lock);

	data->operations = (xrt_iocp_operation*)xrtCalloc(1024, sizeof(xrt_iocp_operation));
	if ( !data->operations ) {
		DeleteCriticalSection(&data->lock);
		CloseHandle(data->iocp_handle);
		xrtFree(data);
		xrtFree(iocp_poller);
		return NULL;
	}

	iocp_poller->data = data;
	iocp_poller->base.platform_data = data;

	return (xrt_net_poller*)iocp_poller;
}

xrt_net_result xrt_net_iocp_send(xrt_net_poller* poller, xrt_net_connection* conn, const char* data, size_t len)
{
	xrt_iocp_poller* iocp_poller = (xrt_iocp_poller*)poller;
	xrt_iocp_data* iocp_data = iocp_poller->data;

	xrt_iocp_operation* op = NULL;
	for ( int i = 0; i < iocp_data->operation_count; i++ ) {
		if ( !iocp_data->operations[i].in_use ) {
			op = &iocp_data->operations[i];
			break;
		}
	}

	if ( !op ) {
		if ( iocp_data->operation_count >= 1024 ) {
			return XRT_NET_ERROR;
		}
		op = &iocp_data->operations[iocp_data->operation_count++];
	}

	memset(op, 0, sizeof(xrt_iocp_operation));
	op->conn = conn;
	op->wsabuf.buf = (char*)data;
	op->wsabuf.len = (ULONG)len;
	op->op_type = XRT_IOCP_OP_SEND;
	op->in_use = true;

	DWORD bytes_sent;
	int result = WSASend(conn->fd, &op->wsabuf, 1, &bytes_sent, 0, &op->overlapped, NULL);
	if ( result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING ) {
		op->in_use = false;
		return XRT_NET_ERROR;
	}

	return XRT_NET_OK;
}

#endif
