



/*
	HTTP Client 模块测试
*/



/* ============================== 回调函数 ============================== */

static bool test_http_progress_callback(xbuffer pBuf, size_t iTotal, size_t iReceived)
{
	if ( iTotal > 0 ) {
		printf("    进度: %u / %u (%.1f%%)\n", (unsigned int)iReceived, (unsigned int)iTotal,
			(float)iReceived / iTotal * 100.0f);
	} else {
		printf("    已接收: %u bytes\n", (unsigned int)iReceived);
	}
	return true;  // 继续传输
}



/* ============================== 测试 1: URL 解析器 (通过公共 API 间接测试) ============================== */

static void Test_HTTP_URLParser(void)
{
	printf("\n===== HTTP URL 解析器测试 (通过 API 间接验证) =====\n\n");
	
	// 测试无效 URL - 应返回 NULL
	xhttpresp pResp = xrtHttpGet("ftp://invalid.com", NULL, NULL);
	printf("  [%s] ftp://invalid.com (应失败)\n", pResp == NULL ? "OK" : "FAIL");
	if ( pResp ) xrtHttpRespFree(pResp);
	
	// 测试 NULL URL
	pResp = xrtHttpGet(NULL, NULL, NULL);
	printf("  [%s] NULL URL (应失败)\n", pResp == NULL ? "OK" : "FAIL");
	if ( pResp ) xrtHttpRespFree(pResp);
	
	printf("  URL 解析器基础测试通过\n");
}



/* ============================== 测试 2: 极简 API - HTTP GET ============================== */

static void Test_HTTP_SimpleGet(void)
{
	printf("\n===== HTTP 极简 API - GET 测试 =====\n\n");
	
	// 测试 1: HTTP GET (httpbin.org)
	printf("  测试 1: GET http://httpbin.org/get\n");
	xhttpresp pResp = xrtHttpGet("http://httpbin.org/get", NULL, NULL);
	if ( pResp ) {
		printf("    Status: %d %s\n", pResp->iStatusCode, pResp->sStatusText);
		printf("    Content-Type: %s\n", pResp->sContentType);
		printf("    Body length: %u\n", (unsigned int)pResp->tBody.Length);
		if ( pResp->tBody.Length > 0 && pResp->tBody.Length < 500 ) {
			printf("    Body: %s\n", pResp->tBody.Buffer);
		}
		xrtHttpRespFree(pResp);
	} else {
		printf("    [FAIL] 请求失败\n");
	}
	
	// 测试 2: HTTPS GET (httpbin.org)
	printf("\n  测试 2: GET https://httpbin.org/get\n");
	pResp = xrtHttpGet("https://httpbin.org/get", NULL, NULL);
	if ( pResp ) {
		printf("    Status: %d %s\n", pResp->iStatusCode, pResp->sStatusText);
		printf("    Body length: %u\n", (unsigned int)pResp->tBody.Length);
		xrtHttpRespFree(pResp);
	} else {
		printf("    [FAIL] 请求失败\n");
	}
	
	// 测试 3: 带自定义头
	printf("\n  测试 3: GET with custom headers\n");
	pResp = xrtHttpGet("https://httpbin.org/headers",
		"X-Custom-Header: XRT-Test\r\nAccept-Language: zh-CN\r\n", NULL);
	if ( pResp ) {
		printf("    Status: %d\n", pResp->iStatusCode);
		printf("    Body: %.300s\n", pResp->tBody.Buffer ? pResp->tBody.Buffer : "(null)");
		xrtHttpRespFree(pResp);
	} else {
		printf("    [FAIL] 请求失败\n");
	}
}



/* ============================== 测试 3: 极简 API - HTTP POST ============================== */

static void Test_HTTP_SimplePost(void)
{
	printf("\n===== HTTP 极简 API - POST 测试 =====\n\n");
	
	// 测试 1: POST with form data
	printf("  测试 1: POST https://httpbin.org/post\n");
	xhttpresp pResp = xrtHttpPost("https://httpbin.org/post",
		"username=test&password=hello", NULL, NULL);
	if ( pResp ) {
		printf("    Status: %d %s\n", pResp->iStatusCode, pResp->sStatusText);
		printf("    Body length: %u\n", (unsigned int)pResp->tBody.Length);
		if ( pResp->tBody.Buffer ) {
			printf("    Body (first 300): %.300s\n", pResp->tBody.Buffer);
		}
		xrtHttpRespFree(pResp);
	} else {
		printf("    [FAIL] 请求失败\n");
	}
}



/* ============================== 测试 4: 重定向 ============================== */

static void Test_HTTP_Redirect(void)
{
	printf("\n===== HTTP 重定向测试 =====\n\n");
	
	// httpbin.org/redirect/2 会重定向 2 次
	printf("  测试 1: GET https://httpbin.org/redirect/2\n");
	xhttpresp pResp = xrtHttpGet("https://httpbin.org/redirect/2", NULL, NULL);
	if ( pResp ) {
		printf("    Final Status: %d %s\n", pResp->iStatusCode, pResp->sStatusText);
		printf("    Body length: %u\n", (unsigned int)pResp->tBody.Length);
		xrtHttpRespFree(pResp);
	} else {
		printf("    [FAIL] 请求失败\n");
	}
}



/* ============================== 测试 5: 带回调的 GET ============================== */

static void Test_HTTP_Callback(void)
{
	printf("\n===== HTTP 回调测试 =====\n\n");
	
	printf("  测试 1: GET with progress callback\n");
	xhttpresp pResp = xrtHttpGet("https://httpbin.org/bytes/1024", NULL, test_http_progress_callback);
	if ( pResp ) {
		printf("    Status: %d, Body length: %u\n", pResp->iStatusCode, (unsigned int)pResp->tBody.Length);
		xrtHttpRespFree(pResp);
	} else {
		printf("    [FAIL] 请求失败\n");
	}
}



/* ============================== 测试 6: 完整 API ============================== */

static void Test_HTTP_FullAPI(void)
{
	printf("\n===== HTTP 完整 API 测试 =====\n\n");
	
	// 测试 1: 使用请求对象进行 GET
	printf("  测试 1: ReqCreate + Execute (GET)\n");
	xhttpreq pReq = xrtHttpReqCreate(XHTTP_GET, "https://httpbin.org/get");
	xrtHttpReqSetHeader(pReq, "X-Test", "FullAPI");
	xrtHttpReqSetTimeout(pReq, 15);
	
	xhttpresp pResp = xrtHttpReqExecute(pReq);
	if ( pResp ) {
		printf("    Status: %d\n", xrtHttpRespCode(pResp));
		printf("    Body length: %u\n", (unsigned int)xrtHttpRespBodyLen(pResp));
		printf("    Content-Type: %s\n", xrtHttpRespContentType(pResp) ? xrtHttpRespContentType(pResp) : (str)"(null)");
		
		// 测试 Header 读取
		str sServer = xrtHttpRespHeader(pResp, "Server");
		printf("    Server: %s\n", sServer ? sServer : (str)"(null)");
		
		xrtHttpRespFree(pResp);
	} else {
		printf("    [FAIL] 请求失败\n");
	}
	xrtHttpReqFree(pReq);
	
	// 测试 2: 使用请求对象进行 POST + 表单字段
	printf("\n  测试 2: ReqCreate + AddField (POST form)\n");
	pReq = xrtHttpReqCreate(XHTTP_POST, "https://httpbin.org/post");
	xrtHttpReqAddField(pReq, "name", "XRT Test");
	xrtHttpReqAddField(pReq, "value", "Hello World!");
	
	pResp = xrtHttpReqExecute(pReq);
	if ( pResp ) {
		printf("    Status: %d\n", xrtHttpRespCode(pResp));
		printf("    Body (first 400): %.400s\n", xrtHttpRespBody(pResp));
		xrtHttpRespFree(pResp);
	} else {
		printf("    [FAIL] 请求失败\n");
	}
	xrtHttpReqFree(pReq);
	
	// 测试 3: 使用请求对象进行 POST + Multipart
	printf("\n  测试 3: ReqCreate + AddFormField + AddFormData (Multipart)\n");
	pReq = xrtHttpReqCreate(XHTTP_POST, "https://httpbin.org/post");
	xrtHttpReqAddFormField(pReq, "field1", "value1");
	xrtHttpReqAddFormField(pReq, "field2", "value2");
	xrtHttpReqAddFormData(pReq, "file", "test.txt", "Hello from XRT!", 15, "text/plain");
	
	pResp = xrtHttpReqExecute(pReq);
	if ( pResp ) {
		printf("    Status: %d\n", xrtHttpRespCode(pResp));
		printf("    Body (first 500): %.500s\n", xrtHttpRespBody(pResp));
		xrtHttpRespFree(pResp);
	} else {
		printf("    [FAIL] 请求失败\n");
	}
	xrtHttpReqFree(pReq);
}



/* ============================== 测试 7: 响应对象查询 ============================== */

static void Test_HTTP_RespQuery(void)
{
	printf("\n===== HTTP 响应对象查询测试 =====\n\n");
	
	xhttpresp pResp = xrtHttpGet("https://httpbin.org/response-headers?X-Test=Hello&Set-Cookie=session%3Dabc123", NULL, NULL);
	if ( pResp ) {
		printf("    Status: %d\n", xrtHttpRespCode(pResp));
		
		// 测试 Header 查询
		str sXTest = xrtHttpRespHeader(pResp, "X-Test");
		printf("    X-Test header: %s\n", sXTest ? sXTest : (str)"(not found)");
		
		// 测试 Cookie 查询
		str sCookie = xrtHttpRespCookie(pResp, "session");
		printf("    Cookie 'session': %s\n", sCookie ? sCookie : (str)"(not found)");
		
		// 测试 Content-Type
		printf("    Content-Type: %s\n", xrtHttpRespContentType(pResp) ? xrtHttpRespContentType(pResp) : (str)"(null)");
		
		xrtHttpRespFree(pResp);
	} else {
		printf("    [FAIL] 请求失败\n");
	}
}



/* ============================== 测试 8: Cookie 管理 ============================== */

static void Test_HTTP_CookieJar(void)
{
	printf("\n===== HTTP Cookie 管理测试 =====\n\n");
	
	// 测试 1: 手动 Cookie (非持久化)
	printf("  测试 1: 手动 SetCookie + 请求验证\n");
	xhttpreq pReq = xrtHttpReqCreate(XHTTP_GET, "https://httpbin.org/cookies");
	xrtHttpReqSetCookie(pReq, "test", "hello");
	xrtHttpReqSetCookie(pReq, "lang", "zh");
	
	xhttpresp pResp = xrtHttpReqExecute(pReq);
	if ( pResp ) {
		printf("    Status: %d\n", xrtHttpRespCode(pResp));
		str sBody = xrtHttpRespBody(pResp);
		printf("    Body: %s\n", sBody);
		// httpbin.org/cookies 返回 JSON: {"cookies": {"test": "hello", "lang": "zh"}}
		printf("    [%s] test=hello\n", strstr(sBody, "hello") ? "OK" : "FAIL");
		printf("    [%s] lang=zh\n", strstr(sBody, "zh") ? "OK" : "FAIL");
		xrtHttpRespFree(pResp);
	} else {
		printf("    [FAIL] 请求失败\n");
	}
	xrtHttpReqFree(pReq);  // 自动清理非持久化 cookie
	
	// 测试 2: 持久化 Cookie Jar (自动保存 Set-Cookie)
	printf("\n  测试 2: EnableCookies + 自动保存 Set-Cookie\n");
	pReq = xrtHttpReqCreate(XHTTP_GET,
		"https://httpbin.org/cookies/set?session=abc123&user=xrt");
	xrtHttpReqEnableCookies(pReq, true);
	
	pResp = xrtHttpReqExecute(pReq);
	if ( pResp ) {
		printf("    Status: %d (重定向后)\n", xrtHttpRespCode(pResp));
		str sBody = xrtHttpRespBody(pResp);
		printf("    Body: %s\n", sBody);
		// /cookies/set 设置 cookie 后 302 重定向到 /cookies, 自动携带 jar
		printf("    [%s] session=abc123 (自动保存)\n", strstr(sBody, "abc123") ? "OK" : "FAIL");
		printf("    [%s] user=xrt (自动保存)\n", strstr(sBody, "xrt") ? "OK" : "FAIL");
		xrtHttpRespFree(pResp);
	} else {
		printf("    [FAIL] 请求失败\n");
	}
	
	// 测试 3: 复用 Jar (共享到新请求)
	printf("\n  测试 3: 复用 Cookie Jar 到新请求\n");
	xhttpreq pReq2 = xrtHttpReqCreate(XHTTP_GET, "https://httpbin.org/cookies");
	pReq2->pCookies = pReq->pCookies;  // 共享 jar
	
	pResp = xrtHttpReqExecute(pReq2);
	if ( pResp ) {
		printf("    Status: %d\n", xrtHttpRespCode(pResp));
		str sBody = xrtHttpRespBody(pResp);
		printf("    Body: %s\n", sBody);
		printf("    [%s] session=abc123 (复用 jar)\n", strstr(sBody, "abc123") ? "OK" : "FAIL");
		printf("    [%s] user=xrt (复用 jar)\n", strstr(sBody, "xrt") ? "OK" : "FAIL");
		xrtHttpRespFree(pResp);
	} else {
		printf("    [FAIL] 请求失败\n");
	}
	
	// 清理: pReq2 的 pCookies 是共享的, 不要让 xrtHttpReqFree 释放
	pReq2->pCookies = NULL;
	xrtHttpReqFree(pReq2);
	
	// 测试 4: RemoveCookie + 关闭 Jar
	printf("\n  测试 4: RemoveCookie + 关闭 Jar\n");
	xrtHttpReqRemoveCookie(pReq, "user");
	
	pReq2 = xrtHttpReqCreate(XHTTP_GET, "https://httpbin.org/cookies");
	pReq2->pCookies = pReq->pCookies;
	pResp = xrtHttpReqExecute(pReq2);
	if ( pResp ) {
		str sBody = xrtHttpRespBody(pResp);
		printf("    Body: %s\n", sBody);
		printf("    [%s] session=abc123 (保留)\n", strstr(sBody, "abc123") ? "OK" : "FAIL");
		printf("    [%s] user 已删除\n", strstr(sBody, "user") == NULL ? "OK" : "FAIL");
		xrtHttpRespFree(pResp);
	} else {
		printf("    [FAIL] 请求失败\n");
	}
	pReq2->pCookies = NULL;
	xrtHttpReqFree(pReq2);
	
	// 关闭并销毁 jar
	xrtHttpReqEnableCookies(pReq, false);
	printf("    [OK] Jar 已销毁\n");
	xrtHttpReqFree(pReq);
}



/* ============================== 主测试入口 ============================== */

void Test_HTTP(void)
{
	printf("\n\n============================================================\n");
	printf("            HTTP Client 模块测试\n");
	printf("============================================================\n");
	
	// URL 解析器 (纯本地测试，不需要网络)
	Test_HTTP_URLParser();
	
	// 网络测试
	Test_HTTP_SimpleGet();
	Test_HTTP_SimplePost();
	Test_HTTP_Redirect();
	Test_HTTP_Callback();
	Test_HTTP_FullAPI();
	Test_HTTP_RespQuery();
	Test_HTTP_CookieJar();
	
	printf("\n============================================================\n");
	printf("            HTTP Client 测试完成\n");
	printf("============================================================\n\n");
}


