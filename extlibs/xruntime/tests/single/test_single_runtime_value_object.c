#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



int main(void)
{
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("tests.single.RuntimeValueObject")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("RuntimeValueObject"),
		.AbiName = XRT_STR_INIT("tests.single.RuntimeValueObject"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = sizeof(int64)
	};
	xrtobject* pObject = xrtObjectCreate(&Type);
	xvalue* pValue = xrtValueRuntimeObjectTake(&pObject);
	int iResult = (pValue == NULL) || (pObject != NULL) ||
		!xrtValueIsRuntimeObject(pValue);

	xrtValueRelease(pValue);
	return iResult;
}
