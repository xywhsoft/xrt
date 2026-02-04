#ifndef XRT_NET_TYPES_H
#define XRT_NET_TYPES_H

#include "../base.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#define XRT_SOCKET SOCKET
	#define XRT_INVALID_SOCKET INVALID_SOCKET
	#define XRT_SOCKET_ERROR SOCKET_ERROR
	typedef int socklen_t;
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <unistd.h>
	#include <fcntl.h>
	#include <errno.h>
	#define XRT_SOCKET int
	#define XRT_INVALID_SOCKET -1
	#define XRT_SOCKET_ERROR -1
	typedef int SOCKET;
	#define closesocket close
#endif

typedef enum {
	XRT_NET_OK = 0,
	XRT_NET_ERROR = -1,
	XRT_NET_AGAIN = -2,
	XRT_NET_TIMEOUT = -3,
	XRT_NET_CLOSED = -4,
	XRT_NET_TLS_ERROR = -5,
} xrt_net_result;

typedef enum {
	XRT_NET_TCP = 0,
	XRT_NET_UDP = 1,
} xrt_net_type;

typedef enum {
	XRT_NET_EV_ACCEPT = 1,
	XRT_NET_EV_CONNECT = 2,
	XRT_NET_EV_RECV = 3,
	XRT_NET_EV_SEND = 4,
	XRT_NET_EV_CLOSE = 5,
	XRT_NET_EV_ERROR = 6,
} xrt_net_event;

typedef struct xrt_net_address {
	uint32 ip;
	uint16 port;
	char ip_str[16];
} xrt_net_address;

typedef struct xrt_net_buffer {
	char* data;
	size_t size;
	size_t capacity;
} xrt_net_buffer;

typedef struct xrt_net_connection {
	int id;
	XRT_SOCKET fd;
	xrt_net_address local_addr;
	xrt_net_address remote_addr;
	xrt_net_type type;
	void* user_data;
	void* tls_ctx;
	bool tls_enabled;
} xrt_net_connection;

typedef struct xrt_net_poller {
	void* platform_data;
	void* user_data;
} xrt_net_poller;

typedef void (*xrt_net_callback)(xrt_net_poller* poller, xrt_net_connection* conn, xrt_net_event event, void* arg);

typedef struct xrt_net_events {
	void (*on_accept)(xrt_net_poller* poller, xrt_net_connection* conn);
	void (*on_connect)(xrt_net_poller* poller, xrt_net_connection* conn, bool success);
	void (*on_recv)(xrt_net_poller* poller, xrt_net_connection* conn, const char* data, size_t len);
	void (*on_send)(xrt_net_poller* poller, xrt_net_connection* conn, size_t len);
	void (*on_close)(xrt_net_poller* poller, xrt_net_connection* conn);
	void (*on_error)(xrt_net_poller* poller, xrt_net_connection* conn, int error_code);
} xrt_net_events;

typedef struct xrt_net_config {
	size_t recv_buffer_size;
	size_t send_buffer_size;
	int thread_count;
	bool enable_tls;
	int poll_timeout_ms;
} xrt_net_config;

#define XRT_NET_CONFIG_DEFAULT { .recv_buffer_size = 8192, .send_buffer_size = 8192, .thread_count = 1, .enable_tls = false, .poll_timeout_ms = 10 }

static inline const char* xrt_net_result_str(xrt_net_result result)
{
	switch ( result ) {
		case XRT_NET_OK: return "OK";
		case XRT_NET_ERROR: return "ERROR";
		case XRT_NET_AGAIN: return "AGAIN";
		case XRT_NET_TIMEOUT: return "TIMEOUT";
		case XRT_NET_CLOSED: return "CLOSED";
		case XRT_NET_TLS_ERROR: return "TLS_ERROR";
		default: return "UNKNOWN";
	}
}

static inline void xrt_net_address_init(xrt_net_address* addr, uint32 ip, uint16 port)
{
	addr->ip = ip;
	addr->port = port;
	uint8* ip_bytes = (uint8*)&ip;
	sprintf(addr->ip_str, "%u.%u.%u.%u", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
}

static inline void xrt_net_address_from_sockaddr(xrt_net_address* addr, struct sockaddr_in* sa)
{
	addr->ip = ntohl(sa->sin_addr.s_addr);
	addr->port = ntohs(sa->sin_port);
	uint8* ip_bytes = (uint8*)&sa->sin_addr.s_addr;
	sprintf(addr->ip_str, "%u.%u.%u.%u", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
}

static inline void xrt_net_address_to_sockaddr(xrt_net_address* addr, struct sockaddr_in* sa)
{
	memset(sa, 0, sizeof(struct sockaddr_in));
	sa->sin_family = AF_INET;
	sa->sin_port = htons(addr->port);
	sa->sin_addr.s_addr = htonl(addr->ip);
}

static inline uint32 xrt_net_ip_from_str(const char* ip_str)
{
	struct sockaddr_in sa;
	if ( inet_pton(AF_INET, ip_str, &sa.sin_addr) == 1 ) {
		return ntohl(sa.sin_addr.s_addr);
	}
	return 0;
}

static inline bool xrt_net_buffer_init(xrt_net_buffer* buf, size_t capacity)
{
	buf->data = (char*)xrtMalloc(capacity);
	if ( !buf->data ) {
		return false;
	}
	buf->size = 0;
	buf->capacity = capacity;
	return true;
}

static inline void xrt_net_buffer_free(xrt_net_buffer* buf)
{
	if ( buf->data ) {
		xrtFree(buf->data);
		buf->data = NULL;
	}
	buf->size = 0;
	buf->capacity = 0;
}

static inline bool xrt_net_buffer_reserve(xrt_net_buffer* buf, size_t new_capacity)
{
	if ( new_capacity <= buf->capacity ) {
		return true;
	}
	char* new_data = (char*)xrtRealloc(buf->data, new_capacity);
	if ( !new_data ) {
		return false;
	}
	buf->data = new_data;
	buf->capacity = new_capacity;
	return true;
}

static inline bool xrt_net_buffer_append(xrt_net_buffer* buf, const char* data, size_t len)
{
	if ( buf->size + len > buf->capacity ) {
		if ( !xrt_net_buffer_reserve(buf, buf->capacity * 2 + len) ) {
			return false;
		}
	}
	memcpy(buf->data + buf->size, data, len);
	buf->size += len;
	return true;
}

static inline void xrt_net_buffer_clear(xrt_net_buffer* buf)
{
	buf->size = 0;
}

static inline void xrt_net_buffer_consume(xrt_net_buffer* buf, size_t len)
{
	if ( len >= buf->size ) {
		buf->size = 0;
	} else {
		memmove(buf->data, buf->data + len, buf->size - len);
		buf->size -= len;
	}
}

#endif
