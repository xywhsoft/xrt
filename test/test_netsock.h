



// Socket 基础操作测试
void Test_NetSock(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Socket 基础操作测试 :\n\n");
	
	// ==================== 地址操作测试 ====================
	printf("--- 地址操作测试 ---\n");
	
	{
		xnetaddr tAddr;
		xrtNetAddrInit(&tAddr, "192.168.1.100", 8080);
		printf("  AddrInit: ip=%s port=%u : %s\n", tAddr.sAddr, tAddr.iPort,
			(strcmp(tAddr.sAddr, "192.168.1.100") == 0 && tAddr.iPort == 8080) ? "PASS" : "FAIL");
	}
	
	{
		xnetaddr tAddr;
		xrtNetAddrInit(&tAddr, NULL, 80);
		printf("  AddrInit(NULL): ip=%s port=%u : %s\n", tAddr.sAddr, tAddr.iPort,
			(strcmp(tAddr.sAddr, "0.0.0.0") == 0 && tAddr.iPort == 80) ? "PASS" : "FAIL");
	}
	
	{
		uint32 iIP = xrtNetIPFromStr("10.0.0.1");
		const char* sIP = xrtNetIPToStr(iIP);
		printf("  IP 转换往返: %s : %s\n", sIP,
			(strcmp(sIP, "10.0.0.1") == 0) ? "PASS" : "FAIL");
	}
	
	{
		xnetaddr tAddr;
		xrtNetAddrInit(&tAddr, "127.0.0.1", 12345);
		struct sockaddr_in tSA;
		xrtNetAddrToSockAddr(&tAddr, &tSA);
		
		xnetaddr tAddr2;
		xrtNetAddrFromSockAddr(&tAddr2, &tSA);
		printf("  sockaddr 往返: ip=%s port=%u : %s\n", tAddr2.sAddr, tAddr2.iPort,
			(strcmp(tAddr2.sAddr, "127.0.0.1") == 0 && tAddr2.iPort == 12345) ? "PASS" : "FAIL");
	}
	
	// ==================== Socket 创建/关闭测试 ====================
	printf("\n--- Socket 创建/关闭测试 ---\n");
	
	{
		xnetconn tConn;
		xnet_result iRes = xrtSockCreate(&tConn, 0);
		printf("  TCP Socket 创建 : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
		if ( iRes == XRT_NET_OK ) {
			printf("  TCP Socket 有效 : %s\n", tConn.hSocket != XSOCKET_INVALID ? "PASS" : "FAIL");
			xrtSockClose(&tConn);
			printf("  TCP Socket 关闭 : %s\n", tConn.hSocket == XSOCKET_INVALID ? "PASS" : "FAIL");
		}
	}
	
	{
		xnetconn tConn;
		xnet_result iRes = xrtSockCreate(&tConn, 1);
		printf("  UDP Socket 创建 : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
		if ( iRes == XRT_NET_OK ) {
			xrtSockClose(&tConn);
			printf("  UDP Socket 关闭 : %s\n", tConn.hSocket == XSOCKET_INVALID ? "PASS" : "FAIL");
		}
	}
	
	// ==================== Socket 选项测试 ====================
	printf("\n--- Socket 选项测试 ---\n");
	
	{
		xnetconn tConn;
		xrtSockCreate(&tConn, 0);
		
		xnet_result iRes;
		iRes = xrtSockSetNonBlock(&tConn);
		printf("  SetNonBlock : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
		
		iRes = xrtSockSetReuseAddr(&tConn);
		printf("  SetReuseAddr : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
		
		iRes = xrtSockSetNoDelay(&tConn);
		printf("  SetNoDelay : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
		
		iRes = xrtSockSetTimeout(&tConn, 5000, 5000);
		printf("  SetTimeout : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
		
		iRes = xrtSockSetKeepAlive(&tConn, 60, 10, 3);
		printf("  SetKeepAlive : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
		
		xrtSockClose(&tConn);
	}
	
	// ==================== Bind + Listen 测试 ====================
	printf("\n--- Bind + Listen 测试 ---\n");
	
	{
		xnetconn tServer;
		xrtSockCreate(&tServer, 0);
		xrtSockSetReuseAddr(&tServer);
		
		xnetaddr tAddr;
		xrtNetAddrInit(&tAddr, "127.0.0.1", 0);  // 端口 0 = 系统分配
		
		xnet_result iRes = xrtSockBind(&tServer, &tAddr);
		printf("  Bind(port=0) : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
		
		iRes = xrtSockListen(&tServer, 5);
		printf("  Listen : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
		
		xrtSockClose(&tServer);
	}
	
	// ==================== Connect + Accept 测试 ====================
	printf("\n--- Connect + Accept 测试 ---\n");
	
	{
		// 创建服务器
		xnetconn tServer;
		xrtSockCreate(&tServer, 0);
		xrtSockSetReuseAddr(&tServer);
		
		xnetaddr tServerAddr;
		xrtNetAddrInit(&tServerAddr, "127.0.0.1", 19876);
		
		xnet_result iRes = xrtSockBind(&tServer, &tServerAddr);
		if ( iRes != XRT_NET_OK ) {
			printf("  Server Bind 失败\n");
			xrtSockClose(&tServer);
		} else {
			xrtSockListen(&tServer, 5);
			
			// 创建客户端并连接
			xnetconn tClient;
			xrtSockCreate(&tClient, 0);
			
			iRes = xrtSockConnect(&tClient, &tServerAddr);
			printf("  Client Connect : %s\n", (iRes == XRT_NET_OK || iRes == XRT_NET_AGAIN) ? "PASS" : "FAIL");
			
			// Accept
			xnetconn tAccepted;
			iRes = xrtSockAccept(&tServer, &tAccepted);
			printf("  Server Accept : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
			
			if ( iRes == XRT_NET_OK ) {
				printf("  Accepted from: %s:%u\n", tAccepted.tRemoteAddr.sAddr, tAccepted.tRemoteAddr.iPort);
				
				// 发送和接收数据
				const char* sMsg = "Hello, XRT Socket!";
				size_t iSent = 0;
				iRes = xrtSockSend(&tClient, sMsg, strlen(sMsg), &iSent);
				printf("  Send %zu bytes : %s\n", iSent, iRes == XRT_NET_OK ? "PASS" : "FAIL");
				
				char sBuf[256] = {0};
				size_t iRecvd = 0;
				iRes = xrtSockRecv(&tAccepted, sBuf, sizeof(sBuf), &iRecvd);
				printf("  Recv %zu bytes : %s\n", iRecvd, iRes == XRT_NET_OK ? "PASS" : "FAIL");
				printf("  数据匹配 : %s\n", (iRecvd == strlen(sMsg) && memcmp(sBuf, sMsg, iRecvd) == 0) ? "PASS" : "FAIL");
				
				xrtSockClose(&tAccepted);
			}
			
			xrtSockClose(&tClient);
			xrtSockClose(&tServer);
		}
	}
	
	// ==================== UDP 收发测试 ====================
	printf("\n--- UDP 收发测试 ---\n");
	
	{
		xnetconn tServer, tClient;
		xrtSockCreate(&tServer, 1);
		xrtSockCreate(&tClient, 1);
		
		xnetaddr tServerAddr;
		xrtNetAddrInit(&tServerAddr, "127.0.0.1", 19877);
		xrtSockBind(&tServer, &tServerAddr);
		
		const char* sMsg = "Hello, UDP!";
		size_t iSent = 0;
		xnet_result iRes = xrtSockSendTo(&tClient, sMsg, strlen(sMsg), &tServerAddr, &iSent);
		printf("  UDP SendTo : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
		
		char sBuf[256] = {0};
		size_t iRecvd = 0;
		xnetaddr tFromAddr;
		iRes = xrtSockRecvFrom(&tServer, sBuf, sizeof(sBuf), &tFromAddr, &iRecvd);
		printf("  UDP RecvFrom : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  UDP 数据匹配 : %s\n", (iRecvd == strlen(sMsg) && memcmp(sBuf, sMsg, iRecvd) == 0) ? "PASS" : "FAIL");
		printf("  UDP 来源地址 : %s:%u\n", tFromAddr.sAddr, tFromAddr.iPort);
		
		xrtSockClose(&tClient);
		xrtSockClose(&tServer);
	}
	
	// ==================== 网络缓冲区测试 ====================
	printf("\n--- 网络缓冲区测试 ---\n");
	
	{
		xnetbuf tBuf;
		bool bOK = xrtNetBufInit(&tBuf, 64);
		printf("  BufInit : %s\n", bOK ? "PASS" : "FAIL");
		
		bOK = xrtNetBufAppend(&tBuf, "Hello, ", 7);
		printf("  BufAppend(7) : %s (size=%zu)\n", bOK ? "PASS" : "FAIL", tBuf.iSize);
		
		bOK = xrtNetBufAppend(&tBuf, "World!", 6);
		printf("  BufAppend(6) : %s (size=%zu)\n", bOK ? "PASS" : "FAIL", tBuf.iSize);
		
		printf("  BufContent : %s\n", (tBuf.iSize == 13 && memcmp(tBuf.pData, "Hello, World!", 13) == 0) ? "PASS" : "FAIL");
		
		xrtNetBufConsume(&tBuf, 7);
		printf("  BufConsume(7) : %s (size=%zu)\n", (tBuf.iSize == 6 && memcmp(tBuf.pData, "World!", 6) == 0) ? "PASS" : "FAIL", tBuf.iSize);
		
		xrtNetBufClear(&tBuf);
		printf("  BufClear : %s (size=%zu)\n", tBuf.iSize == 0 ? "PASS" : "FAIL", tBuf.iSize);
		
		// 测试扩容
		char sLong[200];
		memset(sLong, 'A', 200);
		bOK = xrtNetBufAppend(&tBuf, sLong, 200);
		printf("  BufAppend(200, 扩容) : %s (cap=%zu)\n", (bOK && tBuf.iSize == 200) ? "PASS" : "FAIL", tBuf.iCapacity);
		
		xrtNetBufFree(&tBuf);
		printf("  BufFree : %s\n", tBuf.pData == NULL ? "PASS" : "FAIL");
	}
	
	// ==================== DNS 解析测试 ====================
	printf("\n--- DNS 解析测试 ---\n");
	
	{
		xnetaddr tAddr;
		xnet_result iRes = xrtNetResolve("localhost", &tAddr);
		printf("  Resolve(localhost) : %s (ip=%s)\n", iRes == XRT_NET_OK ? "PASS" : "FAIL",
			iRes == XRT_NET_OK ? tAddr.sAddr : "N/A");
	}
	
	printf("\n");
}


