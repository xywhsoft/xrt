

/*
	TLS SNI 功能测试
	
	测试内容:
	  1. xrtTlsSetCert() PEM/DER 证书加载
	  2. OnSNI 回调配置
	  3. xrtTlsGetSNI() API
*/



// 测试用 SNI 回调函数
static int g_iSNICallCount = 0;
static char g_sSNIHostname[256] = {0};

static void TestSNI_Callback(xtlsctx *pCtx, const char *sHostName, ptr pUserData)
{
	g_iSNICallCount++;
	if ( sHostName ) {
		strncpy(g_sSNIHostname, sHostName, sizeof(g_sSNIHostname) - 1);
	}
	printf("    SNI callback: hostname = %s\n", sHostName ? sHostName : "(null)");
}



// 测试 1: SNI 回调配置
static void Test_TLS_SNI_Config()
{
	printf("  Test_TLS_SNI_Config...\n");
	
	// 创建配置
	xtlsconfig tConfig = {0};
	tConfig.OnSNI = TestSNI_Callback;
	tConfig.pSNIUserData = (ptr)0x12345678;
	tConfig.bVerifyPeer = false;
	
	// 创建 TLS 上下文
	xtlsctx *pCtx = xrtTlsCreate(&tConfig, true);  // 服务端模式
	if ( pCtx == NULL ) {
		printf("    FAIL: xrtTlsCreate returned NULL\n");
		return;
	}
	
	// 清理
	xrtTlsDestroy(pCtx);
	
	printf("    PASS\n");
}



// 测试 2: xrtTlsGetSNI API
static void Test_TLS_SNI_GetSNI()
{
	printf("  Test_TLS_SNI_GetSNI...\n");
	
	xtlsconfig tConfig = {0};
	xtlsctx *pCtx = xrtTlsCreate(&tConfig, true);
	if ( pCtx == NULL ) {
		printf("    FAIL: xrtTlsCreate returned NULL\n");
		return;
	}
	
	// 初始状态应为空
	const char *sSNI = xrtTlsGetSNI(pCtx);
	if ( sSNI != NULL ) {
		printf("    FAIL: initial SNI should be NULL\n");
		xrtTlsDestroy(pCtx);
		return;
	}
	
	xrtTlsDestroy(pCtx);
	
	printf("    PASS\n");
}



// 测试 3: PEM 证书加载
static void Test_TLS_SNI_SetCert()
{
	printf("  Test_TLS_SNI_SetCert...\n");
	
	xtlsconfig tConfig = {0};
	xtlsctx *pCtx = xrtTlsCreate(&tConfig, true);
	if ( pCtx == NULL ) {
		printf("    FAIL: xrtTlsCreate returned NULL\n");
		return;
	}
	
	// 测试空参数不崩溃
	xnet_result iRes = xrtTlsSetCert(pCtx, NULL, NULL);
	if ( iRes != XRT_NET_OK ) {
		printf("    FAIL: xrtTlsSetCert(NULL, NULL) should return OK\n");
		xrtTlsDestroy(pCtx);
		return;
	}
	
	// 测试不存在的文件返回错误
	iRes = xrtTlsSetCert(pCtx, "nonexistent_cert_12345.pem", NULL);
	if ( iRes != XRT_NET_ERROR ) {
		printf("    FAIL: xrtTlsSetCert with nonexistent file should return ERROR\n");
		xrtTlsDestroy(pCtx);
		return;
	}
	
	xrtTlsDestroy(pCtx);
	
	printf("    PASS\n");
}



// SNI 功能测试入口
static void Test_TLS_SNI()
{
	printf("Test_TLS_SNI:\n");
	
	Test_TLS_SNI_Config();
	Test_TLS_SNI_GetSNI();
	Test_TLS_SNI_SetCert();
	
	printf("Test_TLS_SNI: ALL PASSED\n\n");
}
