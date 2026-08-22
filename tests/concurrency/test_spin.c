#include "../test.h"



/* 验证静态、栈上和动态自旋锁的完整生命周期。 */
int main(void)
{
	static xspinlock StaticSpin = XRT_SPIN_INIT;
	xspinlock Spin;
	xspinlock* pDynamic;

	testRequire(xrtSpinTryLock(&StaticSpin), "static spin try-lock failed");
	testRequire(!xrtSpinTryLock(&StaticSpin), "busy spin try-lock should fail");
	testRequire(xrtSpinUnlock(&StaticSpin), "static spin unlock failed");
	testRequire(xrtSpinUnit(&StaticSpin), "static spin unit failed");

	testRequire(xrtSpinInit(&Spin), "spin init failed");
	testRequire(xrtSpinLock(&Spin), "spin lock failed");
	testRequire(!xrtSpinUnit(&Spin), "held spin unit should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "held spin unit error mismatch");
	xrtClearError();
	testRequire(xrtSpinUnlock(&Spin), "spin unlock failed");
	testRequire(!xrtSpinUnlock(&Spin), "unlocked spin should reject unlock");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "unlocked spin error mismatch");
	xrtClearError();
	testRequire(xrtSpinUnit(&Spin), "spin unit failed");

	pDynamic = xrtSpinCreate();
	testRequire(pDynamic != NULL, "spin create failed");
	testRequire(xrtSpinDestroy(pDynamic), "spin destroy failed");
	printf("[PASS] spin\n");
	return 0;
}
