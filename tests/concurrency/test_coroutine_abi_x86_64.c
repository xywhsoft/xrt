#if defined(__x86_64__) && \
	(defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)

#include "../test.h"

#define TEST_CORO_ABI_SWITCH_COUNT 4096u

#define TEST_CORO_STRING_INNER(Value) #Value
#define TEST_CORO_STRING(Value) TEST_CORO_STRING_INNER(Value)

#if defined(__APPLE__)
	#define TEST_CORO_ASM_SYMBOL(Value) "_" TEST_CORO_STRING(Value)
#else
	#define TEST_CORO_ASM_SYMBOL(Value) TEST_CORO_STRING(Value)
#endif



/*
	在当前执行上下文中装入非易失寄存器，执行一次 resume 或 yield，
	恢复后逐个检查寄存器。宿主和协程使用不同的哨兵表。
*/
extern int testCoroAbiCall(xcoro* pCo, int iResume);



#if defined(_WIN32) || defined(_WIN64)

/* Windows x64 ABI 还要求 Fiber 保存 xmm6-xmm15。 */
__asm__(
	".text\n"
	".p2align 4\n"
	".globl " TEST_CORO_ASM_SYMBOL(testCoroAbiCall) "\n"
	TEST_CORO_ASM_SYMBOL(testCoroAbiCall) ":\n"
	"endbr64\n"
	"pushq %rbx\n"
	"pushq %rbp\n"
	"pushq %rdi\n"
	"pushq %rsi\n"
	"pushq %r12\n"
	"pushq %r13\n"
	"pushq %r14\n"
	"pushq %r15\n"
	"subq $216, %rsp\n"
	"movdqu %xmm6,  32(%rsp)\n"
	"movdqu %xmm7,  48(%rsp)\n"
	"movdqu %xmm8,  64(%rsp)\n"
	"movdqu %xmm9,  80(%rsp)\n"
	"movdqu %xmm10, 96(%rsp)\n"
	"movdqu %xmm11, 112(%rsp)\n"
	"movdqu %xmm12, 128(%rsp)\n"
	"movdqu %xmm13, 144(%rsp)\n"
	"movdqu %xmm14, 160(%rsp)\n"
	"movdqu %xmm15, 176(%rsp)\n"
	"movq %rcx, 192(%rsp)\n"
	"movl %edx, 200(%rsp)\n"
	"leaq .LtestCoroAbiHost(%rip), %r10\n"
	"cmpl $0, 200(%rsp)\n"
	"jne .LtestCoroAbiLoad\n"
	"leaq .LtestCoroAbiFiber(%rip), %r10\n"
	".LtestCoroAbiLoad:\n"
	"movq   0(%r10), %rbx\n"
	"movq   8(%r10), %rbp\n"
	"movq  16(%r10), %rdi\n"
	"movq  24(%r10), %rsi\n"
	"movq  32(%r10), %r12\n"
	"movq  40(%r10), %r13\n"
	"movq  48(%r10), %r14\n"
	"movq  56(%r10), %r15\n"
	"movdqu  64(%r10), %xmm6\n"
	"movdqu  80(%r10), %xmm7\n"
	"movdqu  96(%r10), %xmm8\n"
	"movdqu 112(%r10), %xmm9\n"
	"movdqu 128(%r10), %xmm10\n"
	"movdqu 144(%r10), %xmm11\n"
	"movdqu 160(%r10), %xmm12\n"
	"movdqu 176(%r10), %xmm13\n"
	"movdqu 192(%r10), %xmm14\n"
	"movdqu 208(%r10), %xmm15\n"
	"testq $15, %rsp\n"
	"jne .LtestCoroAbiFail\n"
	"cmpl $0, 200(%rsp)\n"
	"je .LtestCoroAbiYield\n"
	"movq 192(%rsp), %rcx\n"
	"call " TEST_CORO_ASM_SYMBOL(xrtCoResume) "\n"
	"movzbl %al, %eax\n"
	"jmp .LtestCoroAbiCalled\n"
	".LtestCoroAbiYield:\n"
	"call " TEST_CORO_ASM_SYMBOL(xrtCoYield) "\n"
	"testl %eax, %eax\n"
	"sete %al\n"
	"movzbl %al, %eax\n"
	".LtestCoroAbiCalled:\n"
	"movl %eax, 204(%rsp)\n"
	"leaq .LtestCoroAbiHost(%rip), %r10\n"
	"cmpl $0, 200(%rsp)\n"
	"jne .LtestCoroAbiCheck\n"
	"leaq .LtestCoroAbiFiber(%rip), %r10\n"
	".LtestCoroAbiCheck:\n"
	"cmpq   0(%r10), %rbx\n"
	"jne .LtestCoroAbiFail\n"
	"cmpq   8(%r10), %rbp\n"
	"jne .LtestCoroAbiFail\n"
	"cmpq  16(%r10), %rdi\n"
	"jne .LtestCoroAbiFail\n"
	"cmpq  24(%r10), %rsi\n"
	"jne .LtestCoroAbiFail\n"
	"cmpq  32(%r10), %r12\n"
	"jne .LtestCoroAbiFail\n"
	"cmpq  40(%r10), %r13\n"
	"jne .LtestCoroAbiFail\n"
	"cmpq  48(%r10), %r14\n"
	"jne .LtestCoroAbiFail\n"
	"cmpq  56(%r10), %r15\n"
	"jne .LtestCoroAbiFail\n"
	"movdqa %xmm6, %xmm0\n"
	"pcmpeqb 64(%r10), %xmm0\n"
	"pmovmskb %xmm0, %eax\n"
	"cmpl $65535, %eax\n"
	"jne .LtestCoroAbiFail\n"
	"movdqa %xmm7, %xmm0\n"
	"pcmpeqb 80(%r10), %xmm0\n"
	"pmovmskb %xmm0, %eax\n"
	"cmpl $65535, %eax\n"
	"jne .LtestCoroAbiFail\n"
	"movdqa %xmm8, %xmm0\n"
	"pcmpeqb 96(%r10), %xmm0\n"
	"pmovmskb %xmm0, %eax\n"
	"cmpl $65535, %eax\n"
	"jne .LtestCoroAbiFail\n"
	"movdqa %xmm9, %xmm0\n"
	"pcmpeqb 112(%r10), %xmm0\n"
	"pmovmskb %xmm0, %eax\n"
	"cmpl $65535, %eax\n"
	"jne .LtestCoroAbiFail\n"
	"movdqa %xmm10, %xmm0\n"
	"pcmpeqb 128(%r10), %xmm0\n"
	"pmovmskb %xmm0, %eax\n"
	"cmpl $65535, %eax\n"
	"jne .LtestCoroAbiFail\n"
	"movdqa %xmm11, %xmm0\n"
	"pcmpeqb 144(%r10), %xmm0\n"
	"pmovmskb %xmm0, %eax\n"
	"cmpl $65535, %eax\n"
	"jne .LtestCoroAbiFail\n"
	"movdqa %xmm12, %xmm0\n"
	"pcmpeqb 160(%r10), %xmm0\n"
	"pmovmskb %xmm0, %eax\n"
	"cmpl $65535, %eax\n"
	"jne .LtestCoroAbiFail\n"
	"movdqa %xmm13, %xmm0\n"
	"pcmpeqb 176(%r10), %xmm0\n"
	"pmovmskb %xmm0, %eax\n"
	"cmpl $65535, %eax\n"
	"jne .LtestCoroAbiFail\n"
	"movdqa %xmm14, %xmm0\n"
	"pcmpeqb 192(%r10), %xmm0\n"
	"pmovmskb %xmm0, %eax\n"
	"cmpl $65535, %eax\n"
	"jne .LtestCoroAbiFail\n"
	"movdqa %xmm15, %xmm0\n"
	"pcmpeqb 208(%r10), %xmm0\n"
	"pmovmskb %xmm0, %eax\n"
	"cmpl $65535, %eax\n"
	"jne .LtestCoroAbiFail\n"
	"movl 204(%rsp), %eax\n"
	"jmp .LtestCoroAbiRestore\n"
	".LtestCoroAbiFail:\n"
	"xorl %eax, %eax\n"
	".LtestCoroAbiRestore:\n"
	"movdqu  32(%rsp), %xmm6\n"
	"movdqu  48(%rsp), %xmm7\n"
	"movdqu  64(%rsp), %xmm8\n"
	"movdqu  80(%rsp), %xmm9\n"
	"movdqu  96(%rsp), %xmm10\n"
	"movdqu 112(%rsp), %xmm11\n"
	"movdqu 128(%rsp), %xmm12\n"
	"movdqu 144(%rsp), %xmm13\n"
	"movdqu 160(%rsp), %xmm14\n"
	"movdqu 176(%rsp), %xmm15\n"
	"addq $216, %rsp\n"
	"popq %r15\n"
	"popq %r14\n"
	"popq %r13\n"
	"popq %r12\n"
	"popq %rsi\n"
	"popq %rdi\n"
	"popq %rbp\n"
	"popq %rbx\n"
	"ret\n"
	".section .rdata,\"dr\"\n"
	".p2align 4\n"
	".LtestCoroAbiHost:\n"
	".quad 0x1122334455667701, 0x1122334455667702\n"
	".quad 0x1122334455667703, 0x1122334455667704\n"
	".quad 0x1122334455667705, 0x1122334455667706\n"
	".quad 0x1122334455667707, 0x1122334455667708\n"
	".quad 0x2110010203040506, 0x2111020304050607\n"
	".quad 0x2112030405060708, 0x2113040506070809\n"
	".quad 0x211405060708090A, 0x2115060708090A0B\n"
	".quad 0x21160708090A0B0C, 0x211708090A0B0C0D\n"
	".quad 0x2118090A0B0C0D0E, 0x21190A0B0C0D0E0F\n"
	".quad 0x211A0B0C0D0E0F10, 0x211B0C0D0E0F1011\n"
	".quad 0x211C0D0E0F101112, 0x211D0E0F10111213\n"
	".quad 0x211E0F1011121314, 0x211F101112131415\n"
	".quad 0x2120111213141516, 0x2121121314151617\n"
	".quad 0x2122131415161718, 0x2123141516171819\n"
	".LtestCoroAbiFiber:\n"
	".quad 0x6655443322118801, 0x6655443322118802\n"
	".quad 0x6655443322118803, 0x6655443322118804\n"
	".quad 0x6655443322118805, 0x6655443322118806\n"
	".quad 0x6655443322118807, 0x6655443322118808\n"
	".quad 0x6220010203040506, 0x6221020304050607\n"
	".quad 0x6222030405060708, 0x6223040506070809\n"
	".quad 0x622405060708090A, 0x6225060708090A0B\n"
	".quad 0x62260708090A0B0C, 0x622708090A0B0C0D\n"
	".quad 0x6228090A0B0C0D0E, 0x62290A0B0C0D0E0F\n"
	".quad 0x622A0B0C0D0E0F10, 0x622B0C0D0E0F1011\n"
	".quad 0x622C0D0E0F101112, 0x622D0E0F10111213\n"
	".quad 0x622E0F1011121314, 0x622F101112131415\n"
	".quad 0x6230111213141516, 0x6231121314151617\n"
	".quad 0x6232131415161718, 0x6233141516171819\n"
	".text\n"
);



#else

/* System V x86-64 ABI 只要求保存非易失通用寄存器。 */
__asm__(
	".text\n"
	".p2align 4\n"
	".globl " TEST_CORO_ASM_SYMBOL(testCoroAbiCall) "\n"
	TEST_CORO_ASM_SYMBOL(testCoroAbiCall) ":\n"
	"endbr64\n"
	"pushq %rbx\n"
	"pushq %rbp\n"
	"pushq %r12\n"
	"pushq %r13\n"
	"pushq %r14\n"
	"pushq %r15\n"
	"subq $24, %rsp\n"
	"movq %rdi, 0(%rsp)\n"
	"movl %esi, 8(%rsp)\n"
	"leaq .LtestCoroAbiHost(%rip), %r10\n"
	"cmpl $0, 8(%rsp)\n"
	"jne .LtestCoroAbiLoad\n"
	"leaq .LtestCoroAbiFiber(%rip), %r10\n"
	".LtestCoroAbiLoad:\n"
	"movq  0(%r10), %rbx\n"
	"movq  8(%r10), %rbp\n"
	"movq 16(%r10), %r12\n"
	"movq 24(%r10), %r13\n"
	"movq 32(%r10), %r14\n"
	"movq 40(%r10), %r15\n"
	"testq $15, %rsp\n"
	"jne .LtestCoroAbiFail\n"
	"cmpl $0, 8(%rsp)\n"
	"je .LtestCoroAbiYield\n"
	"movq 0(%rsp), %rdi\n"
	"call " TEST_CORO_ASM_SYMBOL(xrtCoResume) "\n"
	"movzbl %al, %eax\n"
	"jmp .LtestCoroAbiCalled\n"
	".LtestCoroAbiYield:\n"
	"call " TEST_CORO_ASM_SYMBOL(xrtCoYield) "\n"
	"testl %eax, %eax\n"
	"sete %al\n"
	"movzbl %al, %eax\n"
	".LtestCoroAbiCalled:\n"
	"movl %eax, 12(%rsp)\n"
	"leaq .LtestCoroAbiHost(%rip), %r10\n"
	"cmpl $0, 8(%rsp)\n"
	"jne .LtestCoroAbiCheck\n"
	"leaq .LtestCoroAbiFiber(%rip), %r10\n"
	".LtestCoroAbiCheck:\n"
	"cmpq  0(%r10), %rbx\n"
	"jne .LtestCoroAbiFail\n"
	"cmpq  8(%r10), %rbp\n"
	"jne .LtestCoroAbiFail\n"
	"cmpq 16(%r10), %r12\n"
	"jne .LtestCoroAbiFail\n"
	"cmpq 24(%r10), %r13\n"
	"jne .LtestCoroAbiFail\n"
	"cmpq 32(%r10), %r14\n"
	"jne .LtestCoroAbiFail\n"
	"cmpq 40(%r10), %r15\n"
	"jne .LtestCoroAbiFail\n"
	"movl 12(%rsp), %eax\n"
	"jmp .LtestCoroAbiRestore\n"
	".LtestCoroAbiFail:\n"
	"xorl %eax, %eax\n"
	".LtestCoroAbiRestore:\n"
	"addq $24, %rsp\n"
	"popq %r15\n"
	"popq %r14\n"
	"popq %r13\n"
	"popq %r12\n"
	"popq %rbp\n"
	"popq %rbx\n"
	"ret\n"
	#if defined(__APPLE__)
		".section __TEXT,__const\n"
	#else
		".section .rodata\n"
	#endif
	".p2align 4\n"
	".LtestCoroAbiHost:\n"
	".quad 0x1122334455667701, 0x1122334455667702\n"
	".quad 0x1122334455667703, 0x1122334455667704\n"
	".quad 0x1122334455667705, 0x1122334455667706\n"
	".LtestCoroAbiFiber:\n"
	".quad 0x6655443322118801, 0x6655443322118802\n"
	".quad 0x6655443322118803, 0x6655443322118804\n"
	".quad 0x6655443322118805, 0x6655443322118806\n"
	".text\n"
);

#endif



/* 寄存器测试状态记录协程侧已通过的切换次数。 */
typedef struct testcoroabistate {
	size_t Switches;
	bool RegistersPreserved;
} testcoroabistate;



/* 每次让出前装入协程专用哨兵，恢复后立即检查。 */
static ptr testCoroAbiProc(ptr pData)
{
	testcoroabistate* pState = (testcoroabistate*)pData;
	size_t i;

	for ( i = 0; i < TEST_CORO_ABI_SWITCH_COUNT; i++ ) {
		if ( !testCoroAbiCall(NULL, 0) ) {
			return NULL;
		}
		pState->Switches++;
	}
	pState->RegistersPreserved = true;
	return pState;
}



/* 同时验证宿主和协程两侧的非易失寄存器保存契约。 */
static void testCoroAbiRegisters(void)
{
	testcoroabistate tState;
	xcoro* pCo;
	size_t i;

	memset(&tState, 0, sizeof(tState));
	pCo = xrtCoCreate(testCoroAbiProc, &tState, NULL);
	testRequire(pCo != NULL, "x86-64 ABI coroutine create failed");
	for ( i = 0; i <= TEST_CORO_ABI_SWITCH_COUNT; i++ ) {
		testRequire(
			testCoroAbiCall(pCo, 1) != 0,
			"x86-64 host nonvolatile register preservation failed"
		);
	}
	testRequire(
		tState.RegistersPreserved &&
		(tState.Switches == TEST_CORO_ABI_SWITCH_COUNT),
		"x86-64 coroutine nonvolatile register preservation failed"
	);
	testRequire(xrtCoState(pCo) == XCORO_DONE, "x86-64 ABI coroutine did not finish");
	testRequire(xrtCoDestroy(pCo), "x86-64 ABI coroutine destroy failed");
}



/* 运行 x86-64 协程 ABI 专项门禁。 */
int main(void)
{
	testCoroAbiRegisters();
	testRequire(xrtCoThreadDetach(), "x86-64 ABI coroutine runtime detach failed");

	printf("[PASS] coroutine x86-64 ABI\n");
	return 0;
}



#else

#include <stdio.h>

/* 非目标架构不声明 x86-64 的运行证据。 */
int main(void)
{
	printf("[SKIP] coroutine x86-64 ABI\n");
	return 0;
}

#endif
