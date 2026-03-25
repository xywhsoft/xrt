/**
 * @file main.c
 * @brief Generate XSON Example - xrtStringifyXSON
 *        生成XSON示例 - xrtStringifyXSON
 *
 * This example demonstrates:
 * - Creating list/set/time/class values
 * - Formatted output and ignore-unsupported flags
 *
 * 本示例演示：
 * - 创建 list/set/time/class 值
 * - 格式化输出与忽略不支持类型标志
 *
 * Build: tcc -DXRT_NO_COROUTINE main.c -lWs2_32 -lIPHLPAPI -lShell32 -o ../../bin/xson_generate_xson.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

static xvalue create_demo_value(void)
{
	xvalue pRoot = xvoCreateTable();
	xvalue pScores = NULL;
	xvalue pTags = NULL;
	xvalue pBlob = NULL;
	uint8* pData = NULL;

	if ( pRoot == NULL ) {
		return NULL;
	}

	xvoTableSetText(pRoot, "name", 4, "Alice", 5, FALSE);
	xvoTableSetTimeSerial(pRoot, "created", 7, 2000, 1, 2, 12, 0, 0);

	pScores = xvoCreateList();
	if ( pScores ) {
		xvoListSetInt(pScores, 1, 95);
		xvoListSetInt(pScores, 3, 88);
		xvoTableSetValue(pRoot, "scores", 6, pScores, FALSE);
		xvoUnref(pScores);
	}

	pTags = xvoCreateColl();
	if ( pTags ) {
		xvoCollSetText(pTags, "admin", 5, FALSE);
		xvoCollSetText(pTags, "dev", 3, FALSE);
		xvoTableSetValue(pRoot, "tags", 4, pTags, FALSE);
		xvoUnref(pTags);
	}

	pBlob = xvoCreateClass(4);
	if ( pBlob ) {
		pData = (uint8*)pBlob->vStruct;
		pData[0] = 0x01;
		pData[1] = 0x02;
		pData[2] = 0x03;
		pData[3] = 0x04;
		xvoTableSetValue(pRoot, "blob", 4, pBlob, FALSE);
		xvoUnref(pBlob);
	}

	return pRoot;
}


static void test_stringify_mixed_object(void)
{
	size_t iSize = 0;
	xvalue pRoot = create_demo_value();
	str sXson = NULL;

	printf("=== Test: Stringify Mixed XSON Object ===\n");
	printf("=== 测试：生成混合 XSON 对象 ===\n");

	if ( pRoot == NULL ) {
		printf("Create value failed!\n");
		printf("创建值失败！\n");
		return;
	}

	printf("Compact output:\n");
	printf("紧凑输出:\n");

	sXson = xrtStringifyXSON(pRoot, 0, 0, &iSize);
	if ( sXson ) {
		printf("  %s\n", sXson);
		xrtFree(sXson);
	}

	printf("\nFormatted output:\n");
	printf("格式化输出:\n");

	sXson = xrtStringifyXSON(pRoot, 1, 0, &iSize);
	if ( sXson ) {
		printf("%s\n", sXson);
		xrtFree(sXson);
	}

	xvoUnref(pRoot);
}


static void test_ignore_unsupported(void)
{
	size_t iSize = 0;
	xvalue pRoot = xvoCreateTable();
	str sXson = NULL;

	printf("\n=== Test: Ignore Unsupported Types ===\n");
	printf("=== 测试：忽略不支持类型 ===\n");

	if ( pRoot == NULL ) {
		return;
	}

	xvoTableSetText(pRoot, "name", 4, "demo", 4, FALSE);
	xvoTableSetPoint(pRoot, "ptr", 3, pRoot);

	sXson = xrtStringifyXSON(pRoot, 1, 0, &iSize);
	printf("Default mode: %s\n", sXson ? "unexpected success" : "failed as expected");
	printf("默认模式: %s\n", sXson ? "意外成功" : "按预期失败");
	if ( sXson ) {
		xrtFree(sXson);
	}

	sXson = xrtStringifyXSON(pRoot, 1, XSON_F_IGNORE_UNSUPPORTED_ENCODE, &iSize);
	if ( sXson ) {
		printf("\nIgnore unsupported output:\n");
		printf("忽略不支持类型后的输出:\n");
		printf("%s\n", sXson);
		xrtFree(sXson);
	}

	xvoUnref(pRoot);
}


int main(void)
{
	xrtInit();

	printf("========================================\n");
	printf("  Generate XSON Example / 生成 XSON 示例\n");
	printf("========================================\n");

	test_stringify_mixed_object();
	test_ignore_unsupported();

	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");

	xrtUnit();
	return 0;
}
