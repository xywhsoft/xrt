/*
 * 范例：data/json_tour —— JSON 读写/文件/写入器补集
 * ----------------------------------------------------------------
 * 演示 API：
 *   【读取】    xrtJsonRead（带配置）/ Valid（零 DOM 校验）/
 *              ParseFile / ReadFile（文件双形态）
 *   【文件输出】 xrtJsonWriteFile / StringifyFile（pretty 开关）
 *   【流式写出】 xrtJsonWrite（输出回调）/
 *              JsonQuoteWrite（字符串引号转义写出）
 *   【写入器】  xrtJsonWriterCreateSink（增量回调形态）/
 *              WriterArray / WriterNull / WriterBool /
 *              WriterUInt / WriterFloat / WriterValue(嵌套 DOM)
 *   【错误】    xrtJsonErrorLocation（行/列定位）
 * 模块宏：XRT_MODULE_JSON
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/data/json_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   json: read+valid ok file-parse/read ok
 *   json: write-file/stringify-file(pretty) ok
 *   json: stream write=23 quote=9
 *   json: sink-writer [[1,true,null,2.5,"v"]] ok
 *   json: error-location line=1 ok
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

/* 收集回调：拼接全部分片。 */
typedef struct examplesink {
	char Buffer[128];
	size_t Size;
} examplesink;

static bool exampleCollect(xbytesview Data, ptr pUserData)
{
	examplesink* pSink = (examplesink*)pUserData;

	if ( (pSink->Size + Data.Size) < sizeof(pSink->Buffer) ) {
		memcpy(pSink->Buffer + pSink->Size, Data.Data,
			Data.Size);
	}
	pSink->Size = pSink->Size + Data.Size;
	return true;
}

int main(void)
{
	static const char sFile[] = "xrt-json-tour.json";
	static const char sText[] = "{\"a\":1,\"b\":[true,null]}";
	xjsonreadconfig ReadConfig;
	xjsonwriteconfig WriteConfig;
	xvalue* pDom = NULL;
	xvalue* pFileDom = NULL;
	xvalue* pReadDom = NULL;
	xvalue* pInner = NULL;
	xjsonwriter* pWriter = NULL;
	xerror* pError = NULL;
	xjsonlocation Location;
	examplesink Sink;
	size_t iWritten = 0;
	FILE* pOut;
	int iResult = 1;

	/* ---- Read（配置形态）+ Valid 正反 ---- */
	xrtJsonReadConfigInit(&ReadConfig);
	pDom = xrtJsonRead(SV(sText), &ReadConfig);
	if ( (pDom == NULL) ||
		!xrtJsonValid(SV(sText)) ||
		xrtJsonValid(SV("{\"a\":}")) ||
		!xrtJsonValid(SV("[1,2,3]")) ) {
		goto Cleanup;
	}
	/* ParseFile / ReadFile：先落盘再双形态读回。 */
	pOut = fopen(sFile, "wb");
	if ( (pOut == NULL) ||
		(fwrite(sText, 1u, sizeof(sText) - 1u, pOut) !=
			sizeof(sText) - 1u) ) {
		goto Cleanup;
	}
	fclose(pOut);
	pOut = NULL;
	pFileDom = xrtJsonParseFile(sFile);
	pReadDom = xrtJsonReadFile(sFile, &ReadConfig);
	if ( (pFileDom == NULL) || (pReadDom == NULL) ) {
		goto Cleanup;
	}
	printf("json: read+valid ok file-parse/read ok\n");

	/* ---- WriteFile / StringifyFile ---- */
	xrtJsonWriteConfigInit(&WriteConfig);
	if ( !xrtJsonWriteFile(sFile, pDom, &WriteConfig) ||
		((pInner = xrtJsonParseFile(sFile)) == NULL) ) {
		goto Cleanup;
	}
	xrtValueRelease(pInner);
	pInner = NULL;
	if ( !xrtJsonStringifyFile(sFile, pDom, true) ) {
		goto Cleanup;
	}
	{
		/* pretty 输出多行：读回验证仍是合法 JSON。 */
		pInner = xrtJsonParseFile(sFile);
		if ( pInner == NULL ) {
			goto Cleanup;
		}
		xrtValueRelease(pInner);
		pInner = NULL;
	}
	printf("json: write-file/stringify-file(pretty) ok\n");

	/* ---- Write 流式 + QuoteWrite ---- */
	Sink.Size = 0;
	if ( !xrtJsonWrite(pDom, &WriteConfig, exampleCollect, &Sink) ||
		(Sink.Size != sizeof(sText) - 1u) ||
		(memcmp(Sink.Buffer, sText,
			sizeof(sText) - 1u) != 0) ) {
		goto Cleanup;
	}
	printf("json: stream write=%zu", Sink.Size);
	Sink.Size = 0;
	if ( !xrtJsonQuoteWrite(SV("a\"b\\c"), 0u, exampleCollect,
			&Sink, &iWritten) ||
		(Sink.Size != 9u) ||  /* 带引号转义 "a\"b\\c" 共 9 字节 */
		(memcmp(Sink.Buffer, "\"a\\\"b\\\\c\"", 9u) != 0) ) {
		goto Cleanup;
	}
	printf(" quote=%zu\n", Sink.Size);

	/* ---- Sink 写入器：数组五元素 ---- */
	Sink.Size = 0;
	pWriter = xrtJsonWriterCreateSink(&WriteConfig, exampleCollect,
		&Sink);
	if ( (pWriter == NULL) ||
		!xrtJsonWriterArray(pWriter) ||
		!xrtJsonWriterUInt(pWriter, 1u) ||
		!xrtJsonWriterBool(pWriter, true) ||
		!xrtJsonWriterNull(pWriter) ||
		!xrtJsonWriterFloat(pWriter, 2.5) ) {
		goto Cleanup;
	}
	/* WriterValue：嵌套一个字符串 DOM。 */
	pInner = xrtValueString(SV("v"));
	if ( (pInner == NULL) ||
		!xrtJsonWriterValue(pWriter, pInner) ||
		!xrtJsonWriterEnd(pWriter) ||
		!xrtJsonWriterFinish(pWriter) ) {
		goto Cleanup;
	}
	if ( (Sink.Size != 21u) ||
		(memcmp(Sink.Buffer, "[1,true,null,2.5,\"v\"]",
			21u) != 0) ) {
		goto Cleanup;
	}
	printf("json: sink-writer [%.*s] ok\n",
		(int)(Sink.Size < 30u ? Sink.Size : 30u), Sink.Buffer);

	/* ---- ErrorLocation：截断文本的行列定位 ---- */
	{
		xvalue* pBad = xrtJsonRead(SV("{\"a\":"), &ReadConfig);

		if ( (pBad != NULL) ||
			((pError = xrtTakeError()) == NULL) ||
			!xrtJsonErrorLocation(pError, &Location) ||
			(Location.Line != 1u) ) {
			goto Cleanup;
		}
		xrtErrorFree(pError);
		pError = NULL;
	}
	printf("json: error-location line=%zu ok\n", Location.Line);
	iResult = 0;

Cleanup:
	xrtErrorFree(pError);
	xrtJsonWriterFree(pWriter);
	xrtValueRelease(pInner);
	xrtValueRelease(pReadDom);
	xrtValueRelease(pFileDom);
	xrtValueRelease(pDom);
	if ( pOut != NULL ) {
		fclose(pOut);
	}
	(void)remove(sFile);
	return iResult;
}
