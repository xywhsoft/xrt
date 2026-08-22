/*
 * XRT Example - KV Store
 * XRT 范例 - 键值存储
 *
 * Description / 说明:
 *   EN: Demonstrates an in-memory KV store backed by xdict and JSON persistence.
 *   CN: 演示基于 xdict 的内存 KV 存储与 JSON 持久化。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/tools_kv_store.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/tools_kv_store -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include "../../example_common.h"


int main()
{
	str sPath = NULL;
	xdict hStore = NULL;
	xvalue pTable = NULL;

	xrtInit();

	sPath = exMakeAppFilePath("kv_store_demo.json");
	hStore = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_LOCAL);

	xrtDictSetPtr(hStore, "name", 4, xrtCopyStr("xrt", 4), NULL);
	xrtDictSetPtr(hStore, "mode", 4, xrtCopyStr("demo", 5), NULL);
	xrtDictSetPtr(hStore, "status", 6, xrtCopyStr("active", 7), NULL);

	printf("get name = %s\n", (str)xrtDictGetPtr(hStore, "name", 4));
	printf("count_before_del = %u\n", (unsigned)xrtDictCount(hStore));
	xrtFree(xrtDictRemovePtr(hStore, "mode", 4));
	printf("count_after_del = %u\n", (unsigned)xrtDictCount(hStore));

	pTable = xvoCreateTable();
	DICT_FOREACH(hStore, pKey, pVal) {
		str sVal = *(str*)pVal;
		xvoTableSetText(pTable, pKey->Key, pKey->KeyLen, sVal, strlen(sVal), FALSE);
	}
	xrtStringifyJSON_File(sPath, pTable, TRUE);
	xvoUnref(pTable);

	pTable = xrtParseJSON_File(sPath);
	printf("persisted_name = %s\n", xvoTableGetText(pTable, "name", 4));
	printf("persisted_status = %s\n", xvoTableGetText(pTable, "status", 6));

	DICT_FOREACH(hStore, pKey2, pVal2) {
		xrtFree(*(str*)pVal2);
	}
	xrtDictDestroy(hStore);
	xvoUnref(pTable);
	xrtFileDelete(sPath);
	xrtFree(sPath);
	xrtUnit();
	return 0;
}
