/*
 * 范例：data/xson_tour —— 校验/文件双形态/错误定位与增量写入器
 * ----------------------------------------------------------------
 * 演示 API：
 *   【解析】      xrtXsonValid（零 DOM 校验）
 *                 xrtXsonRead（带配置）/ ReadFile / ParseFile
 *   【错误定位】  xrtXsonErrorLocation（行/列/偏移）
 *   【文件输出】  xrtXsonWriteFile / StringifyFile（pretty 开关）
 *   【同步写出】  xrtXsonWrite（回调形态）
 *   【增量写入器】xrtXsonWriterCreateSink（回调直连）
 *                 Writer Array/IntMap/Key + 全值域
 *                 Null/Bool/UInt/Float/Bytes/Time/Tag/Value
 * 模块宏：XRT_MODULE_XSON
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/data/xson_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   xson: valid +/- and read(config) ok
 *   xson: error location line=1 ok
 *   xson: file roundtrip parse/read -> write/stringify ok
 *   xson: sync write via callback ok
 *   xson: sink writer full value domain ok
 *
 * XSON 是 JSON 的超集：bytes/time/tag 与 set/intmap 容器；
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
	static const char* sFile = "xson_tour_tmp.xson";
	static const char* sBad = "{\"a\":}";
	xxsonreadconfig ReadConfig;
	xxsonwriteconfig WriteConfig;
	xxsonlocation Location;
	xvalue* pDom = NULL;
	xvalue* pFileDom = NULL;
	xvalue* pSub = NULL;
	xxsonwriter* pWriter = NULL;
	examplesink Sink;
	str sText = NULL;
	FILE* pOut = NULL;
	int iResult = 1;

	/* ---- Valid 正反 + Read 带配置。 ---- */
	xrtXsonReadConfigInit(&ReadConfig);
	if ( xrtXsonValid((xstrview) { sBad, 6u }) ||
		!xrtXsonValid(XRT_STR_LITERAL("[1,2,3]")) ||
		((pDom = xrtXsonRead(XRT_STR_LITERAL(
			"{\"n\":7}"), &ReadConfig)) == NULL) ) {
		goto Cleanup;
	}
	printf("xson: valid +/- and read(config) ok\n");

	/* ---- 错误定位：坏文档的 xrt.xson 错误带文本位置。 ---- */
	{
		const xerror* pError;

		if ( (xrtXsonRead((xstrview) { sBad, 6u },
				&ReadConfig) != NULL) ) {
			goto Cleanup;
		}
		pError = xrtGetError();
		if ( (pError == NULL) ||
			!xrtXsonErrorLocation(pError, &Location) ||
			(Location.Line != 1u) ||
			(Location.Column == 0u) ) {
			goto Cleanup;
		}
	}
	printf("xson: error location line=1 ok\n");

	/* ---- 文件双形态：手写落盘 → ParseFile/ReadFile 读回。 ---- */
	pOut = fopen(sFile, "wb");
	if ( (pOut == NULL) ||
		(fwrite("{\"f\":1.5}", 1u, 9u, pOut) != 9u) ) {
		goto Cleanup;
	}
	fclose(pOut);
	pOut = NULL;
	pFileDom = xrtXsonParseFile(sFile);
	if ( (pFileDom == NULL) ||
		(xrtXsonReadFile(sFile, &ReadConfig) == NULL) ) {
		goto Cleanup;
	}
	/* WriteFile/StringifyFile：原子替换后可再读回。 */
	xrtXsonWriteConfigInit(&WriteConfig);
	if ( !xrtXsonWriteFile(sFile, pDom, &WriteConfig) ||
		!xrtXsonStringifyFile(sFile, pDom, true) ||
		(xrtXsonParseFile(sFile) == NULL) ) {
		goto Cleanup;
	}
	printf("xson: file roundtrip parse/read -> write/stringify ok\n");

	/* ---- 同步写出：Write 把 Value 树提交给回调。 ---- */
	memset(&Sink, 0, sizeof(Sink));
	Sink.bOk = true;
	if ( !xrtXsonWrite(pDom, &WriteConfig, exampleSink,
			(ptr)&Sink) ||
		(Sink.iBytes != 7u) ) { /* {"n":7} 恰七字节。 */
		goto Cleanup;
	}
	printf("xson: sync write via callback ok\n");

	/* ---- 增量写入器：CreateSink + 全值域方法。 ---- */
	memset(&Sink, 0, sizeof(Sink));
	Sink.bOk = true;
	pWriter = xrtXsonWriterCreateSink(&WriteConfig, exampleSink,
		(ptr)&Sink);
	pSub = xrtXsonParse(XRT_STR_LITERAL("[true]"));
	if ( (pWriter == NULL) || (pSub == NULL) ||
		/* 顶层数组：混排七种值域 + 子树 + 标签。 */
		!xrtXsonWriterArray(pWriter) ||
		!xrtXsonWriterNull(pWriter) ||
		!xrtXsonWriterBool(pWriter, true) ||
		!xrtXsonWriterUInt(pWriter, 12u) ||
		!xrtXsonWriterFloat(pWriter, 0.5) ||
		!xrtXsonWriterBytes(pWriter,
			(xbytesview) { (const uint8*)"xy", 2u }) ||
		!xrtXsonWriterTime(pWriter, (xtime)1000000) ||
		!xrtXsonWriterTag(pWriter, XRT_STR_LITERAL("base64"),
			XRT_STR_LITERAL("eHl6")) ||
		!xrtXsonWriterValue(pWriter, pSub) ||
		!xrtXsonWriterEnd(pWriter) ||
		!xrtXsonWriterFinish(pWriter) ) {
		goto Cleanup;
	}
	/* 嵌套 intmap：整数键映射容器。 */
	pWriter = (xrtXsonWriterFree(pWriter), NULL);
	pWriter = xrtXsonWriterCreateSink(&WriteConfig, exampleSink,
		(ptr)&Sink);
	if ( (pWriter == NULL) ||
		!xrtXsonWriterIntMap(pWriter) ||
		!xrtXsonWriterKey(pWriter, 7) ||
		!xrtXsonWriterBool(pWriter, false) ||
		!xrtXsonWriterEnd(pWriter) ||
		!xrtXsonWriterFinish(pWriter) ||
		(Sink.iBytes < 20u) ) {
		goto Cleanup;
	}
	printf("xson: sink writer full value domain ok\n");
	iResult = 0;

Cleanup:
	if ( pOut != NULL ) {
		fclose(pOut);
	}
	xrtXsonWriterFree(pWriter);
	xrtValueRelease(pSub);
	xrtValueRelease(pFileDom);
	xrtValueRelease(pDom);
	xrtFree(sText);
	(void)remove(sFile);
	return iResult;
}
