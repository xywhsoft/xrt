#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



/* 统计单头文件 Value 图追踪到的对象所有权边。 */
static bool testSingleRuntimeValueTraceVisit(
	xrtobject* pObject,
	ptr pContext
)
{
	size_t* pCount = (size_t*)pContext;

	if ( pObject == NULL ) {
		return false;
	}
	(*pCount)++;
	return true;
}



/* 验证单头文件保留 Value 对象追踪和共享外壳去重契约。 */
int main(void)
{
	xrttype Type = {
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("SingleValueTrace"),
		.AbiName = XRT_STR_INIT("tests.single.ValueTrace"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = sizeof(int64)
	};
	xrtobject* pObject;
	xvalue* pItem = NULL;
	xvalue* pRoot = NULL;
	size_t iCount = 0u;
	int iResult = 0;

	/* 类型身份始终从 ABI 名称计算，避免测试固化哈希实现细节。 */
	Type.Id = xrtTypeId(Type.AbiName);
	pObject = xrtObjectCreate(&Type);

	/* 分阶段验证依赖闭包，失败码直接指出单头能力缺口。 */
	if ( pObject == NULL ) {
		return 1;
	}
	pItem = xrtValueRuntimeObject(pObject);
	if ( pItem == NULL ) {
		iResult = 2;
		goto Cleanup;
	}
	pRoot = xrtValueArray();
	if ( pRoot == NULL ) {
		iResult = 3;
		goto Cleanup;
	}
	if ( !xrtValueArrayAppend(pRoot, pItem) ||
		 !xrtValueArrayAppend(pRoot, pItem) ) {
		iResult = 4;
		goto Cleanup;
	}
	if ( !xrtValueTraceRuntimeObjects(
		pRoot, testSingleRuntimeValueTraceVisit, &iCount
	) ) {
		iResult = 5;
		goto Cleanup;
	}
	if ( iCount != 1u ) {
		iResult = 6;
	}

	/* 所有部分构造状态都可安全进入统一清理路径。 */
Cleanup:
	xrtValueRelease(pRoot);
	xrtValueRelease(pItem);
	xrtObjectUnref(pObject);
	return iResult;
}
