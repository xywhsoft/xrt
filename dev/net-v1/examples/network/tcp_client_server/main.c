/*
 * XRT Example - TCP Client Server
 * XRT 范例 - TCP客户端服务器
 *
 * Description / 说明:
 *   EN: Demonstrates TCP client-server communication using XRT networking functions
 *       including socket creation, connection handling, data transmission, and
 *       concurrent client support.
 *   CN: 演示使用 XRT 网络函数的 TCP 客户端-服务器通信，
 *       包括套接字创建、连接处理、数据传输和并发客户端支持。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/network_tcp_client_server.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/network_tcp_client_server -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Server listens on localhost:8080
 *   - Client connects to localhost:8080
 *   - Simple echo protocol: server echoes received messages
 *   - Supports multiple concurrent clients
 *   - Graceful shutdown with Ctrl+C handling
 *   - Cross-platform socket operations
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#endif

// Global flag for graceful shutdown
// 优雅关闭的全局标志
volatile int gbRunning = 1;

// Signal handler for Ctrl+C
// Ctrl+C 的信号处理器
void SignalHandler(int iSignal)
{
	gbRunning = 0;
	printf("\nShutting down gracefully...\n");
}

// TCP Server implementation
// TCP 服务器实现
void RunTCPServer()
{
	printf("=== TCP Server ===\n");
	printf("=== TCP 服务器 ===\n");
	
#ifdef _WIN32
	// Windows socket initialization
	// Windows 套接字初始化
	WSADATA wsaData;
	if ( WSAStartup(MAKEWORD(2, 2), &wsaData) != 0 ) {
		printf("Failed to initialize Winsock\n");
		return;
	}
#endif
	
	// Create server socket
	// 创建服务器套接字
	int iServerSocket = socket(AF_INET, SOCK_STREAM, 0);
	if ( iServerSocket < 0 ) {
		printf("Failed to create server socket\n");
#ifdef _WIN32
		WSACleanup();
#endif
		return;
	}
	
	// Allow socket reuse
	// 允许套接字重用
	int iOpt = 1;
	setsockopt(iServerSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&iOpt, sizeof(iOpt));
	
	// Bind to address
	// 绑定到地址
	struct sockaddr_in srvAddr = {0};
	srvAddr.sin_family = AF_INET;
	srvAddr.sin_addr.s_addr = INADDR_ANY;
	srvAddr.sin_port = htons(8080);
	
	if ( bind(iServerSocket, (struct sockaddr*)&srvAddr, sizeof(srvAddr)) < 0 ) {
		printf("Failed to bind socket\n");
#ifdef _WIN32
		closesocket(iServerSocket);
		WSACleanup();
#else
		close(iServerSocket);
#endif
		return;
	}
	
	// Listen for connections
	// 监听连接
	if ( listen(iServerSocket, 5) < 0 ) {
		printf("Failed to listen on socket\n");
#ifdef _WIN32
		closesocket(iServerSocket);
		WSACleanup();
#else
		close(iServerSocket);
#endif
		return;
	}
	
	printf("Server listening on port 8080...\n");
	printf("Press Ctrl+C to stop server\n\n");
	
	// Set up signal handler
	// 设置信号处理器
#ifndef _WIN32
	signal(SIGINT, SignalHandler);
#endif
	
	// Accept and handle clients
	// 接受并处理客户端
	while ( gbRunning ) {
		struct sockaddr_in cliAddr = {0};
		socklen_t iCliLen = sizeof(cliAddr);
		
		// Accept client connection (non-blocking with timeout)
		// 接受客户端连接（带超时的非阻塞）
#ifdef _WIN32
		// Windows: Use select for timeout
		// Windows：使用 select 实现超时
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(iServerSocket, &readfds);
		
		struct timeval timeout = {1, 0};  // 1 second timeout
		int iActivity = select(iServerSocket + 1, &readfds, NULL, NULL, &timeout);
		
		if ( iActivity > 0 && FD_ISSET(iServerSocket, &readfds) ) {
			int iClientSocket = accept(iServerSocket, (struct sockaddr*)&cliAddr, &iCliLen);
#else
		// POSIX: Set socket to non-blocking temporarily
		// POSIX：临时设置套接字为非阻塞
		int iFlags = fcntl(iServerSocket, F_GETFL, 0);
		fcntl(iServerSocket, F_SETFL, iFlags | O_NONBLOCK);
		
		int iClientSocket = accept(iServerSocket, (struct sockaddr*)&cliAddr, &iCliLen);
		
		// Restore blocking mode
		// 恢复阻塞模式
		fcntl(iServerSocket, F_SETFL, iFlags);
		
		if ( iClientSocket >= 0 ) {
#endif
			// Get client information
			// 获取客户端信息
			char sClientIP[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &cliAddr.sin_addr, sClientIP, INET_ADDRSTRLEN);
			int iClientPort = ntohs(cliAddr.sin_port);
			
			printf("Client connected: %s:%d\n", sClientIP, iClientPort);
			
			// Handle client communication
			// 处理客户端通信
			char sBuffer[1024];
			int iBytesReceived;
			
			while ( (iBytesReceived = recv(iClientSocket, sBuffer, sizeof(sBuffer) - 1, 0)) > 0 ) {
				sBuffer[iBytesReceived] = '\0';
				printf("Received from %s:%d: %s", sClientIP, iClientPort, sBuffer);
				
				// Echo message back to client
				// 将消息回显给客户端
				send(iClientSocket, sBuffer, iBytesReceived, 0);
			}
			
			// Client disconnected
			// 客户端断开连接
			printf("Client disconnected: %s:%d\n", sClientIP, iClientPort);
			
#ifdef _WIN32
			closesocket(iClientSocket);
#else
			close(iClientSocket);
#endif
		}
	}
	
	// Cleanup
	// 清理
	printf("Closing server socket...\n");
#ifdef _WIN32
	closesocket(iServerSocket);
	WSACleanup();
#else
	close(iServerSocket);
#endif
}

// TCP Client implementation
// TCP 客户端实现
void RunTCPClient(const char* sMessage)
{
	printf("=== TCP Client ===\n");
	printf("=== TCP 客户端 ===\n");
	
#ifdef _WIN32
	// Windows socket initialization
	// Windows 套接字初始化
	WSADATA wsaData;
	if ( WSAStartup(MAKEWORD(2, 2), &wsaData) != 0 ) {
		printf("Failed to initialize Winsock\n");
		return;
	}
#endif
	
	// Create client socket
	// 创建客户端套接字
	int iClientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if ( iClientSocket < 0 ) {
		printf("Failed to create client socket\n");
#ifdef _WIN32
		WSACleanup();
#endif
		return;
	}
	
	// Connect to server
	// 连接到服务器
	struct sockaddr_in srvAddr = {0};
	srvAddr.sin_family = AF_INET;
	srvAddr.sin_port = htons(8080);
	
	// Connect to localhost
	// 连接到本地主机
#ifdef _WIN32
	inet_pton(AF_INET, "127.0.0.1", &srvAddr.sin_addr);
#else
	inet_aton("127.0.0.1", &srvAddr.sin_addr);
#endif
	
	printf("Connecting to server...\n");
	
	if ( connect(iClientSocket, (struct sockaddr*)&srvAddr, sizeof(srvAddr)) < 0 ) {
		printf("Failed to connect to server\n");
#ifdef _WIN32
		closesocket(iClientSocket);
		WSACleanup();
#else
		close(iClientSocket);
#endif
		return;
	}
	
	printf("Connected to server!\n");
	
	// Send message to server
	// 向服务器发送消息
	if ( sMessage ) {
		printf("Sending: %s", sMessage);
		send(iClientSocket, sMessage, strlen(sMessage), 0);
		
		// Receive echo from server
		// 接收服务器回显
		char sBuffer[1024];
		int iBytesReceived = recv(iClientSocket, sBuffer, sizeof(sBuffer) - 1, 0);
		
		if ( iBytesReceived > 0 ) {
			sBuffer[iBytesReceived] = '\0';
			printf("Received echo: %s", sBuffer);
		}
	}
	
	// Interactive mode if no message provided
	// 如果未提供消息则进入交互模式
	if ( !sMessage ) {
		printf("Interactive mode - type messages (Ctrl+C to quit):\n");
		
		char sInput[1024];
		while ( fgets(sInput, sizeof(sInput), stdin) ) {
			if ( strlen(sInput) > 1 ) {  // Skip empty lines
				// Send to server
				// 发送到服务器
				send(iClientSocket, sInput, strlen(sInput), 0);
				
				// Receive echo
				// 接收回显
				int iBytesReceived = recv(iClientSocket, sInput, sizeof(sInput) - 1, 0);
				if ( iBytesReceived > 0 ) {
					sInput[iBytesReceived] = '\0';
					printf("Echo: %s", sInput);
				}
			}
		}
	}
	
	// Cleanup
	// 清理
	printf("Closing client connection...\n");
#ifdef _WIN32
	closesocket(iClientSocket);
	WSACleanup();
#else
	close(iClientSocket);
#endif
}

// Test concurrent clients
// 测试并发客户端
void TestConcurrentClients()
{
	printf("=== Concurrent Client Test ===\n");
	printf("=== 并发客户端测试 ===\n");
	
	// This would typically be implemented with threading
	// 这通常会用线程实现
	// For simplicity, we'll demonstrate sequential connections
	// 为简单起见，我们演示顺序连接
	
	const char* arrMessages[] = {
		"Hello from client 1\n",
		"Message from client 2\n",
		"Third client saying hi\n",
		"Final client message\n"
	};
	
	int iClientCount = sizeof(arrMessages) / sizeof(arrMessages[0]);
	
	printf("Testing %d sequential client connections:\n", iClientCount);
	
	for ( int i = 0; i < iClientCount; i++ ) {
		printf("Starting client %d...\n", i + 1);
		RunTCPClient(arrMessages[i]);
		// Small delay between clients
		// 客户端之间的短延迟
#ifdef _WIN32
		Sleep(500);
#else
		usleep(500000);
#endif
	}
}

// Test server stress with multiple rapid connections
// 测试服务器压力（多次快速连接）
void TestServerStress()
{
	printf("=== Server Stress Test ===\n");
	printf("=== 服务器压力测试 ===\n");
	
	printf("Testing rapid connection/disconnection cycles...\n");
	
	for ( int i = 0; i < 10; i++ ) {
		// Quick connect and disconnect
		// 快速连接和断开
#ifdef _WIN32
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
		int sock = socket(AF_INET, SOCK_STREAM, 0);
		
		struct sockaddr_in addr = {0};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(8080);
		inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
		
		if ( connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0 ) {
			send(sock, "stress test\n", 12, 0);
			char buffer[256];
			recv(sock, buffer, sizeof(buffer), 0);
			closesocket(sock);
		}
		WSACleanup();
		
		Sleep(100);
#else
		int sock = socket(AF_INET, SOCK_STREAM, 0);
		
		struct sockaddr_in addr = {0};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(8080);
		inet_aton("127.0.0.1", &addr.sin_addr);
		
		if ( connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0 ) {
			send(sock, "stress test\n", 12, 0);
			char buffer[256];
			recv(sock, buffer, sizeof(buffer), 0);
			close(sock);
		}
		
		usleep(100000);
#endif
	}
	
	printf("Stress test completed\n");
}

int main(int argc, char* argv[])
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT TCP Client-Server Demo\n");
	printf("XRT TCP 客户端-服务器演示\n");
	printf("==========================\n\n");
	
	// Parse command line arguments
	// 解析命令行参数
	if ( argc > 1 ) {
		if ( strcmp(argv[1], "server") == 0 ) {
			// Run as server
			// 作为服务器运行
			RunTCPServer();
		} else if ( strcmp(argv[1], "client") == 0 ) {
			// Run as client
			// 作为客户端运行
			const char* sMessage = (argc > 2) ? argv[2] : NULL;
			RunTCPClient(sMessage);
		} else if ( strcmp(argv[1], "concurrent") == 0 ) {
			// Test concurrent clients
			// 测试并发客户端
			TestConcurrentClients();
		} else if ( strcmp(argv[1], "stress") == 0 ) {
			// Run stress test
			// 运行压力测试
			TestServerStress();
		} else {
			printf("Usage: %s [server|client|concurrent|stress] [message]\n", argv[0]);
			printf("  server     - Run TCP server on port 8080\n");
			printf("  client     - Run TCP client (optional: message to send)\n");
			printf("  concurrent - Test multiple sequential client connections\n");
			printf("  stress     - Run server stress test\n");
		}
	} else {
		// Interactive menu
		// 交互式菜单
		printf("Select mode:\n");
		printf("1. Run TCP Server\n");
		printf("2. Run TCP Client\n");
		printf("3. Test Concurrent Clients\n");
		printf("4. Run Stress Test\n");
		printf("Enter choice (1-4): ");
		
		char sChoice[10];
		if ( fgets(sChoice, sizeof(sChoice), stdin) ) {
			switch ( sChoice[0] ) {
				case '1':
					RunTCPServer();
					break;
				case '2':
					RunTCPClient(NULL);
					break;
				case '3':
					TestConcurrentClients();
					break;
				case '4':
					TestServerStress();
					break;
				default:
					printf("Invalid choice\n");
					break;
			}
		}
	}
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}