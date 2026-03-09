



// WebSocket 客户端/服务器测试

// ==================== 测试回调变量 ====================

static volatile int __test_ws_server_open_count = 0;
static volatile int __test_ws_server_msg_count = 0;
static volatile int __test_ws_server_close_count = 0;
static volatile int __test_ws_server_ping_count = 0;
static volatile int __test_ws_server_pong_count = 0;
static char __test_ws_server_recv_data[256] = {0};
static int __test_ws_server_recv_opcode = 0;

static volatile int __test_ws_client_open_count = 0;
static volatile int __test_ws_client_msg_count = 0;
static volatile int __test_ws_client_close_count = 0;
static volatile int __test_ws_client_ping_count = 0;
static volatile int __test_ws_client_pong_count = 0;
static char __test_ws_client_recv_data[256] = {0};
static int __test_ws_client_recv_opcode = 0;

static xwsserver* __test_ws_server = NULL;


// ==================== 服务器回调 ====================

static void __test_ws_server_on_open(ptr pOwner, xnetconn* pConn)
{
	__test_ws_server_open_count++;
}

static void __test_ws_server_on_message(ptr pOwner, xnetconn* pConn, int iOpcode, const char* pData, size_t iLen)
{
	__test_ws_server_msg_count++;
	__test_ws_server_recv_opcode = iOpcode;
	if ( iLen < sizeof(__test_ws_server_recv_data) ) {
		memcpy(__test_ws_server_recv_data, pData, iLen);
		__test_ws_server_recv_data[iLen] = '\0';
	}
	
	// Echo: 将收到的消息发回客户端
	if ( __test_ws_server && pConn ) {
		if ( iOpcode == XRT_WS_OP_TEXT ) {
			xrtWsServerSendText(__test_ws_server, pConn->iId, pData, iLen);
		} else {
			xrtWsServerSendBinary(__test_ws_server, pConn->iId, pData, iLen);
		}
	}
}

static void __test_ws_server_on_close(ptr pOwner, xnetconn* pConn, uint16 iCode, const char* sReason)
{
	__test_ws_server_close_count++;
}

static void __test_ws_server_on_ping(ptr pOwner, xnetconn* pConn, const char* pData, size_t iLen)
{
	__test_ws_server_ping_count++;
}

static void __test_ws_server_on_pong(ptr pOwner, xnetconn* pConn, const char* pData, size_t iLen)
{
	__test_ws_server_pong_count++;
}


// ==================== 客户端回调 ====================

static void __test_ws_client_on_open(ptr pOwner, xnetconn* pConn)
{
	__test_ws_client_open_count++;
}

static void __test_ws_client_on_message(ptr pOwner, xnetconn* pConn, int iOpcode, const char* pData, size_t iLen)
{
	__test_ws_client_msg_count++;
	__test_ws_client_recv_opcode = iOpcode;
	if ( iLen < sizeof(__test_ws_client_recv_data) ) {
		memcpy(__test_ws_client_recv_data, pData, iLen);
		__test_ws_client_recv_data[iLen] = '\0';
	}
}

static void __test_ws_client_on_close(ptr pOwner, xnetconn* pConn, uint16 iCode, const char* sReason)
{
	__test_ws_client_close_count++;
}

static void __test_ws_client_on_ping(ptr pOwner, xnetconn* pConn, const char* pData, size_t iLen)
{
	__test_ws_client_ping_count++;
}

static void __test_ws_client_on_pong(ptr pOwner, xnetconn* pConn, const char* pData, size_t iLen)
{
	__test_ws_client_pong_count++;
}


// ==================== 辅助函数 ====================

static void __test_ws_sleep(int ms)
{
#if defined(_WIN32) || defined(_WIN64)
	Sleep(ms);
#else
	usleep(ms * 1000);
#endif
}


// ==================== 主测试函数 ====================

void Test_NetWS(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n WebSocket 测试 :\n\n");
	
	// 重置计数
	__test_ws_server_open_count = 0;
	__test_ws_server_msg_count = 0;
	__test_ws_server_close_count = 0;
	__test_ws_server_ping_count = 0;
	__test_ws_server_pong_count = 0;
	__test_ws_client_open_count = 0;
	__test_ws_client_msg_count = 0;
	__test_ws_client_close_count = 0;
	__test_ws_client_ping_count = 0;
	__test_ws_client_pong_count = 0;
	memset(__test_ws_server_recv_data, 0, sizeof(__test_ws_server_recv_data));
	memset(__test_ws_client_recv_data, 0, sizeof(__test_ws_client_recv_data));
	
	// ==================== WebSocket 服务器测试 ====================
	printf("--- WebSocket 服务器创建 ---\n");
	
	xwsconfig tServerConfig = {0};
	tServerConfig.iMaxMessageSize = 64 * 1024;  // 64KB
	
	xwsevents tServerEvents = {0};
	tServerEvents.OnOpen = __test_ws_server_on_open;
	tServerEvents.OnMessage = __test_ws_server_on_message;
	tServerEvents.OnClose = __test_ws_server_on_close;
	tServerEvents.OnPing = __test_ws_server_on_ping;
	tServerEvents.OnPong = __test_ws_server_on_pong;
	
	xwsserver* pServer = xrtWsServerCreate("127.0.0.1", 19877, &tServerConfig, &tServerEvents);
	__test_ws_server = pServer;  // 保存引用供回调使用
	printf("  WS Server Create : %s\n", pServer ? "PASS" : "FAIL");
	
	if ( !pServer ) {
		printf("  [ERROR] Failed to create WebSocket server\n");
		return;
	}
	
	// 启动服务器
	xnet_result iRes = xrtWsServerStart(pServer);
	printf("  WS Server Start : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
	
	if ( iRes != XRT_NET_OK ) {
		xrtWsServerDestroy(pServer);
		return;
	}
	
	// 等待服务器准备好
	__test_ws_sleep(100);
	
	// ==================== WebSocket 客户端测试 ====================
	printf("\n--- WebSocket 客户端创建 ---\n");
	
	xwsconfig tClientConfig = {0};
	tClientConfig.iMaxMessageSize = 64 * 1024;
	
	xwsevents tClientEvents = {0};
	tClientEvents.OnOpen = __test_ws_client_on_open;
	tClientEvents.OnMessage = __test_ws_client_on_message;
	tClientEvents.OnClose = __test_ws_client_on_close;
	tClientEvents.OnPing = __test_ws_client_on_ping;
	tClientEvents.OnPong = __test_ws_client_on_pong;
	
	xwsclient* pClient = xrtWsClientCreate("ws://127.0.0.1:19877/", &tClientConfig, &tClientEvents);
	printf("  WS Client Create : %s\n", pClient ? "PASS" : "FAIL");
	
	if ( !pClient ) {
		printf("  [ERROR] Failed to create WebSocket client\n");
		xrtWsServerStop(pServer);
		xrtWsServerDestroy(pServer);
		return;
	}
	
	// 连接服务器
	iRes = xrtWsClientConnect(pClient);
	printf("  WS Client Connect : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
	
	// 等待握手完成
	__test_ws_sleep(500);
	
	printf("\n--- WebSocket 握手验证 ---\n");
	printf("  Server OnOpen Count : %d (%s)\n", __test_ws_server_open_count, 
		__test_ws_server_open_count > 0 ? "PASS" : "FAIL");
	printf("  Client OnOpen Count : %d (%s)\n", __test_ws_client_open_count,
		__test_ws_client_open_count > 0 ? "PASS" : "FAIL");
	printf("  Client IsConnected : %s\n", xrtWsClientIsConnected(pClient) ? "PASS" : "FAIL");
	
	// ==================== 消息收发测试 ====================
	printf("\n--- WebSocket 消息收发测试 ---\n");
	
	// 发送文本消息
	const char* sTestMsg = "Hello WebSocket!";
	iRes = xrtWsClientSendText(pClient, sTestMsg, strlen(sTestMsg));
	printf("  Send Text : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
	
	// 等待消息处理
	__test_ws_sleep(200);
	
	// 验证服务器收到消息
	printf("  Server Recv Count : %d (%s)\n", __test_ws_server_msg_count,
		__test_ws_server_msg_count > 0 ? "PASS" : "FAIL");
	printf("  Server Recv Opcode : %d (%s)\n", __test_ws_server_recv_opcode,
		__test_ws_server_recv_opcode == XRT_WS_OP_TEXT ? "PASS" : "FAIL");
	printf("  Server Recv Data : \"%s\" (%s)\n", __test_ws_server_recv_data,
		strcmp(__test_ws_server_recv_data, sTestMsg) == 0 ? "PASS" : "FAIL");
	
	// 验证客户端收到回声
	printf("  Client Recv Count : %d (%s)\n", __test_ws_client_msg_count,
		__test_ws_client_msg_count > 0 ? "PASS" : "FAIL");
	printf("  Client Recv Data : \"%s\" (%s)\n", __test_ws_client_recv_data,
		strcmp(__test_ws_client_recv_data, sTestMsg) == 0 ? "PASS" : "FAIL");
	
	// ==================== 二进制消息测试 ====================
	printf("\n--- WebSocket 二进制消息测试 ---\n");
	
	// 重置接收数据
	memset(__test_ws_server_recv_data, 0, sizeof(__test_ws_server_recv_data));
	memset(__test_ws_client_recv_data, 0, sizeof(__test_ws_client_recv_data));
	int iOldServerCount = __test_ws_server_msg_count;
	int iOldClientCount = __test_ws_client_msg_count;
	
	// 发送二进制数据
	uint8 aBinaryData[] = {0x01, 0x02, 0x03, 0x04, 0x05};
	iRes = xrtWsClientSendBinary(pClient, (char*)aBinaryData, sizeof(aBinaryData));
	printf("  Send Binary : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
	
	__test_ws_sleep(200);
	
	printf("  Server Recv Binary Count : %d (%s)\n", 
		__test_ws_server_msg_count - iOldServerCount,
		__test_ws_server_msg_count > iOldServerCount ? "PASS" : "FAIL");
	printf("  Server Recv Binary Opcode : %d (%s)\n", __test_ws_server_recv_opcode,
		__test_ws_server_recv_opcode == XRT_WS_OP_BINARY ? "PASS" : "FAIL");
	printf("  Client Recv Binary Count : %d (%s)\n",
		__test_ws_client_msg_count - iOldClientCount,
		__test_ws_client_msg_count > iOldClientCount ? "PASS" : "FAIL");
	
	// ==================== Ping 测试 ====================
	printf("\n--- WebSocket Ping 测试 ---\n");
	
	const char* sPingData = "ping";
	iRes = xrtWsClientPing(pClient, sPingData, strlen(sPingData));
	printf("  Send Ping : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
	
	__test_ws_sleep(200);
	
	printf("  Server Recv Ping Count : %d (%s)\n", __test_ws_server_ping_count,
		__test_ws_server_ping_count > 0 ? "PASS" : "FAIL");
	printf("  Client Recv Pong Count : %d (%s)\n", __test_ws_client_pong_count,
		__test_ws_client_pong_count > 0 ? "PASS" : "FAIL");
	
	// ==================== Close 测试 ====================
	printf("\n--- WebSocket 关闭测试 ---\n");
	
	iRes = xrtWsClientClose(pClient, XRT_WS_CLOSE_NORMAL, "Test close");
	printf("  Send Close : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
	
	__test_ws_sleep(200);
	
	printf("  Server Close Count : %d (%s)\n", __test_ws_server_close_count,
		__test_ws_server_close_count > 0 ? "PASS" : "FAIL");
	printf("  Client IsConnected : %s\n", !xrtWsClientIsConnected(pClient) ? "PASS" : "FAIL");
	
	// ==================== 清理 ====================
	printf("\n--- 清理资源 ---\n");
	
	xrtWsClientDestroy(pClient);
	printf("  WS Client Destroy : PASS\n");
	
	xrtWsServerStop(pServer);
	__test_ws_server = NULL;
	xrtWsServerDestroy(pServer);
	printf("  WS Server Destroy : PASS\n");
	
	printf("\n WebSocket 测试完成!\n");
}

