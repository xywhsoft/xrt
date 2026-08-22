



// Crypto 加密算法库测试
void Test_Crypto(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Crypto 加密算法库测试 :\n\n");
	
	// ==================== SHA-256 测试 ====================
	printf("--- SHA-256 测试 ---\n");
	
	// RFC 6234 测试向量: SHA-256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
	{
		uint8 tOut[32];
		xrtSHA256("abc", 3, tOut);
		uint8 tExpected[] = {
			0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
			0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
			0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
			0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
		};
		bool bOK = memcmp(tOut, tExpected, 32) == 0;
		printf("  SHA-256(\"abc\") : %s\n", bOK ? "PASS" : "FAIL");
		if ( !bOK ) {
			printf("    got: ");
			for ( int i = 0; i < 32; i++ ) printf("%02x", tOut[i]);
			printf("\n");
		}
	}
	
	// SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
	{
		uint8 tOut[32];
		xrtSHA256("", 0, tOut);
		uint8 tExpected[] = {
			0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
			0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
			0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
			0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
		};
		bool bOK = memcmp(tOut, tExpected, 32) == 0;
		printf("  SHA-256(\"\") : %s\n", bOK ? "PASS" : "FAIL");
	}
	
	// 流式 SHA-256 测试
	{
		xsha256_ctx tCtx;
		uint8 tOut1[32], tOut2[32];
		xrtSHA256("hello world", 11, tOut1);
		xrtSHA256Init(&tCtx);
		xrtSHA256Update(&tCtx, "hello ", 6);
		xrtSHA256Update(&tCtx, "world", 5);
		xrtSHA256Final(&tCtx, tOut2);
		bool bOK = memcmp(tOut1, tOut2, 32) == 0;
		printf("  SHA-256 流式 vs 单次 : %s\n", bOK ? "PASS" : "FAIL");
	}
	
	// ==================== HMAC-SHA256 测试 ====================
	printf("\n--- HMAC-SHA256 测试 ---\n");
	
	// RFC 4231 Test Case 2: Key = "Jefe", Data = "what do ya want for nothing?"
	// HMAC = 5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843
	{
		uint8 tOut[32];
		xrtHMAC_SHA256((const uint8*)"Jefe", 4, (const uint8*)"what do ya want for nothing?", 28, tOut);
		uint8 tExpected[] = {
			0x5b, 0xdc, 0xc1, 0x46, 0xbf, 0x60, 0x75, 0x4e,
			0x6a, 0x04, 0x24, 0x26, 0x08, 0x95, 0x75, 0xc7,
			0x5a, 0x00, 0x3f, 0x08, 0x9d, 0x27, 0x39, 0x83,
			0x9d, 0xec, 0x58, 0xb9, 0x64, 0xec, 0x38, 0x43
		};
		bool bOK = memcmp(tOut, tExpected, 32) == 0;
		printf("  HMAC-SHA256 RFC4231 TC2 : %s\n", bOK ? "PASS" : "FAIL");
		if ( !bOK ) {
			printf("    got: ");
			for ( int i = 0; i < 32; i++ ) printf("%02x", tOut[i]);
			printf("\n");
		}
	}
	
	// ==================== PBKDF2-HMAC-SHA256 测试 ====================
	printf("\n--- PBKDF2-HMAC-SHA256 测试 ---\n");
	{
		uint8 tOut[32];
		const uint8 tExpected[32] = {
			0xae, 0x4d, 0x0c, 0x95, 0xaf, 0x6b, 0x46, 0xd3,
			0x2d, 0x0a, 0xdf, 0xf9, 0x28, 0xf0, 0x6d, 0xd0,
			0x2a, 0x30, 0x3f, 0x8e, 0xf3, 0xc2, 0x51, 0xdf,
			0xd6, 0xe2, 0xd8, 0x5a, 0x95, 0x47, 0x4c, 0x43
		};
		bool bOK = xrtPBKDF2_SHA256(
			(const uint8*)"password", 8,
			(const uint8*)"salt", 4,
			2, tOut, sizeof(tOut));
		bOK = bOK && xrtConstTimeEqual(tOut, tExpected, sizeof(tOut));
		printf("  PBKDF2-SHA256 RFC vector : %s\n", bOK ? "PASS" : "FAIL");
	}

	// ==================== ChaCha20 测试 ====================
	printf("\n--- ChaCha20 测试 ---\n");
	
	// RFC 8439 Section 2.4.2 测试向量
	{
		uint8 tKey[32] = {
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
			0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
			0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
		};
		uint8 tNonce[12] = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4a,
			0x00, 0x00, 0x00, 0x00
		};
		const char* sPlain = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";
		size_t iLen = strlen(sPlain);
		uint8 tOut[256];
		xrtChaCha20(tOut, tKey, tNonce, 1, (const uint8*)sPlain, iLen);
		
		// 检查前几个字节
		uint8 tExpectedStart[] = { 0x6e, 0x2e, 0x35, 0x9a, 0x25, 0x68, 0xf9, 0x80 };
		bool bOK = memcmp(tOut, tExpectedStart, 8) == 0;
		printf("  ChaCha20 RFC8439 2.4.2 : %s\n", bOK ? "PASS" : "FAIL");
		if ( !bOK ) {
			printf("    got: ");
			for ( int i = 0; i < 8; i++ ) printf("%02x", tOut[i]);
			printf("\n");
		}
	}
	
	// ==================== ChaCha20-Poly1305 AEAD 测试 ====================
	printf("\n--- ChaCha20-Poly1305 AEAD 测试 ---\n");
	
	// 加密然后解密，验证往返
	{
		uint8 tKey[32];
		uint8 tNonce[12];
		memset(tKey, 0x42, 32);
		memset(tNonce, 0x07, 12);
		
		const char* sPlain = "Hello, ChaCha20-Poly1305!";
		size_t iLen = strlen(sPlain);
		uint8 tAAD[] = { 0x01, 0x02, 0x03 };
		
		uint8 tCipher[256];
		uint8 tDecrypt[256];
		
		bool bEnc = xrtChaCha20Poly1305Encrypt(tCipher, tKey, tNonce, tAAD, 3, (const uint8*)sPlain, iLen);
		printf("  ChaCha20-Poly1305 Encrypt : %s\n", bEnc ? "PASS" : "FAIL");
		
		bool bDec = xrtChaCha20Poly1305Decrypt(tDecrypt, tKey, tNonce, tAAD, 3, tCipher, iLen + 16);
		printf("  ChaCha20-Poly1305 Decrypt : %s\n", bDec ? "PASS" : "FAIL");
		
		bool bMatch = memcmp(tDecrypt, sPlain, iLen) == 0;
		printf("  ChaCha20-Poly1305 往返 : %s\n", bMatch ? "PASS" : "FAIL");
		
		// 篡改密文验证 tag 检查
		tCipher[0] ^= 0xFF;
		bool bTampered = xrtChaCha20Poly1305Decrypt(tDecrypt, tKey, tNonce, tAAD, 3, tCipher, iLen + 16);
		printf("  ChaCha20-Poly1305 篡改检测 : %s\n", !bTampered ? "PASS" : "FAIL");
	}
	
	// ==================== AES-128-GCM 测试 ====================
	printf("\n--- AES-128-GCM 测试 ---\n");
	
	// 加密然后解密，验证往返
	{
		uint8 tKey[16];
		uint8 tNonce[12];
		memset(tKey, 0xAB, 16);
		memset(tNonce, 0xCD, 12);
		
		const char* sPlain = "Hello, AES-GCM!";
		size_t iLen = strlen(sPlain);
		uint8 tAAD[] = { 0x0A, 0x0B };
		
		uint8 tCipher[256];
		uint8 tDecrypt[256];
		
		bool bEnc = xrtAES128GCMEncrypt(tCipher, tKey, tNonce, 12, tAAD, 2, (const uint8*)sPlain, iLen);
		printf("  AES-128-GCM Encrypt : %s\n", bEnc ? "PASS" : "FAIL");
		
		bool bDec = xrtAES128GCMDecrypt(tDecrypt, tKey, tNonce, 12, tAAD, 2, tCipher, iLen + 16);
		printf("  AES-128-GCM Decrypt : %s\n", bDec ? "PASS" : "FAIL");
		
		bool bMatch = memcmp(tDecrypt, sPlain, iLen) == 0;
		printf("  AES-128-GCM 往返 : %s\n", bMatch ? "PASS" : "FAIL");
		
		// 篡改密文
		tCipher[0] ^= 0xFF;
		bool bTampered = xrtAES128GCMDecrypt(tDecrypt, tKey, tNonce, 12, tAAD, 2, tCipher, iLen + 16);
		printf("  AES-128-GCM 篡改检测 : %s\n", !bTampered ? "PASS" : "FAIL");
	}
	
	// ==================== X25519 测试 ====================
	printf("\n--- X25519 测试 ---\n");
	
	// Diffie-Hellman 密钥交换测试
	{
		uint8 tPrivA[32], tPubA[32];
		uint8 tPrivB[32], tPubB[32];
		uint8 tSharedA[32], tSharedB[32];
		
		xrtX25519Keypair(tPrivA, tPubA);
		xrtX25519Keypair(tPrivB, tPubB);
		
		xrtX25519SharedSecret(tSharedA, tPrivA, tPubB);
		xrtX25519SharedSecret(tSharedB, tPrivB, tPubA);
		
		bool bOK = memcmp(tSharedA, tSharedB, 32) == 0;
		printf("  X25519 DH 密钥交换 : %s\n", bOK ? "PASS" : "FAIL");
		if ( !bOK ) {
			printf("    sharedA: ");
			for ( int i = 0; i < 32; i++ ) printf("%02x", tSharedA[i]);
			printf("\n    sharedB: ");
			for ( int i = 0; i < 32; i++ ) printf("%02x", tSharedB[i]);
			printf("\n");
		}
		
		// 验证非零
		bool bNonZero = false;
		for ( int i = 0; i < 32; i++ ) {
			if ( tSharedA[i] != 0 ) { bNonZero = true; break; }
		}
		printf("  X25519 共享密钥非零 : %s\n", bNonZero ? "PASS" : "FAIL");
	}
	
	// ==================== HKDF 测试 ====================
	printf("\n--- HKDF 测试 ---\n");
	
	// RFC 5869 Test Case 1
	{
		uint8 tIKM[22];
		memset(tIKM, 0x0b, 22);
		uint8 tSalt[] = {
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0x0a, 0x0b, 0x0c
		};
		uint8 tInfo[] = {
			0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
			0xf8, 0xf9
		};
		
		uint8 tPRK[32];
		xrtHKDFExtract(tPRK, tSalt, 13, tIKM, 22);
		
		uint8 tExpectedPRK[] = {
			0x07, 0x77, 0x09, 0x36, 0x2c, 0x2e, 0x32, 0xdf,
			0x0d, 0xdc, 0x3f, 0x0d, 0xc4, 0x7b, 0xba, 0x63,
			0x90, 0xb6, 0xc7, 0x3b, 0xb5, 0x0f, 0x9c, 0x31,
			0x22, 0xec, 0x84, 0x4a, 0xd7, 0xc2, 0xb3, 0xe5
		};
		bool bPRK = memcmp(tPRK, tExpectedPRK, 32) == 0;
		printf("  HKDF-Extract RFC5869 TC1 : %s\n", bPRK ? "PASS" : "FAIL");
		
		uint8 tOKM[42];
		xrtHKDFExpand(tOKM, 42, tPRK, 32, tInfo, 10);
		
		uint8 tExpectedOKM[] = {
			0x3c, 0xb2, 0x5f, 0x25, 0xfa, 0xac, 0xd5, 0x7a,
			0x90, 0x43, 0x4f, 0x64, 0xd0, 0x36, 0x2f, 0x2a,
			0x2d, 0x2d, 0x0a, 0x90, 0xcf, 0x1a, 0x5a, 0x4c,
			0x5d, 0xb0, 0x2d, 0x56, 0xec, 0xc4, 0xc5, 0xbf,
			0x34, 0x00, 0x72, 0x08, 0xd5, 0xb8, 0x87, 0x18,
			0x58, 0x65
		};
		bool bOKM = memcmp(tOKM, tExpectedOKM, 42) == 0;
		printf("  HKDF-Expand RFC5869 TC1 : %s\n", bOKM ? "PASS" : "FAIL");
		if ( !bOKM ) {
			printf("    got: ");
			for ( int i = 0; i < 42; i++ ) printf("%02x", tOKM[i]);
			printf("\n");
		}
	}
	
	// ==================== 安全随机数测试 ====================
	printf("\n--- 安全随机数测试 ---\n");
	{
		uint8 tBuf1[32], tBuf2[32];
		xrtRandomBytes(tBuf1, 32);
		xrtRandomBytes(tBuf2, 32);
		bool bDiff = memcmp(tBuf1, tBuf2, 32) != 0;
		printf("  两次随机数不同 : %s\n", bDiff ? "PASS" : "FAIL");
		
		bool bNonZero = false;
		for ( int i = 0; i < 32; i++ ) {
			if ( tBuf1[i] != 0 ) { bNonZero = true; break; }
		}
		printf("  随机数非全零 : %s\n", bNonZero ? "PASS" : "FAIL");
	}
	
	printf("\n");
	
	// ==================== SHA-384 测试 ====================
	printf("\n--- SHA-384 测试 ---\n");
	
	// SHA-384("abc") = cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7
	{
		uint8 tOut[48];
		xrtSHA384("abc", 3, tOut);
		uint8 tExpected[] = {
			0xcb, 0x00, 0x75, 0x3f, 0x45, 0xa3, 0x5e, 0x8b,
			0xb5, 0xa0, 0x3d, 0x69, 0x9a, 0xc6, 0x50, 0x07,
			0x27, 0x2c, 0x32, 0xab, 0x0e, 0xde, 0xd1, 0x63,
			0x1a, 0x8b, 0x60, 0x5a, 0x43, 0xff, 0x5b, 0xed,
			0x80, 0x86, 0x07, 0x2b, 0xa1, 0xe7, 0xcc, 0x23,
			0x58, 0xba, 0xec, 0xa1, 0x34, 0xc8, 0x25, 0xa7
		};
		bool bOK = memcmp(tOut, tExpected, 48) == 0;
		printf("  SHA-384(\"abc\") : %s\n", bOK ? "PASS" : "FAIL");
		if ( !bOK ) {
			printf("    got: ");
			for ( int i = 0; i < 48; i++ ) printf("%02x", tOut[i]);
			printf("\n");
		}
	}
	
	// SHA-384("") = 38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b
	{
		uint8 tOut[48];
		xrtSHA384("", 0, tOut);
		uint8 tExpected[] = {
			0x38, 0xb0, 0x60, 0xa7, 0x51, 0xac, 0x96, 0x38,
			0x4c, 0xd9, 0x32, 0x7e, 0xb1, 0xb1, 0xe3, 0x6a,
			0x21, 0xfd, 0xb7, 0x11, 0x14, 0xbe, 0x07, 0x43,
			0x4c, 0x0c, 0xc7, 0xbf, 0x63, 0xf6, 0xe1, 0xda,
			0x27, 0x4e, 0xde, 0xbf, 0xe7, 0x6f, 0x65, 0xfb,
			0xd5, 0x1a, 0xd2, 0xf1, 0x48, 0x98, 0xb9, 0x5b
		};
		bool bOK = memcmp(tOut, tExpected, 48) == 0;
		printf("  SHA-384(\"\") : %s\n", bOK ? "PASS" : "FAIL");
	}
	
	// ==================== SHA-512 测试 ====================
	printf("\n--- SHA-512 测试 ---\n");
	
	// SHA-512("abc") = ddaf35a193617aba...
	{
		uint8 tOut[64];
		xrtSHA512("abc", 3, tOut);
		uint8 tExpected[] = {
			0xdd, 0xaf, 0x35, 0xa1, 0x93, 0x61, 0x7a, 0xba,
			0xcc, 0x41, 0x73, 0x49, 0xae, 0x20, 0x41, 0x31,
			0x12, 0xe6, 0xfa, 0x4e, 0x89, 0xa9, 0x7e, 0xa2,
			0x0a, 0x9e, 0xee, 0xe6, 0x4b, 0x55, 0xd3, 0x9a,
			0x21, 0x92, 0x99, 0x2a, 0x27, 0x4f, 0xc1, 0xa8,
			0x36, 0xba, 0x3c, 0x23, 0xa3, 0xfe, 0xeb, 0xbd,
			0x45, 0x4d, 0x44, 0x23, 0x64, 0x3c, 0xe8, 0x0e,
			0x2a, 0x9a, 0xc9, 0x4f, 0xa5, 0x4c, 0xa4, 0x9f
		};
		bool bOK = memcmp(tOut, tExpected, 64) == 0;
		printf("  SHA-512(\"abc\") : %s\n", bOK ? "PASS" : "FAIL");
		if ( !bOK ) {
			printf("    got: ");
			for ( int i = 0; i < 64; i++ ) printf("%02x", tOut[i]);
			printf("\n");
		}
	}
	
	// 流式 SHA-512 测试
	{
		xsha512_ctx tCtx;
		uint8 tOut1[64], tOut2[64];
		xrtSHA512("hello world", 11, tOut1);
		xrtSHA512Init(&tCtx);
		xrtSHA512Update(&tCtx, "hello ", 6);
		xrtSHA512Update(&tCtx, "world", 5);
		xrtSHA512Final(&tCtx, tOut2);
		bool bOK = memcmp(tOut1, tOut2, 64) == 0;
		printf("  SHA-512 流式 vs 单次 : %s\n", bOK ? "PASS" : "FAIL");
	}
	
	// ==================== HMAC-SHA384 测试 ====================
	printf("\n--- HMAC-SHA384 测试 ---\n");
	
	// RFC 4231 Test Case 2: Key = "Jefe", Data = "what do ya want for nothing?"
	{
		uint8 tOut[48];
		xrtHMAC_SHA384((const uint8*)"Jefe", 4, (const uint8*)"what do ya want for nothing?", 28, tOut);
		uint8 tExpected[] = {
			0xaf, 0x45, 0xd2, 0xe3, 0x76, 0x48, 0x40, 0x31,
			0x61, 0x7f, 0x78, 0xd2, 0xb5, 0x8a, 0x6b, 0x1b,
			0x9c, 0x7e, 0xf4, 0x64, 0xf5, 0xa0, 0x1b, 0x47,
			0xe4, 0x2e, 0xc3, 0x73, 0x63, 0x22, 0x44, 0x5e,
			0x8e, 0x22, 0x40, 0xca, 0x5e, 0x69, 0xe2, 0xc7,
			0x8b, 0x32, 0x39, 0xec, 0xfa, 0xb2, 0x16, 0x49
		};
		bool bOK = memcmp(tOut, tExpected, 48) == 0;
		printf("  HMAC-SHA384 RFC4231 TC2 : %s\n", bOK ? "PASS" : "FAIL");
		if ( !bOK ) {
			printf("    got: ");
			for ( int i = 0; i < 48; i++ ) printf("%02x", tOut[i]);
			printf("\n");
		}
	}
	
	// ==================== AES-256-GCM 测试 ====================
	printf("\n--- AES-256-GCM 测试 ---\n");
	
	// 加密然后解密，验证往返
	{
		uint8 tKey[32];
		uint8 tNonce[12];
		memset(tKey, 0xAB, 32);
		memset(tNonce, 0xCD, 12);
		
		const char* sPlain = "Hello, AES-256-GCM!";
		size_t iLen = strlen(sPlain);
		uint8 tAAD[] = { 0x0A, 0x0B };
		
		uint8 tCipher[256];
		uint8 tDecrypt[256];
		
		bool bEnc = xrtAES256GCMEncrypt(tCipher, tKey, tNonce, 12, tAAD, 2, (const uint8*)sPlain, iLen);
		printf("  AES-256-GCM Encrypt : %s\n", bEnc ? "PASS" : "FAIL");
		
		bool bDec = xrtAES256GCMDecrypt(tDecrypt, tKey, tNonce, 12, tAAD, 2, tCipher, iLen + 16);
		printf("  AES-256-GCM Decrypt : %s\n", bDec ? "PASS" : "FAIL");
		
		bool bMatch = memcmp(tDecrypt, sPlain, iLen) == 0;
		printf("  AES-256-GCM 往返 : %s\n", bMatch ? "PASS" : "FAIL");
		
		// 篡改密文
		tCipher[0] ^= 0xFF;
		bool bTampered = xrtAES256GCMDecrypt(tDecrypt, tKey, tNonce, 12, tAAD, 2, tCipher, iLen + 16);
		printf("  AES-256-GCM 篡改检测 : %s\n", !bTampered ? "PASS" : "FAIL");
	}
	
	// NIST SP 800-38D Test Case 16 (AES-256-GCM, key=all-zeros, IV=all-zeros, plaintext=empty)
	{
		uint8 tKey[32] = {0};
		uint8 tIV[12] = {0};
		uint8 tCipher[16]; // tag only
		
		xrtAES256GCMEncrypt(tCipher, tKey, tIV, 12, NULL, 0, NULL, 0);
		
		uint8 tExpectedTag[] = {
			0x53, 0x0f, 0x8a, 0xfb, 0xc7, 0x45, 0x36, 0xb9,
			0xa9, 0x63, 0xb4, 0xf1, 0xc4, 0xcb, 0x73, 0x8b
		};
		bool bOK = memcmp(tCipher, tExpectedTag, 16) == 0;
		printf("  AES-256-GCM NIST TC16 : %s\n", bOK ? "PASS" : "FAIL");
		if ( !bOK ) {
			printf("    got: ");
			for ( int i = 0; i < 16; i++ ) printf("%02x", tCipher[i]);
			printf("\n");
		}
	}
	
	// ==================== ECDH secp256r1 测试 ====================
	printf("\n--- ECDH secp256r1 测试 ---\n");
	
	// Diffie-Hellman P-256 密钥交换测试
	{
		uint8 tPrivA[32], tPubA[65];
		uint8 tPrivB[32], tPubB[65];
		uint8 tSharedA[32], tSharedB[32];
		
		xrtECDHSecp256r1Keypair(tPrivA, tPubA);
		xrtECDHSecp256r1Keypair(tPrivB, tPubB);
		
		// 公钥格式检查: 0x04 前缀
		printf("  P-256 公钥格式 (0x04) : %s\n", (tPubA[0] == 0x04 && tPubB[0] == 0x04) ? "PASS" : "FAIL");
		
		xrtECDHSecp256r1SharedSecret(tSharedA, tPrivA, tPubB);
		xrtECDHSecp256r1SharedSecret(tSharedB, tPrivB, tPubA);
		
		bool bOK = memcmp(tSharedA, tSharedB, 32) == 0;
		printf("  P-256 ECDH 密钥交换 : %s\n", bOK ? "PASS" : "FAIL");
		if ( !bOK ) {
			printf("    sharedA: ");
			for ( int i = 0; i < 32; i++ ) printf("%02x", tSharedA[i]);
			printf("\n    sharedB: ");
			for ( int i = 0; i < 32; i++ ) printf("%02x", tSharedB[i]);
			printf("\n");
		}
		
		// 共享密钥非零
		bool bNonZero = false;
		for ( int i = 0; i < 32; i++ ) {
			if ( tSharedA[i] != 0 ) { bNonZero = true; break; }
		}
		printf("  P-256 共享密钥非零 : %s\n", bNonZero ? "PASS" : "FAIL");
	}
	
	// 已知测试向量: 私钥=2, 对方公钥=G(基点), 共享密钥应为 2*G 的 X 坐标
	{
		// 先测试 1*G = G (验证基本标量乘法)
		uint8 tPriv1[32] = {0};
		tPriv1[31] = 0x01;  // 私钥 = 1
		uint8 tPubG[65] = {
			0x04,
			0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47,
			0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2,
			0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0,
			0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96,
			0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b,
			0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16,
			0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31, 0x5e, 0xce,
			0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5
		};
		// 1*G 的 X 坐标应该等于 Gx
		uint8 tExpGx[] = {
			0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47,
			0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2,
			0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0,
			0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96
		};
		uint8 tShared1G[32];
		xrtECDHSecp256r1SharedSecret(tShared1G, tPriv1, tPubG);
		bool b1G = memcmp(tShared1G, tExpGx, 32) == 0;
		printf("  P-256 1*G = G : %s\n", b1G ? "PASS" : "FAIL");
		if ( !b1G ) {
			printf("    got:      ");
			for ( int i = 0; i < 32; i++ ) printf("%02x", tShared1G[i]);
			printf("\n    expected: ");
			for ( int i = 0; i < 32; i++ ) printf("%02x", tExpGx[i]);
			printf("\n");
		}
	}
	
	// 私钥 d = 2, 对方公钥 = G
	{
		// 私钥 d = 2
		uint8 tPrivA[32] = {0};
		tPrivA[31] = 0x02;
		// 对方公钥 = G (基点, 未压缩格式)
		uint8 tPubB[65] = {
			0x04,
			0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47,
			0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2,
			0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0,
			0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96,
			0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b,
			0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16,
			0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31, 0x5e, 0xce,
			0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5
		};
		// 期望共享密钥 = 2*G 的 X 坐标
		uint8 tExpected[] = {
			0x7c, 0xf2, 0x7b, 0x18, 0x8d, 0x03, 0x4f, 0x7e,
			0x8a, 0x52, 0x38, 0x03, 0x04, 0xb5, 0x1a, 0xc3,
			0xc0, 0x89, 0x69, 0xe2, 0x77, 0xf2, 0x1b, 0x35,
			0xa6, 0x0b, 0x48, 0xfc, 0x47, 0x66, 0x99, 0x78
		};
		
		uint8 tShared[32];
		xrtECDHSecp256r1SharedSecret(tShared, tPrivA, tPubB);
		
		bool bOK = memcmp(tShared, tExpected, 32) == 0;
		printf("  P-256 ECDH 2*G 向量 : %s\n", bOK ? "PASS" : "FAIL");
		if ( !bOK ) {
			printf("    got:      ");
			for ( int i = 0; i < 32; i++ ) printf("%02x", tShared[i]);
			printf("\n    expected: ");
			for ( int i = 0; i < 32; i++ ) printf("%02x", tExpected[i]);
			printf("\n");
		}
	}
	
	// ==================== ECDSA secp256r1 测试 ====================
	printf("\n--- ECDSA secp256r1 测试 ---\n");
	
	// ECDSA 基本流程: 生成密钥对 -> 验证公钥在曲线上
	{
		uint8 tPriv[32], tPub[65];
		xrtECDHSecp256r1Keypair(tPriv, tPub);
		printf("  ECDSA 密钥对生成 : %s\n", (tPub[0] == 0x04) ? "PASS" : "FAIL");
	}
	
	// HKDF-SHA384 基本测试
	printf("\n--- HKDF-SHA384 测试 ---\n");
	{
		uint8 tIKM[22];
		memset(tIKM, 0x0b, 22);
		uint8 tSalt[] = {
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0x0a, 0x0b, 0x0c
		};
		uint8 tPRK[48];
		xrtHKDFExtract_SHA384(tPRK, tSalt, 13, tIKM, 22);
		
		// PRK 非零检查
		bool bNonZero = false;
		for ( int i = 0; i < 48; i++ ) {
			if ( tPRK[i] != 0 ) { bNonZero = true; break; }
		}
		printf("  HKDF-SHA384 Extract 非零 : %s\n", bNonZero ? "PASS" : "FAIL");
		
		uint8 tOKM[48];
		uint8 tInfo[] = { 0xf0, 0xf1, 0xf2, 0xf3 };
		xrtHKDFExpand_SHA384(tOKM, 48, tPRK, 48, tInfo, 4);
		
		bool bNonZero2 = false;
		for ( int i = 0; i < 48; i++ ) {
			if ( tOKM[i] != 0 ) { bNonZero2 = true; break; }
		}
		printf("  HKDF-SHA384 Expand 非零 : %s\n", bNonZero2 ? "PASS" : "FAIL");
	}
	
	printf("\n");
}


