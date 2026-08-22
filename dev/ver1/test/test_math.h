


// Math 库测试
void Test_Math(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Math 库测试 :\n\n");
	
	// 测试基础随机数函数
	printf("=== 基础随机数函数（线程不安全）===\n");
	printf("xrtRandRange(0, 100) x10:\n");
	for ( int i = 0; i < 10; i++ ) {
		printf("  %d", xrtRandRange(0, 100));
	}
	printf("\n\n");
	
	printf("xrtRand32() x5:\n");
	for ( int i = 0; i < 5; i++ ) {
		printf("  %u\n", xrtRand32());
	}
	printf("\n");
	
	printf("xrtRand64() x5:\n");
	for ( int i = 0; i < 5; i++ ) {
		printf("  %llu\n", xrtRand64());
	}
	printf("\n");
	
	// 测试 Ex 版本 API（线程安全）
	printf("=== Ex 版本 API（调用者管理状态，线程安全）===\n");
	
	// 创建独立的随机数生成器
	xrand myRng = XRAND_INITIALIZER;
	xrtRandSeed(&myRng, 12345, 1);
	
	printf("固定种子 xrtRand32Ex() x5:\n");
	for ( int i = 0; i < 5; i++ ) {
		printf("  %u\n", xrtRand32Ex(&myRng));
	}
	printf("\n");
	
	// 重置种子，验证可重复性
	xrtRandSeed(&myRng, 12345, 1);
	printf("重置种子后 xrtRand32Ex() x5（应与上面相同）:\n");
	for ( int i = 0; i < 5; i++ ) {
		printf("  %u\n", xrtRand32Ex(&myRng));
	}
	printf("\n");
	
	// 测试范围随机数
	xrtRandSeed(&myRng, 67890, 3);
	printf("xrtRandRangeEx(1, 6) x10（模拟掷骰子）:\n");
	for ( int i = 0; i < 10; i++ ) {
		printf("  %d", xrtRandRangeEx(&myRng, 1, 6));
	}
	printf("\n\n");
	
	// 测试64位随机数
	xrand rngLow = XRAND_INITIALIZER;
	xrand rngHigh = XRAND_INITIALIZER;
	xrtRandSeed(&rngLow, 11111, 1);
	xrtRandSeed(&rngHigh, 22222, 3);
	
	printf("xrtRand64Ex() x3:\n");
	for ( int i = 0; i < 3; i++ ) {
		printf("  0x%016llX\n", xrtRand64Ex(&rngLow, &rngHigh));
	}
	printf("\n");
	
	// 测试独立随机数对象
	printf("=== 独立随机数对象 ===\n");
	ptr pRandA = xrtRandCreate(123456, 9);
	ptr pRandB = xrtRandCreate(123456, 9);
	bool bObj32Ok = xrtRand32Obj(pRandA) == xrtRand32Obj(pRandB);
	bool bObj64Ok = xrtRand64Obj(pRandA) == xrtRand64Obj(pRandB);
	bool bObjRangeOk = TRUE;
	for ( int i = 0; i < 16; i++ ) {
		int a = xrtRandRangeObj(pRandA, 1, 6);
		int b = xrtRandRangeObj(pRandB, 1, 6);
		if ( a != b || a < 1 || a > 6 ) {
			bObjRangeOk = FALSE;
		}
	}
	xrtRandSeedObj(pRandA, 2026, 7);
	xrtRandSeedObj(pRandB, 2026, 7);
	str sRandA = xrtRandStrObj(pRandA, "ab", 0, 12);
	str sRandB = xrtRandStrObj(pRandB, "ab", 0, 12);
	bool bObjStrOk = xrtStrComp(sRandA, sRandB, 0, FALSE) == 0 && xrtStrByteLen(sRandA, 0) == 12;
	printf("  xrtRand32Obj repeat  : %s\n", bObj32Ok ? "PASS" : "FAIL");
	printf("  xrtRand64Obj repeat  : %s\n", bObj64Ok ? "PASS" : "FAIL");
	printf("  xrtRandRangeObj bound: %s\n", bObjRangeOk ? "PASS" : "FAIL");
	printf("  xrtRandStrObj repeat : %s\n", bObjStrOk ? "PASS" : "FAIL");
	xrtFree(sRandA);
	xrtFree(sRandB);
	xrtRandDestroy(pRandA);
	xrtRandDestroy(pRandB);
	printf("\n");
	
	printf("=== xrtMath helpers ===\n");
	printf("  xrtMathMin(-2, 5)      = %s\n", xrtMathMin(-2.0, 5.0) == -2.0 ? "PASS" : "FAIL");
	printf("  xrtMathClamp(12,0,10)  = %s\n", xrtMathClamp(12.0, 0.0, 10.0) == 10.0 ? "PASS" : "FAIL");
	printf("  xrtMathSign(-3)        = %s\n", xrtMathSign(-3.0) == -1 ? "PASS" : "FAIL");
	printf("  xrtMathFract(-1.25)    = %s\n", fabs(xrtMathFract(-1.25) - 0.75) <= 1e-12 ? "PASS" : "FAIL");
	printf("  xrtMathDeg(pi)         = %s\n", fabs(xrtMathDeg(XRT_MATH_PI) - 180.0) <= 1e-12 ? "PASS" : "FAIL");
	printf("  xrtMathHypot(3,4)      = %s\n", fabs(xrtMathHypot(3.0, 4.0) - 5.0) <= 1e-12 ? "PASS" : "FAIL");
	printf("  xrtMathIsNaN(nan)      = %s\n", xrtMathIsNaN(XRT_MATH_NAN) ? "PASS" : "FAIL");
	printf("\n");
	
	printf("Math 库测试完成\n");
}



// 约等于函数测试
void Test_Approx(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n 约等于函数测试 :\n\n");
	
	// === 整数约等于测试 ===
	printf("=== xrtIntApprox 整数约等于 ===\n\n");
	
	// 百分比模式测试
	printf("百分比模式 (1%% 容差):\n");
	xCore->iApproxIntMode = XRT_APPROX_PERCENT;
	xCore->fApproxIntTol = 0.01;  // 1%%
	printf("  10000 ~ 9999  = %s (应为 TRUE)\n", xrtIntApprox(10000, 9999) ? "TRUE" : "FALSE");
	printf("  10000 ~ 9900  = %s (应为 TRUE)\n", xrtIntApprox(10000, 9900) ? "TRUE" : "FALSE");
	printf("  10000 ~ 9899  = %s (应为 FALSE)\n", xrtIntApprox(10000, 9899) ? "TRUE" : "FALSE");
	printf("  10000 ~ 9000  = %s (应为 FALSE)\n", xrtIntApprox(10000, 9000) ? "TRUE" : "FALSE");
	printf("  -1000 ~ -990  = %s (应为 TRUE)\n", xrtIntApprox(-1000, -990) ? "TRUE" : "FALSE");
	printf("\n");
	
	// 差值模式测试
	printf("差值模式 (容差=10):\n");
	xCore->iApproxIntMode = XRT_APPROX_DIFF;
	xCore->fApproxIntTol = 10.0;
	printf("  100 ~ 95   = %s (应为 TRUE)\n", xrtIntApprox(100, 95) ? "TRUE" : "FALSE");
	printf("  100 ~ 110  = %s (应为 TRUE)\n", xrtIntApprox(100, 110) ? "TRUE" : "FALSE");
	printf("  100 ~ 111  = %s (应为 FALSE)\n", xrtIntApprox(100, 111) ? "TRUE" : "FALSE");
	printf("  9 ~ 8      = %s (应为 TRUE)\n", xrtIntApprox(9, 8) ? "TRUE" : "FALSE");
	printf("  10 ~ 21    = %s (应为 FALSE)\n", xrtIntApprox(10, 21) ? "TRUE" : "FALSE");
	printf("\n");
	
	// === 浮点数约等于测试 ===
	printf("=== xrtNumApprox 浮点数约等于 ===\n\n");
	
	// 百分比模式测试
	printf("百分比模式 (0.1%% 容差):\n");
	xCore->iApproxNumMode = XRT_APPROX_PERCENT;
	xCore->fApproxNumTol = 0.001;  // 0.1%%
	printf("  100.0 ~ 99.95  = %s (应为 TRUE)\n", xrtNumApprox(100.0, 99.95) ? "TRUE" : "FALSE");
	printf("  100.0 ~ 99.8   = %s (应为 FALSE)\n", xrtNumApprox(100.0, 99.8) ? "TRUE" : "FALSE");
	printf("\n");
	
	// 差值模式测试
	printf("差值模式 (容差=0.001):\n");
	xCore->iApproxNumMode = XRT_APPROX_DIFF;
	xCore->fApproxNumTol = 0.001;
	printf("  3.14159 ~ 3.14160  = %s (应为 TRUE)\n", xrtNumApprox(3.14159, 3.14160) ? "TRUE" : "FALSE");
	printf("  3.14159 ~ 3.14300  = %s (应为 FALSE)\n", xrtNumApprox(3.14159, 3.14300) ? "TRUE" : "FALSE");
	printf("\n");
	
	// === 时间约等于测试 ===
	printf("=== xrtTimeApprox 时间约等于 ===\n\n");
	
	xtime t1 = xrtDateTimeSerial(2026, 1, 10, 12, 0, 0);
	xtime t2 = xrtDateTimeSerial(2026, 1, 10, 12, 0, 3);
	xtime t3 = xrtDateTimeSerial(2026, 1, 10, 12, 0, 10);
	
	printf("容差=5秒:\n");
	xCore->iApproxTimeTol = 5;  // 5秒
	printf("  12:00:00 ~ 12:00:03 = %s (应为 TRUE)\n", xrtTimeApprox(t1, t2) ? "TRUE" : "FALSE");
	printf("  12:00:00 ~ 12:00:10 = %s (应为 FALSE)\n", xrtTimeApprox(t1, t3) ? "TRUE" : "FALSE");
	printf("\n");
	
	printf("容差=1分钟:\n");
	xCore->iApproxTimeTol = 1 * XRT_TIME_MINUTE;
	printf("  12:00:00 ~ 12:00:10 = %s (应为 TRUE)\n", xrtTimeApprox(t1, t3) ? "TRUE" : "FALSE");
	printf("\n");
	
	printf("约等于函数测试完成\n");
}

