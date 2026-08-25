#if defined(__aarch64__) && \
	(defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)

#include "../test.h"

#define TEST_CORO_ABI_SWITCH_COUNT 4096u

#define TEST_CORO_STRING_INNER(Value) #Value
#define TEST_CORO_STRING(Value) TEST_CORO_STRING_INNER(Value)

#if defined(__APPLE__)
	#define TEST_CORO_ASM_SYMBOL(Value) "_" TEST_CORO_STRING(Value)
	#define TEST_CORO_ASM_TYPE ""
	#define TEST_CORO_ASM_SIZE ""
	#define TEST_CORO_ASM_CONST_SECTION ".section __TEXT,__const\n"
#else
	#define TEST_CORO_ASM_SYMBOL(Value) TEST_CORO_STRING(Value)
	#define TEST_CORO_ASM_TYPE ".type testCoroAbiArm64Call, %function\n"
	#define TEST_CORO_ASM_SIZE \
		".size testCoroAbiArm64Call, .-testCoroAbiArm64Call\n"
	#define TEST_CORO_ASM_CONST_SECTION ".section .rodata\n"
#endif



/*
	在当前执行上下文中装入 AAPCS64 非易失寄存器，执行一次 resume 或
	yield，恢复后逐个检查寄存器和栈对齐。宿主和协程使用不同的哨兵表。
*/
extern int testCoroAbiArm64Call(xcoro* pCo, int iResume);



/* AAPCS64 要求保存 x19-x29、d8-d15，并保持 SP 为 16 字节对齐。 */
__asm__(
	".text\n"
	".p2align 4\n"
	".globl " TEST_CORO_ASM_SYMBOL(testCoroAbiArm64Call) "\n"
	TEST_CORO_ASM_TYPE
	TEST_CORO_ASM_SYMBOL(testCoroAbiArm64Call) ":\n"
	"sub sp, sp, #240\n"
	"stp x19, x20, [sp, #0]\n"
	"stp x21, x22, [sp, #16]\n"
	"stp x23, x24, [sp, #32]\n"
	"stp x25, x26, [sp, #48]\n"
	"stp x27, x28, [sp, #64]\n"
	"stp x29, x30, [sp, #80]\n"
	"stp q8,  q9,  [sp, #96]\n"
	"stp q10, q11, [sp, #128]\n"
	"stp q12, q13, [sp, #160]\n"
	"stp q14, q15, [sp, #192]\n"
	"str x0, [sp, #224]\n"
	"str w1, [sp, #232]\n"
	"adr x9, 7f\n"
	"cbnz w1, 1f\n"
	"adr x9, 8f\n"
	"1f:\n"
	"ldp x19, x20, [x9, #0]\n"
	"ldp x21, x22, [x9, #16]\n"
	"ldp x23, x24, [x9, #32]\n"
	"ldp x25, x26, [x9, #48]\n"
	"ldp x27, x28, [x9, #64]\n"
	"ldr x29, [x9, #80]\n"
	"ldr d8,  [x9, #88]\n"
	"ldr d9,  [x9, #96]\n"
	"ldr d10, [x9, #104]\n"
	"ldr d11, [x9, #112]\n"
	"ldr d12, [x9, #120]\n"
	"ldr d13, [x9, #128]\n"
	"ldr d14, [x9, #136]\n"
	"ldr d15, [x9, #144]\n"
	"mov x10, sp\n"
	"and x10, x10, #15\n"
	"cbnz x10, 5f\n"
	"ldr w10, [sp, #232]\n"
	"cbz w10, 2f\n"
	"ldr x0, [sp, #224]\n"
	"bl " TEST_CORO_ASM_SYMBOL(xrtCoResume) "\n"
	"and w0, w0, #255\n"
	"b 3f\n"
	"2f:\n"
	"bl " TEST_CORO_ASM_SYMBOL(xrtCoYield) "\n"
	"cmp w0, #0\n"
	"cset w0, eq\n"
	"3f:\n"
	"str w0, [sp, #236]\n"
	"adr x9, 7f\n"
	"ldr w10, [sp, #232]\n"
	"cbnz w10, 4f\n"
	"adr x9, 8f\n"
	"4f:\n"
	"ldr x10, [x9, #0]\n"
	"cmp x19, x10\n"
	"b.ne 5f\n"
	"ldr x10, [x9, #8]\n"
	"cmp x20, x10\n"
	"b.ne 5f\n"
	"ldr x10, [x9, #16]\n"
	"cmp x21, x10\n"
	"b.ne 5f\n"
	"ldr x10, [x9, #24]\n"
	"cmp x22, x10\n"
	"b.ne 5f\n"
	"ldr x10, [x9, #32]\n"
	"cmp x23, x10\n"
	"b.ne 5f\n"
	"ldr x10, [x9, #40]\n"
	"cmp x24, x10\n"
	"b.ne 5f\n"
	"ldr x10, [x9, #48]\n"
	"cmp x25, x10\n"
	"b.ne 5f\n"
	"ldr x10, [x9, #56]\n"
	"cmp x26, x10\n"
	"b.ne 5f\n"
	"ldr x10, [x9, #64]\n"
	"cmp x27, x10\n"
	"b.ne 5f\n"
	"ldr x10, [x9, #72]\n"
	"cmp x28, x10\n"
	"b.ne 5f\n"
	"ldr x10, [x9, #80]\n"
	"cmp x29, x10\n"
	"b.ne 5f\n"
	"ldr x10, [x9, #88]\n"
	"fmov x11, d8\n"
	"cmp x11, x10\n"
	"b.ne 5f\n"
	"ldr x10, [x9, #96]\n"
	"fmov x11, d9\n"
	"cmp x11, x10\n"
	"b.ne 5f\n"
	"ldr x10, [x9, #104]\n"
	"fmov x11, d10\n"
	"cmp x11, x10\n"
	"b.ne 5f\n"
	"ldr x10, [x9, #112]\n"
	"fmov x11, d11\n"
	"cmp x11, x10\n"
	"b.ne 5f\n"
	"ldr x10, [x9, #120]\n"
	"fmov x11, d12\n"
	"cmp x11, x10\n"
	"b.ne 5f\n"
	"ldr x10, [x9, #128]\n"
	"fmov x11, d13\n"
	"cmp x11, x10\n"
	"b.ne 5f\n"
	"ldr x10, [x9, #136]\n"
	"fmov x11, d14\n"
	"cmp x11, x10\n"
	"b.ne 5f\n"
	"ldr x10, [x9, #144]\n"
	"fmov x11, d15\n"
	"cmp x11, x10\n"
	"b.ne 5f\n"
	"ldr w0, [sp, #236]\n"
	"b 6f\n"
	"5f:\n"
	"mov w0, #0\n"
	"6f:\n"
	"ldp q14, q15, [sp, #192]\n"
	"ldp q12, q13, [sp, #160]\n"
	"ldp q10, q11, [sp, #128]\n"
	"ldp q8,  q9,  [sp, #96]\n"
	"ldp x29, x30, [sp, #80]\n"
	"ldp x27, x28, [sp, #64]\n"
	"ldp x25, x26, [sp, #48]\n"
	"ldp x23, x24, [sp, #32]\n"
	"ldp x21, x22, [sp, #16]\n"
	"ldp x19, x20, [sp, #0]\n"
	"add sp, sp, #240\n"
	"ret\n"
	TEST_CORO_ASM_SIZE
	TEST_CORO_ASM_CONST_SECTION
	".p2align 3\n"
	"7f:\n"
	".quad 0x1122334455667701, 0x1122334455667702\n"
	".quad 0x1122334455667703, 0x1122334455667704\n"
	".quad 0x1122334455667705, 0x1122334455667706\n"
	".quad 0x1122334455667707, 0x1122334455667708\n"
	".quad 0x1122334455667709\n"
	".quad 0x2110010203040506, 0x2111020304050607\n"
	".quad 0x2112030405060708, 0x2113040506070809\n"
	".quad 0x211405060708090A, 0x2115060708090A0B\n"
	".quad 0x21160708090A0B0C, 0x211708090A0B0C0D\n"
	"8f:\n"
	".quad 0x6655443322118801, 0x6655443322118802\n"
	".quad 0x6655443322118803, 0x6655443322118804\n"
	".quad 0x6655443322118805, 0x6655443322118806\n"
	".quad 0x6655443322118807, 0x6655443322118808\n"
	".quad 0x6655443322118809\n"
	".quad 0x6220010203040506, 0x6221020304050607\n"
	".quad 0x6222030405060708, 0x6223040506070809\n"
	".quad 0x622405060708090A, 0x6225060708090A0B\n"
	".quad 0x62260708090A0B0C, 0x622708090A0B0C0D\n"
	".text\n"
);

#undef TEST_CORO_ASM_CONST_SECTION
#undef TEST_CORO_ASM_SIZE
#undef TEST_CORO_ASM_TYPE
#undef TEST_CORO_ASM_SYMBOL
#undef TEST_CORO_STRING
#undef TEST_CORO_STRING_INNER



/* 寄存器测试状态记录协程侧已经通过的切换次数。 */
typedef struct testcoroabiarm64state {
	size_t Switches;
	bool RegistersPreserved;
	#if defined(__has_feature)
		#if __has_feature(shadow_call_stack)
			uintptr_t ShadowCallStack;
		#endif
	#endif
} testcoroabiarm64state;



#if defined(__has_feature)
	#if __has_feature(shadow_call_stack)
/* 读取当前 ShadowCallStack 指针，验证宿主与协程使用独立映射。 */
static uintptr_t testCoroAbiArm64ShadowCallStack(void)
{
	uintptr_t iShadowCallStack;

	__asm__ volatile ("mov %0, x18" : "=r"(iShadowCallStack));
	return iShadowCallStack;
}
	#endif
#endif



/* 每次让出前装入协程专用哨兵，恢复后立即检查。 */
static ptr testCoroAbiArm64Proc(ptr pData)
{
	testcoroabiarm64state* pState = (testcoroabiarm64state*)pData;
	size_t i;

	#if defined(__has_feature)
		#if __has_feature(shadow_call_stack)
			pState->ShadowCallStack = testCoroAbiArm64ShadowCallStack();
		#endif
	#endif
	for ( i = 0; i < TEST_CORO_ABI_SWITCH_COUNT; i++ ) {
		if ( !testCoroAbiArm64Call(NULL, 0) ) {
			return NULL;
		}
		pState->Switches++;
	}
	pState->RegistersPreserved = true;
	return pState;
}



/* 同时验证宿主和协程两侧的 AAPCS64 非易失寄存器契约。 */
static void testCoroAbiArm64Registers(void)
{
	testcoroabiarm64state tState;
	xcoro* pCo;
	size_t i;
	#if defined(__has_feature)
		#if __has_feature(shadow_call_stack)
			uintptr_t iHostShadowCallStack;
		#endif
	#endif

	memset(&tState, 0, sizeof(tState));
	#if defined(__has_feature)
		#if __has_feature(shadow_call_stack)
			iHostShadowCallStack = testCoroAbiArm64ShadowCallStack();
		#endif
	#endif
	pCo = xrtCoCreate(testCoroAbiArm64Proc, &tState, NULL);
	testRequire(pCo != NULL, "AArch64 ABI coroutine create failed");
	for ( i = 0; i <= TEST_CORO_ABI_SWITCH_COUNT; i++ ) {
		testRequire(
			testCoroAbiArm64Call(pCo, 1) != 0,
			"AArch64 host nonvolatile register preservation failed"
		);
	}
	testRequire(
		tState.RegistersPreserved &&
		(tState.Switches == TEST_CORO_ABI_SWITCH_COUNT),
		"AArch64 coroutine nonvolatile register preservation failed"
	);
	#if defined(__has_feature)
		#if __has_feature(shadow_call_stack)
			testRequire(
				(tState.ShadowCallStack != 0) &&
				(tState.ShadowCallStack != iHostShadowCallStack),
				"AArch64 coroutine shadow call stack is not isolated"
			);
		#endif
	#endif
	testRequire(
		xrtCoState(pCo) == XCORO_DONE,
		"AArch64 ABI coroutine did not finish"
	);
	testRequire(xrtCoDestroy(pCo), "AArch64 ABI coroutine destroy failed");
}



/* 运行 AArch64 协程 ABI 专项门禁。 */
int main(void)
{
	testCoroAbiArm64Registers();
	testRequire(xrtCoThreadDetach(), "AArch64 ABI coroutine runtime detach failed");

	printf("[PASS] coroutine AArch64 ABI\n");
	return 0;
}



#else

#include <stdio.h>

/* 非目标架构不声明 AArch64 的运行证据。 */
int main(void)
{
	printf("[SKIP] coroutine AArch64 ABI\n");
	return 0;
}

#endif
