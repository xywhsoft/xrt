#include "xrt_net_tcp.h"
#include "../mempool.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <process.h>
#else
	#include <pthread.h>
#endif

#define XRT_TCP_MAX_CLIENTS 1024

typedef struct xrt_tcp_client_data {
	xrt_net_connection conn;
	xrt_net_buffer recv_buf;
	xrt_net_buffer send_buf;
	xrt_tls_ctx* tls_ctx;
	bool in_use;
} xrt_tcp_client_data;

typedef struct xrt_tcp_server_data {
	xrt_tcp_server* server;
	xrt_tcp_client_data clients[XRT_TCP_MAX_CLIENTS];
} xrt_tcp_server_data;

static void* tcp_server_thread(void* arg);
static void* tcp_client_thread(void* arg);

xrt_tcp_server* xrt_tcp_server_create(const char* ip, uint16 port, const xrt_net_config* config, const xrt_net_events* events)
{
	if ( !ip ) {
		return NULL;
	}

	xrt_tcp_server* server = (xrt_tcp_server*)xrtCalloc(1, sizeof(xrt_tcp_server));
	if ( !server ) {
		return NULL;
	}

	server->config = config ? *config : XRT_NET_CONFIG_DEFAULT;
	server->events = events ? *events : (xrt_net_events){0};
	server->max_clients = XRT_TCP_MAX_CLIENTS;

#if defined(_WIN32) || defined(_WIN64)
	xrt_tcp_server_data* sdata = (xrt_tcp_server_data*)xrtCalloc(1, sizeof(xrt_tcp_server_data));
	if ( !sdata ) {
		xrtFree(server);
		return NULL;
	}

	for ( int i = 0; i < XRT_TCP_MAX_CLIENTS; i++ ) {
		xrt_net_buffer_init(&sdata->clients[i].recv_buf, server->config.recv_buffer_size);
		xrt_net_buffer_init(&sdata->clients[i].send_buf, server->config.send_buffer_size);
		sdata->clients[i].in_use = false;
		sdata->clients[i].conn.id = i;
	}

	sdata->server = server;
	server->clients = sdata->clients;
#else
	xrt_tcp_server_data* sdata = (xrt_tcp_server_data*)xrtCalloc(1, sizeof(xrt_tcp_server_data));
	if ( !sdata ) {
		xrtFree(server);
		return NULL;
	}

	for ( int i = 0; i < XRT_TCP_MAX_CLIENTS; i++ ) {
		xrt_net_buffer_init(&sdata->clients[i].recv_buf, server->config.recv_buffer_size);
		xrt_net_buffer_init(&sdata->clients[i].send_buf, server->config.send_buffer_size);
		sdata->clients[i].in_use = false;
		sdata->clients[i].conn.id = i;
	}

	sdata->server = server;
	server->clients = sdata->clients;
#endif

	server->listen_conn = (xrt_net_connection*)xrtCalloc(1, sizeof(xrt_net_connection));
	if ( !server->listen_conn ) {
		for ( int i = 0; i < XRT_TCP_MAX_CLIENTS; i++ ) {
			xrt_net_buffer_free(&sdata->clients[i].recv_buf);
			xrt_net_buffer_free(&sdata->clients[i].send_buf);
		}
		xrtFree(sdata);
		xrtFree(server);
		return NULL;
	}

	xrt_net_address addr;
	xrt_net_address_init(&addr, xrt_net_ip_from_str(ip), port);

	if ( xrt_net_socket_create(server->listen_conn, XRT_NET_TCP) != XRT_NET_OK ) {
		goto error;
	}

	if ( xrt_net_socket_set_reuseaddr(server->listen_conn) != XRT_NET_OK ) {
		goto error;
	}

	if ( xrt_net_socket_bind(server->listen_conn, &addr) != XRT_NET_OK ) {
		goto error;
	}

	if ( xrt_net_socket_set_nonblock(server->listen_conn) != XRT_NET_OK ) {
		goto error;
	}

	if ( xrt_net_socket_listen(server->listen_conn, SOMAXCONN) != XRT_NET_OK ) {
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
	xrt_net_socket_close(server->listen_conn);
	xrtFree(server->listen_conn);
	for ( int i = 0; i < XRT_TCP_MAX_CLIENTS; i++ ) {
		xrt_net_buffer_free(&sdata->clients[i].recv_buf);
		xrt_net_buffer_free(&sdata->clients[i].send_buf);
	}
	xrtFree(sdata);
	xrtFree(server);
	return NULL;
}

void xrt_tcp_server_destroy(xrt_tcp_server* server)
{
	if ( !server ) {
		return;
	}

	xrt_tcp_server_stop(server);

	for ( int i = 0; i < XRT_TCP_MAX_CLIENTS; i++ ) {
		xrt_tcp_client_data* cdata = (xrt_tcp_client_data*)&server->clients[i];
		if ( cdata->in_use ) {
			xrt_net_socket_close(&cdata->conn);
			xrt_net_buffer_free(&cdata->recv_buf);
			xrt_net_buffer_free(&cdata->send_buf);
			if ( cdata->tls_ctx ) {
				xrt_tls_ctx_destroy(cdata->tls_ctx);
			}
		}
	}

	if ( server->listen_conn ) {
		xrt_net_socket_close(server->listen_conn);
		xrtFree(server->listen_conn);
	}

	if ( server->poller ) {
		xrt_net_platform* platform = xrt_net_platform_get();
		if ( platform && platform->ops->destroy ) {
			platform->ops->destroy(server->poller);
		}
	}

	xrtFree(server->clients);
	xrtFree(server);
}

static void* tcp_server_thread(void* arg)
{
	xrt_tcp_server* server = (xrt_tcp_server*)arg;

	while ( server->running ) {
		xrt_net_platform* platform = xrt_net_platform_get();
		if ( platform && platform->ops->poll ) {
			platform->ops->poll(server->poller, server->config.poll_timeout_ms);
		}
	}

	return NULL;
}

xrt_net_result xrt_tcp_server_start(xrt_tcp_server* server)
{
	if ( !server ) {
		return XRT_NET_ERROR;
	}

	if ( server->running ) {
		return XRT_NET_OK;
	}

	server->running = true;

#if defined(_WIN32) || defined(_WIN64)
	server->thread = (pthread_t)_beginthreadex(NULL, 0, (unsigned int(__stdcall*)(void*))tcp_server_thread, server, 0, NULL);
	if ( server->thread == 0 ) {
		server->running = false;
		return XRT_NET_ERROR;
	}
#else
	if ( pthread_create(&server->thread, NULL, tcp_server_thread, server) != 0 ) {
		server->running = false;
		return XRT_NET_ERROR;
	}
#endif

	return XRT_NET_OK;
}

void xrt_tcp_server_stop(xrt_tcp_server* server)
{
	if ( !server || !server->running ) {
		return;
	}

	server->running = false;

	if ( server->poller ) {
		xrt_net_platform* platform = xrt_net_platform_get();
		if ( platform && platform->ops->wakeup ) {
			platform->ops->wakeup(server->poller);
		}
	}

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

	for ( int i = 0; i < XRT_TCP_MAX_CLIENTS; i++ ) {
		xrt_tcp_client_data* cdata = (xrt_tcp_client_data*)&server->clients[i];
		if ( cdata->in_use ) {
			if ( server->events.on_close ) {
				server->events.on_close(server->poller, &cdata->conn);
			}
			xrt_net_socket_close(&cdata->conn);
			cdata->in_use = false;
		}
	}
}

xrt_net_result xrt_tcp_server_send(xrt_tcp_server* server, int client_id, const char* data, size_t len)
{
	if ( !server || !data || len == 0 || client_id < 0 || client_id >= XRT_TCP_MAX_CLIENTS ) {
		return XRT_NET_ERROR;
	}

	xrt_tcp_client_data* cdata = (xrt_tcp_client_data*)&server->clients[client_id];
	if ( !cdata->in_use ) {
		return XRT_NET_ERROR;
	}

	if ( cdata->tls_ctx ) {
		size_t sent = 0;
		xrt_tls_result result = xrt_tls_write(cdata->tls_ctx, data, len, &sent);
		if ( result != XRT_TLS_OK ) {
			return XRT_NET_ERROR;
		}
		return XRT_NET_OK;
	} else {
		size_t sent = 0;
		return xrt_net_socket_send(&cdata->conn, data, len, &sent);
	}
}

void xrt_tcp_server_disconnect(xrt_tcp_server* server, int client_id)
{
	if ( !server || client_id < 0 || client_id >= XRT_TCP_MAX_CLIENTS ) {
		return;
	}

	xrt_tcp_client_data* cdata = (xrt_tcp_client_data*)&server->clients[client_id];
	if ( !cdata->in_use ) {
		return;
	}

	xrt_net_socket_close(&cdata->conn);
	cdata->in_use = false;

	if ( cdata->tls_ctx ) {
		xrt_tls_ctx_destroy(cdata->tls_ctx);
		cdata->tls_ctx = NULL;
	}

	server->client_count--;
}

xrt_net_result xrt_tcp_server_enable_tls(xrt_tcp_server* server, const xrt_tls_config* tls_config)
{
	if ( !server || !tls_config ) {
		return XRT_NET_ERROR;
	}

	server->tls_config = *tls_config;
	return XRT_NET_OK;
}

int xrt_tcp_server_get_client_count(xrt_tcp_server* server)
{
	return server ? server->client_count : 0;
}

xrt_tcp_client* xrt_tcp_client_create(const char* ip, uint16 port, const xrt_net_config* config, const xrt_net_events* events)
{
	if ( !ip ) {
		return NULL;
	}

	xrt_tcp_client* client = (xrt_tcp_client*)xrtCalloc(1, sizeof(xrt_tcp_client));
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

	xrt_net_address addr;
	xrt_net_address_init(&addr, xrt_net_ip_from_str(ip), port);

	if ( xrt_net_socket_create(client->conn, XRT_NET_TCP) != XRT_NET_OK ) {
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

void xrt_tcp_client_destroy(xrt_tcp_client* client)
{
	if ( !client ) {
		return;
	}

	xrt_tcp_client_disconnect(client);

	if ( client->conn ) {
		if ( client->conn->tls_ctx ) {
			xrt_tls_ctx_destroy(client->conn->tls_ctx);
		}
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

static void* tcp_client_thread(void* arg)
{
	xrt_tcp_client* client = (xrt_tcp_client*)arg;

	while ( client->running ) {
		xrt_net_platform* platform = xrt_net_platform_get();
		if ( platform && platform->ops->poll ) {
			platform->ops->poll(client->poller, client->config.poll_timeout_ms);
		}
	}

	return NULL;
}

xrt_net_result xrt_tcp_client_connect(xrt_tcp_client* client)
{
	if ( !client || !client->conn ) {
		return XRT_NET_ERROR;
	}

	if ( client->running ) {
		return XRT_NET_OK;
	}

	xrt_net_address addr;
	xrt_net_address_init(&addr, client->conn->remote_addr.ip, client->conn->remote_addr.port);

	xrt_net_result result = xrt_net_socket_connect(client->conn, &addr);
	if ( result != XRT_NET_OK && result != XRT_NET_AGAIN ) {
		return XRT_NET_ERROR;
	}

	if ( client->tls_config.backend != XRT_TLS_BACKEND_NONE ) {
		client->conn->tls_ctx = xrt_tls_ctx_create(&client->tls_config, false);
		if ( !client->conn->tls_ctx ) {
			return XRT_NET_ERROR;
		}

		xrt_tls_result tls_result;
		while ( (tls_result = xrt_tls_handshake(client->conn->tls_ctx)) != XRT_TLS_OK ) {
			if ( tls_result != XRT_TLS_WANT_READ && tls_result != XRT_TLS_WANT_WRITE ) {
				return XRT_NET_ERROR;
			}
		}
	}

	client->running = true;
	client->connected = true;

#if defined(_WIN32) || defined(_WIN64)
	client->thread = (pthread_t)_beginthreadex(NULL, 0, (unsigned int(__stdcall*)(void*))tcp_client_thread, client, 0, NULL);
	if ( client->thread == 0 ) {
		client->running = false;
		client->connected = false;
		return XRT_NET_ERROR;
	}
#else
	if ( pthread_create(&client->thread, NULL, tcp_client_thread, client) != 0 ) {
		client->running = false;
		client->connected = false;
		return XRT_NET_ERROR;
	}
#endif

	if ( client->events.on_connect ) {
		client->events.on_connect(client->poller, client->conn, true);
	}

	return XRT_NET_OK;
}

void xrt_tcp_client_disconnect(xrt_tcp_client* client)
{
	if ( !client ) {
		return;
	}

	if ( !client->running ) {
		return;
	}

	client->running = false;
	client->connected = false;

	if ( client->poller ) {
		xrt_net_platform* platform = xrt_net_platform_get();
		if ( platform && platform->ops->wakeup ) {
			platform->ops->wakeup(client->poller);
		}
	}

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

	if ( client->conn ) {
		if ( client->conn->tls_ctx ) {
			xrt_tls_close(client->conn->tls_ctx);
		}
		xrt_net_socket_close(client->conn);
	}
}

xrt_net_result xrt_tcp_client_send(xrt_tcp_client* client, const char* data, size_t len)
{
	if ( !client || !data || len == 0 || !client->conn ) {
		return XRT_NET_ERROR;
	}

	if ( client->conn->tls_ctx ) {
		size_t sent = 0;
		xrt_tls_result result = xrt_tls_write(client->conn->tls_ctx, data, len, &sent);
		if ( result != XRT_TLS_OK ) {
			return XRT_NET_ERROR;
		}
		return XRT_NET_OK;
	} else {
		size_t sent = 0;
		return xrt_net_socket_send(client->conn, data, len, &sent);
	}
}

xrt_net_result xrt_tcp_client_enable_tls(xrt_tcp_client* client, const xrt_tls_config* tls_config)
{
	if ( !client || !tls_config ) {
		return XRT_NET_ERROR;
	}

	client->tls_config = *tls_config;
	return XRT_NET_OK;
}

bool xrt_tcp_client_is_connected(xrt_tcp_client* client)
{
	return client ? client->connected : false;
}
