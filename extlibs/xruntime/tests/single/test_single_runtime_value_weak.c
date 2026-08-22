#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



int main(void)
{
	xrtweak Weak = { 0 };
	xvalue* pValue = xrtValueWeakTake(&Weak);
	int iResult = (pValue == NULL) || !xrtValueIsWeak(pValue) ||
		!xrtValueWeakExpired(pValue);

	xrtValueRelease(pValue);
	return iResult;
}
