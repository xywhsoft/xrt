#define XRUNTIME_IMPLEMENTATION
#include "../../include/xruntime/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xruntime.h"



/* 验证单头文件中的动态字段所有权和便捷集合路径。 */
int main(void)
{
	xrtdynamicfields* pFields = xrtDynamicFieldsCreate();
	xvalue* pKeys;
	bool bReady = (pFields != NULL) &&
		xrtDynamicFieldsSetNew(
			pFields, XRT_STR_LITERAL("answer"), xrtValueInt(42)
		);

	pKeys = bReady ? xrtDynamicFieldsKeys(pFields) : NULL;
	bReady = bReady && (pKeys != NULL) &&
		(xrtValueCount(pKeys) == 1u);
	xrtValueRelease(pKeys);
	xrtdynamicfields* Source = xrtDynamicFieldsCreate();
	xvalue* Shared = xrtValueArray();
	bReady = bReady && Source && Shared &&
		xrtDynamicFieldsSetRef(Source, XRT_STR_LITERAL("a"), Shared) &&
		xrtDynamicFieldsSetRef(Source, XRT_STR_LITERAL("b"), Shared) &&
		xrtDynamicFieldsMergeRefV1(pFields, Source, true) &&
		xrtDynamicFieldsGet(pFields, XRT_STR_LITERAL("a")) == Shared &&
		xrtDynamicFieldsGet(pFields, XRT_STR_LITERAL("b")) == Shared;
	xrtValueRelease(Shared); xrtDynamicFieldsUnref(Source);
	xrtDynamicFieldsUnref(pFields);
	return bReady ? 0 : 1;
}
