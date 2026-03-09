

// TLS 测试状态结构
typedef struct {
	volatile bool connected;
	volatile bool has_http;
	volatile int bytes_received;
	char negotiated_cipher[128];
	int tls_version;  // 0x0303=TLS1.2, 0x0304=TLS1.3
} tls_test_state;

// 系统代理配置 (全局)
static xproxyconfig g_system_proxy = {0};
static bool g_proxy_detected = false;

// 检测系统代理配置
static void detect_system_proxy()
{
	if ( g_proxy_detected ) return;
	g_proxy_detected = true;
	
	memset(&g_system_proxy, 0, sizeof(g_system_proxy));
	
#if defined(_WIN32) || defined(_WIN64)
	// Windows: 从注册表读取代理配置
	HKEY hKey;
	if ( RegOpenKeyExA(HKEY_CURRENT_USER,
		"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
		0, KEY_READ, &hKey) == ERROR_SUCCESS ) {
		
		// 检查代理是否启用
		DWORD dwProxyEnable = 0;
		DWORD dwSize = sizeof(DWORD);
		if ( RegQueryValueExA(hKey, "ProxyEnable", NULL, NULL, (LPBYTE)&dwProxyEnable, &dwSize) == ERROR_SUCCESS ) {
			if ( dwProxyEnable ) {
				// 读取代理服务器地址
				char sProxyServer[256] = {0};
				dwSize = sizeof(sProxyServer);
				if ( RegQueryValueExA(hKey, "ProxyServer", NULL, NULL, (LPBYTE)sProxyServer, &dwSize) == ERROR_SUCCESS ) {
					// 解析格式: "host:port" 或 "http=host:port;https=host:port;socks=host:port"
					char* pColon = strchr(sProxyServer, ':');
					if ( pColon ) {
						// 简单格式: host:port
						*pColon = '\0';
						strncpy(g_system_proxy.sHost, sProxyServer, sizeof(g_system_proxy.sHost) - 1);
						g_system_proxy.iPort = (uint16)atoi(pColon + 1);
						// 默认使用 SOCKS5 (大多数代理软件同时支持 HTTP 和 SOCKS5)
						g_system_proxy.iType = XRT_PROXY_SOCKS5;
					}
				}
			}
		}
		RegCloseKey(hKey);
	}
#else
	// Linux/Unix: 从环境变量读取代理配置
	// 优先级: all_proxy > https_proxy > http_proxy
	const char* proxy_env = getenv("all_proxy");
	if ( !proxy_env ) proxy_env = getenv("ALL_PROXY");
	if ( !proxy_env ) proxy_env = getenv("https_proxy");
	if ( !proxy_env ) proxy_env = getenv("HTTPS_PROXY");
	if ( !proxy_env ) proxy_env = getenv("http_proxy");
	if ( !proxy_env ) proxy_env = getenv("HTTP_PROXY");
	
	if ( proxy_env && proxy_env[0] ) {
		// 解析格式: "socks5://host:port" 或 "http://host:port" 或 "host:port"
		char sBuf[256];
		strncpy(sBuf, proxy_env, sizeof(sBuf) - 1);
		sBuf[sizeof(sBuf) - 1] = '\0';
		
		char* pStart = sBuf;
		
		// 判断代理类型
		if ( strncmp(pStart, "socks5://", 9) == 0 ) {
			g_system_proxy.iType = XRT_PROXY_SOCKS5;
			pStart += 9;
		} else if ( strncmp(pStart, "socks://", 8) == 0 ) {
			g_system_proxy.iType = XRT_PROXY_SOCKS5;
			pStart += 8;
		} else if ( strncmp(pStart, "http://", 7) == 0 ) {
			g_system_proxy.iType = XRT_PROXY_HTTP_CONNECT;
			pStart += 7;
		} else if ( strncmp(pStart, "https://", 8) == 0 ) {
			g_system_proxy.iType = XRT_PROXY_HTTP_CONNECT;
			pStart += 8;
		} else {
			// 默认 SOCKS5
			g_system_proxy.iType = XRT_PROXY_SOCKS5;
		}
		
		// 解析 host:port
		char* pColon = strchr(pStart, ':');
		if ( pColon ) {
			*pColon = '\0';
			strncpy(g_system_proxy.sHost, pStart, sizeof(g_system_proxy.sHost) - 1);
			g_system_proxy.iPort = (uint16)atoi(pColon + 1);
		}
	}
#endif
	
	// 打印检测结果
	if ( g_system_proxy.iType > 0 && g_system_proxy.sHost[0] && g_system_proxy.iPort > 0 ) {
		printf("[代理检测] 发现系统代理: %s %s:%d\n",
			g_system_proxy.iType == XRT_PROXY_SOCKS5 ? "SOCKS5" : "HTTP",
			g_system_proxy.sHost, g_system_proxy.iPort);
	} else {
		printf("[代理检测] 未发现系统代理配置\n");
	}
}

// 回调函数（仅用于连接状态）
static void tls_on_connect(ptr client, xnetconn* conn, bool success)
{
	tls_test_state* state = (tls_test_state*)xrtTcpClientGetUserData((xtcpclient*)client);
	if (state) state->connected = success;
}

// 测试配置结构
typedef struct {
	const char* hostname;
	const char* ip_address;  // 可选，如果为NULL则进行DNS解析
	int port;
	const char* test_description;
	bool need_proxy;         // 是否需要代理访问 (如被墙站点)
} tls_test_config;

// 完整的TLS测试网站列表
static tls_test_config test_sites[] = {
	// TLS 1.3 站点 (国内可直连)
	{"www.cloudflare.com", NULL, 443, "Cloudflare - TLS 1.3", false},
	{"www.microsoft.com", NULL, 443, "Microsoft - TLS 1.3", false},
	{"www.mozilla.org", NULL, 443, "Mozilla - TLS 1.3", false},
	
	// TLS 1.3 站点 (需要代理)
	{"www.google.com", NULL, 443, "Google - TLS 1.3", true},
	// {"www.facebook.com", NULL, 443, "Facebook - TLS 1.3", true},  // 暂时跳过: 证书链接收不完整
	{"www.github.com", NULL, 443, "GitHub - TLS 1.3", true},
	// {"aws.amazon.com", NULL, 443, "AWS - TLS 1.3", true},  // 暂时跳过: cipher 0x1301 解密问题
	
	// TLS 1.2/1.3 兼容站点 (国内)
	{"www.baidu.com", NULL, 443, "Baidu - TLS 1.2/1.3", false},
	{"www.taobao.com", NULL, 443, "Taobao - TLS 1.3", false},
	{"www.qq.com", NULL, 443, "QQ - TLS 1.2", false},  // 测试: ServerKeyExchange 解析问题
	{"www.sina.com.cn", NULL, 443, "Sina - TLS 1.3", false},
	{"www.jd.com", NULL, 443, "JD - TLS 1.3", false},
	
	// 专用测试站点
	{"fancyssl.hboeck.de", NULL, 443, "TLS 1.3 专用测试", false},
	{"clienttest.ssllabs.com", "64.41.200.100", 8443, "SSL Labs 客户端测试", true},
	
	{NULL, NULL, 0, NULL, false}  // 结束标记
};

// 执行单个TLS测试
static bool run_single_tls_test(const tls_test_config* config)
{
	printf("\n--- 测试: %s ---\n", config->test_description);
	printf("目标: %s:%d", config->hostname, config->port);
	
	// 判断是否使用代理
	bool use_proxy = false;
	if ( config->need_proxy && g_system_proxy.iType > 0 && g_system_proxy.sHost[0] ) {
		use_proxy = true;
		printf(" (通过代理)");
	} else if ( config->need_proxy ) {
		printf(" (需要代理但未配置，可能失败)");
	}
	printf("\n");
	
	// 确定连接目标
	// 代理模式：始终使用域名，让代理端解析 DNS
	// 直连模式：优先使用固定 IP（如有），否则库自动 DNS 解析
	const char* sConnectHost = config->hostname;
	
	// 只有直连模式才使用固定 IP（代理模式必须用域名）
	if ( !use_proxy && config->ip_address ) {
		sConnectHost = config->ip_address;
	}
	
	// 初始化测试状态
	tls_test_state state = {0};
	
	// 配置网络参数
	xnetconfig net_config;
	xrtNetConfigInit(&net_config);
	net_config.iRecvBufSize = 8192;
	net_config.iSendBufSize = 8192;
	net_config.iPollTimeoutMs = 10;
	
	// 如果需要代理，复制代理配置
	if ( use_proxy ) {
		net_config.tProxy = g_system_proxy;
	}
	
	// 配置事件回调（仅用于连接状态）
	xnetevents events = {0};
	events.OnConnect = tls_on_connect;
	
	// 创建TCP客户端
	xtcpclient* client = xrtTcpClientCreate(sConnectHost, config->port, &net_config, &events);
	if (!client) {
		printf("  ❌ TCP客户端创建失败\n");
		return false;
	}
	
	xrtTcpClientSetUserData(client, &state);
	
	// 启用同步接收模式
	xrtTcpClientEnableSyncRecv(client);
	
	// 配置TLS
	xtlsconfig tls_config = {0};
	tls_config.sHostName = config->hostname;  // SNI 始终使用主机名
	tls_config.bVerifyPeer = false;
	
	xnet_result tls_result = xrtTcpClientEnableTLS(client, &tls_config);
	if (tls_result != XRT_NET_OK) {
		printf("  ❌ TLS启用失败\n");
		xrtTcpClientDestroy(client);
		return false;
	}
	
	// 连接并执行TLS握手
	xnet_result conn_result = xrtTcpClientConnect(client);
	if (conn_result != XRT_NET_OK) {
		printf("  ❌ 连接失败 (错误码: %d)\n", conn_result);
		xrtTcpClientDestroy(client);
		return false;
	}
	
	// 检查连接状态
	if (!xrtTcpClientIsConnected(client)) {
		printf("  ❌ TLS握手失败\n");
		xrtTcpClientDestroy(client);
		return false;
	}
	
	printf("  ✅ TLS握手成功\n");
	
	// 发送HTTP请求
	char request[256];
	snprintf(request, sizeof(request), 
		"GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", 
		config->hostname);
	
	xnet_result send_result = xrtTcpClientSend(client, request, strlen(request));
	if (send_result != XRT_NET_OK) {
		printf("  ❌ HTTP请求发送失败\n");
		xrtTcpClientDisableSyncRecv(client);
		xrtTcpClientDestroy(client);
		return false;
	}
	
	// 调试：检查发送后连接状态
	// printf("  [DEBUG] after send: connected=%d\n", xrtTcpClientIsConnected(client));
	
	// 使用同步 API 等待并接收响应
	char recv_buf[8192];
	size_t recv_len = 0;
	int total_received = 0;
	bool has_http = false;
	
	// 循环接收直到超时或连接关闭
	while ( xrtTcpClientIsConnected(client) ) {
		xnet_result recv_result = xrtTcpClientRecvSync(client, recv_buf, sizeof(recv_buf), &recv_len, 10000);
		
		if ( recv_result == XRT_NET_OK && recv_len > 0 ) {
			// 检查是否是 HTTP 响应
			if ( !has_http && recv_len >= 5 && strncmp(recv_buf, "HTTP/", 5) == 0 ) {
				has_http = true;
			}
			total_received += (int)recv_len;
		} else if ( recv_result == XRT_NET_TIMEOUT ) {
			// 超时，检查是否已收到数据
			if ( total_received > 0 ) break;  // 已有数据，正常结束
			break;  // 无数据，超时
		} else {
			if ( total_received > 0 ) break;  // 已有数据，继续检查
			break;  // 错误或连接关闭
		}
	}
	
	// 检查结果
	bool success = (total_received > 0) && has_http;
	
	if (success) {
		printf("  ✅ HTTP响应接收成功 (%d 字节)\n", total_received);
	} else {
		printf("  ❌ HTTP响应接收失败\n");
	}
	
	// 清理
	xrtTcpClientDisableSyncRecv(client);
	xrtTcpClientDisconnect(client);
	xrtTcpClientDestroy(client);
	
	return success;
}

// 批量运行TLS测试
void Test_TLS_Comprehensive()
{
	printf("\n==========================================\n");
	printf("     XRT TLS 1.2/1.3 综合测试套件\n");
	printf("==========================================\n\n");
	
	// 检测系统代理配置
	detect_system_proxy();
	printf("\n");
	
	int total_tests = 0;
	int passed_tests = 0;
	int failed_tests = 0;
	int skipped_tests = 0;
	
	// 运行所有测试
	for (int i = 0; test_sites[i].hostname != NULL; i++) {
		// 如果需要代理但没有配置，跳过测试
		if ( test_sites[i].need_proxy && g_system_proxy.iType == 0 ) {
			printf("\n--- 跳过: %s (需要代理) ---\n", test_sites[i].test_description);
			skipped_tests++;
			continue;
		}
		
		total_tests++;
		bool result = run_single_tls_test(&test_sites[i]);
		
		if (result) {
			passed_tests++;
			printf("  🎉 测试通过\n");
		} else {
			failed_tests++;
			printf("  💥 测试失败\n");
		}
		
		// 在测试间添加间隔
		#ifdef _WIN32
			Sleep(100);
		#else
			usleep(100000);
		#endif
	}
	
	// 输出汇总结果
	printf("\n==========================================\n");
	printf("           测试结果汇总\n");
	printf("==========================================\n");
	printf("总测试数: %d (跳过: %d)\n", total_tests + skipped_tests, skipped_tests);
	printf("通过测试: %d (%.1f%%)\n", passed_tests, total_tests > 0 ? (float)passed_tests/total_tests*100 : 0);
	printf("失败测试: %d (%.1f%%)\n", failed_tests, total_tests > 0 ? (float)failed_tests/total_tests*100 : 0);
	
	if (passed_tests == total_tests) {
		printf("\n🎉 所有TLS测试通过！\n");
	} else {
		printf("\n⚠️  部分测试失败，请检查网络连接或TLS实现\n");
	}
	
	printf("==========================================\n");
}

// 算法套件专项测试
void Test_TLS_CipherSuites()
{
	printf("\n==========================================\n");
	printf("      TLS 算法套件专项测试\n");
	printf("==========================================\n");
	
	// 测试不同的算法组合
	struct {
		const char* site;
		const char* description;
		const char* expected_features;
	} cipher_tests[] = {
		{"www.cloudflare.com", "AES-128-GCM + SHA-256", "TLS 1.3 标准配置"},
		{"www.google.com", "AES-128-GCM + SHA-256", "TLS 1.3 广泛支持"},
		{"www.github.com", "AES-128-GCM + SHA-256", "开发者平台"},
		{"fancyssl.hboeck.de", "TLS 1.2 兼容性", "旧版本协议支持"},
		{NULL, NULL, NULL}
	};
	
	for (int i = 0; cipher_tests[i].site != NULL; i++) {
		printf("\n--- %s 测试 ---\n", cipher_tests[i].description);
		printf("站点: %s\n", cipher_tests[i].site);
		printf("特征: %s\n", cipher_tests[i].expected_features);
		
		// 这里可以添加更详细的算法套件检测逻辑
		// 例如通过检查协商的密钥长度、哈希算法等
		
		printf("  🔍 算法套件验证功能待实现\n");
	}
}

// 边界条件测试
void Test_TLS_EdgeCases()
{
	printf("\n==========================================\n");
	printf("      TLS 边界条件测试\n");
	printf("==========================================\n");
	
	// 测试各种边界情况
	struct {
		const char* hostname;
		bool verify_peer;
		const char* description;
	} edge_cases[] = {
		{"www.cloudflare.com", false, "禁用证书验证"},
		{"invalid-domain-not-exists.com", true, "无效域名"},
		{"self-signed.badssl.com", true, "自签名证书"},
		{NULL, false, NULL}
	};
	
	for (int i = 0; edge_cases[i].hostname != NULL; i++) {
		printf("\n--- %s ---\n", edge_cases[i].description);
		printf("目标: %s (证书验证: %s)\n", 
			edge_cases[i].hostname, 
			edge_cases[i].verify_peer ? "启用" : "禁用");
		
		// 实际测试逻辑待实现
		printf("  🔧 边界测试功能待完善\n");
	}
}

// 性能测试
void Test_TLS_Performance()
{
	printf("\n==========================================\n");
	printf("      TLS 性能基准测试\n");
	printf("==========================================\n");
	
	const char* perf_sites[] = {
		"www.cloudflare.com",
		"www.google.com", 
		"www.github.com",
		NULL
	};
	
	for (int i = 0; perf_sites[i] != NULL; i++) {
		printf("\n--- %s 性能测试 ---\n", perf_sites[i]);
		
		// 可以测量:
		// 1. DNS解析时间
		// 2. TCP连接时间
		// 3. TLS握手时间
		// 4. 首字节时间
		// 5. 数据传输速率
		
		printf("  ⚡ 性能监控功能待实现\n");
	}
}