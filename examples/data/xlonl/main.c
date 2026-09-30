/*
 * 范例：data/xlonl —— XLON Lines：全类型逐行往返、空行策略、全局错误位置及原子文件读写
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtXlonlParse / xrtXlonlStringify   Array 与 XLONL 文本互转（扩展类型原样）
 *   xrtXlonlRead / xrtXlonlValid        按配置读取与逐行语法校验
 *   xrtXlonlWrite / xrtXlonlWriteFile   回调分块写出与原子文件替换
 *   xrtXlonlErrorLocation               错误的全局行号与记录下标
 * 模块宏：XRT_MODULE_XLONL（READ+WRITE+FILE）
 * 预期输出：
 *   XLONL: 3 records, 20 bytes
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
	xxlonlreadconfig Read;
	xxlonlwriteconfig Write;
	xxlonllocation Location;
	xvalue* pArray = NULL;
	xvalue* pRead = NULL;
	str Text = NULL;
	size_t Size = 0;
	cstr Path = ".xrt-xlonl-example.xlonl";
	bool Created = false;
	int Result = 1;

	xrtXlonlReadConfigInit(&Read);
	xrtXlonlWriteConfigInit(&Write);
	pArray = xrtXlonlParse(XRT_STR_LITERAL("{\"id\":1}\n\n[2,3]\r\nnull\n"));
	if ( pArray == NULL ) goto done;
	Text = xrtXlonlStringify(pArray, &Size);
	if ( Text == NULL ) goto done;
	if ( !xrtXlonlValid((xstrview){ Text, Size }) ) goto done;
	pRead = xrtXlonlRead((xstrview){ Text, Size }, &Read);
	if ( pRead == NULL ) goto done;
	printf("XLONL: %zu records, %zu bytes\n", xrtValueCount(pRead), Size);
	xrtValueRelease(pRead);
	pRead = NULL;
	if ( !xrtXlonlWrite(pArray, &Write, discard, NULL) ) goto done;
	if ( !xrtXlonlStringifyFile(Path, pArray) ) goto done;
	Created = true;
	pRead = xrtXlonlParseFile(Path);
	if ( pRead == NULL ) goto done;
	xrtValueRelease(pRead);
	pRead = NULL;
	if ( !xrtXlonlWriteFile(Path, pArray, &Write) ) goto done;
	pRead = xrtXlonlReadFile(Path, &Read);
	if ( pRead == NULL ) goto done;
	xrtValueRelease(pRead);
	pRead = NULL;
	Read.Flags |= XXLONL_READ_REJECT_EMPTY_LINES;
	pRead = xrtXlonlRead(XRT_STR_LITERAL("{}\n\n"), &Read);
	if ( pRead != NULL ) goto done;
	if ( !xrtXlonlErrorLocation(xrtGetError(), &Location) ) goto done;
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
