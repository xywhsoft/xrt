#include <stdio.h>
#include <xruntime.h>



/* 统计 Value 所有权图直接持有的运行时对象引用。 */
static bool countObject(xrtobject* pObject, ptr pContext)
{
	size_t* pCount = (size_t*)pContext;

	if ( pObject == NULL ) {
		return false;
	}
	(*pCount)++;
	return true;
}



/* 构造共享 Value 外壳并枚举其唯一对象所有权槽。 */
int main(void)
{
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("example.runtime.ValueTrace")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("ValueTraceExample"),
		.AbiName = XRT_STR_INIT("example.runtime.ValueTrace"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = sizeof(int64)
	};
	xrtobject* pObject = xrtObjectCreate(&Type);
	xvalue* pItem = xrtValueRuntimeObject(pObject);
	xvalue* pRoot = xrtValueArray();
	size_t iCount = 0u;
	int iResult = 1;

	if ( (pObject != NULL) && (pItem != NULL) && (pRoot != NULL) &&
		 xrtValueArrayAppend(pRoot, pItem) &&
		 xrtValueArrayAppend(pRoot, pItem) &&
		 xrtValueTraceRuntimeObjects(pRoot, countObject, &iCount) ) {
		printf("object_edges=%zu\n", iCount);
		iResult = iCount == 1u ? 0 : 1;
	}
	xrtValueRelease(pRoot);
	xrtValueRelease(pItem);
	xrtObjectUnref(pObject);
	return iResult;
}
