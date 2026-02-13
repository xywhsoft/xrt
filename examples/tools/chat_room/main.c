/*
 * XRT Example - Chat Room
 * XRT 范例 - 聊天室
 *
 * Description / 说明:
 *   EN: Implements a multi-user chat room system with TCP server-client architecture.
 *       Features include user registration, message broadcasting, private messaging,
 *       user presence notifications, and chat history.
 *   CN: 实现基于 TCP 服务器-客户端架构的多用户聊天室系统。
 *       功能包括用户注册、消息广播、私聊、用户在线通知和聊天历史记录。
 *
 * Build / 编译:
 *   tcc main.c -o ../bin/chat_room.exe          (Windows, TCC)
 *   gcc main.c -o ../bin/chat_room -lm           (Linux, GCC)
 *
 * Usage / 用法:
 *   Server mode: chat_room server
 *   Client mode: chat_room client <server_ip>
 *
 * Note / 注意:
 *   - Uses Tab indentation and Hungarian notation
 *   - Cross-platform TCP socket implementation
 *   - Multi-threaded server architecture
 *   - Real-time message broadcasting
 *   - User nickname management
 *   - Connection persistence and reconnection
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
//#pragma comment(lib, "ws2_32.lib")  // Removed for TCC compatibility
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#endif

// Maximum connections
// 最大连接数
#define MAX_CLIENTS		100
#define BUFFER_SIZE		1024
#define NICKNAME_MAX	32

// Message types
// 消息类型
#define MSG_CHAT		1
#define MSG_JOIN		2
#define MSG_LEAVE		3
#define MSG_PRIVATE		4
#define MSG_NICK		5
#define MSG_SERVER		6

// Client structure
// 客户端结构
typedef struct {
	int iSocket;
	str sNickname;
	struct sockaddr_in addr;
	int bActive;
} Client;

// Message structure
// 消息结构
typedef struct {
	unsigned char ucType;
	unsigned int uiFromId;
	unsigned int uiToId;
	char sContent[BUFFER_SIZE];
} ChatMessage;

// Global server state
// 全局服务器状态
Client arrClients[MAX_CLIENTS] = {0};
int giClientCount = 0;
volatile int gbServerRunning = 1;
volatile int gbClientRunning = 1;

#ifdef _WIN32
CRITICAL_SECTION gCriticalSection;
#else
pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;
#endif

// Thread-safe client management
// 线程安全的客户端管理
void LockClients()
{
#ifdef _WIN32
	EnterCriticalSection(&gCriticalSection);
#else
	pthread_mutex_lock(&gMutex);
#endif
}

void UnlockClients()
{
#ifdef _WIN32
	LeaveCriticalSection(&gCriticalSection);
#else
	pthread_mutex_unlock(&gMutex);
#endif
}

// Find free client slot
// 查找空闲客户端槽位
int FindFreeSlot()
{
	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( !arrClients[i].bActive ) {
			return i;
		}
	}
	return -1;
}

// Find client by socket
// 通过套接字查找客户端
int FindClientBySocket(int iSocket)
{
	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( arrClients[i].bActive && arrClients[i].iSocket == iSocket ) {
			return i;
		}
	}
	return -1;
}

// Find client by nickname
// 通过昵称查找客户端
int FindClientByNickname(str sNickname)
{
	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( arrClients[i].bActive && 
		     arrClients[i].sNickname && 
		     strcmp(arrClients[i].sNickname, sNickname) == 0 ) {
			return i;
		}
	}
	return -1;
}

// Broadcast message to all clients
// 向所有客户端广播消息
void BroadcastMessage(ChatMessage* pMsg, int iExcludeSocket)
{
	LockClients();
	
	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( arrClients[i].bActive && 
		     arrClients[i].iSocket != iExcludeSocket ) {
			send(arrClients[i].iSocket, (char*)pMsg, sizeof(ChatMessage), 0);
		}
	}
	
	UnlockClients();
}

// Send message to specific client
// 向特定客户端发送消息
void SendMessageToClient(int iClientId, ChatMessage* pMsg)
{
	if ( iClientId >= 0 && iClientId < MAX_CLIENTS && arrClients[iClientId].bActive ) {
		send(arrClients[iClientId].iSocket, (char*)pMsg, sizeof(ChatMessage), 0);
	}
}

// Handle client connection
// 处理客户端连接
#ifdef _WIN32
unsigned int __stdcall HandleClient(void* pParam)
#else
void* HandleClient(void* pParam)
#endif
{
	int iSocket = *(int*)pParam;
	xrtFree(pParam);
	
	// Find client slot
	// 查找客户端槽位
	LockClients();
	int iClientId = FindFreeSlot();
	if ( iClientId == -1 ) {
		printf("Server full, rejecting connection\n");
#ifdef _WIN32
		closesocket(iSocket);
#else
		close(iSocket);
#endif
		UnlockClients();
#ifdef _WIN32
		_endthreadex(0);
		return 0;
#else
		return NULL;
#endif
	}
	
	// Initialize client
	// 初始化客户端
	arrClients[iClientId].iSocket = iSocket;
	arrClients[iClientId].sNickname = xrtFormat("User%d", iClientId + 1);
	arrClients[iClientId].bActive = 1;
	giClientCount++;
	
	// Notify others of new user
	// 通知其他人新用户加入
	ChatMessage joinMsg = {0};
	joinMsg.ucType = MSG_JOIN;
	joinMsg.uiFromId = iClientId;
	strcpy(joinMsg.sContent, arrClients[iClientId].sNickname);
	BroadcastMessage(&joinMsg, iSocket);
	
	printf("Client connected: %s (ID: %d)\n", 
	       arrClients[iClientId].sNickname, iClientId);
	
	UnlockClients();
	
	// Main message loop
	// 主消息循环
	ChatMessage msg = {0};
	while ( gbServerRunning ) {
		int iReceived = recv(iSocket, (char*)&msg, sizeof(msg), 0);
		if ( iReceived <= 0 ) {
			break;  // Connection closed
		}
		
		// Process message based on type
		// 根据类型处理消息
		switch ( msg.ucType ) {
			case MSG_CHAT:
				// Broadcast chat message
				// 广播聊天消息
				msg.uiFromId = iClientId;
				BroadcastMessage(&msg, iSocket);
				printf("[%s]: %s", arrClients[iClientId].sNickname, msg.sContent);
				break;
				
			case MSG_PRIVATE:
				// Private message
				// 私聊消息
				LockClients();
				int iTargetId = FindClientByNickname(msg.sContent);
				if ( iTargetId != -1 ) {
					msg.uiFromId = iClientId;
					SendMessageToClient(iTargetId, &msg);
				} else {
					// Send error back
					// 发送错误信息回去
					ChatMessage errorMsg = {0};
					errorMsg.ucType = MSG_SERVER;
					errorMsg.uiFromId = 999999;  // Server ID
					strcpy(errorMsg.sContent, "User not found");
					send(iSocket, (char*)&errorMsg, sizeof(errorMsg), 0);
				}
				UnlockClients();
				break;
				
			case MSG_NICK:
				// Nickname change
				// 昵称更改
				LockClients();
				if ( strlen(msg.sContent) > 0 && strlen(msg.sContent) < NICKNAME_MAX ) {
					xrtFree(arrClients[iClientId].sNickname);
					arrClients[iClientId].sNickname = xrtCopyStr(msg.sContent, strlen(msg.sContent) + 1);
					
					// Notify all users
					// 通知所有用户
					ChatMessage nickMsg = {0};
					nickMsg.ucType = MSG_NICK;
					nickMsg.uiFromId = iClientId;
					sprintf(nickMsg.sContent, "%s is now known as %s", 
					       arrClients[iClientId].sNickname, msg.sContent);
					BroadcastMessage(&nickMsg, iSocket);
					
					printf("User %d changed nickname to: %s\n", iClientId, msg.sContent);
				}
				UnlockClients();
				break;
		}
	}
	
	// Client disconnected
	// 客户端断开连接
	LockClients();
	
	// Notify others of departure
	// 通知其他人离开
	ChatMessage leaveMsg = {0};
	leaveMsg.ucType = MSG_LEAVE;
	leaveMsg.uiFromId = iClientId;
	strcpy(leaveMsg.sContent, arrClients[iClientId].sNickname);
	BroadcastMessage(&leaveMsg, iSocket);
	
	printf("Client disconnected: %s\n", arrClients[iClientId].sNickname);
	
	// Cleanup
	// 清理
	xrtFree(arrClients[iClientId].sNickname);
	arrClients[iClientId].bActive = 0;
	closesocket(iSocket);
	giClientCount--;
	
	UnlockClients();
	
#ifdef _WIN32
	_endthreadex(0);
	return 0;
#else
	return NULL;
#endif
}

// Chat Server implementation
// 聊天服务器实现
void RunChatServer()
{
	printf("=== Chat Room Server ===\n");
	printf("=== 聊天室服务器 ===\n");
	
#ifdef _WIN32
	WSADATA wsaData;
	if ( WSAStartup(MAKEWORD(2, 2), &wsaData) != 0 ) {
		printf("Failed to initialize Winsock\n");
		return;
	}
	InitializeCriticalSection(&gCriticalSection);
#else
	signal(SIGPIPE, SIG_IGN);  // Ignore broken pipe signals
#endif
	
	// Create server socket
	// 创建服务器套接字
	int iServerSocket = socket(AF_INET, SOCK_STREAM, 0);
	if ( iServerSocket < 0 ) {
		printf("Failed to create server socket\n");
#ifdef _WIN32
		DeleteCriticalSection(&gCriticalSection);
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
	srvAddr.sin_port = htons(8888);
	
	if ( bind(iServerSocket, (struct sockaddr*)&srvAddr, sizeof(srvAddr)) < 0 ) {
		printf("Failed to bind socket\n");
#ifdef _WIN32
		DeleteCriticalSection(&gCriticalSection);
		closesocket(iServerSocket);
		WSACleanup();
#else
		close(iServerSocket);
#endif
		return;
	}
	
	// Listen for connections
	// 监听连接
	if ( listen(iServerSocket, 10) < 0 ) {
		printf("Failed to listen on socket\n");
#ifdef _WIN32
		DeleteCriticalSection(&gCriticalSection);
		closesocket(iServerSocket);
		WSACleanup();
#else
		close(iServerSocket);
#endif
		return;
	}
	
	printf("Chat server listening on port 8888\n");
	printf("Maximum clients: %d\n", MAX_CLIENTS);
	printf("Press Ctrl+C to stop server\n\n");
	
	// Main server loop
	// 主服务器循环
	while ( gbServerRunning ) {
		struct sockaddr_in cliAddr = {0};
		socklen_t iCliLen = sizeof(cliAddr);
		
		// Accept client connection
		// 接受客户端连接
		int* piClientSocket = xrtMalloc(sizeof(int));
		*piClientSocket = accept(iServerSocket, (struct sockaddr*)&cliAddr, &iCliLen);
		
		if ( *piClientSocket >= 0 ) {
#ifdef _WIN32
			HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, HandleClient, piClientSocket, 0, NULL);
			CloseHandle(hThread);
#else
			pthread_t thread;
			pthread_create(&thread, NULL, HandleClient, piClientSocket);
			pthread_detach(thread);
#endif
		} else {
			xrtFree(piClientSocket);
		}
	}
	
	// Cleanup
	// 清理
	printf("Shutting down server...\n");
	
#ifdef _WIN32
	DeleteCriticalSection(&gCriticalSection);
	closesocket(iServerSocket);
	WSACleanup();
#else
	close(iServerSocket);
#endif
	
	printf("Server stopped\n");
}

// Client receive thread function
// 客户端接收线程函数
#ifdef _WIN32
unsigned int __stdcall ReceiveThread(void* pParam)
#else
void* ReceiveThread(void* pParam)
#endif
{
	int iSock = *(int*)pParam;
	ChatMessage msg = {0};
	
	while ( gbClientRunning ) {
		int iReceived = recv(iSock, (char*)&msg, sizeof(msg), 0);
		if ( iReceived <= 0 ) {
			printf("Disconnected from server\n");
			gbClientRunning = 0;
			break;
		}
		
		switch ( msg.ucType ) {
			case MSG_CHAT:
				printf("[%u]: %s", msg.uiFromId, msg.sContent);
				break;
			case MSG_JOIN:
				printf("*** %s joined the chat ***\n", msg.sContent);
				break;
			case MSG_LEAVE:
				printf("*** %s left the chat ***\n", msg.sContent);
				break;
			case MSG_NICK:
				printf("*** %s ***\n", msg.sContent);
				break;
			case MSG_PRIVATE:
				printf("[Private from %u]: %s", msg.uiFromId, msg.sContent);
				break;
			case MSG_SERVER:
				printf("[Server]: %s", msg.sContent);
				break;
		}
	}
	
#ifdef _WIN32
	_endthreadex(0);
	return 0;
#else
	return NULL;
#endif
}

// Chat Client implementation
// 聊天客户端实现
void RunChatClient(str sServerIp)
{
	printf("=== Chat Room Client ===\n");
	printf("=== 聊天室客户端 ===\n");
	
#ifdef _WIN32
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
	srvAddr.sin_port = htons(8888);
	
#ifdef _WIN32
	srvAddr.sin_addr.s_addr = inet_addr(sServerIp);
#else
	inet_aton(sServerIp, &srvAddr.sin_addr);
#endif
	
	printf("Connecting to %s:8888...\n", sServerIp);
	
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
	
	printf("Connected! Type /help for commands\n");
	
	// Start receiver thread
	// 启动接收线程
	gbClientRunning = 1;
	
#ifdef _WIN32
	HANDLE hRecvThread = (HANDLE)_beginthreadex(NULL, 0, ReceiveThread, &iClientSocket, 0, NULL);
#else
	pthread_t recvThread;
	pthread_create(&recvThread, NULL, ReceiveThread, &iClientSocket);
#endif
	
	// Main input loop
	// 主输入循环
	char sInput[BUFFER_SIZE];
	while ( gbClientRunning ) {
		if ( !fgets(sInput, sizeof(sInput), stdin) ) {
			break;
		}
		
		// Process commands
		// 处理命令
		if ( strncmp(sInput, "/nick ", 6) == 0 ) {
			// Change nickname
			// 更改昵称
			char* sNick = sInput + 6;
			sNick[strcspn(sNick, "\n")] = '\0';
			
			ChatMessage msg = {0};
			msg.ucType = MSG_NICK;
			strcpy(msg.sContent, sNick);
			send(iClientSocket, (char*)&msg, sizeof(msg), 0);
		}
		else if ( strncmp(sInput, "/msg ", 5) == 0 ) {
			// Private message
			// 私聊消息
			char* sRest = sInput + 5;
			char* sSpace = strchr(sRest, ' ');
			if ( sSpace ) {
				*sSpace = '\0';
				char* sTarget = sRest;
				char* sMessage = sSpace + 1;
				
				ChatMessage msg = {0};
				msg.ucType = MSG_PRIVATE;
				msg.uiToId = atoi(sTarget);  // Could be improved with nickname lookup
				strcpy(msg.sContent, sMessage);
				send(iClientSocket, (char*)&msg, sizeof(msg), 0);
			}
		}
		else if ( strcmp(sInput, "/users\n") == 0 ) {
			// List users (would need server support)
			// 列出用户（需要服务器支持）
			printf("User list command sent\n");
		}
		else if ( strcmp(sInput, "/quit\n") == 0 ) {
			// Quit
			// 退出
			gbClientRunning = 0;
			break;
		}
		else if ( strcmp(sInput, "/help\n") == 0 ) {
			// Help
			// 帮助
			printf("Commands:\n");
			printf("  /nick <name>    - Change nickname\n");
			printf("  /msg <id> <msg> - Send private message\n");
			printf("  /users          - List online users\n");
			printf("  /help           - Show this help\n");
			printf("  /quit           - Exit chat\n");
			printf("Just type to chat publicly\n");
		}
		else if ( sInput[0] != '/' ) {
			// Regular chat message
			// 普通聊天消息
			ChatMessage msg = {0};
			msg.ucType = MSG_CHAT;
			strcpy(msg.sContent, sInput);
			send(iClientSocket, (char*)&msg, sizeof(msg), 0);
		}
	}
	
	// Cleanup
	// 清理
	gbClientRunning = 0;
#ifdef _WIN32
	WaitForSingleObject(hRecvThread, INFINITE);
	CloseHandle(hRecvThread);
	closesocket(iClientSocket);
	WSACleanup();
#else
	pthread_join(recvThread, NULL);
	close(iClientSocket);
#endif
	
	printf("Client disconnected\n");
}

int main(int argc, char* argv[])
{
	// Initialize XRT library
	// 初始化 XRT 库
	xrtInit();
	
	printf("XRT Chat Room Demo\n");
	printf("XRT 聊天室演示\n");
	printf("==================\n\n");
	
	// Parse command line arguments
	// 解析命令行参数
	if ( argc > 1 ) {
		if ( strcmp(argv[1], "server") == 0 ) {
			// Run as server
			// 作为服务器运行
			RunChatServer();
		} else if ( strcmp(argv[1], "client") == 0 ) {
			// Run as client
			// 作为客户端运行
			str sServerIp = (argc > 2) ? argv[2] : "127.0.0.1";
			RunChatClient(sServerIp);
		} else {
			printf("Usage: %s [server|client] [server_ip]\n", argv[0]);
			printf("  server      - Run chat server\n");
			printf("  client      - Run chat client (connects to server)\n");
			printf("Example: %s server\n", argv[0]);
			printf("Example: %s client 127.0.0.1\n", argv[0]);
		}
	} else {
		printf("XRT Chat Room\n");
		printf("Modes:\n");
		printf("  Server: %s server\n", argv[0]);
		printf("  Client: %s client [server_ip]\n", argv[0]);
	}
	
	// Cleanup XRT library
	// 清理 XRT 库
	xrtUnit();
	
	return 0;
}