/*
 * 范例：core/atomic_tour —— 原子操作 RMW/栅栏补集
 * ----------------------------------------------------------------
 * 演示 API：
 *   【RMW 补集】  xrtAtomic32Init / Load / Store / Exchange /
 *                FetchAdd / FetchSub / FetchAnd / FetchOr /
 *                FetchXor / CompareExchange（强语义失败回写）
 *   【64 位】    xrtAtomic64Init / Load / Store / Exchange /
 *                FetchSub / FetchAnd / FetchOr / FetchXor
 *   【指针】    xrtAtomicPtrInit / Load / Store / Exchange /
 *                PtrCompareExchange
 *   【杂项】    xrtAtomicIsLockFree / ThreadFence / SignalFence /
 *              Pause
 * 模块宏：XRT_MODULE_ATOMIC
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/core/atomic_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   atomic: 32-bit exchange=0F and/or/xor = E/C/5
 *   atomic: cas ok=1 fail-rewrites=A
 *   atomic: 64-bit init/exchange/sub/and/or/xor = 7
 *   atomic: ptr exchange/cas = ok
 *   atomic: lockfree(4/8)=1 fence+pause ok
 */

#include <stdio.h>
#include <stdint.h>
#include <xrt.h>

int main(void)
{
	xatomic32 A32;
	xatomic64 A64;
	xatomicptr APtr;
	uint32 iOld32 = 0;
	uint32 iOldAnd = 0;
	uint32 iOldOr = 0;
	uint32 iExpected = 0;
	uint64 iOld64 = 0;
	ptr pOld = NULL;
	ptr pExpected = NULL;
	static uint8 s_Target[2];
	int iResult = 1;

	/* ---- 32 位：Exchange / And / Or / Xor ---- */
	xrtAtomic32Init(&A32, 0x0Fu);
	iOld32 = xrtAtomic32Exchange(&A32, 0x3Cu, XMEMORY_ACQ_REL);
	if ( (iOld32 != 0x0Fu) ||
		(xrtAtomic32Load(&A32, XMEMORY_ACQUIRE) != 0x3Cu) ) {
		goto Cleanup;
	}
	iOldAnd = xrtAtomic32FetchAnd(&A32, 0x0Fu, XMEMORY_ACQ_REL);
	if ( iOldAnd != 0x3Cu ) {
		goto Cleanup;
	}
	if ( xrtAtomic32Load(&A32, XMEMORY_ACQUIRE) != 0x0Cu ) {
		goto Cleanup;  /* 0x3C & 0x0F = 0x0C */
	}
	iOldOr = xrtAtomic32FetchOr(&A32, 0x06u, XMEMORY_ACQ_REL);
	if ( iOldOr != 0x0Cu ) {
		goto Cleanup;
	}
	if ( xrtAtomic32Load(&A32, XMEMORY_ACQUIRE) != 0x0Eu ) {
		goto Cleanup;  /* 0x0C | 0x06 = 0x0E */
	}
	iOld32 = xrtAtomic32FetchXor(&A32, 0x0Bu, XMEMORY_ACQ_REL);
	if ( (iOld32 != 0x0Eu) ||
		(xrtAtomic32Load(&A32, XMEMORY_ACQUIRE) != 0x05u) ) {
		goto Cleanup;  /* 0x0E ^ 0x0B = 0x05 */
	}
	printf("atomic: 32-bit exchange=0F and/or/xor = %X/%X/%X\n",
		(unsigned)iOld32 & 0xFu,
		0xCu, 0x5u);

	/* ---- 32 位 CAS：成功路径 + 强语义失败回写 ---- */
	iExpected = 0x05u;
	if ( !xrtAtomic32CompareExchange(&A32, &iExpected, 0x0Au,
			XMEMORY_ACQ_REL, XMEMORY_ACQUIRE) ||
		(xrtAtomic32Load(&A32, XMEMORY_ACQUIRE) != 0x0Au) ) {
		goto Cleanup;
	}
	iExpected = 0x99u;  /* 期望错误：失败并把实际值 0x0A 写回 */
	if ( xrtAtomic32CompareExchange(&A32, &iExpected, 0x0Bu,
			XMEMORY_ACQ_REL, XMEMORY_ACQUIRE) ||
		(iExpected != 0x0Au) ) {
		goto Cleanup;
	}
	printf("atomic: cas ok=1 fail-rewrites=%X\n",
		(unsigned)iExpected);

	/* ---- 32 位：Store / FetchAdd / FetchSub（加减按模回绕） ---- */
	xrtAtomic32Store(&A32, 0x33u, XMEMORY_RELEASE);
	iOld32 = xrtAtomic32FetchAdd(&A32, 0x0Cu, XMEMORY_ACQ_REL);
	if ( (iOld32 != 0x33u) ||
		(xrtAtomic32Load(&A32, XMEMORY_ACQUIRE) != 0x3Fu) ) {
		goto Cleanup;
	}
	iOld32 = xrtAtomic32FetchSub(&A32, 0x0Fu, XMEMORY_ACQ_REL);
	if ( (iOld32 != 0x3Fu) ||
		(xrtAtomic32Load(&A32, XMEMORY_ACQUIRE) != 0x30u) ) {
		goto Cleanup;
	}

	/* ---- 64 位：Init / Exchange / Sub / And / Or / Xor ---- */
	xrtAtomic64Init(&A64, UINT64_C(100));
	iOld64 = xrtAtomic64Exchange(&A64, UINT64_C(50),
		XMEMORY_ACQ_REL);
	if ( iOld64 != UINT64_C(100) ) {
		goto Cleanup;
	}
	iOld64 = xrtAtomic64FetchSub(&A64, UINT64_C(20),
		XMEMORY_ACQ_REL);
	if ( (iOld64 != UINT64_C(50)) ||
		(xrtAtomic64Load(&A64, XMEMORY_ACQUIRE) != UINT64_C(30)) ) {
		goto Cleanup;
	}
	iOld64 = xrtAtomic64FetchAnd(&A64, UINT64_C(0x1C),
		XMEMORY_ACQ_REL);
	if ( (iOld64 != UINT64_C(30)) ||
		(xrtAtomic64Load(&A64, XMEMORY_ACQUIRE) != UINT64_C(28)) ) {
		goto Cleanup;  /* 30 & 28 = 28 */
	}
	iOld64 = xrtAtomic64FetchOr(&A64, UINT64_C(3),
		XMEMORY_ACQ_REL);
	if ( (iOld64 != UINT64_C(28)) ||
		(xrtAtomic64Load(&A64, XMEMORY_ACQUIRE) != UINT64_C(31)) ) {
		goto Cleanup;  /* 28 | 3 = 31 */
	}
	iOld64 = xrtAtomic64FetchXor(&A64, UINT64_C(31),
		XMEMORY_ACQ_REL);
	if ( (iOld64 != UINT64_C(31)) ||
		(xrtAtomic64Load(&A64, XMEMORY_ACQUIRE) != 0u) ) {
		goto Cleanup;  /* 31 ^ 31 = 0 */
	}
	xrtAtomic64Store(&A64, UINT64_C(7), XMEMORY_RELEASE);
	if ( xrtAtomic64Load(&A64, XMEMORY_ACQUIRE) != UINT64_C(7) ) {
		goto Cleanup;
	}
	printf("atomic: 64-bit init/exchange/sub/and/or/xor = %llu\n",
		(unsigned long long)xrtAtomic64Load(&A64,
			XMEMORY_ACQUIRE));

	/* ---- 指针：Init / Exchange / CompareExchange ---- */
	xrtAtomicPtrInit(&APtr, (ptr)&s_Target[0]);
	pOld = xrtAtomicPtrExchange(&APtr, (ptr)&s_Target[1],
		XMEMORY_ACQ_REL);
	if ( (pOld != (ptr)&s_Target[0]) ||
		(xrtAtomicPtrLoad(&APtr, XMEMORY_ACQUIRE) !=
			(ptr)&s_Target[1]) ) {
		goto Cleanup;
	}
	pExpected = (ptr)&s_Target[0];  /* 错误期望：失败回写 */
	if ( xrtAtomicPtrCompareExchange(&APtr, &pExpected,
			(ptr)&s_Target[0], XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE) ||
		(pExpected != (ptr)&s_Target[1]) ) {
		goto Cleanup;
	}
	pExpected = (ptr)&s_Target[1];
	if ( !xrtAtomicPtrCompareExchange(&APtr, &pExpected,
			(ptr)&s_Target[0], XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE) ) {
		goto Cleanup;
	}
	/* Store：CAS 后直接写回并核对。 */
	xrtAtomicPtrStore(&APtr, (ptr)&s_Target[1], XMEMORY_RELEASE);
	if ( xrtAtomicPtrLoad(&APtr, XMEMORY_ACQUIRE) !=
		(ptr)&s_Target[1] ) {
		goto Cleanup;
	}
	printf("atomic: ptr exchange/cas = ok\n");

	/* ---- 杂项：无锁判定 / 栅栏 / Pause ---- */
	if ( !xrtAtomicIsLockFree(sizeof(uint32)) ||
		!xrtAtomicIsLockFree(sizeof(uint64)) ) {
		goto Cleanup;
	}
	xrtAtomicThreadFence(XMEMORY_ACQ_REL);
	xrtAtomicSignalFence(XMEMORY_SEQ_CST);
	xrtAtomicPause();
	printf("atomic: lockfree(4/8)=1 fence+pause ok\n");
	iResult = 0;

Cleanup:
	return iResult;
}
