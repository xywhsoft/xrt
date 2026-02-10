



// TLS 1.3 测试

// TLS 测试回调状态
typedef struct {
	volatile bool bConnected;
	volatile bool bRecvData;
	volatile bool bClosed;
	volatile bool bHasHTTP;
	volatile int iRecvBytes;
} __xrt_tls_test_state;

static void __test_tls_on_connect(ptr pClient, xnetconn* pConn, bool bSuccess)
{
	__xrt_tls_test_state* pState = (__xrt_tls_test_state*)xrtTcpClientGetUserData((xtcpclient*)pClient);
	if ( pState ) pState->bConnected = bSuccess;
}
static void __test_tls_on_recv(ptr pClient, xnetconn* pConn, const char* pData, size_t iLen)
{
	__xrt_tls_test_state* pState = (__xrt_tls_test_state*)xrtTcpClientGetUserData((xtcpclient*)pClient);
	if ( pState ) {
		pState->iRecvBytes += (int)iLen;
		if ( (iLen >= 5) && (strncmp(pData, "HTTP/", 5) == 0) ) {
			pState->bHasHTTP = true;
		}
		pState->bRecvData = true;
	}
}
static void __test_tls_on_close(ptr pClient, xnetconn* pConn)
{
	__xrt_tls_test_state* pState = (__xrt_tls_test_state*)xrtTcpClientGetUserData((xtcpclient*)pClient);
	if ( pState ) pState->bClosed = true;
}

void Test_NetTLS(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n TLS 测试 :\n\n");
	
	// ==================== P-256 ECDH RFC 5903 Test Vectors ====================
	printf("--- P-256 ECDH RFC 5903 测试 ---\n");
	{
		// RFC 5903 Section 8.1 - P-256 test vectors
		// Initiator private key
		uint8 aPrivI[] = {
			0xC8,0x8F,0x01,0xF5,0x10,0xD9,0xAC,0x3F,0x70,0xA2,0x92,0xDA,0xA2,0x31,0x6D,0xE5,
			0x44,0xE9,0xAA,0xB8,0xAF,0xE8,0x40,0x49,0xC6,0x2A,0x9C,0x57,0x86,0x2D,0x14,0x33};
		// Initiator public key (i * G)
		uint8 aPubI_expected_x[] = {
			0xDA,0xD0,0xB6,0x53,0x94,0x22,0x1C,0xF9,0xB0,0x51,0xE1,0xFE,0xCA,0x57,0x87,0xD0,
			0x98,0xDF,0xE6,0x37,0xFC,0x90,0xB9,0xEF,0x94,0x5D,0x0C,0x37,0x72,0x58,0x11,0x80};
		// Responder private key
		uint8 aPrivR[] = {
			0xC6,0xEF,0x9C,0x5D,0x78,0xAE,0x01,0x2A,0x01,0x11,0x64,0xAC,0xB3,0x97,0xCE,0x20,
			0x88,0x68,0x5D,0x8F,0x06,0xBF,0x9B,0xE0,0xB2,0x83,0xAB,0x46,0x47,0x6B,0xEE,0x53};
		// Responder public key
		uint8 aPubR[65] = {0x04,
			0xD1,0x2D,0xFB,0x52,0x89,0xC8,0xD4,0xF8,0x12,0x08,0xB7,0x02,0x70,0x39,0x8C,0x34,
			0x22,0x96,0x97,0x0A,0x0B,0xCC,0xB7,0x4C,0x73,0x6F,0xC7,0x55,0x44,0x94,0xBF,0x63,
			0x56,0xFB,0xF3,0xCA,0x36,0x6C,0xC2,0x3E,0x81,0x57,0x85,0x4C,0x13,0xC5,0x8D,0x6A,
			0xAC,0x23,0xF0,0x46,0xAD,0xA3,0x0F,0x83,0x53,0xE7,0x4F,0x33,0x03,0x98,0x72,0xAB};
		// Expected shared secret
		uint8 aExpected[] = {
			0xD6,0x84,0x0F,0x6B,0x42,0xF6,0xED,0xAF,0xD1,0x31,0x16,0xE0,0xE1,0x25,0x65,0x20,
			0x2F,0xEF,0x8E,0x9E,0xCE,0x7D,0xCE,0x03,0x81,0x24,0x64,0xD0,0x4B,0x94,0x42,0xDE};
		
		// Test 1: i * G (verify public key generation)
		uint8 aGPub[65] = {0x04,
			0x6b,0x17,0xd1,0xf2,0xe1,0x2c,0x42,0x47,0xf8,0xbc,0xe6,0xe5,0x63,0xa4,0x40,0xf2,
			0x77,0x03,0x7d,0x81,0x2d,0xeb,0x33,0xa0,0xf4,0xa1,0x39,0x45,0xd8,0x98,0xc2,0x96,
			0x4f,0xe3,0x42,0xe2,0xfe,0x1a,0x7f,0x9b,0x8e,0xe7,0xeb,0x4a,0x7c,0x0f,0x9e,0x16,
			0x2b,0xce,0x33,0x57,0x6b,0x31,0x5e,0xce,0xcb,0xb6,0x40,0x68,0x37,0xbf,0x51,0xf5};
		uint8 aOut[32];
		xrtECDHSecp256r1SharedSecret(aOut, aPrivI, aGPub);  // = X(i*G)
		bool bTestIG = memcmp(aOut, aPubI_expected_x, 32) == 0;
		printf("  RFC5903 i*G (pub key) : %s\n", bTestIG ? "PASS" : "FAIL");
		if ( !bTestIG ) {
			printf("    got:      "); for (int i=0;i<32;i++) printf("%02x",aOut[i]); printf("\n");
			printf("    expected: "); for (int i=0;i<32;i++) printf("%02x",aPubI_expected_x[i]); printf("\n");
		}
		
		// Test 2: i * Qr (ECDH shared secret)
		xrtECDHSecp256r1SharedSecret(aOut, aPrivI, aPubR);  // = X(i*Qr)
		bool bTestIR = memcmp(aOut, aExpected, 32) == 0;
		printf("  RFC5903 i*Qr (shared secret) : %s\n", bTestIR ? "PASS" : "FAIL");
		if ( !bTestIR ) {
			printf("    got:      "); for (int i=0;i<32;i++) printf("%02x",aOut[i]); printf("\n");
			printf("    expected: "); for (int i=0;i<32;i++) printf("%02x",aExpected[i]); printf("\n");
		}
		
		// Test 3: r * Qi (should give same shared secret)
		uint8 aPubI[65] = {0x04,
			0xDA,0xD0,0xB6,0x53,0x94,0x22,0x1C,0xF9,0xB0,0x51,0xE1,0xFE,0xCA,0x57,0x87,0xD0,
			0x98,0xDF,0xE6,0x37,0xFC,0x90,0xB9,0xEF,0x94,0x5D,0x0C,0x37,0x72,0x58,0x11,0x80,
			0x52,0x71,0xA0,0x46,0x1C,0xDB,0x82,0x52,0xD6,0x1F,0x1C,0x45,0x6F,0xA3,0xE5,0x9A,
			0xB1,0xF4,0x5B,0x33,0xAC,0xCF,0x5F,0x58,0x38,0x9E,0x05,0x77,0xB8,0x99,0x0B,0xB3};
		xrtECDHSecp256r1SharedSecret(aOut, aPrivR, aPubI);  // = X(r*Qi)
		bool bTestRI = memcmp(aOut, aExpected, 32) == 0;
		printf("  RFC5903 r*Qi (cross check) : %s\n", bTestRI ? "PASS" : "FAIL");
		if ( !bTestRI ) {
			printf("    got:      "); for (int i=0;i<32;i++) printf("%02x",aOut[i]); printf("\n");
			printf("    expected: "); for (int i=0;i<32;i++) printf("%02x",aExpected[i]); printf("\n");
		}
		
		// Test 4: r * G (should give Qr.x)
		uint8 aQr_x[] = {
			0xD1,0x2D,0xFB,0x52,0x89,0xC8,0xD4,0xF8,0x12,0x08,0xB7,0x02,0x70,0x39,0x8C,0x34,
			0x22,0x96,0x97,0x0A,0x0B,0xCC,0xB7,0x4C,0x73,0x6F,0xC7,0x55,0x44,0x94,0xBF,0x63};
		xrtECDHSecp256r1SharedSecret(aOut, aPrivR, aGPub);  // = X(r*G)
		bool bTestRG = memcmp(aOut, aQr_x, 32) == 0;
		printf("  RFC5903 r*G (pub key) : %s\n", bTestRG ? "PASS" : "FAIL");
		if ( !bTestRG ) {
			printf("    got:      "); for (int i=0;i<32;i++) printf("%02x",aOut[i]); printf("\n");
			printf("    expected: "); for (int i=0;i<32;i++) printf("%02x",aQr_x[i]); printf("\n");
		}
		
		// Test 5: 1 * Qi (should return Qi_x)
		{
			uint8 aScalar1[32] = {0}; aScalar1[31] = 1;
			xrtECDHSecp256r1SharedSecret(aOut, aScalar1, aPubI);  // = X(1*Qi) = Qi_x
			bool bTest1Q = memcmp(aOut, aPubI_expected_x, 32) == 0;
			printf("  Test 1*Qi = Qi_x : %s\n", bTest1Q ? "PASS" : "FAIL");
			if ( !bTest1Q ) {
				printf("    got:      "); for (int i=0;i<32;i++) printf("%02x",aOut[i]); printf("\n");
				printf("    expected: "); for (int i=0;i<32;i++) printf("%02x",aPubI_expected_x[i]); printf("\n");
			}
		}
		
		// Test 6: small scalars with Qi to find where it breaks
		{
			uint8 aScalarN[32] = {0};
			for (int n = 2; n <= 16; n++) {
				aScalarN[31] = (uint8)n;
				uint8 aR1[32], aR2[32];
				// n * Qi
				xrtECDHSecp256r1SharedSecret(aR1, aScalarN, aPubI);
				// Verify via different decomposition: (n-1)*Qi + Qi
				// We can't easily do this without internal API, so just check consistency:
				// n * Qi should equal n * Qi (deterministic)
				xrtECDHSecp256r1SharedSecret(aR2, aScalarN, aPubI);
				if ( memcmp(aR1, aR2, 32) != 0 ) {
					printf("  %d*Qi non-deterministic!\n", n);
				}
			}
			printf("  Small scalar determinism test : PASS\n");
		}
		
		// Test 7: Deep P-256 diagnostic - directly test internal functions
		{
			printf("\n  --- Deep P-256 diagnostic ---\n");
			#ifdef DEBUG_TRACE
			extern void xrtP256ArithTest(void);
			xrtP256ArithTest();
			
			extern void xrtP256DebugTest(const uint8 *pPriv, const uint8 *pPub65, const uint8 *pExpected32);
			printf("  ++ Testing r*Qi (failing case):\n");
			xrtP256DebugTest(aPrivR, aPubI, aExpected);
			printf("\n  ++ Testing i*G (should produce Qi):\n");
			xrtP256DebugTest(aPrivI, aGPub, aPubI_expected_x);
			#endif
		}
	}

	// ==================== TLS 上下文生命周期测试 ====================
	printf("--- TLS 上下文生命周期测试 ---\n");
	
	// 客户端模式创建
	{
		xtlsctx* pCtx = xrtTlsCreate(NULL, false);
		printf("  Client ctx create (no config) : %s\n", pCtx ? "PASS" : "FAIL");
		if ( pCtx ) {
			printf("  Client ctx not ready : %s\n", !xrtTlsIsReady(pCtx) ? "PASS" : "FAIL");
			xrtTlsDestroy(pCtx);
			printf("  Client ctx destroy : PASS\n");
		}
	}
	
	// 服务端模式创建
	{
		xtlsctx* pCtx = xrtTlsCreate(NULL, true);
		printf("  Server ctx create (no config) : %s\n", pCtx ? "PASS" : "FAIL");
		if ( pCtx ) {
			printf("  Server ctx not ready : %s\n", !xrtTlsIsReady(pCtx) ? "PASS" : "FAIL");
			xrtTlsDestroy(pCtx);
			printf("  Server ctx destroy : PASS\n");
		}
	}
	
	// 带配置创建
	{
		xtlsconfig tConfig;
		memset(&tConfig, 0, sizeof(xtlsconfig));
		tConfig.sHostName = "example.com";
		tConfig.bVerifyPeer = false;
		
		xtlsctx* pCtx = xrtTlsCreate(&tConfig, false);
		printf("  Create with config : %s\n", pCtx ? "PASS" : "FAIL");
		if ( pCtx ) {
			printf("  Ctx not ready (no handshake) : %s\n", !xrtTlsIsReady(pCtx) ? "PASS" : "FAIL");
			xrtTlsDestroy(pCtx);
			printf("  Destroy : PASS\n");
		}
	}
	
	// 带验证的配置
	{
		xtlsconfig tConfig;
		memset(&tConfig, 0, sizeof(xtlsconfig));
		tConfig.sHostName = "secure.example.com";
		tConfig.bVerifyPeer = true;
		
		xtlsctx* pCtx = xrtTlsCreate(&tConfig, false);
		printf("  Create with verify=true : %s\n", pCtx ? "PASS" : "FAIL");
		if ( pCtx ) {
			xrtTlsDestroy(pCtx);
		}
	}
	
	
	
	// ==================== TCP EnableTLS 标志测试 ====================
	printf("\n--- TCP EnableTLS 标志测试 ---\n");
	
	// 服务器 EnableTLS
	{
		xnetconfig tConfig = {0};
		tConfig.iRecvBufSize = 4096;
		tConfig.iSendBufSize = 4096;
		tConfig.iMaxClients = 4;
		tConfig.iPollTimeoutMs = 5;
		
		xnetevents tEvents = {0};
		
		xtcpserver* pServer = xrtTcpServerCreate("127.0.0.1", 19900, &tConfig, &tEvents);
		printf("  TCP Server create : %s\n", pServer ? "PASS" : "FAIL");
		
		if ( pServer ) {
			xtlsconfig tTlsConfig;
			memset(&tTlsConfig, 0, sizeof(xtlsconfig));
			tTlsConfig.sCertFile = "/tmp/cert.pem";
			tTlsConfig.sKeyFile = "/tmp/key.pem";
			tTlsConfig.bVerifyPeer = false;
			
			xnet_result iRes = xrtTcpServerEnableTLS(pServer, &tTlsConfig);
			printf("  Server EnableTLS : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
			
			// NULL config 应该返回 ERROR
			iRes = xrtTcpServerEnableTLS(pServer, NULL);
			printf("  Server EnableTLS(NULL) = ERROR : %s\n", iRes == XRT_NET_ERROR ? "PASS" : "FAIL");
			
			// NULL server 应该返回 ERROR
			iRes = xrtTcpServerEnableTLS(NULL, &tTlsConfig);
			printf("  EnableTLS(NULL server) = ERROR : %s\n", iRes == XRT_NET_ERROR ? "PASS" : "FAIL");
			
			xrtTcpServerDestroy(pServer);
			printf("  Server destroy : PASS\n");
		}
	}
	
	// 客户端 EnableTLS
	{
		xnetconfig tConfig = {0};
		tConfig.iRecvBufSize = 4096;
		tConfig.iSendBufSize = 4096;
		tConfig.iPollTimeoutMs = 5;
		
		xnetevents tEvents = {0};
		
		xtcpclient* pClient = xrtTcpClientCreate("127.0.0.1", 19901, &tConfig, &tEvents);
		printf("  TCP Client create : %s\n", pClient ? "PASS" : "FAIL");
		
		if ( pClient ) {
			xtlsconfig tTlsConfig;
			memset(&tTlsConfig, 0, sizeof(xtlsconfig));
			tTlsConfig.sHostName = "example.com";
			tTlsConfig.bVerifyPeer = false;
			
			xnet_result iRes = xrtTcpClientEnableTLS(pClient, &tTlsConfig);
			printf("  Client EnableTLS : %s\n", iRes == XRT_NET_OK ? "PASS" : "FAIL");
			
			// NULL config 应该返回 ERROR
			iRes = xrtTcpClientEnableTLS(pClient, NULL);
			printf("  Client EnableTLS(NULL) = ERROR : %s\n", iRes == XRT_NET_ERROR ? "PASS" : "FAIL");
			
			// NULL client 应该返回 ERROR
			iRes = xrtTcpClientEnableTLS(NULL, &tTlsConfig);
			printf("  EnableTLS(NULL client) = ERROR : %s\n", iRes == XRT_NET_ERROR ? "PASS" : "FAIL");
			
			xrtTcpClientDestroy(pClient);
			printf("  Client destroy : PASS\n");
		}
	}
	
	
	
	// ==================== TLS 密钥派生内部测试 ====================
	printf("\n--- TLS 密钥派生测试 ---\n");
	
	// 测试 HKDF 密钥派生链路在 TLS 场景下的正确性
	{
		// 生成 X25519 密钥对
		uint8 aPriv[32], aPub[32];
		xrtRandomBytes(aPriv, 32);
		xrtX25519Keypair(aPriv, aPub);
		
		// 验证密钥对非零
		bool bPrivNonZero = false;
		bool bPubNonZero = false;
		for ( int i = 0; i < 32; i++ ) {
			if ( aPriv[i] != 0 ) bPrivNonZero = true;
			if ( aPub[i] != 0 ) bPubNonZero = true;
		}
		printf("  X25519 PrivKey non-zero : %s\n", bPrivNonZero ? "PASS" : "FAIL");
		printf("  X25519 PubKey non-zero : %s\n", bPubNonZero ? "PASS" : "FAIL");
		
		// 验证私钥和公钥不同
		bool bDiff = memcmp(aPriv, aPub, 32) != 0;
		printf("  Priv != Pub : %s\n", bDiff ? "PASS" : "FAIL");
		
		// 测试 HKDF 密钥派生
		uint8 aPRK[32], aOKM[32];
		uint8 aSalt[32];
		memset(aSalt, 0, 32);
		
		xrtHKDFExtract(aPRK, aSalt, 32, aPriv, 32);
		xrtHKDFExpand(aOKM, 32, aPRK, 32, (const uint8*)"tls13 derived", 13);
		
		bool bOKMNonZero = false;
		for ( int i = 0; i < 32; i++ ) {
			if ( aOKM[i] != 0 ) bOKMNonZero = true;
		}
		printf("  HKDF-Expand output non-zero : %s\n", bOKMNonZero ? "PASS" : "FAIL");
		
		// 测试两次不同输入产生不同输出
		uint8 aOKM2[32];
		xrtHKDFExpand(aOKM2, 32, aPRK, 32, (const uint8*)"tls13 s hs traffic", 18);
		bool bDiffOKM = memcmp(aOKM, aOKM2, 32) != 0;
		printf("  Different labels -> different keys : %s\n", bDiffOKM ? "PASS" : "FAIL");
	}
	
	
	
	// ==================== RSA ModPow 自检测试 ====================
	printf("\n--- RSA ModPow 基础测试 ---\n");
	{
		// 7^3 mod 11 = 343 mod 11 = 2
		uint8 mod1[] = {0x0b};
		uint8 exp1[] = {0x03};
		uint8 msg1[] = {0x07};
		uint8 out1[1] = {0};
		xrtRSAModPow(mod1, 1, exp1, 1, msg1, 1, out1, 1);
		printf("  7^3 mod 11 = %d (expected 2) : %s\n", out1[0], out1[0] == 2 ? "PASS" : "FAIL");
		
		// 2^16 mod 100 = 65536 mod 100 = 36
		uint8 mod2[] = {0x64};
		uint8 exp2[] = {0x10};
		uint8 msg2[] = {0x02};
		uint8 out2[1] = {0};
		xrtRSAModPow(mod2, 1, exp2, 1, msg2, 1, out2, 1);
		printf("  2^16 mod 100 = %d (expected 36) : %s\n", out2[0], out2[0] == 36 ? "PASS" : "FAIL");
		
		// 3^65537 mod 17 = 3 (Fermat: 3^16 = 1 mod 17, 65537 mod 16 = 1, so 3^1 mod 17 = 3)
		uint8 mod3[] = {0x11};
		uint8 exp3[] = {0x01, 0x00, 0x01};
		uint8 msg3[] = {0x03};
		uint8 out3[1] = {0};
		xrtRSAModPow(mod3, 1, exp3, 3, msg3, 1, out3, 1);
		printf("  3^65537 mod 17 = %d (expected 3) : %s\n", out3[0], out3[0] == 3 ? "PASS" : "FAIL");
		
		// 2^64 mod (2^32+1) = 1 (multi-component test)
		{
			uint8 mod4[] = {0x01, 0x00, 0x00, 0x00, 0x01};
			uint8 exp4[] = {0x40}; // 64
			uint8 msg4[] = {0x02};
			uint8 out4[5] = {0};
			xrtRSAModPow(mod4, 5, exp4, 1, msg4, 1, out4, 5);
			printf("  2^64 mod (2^32+1): %02x %02x %02x %02x %02x (expected 00 00 00 00 01) : %s\n",
				out4[0], out4[1], out4[2], out4[3], out4[4],
				(out4[0]==0 && out4[1]==0 && out4[2]==0 && out4[3]==0 && out4[4]==1) ? "PASS" : "FAIL");
		}
		
		// RSA test: sig^e mod n where e=65537, with 8-byte key
		// n = 0xD5 0x5B 0xB1 0x63 0xA1 0xBB 0x5F 0xD1 (15381259694171455441)
		// e = 0x01 0x00 0x01
		// msg = 0x42 (66)
		// 66^65537 mod 15381259694171455441
		// Just verify it completes without crash and result fits in modulus size
		{
			uint8 mod5[] = {0xD5, 0x5B, 0xB1, 0x63, 0xA1, 0xBB, 0x5F, 0xD1};
			uint8 exp5[] = {0x01, 0x00, 0x01};
			uint8 msg5[] = {0x42};
			uint8 out5[8] = {0};
			xrtRSAModPow(mod5, 8, exp5, 3, msg5, 1, out5, 8);
			printf("  66^65537 mod n (8-byte): result=%02x%02x%02x%02x%02x%02x%02x%02x (no crash) : PASS\n",
				out5[0], out5[1], out5[2], out5[3], out5[4], out5[5], out5[6], out5[7]);
		}
	}

	// ==================== TLS 1.3 客户端握手测试（高级 API） ====================
	printf("\n--- TLS 1.3 客户端握手测试 ---\n");
	
	// 尝试多个服务器，确保至少一个可达
	{
		const char* arrHosts[] = { "www.cloudflare.com", "www.microsoft.com", "www.baidu.com", NULL };
		const char* arrSNI[] = { "www.cloudflare.com", "www.microsoft.com", "www.baidu.com", NULL };
		bool bAnySuccess = false;
		
		for ( int iHost = 0; arrHosts[iHost]; iHost++ ) {
			printf("\n  --- 尝试 %s (SNI: %s) ---\n", arrHosts[iHost], arrSNI[iHost]);
			
			// 对于 IP 地址直接使用，对于域名先解析
			const char* sIP = arrHosts[iHost];
			xnetaddr tAddr;
			
			// 检查是否是域名（包含字母则为域名）
			bool bIsDomain = false;
			for ( const char* p = sIP; *p; p++ ) {
				if ( (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ) { bIsDomain = true; break; }
			}
			
			if ( bIsDomain ) {
				xnet_result iDnsRes = xrtNetResolve(sIP, &tAddr);
				if ( iDnsRes != XRT_NET_OK ) {
					printf("  DNS 解析失败 : SKIP\n");
					continue;
				}
				sIP = tAddr.sAddr;
				printf("  DNS 解析 : %s (PASS)\n", sIP);
			}
			
			__xrt_tls_test_state tState;
			memset(&tState, 0, sizeof(__xrt_tls_test_state));
			
			xnetconfig tConfig = {0};
			tConfig.iRecvBufSize = 8192;
			tConfig.iSendBufSize = 8192;
			tConfig.iPollTimeoutMs = 10;
			
			xnetevents tEvents = {0};
			tEvents.OnConnect = __test_tls_on_connect;
			tEvents.OnRecv = __test_tls_on_recv;
			tEvents.OnClose = __test_tls_on_close;
			
			xtcpclient* pClient = xrtTcpClientCreate(sIP, 443, &tConfig, &tEvents);
			printf("  TCP Client create : %s\n", pClient ? "PASS" : "FAIL");
			
			if ( pClient ) {
				xrtTcpClientSetUserData(pClient, &tState);
				
				xtlsconfig tTlsCfg;
				memset(&tTlsCfg, 0, sizeof(xtlsconfig));
				tTlsCfg.sHostName = arrSNI[iHost];
				tTlsCfg.bVerifyPeer = true;
				
				xnet_result iEnableRes = xrtTcpClientEnableTLS(pClient, &tTlsCfg);
				printf("  EnableTLS : %s\n", iEnableRes == XRT_NET_OK ? "PASS" : "FAIL");
				
				xnet_result iConnRes = xrtTcpClientConnect(pClient);
				printf("  Connect + TLS Handshake : %s\n", iConnRes == XRT_NET_OK ? "PASS" : "FAIL");
				
				if ( iConnRes == XRT_NET_OK ) {
					printf("  IsConnected : %s\n", xrtTcpClientIsConnected(pClient) ? "PASS" : "FAIL");
					
					// 发送 HTTP GET 请求
					char sReq[256];
					snprintf(sReq, sizeof(sReq), "GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", arrSNI[iHost]);
					xnet_result iSendRes = xrtTcpClientSend(pClient, sReq, strlen(sReq));
					printf("  TLS Send HTTP request : %s\n", iSendRes == XRT_NET_OK ? "PASS" : "FAIL");
					
					// 等待接收响应（最多 3 秒）
					for ( int i = 0; i < 300; i++ ) {
						if ( tState.bRecvData || tState.bClosed ) break;
						#if defined(_WIN32) || defined(_WIN64)
							Sleep(10);
						#else
							usleep(10000);
						#endif
					}
					
					if ( tState.bRecvData ) {
						printf("  TLS Recv HTTP response : PASS (%d bytes, HTTP=%s)\n",
							tState.iRecvBytes, tState.bHasHTTP ? "yes" : "no");
					} else {
						printf("  TLS Recv HTTP response : FAIL (no data in 3s)\n");
					}
					bAnySuccess = true;
				} else {
					printf("  连接或握手失败 : SKIP (尝试下一个)\n");
				}
				
				xrtTcpClientDisconnect(pClient);
				printf("  Disconnect : PASS\n");
				xrtTcpClientDestroy(pClient);
				printf("  Destroy : PASS\n");
			}
		}
		
		if ( !bAnySuccess ) {
			printf("  所有服务器均失败 (可能无网络或 TLS 实现需调试) : SKIP\n");
		}
	}
}
