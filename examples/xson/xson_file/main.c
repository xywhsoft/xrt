/**
 * @file main.c
 * @brief XSON File Example - xrtParseXSON_File/xrtStringifyXSON_File
 *        XSON文件示例 - xrtParseXSON_File/xrtStringifyXSON_File
 *
 * This example demonstrates:
 * - Writing XSON to files
 * - Reading XSON from files and verifying roundtrip
 *
 * 本示例演示：
 * - 将 XSON 写入文件
 * - 从文件读取 XSON 并验证往返结果
 *
 * Build: tcc -DXRT_NO_COROUTINE main.c -lWs2_32 -lIPHLPAPI -lShell32 -o ../../bin/xson_xson_file.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

static xvalue create_file_value(void)
{
	xvalue pRoot = xvoCreateTable();
	xvalue pQueue = NULL;
	xvalue pFlags = NULL;
	xvalue pBlob = NULL;
	uint8* pData = NULL;

	if ( pRoot == NULL ) {
		return NULL;
	}

	xvoTableSetText(pRoot, "name", 4, "file-demo", 9, FALSE);
	xvoTableSetTimeSerial(pRoot, "created", 7, 2026, 3, 25, 10, 30, 45);

	pQueue = xvoCreateList();
	if ( pQueue ) {
		xvoListSetText(pQueue, 1, "alpha", 5, FALSE);
		xvoListSetText(pQueue, 2, "beta", 4, FALSE);
		xvoTableSetValue(pRoot, "queue", 5, pQueue, FALSE);
		xvoUnref(pQueue);
	}

	pFlags = xvoCreateColl();
	if ( pFlags ) {
		xvoCollSetText(pFlags, "xson", 4, FALSE);
		xvoCollSetText(pFlags, "demo", 4, FALSE);
		xvoTableSetValue(pRoot, "flags", 5, pFlags, FALSE);
		xvoUnref(pFlags);
	}

	pBlob = xvoCreateClass(3);
	if ( pBlob ) {
		pData = (uint8*)pBlob->vStruct;
		pData[0] = 0xAA;
		pData[1] = 0xBB;
		pData[2] = 0xCC;
		xvoTableSetValue(pRoot, "blob", 4, pBlob, FALSE);
		xvoUnref(pBlob);
	}

	return pRoot;
}


static void test_file_operations(void)
{
	const char* sFile = "test_data.xson";
	size_t iSize = 0;
	xvalue pRoot = create_file_value();
	xvalue pRead = NULL;
	xvalue pName = NULL;
	xvalue pQueue = NULL;
	str sXson = NULL;
	int iResult = 0;

	printf("=== Test: XSON File Operations ===\n");
	printf("=== 测试：XSON 文件操作 ===\n");

	if ( pRoot == NULL ) {
		printf("Create value failed!\n");
		printf("创建值失败！\n");
		return;
	}

	printf("Writing XSON to file: %s\n", sFile);
	printf("将 XSON 写入文件: %s\n", sFile);

	iResult = xrtStringifyXSON_File((char*)sFile, pRoot, 1, 0);
	if ( iResult <= 0 ) {
		printf("Write file failed!\n");
		printf("写入文件失败！\n");
		xvoUnref(pRoot);
		return;
	}

	pRead = xrtParseXSON_File((char*)sFile);
	if ( pRead == NULL ) {
		printf("Parse file failed!\n");
		printf("解析文件失败！\n");
		xrtFileDelete((char*)sFile);
		xvoUnref(pRoot);
		return;
	}

	printf("Read file successfully!\n");
	printf("读取文件成功！\n");

	pName = xvoTableGetValue(pRead, "name", 4);
	pQueue = xvoTableGetValue(pRead, "queue", 5);

	if ( pName ) {
		printf("  name: %s\n", pName->vText);
	}

	if ( pQueue ) {
		xvalue pItem1 = xvoListGetValue(pQueue, 1);
		xvalue pItem2 = xvoListGetValue(pQueue, 2);

		printf("  queue[1]: %s\n", pItem1 ? (char*)pItem1->vText : "(null)");
		printf("  queue[2]: %s\n", pItem2 ? (char*)pItem2->vText : "(null)");
	}

	sXson = xrtStringifyXSON(pRead, 1, 0, &iSize);
	if ( sXson ) {
		printf("\nRoundtrip output:\n");
		printf("往返输出:\n");
		printf("%s\n", sXson);
		xrtFree(sXson);
	}

	xvoUnref(pRead);
	xvoUnref(pRoot);
	xrtFileDelete((char*)sFile);

	printf("\nTest file deleted.\n");
	printf("测试文件已删除。\n");
}


int main(void)
{
	xrtInit();

	printf("========================================\n");
	printf("  XSON File Example / XSON 文件示例\n");
	printf("========================================\n");

	test_file_operations();

	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");

	xrtUnit();
	return 0;
}
