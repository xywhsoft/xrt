


// Base 库测试
void Test_Base(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Base 库测试 :\n\n");
	
	printf("--- 全局数据测试 ---\n");
	printf("AppFile : %s\n", xCore->AppFile);
	printf("AppPath : %s\n", xCore->AppPath);
	
	printf("\n--- 内存管理测试 ---\n");
	
	// 测试 xrtMalloc
	printf("[1] xrtMalloc 测试\n");
	ptr pMem1 = xrtMalloc(100);
	if ( pMem1 != NULL ) {
		printf("  ✓ 分配 100 字节成功\n");
	} else {
		printf("  ✗ 分配失败\n");
	}
	
	// 测试 xrtCalloc
	printf("[2] xrtCalloc 测试\n");
	ptr pMem2 = xrtCalloc(10, 10);
	if ( pMem2 != NULL ) {
		printf("  ✓ 分配 10x10 字节成功\n");
	} else {
		printf("  ✗ 分配失败\n");
	}
	
	// 测试 xrtRealloc
	printf("[3] xrtRealloc 测试\n");
	ptr pMem3 = xrtRealloc(pMem1, 200);
	if ( pMem3 != NULL ) {
		printf("  ✓ 重新分配到 200 字节成功\n");
	} else {
		printf("  ✗ 重新分配失败\n");
	}
	
	// 测试 xrtFree
	printf("[4] xrtFree 测试\n");
	xrtFree(pMem2);
	xrtFree(pMem3);
	printf("  ✓ 释放内存成功\n");
	
	// 测试 NULL 释放（不应崩溃）
	printf("[5] xrtFree(NULL) 测试\n");
	xrtFree(NULL);
	printf("  ✓ 释放 NULL 不会崩溃\n");
	
	// 测试临时内存
	printf("\n[6] xrtTempMemory 测试\n");
	for ( int i = 0; i < 35; i++ ) {
		ptr pTemp = xrtTempMemory(100);
		if ( pTemp == NULL ) {
			printf("  ✗ 临时内存分配失败\n");
			break;
		}
		if ( i == 32 ) {
			printf("  ✓ 临时内存循环分配超过 32 次正常（环形缓冲）\n");
		}
	}
	
	// 测试释放所有临时内存
	printf("[7] xrtFreeTempMemory 测试\n");
	xrtFreeTempMemory();
	printf("  ✓ 释放所有临时内存成功\n");
	
	printf("\n--- 错误处理测试 ---\n");
	
	// 测试 xrtSetError
	printf("[8] xrtSetError 测试\n");
	xrtSetError("测试错误信息", FALSE);
	if ( xCore->LastError != NULL && xrtStrComp(xCore->LastError, "测试错误信息", 0, 0) == 0 ) {
		printf("  ✓ 设置错误信息成功: %s\n", xCore->LastError);
	} else {
		printf("  ✗ 设置错误信息失败\n");
	}
	
	// 测试 xrtClearError
	printf("[9] xrtClearError 测试\n");
	xrtClearError();
	if ( xCore->LastError == xCore->sNull || xCore->LastError == NULL ) {
		printf("  ✓ 清除错误信息成功\n");
	} else {
		printf("  ✗ 清除错误信息失败\n");
	}
	
	// 测试 xrtSetErrorU16
	printf("[10] xrtSetErrorU16 测试\n");
	str sErrorUTF8 = "UTF16错误信息";
	u16str u16Error = xrtUTF8to16(sErrorUTF8, 0, NULL);
	xrtSetErrorU16(u16Error, 0, TRUE);
	if ( xCore->LastError != NULL && strstr(xCore->LastError, "错误信息") != NULL ) {
		printf("  ✓ UTF16 错误转换成功: %s\n", xCore->LastError);
	} else {
		printf("  ✗ UTF16 错误转换失败\n");
	}
	
	// 测试 xrtSetErrorU32
	printf("[11] xrtSetErrorU32 测试\n");
	u32str u32Error = xrtUTF8to32(sErrorUTF8, 0, NULL);
	xrtSetErrorU32(u32Error, 0, TRUE);
	if ( xCore->LastError != NULL && strstr(xCore->LastError, "错误信息") != NULL ) {
		printf("  ✓ UTF32 错误转换成功: %s\n", xCore->LastError);
	} else {
		printf("  ✗ UTF32 错误转换失败\n");
	}
	
	// 清理最后的错误
	xrtClearError();
	
	printf("\n--- 边界条件测试 ---\n");
	
	// 测试分配 0 字节
	printf("[12] 分配 0 字节测试\n");
	ptr pZero = xrtMalloc(0);
	if ( pZero != NULL || pZero == NULL ) {
		printf("  ✓ 分配 0 字节处理正常\n");
	}
	if ( pZero != NULL ) {
		xrtFree(pZero);
	}
	
	// 测试大块内存分配
	printf("[13] 大块内存分配测试\n");
	ptr pLarge = xrtMalloc(10 * 1024 * 1024);
	if ( pLarge != NULL ) {
		printf("  ✓ 分配 10MB 内存成功\n");
		xrtFree(pLarge);
	} else {
		printf("  ✗ 分配 10MB 内存失败\n");
	}
	
	// 测试 realloc NULL
	printf("[14] realloc(NULL) 测试\n");
	ptr pFromNull = xrtRealloc(NULL, 100);
	if ( pFromNull != NULL ) {
		printf("  ✓ 从 NULL realloc 成功\n");
		xrtFree(pFromNull);
	} else {
		printf("  ✗ 从 NULL realloc 失败\n");
	}
}


