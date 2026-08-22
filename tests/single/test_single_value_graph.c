#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供值图深拷贝和结构相等。 */
int main(void)
{
	xvalue* pRoot = xrtValueArray();
	xvalue* pChild = xrtValueObject();
	xvalue* pCopy;

	if ( (pRoot == NULL) || (pChild == NULL) ||
		 !xrtValueObjectSetNew(
			pChild,
			XRT_STR_LITERAL("id"),
			xrtValueInt(1)
		 ) ||
		 !xrtValueArrayAppend(pRoot, pChild) ||
		 !xrtValueArrayAppend(pRoot, pChild) ) {
		xrtValueRelease(pChild);
		xrtValueRelease(pRoot);
		return 1;
	}
	pCopy = xrtValueDeepClone(pRoot);
	if ( (pCopy == NULL) ||
		 !xrtValueEqual(pRoot, pCopy) ||
		 (xrtValueArrayGet(pCopy, 0) !=
		  xrtValueArrayGet(pCopy, 1)) ) {
		xrtValueRelease(pCopy);
		xrtValueRelease(pChild);
		xrtValueRelease(pRoot);
		return 2;
	}
	xrtValueRelease(pCopy);
	xrtValueRelease(pChild);
	xrtValueRelease(pRoot);
	return 0;
}
