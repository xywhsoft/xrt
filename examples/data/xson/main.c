#include <inttypes.h>
#include <stdio.h>

#include <xrt.h>



/* 直接访问扩展事件，不构造中间 DOM。 */
static xxsonvisitaction printXsonEvent(
	const xxsonevent* pEvent,
	ptr pUserData
)
{
	(void)pUserData;
	if ( pEvent->Type == XXSON_EVENT_BYTES ) {
		printf("bytes = %u\n", (unsigned)pEvent->Value.Bytes.Size);
	} else if ( pEvent->Type == XXSON_EVENT_TIME ) {
		printf("time = %" PRId64 "\n", (int64)pEvent->Value.Time);
	}
	return XXSON_VISIT_NEXT;
}



/* 演示完整类型 DOM 往返、事件访问和无需 DOM 的直接构建。 */
int main(void)
{
	static const char sInput[] =
		"{\"blob\":bytes(\"AAEC/w==\"),"
		"\"updated\":time(\"2026-07-31T08:00:00+08:00\"),"
		"\"roles\":set[\"reader\",\"writer\"],"
		"\"ports\":intmap{80:\"http\",443:\"https\"}}";
	xxsonreadconfig ReadConfig;
	xxsonwriteconfig WriteConfig;
	xxsonwriter* pWriter;
	xvalue* pRoot;
	str sText;
	size_t iSize;

	pRoot = xrtXsonParse((xstrview){ sInput, sizeof(sInput) - 1u });
	if ( pRoot == NULL ) {
		return 1;
	}
	sText = xrtXsonStringify(pRoot, true, &iSize);
	xrtValueRelease(pRoot);
	if ( sText == NULL ) {
		return 2;
	}
	printf("%.*s\n", (int)iSize, sText);
	xrtFree(sText);

	xrtXsonReadConfigInit(&ReadConfig);
	if (
		xrtXsonVisit(
			(xstrview){ sInput, sizeof(sInput) - 1u },
			&ReadConfig,
			printXsonEvent,
			NULL
		) != XXSON_VISIT_DONE
	) {
		return 3;
	}

	xrtXsonWriteConfigInit(&WriteConfig);
	pWriter = xrtXsonWriterCreate(&WriteConfig);
	if (
		(pWriter == NULL) ||
		!xrtXsonWriterObject(pWriter) ||
		!xrtXsonWriterName(pWriter, XRT_STR_LITERAL("code")) ||
		!xrtXsonWriterInt(pWriter, 200) ||
		!xrtXsonWriterName(pWriter, XRT_STR_LITERAL("tags")) ||
		!xrtXsonWriterSet(pWriter) ||
		!xrtXsonWriterString(pWriter, XRT_STR_LITERAL("xrt")) ||
		!xrtXsonWriterEnd(pWriter) ||
		!xrtXsonWriterEnd(pWriter) ||
		!xrtXsonWriterFinish(pWriter)
	) {
		xrtXsonWriterFree(pWriter);
		return 4;
	}
	sText = xrtXsonWriterTake(pWriter, &iSize);
	xrtXsonWriterFree(pWriter);
	if ( sText == NULL ) {
		return 5;
	}
	printf("%.*s\n", (int)iSize, sText);
	xrtFree(sText);
	return 0;
}
