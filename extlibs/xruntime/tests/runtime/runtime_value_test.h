#ifndef XRT_RUNTIME_VALUE_TEST_H
#define XRT_RUNTIME_VALUE_TEST_H



#define TEST_RUNTIME_VALUE_EXHAUST_LIMIT 1024u



/*
	持有当前线程可取得的全部 xvalue 尺寸类块，直到下一次请求到达失败分配器。
	分级堆可能从既有 span 或线程缓存满足请求，OOM 测试不能假定切换分配器后立即失败。
*/
static size_t testRuntimeValueExhaust(xvalue** pValues, size_t iCapacity)
{
	size_t iCount = 0u;

	while ( iCount < iCapacity ) {
		xvalue* pValue = xrtValueInt((int64)iCount);

		if ( pValue == NULL ) {
			break;
		}
		pValues[iCount++] = pValue;
	}
	return iCount;
}



/* 释放故障注入期间持有的全部 Value 外壳。 */
static void testRuntimeValueReleaseAll(xvalue** pValues, size_t iCount)
{
	while ( iCount != 0u ) {
		xrtValueRelease(pValues[--iCount]);
	}
}



#endif
