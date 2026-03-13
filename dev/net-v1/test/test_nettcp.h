



// TCP/UDP 网络通信测试

// ==================== TCP 测试回调 ====================

static volatile int __test_tcp_server_accept_count = 0;
static volatile int __test_tcp_server_recv_count = 0;
static volatile int __test_tcp_server_close_count = 0;
static char __test_tcp_server_recv_data[256] = {0};

static volatile int __test_tcp_client_connect_count = 0;
static volatile int __test_tcp_client_recv_count = 0;
static volatile int __test_tcp_client_close_count = 0;
static char __test_tcp_client_recv_data[256] = {0};

static void __test_tcp_on_accept(ptr pServer, xnetconn* pConn)
{
	__test_tcp_server_accept_count++;
}
static void __test_tcp_on_server_recv(ptr pServer, xnetconn* pConn, const char* pData, size_t iLen)
{
	__test_tcp_server_recv_count++;
	if ( iLen < sizeof(__test_tcp_server_recv_data) ) {
		memcpy(__test_tcp_server_recv_data, pData, iLen);
		__test_tcp_server_recv_data[iLen] = '\0';
	}
	// 回声：将收到的数据发回
	xtcpserver* pSrv = (xtcpserver*)pServer;
	xrtTcpServerSend(pSrv, pConn->iId, pData, iLen);
}
static void __test_tcp_on_server_close(ptr pServer, xnetconn* pConn)
{
	__test_tcp_server_close_count++;
}

static void __test_tcp_on_connect(ptr pClient, xnetconn* pConn, bool bSuccess)
{
	__test_tcp_client_connect_count++;
}
static void __test_tcp_on_client_recv(ptr pClient, xnetconn* pConn, const char* pData, size_t iLen)
{
	__test_tcp_client_recv_count++;
	if ( iLen < sizeof(__test_tcp_client_recv_data) ) {
		memcpy(__test_tcp_client_recv_data, pData, iLen);
		__test_tcp_client_recv_data[iLen] = '\0';
	}
}
static void __test_tcp_on_client_close(ptr pClient, xnetconn* pConn)
{
	__test_tcp_client_close_count++;
}



// ==================== UDP 测试回调 ====================

static volatile int __test_udp_server_recv_count = 0;
static char __test_udp_server_recv_data[256] = {0};
static xnetaddr __test_udp_server_from_addr = {0};

static volatile int __test_udp_client_recv_count = 0;
static char __test_udp_client_recv_data[256] = {0};

static void __test_udp_on_server_recv(ptr pServer, xnetconn* pConn, const char* pData, size_t iLen)
{
	__test_udp_server_recv_count++;
	if ( iLen < sizeof(__test_udp_server_recv_data) ) {
		memcpy(__test_udp_server_recv_data, pData, iLen);
		__test_udp_server_recv_data[iLen] = '\0';
	}
	__test_udp_server_from_addr = pConn->tRemoteAddr;
	
	// 回声
	xudpserver* pSrv = (xudpserver*)pServer;
	xrtUdpServerSendTo(pSrv, &pConn->tRemoteAddr, pData, iLen);
}

static void __test_udp_on_client_recv(ptr pClient, xnetconn* pConn, const char* pData, size_t iLen)
{
	__test_udp_client_recv_count++;
	if ( iLen < sizeof(__test_udp_client_recv_data) ) {
		memcpy(__test_udp_client_recv_data, pData, iLen);
		__test_udp_client_recv_data[iLen] = '\0';
	}
}



// ==================== 主测试函数 ====================

void Test_NetTCPUDP(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n TCP/UDP 网络通信测试 :\n\n");
	
	// ==================== TCP 服务器/客户端测试 ====================
	printf("--- TCP 服务器/客户端测试 ---\n");
	
	// 重置计数
	__test_tcp_server_accept_count = 0;
	__test_tcp_server_recv_count = 0;
	__test_tcp_server_close_count = 0;
	__test_tcp_client_connect_count = 0;
	__test_tcp_client_recv_count = 0;
	__test_tcp_client_close_count = 0;
	memset(__test_tcp_server_recv_data, 0, sizeof(__test_tcp_server_recv_data));
	memset(__test_tcp_client_recv_data, 0, sizeof(__test_tcp_client_recv_data));
	
	xnetconfig tTcpConfig = {0};
	tTcpConfig.iRecvBufSize = 4096;
	tTcpConfig.iSendBufSize = 4096;
	tTcpConfig.iMaxClients = 16;
	tTcpConfig.iPollTimeoutMs = 5;
	
	xnetevents tServerEvents = {0};
	tServerEvents.OnAccept = __test_tcp_on_accept;
	tServerEvents.OnRecv = __test_tcp_on_server_recv;
	tServerEvents.OnClose = __test_tcp_on_server_close;
	
	// 创建服务器
	xtcpserver* pServer = xrtTcpServerCreate("127.0.0.1", 19876, &tTcpConfig, &tServerEvents);
	printf("  TCP Server Create : %s\n", pServer ? "PASS" : "FAIL");
	
	if ( pServer ) {
		// 启动服务器
		xnet_result iRes = xrtTcpServerStart(pServer);
		printf("  TCP Server Start : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
		
		// 等一下让服务器准备好
		#if defined(_WIN32) || defined(_WIN64)
			Sleep(50);
		#else
			usleep(50000);
		#endif
		
		// 创建客户端
		xnetevents tClientEvents = {0};
		tClientEvents.OnConnect = __test_tcp_on_connect;
		tClientEvents.OnRecv = __test_tcp_on_client_recv;
		tClientEvents.OnClose = __test_tcp_on_client_close;
		
		xtcpclient* pClient = xrtTcpClientCreate("127.0.0.1", 19876, &tTcpConfig, &tClientEvents);
		printf("  TCP Client Create : %s\n", pClient ? "PASS" : "FAIL");
		
		if ( pClient ) {
			// 连接服务器
			iRes = xrtTcpClientConnect(pClient);
			printf("  TCP Client Connect : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
			
			// 等待服务器 accept
			#if defined(_WIN32) || defined(_WIN64)
				Sleep(100);
			#else
				usleep(100000);
			#endif
			
			printf("  Server Accept Count : %d (%s)\n",
				__test_tcp_server_accept_count,
				__test_tcp_server_accept_count >= 1 ? "PASS" : "FAIL");
			
			// 发送数据
			const char* sMsg = "Hello TCP Server!";
			iRes = xrtTcpClientSend(pClient, sMsg, strlen(sMsg));
			printf("  TCP Client Send : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
			
			// 等待回声
			#if defined(_WIN32) || defined(_WIN64)
				Sleep(200);
			#else
				usleep(200000);
			#endif
			
			printf("  Server Recv Count : %d (%s)\n",
				__test_tcp_server_recv_count,
				__test_tcp_server_recv_count >= 1 ? "PASS" : "FAIL");
			printf("  Server Recv Data : \"%s\" (%s)\n",
				__test_tcp_server_recv_data,
				strcmp(__test_tcp_server_recv_data, "Hello TCP Server!") == 0 ? "PASS" : "FAIL");
			
			printf("  Client Recv Count (echo) : %d (%s)\n",
				__test_tcp_client_recv_count,
				__test_tcp_client_recv_count >= 1 ? "PASS" : "FAIL");
			printf("  Client Recv Data (echo) : \"%s\" (%s)\n",
				__test_tcp_client_recv_data,
				strcmp(__test_tcp_client_recv_data, "Hello TCP Server!") == 0 ? "PASS" : "FAIL");
			
			printf("  TCP IsConnected : %s\n",
				xrtTcpClientIsConnected(pClient) ? "PASS" : "FAIL");
			
			// 断开客户端
			xrtTcpClientDisconnect(pClient);
			printf("  TCP Client Disconnect : PASS\n");
			
			// 等待服务器检测到断开
			#if defined(_WIN32) || defined(_WIN64)
				Sleep(100);
			#else
				usleep(100000);
			#endif
			
			// 销毁客户端
			xrtTcpClientDestroy(pClient);
			printf("  TCP Client Destroy : PASS\n");
		}
		
		// 测试 UserData
		xrtTcpServerSetUserData(pServer, (ptr)0x12345678);
		printf("  TCP Server UserData : %s\n",
			xrtTcpServerGetUserData(pServer) == (ptr)0x12345678 ? "PASS" : "FAIL");
		
		// 停止并销毁服务器
		xrtTcpServerStop(pServer);
		printf("  TCP Server Stop : PASS\n");
		
		xrtTcpServerDestroy(pServer);
		printf("  TCP Server Destroy : PASS\n");
	}
	
	
	
	// ==================== UDP 服务器/客户端测试 ====================
	printf("\n--- UDP 服务器/客户端测试 ---\n");
	
	// 重置计数
	__test_udp_server_recv_count = 0;
	__test_udp_client_recv_count = 0;
	memset(__test_udp_server_recv_data, 0, sizeof(__test_udp_server_recv_data));
	memset(__test_udp_client_recv_data, 0, sizeof(__test_udp_client_recv_data));
	
	xnetconfig tUdpConfig = {0};
	tUdpConfig.iRecvBufSize = 4096;
	tUdpConfig.iSendBufSize = 4096;
	tUdpConfig.iPollTimeoutMs = 5;
	
	xnetevents tUdpServerEvents = {0};
	tUdpServerEvents.OnRecv = __test_udp_on_server_recv;
	
	// 创建 UDP 服务器
	xudpserver* pUdpServer = xrtUdpServerCreate("127.0.0.1", 19877, &tUdpConfig, &tUdpServerEvents);
	printf("  UDP Server Create : %s\n", pUdpServer ? "PASS" : "FAIL");
	
	if ( pUdpServer ) {
		xnet_result iRes = xrtUdpServerStart(pUdpServer);
		printf("  UDP Server Start : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
		
		#if defined(_WIN32) || defined(_WIN64)
			Sleep(50);
		#else
			usleep(50000);
		#endif
		
		// 简单的手动 UDP 回声测试（不依赖客户端线程）
		{
			xnetconn tTestSock;
			memset(&tTestSock, 0, sizeof(xnetconn));
			xrtSockCreate(&tTestSock, 1);  // UDP
			xrtSockSetNonBlock(&tTestSock);  // 非阻塞避免 recvfrom 卡死
			
			xnetaddr tSvrAddr;
			xrtNetAddrInit(&tSvrAddr, "127.0.0.1", 19877);
			
			const char* sTestMsg = "Manual UDP Test";
			size_t iSentManual = 0;
			xnet_result iSendRes = xrtSockSendTo(&tTestSock, sTestMsg, strlen(sTestMsg), &tSvrAddr, &iSentManual);
			printf("  Manual UDP Send : %s (sent=%d)\n", iSendRes == XRT_NET_OK ? "PASS" : "FAIL", (int)iSentManual);
			
			// 轮询等待回声，最多 500ms
			xnetaddr tFromAddrManual;
			char aManBuf[256];
			size_t iManRecvd = 0;
			xnet_result iManRes = XRT_NET_AGAIN;
			for ( int iRetry = 0; iRetry < 50; iRetry++ ) {
				iManRes = xrtSockRecvFrom(&tTestSock, aManBuf, sizeof(aManBuf), &tFromAddrManual, &iManRecvd);
				if ( (iManRes == XRT_NET_OK) && (iManRecvd > 0) ) break;
				#if defined(_WIN32) || defined(_WIN64)
					Sleep(10);
				#else
					usleep(10000);
				#endif
			}
			if ( (iManRes == XRT_NET_OK) && (iManRecvd > 0) ) {
				aManBuf[iManRecvd] = '\0';
				printf("  Manual UDP Echo : \"%s\" (%s)\n", aManBuf,
					strcmp(aManBuf, "Manual UDP Test") == 0 ? "PASS" : "FAIL");
			} else {
				printf("  Manual UDP Echo : FAIL (res=%d, recvd=%d)\n", (int)iManRes, (int)iManRecvd);
			}
			
			xrtSockClose(&tTestSock);
		}
		
		// 创建 UDP 客户端
		xnetevents tUdpClientEvents = {0};
		tUdpClientEvents.OnRecv = __test_udp_on_client_recv;
		
		xudpclient* pUdpClient = xrtUdpClientCreate(&tUdpConfig, &tUdpClientEvents);
		printf("  UDP Client Create : %s\n", pUdpClient ? "PASS" : "FAIL");
		
		if ( pUdpClient ) {
			iRes = xrtUdpClientStart(pUdpClient);
			printf("  UDP Client Start : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
			
			// 发送数据到服务器
			xnetaddr tServerAddr;
			xrtNetAddrInit(&tServerAddr, "127.0.0.1", 19877);
			
			const char* sMsg = "Hello UDP Server!";
			iRes = xrtUdpClientSendTo(pUdpClient, &tServerAddr, sMsg, strlen(sMsg));
			printf("  UDP Client SendTo : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
			
			// 等待回声（需要足够时间让服务器收到、回声、客户端接收）
			#if defined(_WIN32) || defined(_WIN64)
				Sleep(500);
			#else
				usleep(500000);
			#endif
			
			printf("  UDP Server Recv Count : %d (%s)\n",
				__test_udp_server_recv_count,
				__test_udp_server_recv_count >= 1 ? "PASS" : "FAIL");
			printf("  UDP Server Recv Data : \"%s\" (%s)\n",
				__test_udp_server_recv_data,
				strcmp(__test_udp_server_recv_data, "Hello UDP Server!") == 0 ? "PASS" : "FAIL");
			printf("  UDP Server From Addr : %s:%d\n",
				__test_udp_server_from_addr.sAddr,
				__test_udp_server_from_addr.iPort);
			
			printf("  UDP Client Recv Count (echo) : %d (%s)\n",
				__test_udp_client_recv_count,
				__test_udp_client_recv_count >= 1 ? "PASS" : "FAIL");
			printf("  UDP Client Recv Data (echo) : \"%s\" (%s)\n",
				__test_udp_client_recv_data,
				strcmp(__test_udp_client_recv_data, "Hello UDP Server!") == 0 ? "PASS" : "FAIL");
			
			// UserData 测试
			xrtUdpClientSetUserData(pUdpClient, (ptr)0xABCD1234);
			printf("  UDP Client UserData : %s\n",
				xrtUdpClientGetUserData(pUdpClient) == (ptr)0xABCD1234 ? "PASS" : "FAIL");
			
			xrtUdpClientStop(pUdpClient);
			printf("  UDP Client Stop : PASS\n");
			xrtUdpClientDestroy(pUdpClient);
			printf("  UDP Client Destroy : PASS\n");
		}
		
		xrtUdpServerStop(pUdpServer);
		printf("  UDP Server Stop : PASS\n");
		xrtUdpServerDestroy(pUdpServer);
		printf("  UDP Server Destroy : PASS\n");
	}
}


