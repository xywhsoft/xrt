



// 环形网络缓冲区测试

void Test_NetRingBuf(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n 环形网络缓冲区测试 :\n\n");
	
	// ==================== 基本创建/销毁 ====================
	{
		xnetringbuf tBuf;
		bool bOk = xrtNetRingBufInit(&tBuf, 100);
		printf("  Init (100 -> pow2=128) : %s (cap=%d)\n", bOk ? "PASS" : "FAIL", (int)tBuf.iCapacity);
		printf("  Capacity is pow2 : %s\n", tBuf.iCapacity == 128 ? "PASS" : "FAIL");
		printf("  Mask == cap-1 : %s\n", tBuf.iMask == 127 ? "PASS" : "FAIL");
		printf("  Readable == 0 : %s\n", xrtNetRingBufReadable(&tBuf) == 0 ? "PASS" : "FAIL");
		printf("  Writable == cap : %s\n", xrtNetRingBufWritable(&tBuf) == 128 ? "PASS" : "FAIL");
		xrtNetRingBufFree(&tBuf);
		printf("  Free : PASS\n");
	}
	
	// ==================== 基本读写 ====================
	{
		xnetringbuf tBuf;
		xrtNetRingBufInit(&tBuf, 64);
		
		const char* sData = "Hello Ring Buffer!";
		size_t iLen = strlen(sData);
		size_t iWritten = xrtNetRingBufWrite(&tBuf, sData, iLen);
		printf("  Write %d bytes : %s\n", (int)iLen, iWritten == iLen ? "PASS" : "FAIL");
		printf("  Readable after write : %s\n", xrtNetRingBufReadable(&tBuf) == iLen ? "PASS" : "FAIL");
		
		char aOut[64] = {0};
		size_t iRead = xrtNetRingBufRead(&tBuf, aOut, sizeof(aOut));
		printf("  Read %d bytes : %s\n", (int)iRead, iRead == iLen ? "PASS" : "FAIL");
		printf("  Data match : %s\n", memcmp(aOut, sData, iLen) == 0 ? "PASS" : "FAIL");
		printf("  Readable after read : %s\n", xrtNetRingBufReadable(&tBuf) == 0 ? "PASS" : "FAIL");
		
		xrtNetRingBufFree(&tBuf);
	}
	
	// ==================== Peek 不推进读位置 ====================
	{
		xnetringbuf tBuf;
		xrtNetRingBufInit(&tBuf, 32);
		
		xrtNetRingBufWrite(&tBuf, "ABCDEF", 6);
		
		char aOut[32] = {0};
		size_t iPeeked = xrtNetRingBufPeek(&tBuf, aOut, 3);
		printf("  Peek 3 bytes : %s\n", iPeeked == 3 && memcmp(aOut, "ABC", 3) == 0 ? "PASS" : "FAIL");
		printf("  Readable after peek : %s\n", xrtNetRingBufReadable(&tBuf) == 6 ? "PASS" : "FAIL");
		
		xrtNetRingBufFree(&tBuf);
	}
	
	// ==================== Consume O(1) ====================
	{
		xnetringbuf tBuf;
		xrtNetRingBufInit(&tBuf, 32);
		
		xrtNetRingBufWrite(&tBuf, "ABCDEFGH", 8);
		xrtNetRingBufConsume(&tBuf, 3);
		printf("  Consume 3: readable=%d (%s)\n", (int)xrtNetRingBufReadable(&tBuf),
			xrtNetRingBufReadable(&tBuf) == 5 ? "PASS" : "FAIL");
		
		char aOut[32] = {0};
		xrtNetRingBufRead(&tBuf, aOut, 5);
		printf("  Read after consume : %s\n", memcmp(aOut, "DEFGH", 5) == 0 ? "PASS" : "FAIL");
		
		xrtNetRingBufFree(&tBuf);
	}
	
	// ==================== 环绕 (Wrap-around) 测试 ====================
	{
		xnetringbuf tBuf;
		xrtNetRingBufInit(&tBuf, 16);  // cap=16
		
		// 写入 12 字节
		xrtNetRingBufWrite(&tBuf, "AAAAAAAAAAAA", 12);
		// 读出 10 字节 -> readPos=10, writePos=12
		char aTmp[16];
		xrtNetRingBufRead(&tBuf, aTmp, 10);
		
		// 再写入 10 字节 -> 应该环绕 (writePos=22, 实际位 22&15=6)
		size_t iWr = xrtNetRingBufWrite(&tBuf, "BBBBBBBBBB", 10);
		printf("  Wrap write 10 : %s\n", iWr == 10 ? "PASS" : "FAIL");
		printf("  Readable after wrap : %s\n", xrtNetRingBufReadable(&tBuf) == 12 ? "PASS" : "FAIL");
		
		// 读出全部 12 字节
		char aOut[16] = {0};
		size_t iRd = xrtNetRingBufRead(&tBuf, aOut, 12);
		printf("  Wrap read 12 : %s\n", iRd == 12 ? "PASS" : "FAIL");
		printf("  Data: AA + BBBBBBBBBB : %s\n",
			memcmp(aOut, "AABBBBBBBBBB", 12) == 0 ? "PASS" : "FAIL");
		
		xrtNetRingBufFree(&tBuf);
	}
	
	// ==================== 满/空 边界 ====================
	{
		xnetringbuf tBuf;
		xrtNetRingBufInit(&tBuf, 16);
		
		// 写满
		size_t iWr = xrtNetRingBufWrite(&tBuf, "1234567890123456", 16);
		printf("  Full write : %s (writable=%d)\n", iWr == 16 ? "PASS" : "FAIL",
			(int)xrtNetRingBufWritable(&tBuf));
		printf("  Writable == 0 when full : %s\n", xrtNetRingBufWritable(&tBuf) == 0 ? "PASS" : "FAIL");
		
		// 尝试再写入应返回 0
		size_t iWr2 = xrtNetRingBufWrite(&tBuf, "X", 1);
		printf("  Write when full : %s\n", iWr2 == 0 ? "PASS" : "FAIL");
		
		// 读空
		char aOut[16];
		xrtNetRingBufRead(&tBuf, aOut, 16);
		printf("  Readable == 0 when empty : %s\n", xrtNetRingBufReadable(&tBuf) == 0 ? "PASS" : "FAIL");
		
		xrtNetRingBufFree(&tBuf);
	}
	
	// ==================== 最小容量 (向上对齐到 16) ====================
	{
		xnetringbuf tBuf;
		xrtNetRingBufInit(&tBuf, 1);
		printf("  Min capacity (1 -> 16) : %s\n", tBuf.iCapacity == 16 ? "PASS" : "FAIL");
		xrtNetRingBufFree(&tBuf);
	}
}

