/*
 * 范例：data/jsonl —— JSON Lines：逐行解析、空行策略、全局错误位置及原子文件读写
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtJsonlParse / xrtJsonlStringify   Array 与 JSONL 文本互转
 *   xrtJsonlRead / xrtJsonlValid        按配置读取与逐行语法校验
 *   xrtJsonlWrite / xrtJsonlWriteFile   回调分块写出与原子文件替换
 *   xrtJsonlErrorLocation               错误的全局行号与记录下标
 * 模块宏：XRT_MODULE_JSONL（READ+WRITE+FILE）
 * 预期输出：
 *   JSONL: 3 records, 20 bytes
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
	xjsonlreadconfig Read;
	xjsonlwriteconfig Write;
	xjsonllocation Location;
	xvalue* pArray = NULL;
	xvalue* pRead = NULL;
	str Text = NULL;
	size_t Size = 0;
	cstr Path = ".xrt-jsonl-example.jsonl";
	bool Created = false;
	int Result = 1;

	xrtJsonlReadConfigInit(&Read);
	xrtJsonlWriteConfigInit(&Write);
	pArray = xrtJsonlParse(XRT_STR_LITERAL("{\"id\":1}\n\n[2,3]\r\nnull\n"));
	if ( pArray == NULL ) goto done;
	Text = xrtJsonlStringify(pArray, &Size);
	if ( Text == NULL ) goto done;
	if ( !xrtJsonlValid((xstrview){ Text, Size }) ) goto done;
	pRead = xrtJsonlRead((xstrview){ Text, Size }, &Read);
	if ( pRead == NULL ) goto done;
	printf("JSONL: %zu records, %zu bytes\n", xrtValueCount(pRead), Size);
	xrtValueRelease(pRead);
	pRead = NULL;
	if ( !xrtJsonlWrite(pArray, &Write, discard, NULL) ) goto done;
	if ( !xrtJsonlStringifyFile(Path, pArray) ) goto done;
	Created = true;
	pRead = xrtJsonlParseFile(Path);
	if ( pRead == NULL ) goto done;
	xrtValueRelease(pRead);
	pRead = NULL;
	if ( !xrtJsonlWriteFile(Path, pArray, &Write) ) goto done;
	pRead = xrtJsonlReadFile(Path, &Read);
	if ( pRead == NULL ) goto done;
	xrtValueRelease(pRead);
	pRead = NULL;
	Read.Flags |= XJSONL_READ_REJECT_EMPTY_LINES;
	pRead = xrtJsonlRead(XRT_STR_LITERAL("{}\n\n"), &Read);
	if ( pRead != NULL ) goto done;
	if ( !xrtJsonlErrorLocation(xrtGetError(), &Location) ) goto done;
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

