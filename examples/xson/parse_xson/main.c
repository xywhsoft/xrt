/**
 * @file main.c
 * @brief Parse XSON Example - xrtParseXSON
 *        解析XSON示例 - xrtParseXSON
 *
 * This example demonstrates:
 * - Parsing JSON-compatible text with XSON
 * - Parsing list/set/time/class extended values
 *
 * 本示例演示：
 * - 使用 XSON 解析兼容 JSON 的文本
 * - 解析 list/set/time/class 扩展值
 *
 * Build: tcc -DXRT_NO_COROUTINE main.c -lWs2_32 -lIPHLPAPI -lShell32 -o ../../bin/xson_parse_xson.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

static void print_time_value(xtime tValue)
{
	str sTime = xrtTimeFormat(tValue, "yyyy-mm-dd HH:nn:ss");

	if ( sTime ) {
		printf("%s\n", sTime);
		xrtFree(sTime);
	} else {
		printf("(format failed)\n");
	}
}

static void test_parse_mixed_object(void)
{
	printf("=== Test: Parse Mixed XSON Object ===\n");
	printf("=== 测试：解析混合 XSON 对象 ===\n");

	const char* sXson = "{\"name\":\"Alice\",\"scores\":list[1:95,3:88],\"tags\":set{\"admin\",\"dev\"},\"created\":time(2000-01-02 12:00:00),\"blob\":class(AQIDBA==)}";
	printf("Input: %s\n", sXson);
	printf("输入: %s\n", sXson);

	xvalue pRoot = xrtParseXSON((char*)sXson, strlen(sXson));
	if ( !xvoIsNull(pRoot) ) {
		xvalue pName = xvoTableGetValue(pRoot, "name", 4);
		xvalue pScores = xvoTableGetValue(pRoot, "scores", 6);
		xvalue pTags = xvoTableGetValue(pRoot, "tags", 4);
		xvalue pCreated = xvoTableGetValue(pRoot, "created", 7);
		xvalue pBlob = xvoTableGetValue(pRoot, "blob", 4);

		printf("Parsed successfully!\n");
		printf("解析成功！\n");

		if ( !xvoIsNull(pName) ) {
			printf("  name: %s\n", pName->vText);
		}

		if ( !xvoIsNull(pScores) ) {
			xvalue pScore1 = xvoListGetValue(pScores, 1);
			xvalue pScore3 = xvoListGetValue(pScores, 3);

			printf("  scores[1]: %lld\n", !xvoIsNull(pScore1) ? (long long)pScore1->vInt : -1LL);
			printf("  scores[3]: %lld\n", !xvoIsNull(pScore3) ? (long long)pScore3->vInt : -1LL);
		}

		if ( !xvoIsNull(pTags) ) {
			size_t iSize = 0;
			str sTags = xrtStringifyXSON(pTags, 0, 0, &iSize);

			printf("  tags: %s\n", sTags ? (char*)sTags : "(stringify failed)");

			if ( sTags ) {
				xrtFree(sTags);
			}
		}

		if ( !xvoIsNull(pCreated) ) {
			printf("  created: ");
			print_time_value(pCreated->vTime);
		}

		if ( !xvoIsNull(pBlob) ) {
			uint8* pData = (uint8*)pBlob->vStruct;

			printf("  blob size: %u\n", pBlob->Size);
			printf("  blob data:");

			for ( uint32 i = 0; i < pBlob->Size; i++ ) {
				printf(" %02X", pData[i]);
			}
			printf("\n");
		}

		xvoUnref(pRoot);
	} else {
		printf("Parse failed!\n");
		printf("解析失败！\n");
	}
}


static void test_parse_implicit_containers(void)
{
	printf("\n=== Test: Parse Implicit Containers ===\n");
	printf("=== 测试：解析隐式容器 ===\n");

	const char* sListXson = "[1:\"Alice\",5:time(2000-01-02 12:00:00)]";
	const char* sSetXson = "{\"dev\",\"ops\",\"qa\"}";

	printf("Implicit list input: %s\n", sListXson);
	printf("隐式列表输入: %s\n", sListXson);

	xvalue pList = xrtParseXSON((char*)sListXson, strlen(sListXson));
	if ( !xvoIsNull(pList) ) {
		xvalue pName = xvoListGetValue(pList, 1);
		xvalue pTime = xvoListGetValue(pList, 5);

		printf("  root type: %s\n", (pList->Type == XVO_DT_LIST) ? "list" : "unexpected");
		if ( !xvoIsNull(pName) ) {
			printf("  list[1]: %s\n", pName->vText);
		}
		if ( !xvoIsNull(pTime) ) {
			printf("  list[5]: ");
			print_time_value(pTime->vTime);
		}

		xvoUnref(pList);
	}

	printf("\nImplicit set input: %s\n", sSetXson);
	printf("隐式集合输入: %s\n", sSetXson);

	xvalue pSet = xrtParseXSON((char*)sSetXson, strlen(sSetXson));
	if ( !xvoIsNull(pSet) ) {
		size_t iSize = 0;
		str sOut = xrtStringifyXSON(pSet, 0, 0, &iSize);

		printf("  root type: %s\n", (pSet->Type == XVO_DT_COLL) ? "set" : "unexpected");
		printf("  canonical: %s\n", sOut ? (char*)sOut : "(stringify failed)");

		if ( sOut ) {
			xrtFree(sOut);
		}
		xvoUnref(pSet);
	}
}


static void test_parse_json_compatibility(void)
{
	printf("\n=== Test: Parse Standard JSON With XSON ===\n");
	printf("=== 测试：使用 XSON 解析标准 JSON ===\n");

	const char* sJson = "{\"project\":\"xrt\",\"items\":[1,2,3],\"active\":true}";
	printf("Input: %s\n", sJson);
	printf("输入: %s\n", sJson);

	xvalue pRoot = xrtParseXSON((char*)sJson, strlen(sJson));
	if ( !xvoIsNull(pRoot) ) {
		xvalue pProject = xvoTableGetValue(pRoot, "project", 7);
		xvalue pItems = xvoTableGetValue(pRoot, "items", 5);

		printf("Parsed successfully!\n");
		printf("解析成功！\n");

		if ( !xvoIsNull(pProject) ) {
			printf("  project: %s\n", pProject->vText);
		}

		if ( !xvoIsNull(pItems) ) {
			printf("  items count: %u\n", xvoArrayItemCount(pItems));
		}

		xvoUnref(pRoot);
	}
}


int main(void)
{
	xrtInit();

	printf("========================================\n");
	printf("  Parse XSON Example / 解析 XSON 示例\n");
	printf("========================================\n");

	test_parse_mixed_object();
	test_parse_implicit_containers();
	test_parse_json_compatibility();

	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");

	xrtUnit();
	return 0;
}
