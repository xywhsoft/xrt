#include "../internal/xrt_coroutine.h"



#if defined(XRT_FEATURE_COROUTINE) && !defined(_WIN32) && !defined(_WIN64)

#if defined(__x86_64__) && defined(__CET__) && \
	((__CET__ & 0x2) != 0) && !defined(__linux__)
	#error "XRT coroutine shadow stacks currently require Linux x86-64"
#endif

#if defined(__GNUC__) || defined(__clang__)
	#define XRT_CO_CONTEXT_NOINLINE \
		__attribute__((noinline)) \
		XRT_CO_NO_ADDRESS_SANITIZER XRT_CO_NO_THREAD_SANITIZER \
		XRT_CO_NO_MEMORY_SANITIZER XRT_CO_NO_SHADOW_CALL_STACK
#else
	#define XRT_CO_CONTEXT_NOINLINE
#endif

_Static_assert(
	sizeof(xrt_co_context) >= 320u,
	"coroutine context storage is too small"
);
_Static_assert(
	(offsetof(xrt_co_context, Registers) + (39u * sizeof(ptr))) == 0x138u,
	"coroutine entry data offset changed"
);
_Static_assert(
	(offsetof(xrt_co_context, Registers) + (38u * sizeof(ptr))) == 0x130u,
	"coroutine shadow stack offset changed"
);



#if defined(__x86_64__) || defined(_M_X64)

#if defined(__TINYC__)
	/* TinyCC 不接受 XMM 寄存器 clobber，函数调用边界已按 ABI 破坏这些寄存器。 */
	#define XRT_CO_X64_CLOBBERS \
		"memory", "cc", "rax", "rcx", "rdx", "r8", "r9", "r10", "r11"
#else
	#define XRT_CO_X64_CLOBBERS \
		"memory", "cc", "rax", "rcx", "rdx", "r8", "r9", "r10", "r11", \
		"xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", \
		"xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"
#endif

/* x86-64 System V 保存全部非易失通用寄存器。 */
XRT_CO_CONTEXT_NOINLINE
void __xrtCoContextSwap(xrt_co_context* pFrom, xrt_co_context* pTo)
{
	__asm__ volatile (
		"leaq 1f(%%rip), %%rax\n\t"
		"movq %%rax, 0x00(%%rdi)\n\t"
		"movq %%rsp, 0x08(%%rdi)\n\t"
		"movq %%rbp, 0x10(%%rdi)\n\t"
		"movq %%rbx, 0x18(%%rdi)\n\t"
		"movq %%r12, 0x20(%%rdi)\n\t"
		"movq %%r13, 0x28(%%rdi)\n\t"
		"movq %%r14, 0x30(%%rdi)\n\t"
		"movq %%r15, 0x38(%%rdi)\n\t"
		#if defined(XRT_CO_SHADOW_STACK)
			"movq 0x130(%%rsi), %%rcx\n\t"
			"testq %%rcx, %%rcx\n\t"
			"jz 2f\n\t"
			"rdsspq %%rdx\n\t"
			"movq %%rdx, 0x130(%%rdi)\n\t"
			"rstorssp -0x8(%%rcx)\n\t"
			"saveprevssp\n\t"
			"2:\n\t"
		#endif
		"movq 0x38(%%rsi), %%r15\n\t"
		"movq 0x30(%%rsi), %%r14\n\t"
		"movq 0x28(%%rsi), %%r13\n\t"
		"movq 0x20(%%rsi), %%r12\n\t"
		"movq 0x18(%%rsi), %%rbx\n\t"
		"movq 0x10(%%rsi), %%rbp\n\t"
		"movq 0x08(%%rsi), %%rsp\n\t"
		"movq 0x138(%%rsi), %%rdi\n\t"
		"jmp *0x00(%%rsi)\n\t"
		"1:\n\t"
		/* TinyCC 汇编器不识别 endbr64，直接编码同一条 CET 指令。 */
		".byte 0xf3, 0x0f, 0x1e, 0xfa\n\t"
		: "+D"(pFrom), "+S"(pTo)
		:
		: XRT_CO_X64_CLOBBERS
	);
}

#undef XRT_CO_X64_CLOBBERS



#elif defined(__aarch64__)

/* ARM64 AAPCS64 同时保存整数和 SIMD 非易失寄存器。 */
XRT_CO_CONTEXT_NOINLINE
void __xrtCoContextSwap(xrt_co_context* pFrom, xrt_co_context* pTo)
{
	__asm__ volatile (
		"mov x9, %0\n\t"
		"mov x10, %1\n\t"
		"adr x2, 1f\n\t"
		"mov x3, sp\n\t"
		"stp x2,  x3,  [x9, #0x00]\n\t"
		"stp x19, x20, [x9, #0x10]\n\t"
		"stp x21, x22, [x9, #0x20]\n\t"
		"stp x23, x24, [x9, #0x30]\n\t"
		"stp x25, x26, [x9, #0x40]\n\t"
		"stp x27, x28, [x9, #0x50]\n\t"
		"stp x29, x30, [x9, #0x60]\n\t"
		"stp q8,  q9,  [x9, #0x70]\n\t"
		"stp q10, q11, [x9, #0x90]\n\t"
		"stp q12, q13, [x9, #0xB0]\n\t"
		"stp q14, q15, [x9, #0xD0]\n\t"
		#if defined(XRT_CO_SHADOW_CALL_STACK)
			"str x18, [x9, #0xF0]\n\t"
		#endif
		"ldp q14, q15, [x10, #0xD0]\n\t"
		"ldp q12, q13, [x10, #0xB0]\n\t"
		"ldp q10, q11, [x10, #0x90]\n\t"
		"ldp q8,  q9,  [x10, #0x70]\n\t"
		"ldp x29, x30, [x10, #0x60]\n\t"
		"ldp x27, x28, [x10, #0x50]\n\t"
		"ldp x25, x26, [x10, #0x40]\n\t"
		"ldp x23, x24, [x10, #0x30]\n\t"
		"ldp x21, x22, [x10, #0x20]\n\t"
		"ldp x19, x20, [x10, #0x10]\n\t"
		"ldp x2,  x3,  [x10, #0x00]\n\t"
		"ldr x0, [x10, #0x138]\n\t"
		#if defined(XRT_CO_SHADOW_CALL_STACK)
			"ldr x18, [x10, #0xF0]\n\t"
		#endif
		"mov sp, x3\n\t"
		/* x16 使 BTI 将上下文跳转视为兼容函数调用的尾跳转。 */
		"mov x16, x2\n\t"
		"br x16\n\t"
		"1:\n\t"
		/* BTI JC 同时接受首次进入和恢复上下文的间接分支类型。 */
		".inst 0xd50324df\n\t"
		: "+r"(pFrom), "+r"(pTo)
		:
		: "memory", "cc", "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
		  "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16",
		  "x17", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
		  "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
		  "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31"
	);
}



#elif defined(__riscv) && (__riscv_xlen == 64)

#if (defined(__riscv_flen) && (__riscv_flen == 32)) || \
	defined(__riscv_float_abi_single)
	#define XRT_CO_RV_FP_SAVE \
		"fsw fs0,  0x70(t0)\n\t" \
		"fsw fs1,  0x78(t0)\n\t" \
		"fsw fs2,  0x80(t0)\n\t" \
		"fsw fs3,  0x88(t0)\n\t" \
		"fsw fs4,  0x90(t0)\n\t" \
		"fsw fs5,  0x98(t0)\n\t" \
		"fsw fs6,  0xA0(t0)\n\t" \
		"fsw fs7,  0xA8(t0)\n\t" \
		"fsw fs8,  0xB0(t0)\n\t" \
		"fsw fs9,  0xB8(t0)\n\t" \
		"fsw fs10, 0xC0(t0)\n\t" \
		"fsw fs11, 0xC8(t0)\n\t"
	#define XRT_CO_RV_FP_LOAD \
		"flw fs11, 0xC8(t1)\n\t" \
		"flw fs10, 0xC0(t1)\n\t" \
		"flw fs9,  0xB8(t1)\n\t" \
		"flw fs8,  0xB0(t1)\n\t" \
		"flw fs7,  0xA8(t1)\n\t" \
		"flw fs6,  0xA0(t1)\n\t" \
		"flw fs5,  0x98(t1)\n\t" \
		"flw fs4,  0x90(t1)\n\t" \
		"flw fs3,  0x88(t1)\n\t" \
		"flw fs2,  0x80(t1)\n\t" \
		"flw fs1,  0x78(t1)\n\t" \
		"flw fs0,  0x70(t1)\n\t"
#elif (defined(__riscv_flen) && (__riscv_flen >= 64)) || \
	defined(__riscv_float_abi_double) || defined(__riscv_float_abi_quad)
	#define XRT_CO_RV_FP_SAVE \
		"fsd fs0,  0x70(t0)\n\t" \
		"fsd fs1,  0x78(t0)\n\t" \
		"fsd fs2,  0x80(t0)\n\t" \
		"fsd fs3,  0x88(t0)\n\t" \
		"fsd fs4,  0x90(t0)\n\t" \
		"fsd fs5,  0x98(t0)\n\t" \
		"fsd fs6,  0xA0(t0)\n\t" \
		"fsd fs7,  0xA8(t0)\n\t" \
		"fsd fs8,  0xB0(t0)\n\t" \
		"fsd fs9,  0xB8(t0)\n\t" \
		"fsd fs10, 0xC0(t0)\n\t" \
		"fsd fs11, 0xC8(t0)\n\t"
	#define XRT_CO_RV_FP_LOAD \
		"fld fs11, 0xC8(t1)\n\t" \
		"fld fs10, 0xC0(t1)\n\t" \
		"fld fs9,  0xB8(t1)\n\t" \
		"fld fs8,  0xB0(t1)\n\t" \
		"fld fs7,  0xA8(t1)\n\t" \
		"fld fs6,  0xA0(t1)\n\t" \
		"fld fs5,  0x98(t1)\n\t" \
		"fld fs4,  0x90(t1)\n\t" \
		"fld fs3,  0x88(t1)\n\t" \
		"fld fs2,  0x80(t1)\n\t" \
		"fld fs1,  0x78(t1)\n\t" \
		"fld fs0,  0x70(t1)\n\t"
#else
	#define XRT_CO_RV_FP_SAVE ""
	#define XRT_CO_RV_FP_LOAD ""
#endif



/* RISC-V LP64 保存整数和当前浮点 ABI 要求的非易失寄存器。 */
XRT_CO_CONTEXT_NOINLINE
void __xrtCoContextSwap(xrt_co_context* pFrom, xrt_co_context* pTo)
{
	__asm__ volatile (
		"mv t0, %0\n\t"
		"mv t1, %1\n\t"
		"la t2, 1f\n\t"
		"sd t2,  0x00(t0)\n\t"
		"sd sp,  0x08(t0)\n\t"
		"sd s0,  0x10(t0)\n\t"
		"sd s1,  0x18(t0)\n\t"
		"sd s2,  0x20(t0)\n\t"
		"sd s3,  0x28(t0)\n\t"
		"sd s4,  0x30(t0)\n\t"
		"sd s5,  0x38(t0)\n\t"
		"sd s6,  0x40(t0)\n\t"
		"sd s7,  0x48(t0)\n\t"
		"sd s8,  0x50(t0)\n\t"
		"sd s9,  0x58(t0)\n\t"
		"sd s10, 0x60(t0)\n\t"
		"sd s11, 0x68(t0)\n\t"
		"sd ra,  0xD0(t0)\n\t"
		XRT_CO_RV_FP_SAVE
		XRT_CO_RV_FP_LOAD
		"ld ra,  0xD0(t1)\n\t"
		"ld s11, 0x68(t1)\n\t"
		"ld s10, 0x60(t1)\n\t"
		"ld s9,  0x58(t1)\n\t"
		"ld s8,  0x50(t1)\n\t"
		"ld s7,  0x48(t1)\n\t"
		"ld s6,  0x40(t1)\n\t"
		"ld s5,  0x38(t1)\n\t"
		"ld s4,  0x30(t1)\n\t"
		"ld s3,  0x28(t1)\n\t"
		"ld s2,  0x20(t1)\n\t"
		"ld s1,  0x18(t1)\n\t"
		"ld s0,  0x10(t1)\n\t"
		"ld sp,  0x08(t1)\n\t"
		"ld a0,  0x138(t1)\n\t"
		"ld t2,  0x00(t1)\n\t"
		"jr t2\n\t"
		"1:\n\t"
		: "+r"(pFrom), "+r"(pTo)
		:
		: "memory", "t0", "t1", "t2", "t3", "t4", "t5", "t6",
		  "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
		  "ft0", "ft1", "ft2", "ft3", "ft4", "ft5", "ft6", "ft7",
		  "ft8", "ft9", "ft10", "ft11", "fa0", "fa1", "fa2", "fa3",
		  "fa4", "fa5", "fa6", "fa7"
	);
}

#undef XRT_CO_RV_FP_SAVE
#undef XRT_CO_RV_FP_LOAD



#elif defined(__loongarch64)

#if (defined(__loongarch_frlen) && (__loongarch_frlen == 32)) || \
	defined(__loongarch_single_float)
	#define XRT_CO_LA_FP_SAVE \
		"fst.s $fs0, $t0, 0x60\n\t" \
		"fst.s $fs1, $t0, 0x68\n\t" \
		"fst.s $fs2, $t0, 0x70\n\t" \
		"fst.s $fs3, $t0, 0x78\n\t" \
		"fst.s $fs4, $t0, 0x80\n\t" \
		"fst.s $fs5, $t0, 0x88\n\t" \
		"fst.s $fs6, $t0, 0x90\n\t" \
		"fst.s $fs7, $t0, 0x98\n\t"
	#define XRT_CO_LA_FP_LOAD \
		"fld.s $fs7, $t1, 0x98\n\t" \
		"fld.s $fs6, $t1, 0x90\n\t" \
		"fld.s $fs5, $t1, 0x88\n\t" \
		"fld.s $fs4, $t1, 0x80\n\t" \
		"fld.s $fs3, $t1, 0x78\n\t" \
		"fld.s $fs2, $t1, 0x70\n\t" \
		"fld.s $fs1, $t1, 0x68\n\t" \
		"fld.s $fs0, $t1, 0x60\n\t"
#elif (defined(__loongarch_frlen) && (__loongarch_frlen >= 64)) || \
	defined(__loongarch_double_float)
	#define XRT_CO_LA_FP_SAVE \
		"fst.d $fs0, $t0, 0x60\n\t" \
		"fst.d $fs1, $t0, 0x68\n\t" \
		"fst.d $fs2, $t0, 0x70\n\t" \
		"fst.d $fs3, $t0, 0x78\n\t" \
		"fst.d $fs4, $t0, 0x80\n\t" \
		"fst.d $fs5, $t0, 0x88\n\t" \
		"fst.d $fs6, $t0, 0x90\n\t" \
		"fst.d $fs7, $t0, 0x98\n\t"
	#define XRT_CO_LA_FP_LOAD \
		"fld.d $fs7, $t1, 0x98\n\t" \
		"fld.d $fs6, $t1, 0x90\n\t" \
		"fld.d $fs5, $t1, 0x88\n\t" \
		"fld.d $fs4, $t1, 0x80\n\t" \
		"fld.d $fs3, $t1, 0x78\n\t" \
		"fld.d $fs2, $t1, 0x70\n\t" \
		"fld.d $fs1, $t1, 0x68\n\t" \
		"fld.d $fs0, $t1, 0x60\n\t"
#else
	#define XRT_CO_LA_FP_SAVE ""
	#define XRT_CO_LA_FP_LOAD ""
#endif



/* LoongArch64 LP64 保存整数和当前浮点 ABI 的非易失寄存器。 */
XRT_CO_CONTEXT_NOINLINE
void __xrtCoContextSwap(xrt_co_context* pFrom, xrt_co_context* pTo)
{
	__asm__ volatile (
		"move $t0, %0\n\t"
		"move $t1, %1\n\t"
		"la.local $t2, 1f\n\t"
		"st.d $t2, $t0, 0x00\n\t"
		"st.d $sp, $t0, 0x08\n\t"
		"st.d $fp, $t0, 0x10\n\t"
		"st.d $s0, $t0, 0x18\n\t"
		"st.d $s1, $t0, 0x20\n\t"
		"st.d $s2, $t0, 0x28\n\t"
		"st.d $s3, $t0, 0x30\n\t"
		"st.d $s4, $t0, 0x38\n\t"
		"st.d $s5, $t0, 0x40\n\t"
		"st.d $s6, $t0, 0x48\n\t"
		"st.d $s7, $t0, 0x50\n\t"
		"st.d $s8, $t0, 0x58\n\t"
		"st.d $ra, $t0, 0xA0\n\t"
		XRT_CO_LA_FP_SAVE
		XRT_CO_LA_FP_LOAD
		"ld.d $ra, $t1, 0xA0\n\t"
		"ld.d $s8, $t1, 0x58\n\t"
		"ld.d $s7, $t1, 0x50\n\t"
		"ld.d $s6, $t1, 0x48\n\t"
		"ld.d $s5, $t1, 0x40\n\t"
		"ld.d $s4, $t1, 0x38\n\t"
		"ld.d $s3, $t1, 0x30\n\t"
		"ld.d $s2, $t1, 0x28\n\t"
		"ld.d $s1, $t1, 0x20\n\t"
		"ld.d $s0, $t1, 0x18\n\t"
		"ld.d $fp, $t1, 0x10\n\t"
		"ld.d $sp, $t1, 0x08\n\t"
		"ld.d $a0, $t1, 0x138\n\t"
		"ld.d $t2, $t1, 0x00\n\t"
		"jr $t2\n\t"
		"1:\n\t"
		: "+r"(pFrom), "+r"(pTo)
		:
		: "memory", "$a0", "$a1", "$a2", "$a3", "$a4", "$a5",
		  "$a6", "$a7", "$t0", "$t1", "$t2", "$t3", "$t4",
		  "$t5", "$t6", "$t7", "$t8", "$f0", "$f1", "$f2", "$f3",
		  "$f4", "$f5", "$f6", "$f7", "$f8", "$f9", "$f10", "$f11",
		  "$f12", "$f13", "$f14", "$f15", "$f16", "$f17", "$f18",
		  "$f19", "$f20", "$f21", "$f22", "$f23"
	);
}

#undef XRT_CO_LA_FP_SAVE
#undef XRT_CO_LA_FP_LOAD



#else
	#error "XRT coroutine context backend is not available for this POSIX architecture"
#endif

#undef XRT_CO_CONTEXT_NOINLINE

#endif
