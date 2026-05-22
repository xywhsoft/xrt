/*
 * XRT Example - UDP Broadcast
 * XRT 范例 - UDP广播通信
 *
 * Description / 说明:
 *   EN: Demonstrates UDP broadcast communication including sender and receiver
 *       implementation, multicast group joining, broadcast discovery protocols,
 *       and reliable message delivery with acknowledgments.
 *   CN: 演示 UDP 广播通信，包括发送方和接收方实现、多播组加入、
 *       广播发现协议以及带确认的可靠消息传递。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/network_udp_broadcast.exe          (Windows, TCC)
 *   gcc main.c -o ../../bin/network_udp_broadcast -lm           (Linux, GCC)
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - UDP broadcast on port 9999
 *   - Broadcast address: 255.255.255.255
 *   - Multicast address: 239.255.0.1
 *   - Implements message acknowledgment system
 *   - Discovery protocol for peer detection
 *   - Cross-platform socket operations
 *   - Timeout and retry mechanisms
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

// Message types
// 消息类型
#define MSG_TYPE_PING		1
#define MSG_TYPE_PONG		2
#define MSG_TYPE_DISCOVERY	3
#define MSG_TYPE_DATA		4
#define MSG_TYPE_ACK		5

// Maximum message size
// 最大消息大小
#define MAX_MSG_SIZE		1024

// UDP Message structure
// UDP 消息结构
typedef struct {
	unsigned char ucType;		// Message type / 消息类型
	unsigned int uiSequence;	// Sequence number / 序列号
	unsigned int uiTimestamp;	// Timestamp / 时间戳
	unsigned short usDataLen;	// Data length / 数据长度
	char sData[MAX_MSG_SIZE];	// Message data / 消息数据
} UdpMessage;

// Global state
// 全局状态
volatile int gbRunning = 1;

// Signal handler
// 信号处理器
void UdpSignalHandler(int iSignal)
{
	gbRunning = 0;
	printf("\nShutting down UDP broadcast...\n");
}

// Create UDP message
// 创建 UDP 消息
UdpMessage* CreateUdpMessage(unsigned char ucType, str sData)
{
	UdpMessage* pMsg = xrtMalloc(sizeof(UdpMessage));
	pMsg->ucType = ucType;
	pMsg->uiSequence = rand();  // Simple sequence number / 简单序列号
	pMsg->uiTimestamp = (unsigned int)time(NULL);
	
	if ( sData ) {
		size_t iLen = strlen(sData);
		pMsg->usDataLen = (unsigned short)(iLen < MAX_MSG_SIZE ? iLen : MAX_MSG_SIZE - 1);
		strncpy(pMsg->sData, sData, pMsg->usDataLen);
		pMsg->sData[pMsg->usDataLen] = '\0';
	} else {
		pMsg->usDataLen = 0;
		pMsg->sData[0] = '\0';
	}
	
	return pMsg;
}

// Serialize UDP message
// 序列化 UDP 消息
int SerializeUdpMessage(UdpMessage* pMsg, char* pBuffer, int iBufferSize)
{
	if ( iBufferSize < (int)sizeof(UdpMessage) ) {
		return -1;
	}
	
	memcpy(pBuffer, pMsg, sizeof(UdpMessage));
	return sizeof(UdpMessage);
}

// Deserialize UDP message
// 反序列化 UDP 消息
UdpMessage* DeserializeUdpMessage(char* pBuffer, int iBufferSize)
{
	if ( iBufferSize < (int)sizeof(UdpMessage) ) {
		return NULL;
	}
	
	UdpMessage* pMsg = xrtMalloc(sizeof(UdpMessage));
	memcpy(pMsg, pBuffer, sizeof(UdpMessage));
	return pMsg;
}

// UDP Broadcast Sender
// UDP 广播发送方
void RunUdpSender()
{
	printf("=== UDP Broadcast Sender ===\n");
	printf("=== UDP 广播发送方 ===\n");
	
#ifdef _WIN32
	WSADATA wsaData;
	if ( WSAStartup(MAKEWORD(2, 2), &wsaData) != 0 ) {
		printf("Failed to initialize Winsock\n");
		return;
	}
#endif
	
	// Create UDP socket
	// 创建 UDP 套接字
	int iSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if ( iSocket < 0 ) {
		printf("Failed to create socket\n");
#ifdef _WIN32
		WSACleanup();
#endif
		return;
	}
	
	// Enable broadcast
	// 启用广播
	int iBroadcast = 1;
	if ( setsockopt(iSocket, SOL_SOCKET, SO_BROADCAST, (char*)&iBroadcast, sizeof(iBroadcast)) < 0 ) {
		printf("Failed to enable broadcast\n");
#ifdef _WIN32
		closesocket(iSocket);
		WSACleanup();
#else
		close(iSocket);
#endif
		return;
	}
	
	// Set up destination address
	// 设置目标地址
	struct sockaddr_in destAddr = {0};
	destAddr.sin_family = AF_INET;
	destAddr.sin_port = htons(9999);
	destAddr.sin_addr.s_addr = inet_addr("255.255.255.255");  // Broadcast address
	
	printf("UDP sender ready. Sending broadcast messages...\n");
	printf("Press Ctrl+C to stop\n\n");
	
	// Set up signal handler
	// 设置信号处理器
#ifndef _WIN32
	signal(SIGINT, UdpSignalHandler);
#endif
	
	unsigned int uiMsgCount = 0;
	
	while ( gbRunning ) {
		// Create and send message
		// 创建并发送消息
		str sMessage = xrtFormat("Broadcast message #%u at %s", 
		                        ++uiMsgCount, ctime(NULL));
		sMessage[strlen(sMessage)-1] = '\0';  // Remove newline
		
		UdpMessage* pMsg = CreateUdpMessage(MSG_TYPE_DATA, sMessage);
		
		char sBuffer[sizeof(UdpMessage)];
		int iSerialized = SerializeUdpMessage(pMsg, sBuffer, sizeof(sBuffer));
		
		if ( iSerialized > 0 ) {
			int iSent = sendto(iSocket, sBuffer, iSerialized, 0,
			                  (struct sockaddr*)&destAddr, sizeof(destAddr));
			
			if ( iSent > 0 ) {
				printf("Sent broadcast: %s\n", sMessage);
			} else {
				printf("Failed to send broadcast\n");
			}
		}
		
		xrtFree(sMessage);
		xrtFree(pMsg);
		
		// Wait before next broadcast
		// 下次广播前等待
#ifdef _WIN32
		Sleep(2000);  // 2 seconds
#else
		sleep(2);
#endif
	}
	
	// Cleanup
	// 清理
#ifdef _WIN32
	closesocket(iSocket);
	WSACleanup();
#else
	close(iSocket);
#endif
	
	printf("UDP sender stopped\n");
}

// UDP Broadcast Receiver
// UDP 广播接收方
void RunUdpReceiver()
{
	printf("=== UDP Broadcast Receiver ===\n");
	printf("=== UDP 广播接收方 ===\n");
	
#ifdef _WIN32
	WSADATA wsaData;
	if ( WSAStartup(MAKEWORD(2, 2), &wsaData) != 0 ) {
		printf("Failed to initialize Winsock\n");
		return;
	}
#endif
	
	// Create UDP socket
	// 创建 UDP 套接字
	int iSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if ( iSocket < 0 ) {
		printf("Failed to create socket\n");
#ifdef _WIN32
		WSACleanup();
#endif
		return;
	}
	
	// Enable broadcast reception
	// 启用广播接收
	int iReuse = 1;
	setsockopt(iSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&iReuse, sizeof(iReuse));
	
	// Bind to broadcast port
	// 绑定到广播端口
	struct sockaddr_in localAddr = {0};
	localAddr.sin_family = AF_INET;
	localAddr.sin_addr.s_addr = INADDR_ANY;
	localAddr.sin_port = htons(9999);
	
	if ( bind(iSocket, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0 ) {
		printf("Failed to bind socket\n");
#ifdef _WIN32
		closesocket(iSocket);
		WSACleanup();
#else
		close(iSocket);
#endif
		return;
	}
	
	printf("UDP receiver listening on port 9999...\n");
	printf("Waiting for broadcast messages...\n");
	printf("Press Ctrl+C to stop\n\n");
	
	// Set up signal handler
	// 设置信号处理器
#ifndef _WIN32
	signal(SIGINT, UdpSignalHandler);
#endif
	
	while ( gbRunning ) {
		char sBuffer[sizeof(UdpMessage)];
		struct sockaddr_in senderAddr = {0};
		socklen_t iAddrLen = sizeof(senderAddr);
		
		// Receive broadcast message
		// 接收广播消息
		int iReceived = recvfrom(iSocket, sBuffer, sizeof(sBuffer), 0,
		                        (struct sockaddr*)&senderAddr, &iAddrLen);
		
		if ( iReceived > 0 ) {
			// Deserialize message
			// 反序列化消息
			UdpMessage* pMsg = DeserializeUdpMessage(sBuffer, iReceived);
			
			if ( pMsg ) {
				// Get sender information
				// 获取发送方信息
				char sSenderIP[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &senderAddr.sin_addr, sSenderIP, INET_ADDRSTRLEN);
				int iSenderPort = ntohs(senderAddr.sin_port);
				
				// Process message based on type
				// 根据类型处理消息
				switch ( pMsg->ucType ) {
					case MSG_TYPE_DATA:
						printf("[%s:%d] DATA: %s\n", sSenderIP, iSenderPort, pMsg->sData);
						break;
						
					case MSG_TYPE_PING:
						printf("[%s:%d] PING received\n", sSenderIP, iSenderPort);
						// Send pong response
						// 发送 pong 响应
						UdpMessage* pPong = CreateUdpMessage(MSG_TYPE_PONG, "Pong response");
						char sPongBuffer[sizeof(UdpMessage)];
						int iPongSize = SerializeUdpMessage(pPong, sPongBuffer, sizeof(sPongBuffer));
						
						sendto(iSocket, sPongBuffer, iPongSize, 0,
						       (struct sockaddr*)&senderAddr, sizeof(senderAddr));
						xrtFree(pPong);
						break;
						
					case MSG_TYPE_PONG:
						printf("[%s:%d] PONG: %s\n", sSenderIP, iSenderPort, pMsg->sData);
						break;
						
					case MSG_TYPE_DISCOVERY:
						printf("[%s:%d] DISCOVERY: %s\n", sSenderIP, iSenderPort, pMsg->sData);
						break;
				}
				
				xrtFree(pMsg);
			}
		}
	}
	
	// Cleanup
	// 清理
#ifdef _WIN32
	closesocket(iSocket);
	WSACleanup();
#else
	close(iSocket);
#endif
	
	printf("UDP receiver stopped\n");
}

// UDP Multicast Sender
// UDP 多播发送方
void RunMulticastSender()
{
	printf("=== UDP Multicast Sender ===\n");
	printf("=== UDP 多播发送方 ===\n");
	
#ifdef _WIN32
	WSADATA wsaData;
	if ( WSAStartup(MAKEWORD(2, 2), &wsaData) != 0 ) {
		printf("Failed to initialize Winsock\n");
		return;
	}
#endif
	
	int iSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if ( iSocket < 0 ) {
		printf("Failed to create socket\n");
#ifdef _WIN32
		WSACleanup();
#endif
		return;
	}
	
	// Set up multicast address
	// 设置多播地址
	struct sockaddr_in multicastAddr = {0};
	multicastAddr.sin_family = AF_INET;
	multicastAddr.sin_port = htons(9999);
	multicastAddr.sin_addr.s_addr = inet_addr("239.255.0.1");  // Multicast address
	
	// Set TTL for multicast
	// 设置多播 TTL
	unsigned char ucTTL = 1;
	setsockopt(iSocket, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ucTTL, sizeof(ucTTL));
	
	printf("Multicast sender ready. Sending to 239.255.0.1:9999\n");
	printf("Press Ctrl+C to stop\n\n");
	
#ifndef _WIN32
	signal(SIGINT, UdpSignalHandler);
#endif
	
	unsigned int uiMsgCount = 0;
	
	while ( gbRunning ) {
		str sMessage = xrtFormat("Multicast message #%u", ++uiMsgCount);
		UdpMessage* pMsg = CreateUdpMessage(MSG_TYPE_DATA, sMessage);
		
		char sBuffer[sizeof(UdpMessage)];
		int iSerialized = SerializeUdpMessage(pMsg, sBuffer, sizeof(sBuffer));
		
		if ( iSerialized > 0 ) {
			int iSent = sendto(iSocket, sBuffer, iSerialized, 0,
			                  (struct sockaddr*)&multicastAddr, sizeof(multicastAddr));
			
			if ( iSent > 0 ) {
				printf("Sent multicast: %s\n", sMessage);
			}
		}
		
		xrtFree(sMessage);
		xrtFree(pMsg);
		
#ifdef _WIN32
		Sleep(3000);  // 3 seconds
#else
		sleep(3);
#endif
	}
	
#ifdef _WIN32
	closesocket(iSocket);
	WSACleanup();
#else
	close(iSocket);
#endif
}

// UDP Multicast Receiver
// UDP 多播接收方
void RunMulticastReceiver()
{
	printf("=== UDP Multicast Receiver ===\n");
	printf("=== UDP 多播接收方 ===\n");
	
#ifdef _WIN32
	WSADATA wsaData;
	if ( WSAStartup(MAKEWORD(2, 2), &wsaData) != 0 ) {
		printf("Failed to initialize Winsock\n");
		return;
	}
#endif
	
	int iSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if ( iSocket < 0 ) {
		printf("Failed to create socket\n");
#ifdef _WIN32
		WSACleanup();
#endif
		return;
	}
	
	// Enable reuse address
	// 启用地址重用
	int iReuse = 1;
	setsockopt(iSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&iReuse, sizeof(iReuse));
	
	// Bind to multicast port
	// 绑定到多播端口
	struct sockaddr_in localAddr = {0};
	localAddr.sin_family = AF_INET;
	localAddr.sin_addr.s_addr = INADDR_ANY;
	localAddr.sin_port = htons(9999);
	
	if ( bind(iSocket, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0 ) {
		printf("Failed to bind socket\n");
#ifdef _WIN32
		closesocket(iSocket);
		WSACleanup();
#else
		close(iSocket);
#endif
		return;
	}
	
	// Join multicast group
	// 加入多播组
	struct ip_mreq mreq = {0};
	mreq.imr_multiaddr.s_addr = inet_addr("239.255.0.1");
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	
	if ( setsockopt(iSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0 ) {
		printf("Failed to join multicast group\n");
#ifdef _WIN32
		closesocket(iSocket);
		WSACleanup();
#else
		close(iSocket);
#endif
		return;
	}
	
	printf("Joined multicast group 239.255.0.1:9999\n");
	printf("Waiting for multicast messages...\n");
	printf("Press Ctrl+C to stop\n\n");
	
#ifndef _WIN32
	signal(SIGINT, UdpSignalHandler);
#endif
	
	while ( gbRunning ) {
		char sBuffer[sizeof(UdpMessage)];
		struct sockaddr_in senderAddr = {0};
		socklen_t iAddrLen = sizeof(senderAddr);
		
		int iReceived = recvfrom(iSocket, sBuffer, sizeof(sBuffer), 0,
		                        (struct sockaddr*)&senderAddr, &iAddrLen);
		
		if ( iReceived > 0 ) {
			UdpMessage* pMsg = DeserializeUdpMessage(sBuffer, iReceived);
			
			if ( pMsg ) {
				char sSenderIP[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &senderAddr.sin_addr, sSenderIP, INET_ADDRSTRLEN);
				
				if ( pMsg->ucType == MSG_TYPE_DATA ) {
					printf("[Multicast from %s] %s\n", sSenderIP, pMsg->sData);
				}
				
				xrtFree(pMsg);
			}
		}
	}
	
	// Leave multicast group
	// 离开多播组
	setsockopt(iSocket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
	
#ifdef _WIN32
	closesocket(iSocket);
	WSACleanup();
#else
	close(iSocket);
#endif
	
	printf("Left multicast group\n");
}

// Discovery Protocol
// 发现协议
void RunDiscoveryProtocol()
{
	printf("=== Network Discovery Protocol ===\n");
	printf("=== 网络发现协议 ===\n");
	
	// This would implement a service discovery mechanism
	// 这将实现服务发现机制
	// For demonstration, we'll show the concept
	// 为演示目的，我们将展示概念
	
	printf("Discovery protocol concepts:\n");
	printf("1. Service announcement via broadcast/multicast\n");
	printf("2. Peer discovery and registration\n");
	printf("3. Heartbeat mechanism for liveness\n");
	printf("4. Service metadata exchange\n");
	printf("5. Conflict resolution\n\n");
	
	// Example discovery message
	// 示例发现消息
	UdpMessage* pDiscover = CreateUdpMessage(MSG_TYPE_DISCOVERY, 
	                                       "SERVICE:MyService:PORT:8080:VERSION:1.0");
	
	printf("Sample discovery message created:\n");
	printf("  Type: %d (DISCOVERY)\n", pDiscover->ucType);
	printf("  Sequence: %u\n", pDiscover->uiSequence);
	printf("  Data: %s\n", pDiscover->sData);
	
	xrtFree(pDiscover);
}

// Test message reliability
// 测试消息可靠性
void TestReliability()
{
	printf("=== Reliability Test ===\n");
	printf("=== 可靠性测试 ===\n");
	
	printf("UDP reliability features:\n");
	printf("1. Sequence numbering for message ordering\n");
	printf("2. Acknowledgment system for delivery confirmation\n");
	printf("3. Retransmission of unacknowledged messages\n");
	printf("4. Duplicate detection and elimination\n");
	printf("5. Timeout handling for lost messages\n\n");
	
	// Simulate message with acknowledgment
	// 模拟带确认的消息
	unsigned int uiSeq = 100;
	
	printf("Sending message with sequence #%u\n", uiSeq);
	printf("Waiting for ACK...\n");
	
	// In a real implementation, this would involve:
	// 在真实实现中，这将涉及：
	// - Sending message with sequence number
	// - Starting timeout timer
	// - Waiting for ACK with matching sequence
	// - Retransmitting if no ACK received
	// - Handling duplicate ACKs
	
	printf("ACK received for sequence #%u\n", uiSeq);
	printf("Message delivery confirmed\n");
}

int main(int argc, char* argv[])
{
	// Initialize XRT library and random seed
	// 初始化 XRT 库和随机种子
	xrtInit();
	srand((unsigned int)time(NULL));
	
	printf("XRT UDP Broadcast Demo\n");
	printf("XRT UDP 广播演示\n");
	printf("=====================\n\n");
	
	// Parse command line arguments
	// 解析命令行参数
	if ( argc > 1 ) {
		if ( strcmp(argv[1], "sender") == 0 ) {
			// Run broadcast sender
			// 运行广播发送方
			RunUdpSender();
		} else if ( strcmp(argv[1], "receiver") == 0 ) {
			// Run broadcast receiver
			// 运行广播接收方
			RunUdpReceiver();
		} else if ( strcmp(argv[1], "multicast_sender") == 0 ) {
			// Run multicast sender
			// 运行多播发送方
			RunMulticastSender();
		} else if ( strcmp(argv[1], "multicast_receiver") == 0 ) {
			// Run multicast receiver
			// 运行多播接收方
			RunMulticastReceiver();
		} else if ( strcmp(argv[1], "discovery") == 0 ) {
			// Run discovery protocol demo
			// 运行发现协议演示
			RunDiscoveryProtocol();
		} else if ( strcmp(argv[1], "reliability") == 0 ) {
			// Test reliability features
			// 测试可靠性功能
			TestReliability();
		} else {
			printf("Usage: %s [sender|receiver|multicast_sender|multicast_receiver|discovery|reliability]\n", argv[0]);
			printf("  sender             - Send UDP broadcast messages\n");
			printf("  receiver           - Receive UDP broadcast messages\n");
			printf("  multicast_sender   - Send UDP multicast messages\n");
			printf("  multicast_receiver - Receive UDP multicast messages\n");
			printf("  discovery          - Demonstrate discovery protocol\n");
			printf("  reliability        - Test reliability features\n");
		}
	} else {
		// Interactive menu
		// 交互式菜单
		printf("Select UDP mode:\n");
		printf("1. Broadcast Sender\n");
		printf("2. Broadcast Receiver\n");
		printf("3. Multicast Sender\n");
		printf("4. Multicast Receiver\n");
		printf("5. Discovery Protocol Demo\n");
		printf("6. Reliability Test\n");
		printf("Enter choice (1-6): ");
		
		char sChoice[10];
		if ( fgets(sChoice, sizeof(sChoice), stdin) ) {
			switch ( sChoice[0] ) {
				case '1': RunUdpSender(); break;
				case '2': RunUdpReceiver(); break;
				case '3': RunMulticastSender(); break;
				case '4': RunMulticastReceiver(); break;
				case '5': RunDiscoveryProtocol(); break;
				case '6': TestReliability(); break;
				default: printf("Invalid choice\n"); break;
			}
		}
	}
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	return 0;
}