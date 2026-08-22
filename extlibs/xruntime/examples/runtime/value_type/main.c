#include <stdio.h>
#include <xruntime.h>



/* 使用运行时类型统一拥有、COW 复制并深克隆一个动态 Value。 */
int main(void)
{
	const xrttype* pType = xrtTypeValue();
	xvalue* pSource = xrtValueArray();
	xvalue* pCopy = xrtValueNull();
	xvalue* pClone = xrtValueNull();

	if ( (pSource == NULL) ||
		 !xrtValueArrayAppendNew(
			 pSource, xrtValueString(XRT_STR_LITERAL("xlang"))
		 ) ||
		 !xrtTypeCopyValue(pType, &pCopy, &pSource) ||
		 !xrtTypeCloneValue(pType, &pClone, &pSource) ) {
		xrtValueRelease(pSource);
		xrtTypeDropValue(pType, &pCopy);
		xrtTypeDropValue(pType, &pClone);
		return 1;
	}
	printf(
		"copy=%zu clone=%zu\n",
		xrtValueCount(pCopy),
		xrtValueCount(pClone)
	);
	xrtValueRelease(pSource);
	xrtTypeDropValue(pType, &pCopy);
	xrtTypeDropValue(pType, &pClone);
	return 0;
}
