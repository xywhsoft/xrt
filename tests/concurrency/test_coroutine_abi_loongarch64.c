#if defined(__loongarch64) && \
	(defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)

#include "../test.h"

#define TEST_CORO_ABI_SWITCH_COUNT 4096u

#if (defined(__loongarch_frlen) && (__loongarch_frlen >= 64)) || \
	defined(__loongarch_double_float)
	#define TEST_CORO_LA_FP_SAVE \
		"fst.d $fs0, $sp, 88\n" \
		"fst.d $fs1, $sp, 96\n" \
		"fst.d $fs2, $sp, 104\n" \
		"fst.d $fs3, $sp, 112\n" \
		"fst.d $fs4, $sp, 120\n" \
		"fst.d $fs5, $sp, 128\n" \
		"fst.d $fs6, $sp, 136\n" \
		"fst.d $fs7, $sp, 144\n"
	#define TEST_CORO_LA_FP_LOAD \
		"fld.d $fs0, $t0, 80\n" \
		"fld.d $fs1, $t0, 88\n" \
		"fld.d $fs2, $t0, 96\n" \
		"fld.d $fs3, $t0, 104\n" \
		"fld.d $fs4, $t0, 112\n" \
		"fld.d $fs5, $t0, 120\n" \
		"fld.d $fs6, $t0, 128\n" \
		"fld.d $fs7, $t0, 136\n"
	#define TEST_CORO_LA_FP_CHECK \
		"movfr2gr.d $t2, $fs0\n" \
		"ld.d $t1, $t0, 80\n" \
		"bne $t2, $t1, .LtestCoroAbiLoongArch64Fail\n" \
		"movfr2gr.d $t2, $fs1\n" \
		"ld.d $t1, $t0, 88\n" \
		"bne $t2, $t1, .LtestCoroAbiLoongArch64Fail\n" \
		"movfr2gr.d $t2, $fs2\n" \
		"ld.d $t1, $t0, 96\n" \
		"bne $t2, $t1, .LtestCoroAbiLoongArch64Fail\n" \
		"movfr2gr.d $t2, $fs3\n" \
		"ld.d $t1, $t0, 104\n" \
		"bne $t2, $t1, .LtestCoroAbiLoongArch64Fail\n" \
		"movfr2gr.d $t2, $fs4\n" \
		"ld.d $t1, $t0, 112\n" \
		"bne $t2, $t1, .LtestCoroAbiLoongArch64Fail\n" \
		"movfr2gr.d $t2, $fs5\n" \
		"ld.d $t1, $t0, 120\n" \
		"bne $t2, $t1, .LtestCoroAbiLoongArch64Fail\n" \
		"movfr2gr.d $t2, $fs6\n" \
		"ld.d $t1, $t0, 128\n" \
		"bne $t2, $t1, .LtestCoroAbiLoongArch64Fail\n" \
		"movfr2gr.d $t2, $fs7\n" \
		"ld.d $t1, $t0, 136\n" \
		"bne $t2, $t1, .LtestCoroAbiLoongArch64Fail\n"
	#define TEST_CORO_LA_FP_RESTORE \
		"fld.d $fs0, $sp, 88\n" \
		"fld.d $fs1, $sp, 96\n" \
		"fld.d $fs2, $sp, 104\n" \
		"fld.d $fs3, $sp, 112\n" \
		"fld.d $fs4, $sp, 120\n" \
		"fld.d $fs5, $sp, 128\n" \
		"fld.d $fs6, $sp, 136\n" \
		"fld.d $fs7, $sp, 144\n"
#elif (defined(__loongarch_frlen) && (__loongarch_frlen == 32)) || \
	defined(__loongarch_single_float)
	#define TEST_CORO_LA_FP_SAVE \
		"fst.s $fs0, $sp, 88\n" \
		"fst.s $fs1, $sp, 96\n" \
		"fst.s $fs2, $sp, 104\n" \
		"fst.s $fs3, $sp, 112\n" \
		"fst.s $fs4, $sp, 120\n" \
		"fst.s $fs5, $sp, 128\n" \
		"fst.s $fs6, $sp, 136\n" \
		"fst.s $fs7, $sp, 144\n"
	#define TEST_CORO_LA_FP_LOAD \
		"fld.s $fs0, $t0, 80\n" \
		"fld.s $fs1, $t0, 88\n" \
		"fld.s $fs2, $t0, 96\n" \
		"fld.s $fs3, $t0, 104\n" \
		"fld.s $fs4, $t0, 112\n" \
		"fld.s $fs5, $t0, 120\n" \
		"fld.s $fs6, $t0, 128\n" \
		"fld.s $fs7, $t0, 136\n"
	#define TEST_CORO_LA_FP_CHECK \
		"movfr2gr.s $t2, $fs0\n" \
		"ld.w $t1, $t0, 80\n" \
		"bne $t2, $t1, .LtestCoroAbiLoongArch64Fail\n" \
		"movfr2gr.s $t2, $fs1\n" \
		"ld.w $t1, $t0, 88\n" \
		"bne $t2, $t1, .LtestCoroAbiLoongArch64Fail\n" \
		"movfr2gr.s $t2, $fs2\n" \
		"ld.w $t1, $t0, 96\n" \
		"bne $t2, $t1, .LtestCoroAbiLoongArch64Fail\n" \
		"movfr2gr.s $t2, $fs3\n" \
		"ld.w $t1, $t0, 104\n" \
		"bne $t2, $t1, .LtestCoroAbiLoongArch64Fail\n" \
		"movfr2gr.s $t2, $fs4\n" \
		"ld.w $t1, $t0, 112\n" \
		"bne $t2, $t1, .LtestCoroAbiLoongArch64Fail\n" \
		"movfr2gr.s $t2, $fs5\n" \
		"ld.w $t1, $t0, 120\n" \
		"bne $t2, $t1, .LtestCoroAbiLoongArch64Fail\n" \
		"movfr2gr.s $t2, $fs6\n" \
		"ld.w $t1, $t0, 128\n" \
		"bne $t2, $t1, .LtestCoroAbiLoongArch64Fail\n" \
		"movfr2gr.s $t2, $fs7\n" \
		"ld.w $t1, $t0, 136\n" \
		"bne $t2, $t1, .LtestCoroAbiLoongArch64Fail\n"
	#define TEST_CORO_LA_FP_RESTORE \
		"fld.s $fs0, $sp, 88\n" \
		"fld.s $fs1, $sp, 96\n" \
		"fld.s $fs2, $sp, 104\n" \
		"fld.s $fs3, $sp, 112\n" \
		"fld.s $fs4, $sp, 120\n" \
		"fld.s $fs5, $sp, 128\n" \
		"fld.s $fs6, $sp, 136\n" \
		"fld.s $fs7, $sp, 144\n"
#else
	#define TEST_CORO_LA_FP_SAVE ""
	#define TEST_CORO_LA_FP_LOAD ""
	#define TEST_CORO_LA_FP_CHECK ""
	#define TEST_CORO_LA_FP_RESTORE ""
#endif



/*
	在当前执行上下文中装入 LoongArch64 非易失寄存器，执行一次 resume 或
	yield，恢复后逐个检查寄存器和栈对齐。宿主和协程使用不同的哨兵表。
*/
extern int testCoroAbiLoongArch64Call(xcoro* pCo, int iResume);



/* LoongArch ELF ABI 要求保存 fp/s0-s8、fs0-fs7，并保持 SP 为 16 字节对齐。 */
__asm__(
	".text\n"
	".p2align 4\n"
	".globl testCoroAbiLoongArch64Call\n"
	".type testCoroAbiLoongArch64Call, @function\n"
	"testCoroAbiLoongArch64Call:\n"
	"addi.d $sp, $sp, -176\n"
	"st.d $ra, $sp, 0\n"
	"st.d $fp, $sp, 8\n"
	"st.d $s0, $sp, 16\n"
	"st.d $s1, $sp, 24\n"
	"st.d $s2, $sp, 32\n"
	"st.d $s3, $sp, 40\n"
	"st.d $s4, $sp, 48\n"
	"st.d $s5, $sp, 56\n"
	"st.d $s6, $sp, 64\n"
	"st.d $s7, $sp, 72\n"
	"st.d $s8, $sp, 80\n"
	TEST_CORO_LA_FP_SAVE
	"st.d $a0, $sp, 152\n"
	"st.w $a1, $sp, 160\n"
	"la.local $t0, .LtestCoroAbiLoongArch64Host\n"
	"bnez $a1, .LtestCoroAbiLoongArch64Load\n"
	"la.local $t0, .LtestCoroAbiLoongArch64Fiber\n"
	".LtestCoroAbiLoongArch64Load:\n"
	"ld.d $fp, $t0, 0\n"
	"ld.d $s0, $t0, 8\n"
	"ld.d $s1, $t0, 16\n"
	"ld.d $s2, $t0, 24\n"
	"ld.d $s3, $t0, 32\n"
	"ld.d $s4, $t0, 40\n"
	"ld.d $s5, $t0, 48\n"
	"ld.d $s6, $t0, 56\n"
	"ld.d $s7, $t0, 64\n"
	"ld.d $s8, $t0, 72\n"
	TEST_CORO_LA_FP_LOAD
	"andi $t1, $sp, 15\n"
	"bnez $t1, .LtestCoroAbiLoongArch64Fail\n"
	"ld.w $t1, $sp, 160\n"
	"beqz $t1, .LtestCoroAbiLoongArch64Yield\n"
	"ld.d $a0, $sp, 152\n"
	"bl xrtCoResume\n"
	"andi $a0, $a0, 255\n"
	"b .LtestCoroAbiLoongArch64Called\n"
	".LtestCoroAbiLoongArch64Yield:\n"
	"bl xrtCoYield\n"
	"sltui $a0, $a0, 1\n"
	".LtestCoroAbiLoongArch64Called:\n"
	"st.w $a0, $sp, 164\n"
	"la.local $t0, .LtestCoroAbiLoongArch64Host\n"
	"ld.w $t1, $sp, 160\n"
	"bnez $t1, .LtestCoroAbiLoongArch64Check\n"
	"la.local $t0, .LtestCoroAbiLoongArch64Fiber\n"
	".LtestCoroAbiLoongArch64Check:\n"
	"ld.d $t1, $t0, 0\n"
	"bne $fp, $t1, .LtestCoroAbiLoongArch64Fail\n"
	"ld.d $t1, $t0, 8\n"
	"bne $s0, $t1, .LtestCoroAbiLoongArch64Fail\n"
	"ld.d $t1, $t0, 16\n"
	"bne $s1, $t1, .LtestCoroAbiLoongArch64Fail\n"
	"ld.d $t1, $t0, 24\n"
	"bne $s2, $t1, .LtestCoroAbiLoongArch64Fail\n"
	"ld.d $t1, $t0, 32\n"
	"bne $s3, $t1, .LtestCoroAbiLoongArch64Fail\n"
	"ld.d $t1, $t0, 40\n"
	"bne $s4, $t1, .LtestCoroAbiLoongArch64Fail\n"
	"ld.d $t1, $t0, 48\n"
	"bne $s5, $t1, .LtestCoroAbiLoongArch64Fail\n"
	"ld.d $t1, $t0, 56\n"
	"bne $s6, $t1, .LtestCoroAbiLoongArch64Fail\n"
	"ld.d $t1, $t0, 64\n"
	"bne $s7, $t1, .LtestCoroAbiLoongArch64Fail\n"
	"ld.d $t1, $t0, 72\n"
	"bne $s8, $t1, .LtestCoroAbiLoongArch64Fail\n"
	TEST_CORO_LA_FP_CHECK
	"ld.w $a0, $sp, 164\n"
	"b .LtestCoroAbiLoongArch64Restore\n"
	".LtestCoroAbiLoongArch64Fail:\n"
	"move $a0, $zero\n"
	".LtestCoroAbiLoongArch64Restore:\n"
	TEST_CORO_LA_FP_RESTORE
	"ld.d $s8, $sp, 80\n"
	"ld.d $s7, $sp, 72\n"
	"ld.d $s6, $sp, 64\n"
	"ld.d $s5, $sp, 56\n"
	"ld.d $s4, $sp, 48\n"
	"ld.d $s3, $sp, 40\n"
	"ld.d $s2, $sp, 32\n"
	"ld.d $s1, $sp, 24\n"
	"ld.d $s0, $sp, 16\n"
	"ld.d $fp, $sp, 8\n"
	"ld.d $ra, $sp, 0\n"
	"addi.d $sp, $sp, 176\n"
	"jr $ra\n"
	".size testCoroAbiLoongArch64Call, .-testCoroAbiLoongArch64Call\n"
	".section .rodata\n"
	".p2align 3\n"
	".LtestCoroAbiLoongArch64Host:\n"
	".quad 0x1122334455667701, 0x1122334455667702\n"
	".quad 0x1122334455667703, 0x1122334455667704\n"
	".quad 0x1122334455667705, 0x1122334455667706\n"
	".quad 0x1122334455667707, 0x1122334455667708\n"
	".quad 0x1122334455667709, 0x112233445566770A\n"
	".quad 0x2110010203040506, 0x2111020304050607\n"
	".quad 0x2112030405060708, 0x2113040506070809\n"
	".quad 0x211405060708090A, 0x2115060708090A0B\n"
	".LtestCoroAbiLoongArch64Fiber:\n"
	".quad 0x6655443322118801, 0x6655443322118802\n"
	".quad 0x6655443322118803, 0x6655443322118804\n"
	".quad 0x6655443322118805, 0x6655443322118806\n"
	".quad 0x6655443322118807, 0x6655443322118808\n"
	".quad 0x6655443322118809, 0x665544332211880A\n"
	".quad 0x6220010203040506, 0x6221020304050607\n"
	".quad 0x6222030405060708, 0x6223040506070809\n"
	".quad 0x622405060708090A, 0x6225060708090A0B\n"
	".text\n"
);

#undef TEST_CORO_LA_FP_RESTORE
#undef TEST_CORO_LA_FP_CHECK
#undef TEST_CORO_LA_FP_LOAD
#undef TEST_CORO_LA_FP_SAVE



/* 寄存器测试状态记录协程侧已经通过的切换次数。 */
typedef struct testcoroabiloongarch64state {
	size_t Switches;
	bool RegistersPreserved;
} testcoroabiloongarch64state;



/* 每次让出前装入协程专用哨兵，恢复后立即检查。 */
static ptr testCoroAbiLoongArch64Proc(ptr pData)
{
	testcoroabiloongarch64state* pState =
		(testcoroabiloongarch64state*)pData;
	size_t i;

	for ( i = 0; i < TEST_CORO_ABI_SWITCH_COUNT; i++ ) {
		if ( !testCoroAbiLoongArch64Call(NULL, 0) ) {
			return NULL;
		}
		pState->Switches++;
	}
	pState->RegistersPreserved = true;
	return pState;
}



/* 同时验证宿主和协程两侧的 LoongArch64 非易失寄存器契约。 */
static void testCoroAbiLoongArch64Registers(void)
{
	testcoroabiloongarch64state tState;
	xcoro* pCo;
	size_t i;

	memset(&tState, 0, sizeof(tState));
	pCo = xrtCoCreate(testCoroAbiLoongArch64Proc, &tState, NULL);
	testRequire(pCo != NULL, "LoongArch64 ABI coroutine create failed");
	for ( i = 0; i <= TEST_CORO_ABI_SWITCH_COUNT; i++ ) {
		testRequire(
			testCoroAbiLoongArch64Call(pCo, 1) != 0,
			"LoongArch64 host nonvolatile register preservation failed"
		);
	}
	testRequire(
		tState.RegistersPreserved &&
		(tState.Switches == TEST_CORO_ABI_SWITCH_COUNT),
		"LoongArch64 coroutine nonvolatile register preservation failed"
	);
	testRequire(
		xrtCoState(pCo) == XCORO_DONE,
		"LoongArch64 ABI coroutine did not finish"
	);
	testRequire(
		xrtCoDestroy(pCo),
		"LoongArch64 ABI coroutine destroy failed"
	);
}



/* 运行 LoongArch64 协程 ABI 专项门禁。 */
int main(void)
{
	testCoroAbiLoongArch64Registers();
	testRequire(
		xrtCoThreadDetach(),
		"LoongArch64 ABI coroutine runtime detach failed"
	);

	printf("[PASS] coroutine LoongArch64 ABI\n");
	return 0;
}



#else

#include <stdio.h>

/* 非目标架构不声明 LoongArch64 的运行证据。 */
int main(void)
{
	printf("[SKIP] coroutine LoongArch64 ABI\n");
	return 0;
}

#endif
