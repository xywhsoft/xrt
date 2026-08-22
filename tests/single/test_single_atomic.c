#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供 32 位、64 位和指针原子操作。 */
int main(void)
{
	xatomic32 tValue32 = XRT_ATOMIC32_INIT(1u);
	xatomic64 tValue64 = XRT_ATOMIC64_INIT(2u);
	xatomicptr tPointer = XRT_ATOMICPTR_INIT(NULL);
	int iValue = 7;

	if ( !xrtAtomicIsLockFree(sizeof(uint32)) ) {
		return 1;
	}
	if ( xrtAtomic32FetchAdd(&tValue32, 2u, XMEMORY_RELAXED) != 1u ) {
		return 2;
	}
	if ( xrtAtomic64Exchange(&tValue64, 4u, XMEMORY_SEQ_CST) != 2u ) {
		return 3;
	}
	xrtAtomicPtrStore(&tPointer, &iValue, XMEMORY_RELEASE);
	if ( xrtAtomicPtrLoad(&tPointer, XMEMORY_ACQUIRE) != &iValue ) {
		return 4;
	}
	return 0;
}
