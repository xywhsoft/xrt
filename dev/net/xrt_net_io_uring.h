#ifndef XRT_NET_IO_URING_H
#define XRT_NET_IO_URING_H

#if !defined(_WIN32) && !defined(_WIN64)

#include "xrt_net_types.h"
#include "xrt_net_platform.h"

#include <linux/io_uring.h>

#define XRT_IO_URING_ENTRIES 256
#define XRT_IO_URING_BUFFER_SIZE 8192

typedef struct xrt_io_uring_operation {
	xrt_net_connection* conn;
	char buffer[XRT_IO_URING_BUFFER_SIZE];
	size_t buffer_size;
	int op_type;
	bool in_use;
} xrt_io_uring_operation;

typedef struct xrt_io_uring_data {
	struct io_uring ring;
	pthread_mutex_t lock;
	xrt_io_uring_operation* operations;
	int operation_count;
	int wakeup_fd[2];
} xrt_io_uring_data;

xrt_net_platform* xrt_net_io_uring_platform_get();

#endif

#endif
