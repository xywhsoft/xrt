#ifndef XRT_NET_PLATFORM_H
#define XRT_NET_PLATFORM_H

#include "xrt_net_types.h"

typedef xrt_net_result (*xrt_net_poll_fn)(xrt_net_poller* poller, int timeout_ms);
typedef xrt_net_result (*xrt_net_add_fn)(xrt_net_poller* poller, xrt_net_connection* conn);
typedef xrt_net_result (*xrt_net_remove_fn)(xrt_net_poller* poller, xrt_net_connection* conn);
typedef void (*xrt_net_destroy_fn)(xrt_net_poller* poller);
typedef void (*xrt_net_wakeup_fn)(xrt_net_poller* poller);

typedef struct xrt_net_platform_ops {
	xrt_net_poll_fn poll;
	xrt_net_add_fn add;
	xrt_net_remove_fn remove;
	xrt_net_destroy_fn destroy;
	xrt_net_wakeup_fn wakeup;
} xrt_net_platform_ops;

typedef struct xrt_net_platform {
	const char* name;
	const xrt_net_platform_ops* ops;
} xrt_net_platform;

xrt_net_platform* xrt_net_platform_get();

xrt_net_result xrt_net_platform_init();
void xrt_net_platform_cleanup();

xrt_net_result xrt_net_socket_create(xrt_net_connection* conn, xrt_net_type type);
void xrt_net_socket_close(xrt_net_connection* conn);
xrt_net_result xrt_net_socket_set_nonblock(xrt_net_connection* conn);
xrt_net_result xrt_net_socket_set_reuseaddr(xrt_net_connection* conn);
xrt_net_result xrt_net_socket_bind(xrt_net_connection* conn, const xrt_net_address* addr);
xrt_net_result xrt_net_socket_listen(xrt_net_connection* conn, int backlog);
xrt_net_result xrt_net_socket_accept(xrt_net_connection* server, xrt_net_connection* client);
xrt_net_result xrt_net_socket_connect(xrt_net_connection* conn, const xrt_net_address* addr);
xrt_net_result xrt_net_socket_send(xrt_net_connection* conn, const char* data, size_t len, size_t* sent);
xrt_net_result xrt_net_socket_recv(xrt_net_connection* conn, char* data, size_t len, size_t* received);
xrt_net_result xrt_net_socket_sendto(xrt_net_connection* conn, const char* data, size_t len, const xrt_net_address* addr, size_t* sent);
xrt_net_result xrt_net_socket_recvfrom(xrt_net_connection* conn, char* data, size_t len, xrt_net_address* addr, size_t* received);

#endif
