/*
 * 范例：data/xsonl —— XSON Lines：全类型逐行往返、空行策略、全局错误位置及原子文件读写
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtXsonlParse / xrtXsonlStringify   Array 与 XSONL 文本互转（扩展类型原样）
 *   xrtXsonlRead / xrtXsonlValid        按配置读取与逐行语法校验
 *   xrtXsonlWrite / xrtXsonlWriteFile   回调分块写出与原子文件替换
 *   xrtXsonlErrorLocation               错误的全局行号与记录下标
 * 模块宏：XRT_MODULE_XSONL（READ+WRITE+FILE）
 * 预期输出：
 *   XSONL: 3 records, 20 bytes
 *   empty line: line=2 record=1
 */
#include <stdio.h>
#include <xrt.h>

static bool discard(xbytesview Data, ptr pUserData)
{
	(void)Data;
	(void)pUserData;
	return true;
}

int main(void)
{
	xxsonlreadconfig Read;
	xxsonlwriteconfig Write;
	xxsonllocation Location;
	xvalue* pArray = NULL;
	xvalue* pRead = NULL;
	str Text = NULL;
	size_t Size = 0;
	cstr Path = ".xrt-xsonl-example.xsonl";
	bool Created = false;
	int Result = 1;

	xrtXsonlReadConfigInit(&Read);
	xrtXsonlWriteConfigInit(&Write);
	pArray = xrtXsonlParse(XRT_STR_LITERAL("{\"id\":1}\n\n[2,3]\r\nnull\n"));
	if ( pArray == NULL ) goto done;
	Text = xrtXsonlStringify(pArray, &Size);
	if ( Text == NULL ) goto done;
	if ( !xrtXsonlValid((xstrview){ Text, Size }) ) goto done;
	pRead = xrtXsonlRead((xstrview){ Text, Size }, &Read);
	if ( pRead == NULL ) goto done;
	printf("XSONL: %zu records, %zu bytes\n", xrtValueCount(pRead), Size);
	xrtValueRelease(pRead);
	pRead = NULL;
	if ( !xrtXsonlWrite(pArray, &Write, discard, NULL) ) goto done;
	if ( !xrtXsonlStringifyFile(Path, pArray) ) goto done;
	Created = true;
	pRead = xrtXsonlParseFile(Path);
	if ( pRead == NULL ) goto done;
	xrtValueRelease(pRead);
	pRead = NULL;
	if ( !xrtXsonlWriteFile(Path, pArray, &Write) ) goto done;
	pRead = xrtXsonlReadFile(Path, &Read);
	if ( pRead == NULL ) goto done;
	xrtValueRelease(pRead);
	pRead = NULL;
	Read.Flags |= XXSONL_READ_REJECT_EMPTY_LINES;
	pRead = xrtXsonlRead(XRT_STR_LITERAL("{}\n\n"), &Read);
	if ( pRead != NULL ) goto done;
	if ( !xrtXsonlErrorLocation(xrtGetError(), &Location) ) goto done;
	printf("empty line: line=%zu record=%zu\n", Location.Line, Location.RecordIndex);
	xrtClearError();
	Result = 0;
done:
	xrtValueRelease(pRead);
	xrtValueRelease(pArray);
	xrtFree(Text);
	if ( Created && !xrtFileDelete(Path) ) Result = 1;
	return Result;
}

