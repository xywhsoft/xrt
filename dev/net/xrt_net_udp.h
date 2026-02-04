#ifndef XRT_NET_UDP_H
#define XRT_NET_UDP_H

#include "xrt_net_types.h"
#include "xrt_net_platform.h"

typedef struct xrt_udp_server {
	xrt_net_poller* poller;
	xrt_net_connection* conn;
	xrt_net_events events;
	xrt_net_config config;
	bool running;
	pthread_t thread;
} xrt_udp_server;

typedef struct xrt_udp_client {
	xrt_net_poller* poller;
	xrt_net_connection* conn;
	xrt_net_events events;
	xrt_net_config config;
	bool running;
	pthread_t thread;
} xrt_udp_client;

xrt_udp_server* xrt_udp_server_create(const char* ip, uint16 port, const xrt_net_config* config, const xrt_net_events* events);
void xrt_udp_server_destroy(xrt_udp_server* server);
xrt_net_result xrt_udp_server_start(xrt_udp_server* server);
void xrt_udp_server_stop(xrt_udp_server* server);
xrt_net_result xrt_udp_server_send(xrt_udp_server* server, const xrt_net_address* addr, const char* data, size_t len);

xrt_udp_client* xrt_udp_client_create(const xrt_net_config* config, const xrt_net_events* events);
void xrt_udp_client_destroy(xrt_udp_client* client);
xrt_net_result xrt_udp_client_start(xrt_udp_client* client);
void xrt_udp_client_stop(xrt_udp_client* client);
xrt_net_result xrt_udp_client_send(xrt_udp_client* client, const xrt_net_address* addr, const char* data, size_t len);

#endif
