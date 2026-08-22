/*
 * XRT Example - Memory Debug
 * XRT 范例 - 内存调试
 *
 * Description / 说明:
 *   EN: Demonstrates enabling memory debug, exporting text and JSON reports,
 *       reading the generated files, and cleaning them up.
 *   CN: 演示启用内存调试、导出文本和 JSON 报告、读取生成文件并清理它们。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/base_mem_debug.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/base_mem_debug -lm -lpthread
 */

#define XRT_MEM_DEBUG
#include "../../../xrt.c"
#include <stdio.h>
#include <string.h>


static bool procContains(const char* sText, const char* sNeedle)
{
	if ( sText == NULL || sNeedle == NULL ) {
		return FALSE;
	}

	return strstr(sText, sNeedle) != NULL;
}


int main(void)
{
	char* pLive = NULL;
	char* pTemp = NULL;
	str sTextPath = NULL;
	str sJsonPath = NULL;
	str sTextReport = NULL;
	str sJsonReport = NULL;
	bool bTextDump = FALSE;
	bool bJsonDump = FALSE;
	bool bTextHasHeader = FALSE;
	bool bJsonHasHeader = FALSE;
	int iRet = 0;

	xrtInit();

	xrtMemDebugReset();
	xrtMemDebugEnable(TRUE);

	sTextPath = xrtPathJoin(2u, xCore.AppPath, "xrt_mem_debug_report.txt");
	sJsonPath = xrtPathJoin(2u, xCore.AppPath, "xrt_mem_debug_report.json");
	pLive = (char*)xrtMalloc(64u);
	pTemp = (char*)xrtTempMemory(32u);
	if ( sTextPath == NULL || sJsonPath == NULL || pLive == NULL || pTemp == NULL ) {
		printf("prepare = failed\n");
		iRet = 1;
		goto cleanup;
	}

	strcpy(pLive, "mem-debug-live-block");
	strcpy(pTemp, "temp-scratch");

	bTextDump = xrtMemDebugDumpText(sTextPath);
	bJsonDump = xrtMemDebugDumpJson(sJsonPath);
	if ( !bTextDump || !bJsonDump ) {
		printf("dump = failed\n");
		iRet = 1;
		goto cleanup;
	}

	sTextReport = xrtFileReadAll(sTextPath, 0, NULL);
	sJsonReport = xrtFileReadAll(sJsonPath, 0, NULL);
	if ( sTextReport == NULL || sJsonReport == NULL ) {
		printf("read_report = failed\n");
		iRet = 1;
		goto cleanup;
	}

	bTextHasHeader = procContains(sTextReport, "live_alloc_count")
		&& procContains(sTextReport, "LEAK");
	bJsonHasHeader = procContains(sJsonReport, "\"live_alloc_count\"")
		&& procContains(sJsonReport, "\"live_allocations\"");

	printf("debug_enabled = %s\n", xrtMemDebugIsEnabled() ? "TRUE" : "FALSE");
	printf("text_dump = %s\n", bTextDump ? "TRUE" : "FALSE");
	printf("json_dump = %s\n", bJsonDump ? "TRUE" : "FALSE");
	printf("text_report_ok = %s\n", bTextHasHeader ? "TRUE" : "FALSE");
	printf("json_report_ok = %s\n", bJsonHasHeader ? "TRUE" : "FALSE");
	printf("live_block = %s\n", pLive);
	printf("temp_text = %s\n", pTemp);
	printf("text_report_bytes = %u\n", (unsigned)strlen(sTextReport));
	printf("json_report_bytes = %u\n", (unsigned)strlen(sJsonReport));

cleanup:
	if ( pLive != NULL ) {
		xrtFree(pLive);
	}
	if ( sTextReport != NULL ) {
		xrtFree(sTextReport);
	}
	if ( sJsonReport != NULL ) {
		xrtFree(sJsonReport);
	}
	if ( sTextPath != NULL ) {
		(void)xrtFileDelete(sTextPath);
		xrtFree(sTextPath);
	}
	if ( sJsonPath != NULL ) {
		(void)xrtFileDelete(sJsonPath);
		xrtFree(sJsonPath);
	}
	xrtMemDebugEnable(FALSE);
	xrtUnit();
	return iRet;
}
