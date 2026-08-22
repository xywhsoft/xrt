/**
 * @file main.c
 * @brief JSON File Example - xrtParseJSON_File/xrtStringifyJSON_File
 *        JSON文件示例 - xrtParseJSON_File/xrtStringifyJSON_File
 * 
 * This example demonstrates:
 * - Reading JSON from files
 * - Writing JSON to files
 * 
 * 本示例演示：
 * - 从文件读取JSON
 * - 将JSON写入文件
 * 
 * Build: tcc main.c -o ../../bin/json_json_file.exe
 */

#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

void test_file_operations(void)
{
	printf("=== Test: JSON File Operations ===\n");
	printf("=== 测试：JSON文件操作 ===\n");
	
	const char* szFilename = "test_data.json";
	
	xvalue pTable = xvoCreateTable();
	xvoTableSetText(pTable, "name", 4, "Test User", 9, false);
	xvoTableSetInt(pTable, "version", 7, 100);
	xvoTableSetBool(pTable, "enabled", 7, true);
	
	printf("Writing JSON to file: %s\n", szFilename);
	printf("将JSON写入文件: %s\n", szFilename);
	
	int result = xrtStringifyJSON_File((char*)szFilename, pTable, 1);
	if (result == 0)
	{
		printf("File written successfully!\n");
		printf("文件写入成功！\n");
		
		printf("\nReading JSON from file...\n");
		printf("\n从文件读取JSON...\n");
		
		xvalue pRead = xrtParseJSON_File((char*)szFilename);
		if (pRead)
		{
			printf("File parsed successfully!\n");
			printf("文件解析成功！\n");
			
			xvalue pName = xvoTableGetValue(pRead, "name", 4);
			if (pName)
			{
				printf("  name: %s\n", pName->vText);
				xvoUnref(pName);
			}
			
			xvalue pVer = xvoTableGetValue(pRead, "version", 7);
			if (pVer)
			{
				printf("  version: %lld\n", (long long)pVer->vInt);
				xvoUnref(pVer);
			}
			
			xvalue pEnabled = xvoTableGetValue(pRead, "enabled", 7);
			if (pEnabled)
			{
				printf("  enabled: %s\n", pEnabled->vBool ? "true" : "false");
				xvoUnref(pEnabled);
			}
			
			xvoUnref(pRead);
		}
		else
		{
			printf("Failed to parse file!\n");
			printf("解析文件失败！\n");
		}
		
		xrtFileDelete((char*)szFilename);
		printf("\nTest file deleted.\n");
		printf("测试文件已删除。\n");
	}
	else
	{
		printf("Failed to write file!\n");
		printf("写入文件失败！\n");
	}
	
	xvoUnref(pTable);
}

void test_roundtrip(void)
{
	printf("\n=== Test: JSON Roundtrip ===\n");
	printf("=== 测试：JSON往返 ===\n");
	
	const char* szFilename = "roundtrip.json";
	
	xvalue pOriginal = xvoCreateTable();
	xvoTableSetText(pOriginal, "key1", 4, "value1", 6, false);
	xvoTableSetInt(pOriginal, "key2", 4, 42);
	
	xvalue pArr = xvoCreateArray();
	xvoArrayAppendInt(pArr, 1);
	xvoArrayAppendInt(pArr, 2);
	xvoArrayAppendInt(pArr, 3);
	xvoTableSetValue(pOriginal, "numbers", 7, pArr, true);
	xvoUnref(pArr);
	
	printf("Writing original structure...\n");
	printf("写入原始结构...\n");
	xrtStringifyJSON_File((char*)szFilename, pOriginal, 1);
	
	printf("Reading back...\n");
	printf("读回...\n");
	xvalue pRestored = xrtParseJSON_File((char*)szFilename);
	
	if (pRestored)
	{
		printf("Roundtrip successful!\n");
		printf("往返成功！\n");
		
		size_t size = 0;
		str pJson = xrtStringifyJSON(pRestored, 1, &size);
		if (pJson)
		{
			printf("Restored content:\n");
			printf("恢复的内容:\n");
			printf("%s\n", pJson);
			xrtFree(pJson);
		}
		
		xvoUnref(pRestored);
	}
	
	xrtFileDelete((char*)szFilename);
	xvoUnref(pOriginal);
}

int main(void)
{
	xrtInit();
	
	printf("========================================\n");
	printf("  JSON File Example / JSON文件示例\n");
	printf("========================================\n");
	
	test_file_operations();
	test_roundtrip();
	
	printf("\n=== All tests completed! ===\n");
	printf("=== 所有测试完成！===\n");
	
	xrtUnit();
	return 0;
}
