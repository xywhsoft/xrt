#define XRUNTIME_IMPLEMENTATION
#include "../../include/xruntime/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xruntime.h"



int main(void)
{
	xrtweak Weak = { 0 };
	xvalue* pValue = xrtValueWeakTake(&Weak);
	int iResult = (pValue == NULL) || !xrtValueIsWeak(pValue) ||
		!xrtValueWeakExpired(pValue);

	xrtValueRelease(pValue);
	return iResult;
}
