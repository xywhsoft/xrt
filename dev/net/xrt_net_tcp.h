#ifndef XRT_NET_TCP_H
#define XRT_NET_TCP_H

#include "xrt_net_types.h"
#include "xrt_net_platform.h"
#include "xrt_net_tls.h"

typedef struct xrt_tcp_server {
	xrt_net_poller* poller;
	xrt_net_connection* listen_conn;
	xrt_net_connection* clients;
	int client_count;
	int max_clients;
	xrt_net_events events;
	xrt_tls_config tls_config;
	xrt_net_config config;
	bool running;
	pthread_t thread;
} xrt_tcp_server;

typedef struct xrt_tcp_client {
	xrt_net_poller* poller;
	xrt_net_connection* conn;
	xrt_net_events events;
	xrt_tls_config tls_config;
	xrt_net_config config;
	bool running;
	bool connected;
	pthread_t thread;
} xrt_tcp_client;

xrt_tcp_server* xrt_tcp_server_create(const char* ip, uint16 port, const xrt_net_config* config, const xrt_net_events* events);
void xrt_tcp_server_destroy(xrt_tcp_server* server);
xrt_net_result xrt_tcp_server_start(xrt_tcp_server* server);
void xrt_tcp_server_stop(xrt_tcp_server* server);
xrt_net_result xrt_tcp_server_send(xrt_tcp_server* server, int client_id, const char* data, size_t len);
void xrt_tcp_server_disconnect(xrt_tcp_server* server, int client_id);
xrt_net_result xrt_tcp_server_enable_tls(xrt_tcp_server* server, const xrt_tls_config* tls_config);
int xrt_tcp_server_get_client_count(xrt_tcp_server* server);

xrt_tcp_client* xrt_tcp_client_create(const char* ip, uint16 port, const xrt_net_config* config, const xrt_net_events* events);
void xrt_tcp_client_destroy(xrt_tcp_client* client);
xrt_net_result xrt_tcp_client_connect(xrt_tcp_client* client);
void xrt_tcp_client_disconnect(xrt_tcp_client* client);
xrt_net_result xrt_tcp_client_send(xrt_tcp_client* client, const char* data, size_t len);
xrt_net_result xrt_tcp_client_enable_tls(xrt_tcp_client* client, const xrt_tls_config* tls_config);
bool xrt_tcp_client_is_connected(xrt_tcp_client* client);

#endif
