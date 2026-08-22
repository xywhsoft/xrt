#include <stdio.h>

#include <xrt.h>



/* 示例对象直接复用核心引用计数原语。 */
typedef struct example_object {
	volatile int32 RefCount;
	int Value;
} example_object;



/* 创建一个初始引用计数为一的对象。 */
static example_object* exampleCreate(int iValue)
{
	example_object* pObject = (example_object*)xrtMalloc(sizeof(example_object));

	if ( pObject != NULL ) {
		pObject->RefCount = 1;
		pObject->Value = iValue;
	}
	return pObject;
}



/* 增加对象引用并返回同一指针。 */
static example_object* exampleRef(example_object* pObject)
{
	if ( (pObject == NULL) || (xrtRefRetain(&pObject->RefCount) < 0) ) {
		return NULL;
	}
	return pObject;
}



/* 释放对象引用，最后一个引用负责析构。 */
static void exampleFree(example_object* pObject)
{
	if ( (pObject != NULL) && (xrtRefRelease(&pObject->RefCount) == 0) ) {
		xrtFree(pObject);
	}
}



/* 验证两个持有者共享同一个对象。 */
int main(void)
{
	example_object* pFirst = exampleCreate(42);
	example_object* pSecond = exampleRef(pFirst);

	if ( pSecond == NULL ) {
		exampleFree(pFirst);
		return 1;
	}
	printf("value: %d\n", pSecond->Value);
	exampleFree(pSecond);
	exampleFree(pFirst);
	return 0;
}
