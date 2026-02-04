#include "xrt_net_platform.h"

static bool g_platform_initialized = false;

#if defined(_WIN32) || defined(_WIN64)
static WSADATA g_wsa_data;
#endif

xrt_net_result xrt_net_platform_init()
{
	if ( g_platform_initialized ) {
		return XRT_NET_OK;
	}

#if defined(_WIN32) || defined(_WIN64)
	if ( WSAStartup(MAKEWORD(2, 2), &g_wsa_data) != 0 ) {
		return XRT_NET_ERROR;
	}
#endif

	g_platform_initialized = true;
	return XRT_NET_OK;
}

void xrt_net_platform_cleanup()
{
	if ( !g_platform_initialized ) {
		return;
	}

#if defined(_WIN32) || defined(_WIN64)
	WSACleanup();
#endif

	g_platform_initialized = false;
}

xrt_net_result xrt_net_socket_create(xrt_net_connection* conn, xrt_net_type type)
{
	if ( !conn ) {
		return XRT_NET_ERROR;
	}

	int sock_type = (type == XRT_NET_TCP) ? SOCK_STREAM : SOCK_DGRAM;
	int protocol = (type == XRT_NET_TCP) ? IPPROTO_TCP : IPPROTO_UDP;

	conn->fd = socket(AF_INET, sock_type, protocol);
	if ( conn->fd == XRT_INVALID_SOCKET ) {
		return XRT_NET_ERROR;
	}

	conn->type = type;
	return XRT_NET_OK;
}

void xrt_net_socket_close(xrt_net_connection* conn)
{
	if ( !conn || conn->fd == XRT_INVALID_SOCKET ) {
		return;
	}

#if defined(_WIN32) || defined(_WIN64)
	closesocket(conn->fd);
#else
	close(conn->fd);
#endif

	conn->fd = XRT_INVALID_SOCKET;
}

xrt_net_result xrt_net_socket_set_nonblock(xrt_net_connection* conn)
{
	if ( !conn || conn->fd == XRT_INVALID_SOCKET ) {
		return XRT_NET_ERROR;
	}

#if defined(_WIN32) || defined(_WIN64)
	u_long mode = 1;
	if ( ioctlsocket(conn->fd, FIONBIO, &mode) != 0 ) {
		return XRT_NET_ERROR;
	}
#else
	int flags = fcntl(conn->fd, F_GETFL, 0);
	if ( flags == -1 ) {
		return XRT_NET_ERROR;
	}
	if ( fcntl(conn->fd, F_SETFL, flags | O_NONBLOCK) == -1 ) {
		return XRT_NET_ERROR;
	}
#endif

	return XRT_NET_OK;
}

xrt_net_result xrt_net_socket_set_reuseaddr(xrt_net_connection* conn)
{
	if ( !conn || conn->fd == XRT_INVALID_SOCKET ) {
		return XRT_NET_ERROR;
	}

	int opt = 1;
#if defined(_WIN32) || defined(_WIN64)
	if ( setsockopt(conn->fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) != 0 ) {
		return XRT_NET_ERROR;
	}
#else
	if ( setsockopt(conn->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0 ) {
		return XRT_NET_ERROR;
	}
#endif

	return XRT_NET_OK;
}

xrt_net_result xrt_net_socket_bind(xrt_net_connection* conn, const xrt_net_address* addr)
{
	if ( !conn || !addr || conn->fd == XRT_INVALID_SOCKET ) {
		return XRT_NET_ERROR;
	}

	struct sockaddr_in sa;
	xrt_net_address_to_sockaddr((xrt_net_address*)addr, &sa);

	if ( bind(conn->fd, (struct sockaddr*)&sa, sizeof(sa)) != 0 ) {
		return XRT_NET_ERROR;
	}

	conn->local_addr = *addr;
	return XRT_NET_OK;
}

xrt_net_result xrt_net_socket_listen(xrt_net_connection* conn, int backlog)
{
	if ( !conn || conn->fd == XRT_INVALID_SOCKET || conn->type != XRT_NET_TCP ) {
		return XRT_NET_ERROR;
	}

	if ( listen(conn->fd, backlog) != 0 ) {
		return XRT_NET_ERROR;
	}

	return XRT_NET_OK;
}

xrt_net_result xrt_net_socket_accept(xrt_net_connection* server, xrt_net_connection* client)
{
	if ( !server || !client || server->fd == XRT_INVALID_SOCKET ) {
		return XRT_NET_ERROR;
	}

	struct sockaddr_in sa;
	socklen_t sa_len = sizeof(sa);

	client->fd = accept(server->fd, (struct sockaddr*)&sa, &sa_len);
	if ( client->fd == XRT_INVALID_SOCKET ) {
#if defined(_WIN32) || defined(_WIN64)
		int err = WSAGetLastError();
		if ( err == WSAEWOULDBLOCK ) {
			return XRT_NET_AGAIN;
		}
#else
		if ( errno == EAGAIN || errno == EWOULDBLOCK ) {
			return XRT_NET_AGAIN;
		}
#endif
		return XRT_NET_ERROR;
	}

	xrt_net_address_from_sockaddr(&client->remote_addr, &sa);
	client->type = server->type;

	return XRT_NET_OK;
}

xrt_net_result xrt_net_socket_connect(xrt_net_connection* conn, const xrt_net_address* addr)
{
	if ( !conn || !addr || conn->fd == XRT_INVALID_SOCKET ) {
		return XRT_NET_ERROR;
	}

	struct sockaddr_in sa;
	xrt_net_address_to_sockaddr((xrt_net_address*)addr, &sa);

	int result = connect(conn->fd, (struct sockaddr*)&sa, sizeof(sa));
	if ( result != 0 ) {
#if defined(_WIN32) || defined(_WIN64)
		int err = WSAGetLastError();
		if ( err == WSAEWOULDBLOCK ) {
			return XRT_NET_AGAIN;
		}
		if ( err == WSAEISCONN ) {
			return XRT_NET_OK;
		}
#else
		if ( errno == EINPROGRESS ) {
			return XRT_NET_AGAIN;
		}
		if ( errno == EISCONN ) {
			return XRT_NET_OK;
		}
#endif
		return XRT_NET_ERROR;
	}

	return XRT_NET_OK;
}

xrt_net_result xrt_net_socket_send(xrt_net_connection* conn, const char* data, size_t len, size_t* sent)
{
	if ( !conn || !data || len == 0 || conn->fd == XRT_INVALID_SOCKET ) {
		return XRT_NET_ERROR;
	}

#if defined(_WIN32) || defined(_WIN64)
	int result = send(conn->fd, data, (int)len, 0);
#else
	ssize_t result = send(conn->fd, data, len, 0);
#endif

	if ( result < 0 ) {
#if defined(_WIN32) || defined(_WIN64)
		int err = WSAGetLastError();
		if ( err == WSAEWOULDBLOCK ) {
			return XRT_NET_AGAIN;
		}
#else
		if ( errno == EAGAIN || errno == EWOULDBLOCK ) {
			return XRT_NET_AGAIN;
		}
#endif
		return XRT_NET_ERROR;
	}

	if ( sent ) {
		*sent = (size_t)result;
	}
	return XRT_NET_OK;
}

xrt_net_result xrt_net_socket_recv(xrt_net_connection* conn, char* data, size_t len, size_t* received)
{
	if ( !conn || !data || len == 0 || conn->fd == XRT_INVALID_SOCKET ) {
		return XRT_NET_ERROR;
	}

#if defined(_WIN32) || defined(_WIN64)
	int result = recv(conn->fd, data, (int)len, 0);
#else
	ssize_t result = recv(conn->fd, data, len, 0);
#endif

	if ( result < 0 ) {
#if defined(_WIN32) || defined(_WIN64)
		int err = WSAGetLastError();
		if ( err == WSAEWOULDBLOCK ) {
			return XRT_NET_AGAIN;
		}
#else
		if ( errno == EAGAIN || errno == EWOULDBLOCK ) {
			return XRT_NET_AGAIN;
		}
#endif
		return XRT_NET_ERROR;
	}

	if ( result == 0 ) {
		return XRT_NET_CLOSED;
	}

	if ( received ) {
		*received = (size_t)result;
	}
	return XRT_NET_OK;
}

xrt_net_result xrt_net_socket_sendto(xrt_net_connection* conn, const char* data, size_t len, const xrt_net_address* addr, size_t* sent)
{
	if ( !conn || !data || len == 0 || !addr || conn->fd == XRT_INVALID_SOCKET ) {
		return XRT_NET_ERROR;
	}

	struct sockaddr_in sa;
	xrt_net_address_to_sockaddr((xrt_net_address*)addr, &sa);

#if defined(_WIN32) || defined(_WIN64)
	int result = sendto(conn->fd, data, (int)len, 0, (struct sockaddr*)&sa, sizeof(sa));
#else
	ssize_t result = sendto(conn->fd, data, len, 0, (struct sockaddr*)&sa, sizeof(sa));
#endif

	if ( result < 0 ) {
#if defined(_WIN32) || defined(_WIN64)
		int err = WSAGetLastError();
		if ( err == WSAEWOULDBLOCK ) {
			return XRT_NET_AGAIN;
		}
#else
		if ( errno == EAGAIN || errno == EWOULDBLOCK ) {
			return XRT_NET_AGAIN;
		}
#endif
		return XRT_NET_ERROR;
	}

	if ( sent ) {
		*sent = (size_t)result;
	}
	return XRT_NET_OK;
}

xrt_net_result xrt_net_socket_recvfrom(xrt_net_connection* conn, char* data, size_t len, xrt_net_address* addr, size_t* received)
{
	if ( !conn || !data || len == 0 || !addr || conn->fd == XRT_INVALID_SOCKET ) {
		return XRT_NET_ERROR;
	}

	struct sockaddr_in sa;
	socklen_t sa_len = sizeof(sa);

#if defined(_WIN32) || defined(_WIN64)
	int result = recvfrom(conn->fd, data, (int)len, 0, (struct sockaddr*)&sa, &sa_len);
#else
	ssize_t result = recvfrom(conn->fd, data, len, 0, (struct sockaddr*)&sa, &sa_len);
#endif

	if ( result < 0 ) {
#if defined(_WIN32) || defined(_WIN64)
		int err = WSAGetLastError();
		if ( err == WSAEWOULDBLOCK ) {
			return XRT_NET_AGAIN;
		}
#else
		if ( errno == EAGAIN || errno == EWOULDBLOCK ) {
			return XRT_NET_AGAIN;
		}
#endif
		return XRT_NET_ERROR;
	}

	xrt_net_address_from_sockaddr(addr, &sa);

	if ( result == 0 ) {
		return XRT_NET_CLOSED;
	}

	if ( received ) {
		*received = (size_t)result;
	}
	return XRT_NET_OK;
}
