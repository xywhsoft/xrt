/*
 * XRT Example - JSON Config
 * XRT 范例 - JSON 配置管理
 *
 * Description / 说明:
 *   EN: Demonstrates creating, loading, updating and saving a JSON config file.
 *   CN: 演示 JSON 配置文件的创建、加载、更新与保存。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/tools_json_config.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/tools_json_config -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include "../../example_common.h"


int main()
{
	str sPath = NULL;
	xvalue pConfig = NULL;

	xrtInit();

	sPath = exMakeAppFilePath("json_config_demo.json");

	pConfig = xvoCreateTable();
	xvoTableSetText(pConfig, "host", 4, "127.0.0.1", 9, FALSE);
	xvoTableSetInt(pConfig, "port", 4, 8080);
	xvoTableSetBool(pConfig, "debug", 5, TRUE);
	xrtStringifyJSON_File(sPath, pConfig, TRUE);
	xvoUnref(pConfig);

	pConfig = xrtParseJSON_File(sPath);
	xvoTableSetInt(pConfig, "port", 4, 9090);
	xvoTableSetBool(pConfig, "debug", 5, FALSE);
	xrtStringifyJSON_File(sPath, pConfig, TRUE);

	printf("host = %s\n", xvoTableGetText(pConfig, "host", 4));
	printf("port = %lld\n", (long long)xvoTableGetInt(pConfig, "port", 4));
	printf("debug = %s\n", xvoTableGetBool(pConfig, "debug", 5) ? "TRUE" : "FALSE");

	xvoUnref(pConfig);
	xrtFileDelete(sPath);
	xrtFree(sPath);
	xrtUnit();
	return 0;
}
