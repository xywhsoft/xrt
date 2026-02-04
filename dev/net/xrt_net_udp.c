#include "xrt_net_udp.h"
#include "../mempool.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <process.h>
#else
	#include <pthread.h>
#endif

#define XRT_UDP_BUFFER_SIZE 8192

static void* udp_server_thread(void* arg);
static void* udp_client_thread(void* arg);

xrt_udp_server* xrt_udp_server_create(const char* ip, uint16 port, const xrt_net_config* config, const xrt_net_events* events)
{
	if ( !ip ) {
		return NULL;
	}

	xrt_udp_server* server = (xrt_udp_server*)xrtCalloc(1, sizeof(xrt_udp_server));
	if ( !server ) {
		return NULL;
	}

	server->config = config ? *config : XRT_NET_CONFIG_DEFAULT;
	server->events = events ? *events : (xrt_net_events){0};

	server->conn = (xrt_net_connection*)xrtCalloc(1, sizeof(xrt_net_connection));
	if ( !server->conn ) {
		xrtFree(server);
		return NULL;
	}

	xrt_net_address addr;
	xrt_net_address_init(&addr, xrt_net_ip_from_str(ip), port);

	if ( xrt_net_socket_create(server->conn, XRT_NET_UDP) != XRT_NET_OK ) {
		goto error;
	}

	if ( xrt_net_socket_set_reuseaddr(server->conn) != XRT_NET_OK ) {
		goto error;
	}

	if ( xrt_net_socket_bind(server->conn, &addr) != XRT_NET_OK ) {
		goto error;
	}

	if ( xrt_net_socket_set_nonblock(server->conn) != XRT_NET_OK ) {
		goto error;
	}

#if defined(_WIN32) || defined(_WIN64)
	server->poller = xrt_net_iocp_poller_create(&server->config);
#else
	server->poller = xrt_net_io_uring_poller_create(&server->config);
#endif

	if ( !server->poller ) {
		goto error;
	}

	server->poller->user_data = server;

	return server;

error:
	xrt_net_socket_close(server->conn);
	xrtFree(server->conn);
	xrtFree(server);
	return NULL;
}

void xrt_udp_server_destroy(xrt_udp_server* server)
{
	if ( !server ) {
		return;
	}

	xrt_udp_server_stop(server);

	if ( server->conn ) {
		xrt_net_socket_close(server->conn);
		xrtFree(server->conn);
	}

	if ( server->poller ) {
		xrt_net_platform* platform = xrt_net_platform_get();
		if ( platform && platform->ops->destroy ) {
			platform->ops->destroy(server->poller);
		}
	}

	xrtFree(server);
}

static void* udp_server_thread(void* arg)
{
	xrt_udp_server* server = (xrt_udp_server*)arg;
	char buffer[XRT_UDP_BUFFER_SIZE];
	xrt_net_address addr;

	while ( server->running ) {
		size_t received = 0;
		xrt_net_result result = xrt_net_socket_recvfrom(server->conn, buffer, sizeof(buffer), &addr, &received);

		if ( result == XRT_NET_OK && received > 0 ) {
			if ( server->events.on_recv ) {
				server->events.on_recv(server->poller, server->conn, buffer, received);
			}
		} else if ( result == XRT_NET_AGAIN ) {
#if defined(_WIN32) || defined(_WIN64)
			Sleep(server->config.poll_timeout_ms);
#else
			usleep(server->config.poll_timeout_ms * 1000);
#endif
		} else if ( result == XRT_NET_CLOSED ) {
			break;
		}
	}

	return NULL;
}

xrt_net_result xrt_udp_server_start(xrt_udp_server* server)
{
	if ( !server ) {
		return XRT_NET_ERROR;
	}

	if ( server->running ) {
		return XRT_NET_OK;
	}

	server->running = true;

#if defined(_WIN32) || defined(_WIN64)
	server->thread = (pthread_t)_beginthreadex(NULL, 0, (unsigned int(__stdcall*)(void*))udp_server_thread, server, 0, NULL);
	if ( server->thread == 0 ) {
		server->running = false;
		return XRT_NET_ERROR;
	}
#else
	if ( pthread_create(&server->thread, NULL, udp_server_thread, server) != 0 ) {
		server->running = false;
		return XRT_NET_ERROR;
	}
#endif

	return XRT_NET_OK;
}

void xrt_udp_server_stop(xrt_udp_server* server)
{
	if ( !server || !server->running ) {
		return;
	}

	server->running = false;

#if defined(_WIN32) || defined(_WIN64)
	if ( server->thread ) {
		WaitForSingleObject((HANDLE)server->thread, INFINITE);
		CloseHandle((HANDLE)server->thread);
	}
#else
	if ( server->thread ) {
		pthread_join(server->thread, NULL);
	}
#endif
}

xrt_net_result xrt_udp_server_send(xrt_udp_server* server, const xrt_net_address* addr, const char* data, size_t len)
{
	if ( !server || !addr || !data || len == 0 ) {
		return XRT_NET_ERROR;
	}

	size_t sent = 0;
	return xrt_net_socket_sendto(server->conn, data, len, addr, &sent);
}

xrt_udp_client* xrt_udp_client_create(const xrt_net_config* config, const xrt_net_events* events)
{
	xrt_udp_client* client = (xrt_udp_client*)xrtCalloc(1, sizeof(xrt_udp_client));
	if ( !client ) {
		return NULL;
	}

	client->config = config ? *config : XRT_NET_CONFIG_DEFAULT;
	client->events = events ? *events : (xrt_net_events){0};

	client->conn = (xrt_net_connection*)xrtCalloc(1, sizeof(xrt_net_connection));
	if ( !client->conn ) {
		xrtFree(client);
		return NULL;
	}

	if ( xrt_net_socket_create(client->conn, XRT_NET_UDP) != XRT_NET_OK ) {
		goto error;
	}

	if ( xrt_net_socket_set_nonblock(client->conn) != XRT_NET_OK ) {
		goto error;
	}

#if defined(_WIN32) || defined(_WIN64)
	client->poller = xrt_net_iocp_poller_create(&client->config);
#else
	client->poller = xrt_net_io_uring_poller_create(&client->config);
#endif

	if ( !client->poller ) {
		goto error;
	}

	client->poller->user_data = client;

	return client;

error:
	xrt_net_socket_close(client->conn);
	xrtFree(client->conn);
	xrtFree(client);
	return NULL;
}

void xrt_udp_client_destroy(xrt_udp_client* client)
{
	if ( !client ) {
		return;
	}

	xrt_udp_client_stop(client);

	if ( client->conn ) {
		xrt_net_socket_close(client->conn);
		xrtFree(client->conn);
	}

	if ( client->poller ) {
		xrt_net_platform* platform = xrt_net_platform_get();
		if ( platform && platform->ops->destroy ) {
			platform->ops->destroy(client->poller);
		}
	}

	xrtFree(client);
}

static void* udp_client_thread(void* arg)
{
	xrt_udp_client* client = (xrt_udp_client*)arg;
	char buffer[XRT_UDP_BUFFER_SIZE];
	xrt_net_address addr;

	while ( client->running ) {
		size_t received = 0;
		xrt_net_result result = xrt_net_socket_recvfrom(client->conn, buffer, sizeof(buffer), &addr, &received);

		if ( result == XRT_NET_OK && received > 0 ) {
			if ( client->events.on_recv ) {
				client->events.on_recv(client->poller, client->conn, buffer, received);
			}
		} else if ( result == XRT_NET_AGAIN ) {
#if defined(_WIN32) || defined(_WIN64)
			Sleep(client->config.poll_timeout_ms);
#else
			usleep(client->config.poll_timeout_ms * 1000);
#endif
		} else if ( result == XRT_NET_CLOSED ) {
			break;
		}
	}

	return NULL;
}

xrt_net_result xrt_udp_client_start(xrt_udp_client* client)
{
	if ( !client ) {
		return XRT_NET_ERROR;
	}

	if ( client->running ) {
		return XRT_NET_OK;
	}

	client->running = true;

#if defined(_WIN32) || defined(_WIN64)
	client->thread = (pthread_t)_beginthreadex(NULL, 0, (unsigned int(__stdcall*)(void*))udp_client_thread, client, 0, NULL);
	if ( client->thread == 0 ) {
		client->running = false;
		return XRT_NET_ERROR;
	}
#else
	if ( pthread_create(&client->thread, NULL, udp_client_thread, client) != 0 ) {
		client->running = false;
		return XRT_NET_ERROR;
	}
#endif

	return XRT_NET_OK;
}

void xrt_udp_client_stop(xrt_udp_client* client)
{
	if ( !client || !client->running ) {
		return;
	}

	client->running = false;

#if defined(_WIN32) || defined(_WIN64)
	if ( client->thread ) {
		WaitForSingleObject((HANDLE)client->thread, INFINITE);
		CloseHandle((HANDLE)client->thread);
	}
#else
	if ( client->thread ) {
		pthread_join(client->thread, NULL);
	}
#endif
}

xrt_net_result xrt_udp_client_send(xrt_udp_client* client, const xrt_net_address* addr, const char* data, size_t len)
{
	if ( !client || !addr || !data || len == 0 ) {
		return XRT_NET_ERROR;
	}

	size_t sent = 0;
	return xrt_net_socket_sendto(client->conn, data, len, addr, &sent);
}
