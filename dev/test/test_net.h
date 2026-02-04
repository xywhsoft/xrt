#include "test_net_tcp.h"
#include "test_net_udp.h"
#include "test_net_builtin.h"

void test_network()
{
	printf("\n\n\n------------------------------------\n\n 网络库测试 :\n\n");

	if ( xrt_net_init() != XRT_NET_OK ) {
		printf("Failed to initialize network library\n");
		return;
	}

	printf("Network library initialized\n");

	int choice = 0;
	printf("\nSelect test:\n");
	printf("1. TCP Server\n");
	printf("2. TCP Client\n");
	printf("3. TCP TLS Server (OpenSSL)\n");
	printf("4. TCP TLS Client (OpenSSL)\n");
	printf("5. TCP TLS Server (BUILTIN)\n");
	printf("6. TCP TLS Client (BUILTIN)\n");
	printf("7. UDP Server\n");
	printf("8. UDP Client\n");
	printf("Enter choice: ");
	scanf("%d", &choice);

	switch ( choice ) {
		case 1:
			test_tcp_server();
			break;
		case 2:
			test_tcp_client();
			break;
		case 3:
			test_tcp_tls_server();
			break;
		case 4:
			test_tcp_tls_client();
			break;
		case 5:
			test_tcp_builtin_tls_server();
			break;
		case 6:
			test_tcp_builtin_tls_client();
			break;
		case 7:
			test_udp_server();
			break;
		case 8:
			test_udp_client();
			break;
		default:
			printf("Invalid choice\n");
			break;
	}

	xrt_net_cleanup();

	printf("Network library cleaned up\n");
}
