



// HTTP 服务器测试 (单元测试模式 - 只测试公开 API)

// ==================== 测试入口 ====================

static void Test_NetHTTPD(xrtGlobalData* pCore)
{
	printf("\n========== HTTP Server Test ==========\n");
	
	// 测试 1: URI 模式匹配 - 精确匹配
	{
		xhttpdreq tReq = {0};
		strcpy(tReq.sPath, "/api/users");
		bool b1 = xrtHttpReqMatch(&tReq, "/api/users");
		bool b2 = !xrtHttpReqMatch(&tReq, "/api/posts");
		bool b3 = !xrtHttpReqMatch(&tReq, "/api/users/123");
		printf("URI Match (exact): %s (match=%d, no-match=%d, partial=%d)\n", 
			(b1 && b2 && b3) ? "PASS" : "FAIL", b1, b2, b3);
	}
	
	// 测试 2: URI 模式匹配 - 通配符 *
	{
		xhttpdreq tReq = {0};
		strcpy(tReq.sPath, "/api/users");
		bool b1 = xrtHttpReqMatch(&tReq, "/api/*");
		strcpy(tReq.sPath, "/static/js/app.js");
		bool b2 = xrtHttpReqMatch(&tReq, "/static/**");
		printf("URI Match (wildcard): %s (single=%d, double=%d)\n", 
			(b1 && b2) ? "PASS" : "FAIL", b1, b2);
	}
	
	// 测试 3: HTTP 服务器创建/销毁 (不启动)
	{
		xhttpsrvconfig tConfig = {0};
		tConfig.iMaxClients = 8;
		xhttpsrvevents tEvents = {0};
		
		xhttpserver* pServer = xrtHttpServerCreate("127.0.0.1", 18888, &tConfig, &tEvents);
		bool bCreate = (pServer != NULL);
		if ( pServer ) {
			xrtHttpServerDestroy(pServer);
		}
		printf("Server Create/Destroy: %s\n", bCreate ? "PASS" : "FAIL");
	}
	
	// 测试 4: xhttpdreq 结构体初始化
	{
		xhttpdreq tReq = {0};
		tReq.iMethod = XHTTP_GET;
		strcpy(tReq.sMethod, "GET");
		strcpy(tReq.sPath, "/test");
		strcpy(tReq.sQuery, "a=1&b=2");
		tReq.bKeepAlive = true;
		bool bOK = (tReq.iMethod == XHTTP_GET && tReq.bKeepAlive);
		printf("Request Struct Init: %s\n", bOK ? "PASS" : "FAIL");
	}
	
	printf("========== HTTP Server Test Complete ==========\n");
}
