#include "../lib/net/xrt_net.h"
#include <stdio.h>
#include <stdlib.h>

static void on_recv_udp(xrt_net_poller* poller, xrt_net_connection* conn, const char* data, size_t len)
{
	printf("Received UDP data: %.*s\n", (int)len, data);
}

static void on_send_udp(xrt_net_poller* poller, xrt_net_connection* conn, size_t len)
{
	printf("Sent %zu UDP bytes\n", len);
}

static void on_error_udp(xrt_net_poller* poller, xrt_net_connection* conn, int error_code)
{
	printf("UDP Error: %d\n", error_code);
}

void test_udp_server()
{
	printf("\n=== UDP Server Test ===\n");

	xrt_net_events events = {
		.on_recv = on_recv_udp,
		.on_send = on_send_udp,
		.on_error = on_error_udp,
	};

	xrt_net_config config = XRT_NET_CONFIG_DEFAULT;

	xrt_udp_server* server = xrt_udp_server_create("0.0.0.0", 9999, &config, &events);
	if ( !server ) {
		printf("Failed to create UDP server\n");
		return;
	}

	printf("UDP server created on 0.0.0.0:9999\n");

	if ( xrt_udp_server_start(server) != XRT_NET_OK ) {
		printf("Failed to start UDP server\n");
		xrt_udp_server_destroy(server);
		return;
	}

	printf("UDP server started\n");

#if defined(_WIN32) || defined(_WIN64)
	printf("Press Enter to stop...\n");
	getchar();
#else
	printf("Press Ctrl+C to stop...\n");
	pause();
#endif

	xrt_udp_server_stop(server);
	xrt_udp_server_destroy(server);

	printf("UDP server stopped\n");
}

void test_udp_client()
{
	printf("\n=== UDP Client Test ===\n");

	xrt_net_events events = {
		.on_recv = on_recv_udp,
		.on_send = on_send_udp,
		.on_error = on_error_udp,
	};

	xrt_net_config config = XRT_NET_CONFIG_DEFAULT;

	xrt_udp_client* client = xrt_udp_client_create(&config, &events);
	if ( !client ) {
		printf("Failed to create UDP client\n");
		return;
	}

	printf("UDP client created\n");

	if ( xrt_udp_client_start(client) != XRT_NET_OK ) {
		printf("Failed to start UDP client\n");
		xrt_udp_client_destroy(client);
		return;
	}

	printf("UDP client started\n");

	xrt_net_address addr;
	xrt_net_address_init(&addr, xrt_net_ip_from_str("127.0.0.1"), 9999);

	const char* message = "Hello from UDP client!";
	if ( xrt_udp_client_send(client, &addr, message, strlen(message)) != XRT_NET_OK ) {
		printf("Failed to send data\n");
	} else {
		printf("Sent: %s\n", message);
	}

#if defined(_WIN32) || defined(_WIN64)
	Sleep(1000);
#else
	sleep(1);
#endif

	xrt_udp_client_stop(client);
	xrt_udp_client_destroy(client);

	printf("UDP client stopped\n");
}
