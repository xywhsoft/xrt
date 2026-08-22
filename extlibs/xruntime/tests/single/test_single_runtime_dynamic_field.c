#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



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
	xrtDynamicFieldsUnref(pFields);
	return bReady ? 0 : 1;
}
