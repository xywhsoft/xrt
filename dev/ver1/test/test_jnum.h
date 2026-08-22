

// JNum 库测试
void Test_JNum(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n JNum 库测试 :\n\n");

	// JNUM-001: 正整数转换
	printf("JNum test subject 1 : 正整数转换\n\n");
	char buffer[64];

	xrtI64ToStr(0, buffer);
	printf("xrtI64ToStr(0) = %s\t\t\t=> 0\n", buffer);
	if (strcmp(buffer, "0") == 0) printf("pass! √\n");
	else printf("fail! ×\n");

	xrtI64ToStr(1, buffer);
	printf("xrtI64ToStr(1) = %s\t\t\t=> 1\n", buffer);
	if (strcmp(buffer, "1") == 0) printf("pass! √\n");
	else printf("fail! ×\n");

	xrtI64ToStr(123, buffer);
	printf("xrtI64ToStr(123) = %s\t\t\t=> 123\n", buffer);
	if (strcmp(buffer, "123") == 0) printf("pass! √\n");
	else printf("fail! ×\n");

	xrtI64ToStr(123456789, buffer);
	printf("xrtI64ToStr(123456789) = %s\t\t=> 123456789\n", buffer);
	if (strcmp(buffer, "123456789") == 0) printf("pass! √\n");
	else printf("fail! ×\n");

	printf("\n");

	// JNUM-002: 负整数转换
	printf("JNum test subject 2 : 负整数转换\n\n");

	xrtI64ToStr(-1, buffer);
	printf("xrtI64ToStr(-1) = %s\t\t\t=> -1\n", buffer);
	if (strcmp(buffer, "-1") == 0) printf("pass! √\n");
	else printf("fail! ×\n");

	xrtI64ToStr(-123, buffer);
	printf("xrtI64ToStr(-123) = %s\t\t=> -123\n", buffer);
	if (strcmp(buffer, "-123") == 0) printf("pass! √\n");
	else printf("fail! ×\n");

	xrtI64ToStr(-123456789, buffer);
	printf("xrtI64ToStr(-123456789) = %s\t=> -123456789\n", buffer);
	if (strcmp(buffer, "-123456789") == 0) printf("pass! √\n");
	else printf("fail! ×\n");

	printf("\n");

	// JNUM-003: INT64边界值
	printf("JNum test subject 3 : INT64边界值\n\n");

	xrtI64ToStr(INT64_MAX, buffer);
	printf("xrtI64ToStr(INT64_MAX) = %s\n", buffer);
	if (strcmp(buffer, "9223372036854775807") == 0) printf("pass! √\n");
	else printf("fail! ×\n");

	xrtI64ToStr(INT64_MIN, buffer);
	printf("xrtI64ToStr(INT64_MIN) = %s\n", buffer);
	if (strcmp(buffer, "-9223372036854775808") == 0) printf("pass! √\n");
	else printf("fail! ×\n");

	printf("\n");

	// JNUM-004: 浮点数转换
	printf("JNum test subject 4 : 浮点数转换\n\n");

	xrtNumToStr(3.14, buffer);
	printf("xrtNumToStr(3.14) = %s\t\t\t=> 3.14\n", buffer);

	xrtNumToStr(-3.14, buffer);
	printf("xrtNumToStr(-3.14) = %s\t\t=> -3.14\n", buffer);

	xrtNumToStr(0.0, buffer);
	printf("xrtNumToStr(0.0) = %s\t\t\t=> 0.0\n", buffer);

	xrtNumToStr(1.0e10, buffer);
	printf("xrtNumToStr(1.0e10) = %s\t\t=> 10000000000.0\n", buffer);

	printf("\n");

	// JNUM-005: 十六进制数字
	printf("JNum test subject 5 : 十六进制数字\n\n");

	xrtU32ToStr(0x123, buffer);
	printf("xrtU32ToStr(0x123) = %s\t\t=> 0x123\n", buffer);
	if (strcmp(buffer, "0x123") == 0) printf("pass! √\n");
	else printf("fail! ×\n");

	xrtU32ToStr(0xABCDEF, buffer);
	printf("xrtU32ToStr(0xABCDEF) = %s\t\t=> 0xabcdef\n", buffer);
	if (strcmp(buffer, "0xabcdef") == 0) printf("pass! √\n");
	else printf("fail! ×\n");

	xrtU64ToStr(0x123456789ABCDEF0ULL, buffer);
	printf("xrtU64ToStr(0x123456789ABCDEF0) = %s\n", buffer);
	if (strcmp(buffer, "0x123456789abcdef0") == 0) printf("pass! √\n");
	else printf("fail! ×\n");

	printf("\n");

	// JNUM-006: 空字符串
	printf("JNum test subject 6 : 空字符串\n\n");

	double result1 = xrtStrToNum("");
	printf("xrtStrToNum(\"\") = %f\t\t\t=> 0.0\n", result1);
	if (result1 == 0.0) printf("pass! √\n");
	else printf("fail! ×\n");

	printf("\n");

	// JNUM-007: 无效输入
	printf("JNum test subject 7 : 无效输入\n\n");

	double result2 = xrtStrToNum("abc");
	printf("xrtStrToNum(\"abc\") = %f\t\t\t=> 0.0\n", result2);
	if (result2 == 0.0) printf("pass! √\n");
	else printf("fail! ×\n");

	double result3 = xrtStrToNum("12a34");
	printf("xrtStrToNum(\"12a34\") = %f\t\t=> 12.0 (部分解析)\n", result3);
	if (result3 == 12.0) printf("pass! √\n");
	else printf("fail! ×\n");

	printf("\n");

	// JNUM-008: 大数字性能
	printf("JNum test subject 8 : 大数字性能\n\n");

#if defined(_WIN32) || defined(_WIN64)
	LARGE_INTEGER frequency, start, end;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&start);

	for (int i = 0; i < 10000; i++) {
		xrtI64ToStr(123456789012345, buffer);
	}

	QueryPerformanceCounter(&end);
	double elapsed = (double)(end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;
	printf("10000次转换耗时: %.3f ms\n", elapsed);
#else
	struct timespec ts_start, ts_end;
	clock_gettime(CLOCK_MONOTONIC, &ts_start);

	for (int i = 0; i < 10000; i++) {
		xrtI64ToStr(123456789012345, buffer);
	}

	clock_gettime(CLOCK_MONOTONIC, &ts_end);
	double elapsed = (ts_end.tv_sec - ts_start.tv_sec) * 1000.0 + (ts_end.tv_nsec - ts_start.tv_nsec) / 1000000.0;
	printf("10000次转换耗时: %.3f ms\n", elapsed);
#endif

	printf("\n\n\n");
	#if defined(_WIN32) || defined(_WIN64)
		system("pause");
	#else
		printf("Press Enter to continue...");
		getchar();
	#endif
	#if defined(_WIN32) || defined(_WIN64)
		system("cls");
	#else
		printf("\033[2J\033[H");
	#endif
}
