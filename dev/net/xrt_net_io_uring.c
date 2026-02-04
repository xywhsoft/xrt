#if !defined(_WIN32) && !defined(_WIN64)

#include "xrt_net_io_uring.h"
#include <sys/eventfd.h>
#include <unistd.h>

#define XRT_IO_URING_OP_ACCEPT 1
#define XRT_IO_URING_OP_RECV 2
#define XRT_IO_URING_OP_SEND 3

typedef struct xrt_io_uring_poller {
	xrt_net_poller base;
	xrt_io_uring_data* data;
	xrt_net_events* events;
} xrt_io_uring_poller;

static xrt_net_result io_uring_poll(xrt_net_poller* poller, int timeout_ms)
{
	xrt_io_uring_poller* uring_poller = (xrt_io_uring_poller*)poller;
	xrt_io_uring_data* data = uring_poller->data;
	struct io_uring* ring = &data->ring;

	struct io_uring_cqe* cqe;
	int ret = io_uring_wait_cqe_timeout(ring, &cqe, timeout_ms);
	if ( ret < 0 ) {
		if ( ret == -ETIME ) {
			return XRT_NET_TIMEOUT;
		}
		return XRT_NET_ERROR;
	}

	xrt_io_uring_operation* op = (xrt_io_uring_operation*)io_uring_cqe_get_data(cqe);
	if ( !op ) {
		io_uring_cqe_seen(ring, cqe);
		return XRT_NET_OK;
	}

	xrt_net_connection* conn = op->conn;

	if ( cqe->res <= 0 && op->op_type != XRT_IO_URING_OP_ACCEPT ) {
		if ( uring_poller->events && uring_poller->events->on_close ) {
			uring_poller->events->on_close(poller, conn);
		}
		op->in_use = false;
		io_uring_cqe_seen(ring, cqe);
		return XRT_NET_CLOSED;
	}

	switch ( op->op_type ) {
		case XRT_IO_URING_OP_RECV:
			if ( uring_poller->events && uring_poller->events->on_recv ) {
				uring_poller->events->on_recv(poller, conn, op->buffer, cqe->res);
			}
			break;

		case XRT_IO_URING_OP_SEND:
			if ( uring_poller->events && uring_poller->events->on_send ) {
				uring_poller->events->on_send(poller, conn, cqe->res);
			}
			break;

		case XRT_IO_URING_OP_ACCEPT:
			if ( uring_poller->events && uring_poller->events->on_accept ) {
				uring_poller->events->on_accept(poller, conn);
			}
			break;
	}

	op->in_use = false;
	io_uring_cqe_seen(ring, cqe);
	return XRT_NET_OK;
}

static xrt_net_result io_uring_add(xrt_net_poller* poller, xrt_net_connection* conn)
{
	xrt_io_uring_poller* uring_poller = (xrt_io_uring_poller*)poller;
	xrt_io_uring_data* data = uring_poller->data;

	if ( conn->type == XRT_NET_TCP ) {
		xrt_io_uring_operation* op = NULL;
		for ( int i = 0; i < data->operation_count; i++ ) {
			if ( !data->operations[i].in_use ) {
				op = &data->operations[i];
				break;
			}
		}

		if ( !op ) {
			if ( data->operation_count >= 1024 ) {
				return XRT_NET_ERROR;
			}
			op = &data->operations[data->operation_count++];
		}

		memset(op, 0, sizeof(xrt_io_uring_operation));
		op->conn = conn;
		op->buffer_size = XRT_IO_URING_BUFFER_SIZE;
		op->op_type = XRT_IO_URING_OP_RECV;
		op->in_use = true;

		struct io_uring_sqe* sqe = io_uring_get_sqe(&data->ring);
		if ( !sqe ) {
			op->in_use = false;
			return XRT_NET_ERROR;
		}

		io_uring_prep_recv(sqe, conn->fd, op->buffer, op->buffer_size, 0);
		io_uring_sqe_set_data(sqe, op);

		ret = io_uring_submit(&data->ring);
		if ( ret < 0 ) {
			op->in_use = false;
			return XRT_NET_ERROR;
		}
	}

	return XRT_NET_OK;
}

static xrt_net_result io_uring_remove(xrt_net_poller* poller, xrt_net_connection* conn)
{
	xrt_io_uring_poller* uring_poller = (xrt_io_uring_poller*)poller;
	xrt_io_uring_data* data = uring_poller->data;

	for ( int i = 0; i < data->operation_count; i++ ) {
		if ( data->operations[i].conn == conn && data->operations[i].in_use ) {
			data->operations[i].in_use = false;
		}
	}

	return XRT_NET_OK;
}

static void io_uring_destroy(xrt_net_poller* poller)
{
	xrt_io_uring_poller* uring_poller = (xrt_io_uring_poller*)poller;
	xrt_io_uring_data* data = uring_poller->data;

	if ( data->operations ) {
		xrtFree(data->operations);
	}

	if ( data->wakeup_fd[0] >= 0 ) {
		close(data->wakeup_fd[0]);
	}
	if ( data->wakeup_fd[1] >= 0 ) {
		close(data->wakeup_fd[1]);
	}

	io_uring_queue_exit(&data->ring);
	pthread_mutex_destroy(&data->lock);
	xrtFree(data);
	xrtFree(uring_poller);
}

static void io_uring_wakeup(xrt_net_poller* poller)
{
	xrt_io_uring_poller* uring_poller = (xrt_io_uring_poller*)poller;
	xrt_io_uring_data* data = uring_poller->data;

	uint64_t value = 1;
	write(data->wakeup_fd[1], &value, sizeof(value));
}

static const xrt_net_platform_ops io_uring_ops = {
	.poll = io_uring_poll,
	.add = io_uring_add,
	.remove = io_uring_remove,
	.destroy = io_uring_destroy,
	.wakeup = io_uring_wakeup,
};

xrt_net_platform* xrt_net_io_uring_platform_get()
{
	static xrt_net_platform platform = {
		.name = "io_uring",
		.ops = &io_uring_ops,
	};
	return &platform;
}

xrt_net_poller* xrt_net_io_uring_poller_create(const xrt_net_config* config)
{
	xrt_io_uring_poller* uring_poller = (xrt_io_uring_poller*)xrtCalloc(1, sizeof(xrt_io_uring_poller));
	if ( !uring_poller ) {
		return NULL;
	}

	xrt_io_uring_data* data = (xrt_io_uring_data*)xrtCalloc(1, sizeof(xrt_io_uring_data));
	if ( !data ) {
		xrtFree(uring_poller);
		return NULL;
	}

	pthread_mutex_init(&data->lock, NULL);

	int ret = io_uring_queue_init(XRT_IO_URING_ENTRIES, &data->ring, 0);
	if ( ret < 0 ) {
		pthread_mutex_destroy(&data->lock);
		xrtFree(data);
		xrtFree(uring_poller);
		return NULL;
	}

	if ( pipe(data->wakeup_fd) < 0 ) {
		io_uring_queue_exit(&data->ring);
		pthread_mutex_destroy(&data->lock);
		xrtFree(data);
		xrtFree(uring_poller);
		return NULL;
	}

	data->operations = (xrt_io_uring_operation*)xrtCalloc(1024, sizeof(xrt_io_uring_operation));
	if ( !data->operations ) {
		close(data->wakeup_fd[0]);
		close(data->wakeup_fd[1]);
		io_uring_queue_exit(&data->ring);
		pthread_mutex_destroy(&data->lock);
		xrtFree(data);
		xrtFree(uring_poller);
		return NULL;
	}

	uring_poller->data = data;
	uring_poller->base.platform_data = data;

	return (xrt_net_poller*)uring_poller;
}

xrt_net_result xrt_net_io_uring_send(xrt_net_poller* poller, xrt_net_connection* conn, const char* data, size_t len)
{
	xrt_io_uring_poller* uring_poller = (xrt_io_uring_poller*)poller;
	xrt_io_uring_data* uring_data = uring_poller->data;

	xrt_io_uring_operation* op = NULL;
	for ( int i = 0; i < uring_data->operation_count; i++ ) {
		if ( !uring_data->operations[i].in_use ) {
			op = &uring_data->operations[i];
			break;
		}
	}

	if ( !op ) {
		if ( uring_data->operation_count >= 1024 ) {
			return XRT_NET_ERROR;
		}
		op = &uring_data->operations[uring_data->operation_count++];
	}

	memset(op, 0, sizeof(xrt_io_uring_operation));
	op->conn = conn;
	op->op_type = XRT_IO_URING_OP_SEND;
	op->in_use = true;

	struct io_uring_sqe* sqe = io_uring_get_sqe(&uring_data->ring);
	if ( !sqe ) {
		op->in_use = false;
		return XRT_NET_ERROR;
	}

	io_uring_prep_send(sqe, conn->fd, data, len, 0);
	io_uring_sqe_set_data(sqe, op);

	int ret = io_uring_submit(&uring_data->ring);
	if ( ret < 0 ) {
		op->in_use = false;
		return XRT_NET_ERROR;
	}

	return XRT_NET_OK;
}

#endif
