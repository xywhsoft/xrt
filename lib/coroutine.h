



/* ================================ 协程后端自动选择 ================================ */

#if defined(_WIN32) || defined(_WIN64)
	// Windows: 全部编译器统一用 Fiber API
	#define __XRT_CO_FIBER

#elif (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
	// Linux: GCC / Clang 用内联汇编
	#if defined(__x86_64__) || defined(_M_X64)
		#define __XRT_CO_ASM_X64
	#elif defined(__aarch64__)
		#define __XRT_CO_ASM_ARM64
	#elif defined(__riscv) && (__riscv_xlen == 64)
		#define __XRT_CO_ASM_RV64
	#elif defined(__loongarch64)
		#define __XRT_CO_ASM_LA64
	#else
		#define __XRT_CO_UCONTEXT
	#endif

#else
	// TCC / 其他编译器: ucontext 兜底
	#define __XRT_CO_UCONTEXT
#endif

#ifdef __XRT_CO_UCONTEXT
	#include <ucontext.h>
#endif



/* ================================ 线程局部协程状态 ================================ */

#ifdef __XRT_CO_FIBER
	static ptr __xrt_co_main_fiber = NULL;
	static int __xrt_co_fiber_converted = 0;
#elif defined(__XRT_CO_UCONTEXT)
	static ucontext_t __xrt_co_main_uctx;
#else
	static __xrt_co_ctx __xrt_co_main_ctx;
#endif

// 当前正在运行的协程
static xcoro __xrt_co_current = NULL;



/* ================================ 时间辅助函数 ================================ */

static int64 __xrt_co_time_ms()
{
	#if defined(_WIN32) || defined(_WIN64)
		return (int64)GetTickCount64();
	#else
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return (int64)ts.tv_sec * 1000 + (int64)ts.tv_nsec / 1000000;
	#endif
}

static void __xrt_co_sleep_ms(int iMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iMs);
	#else
		struct timespec ts;
		ts.tv_sec = iMs / 1000;
		ts.tv_nsec = (iMs % 1000) * 1000000L;
		nanosleep(&ts, NULL);
	#endif
}



/* ================================ 后端实现: Windows Fiber ================================ */

#ifdef __XRT_CO_FIBER

static void CALLBACK __xrt_co_fiber_entry(LPVOID lpParameter)
{
	xcoro pCo = (xcoro)lpParameter;
	pCo->pfnEntry(pCo->pParam);
	pCo->iState = XRT_CO_DEAD;
	SwitchToFiber(__xrt_co_main_fiber);
}

static void __xrt_co_init_ctx(xcoro pCo)
{
	pCo->__hFiber = CreateFiber(pCo->iStackSize, __xrt_co_fiber_entry, pCo);
}

static void __xrt_co_swap_to_co(xcoro pCo)
{
	if ( !__xrt_co_fiber_converted ) {
		__xrt_co_main_fiber = ConvertThreadToFiber(NULL);
		__xrt_co_fiber_converted = 1;
	}
	SwitchToFiber(pCo->__hFiber);
}

static void __xrt_co_swap_to_main()
{
	SwitchToFiber(__xrt_co_main_fiber);
}

#endif



/* ================================ 后端实现: x86_64 内联汇编 ================================ */

#ifdef __XRT_CO_ASM_X64

/*
	x86_64 System V ABI callee-saved 寄存器:
	arrReg[0] = 恢复点地址 (rip)
	arrReg[1] = rsp
	arrReg[2] = rbp
	arrReg[3] = rbx
	arrReg[4] = r12
	arrReg[5] = r13
	arrReg[6] = r14
	arrReg[7] = r15
*/

static void __xrt_co_swap(__xrt_co_ctx* pFrom, __xrt_co_ctx* pTo)
{
	__asm__ volatile (
		"leaq 1f(%%rip), %%rax\n\t"
		"movq %%rax, 0x00(%0)\n\t"
		"movq %%rsp, 0x08(%0)\n\t"
		"movq %%rbp, 0x10(%0)\n\t"
		"movq %%rbx, 0x18(%0)\n\t"
		"movq %%r12, 0x20(%0)\n\t"
		"movq %%r13, 0x28(%0)\n\t"
		"movq %%r14, 0x30(%0)\n\t"
		"movq %%r15, 0x38(%0)\n\t"
		"movq 0x38(%1), %%r15\n\t"
		"movq 0x30(%1), %%r14\n\t"
		"movq 0x28(%1), %%r13\n\t"
		"movq 0x20(%1), %%r12\n\t"
		"movq 0x18(%1), %%rbx\n\t"
		"movq 0x10(%1), %%rbp\n\t"
		"movq 0x08(%1), %%rsp\n\t"
		"jmpq *0x00(%1)\n\t"
		"1:\n\t"
		: : "r"(pFrom), "r"(pTo)
		: "memory", "rax", "rcx", "rdx", "rsi", "rdi",
		  "r8", "r9", "r10", "r11"
	);
}

#endif



/* ================================ 后端实现: ARM64 内联汇编 ================================ */

#ifdef __XRT_CO_ASM_ARM64

/*
	ARM64 AAPCS64 callee-saved 寄存器:
	arrReg[0] = 恢复点地址
	arrReg[1] = sp
	arrReg[2..3] = x19, x20
	arrReg[4..5] = x21, x22
	arrReg[6..7] = x23, x24
	arrReg[8..9] = x25, x26
	arrReg[10..11] = x27, x28
	arrReg[12..13] = x29 (fp), x30 (lr)
*/

static void __xrt_co_swap(__xrt_co_ctx* pFrom, __xrt_co_ctx* pTo)
{
	__asm__ volatile (
		"adr x2, 1f\n\t"
		"mov x3, sp\n\t"
		"stp x2,  x3,  [%0, #0x00]\n\t"
		"stp x19, x20, [%0, #0x10]\n\t"
		"stp x21, x22, [%0, #0x20]\n\t"
		"stp x23, x24, [%0, #0x30]\n\t"
		"stp x25, x26, [%0, #0x40]\n\t"
		"stp x27, x28, [%0, #0x50]\n\t"
		"stp x29, x30, [%0, #0x60]\n\t"
		"ldp x29, x30, [%1, #0x60]\n\t"
		"ldp x27, x28, [%1, #0x50]\n\t"
		"ldp x25, x26, [%1, #0x40]\n\t"
		"ldp x23, x24, [%1, #0x30]\n\t"
		"ldp x21, x22, [%1, #0x20]\n\t"
		"ldp x19, x20, [%1, #0x10]\n\t"
		"ldp x2,  x3,  [%1, #0x00]\n\t"
		"mov sp, x3\n\t"
		"br x2\n\t"
		"1:\n\t"
		: : "r"(pFrom), "r"(pTo)
		: "memory", "x2", "x3"
	);
}

#endif



/* ================================ 后端实现: RISC-V 64 内联汇编 ================================ */

#ifdef __XRT_CO_ASM_RV64

/*
	RISC-V LP64 callee-saved 寄存器:
	arrReg[0] = 恢复点地址 (ra/pc)
	arrReg[1] = sp
	arrReg[2] = s0 (fp)
	arrReg[3..13] = s1 ~ s11
*/

static void __xrt_co_swap(__xrt_co_ctx* pFrom, __xrt_co_ctx* pTo)
{
	__asm__ volatile (
		"la t0, 1f\n\t"
		"sd t0,  0x00(%0)\n\t"
		"sd sp,  0x08(%0)\n\t"
		"sd s0,  0x10(%0)\n\t"
		"sd s1,  0x18(%0)\n\t"
		"sd s2,  0x20(%0)\n\t"
		"sd s3,  0x28(%0)\n\t"
		"sd s4,  0x30(%0)\n\t"
		"sd s5,  0x38(%0)\n\t"
		"sd s6,  0x40(%0)\n\t"
		"sd s7,  0x48(%0)\n\t"
		"sd s8,  0x50(%0)\n\t"
		"sd s9,  0x58(%0)\n\t"
		"sd s10, 0x60(%0)\n\t"
		"sd s11, 0x68(%0)\n\t"
		"ld s11, 0x68(%1)\n\t"
		"ld s10, 0x60(%1)\n\t"
		"ld s9,  0x58(%1)\n\t"
		"ld s8,  0x50(%1)\n\t"
		"ld s7,  0x48(%1)\n\t"
		"ld s6,  0x40(%1)\n\t"
		"ld s5,  0x38(%1)\n\t"
		"ld s4,  0x30(%1)\n\t"
		"ld s3,  0x28(%1)\n\t"
		"ld s2,  0x20(%1)\n\t"
		"ld s1,  0x18(%1)\n\t"
		"ld s0,  0x10(%1)\n\t"
		"ld sp,  0x08(%1)\n\t"
		"ld t0,  0x00(%1)\n\t"
		"jr t0\n\t"
		"1:\n\t"
		: : "r"(pFrom), "r"(pTo)
		: "memory", "t0", "t1", "t2", "t3", "t4", "t5", "t6",
		  "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
	);
}

#endif



/* ================================ 后端实现: LoongArch64 内联汇编 ================================ */

#ifdef __XRT_CO_ASM_LA64

/*
	LoongArch64 LP64 callee-saved 寄存器:
	arrReg[0] = 恢复点地址 (ra)
	arrReg[1] = sp ($r3)
	arrReg[2] = fp ($r22)
	arrReg[3..11] = s0~s8 ($r23~$r31)
*/

static void __xrt_co_swap(__xrt_co_ctx* pFrom, __xrt_co_ctx* pTo)
{
	__asm__ volatile (
		"la.local $t0, 1f\n\t"
		"st.d $t0,  %0, 0x00\n\t"
		"st.d $sp,  %0, 0x08\n\t"
		"st.d $fp,  %0, 0x10\n\t"
		"st.d $s0,  %0, 0x18\n\t"
		"st.d $s1,  %0, 0x20\n\t"
		"st.d $s2,  %0, 0x28\n\t"
		"st.d $s3,  %0, 0x30\n\t"
		"st.d $s4,  %0, 0x38\n\t"
		"st.d $s5,  %0, 0x40\n\t"
		"st.d $s6,  %0, 0x48\n\t"
		"st.d $s7,  %0, 0x50\n\t"
		"st.d $s8,  %0, 0x58\n\t"
		"ld.d $s8,  %1, 0x58\n\t"
		"ld.d $s7,  %1, 0x50\n\t"
		"ld.d $s6,  %1, 0x48\n\t"
		"ld.d $s5,  %1, 0x40\n\t"
		"ld.d $s4,  %1, 0x38\n\t"
		"ld.d $s3,  %1, 0x30\n\t"
		"ld.d $s2,  %1, 0x28\n\t"
		"ld.d $s1,  %1, 0x20\n\t"
		"ld.d $s0,  %1, 0x18\n\t"
		"ld.d $fp,  %1, 0x10\n\t"
		"ld.d $sp,  %1, 0x08\n\t"
		"ld.d $t0,  %1, 0x00\n\t"
		"jr $t0\n\t"
		"1:\n\t"
		: : "r"(pFrom), "r"(pTo)
		: "memory", "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8"
	);
}

#endif



/* ================================ 汇编后端: 入口 + 初始化 + swap 包装 ================================ */

#if defined(__XRT_CO_ASM_X64) || defined(__XRT_CO_ASM_ARM64) || defined(__XRT_CO_ASM_RV64) || defined(__XRT_CO_ASM_LA64)

// 协程入口包装（不接收参数，从全局状态读取当前协程）
static void __xrt_co_asm_entry()
{
	xcoro pCo = __xrt_co_current;
	pCo->pfnEntry(pCo->pParam);
	pCo->iState = XRT_CO_DEAD;
	__xrt_co_swap(&pCo->__tCtx, &__xrt_co_main_ctx);
}

static void __xrt_co_init_ctx(xcoro pCo)
{
	uint8* pStackTop = (uint8*)pCo->__pStack + pCo->iStackSize;
	
	// 对齐到 16 字节边界
	pStackTop = (uint8*)((uintptr)pStackTop & ~(uintptr)0x0F);
	
	#ifdef __XRT_CO_ASM_X64
		// x86_64: 模拟 call 指令压入返回地址的效果 (rsp mod 16 == 8)
		pStackTop -= 8;
	#endif
	
	memset(&pCo->__tCtx, 0, sizeof(__xrt_co_ctx));
	
	// arrReg[0] = 入口点, arrReg[1] = 栈顶
	pCo->__tCtx.arrReg[0] = (ptr)__xrt_co_asm_entry;
	pCo->__tCtx.arrReg[1] = (ptr)pStackTop;
}

static void __xrt_co_swap_to_co(xcoro pCo)
{
	__xrt_co_swap(&__xrt_co_main_ctx, &pCo->__tCtx);
}

static void __xrt_co_swap_to_main()
{
	__xrt_co_swap(&__xrt_co_current->__tCtx, &__xrt_co_main_ctx);
}

#endif



/* ================================ ucontext 后端: 入口 + 初始化 + swap 包装 ================================ */

#ifdef __XRT_CO_UCONTEXT

static void __xrt_co_ucontext_entry()
{
	xcoro pCo = __xrt_co_current;
	pCo->pfnEntry(pCo->pParam);
	pCo->iState = XRT_CO_DEAD;
	swapcontext((ucontext_t*)pCo->__hFiber, &__xrt_co_main_uctx);
}

static void __xrt_co_init_ctx(xcoro pCo)
{
	// ucontext_t 体积较大，动态分配并将指针存入 __hFiber
	ucontext_t* pCtx = (ucontext_t*)xrtMalloc(sizeof(ucontext_t));
	pCo->__hFiber = pCtx;
	getcontext(pCtx);
	pCtx->uc_stack.ss_sp = pCo->__pStack;
	pCtx->uc_stack.ss_size = pCo->iStackSize;
	pCtx->uc_link = NULL;
	makecontext(pCtx, (void(*)())__xrt_co_ucontext_entry, 0);
}

static void __xrt_co_swap_to_co(xcoro pCo)
{
	swapcontext(&__xrt_co_main_uctx, (ucontext_t*)pCo->__hFiber);
}

static void __xrt_co_swap_to_main()
{
	swapcontext((ucontext_t*)__xrt_co_current->__hFiber, &__xrt_co_main_uctx);
}

#endif



/* ================================ 基础协程 API ================================ */

XXAPI xcoro xrtCoCreate(xco_entry pfnEntry, ptr pParam, size_t iStackSize)
{
	if ( !pfnEntry ) return NULL;
	
	// 栈大小处理
	if ( iStackSize == 0 ) iStackSize = XRT_CO_STACK_DEFAULT;
	if ( iStackSize < XRT_CO_STACK_MIN ) iStackSize = XRT_CO_STACK_MIN;
	if ( iStackSize > XRT_CO_STACK_MAX ) iStackSize = XRT_CO_STACK_MAX;
	
	// 分配协程结构体
	xcoro pCo = (xcoro)xrtMalloc(sizeof(xcoro_struct));
	if ( !pCo ) return NULL;
	
	memset(pCo, 0, sizeof(xcoro_struct));
	pCo->iState = XRT_CO_READY;
	pCo->pfnEntry = pfnEntry;
	pCo->pParam = pParam;
	pCo->iStackSize = iStackSize;
	
	// 分配栈内存（Fiber 后端由系统分配栈，不需要手动分配）
	#ifndef __XRT_CO_FIBER
		pCo->__pStack = xrtMalloc(iStackSize);
		if ( !pCo->__pStack ) {
			xrtFree(pCo);
			return NULL;
		}
	#endif
	
	// 初始化上下文
	__xrt_co_init_ctx(pCo);
	
	return pCo;
}


XXAPI void xrtCoDestroy(xcoro pCo)
{
	if ( !pCo ) return;
	
	#ifdef __XRT_CO_FIBER
		if ( pCo->__hFiber ) {
			DeleteFiber(pCo->__hFiber);
		}
	#elif defined(__XRT_CO_UCONTEXT)
		if ( pCo->__hFiber ) {
			xrtFree(pCo->__hFiber);  // 释放 ucontext_t
		}
		if ( pCo->__pStack ) {
			xrtFree(pCo->__pStack);
		}
	#else
		if ( pCo->__pStack ) {
			xrtFree(pCo->__pStack);
		}
	#endif
	
	xrtFree(pCo);
}


XXAPI bool xrtCoResume(xcoro pCo)
{
	if ( !pCo ) return false;
	
	// 只能恢复 READY 或 SUSPENDED 状态的协程
	if ( (pCo->iState != XRT_CO_READY) && (pCo->iState != XRT_CO_SUSPENDED) ) return false;
	
	// 不能在协程内部调用 Resume（不支持嵌套）
	if ( __xrt_co_current != NULL ) return false;
	
	pCo->__iWakeTime = 0;
	__xrt_co_current = pCo;
	pCo->iState = XRT_CO_RUNNING;
	
	__xrt_co_swap_to_co(pCo);
	
	// 从这里恢复意味着协程 yield 了或者结束了
	__xrt_co_current = NULL;
	return true;
}


XXAPI void xrtCoYield()
{
	xcoro pCo = __xrt_co_current;
	if ( !pCo ) return;
	
	pCo->iState = XRT_CO_SUSPENDED;
	__xrt_co_swap_to_main();
}


XXAPI int xrtCoGetState(xcoro pCo)
{
	if ( !pCo ) return XRT_CO_DEAD;
	return pCo->iState;
}

XXAPI xcoro xrtCoGetCurrent()
{
	return __xrt_co_current;
}

XXAPI void xrtCoSetUserData(xcoro pCo, ptr pData)
{
	if ( pCo ) pCo->pUserData = pData;
}

XXAPI ptr xrtCoGetUserData(xcoro pCo)
{
	if ( !pCo ) return NULL;
	return pCo->pUserData;
}



/* ================================ 协程调度器 ================================ */

#define __XRT_CO_SCHED_INIT_CAP  16

struct xrt_co_scheduler {
	xcoro* arrCoros;
	int iCount;
	int iCapacity;
	int iAlive;
};


XXAPI xcosched* xrtCoSchedCreate()
{
	xcosched* pSched = (xcosched*)xrtMalloc(sizeof(xcosched));
	if ( !pSched ) return NULL;
	
	pSched->arrCoros = (xcoro*)xrtMalloc(sizeof(xcoro) * __XRT_CO_SCHED_INIT_CAP);
	if ( !pSched->arrCoros ) {
		xrtFree(pSched);
		return NULL;
	}
	
	pSched->iCount = 0;
	pSched->iCapacity = __XRT_CO_SCHED_INIT_CAP;
	pSched->iAlive = 0;
	return pSched;
}


XXAPI void xrtCoSchedDestroy(xcosched* pSched)
{
	if ( !pSched ) return;
	
	// 销毁所有协程
	for ( int i = 0; i < pSched->iCount; i++ ) {
		if ( pSched->arrCoros[i] ) {
			xrtCoDestroy(pSched->arrCoros[i]);
		}
	}
	
	xrtFree(pSched->arrCoros);
	xrtFree(pSched);
}


XXAPI xcoro xrtCoSchedSpawn(xcosched* pSched, xco_entry pfnEntry, ptr pParam, size_t iStackSize)
{
	if ( !pSched ) return NULL;
	
	// 扩容检查
	if ( pSched->iCount >= pSched->iCapacity ) {
		int iNewCap = pSched->iCapacity * 2;
		xcoro* pNewArr = (xcoro*)xrtRealloc(pSched->arrCoros, sizeof(xcoro) * iNewCap);
		if ( !pNewArr ) return NULL;
		pSched->arrCoros = pNewArr;
		pSched->iCapacity = iNewCap;
	}
	
	xcoro pCo = xrtCoCreate(pfnEntry, pParam, iStackSize);
	if ( !pCo ) return NULL;
	
	pCo->__pSched = pSched;
	pSched->arrCoros[pSched->iCount] = pCo;
	pSched->iCount++;
	pSched->iAlive++;
	
	return pCo;
}


XXAPI bool xrtCoSchedStep(xcosched* pSched)
{
	if ( !pSched ) return false;
	if ( pSched->iAlive <= 0 ) return false;
	
	int64 iNow = __xrt_co_time_ms();
	
	for ( int i = 0; i < pSched->iCount; i++ ) {
		xcoro pCo = pSched->arrCoros[i];
		if ( !pCo ) continue;
		
		// 已结束的协程: 更新计数
		if ( pCo->iState == XRT_CO_DEAD ) {
			pSched->iAlive--;
			xrtCoDestroy(pCo);
			pSched->arrCoros[i] = NULL;
			continue;
		}
		
		// 只处理可恢复的协程
		if ( (pCo->iState != XRT_CO_READY) && (pCo->iState != XRT_CO_SUSPENDED) ) continue;
		
		// 检查休眠定时器
		if ( (pCo->__iWakeTime > 0) && (iNow < pCo->__iWakeTime) ) continue;
		
		// 恢复协程
		xrtCoResume(pCo);
		
		// Resume 返回后，如果协程已结束，下一轮 Step 会清理
	}
	
	return pSched->iAlive > 0;
}


XXAPI void xrtCoSchedRun(xcosched* pSched)
{
	if ( !pSched ) return;
	
	while ( pSched->iAlive > 0 ) {
		int iAliveBefore = pSched->iAlive;
		bool bResult = xrtCoSchedStep(pSched);
		if ( !bResult ) break;
		
		// 检查是否所有存活协程都在休眠
		// 如果 Step 没有减少存活数且还有存活协程，可能都在等待
		// 短暂休眠避免空转
		int64 iNow = __xrt_co_time_ms();
		bool bAllSleeping = true;
		for ( int i = 0; i < pSched->iCount; i++ ) {
			xcoro pCo = pSched->arrCoros[i];
			if ( !pCo ) continue;
			if ( pCo->iState == XRT_CO_DEAD ) continue;
			if ( (pCo->__iWakeTime <= 0) || (iNow >= pCo->__iWakeTime) ) {
				bAllSleeping = false;
				break;
			}
		}
		if ( bAllSleeping ) {
			__xrt_co_sleep_ms(1);
		}
	}
}


XXAPI int xrtCoSchedGetAlive(xcosched* pSched)
{
	if ( !pSched ) return 0;
	return pSched->iAlive;
}


XXAPI void xrtCoSleep(uint32 iMs)
{
	xcoro pCo = __xrt_co_current;
	if ( !pCo ) return;
	
	pCo->__iWakeTime = __xrt_co_time_ms() + (int64)iMs;
	xrtCoYield();
}


