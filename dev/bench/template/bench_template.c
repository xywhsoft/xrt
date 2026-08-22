#include "../bench_common.h"

#define XRT_MODULE_TEMPLATE_CONTROL
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



typedef struct xbenchtemplatewriter {
	uint64 Bytes;
	uint64 Checksum;
} xbenchtemplatewriter;



/* 消费借用分片但不构造结果字符串，用于测量模板流式高性能路径。 */
static bool xbenchTemplateWrite(ptr pUserData, xstrview Text)
{
	xbenchtemplatewriter* pWriter = (xbenchtemplatewriter*)pUserData;

	pWriter->Bytes += (uint64)Text.Size;
	if ( Text.Size != 0u ) {
		pWriter->Checksum += (uint8)Text.Data[0];
	}
	return true;
}



/* 构造一次稳定数据输入，计时阶段只执行模板自身操作。 */
static xvalue* xbenchTemplateData(void)
{
	xvalue* pRoot = xrtValueObject();
	xvalue* pItems = xrtValueArray();

	if ( (pRoot == NULL) || (pItems == NULL) ) {
		xrtValueRelease(pItems);
		xrtValueRelease(pRoot);
		return NULL;
	}
	for ( int64 i = 0; i < 16; i++ ) {
		xvalue* pItem = xrtValueObject();

		if (
			(pItem == NULL) ||
			!xrtValueObjectSetNew(
				pItem,
				XRT_STR_LITERAL("name"),
				xrtValueString((i & 1) != 0
					? XRT_STR_LITERAL("beta")
					: XRT_STR_LITERAL("alpha"))
			) ||
			!xrtValueObjectSetNew(
				pItem,
				XRT_STR_LITERAL("count"),
				xrtValueInt(i * 17)
			) ||
			!xrtValueArrayAppendNew(pItems, pItem)
		) {
			xrtValueRelease(pItem);
			xrtValueRelease(pItems);
			xrtValueRelease(pRoot);
			return NULL;
		}
	}
	if ( !xrtValueObjectSetTake(
		pRoot,
		XRT_STR_LITERAL("items"),
		&pItems
	) ) {
		xrtValueRelease(pItems);
		xrtValueRelease(pRoot);
		return NULL;
	}
	return pRoot;
}



/* 测量编译、流式渲染和分配式便利渲染。 */
int main(int argc, char** argv)
{
	static const char sSource[] =
		"{#if:items}"
		"{#foreach:items}"
		"{?loop.first::,}{$loop.value.name}:{%loop.value.count}"
		"{#end}"
		"{#else}empty{#end}";
	uint32 iCompileCount = xbenchArgU32(argc, argv, 1, 20000u);
	uint32 iWriteCount = xbenchArgU32(argc, argv, 2, 100000u);
	uint32 iRenderCount = xbenchArgU32(argc, argv, 3, 50000u);
	xstrview Source = { sSource, sizeof(sSource) - 1u };
	xvalue* pData = xbenchTemplateData();
	xtemplate* pTemplate;
	xtemplaterenderconfig Config;
	xbenchtemplatewriter Writer = { 0u, 0u };
	xbenchtimer Timer;
	uint64 iCompileElapsed;
	uint64 iWriteElapsed;
	uint64 iRenderElapsed;
	uint64 iChecksum = 0u;

	if (
		(iCompileCount == 0u) ||
		(iWriteCount == 0u) ||
		(iRenderCount == 0u) ||
		(pData == NULL)
	) {
		xrtValueRelease(pData);
		return 1;
	}
	pTemplate = xrtTemplateCompile(Source);
	if ( pTemplate == NULL ) {
		xrtValueRelease(pData);
		return 2;
	}
	xrtTemplateRenderConfigInit(&Config);
	Config.Root = pData;
	Config.Current = pData;

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0u; i < iCompileCount; i++ ) {
		xtemplate* pCompiled = xrtTemplateCompile(Source);

		if ( pCompiled == NULL ) {
			return 3;
		}
		iChecksum += (uint64)xrtTemplateNodeCount(pCompiled);
		xrtTemplateRelease(pCompiled);
	}
	xbenchTimerStop(&Timer);
	iCompileElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0u; i < iWriteCount; i++ ) {
		if ( !xrtTemplateWrite(
			pTemplate,
			&Config,
			xbenchTemplateWrite,
			&Writer
		) ) {
			return 4;
		}
	}
	xbenchTimerStop(&Timer);
	iWriteElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0u; i < iRenderCount; i++ ) {
		size_t iSize;
		str sOutput = xrtTemplateRender(pTemplate, pData, &iSize);

		if ( sOutput == NULL ) {
			return 5;
		}
		iChecksum += (uint64)iSize + (uint8)sOutput[0];
		xrtFree(sOutput);
	}
	xbenchTimerStop(&Timer);
	iRenderElapsed = xbenchTimerElapsedNs(&Timer);

	printf("xrt template benchmark\n");
	xbenchPrintMetricDouble(
		"template_compile_ops_per_sec",
		xbenchSafeRate(iCompileCount, iCompileElapsed)
	);
	xbenchPrintMetricDouble(
		"template_write_ops_per_sec",
		xbenchSafeRate(iWriteCount, iWriteElapsed)
	);
	xbenchPrintMetricDouble(
		"template_render_ops_per_sec",
		xbenchSafeRate(iRenderCount, iRenderElapsed)
	);
	xbenchPrintMetricU64(
		"checksum",
		iChecksum ^ Writer.Bytes ^ Writer.Checksum
	);

	xrtTemplateRelease(pTemplate);
	xrtValueRelease(pData);
	return 0;
}
