/*
 * 范例：data/xlon_tour —— 校验/文件双形态/错误定位与增量写入器
 * ----------------------------------------------------------------
 * 演示 API：
 *   【解析】      xrtXlonValid（零 DOM 校验）
 *                 xrtXlonRead（带配置）/ ReadFile / ParseFile
 *   【错误定位】  xrtXlonErrorLocation（行/列/偏移）
 *   【文件输出】  xrtXlonWriteFile / StringifyFile（pretty 开关）
 *   【同步写出】  xrtXlonWrite（回调形态）
 *   【增量写入器】xrtXlonWriterCreateSink（回调直连）
 *                 Writer Array/IntMap/Key + 全值域
 *                 Null/Bool/UInt/Float/Bytes/Time/Tag/Value
 * 模块宏：XRT_MODULE_XLON
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/data/xlon_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   xlon: valid +/- and read(config) ok
 *   xlon: error location line=1 ok
 *   xlon: file roundtrip parse/read -> write/stringify ok
 *   xlon: sync write via callback ok
 *   xlon: sink writer full value domain ok
 *
 * XLON 是 JSON 的超集：bytes/time/tag 与 set/intmap 容器；
 *   写入器方法族与读取事件族一一对应。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

/* 输出回调：累计字节数。 */
typedef struct examplesink {
	size_t iBytes;
	bool bOk;
} examplesink;

static bool exampleSink(xbytesview Data, ptr pUserData)
{
	examplesink* pSink = (examplesink*)pUserData;

	pSink->iBytes += Data.Size;
	return pSink->bOk;
}

int main(void)
{
	static const char* sFile = "xlon_tour_tmp.xlon";
	static const char* sBad = "{\"a\":}";
	xxlonreadconfig ReadConfig;
	xxlonwriteconfig WriteConfig;
	xxlonlocation Location;
	xvalue* pDom = NULL;
	xvalue* pFileDom = NULL;
	xvalue* pSub = NULL;
	xxlonwriter* pWriter = NULL;
	examplesink Sink;
	str sText = NULL;
	FILE* pOut = NULL;
	int iResult = 1;

	/* ---- Valid 正反 + Read 带配置。 ---- */
	xrtXlonReadConfigInit(&ReadConfig);
	if ( xrtXlonValid((xstrview) { sBad, 6u }) ||
		!xrtXlonValid(XRT_STR_LITERAL("[1,2,3]")) ||
		((pDom = xrtXlonRead(XRT_STR_LITERAL(
			"{\"n\":7}"), &ReadConfig)) == NULL) ) {
		goto Cleanup;
	}
	printf("xlon: valid +/- and read(config) ok\n");

	/* ---- 错误定位：坏文档的 xrt.xlon 错误带文本位置。 ---- */
	{
		const xerror* pError;

		if ( (xrtXlonRead((xstrview) { sBad, 6u },
				&ReadConfig) != NULL) ) {
			goto Cleanup;
		}
		pError = xrtGetError();
		if ( (pError == NULL) ||
			!xrtXlonErrorLocation(pError, &Location) ||
			(Location.Line != 1u) ||
			(Location.Column == 0u) ) {
			goto Cleanup;
		}
	}
	printf("xlon: error location line=1 ok\n");

	/* ---- 文件双形态：手写落盘 → ParseFile/ReadFile 读回。 ---- */
	pOut = fopen(sFile, "wb");
	if ( (pOut == NULL) ||
		(fwrite("{\"f\":1.5}", 1u, 9u, pOut) != 9u) ) {
		goto Cleanup;
	}
	fclose(pOut);
	pOut = NULL;
	pFileDom = xrtXlonParseFile(sFile);
	if ( (pFileDom == NULL) ||
		(xrtXlonReadFile(sFile, &ReadConfig) == NULL) ) {
		goto Cleanup;
	}
	/* WriteFile/StringifyFile：原子替换后可再读回。 */
	xrtXlonWriteConfigInit(&WriteConfig);
	if ( !xrtXlonWriteFile(sFile, pDom, &WriteConfig) ||
		!xrtXlonStringifyFile(sFile, pDom, true) ||
		(xrtXlonParseFile(sFile) == NULL) ) {
		goto Cleanup;
	}
	printf("xlon: file roundtrip parse/read -> write/stringify ok\n");

	/* ---- 同步写出：Write 把 Value 树提交给回调。 ---- */
	memset(&Sink, 0, sizeof(Sink));
	Sink.bOk = true;
	if ( !xrtXlonWrite(pDom, &WriteConfig, exampleSink,
			(ptr)&Sink) ||
		(Sink.iBytes != 7u) ) { /* {"n":7} 恰七字节。 */
		goto Cleanup;
	}
	printf("xlon: sync write via callback ok\n");

	/* ---- 增量写入器：CreateSink + 全值域方法。 ---- */
	memset(&Sink, 0, sizeof(Sink));
	Sink.bOk = true;
	pWriter = xrtXlonWriterCreateSink(&WriteConfig, exampleSink,
		(ptr)&Sink);
	pSub = xrtXlonParse(XRT_STR_LITERAL("[true]"));
	if ( (pWriter == NULL) || (pSub == NULL) ||
		/* 顶层数组：混排七种值域 + 子树 + 标签。 */
		!xrtXlonWriterArray(pWriter) ||
		!xrtXlonWriterNull(pWriter) ||
		!xrtXlonWriterBool(pWriter, true) ||
		!xrtXlonWriterUInt(pWriter, 12u) ||
		!xrtXlonWriterFloat(pWriter, 0.5) ||
		!xrtXlonWriterBytes(pWriter,
			(xbytesview) { (const uint8*)"xy", 2u }) ||
		!xrtXlonWriterTime(pWriter, (xtime)1000000) ||
		!xrtXlonWriterTag(pWriter, XRT_STR_LITERAL("base64"),
			XRT_STR_LITERAL("eHl6")) ||
		!xrtXlonWriterValue(pWriter, pSub) ||
		!xrtXlonWriterEnd(pWriter) ||
		!xrtXlonWriterFinish(pWriter) ) {
		goto Cleanup;
	}
	/* 嵌套 intmap：整数键映射容器。 */
	pWriter = (xrtXlonWriterFree(pWriter), NULL);
	pWriter = xrtXlonWriterCreateSink(&WriteConfig, exampleSink,
		(ptr)&Sink);
	if ( (pWriter == NULL) ||
		!xrtXlonWriterIntMap(pWriter) ||
		!xrtXlonWriterKey(pWriter, 7) ||
		!xrtXlonWriterBool(pWriter, false) ||
		!xrtXlonWriterEnd(pWriter) ||
		!xrtXlonWriterFinish(pWriter) ||
		(Sink.iBytes < 20u) ) {
		goto Cleanup;
	}
	printf("xlon: sink writer full value domain ok\n");
	iResult = 0;

Cleanup:
	if ( pOut != NULL ) {
		fclose(pOut);
	}
	xrtXlonWriterFree(pWriter);
	xrtValueRelease(pSub);
	xrtValueRelease(pFileDom);
	xrtValueRelease(pDom);
	xrtFree(sText);
	(void)remove(sFile);
	return iResult;
}
