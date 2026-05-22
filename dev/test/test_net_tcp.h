#include "../lib/net/xrt_net.h"
#include <stdio.h>
#include <stdlib.h>

static void on_accept(xrt_net_poller* poller, xrt_net_connection* conn)
{
	printf("Client connected: %s:%d\n", conn->remote_addr.ip_str, conn->remote_addr.port);
}

static void on_connect(xrt_net_poller* poller, xrt_net_connection* conn, bool success)
{
	if ( success ) {
		printf("Connected to server: %s:%d\n", conn->remote_addr.ip_str, conn->remote_addr.port);
	} else {
		printf("Failed to connect to server\n");
	}
}

static void on_recv(xrt_net_poller* poller, xrt_net_connection* conn, const char* data, size_t len)
{
	printf("Received data: %.*s\n", (int)len, data);
}

static void on_send(xrt_net_poller* poller, xrt_net_connection* conn, size_t len)
{
	printf("Sent %zu bytes\n", len);
}

static void on_close(xrt_net_poller* poller, xrt_net_connection* conn)
{
	printf("Connection closed\n");
}

static void on_error(xrt_net_poller* poller, xrt_net_connection* conn, int error_code)
{
	printf("Error: %d\n", error_code);
}

void test_tcp_server()
{
	printf("\n=== TCP Server Test ===\n");

	xrt_net_events events = {
		.on_accept = on_accept,
		.on_connect = on_connect,
		.on_recv = on_recv,
		.on_send = on_send,
		.on_close = on_close,
		.on_error = on_error,
	};

	xrt_net_config config = XRT_NET_CONFIG_DEFAULT;
	config.thread_count = 4;
	config.poll_timeout_ms = 10;

	xrt_tcp_server* server = xrt_tcp_server_create("0.0.0.0", 8888, &config, &events);
	if ( !server ) {
		printf("Failed to create TCP server\n");
		return;
	}

	printf("TCP server created on 0.0.0.0:8888\n");

	if ( xrt_tcp_server_start(server) != XRT_NET_OK ) {
		printf("Failed to start TCP server\n");
		xrt_tcp_server_destroy(server);
		return;
	}

	printf("TCP server started\n");

#if defined(_WIN32) || defined(_WIN64)
	printf("Press Enter to stop...\n");
	getchar();
#else
	printf("Press Ctrl+C to stop...\n");
	pause();
#endif

	xrt_tcp_server_stop(server);
	xrt_tcp_server_destroy(server);

	printf("TCP server stopped\n");
}

void test_tcp_client()
{
	printf("\n=== TCP Client Test ===\n");

	xrt_net_events events = {
		.on_connect = on_connect,
		.on_recv = on_recv,
		.on_send = on_send,
		.on_close = on_close,
		.on_error = on_error,
	};

	xrt_net_config config = XRT_NET_CONFIG_DEFAULT;

	xrt_tcp_client* client = xrt_tcp_client_create("127.0.0.1", 8888, &config, &events);
	if ( !client ) {
		printf("Failed to create TCP client\n");
		return;
	}

	printf("TCP client created\n");

	if ( xrt_tcp_client_connect(client) != XRT_NET_OK ) {
		printf("Failed to connect to server\n");
		xrt_tcp_client_destroy(client);
		return;
	}

	printf("Connected to server\n");

	const char* message = "Hello from TCP client!";
	if ( xrt_tcp_client_send(client, message, strlen(message)) != XRT_NET_OK ) {
		printf("Failed to send data\n");
	} else {
		printf("Sent: %s\n", message);
	}

#if defined(_WIN32) || defined(_WIN64)
	Sleep(1000);
#else
	sleep(1);
#endif

	xrt_tcp_client_disconnect(client);
	xrt_tcp_client_destroy(client);

	printf("TCP client stopped\n");
}

void test_tcp_tls_server()
{
	printf("\n=== TCP TLS Server Test ===\n");

	xrt_net_events events = {
		.on_accept = on_accept,
		.on_recv = on_recv,
		.on_send = on_send,
		.on_close = on_close,
		.on_error = on_error,
	};

	xrt_net_config config = XRT_NET_CONFIG_DEFAULT;

	xrt_tcp_server* server = xrt_tcp_server_create("0.0.0.0", 8443, &config, &events);
	if ( !server ) {
		printf("Failed to create TCP TLS server\n");
		return;
	}

	xrt_tls_config tls_config = {
		.backend = XRT_TLS_BACKEND_OPENSSL,
		.cert_file = "server.crt",
		.key_file = "server.key",
		.ca_file = NULL,
		.verify_peer = false,
		.verify_host = false,
		.host_name = NULL,
	};

	if ( xrt_tcp_server_enable_tls(server, &tls_config) != XRT_NET_OK ) {
		printf("Failed to enable TLS\n");
		xrt_tcp_server_destroy(server);
		return;
	}

	printf("TCP TLS server created on 0.0.0.0:8443\n");

	if ( xrt_tcp_server_start(server) != XRT_NET_OK ) {
		printf("Failed to start TCP TLS server\n");
		xrt_tcp_server_destroy(server);
		return;
	}

	printf("TCP TLS server started\n");

#if defined(_WIN32) || defined(_WIN64)
	printf("Press Enter to stop...\n");
	getchar();
#else
	printf("Press Ctrl+C to stop...\n");
	pause();
#endif

	xrt_tcp_server_stop(server);
	xrt_tcp_server_destroy(server);

	printf("TCP TLS server stopped\n");
}

void test_tcp_tls_client()
{
	printf("\n=== TCP TLS Client Test ===\n");

	xrt_net_events events = {
		.on_connect = on_connect,
		.on_recv = on_recv,
		.on_send = on_send,
		.on_close = on_close,
		.on_error = on_error,
	};

	xrt_net_config config = XRT_NET_CONFIG_DEFAULT;

	xrt_tcp_client* client = xrt_tcp_client_create("127.0.0.1", 8443, &config, &events);
	if ( !client ) {
		printf("Failed to create TCP TLS client\n");
		return;
	}

	xrt_tls_config tls_config = {
		.backend = XRT_TLS_BACKEND_OPENSSL,
		.cert_file = NULL,
		.key_file = NULL,
		.ca_file = "ca.crt",
		.verify_peer = false,
		.verify_host = false,
		.host_name = "localhost",
	};

	if ( xrt_tcp_client_enable_tls(client, &tls_config) != XRT_NET_OK ) {
		printf("Failed to enable TLS\n");
		xrt_tcp_client_destroy(client);
		return;
	}

	printf("TCP TLS client created\n");

	if ( xrt_tcp_client_connect(client) != XRT_NET_OK ) {
		printf("Failed to connect to server\n");
		xrt_tcp_client_destroy(client);
		return;
	}

	printf("Connected to TLS server\n");

	const char* message = "Hello from TCP TLS client!";
	if ( xrt_tcp_client_send(client, message, strlen(message)) != XRT_NET_OK ) {
		printf("Failed to send data\n");
	} else {
		printf("Sent: %s\n", message);
	}

#if defined(_WIN32) || defined(_WIN64)
	Sleep(1000);
#else
	sleep(1);
#endif

	xrt_tcp_client_disconnect(client);
	xrt_tcp_client_destroy(client);

	printf("TCP TLS client stopped\n");
}
