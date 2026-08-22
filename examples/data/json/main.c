#include <stdio.h>

#include <xrt.h>



/* 直接访问 JSON 事件而不构造中间 DOM。 */
static xjsonvisitaction printJsonEvent(
	const xjsonevent* pEvent,
	ptr pUserData
)
{
	(void)pUserData;
	if ( pEvent->HasName ) {
		printf("member: %.*s\n", (int)pEvent->Name.Size, pEvent->Name.Data);
	}
	return XJSON_VISIT_NEXT;
}



/* 演示 DOM 往返、事件访问和无需 Value 的增量构建。 */
int main(void)
{
	xjsonreadconfig ReadConfig;
	xjsonwriteconfig WriteConfig;
	xjsonwriter* pWriter;
	xvalue* pRoot;
	xvalue* pName;
	xstrview Name;
	str sText;
	size_t iSize;

	pRoot = xrtJsonParse(XRT_STR_LITERAL(
		"{\"name\":\"xrt\",\"features\":[\"json\",\"http\"]}"
	));
	if ( pRoot == NULL ) {
		return 1;
	}
	pName = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("name"));
	if ( !xrtValueGetString(pName, &Name) ) {
		xrtValueRelease(pRoot);
		return 2;
	}
	printf("name = %.*s\n", (int)Name.Size, Name.Data);

	sText = xrtJsonStringify(pRoot, true, &iSize);
	xrtValueRelease(pRoot);
	if ( sText == NULL ) {
		return 3;
	}
	printf("%.*s\n", (int)iSize, sText);
	xrtFree(sText);

	xrtJsonReadConfigInit(&ReadConfig);
	if (
		xrtJsonVisit(
			XRT_STR_LITERAL("{\"code\":200,\"ok\":true}"),
			&ReadConfig,
			printJsonEvent,
			NULL
		) != XJSON_VISIT_DONE
	) {
		return 4;
	}

	xrtJsonWriteConfigInit(&WriteConfig);
	pWriter = xrtJsonWriterCreate(&WriteConfig);
	if (
		(pWriter == NULL) ||
		!xrtJsonWriterObject(pWriter) ||
		!xrtJsonWriterName(pWriter, XRT_STR_LITERAL("code")) ||
		!xrtJsonWriterInt(pWriter, 200) ||
		!xrtJsonWriterName(pWriter, XRT_STR_LITERAL("message")) ||
		!xrtJsonWriterString(pWriter, XRT_STR_LITERAL("OK")) ||
		!xrtJsonWriterEnd(pWriter) ||
		!xrtJsonWriterFinish(pWriter)
	) {
		xrtJsonWriterFree(pWriter);
		return 5;
	}
	sText = xrtJsonWriterTake(pWriter, &iSize);
	xrtJsonWriterFree(pWriter);
	if ( sText == NULL ) {
		return 6;
	}
	printf("%.*s\n", (int)iSize, sText);
	xrtFree(sText);
	return 0;
}
