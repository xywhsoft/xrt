/* JSONL：Array 往返、空行策略、全局错误位置及原子文件读写。 */
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

