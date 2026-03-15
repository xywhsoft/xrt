


/* ================================ 协程后端自动选择 ================================ */

/*
	backend 策略说明：
	1. 单头文件仍是 XRT 的第一优先级约束之一
	2. production backend 优先使用 header 内的 inline asm
	3. 主线只保留 production backend；不再在主线内编译 archive fallback
	4. 平台支持按 ABI + 编译器家族逐步扩展，不对未验证目标做超前承诺
*/

#ifndef XRT_CO_REQUIRE_PRODUCTION_BACKEND
	#define XRT_CO_REQUIRE_PRODUCTION_BACKEND	1
#endif

#define __XRT_CO_BACKEND_TIER_PRODUCTION	2
#define __XRT_CO_BACKEND_STYLE_INLINE_ASM	2

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
	#if (defined(_WIN64)) && (defined(__x86_64__) || defined(_M_X64))
		#define __XRT_CO_ASM_X64_WIN
		#define __XRT_CO_BACKEND_NAME	"asm-x64-win64"
		#define __XRT_CO_BACKEND_TIER	__XRT_CO_BACKEND_TIER_PRODUCTION
		#define __XRT_CO_BACKEND_STYLE	__XRT_CO_BACKEND_STYLE_INLINE_ASM
	#elif !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
		#define __XRT_CO_ASM_X64
		#define __XRT_CO_BACKEND_NAME	"asm-x64-sysv"
		#define __XRT_CO_BACKEND_TIER	__XRT_CO_BACKEND_TIER_PRODUCTION
		#define __XRT_CO_BACKEND_STYLE	__XRT_CO_BACKEND_STYLE_INLINE_ASM
	#elif !defined(_WIN32) && !defined(_WIN64) && defined(__aarch64__)
		#define __XRT_CO_ASM_ARM64
		#define __XRT_CO_BACKEND_NAME	"asm-arm64-aapcs64"
		#define __XRT_CO_BACKEND_TIER	__XRT_CO_BACKEND_TIER_PRODUCTION
		#define __XRT_CO_BACKEND_STYLE	__XRT_CO_BACKEND_STYLE_INLINE_ASM
	#elif !defined(_WIN32) && !defined(_WIN64) && defined(__riscv) && (__riscv_xlen == 64)
		#define __XRT_CO_ASM_RV64
		#define __XRT_CO_BACKEND_NAME	"asm-rv64"
		#define __XRT_CO_BACKEND_TIER	__XRT_CO_BACKEND_TIER_PRODUCTION
		#define __XRT_CO_BACKEND_STYLE	__XRT_CO_BACKEND_STYLE_INLINE_ASM
	#elif !defined(_WIN32) && !defined(_WIN64) && defined(__loongarch64)
		#define __XRT_CO_ASM_LA64
		#define __XRT_CO_BACKEND_NAME	"asm-la64"
		#define __XRT_CO_BACKEND_TIER	__XRT_CO_BACKEND_TIER_PRODUCTION
		#define __XRT_CO_BACKEND_STYLE	__XRT_CO_BACKEND_STYLE_INLINE_ASM
	#endif
#endif

#ifndef __XRT_CO_BACKEND_TIER
	#error "XRT coroutine production backend is unavailable for this target."
#endif

#if XRT_CO_REQUIRE_PRODUCTION_BACKEND && (__XRT_CO_BACKEND_TIER != __XRT_CO_BACKEND_TIER_PRODUCTION)
	#error "XRT coroutine production backend is required, but current target is not using a production backend."
#endif



/* ================================ 线程级协程运行时 ================================ */

static xrtCoroRuntimeState* __xrt_co_get_runtime_from_thread(xrtThreadData* pThreadData)
{
	if ( pThreadData == NULL ) {
		return NULL;
	}
	return &pThreadData->tCoro;
}

static xrtThreadData* __xrt_co_require_thread_data(bool bSetError)
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();

	if ( pThreadData == NULL && bSetError ) {
		xrtSetError("current thread is not attached to xrt runtime.", FALSE);
	}

	return pThreadData;
}

static xrtCoroRuntimeState* __xrt_co_get_runtime()
{
	return __xrt_co_get_runtime_from_thread(xrtThreadGetCurrent());
}

static xrtCoroRuntimeState* __xrt_co_require_runtime(bool bSetError)
{
	xrtThreadData* pThreadData = __xrt_co_require_thread_data(bSetError);
	return __xrt_co_get_runtime_from_thread(pThreadData);
}

static xcoro __xrt_co_get_current()
{
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime();

	if ( pRuntime == NULL ) {
		return NULL;
	}

	return (xcoro)pRuntime->pCurrent;
}

static void __xrt_co_set_current(xcoro pCo)
{
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime();

	if ( pRuntime ) {
		pRuntime->pCurrent = pCo;
	}
}

static bool __xrt_co_check_owner_tid(uint64 iOwnerThreadId, str sError)
{
	xrtThreadData* pThreadData = __xrt_co_require_thread_data(TRUE);

	if ( pThreadData == NULL ) {
		return FALSE;
	}

	if ( iOwnerThreadId != 0 && pThreadData->iThreadId != iOwnerThreadId ) {
		if ( sError ) {
			xrtSetError(sError, FALSE);
		}
		return FALSE;
	}

	return TRUE;
}

static bool __xrt_co_check_owner(xcoro pCo, str sError)
{
	if ( pCo == NULL ) {
		xrtSetError("invalid coroutine handle.", FALSE);
		return FALSE;
	}

	return __xrt_co_check_owner_tid(pCo->__iOwnerThreadId, sError);
}

static void __xrtCoroRuntimeInitThread(xrtThreadData* pThreadData)
{
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime_from_thread(pThreadData);

	if ( pRuntime == NULL ) {
		return;
	}

	memset(pRuntime, 0, sizeof(xrtCoroRuntimeState));
	pRuntime->iOwnerThreadId = pThreadData->iThreadId;
}

static void __xrtCoroRuntimeUnitThread(xrtThreadData* pThreadData)
{
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime_from_thread(pThreadData);

	if ( pRuntime == NULL ) {
		return;
	}

	if ( pRuntime->pBackendMain ) {
		xrtFree(pRuntime->pBackendMain);
	}

	memset(pRuntime, 0, sizeof(xrtCoroRuntimeState));
}



/* ================================ 协程生命周期辅助 ================================ */

#ifndef MAP_ANONYMOUS
	#ifdef MAP_ANON
		#define MAP_ANONYMOUS MAP_ANON
	#endif
#endif

#define __XRT_CO_FLAG_CANCEL_REQUESTED	0x00000001u
#define __XRT_CO_FLAG_CANCELLED			0x00000002u
#define __XRT_CO_FLAG_CLOSE_REQUESTED	0x00000004u
#define __XRT_CO_FLAG_REAP_PENDING		0x00000008u
#define __XRT_CO_FLAG_JOIN_PINNED		0x00000010u
#define __XRT_CO_FLAG_READY_QUEUED		0x00000020u
#define __XRT_CO_FLAG_TIMER_QUEUED		0x00000040u
#define __XRT_CO_FLAG_STARTED			0x00000080u
#define __XRT_CO_FLAG_IN_CLEANUP		0x00000100u

#define __XRT_CO_WAIT_NONE				0u
#define __XRT_CO_WAIT_TIMER				1u
#define __XRT_CO_WAIT_JOIN				2u
#define __XRT_CO_WAIT_EVENT				3u

#define __XRT_CO_WAIT_RESULT_NONE		0u
#define __XRT_CO_WAIT_RESULT_SIGNAL		1u
#define __XRT_CO_WAIT_RESULT_TIMEOUT	2u

static bool __xrt_co_is_terminal(xcoro pCo)
{
	return pCo && pCo->iState == XRT_CO_DEAD;
}

static bool __xrt_co_is_cancel_requested_flag(xcoro pCo)
{
	return pCo && ((pCo->__iFlags & __XRT_CO_FLAG_CANCEL_REQUESTED) != 0);
}

static bool __xrt_co_sched_mark_ready(xcosched* pSched, xcoro pCo);
static bool __xrt_co_sched_detach_timer(xcosched* pSched, xcoro pCo);
static bool __xrt_co_sched_release_dead(xcoro pCo);
static bool __xrt_co_sched_run_until(xcosched* pSched, xcoro pTarget);

static void __xrt_co_clear_wait_state(xcoro pCo)
{
	if ( pCo == NULL ) {
		return;
	}

	pCo->__pWaitObject = NULL;
	pCo->__iWaitKind = __XRT_CO_WAIT_NONE;
}

static void __xrt_co_wait_link_clear(xcoro pCo)
{
	if ( pCo == NULL ) {
		return;
	}

	pCo->__pWaitPrev = NULL;
	pCo->__pWaitNext = NULL;
}

static void __xrt_co_wait_queue_push_tail(xcoro* ppHead, xcoro* ppTail, xcoro pCo)
{
	if ( ppHead == NULL || ppTail == NULL || pCo == NULL ) {
		return;
	}

	__xrt_co_wait_link_clear(pCo);
	if ( *ppTail != NULL ) {
		(*ppTail)->__pWaitNext = pCo;
		pCo->__pWaitPrev = *ppTail;
	}
	else {
		*ppHead = pCo;
	}
	*ppTail = pCo;
}

static void __xrt_co_wait_queue_remove(xcoro* ppHead, xcoro* ppTail, xcoro pCo)
{
	if ( ppHead == NULL || ppTail == NULL || pCo == NULL ) {
		return;
	}

	if ( pCo->__pWaitPrev != NULL ) {
		pCo->__pWaitPrev->__pWaitNext = pCo->__pWaitNext;
	}
	else if ( *ppHead == pCo ) {
		*ppHead = pCo->__pWaitNext;
	}

	if ( pCo->__pWaitNext != NULL ) {
		pCo->__pWaitNext->__pWaitPrev = pCo->__pWaitPrev;
	}
	else if ( *ppTail == pCo ) {
		*ppTail = pCo->__pWaitPrev;
	}

	__xrt_co_wait_link_clear(pCo);
}

static xcoro __xrt_co_wait_queue_pop_head(xcoro* ppHead, xcoro* ppTail)
{
	xcoro pHead = NULL;

	if ( ppHead == NULL || ppTail == NULL ) {
		return NULL;
	}

	pHead = *ppHead;
	if ( pHead != NULL ) {
		__xrt_co_wait_queue_remove(ppHead, ppTail, pHead);
	}
	return pHead;
}

static void __xrt_co_join_pin_acquire(xcoro pTarget)
{
	if ( pTarget == NULL ) {
		return;
	}

	pTarget->__iJoinRefCount++;
	pTarget->__iFlags |= __XRT_CO_FLAG_JOIN_PINNED;
}

static void __xrt_co_join_pin_release(xcoro pTarget)
{
	if ( pTarget == NULL ) {
		return;
	}

	if ( pTarget->__iJoinRefCount > 0 ) {
		pTarget->__iJoinRefCount--;
	}

	if ( pTarget->__iJoinRefCount == 0 ) {
		pTarget->__iFlags &= ~__XRT_CO_FLAG_JOIN_PINNED;
	}
}

static void __xrt_co_detach_join_waiter(xcoro pWaiter, bool bReleasePin)
{
	xcoro pTarget = NULL;

	if ( pWaiter == NULL ) {
		return;
	}

	pTarget = pWaiter->__pJoinTarget;
	if ( pTarget != NULL ) {
		__xrt_co_wait_queue_remove(&pTarget->__pJoinWaitHead, &pTarget->__pJoinWaitTail, pWaiter);
	}

	if ( pWaiter->__iWaitKind == __XRT_CO_WAIT_JOIN ) {
		__xrt_co_clear_wait_state(pWaiter);
	}

	if ( bReleasePin && pTarget != NULL ) {
		pWaiter->__pJoinTarget = NULL;
		__xrt_co_join_pin_release(pTarget);
	}
}

static void __xrt_co_detach_event_waiter_locked(xcoevent pEvent, xcoro pCo)
{
	if ( pEvent == NULL || pCo == NULL ) {
		return;
	}

	__xrt_co_wait_queue_remove(&pEvent->pWaitHead, &pEvent->pWaitTail, pCo);

	if ( pCo->__pWaitObject == pEvent && pCo->__iWaitKind == __XRT_CO_WAIT_EVENT ) {
		__xrt_co_clear_wait_state(pCo);
	}
}

static xcoro __xrt_co_pop_event_waiter_locked(xcoevent pEvent)
{
	xcoro pCo = NULL;

	if ( pEvent == NULL ) {
		return NULL;
	}

	pCo = __xrt_co_wait_queue_pop_head(&pEvent->pWaitHead, &pEvent->pWaitTail);
	if ( pCo != NULL && pCo->__pWaitObject == pEvent && pCo->__iWaitKind == __XRT_CO_WAIT_EVENT ) {
		__xrt_co_clear_wait_state(pCo);
	}
	return pCo;
}

static bool __xrt_co_event_try_consume_locked(xcoevent pEvent)
{
	if ( pEvent == NULL || !pEvent->bSignaled ) {
		return FALSE;
	}

	if ( !pEvent->bManualReset ) {
		pEvent->bSignaled = FALSE;
	}

	return TRUE;
}

static void __xrt_co_free_cleanup_nodes(xcoro pCo)
{
	while ( pCo && pCo->__pCleanupTop ) {
		xco_cleanup* pCleanup = pCo->__pCleanupTop;
		pCo->__pCleanupTop = pCleanup->pPrev;
		xrtFree(pCleanup);
	}
}

static void __xrt_co_run_cleanup(xcoro pCo)
{
	if ( pCo == NULL ) {
		return;
	}

	pCo->__iFlags |= __XRT_CO_FLAG_IN_CLEANUP;
	while ( pCo->__pCleanupTop ) {
		xco_cleanup* pCleanup = pCo->__pCleanupTop;

		pCo->__pCleanupTop = pCleanup->pPrev;
		if ( pCleanup->Proc ) {
			pCleanup->Proc(pCleanup->Arg);
		}
		xrtFree(pCleanup);
	}
	pCo->__iFlags &= ~__XRT_CO_FLAG_IN_CLEANUP;
}

static size_t __xrt_co_stack_page_size()
{
	#if defined(_WIN32) || defined(_WIN64)
		SYSTEM_INFO tInfo;
		GetSystemInfo(&tInfo);
		return (tInfo.dwPageSize != 0) ? (size_t)tInfo.dwPageSize : 4096u;
	#else
		long iPage = sysconf(_SC_PAGESIZE);
		return (iPage > 0) ? (size_t)iPage : 4096u;
	#endif
}

static size_t __xrt_co_align_up(size_t iValue, size_t iAlign)
{
	if ( iAlign == 0 ) {
		return iValue;
	}
	return (iValue + iAlign - 1) & ~(iAlign - 1);
}

static bool __xrt_co_stack_alloc(xcoro pCo)
{
	size_t iPageSize = 0;
	size_t iGuardSize = 0;
	size_t iStackSize = 0;
	size_t iAllocSize = 0;
	ptr pBase = NULL;

	if ( pCo == NULL ) {
		return FALSE;
	}

	iPageSize = __xrt_co_stack_page_size();
	iGuardSize = iPageSize;
	iStackSize = __xrt_co_align_up(pCo->iStackSize, iPageSize);
	if ( iStackSize < XRT_CO_STACK_MIN ) {
		iStackSize = __xrt_co_align_up(XRT_CO_STACK_MIN, iPageSize);
	}
	if ( iStackSize > XRT_CO_STACK_MAX ) {
		iStackSize = __xrt_co_align_up(XRT_CO_STACK_MAX, iPageSize);
	}
	if ( iStackSize > (SIZE_MAX - iGuardSize) ) {
		xrtSetError("coroutine stack size overflow.", FALSE);
		return FALSE;
	}
	iAllocSize = iStackSize + iGuardSize;

	#if defined(_WIN32) || defined(_WIN64)
		{
			DWORD iOldProtect = 0;

			pBase = VirtualAlloc(NULL, iAllocSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			if ( pBase == NULL ) {
				xrtSetError("failed to reserve coroutine stack.", FALSE);
				return FALSE;
			}

			if ( !VirtualProtect(pBase, iGuardSize, PAGE_NOACCESS, &iOldProtect) ) {
				VirtualFree(pBase, 0, MEM_RELEASE);
				xrtSetError("failed to protect coroutine guard page.", FALSE);
				return FALSE;
			}
		}
	#else
		pBase = mmap(NULL, iAllocSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if ( pBase == MAP_FAILED ) {
			xrtSetError("failed to reserve coroutine stack.", FALSE);
			return FALSE;
		}

		if ( mprotect(pBase, iGuardSize, PROT_NONE) != 0 ) {
			munmap(pBase, iAllocSize);
			xrtSetError("failed to protect coroutine guard page.", FALSE);
			return FALSE;
		}
	#endif

	pCo->__pStackMem = pBase;
	pCo->__iStackAllocSize = iAllocSize;
	pCo->__iStackGuardSize = iGuardSize;
	pCo->__pStack = (uint8*)pBase + iGuardSize;
	pCo->iStackSize = iStackSize;
	return TRUE;
}

static void __xrt_co_stack_free(xcoro pCo)
{
	if ( pCo == NULL || pCo->__pStackMem == NULL ) {
		return;
	}

	#if defined(_WIN32) || defined(_WIN64)
		VirtualFree(pCo->__pStackMem, 0, MEM_RELEASE);
	#else
		munmap(pCo->__pStackMem, pCo->__iStackAllocSize);
	#endif

	pCo->__pStack = NULL;
	pCo->__pStackMem = NULL;
	pCo->__iStackAllocSize = 0;
	pCo->__iStackGuardSize = 0;
}

static void __xrt_co_wake_join_waiters(xcoro pTarget)
{
	xcoro pWaiter = NULL;

	if ( pTarget == NULL ) {
		return;
	}

	for ( ;; ) {
		pWaiter = __xrt_co_wait_queue_pop_head(&pTarget->__pJoinWaitHead, &pTarget->__pJoinWaitTail);
		if ( pWaiter == NULL ) {
			break;
		}

		if ( pWaiter->__iWaitKind == __XRT_CO_WAIT_JOIN ) {
			__xrt_co_clear_wait_state(pWaiter);
		}

		pWaiter->__iWakeTime = 0;
		if ( pWaiter->__pSched ) {
			__xrt_co_sched_mark_ready((xcosched*)pWaiter->__pSched, pWaiter);
		}
	}
}

static void __xrt_co_finish(xcoro pCo, uint32 iTermReason, int64 iExitCode)
{
	if ( pCo == NULL ) {
		return;
	}

	pCo->iState = XRT_CO_DEAD;
	pCo->iExitCode = iExitCode;
	pCo->iTermReason = iTermReason;
	pCo->__iWakeTime = 0;
	__xrt_co_clear_wait_state(pCo);

	if ( iTermReason == XRT_CO_TERM_CANCELLED ) {
		pCo->__iFlags |= __XRT_CO_FLAG_CANCELLED;
	}

	__xrt_co_run_cleanup(pCo);

	__xrt_co_wake_join_waiters(pCo);
}

static bool __xrt_co_finalize_ready_cancel(xcoro pCo)
{
	if ( pCo == NULL ) {
		return FALSE;
	}

	if ( pCo->iState != XRT_CO_READY ) {
		return FALSE;
	}

	__xrt_co_finish(pCo, XRT_CO_TERM_CANCELLED, -1);
	return TRUE;
}



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

/* ================================ 后端实现: x86_64 内联汇编 ================================ */

#ifdef __XRT_CO_ASM_X64_WIN

/*
	Windows x64 ABI callee-saved 寄存器:
	0x00 = rip
	0x08 = rsp
	0x10 = rbp
	0x18 = rbx
	0x20 = rdi
	0x28 = rsi
	0x30 = r12
	0x38 = r13
	0x40 = r14
	0x48 = r15
	0x50 = xmm6
	0x60 = xmm7
	0x70 = xmm8
	0x80 = xmm9
	0x90 = xmm10
	0xA0 = xmm11
	0xB0 = xmm12
	0xC0 = xmm13
	0xD0 = xmm14
	0xE0 = xmm15
*/

static void __xrt_co_swap(__xrt_co_ctx* pFrom, __xrt_co_ctx* pTo)
{
	__asm__ volatile (
		"leaq 1f(%%rip), %%rax\n\t"
		"movq %%rax, 0x00(%%rcx)\n\t"
		"movq %%rsp, 0x08(%%rcx)\n\t"
		"movq %%rbp, 0x10(%%rcx)\n\t"
		"movq %%rbx, 0x18(%%rcx)\n\t"
		"movq %%rdi, 0x20(%%rcx)\n\t"
		"movq %%rsi, 0x28(%%rcx)\n\t"
		"movq %%r12, 0x30(%%rcx)\n\t"
		"movq %%r13, 0x38(%%rcx)\n\t"
		"movq %%r14, 0x40(%%rcx)\n\t"
		"movq %%r15, 0x48(%%rcx)\n\t"
		"movdqu %%xmm6,  0x50(%%rcx)\n\t"
		"movdqu %%xmm7,  0x60(%%rcx)\n\t"
		"movdqu %%xmm8,  0x70(%%rcx)\n\t"
		"movdqu %%xmm9,  0x80(%%rcx)\n\t"
		"movdqu %%xmm10, 0x90(%%rcx)\n\t"
		"movdqu %%xmm11, 0xA0(%%rcx)\n\t"
		"movdqu %%xmm12, 0xB0(%%rcx)\n\t"
		"movdqu %%xmm13, 0xC0(%%rcx)\n\t"
		"movdqu %%xmm14, 0xD0(%%rcx)\n\t"
		"movdqu %%xmm15, 0xE0(%%rcx)\n\t"
		"movdqu 0xE0(%%rdx), %%xmm15\n\t"
		"movdqu 0xD0(%%rdx), %%xmm14\n\t"
		"movdqu 0xC0(%%rdx), %%xmm13\n\t"
		"movdqu 0xB0(%%rdx), %%xmm12\n\t"
		"movdqu 0xA0(%%rdx), %%xmm11\n\t"
		"movdqu 0x90(%%rdx), %%xmm10\n\t"
		"movdqu 0x80(%%rdx), %%xmm9\n\t"
		"movdqu 0x70(%%rdx), %%xmm8\n\t"
		"movdqu 0x60(%%rdx), %%xmm7\n\t"
		"movdqu 0x50(%%rdx), %%xmm6\n\t"
		"movq 0x48(%%rdx), %%r15\n\t"
		"movq 0x40(%%rdx), %%r14\n\t"
		"movq 0x38(%%rdx), %%r13\n\t"
		"movq 0x30(%%rdx), %%r12\n\t"
		"movq 0x28(%%rdx), %%rsi\n\t"
		"movq 0x20(%%rdx), %%rdi\n\t"
		"movq 0x18(%%rdx), %%rbx\n\t"
		"movq 0x10(%%rdx), %%rbp\n\t"
		"movq 0x08(%%rdx), %%rsp\n\t"
		"jmpq *0x00(%%rdx)\n\t"
		"1:\n\t"
		:
		: "c"(pFrom), "d"(pTo)
		: "memory", "rax", "r8", "r9", "r10", "r11"
	);
}

#endif

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
		"movq %%rax, 0x00(%%rdi)\n\t"
		"movq %%rsp, 0x08(%%rdi)\n\t"
		"movq %%rbp, 0x10(%%rdi)\n\t"
		"movq %%rbx, 0x18(%%rdi)\n\t"
		"movq %%r12, 0x20(%%rdi)\n\t"
		"movq %%r13, 0x28(%%rdi)\n\t"
		"movq %%r14, 0x30(%%rdi)\n\t"
		"movq %%r15, 0x38(%%rdi)\n\t"
		"movq 0x38(%%rsi), %%r15\n\t"
		"movq 0x30(%%rsi), %%r14\n\t"
		"movq 0x28(%%rsi), %%r13\n\t"
		"movq 0x20(%%rsi), %%r12\n\t"
		"movq 0x18(%%rsi), %%rbx\n\t"
		"movq 0x10(%%rsi), %%rbp\n\t"
		"movq 0x08(%%rsi), %%rsp\n\t"
		"jmpq *0x00(%%rsi)\n\t"
		"1:\n\t"
		: : "D"(pFrom), "S"(pTo)
		: "memory", "rax", "rcx", "rdx",
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
	arrReg[14..17] = q8, q9
	arrReg[18..21] = q10, q11
	arrReg[22..25] = q12, q13
	arrReg[26..29] = q14, q15
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
		"stp q8,  q9,  [%0, #0x70]\n\t"
		"stp q10, q11, [%0, #0x90]\n\t"
		"stp q12, q13, [%0, #0xB0]\n\t"
		"stp q14, q15, [%0, #0xD0]\n\t"
		"ldp q14, q15, [%1, #0xD0]\n\t"
		"ldp q12, q13, [%1, #0xB0]\n\t"
		"ldp q10, q11, [%1, #0x90]\n\t"
		"ldp q8,  q9,  [%1, #0x70]\n\t"
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

	#if (defined(__riscv_flen) && (__riscv_flen == 32)) || defined(__riscv_float_abi_single)
		#define __XRT_CO_RV64_FP32
		#define __XRT_CO_RV64_HAS_FP
	#elif (defined(__riscv_flen) && (__riscv_flen >= 64)) || defined(__riscv_float_abi_double) || defined(__riscv_float_abi_quad)
		#define __XRT_CO_RV64_FP64
		#define __XRT_CO_RV64_HAS_FP
	#endif

/*
	RISC-V LP64 callee-saved 寄存器:
	arrReg[0] = 恢复点地址 (ra/pc)
	arrReg[1] = sp
	arrReg[2] = s0 (fp)
	arrReg[3..13] = s1 ~ s11
	若使用硬浮点 ABI:
	arrReg[14..25] = fs0 ~ fs11（统一按 8 字节槽位存放，单精度 ABI 仅使用每槽低 4 字节）
*/

static void __xrt_co_swap(__xrt_co_ctx* pFrom, __xrt_co_ctx* pTo)
{
	#ifdef __XRT_CO_RV64_FP64
		__asm__ volatile (
			"la t0, 1f\n\t"
			"sd t0,   0x00(%0)\n\t"
			"sd sp,   0x08(%0)\n\t"
			"sd s0,   0x10(%0)\n\t"
			"sd s1,   0x18(%0)\n\t"
			"sd s2,   0x20(%0)\n\t"
			"sd s3,   0x28(%0)\n\t"
			"sd s4,   0x30(%0)\n\t"
			"sd s5,   0x38(%0)\n\t"
			"sd s6,   0x40(%0)\n\t"
			"sd s7,   0x48(%0)\n\t"
			"sd s8,   0x50(%0)\n\t"
			"sd s9,   0x58(%0)\n\t"
			"sd s10,  0x60(%0)\n\t"
			"sd s11,  0x68(%0)\n\t"
			"fsd fs0,  0x70(%0)\n\t"
			"fsd fs1,  0x78(%0)\n\t"
			"fsd fs2,  0x80(%0)\n\t"
			"fsd fs3,  0x88(%0)\n\t"
			"fsd fs4,  0x90(%0)\n\t"
			"fsd fs5,  0x98(%0)\n\t"
			"fsd fs6,  0xA0(%0)\n\t"
			"fsd fs7,  0xA8(%0)\n\t"
			"fsd fs8,  0xB0(%0)\n\t"
			"fsd fs9,  0xB8(%0)\n\t"
			"fsd fs10, 0xC0(%0)\n\t"
			"fsd fs11, 0xC8(%0)\n\t"
			"fld fs11, 0xC8(%1)\n\t"
			"fld fs10, 0xC0(%1)\n\t"
			"fld fs9,  0xB8(%1)\n\t"
			"fld fs8,  0xB0(%1)\n\t"
			"fld fs7,  0xA8(%1)\n\t"
			"fld fs6,  0xA0(%1)\n\t"
			"fld fs5,  0x98(%1)\n\t"
			"fld fs4,  0x90(%1)\n\t"
			"fld fs3,  0x88(%1)\n\t"
			"fld fs2,  0x80(%1)\n\t"
			"fld fs1,  0x78(%1)\n\t"
			"fld fs0,  0x70(%1)\n\t"
			"ld s11,   0x68(%1)\n\t"
			"ld s10,   0x60(%1)\n\t"
			"ld s9,    0x58(%1)\n\t"
			"ld s8,    0x50(%1)\n\t"
			"ld s7,    0x48(%1)\n\t"
			"ld s6,    0x40(%1)\n\t"
			"ld s5,    0x38(%1)\n\t"
			"ld s4,    0x30(%1)\n\t"
			"ld s3,    0x28(%1)\n\t"
			"ld s2,    0x20(%1)\n\t"
			"ld s1,    0x18(%1)\n\t"
			"ld s0,    0x10(%1)\n\t"
			"ld sp,    0x08(%1)\n\t"
			"ld t0,    0x00(%1)\n\t"
			"jr t0\n\t"
			"1:\n\t"
			: : "r"(pFrom), "r"(pTo)
			: "memory", "t0", "t1", "t2", "t3", "t4", "t5", "t6",
			  "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
		);
	#elif defined(__XRT_CO_RV64_FP32)
		__asm__ volatile (
			"la t0, 1f\n\t"
			"sd t0,   0x00(%0)\n\t"
			"sd sp,   0x08(%0)\n\t"
			"sd s0,   0x10(%0)\n\t"
			"sd s1,   0x18(%0)\n\t"
			"sd s2,   0x20(%0)\n\t"
			"sd s3,   0x28(%0)\n\t"
			"sd s4,   0x30(%0)\n\t"
			"sd s5,   0x38(%0)\n\t"
			"sd s6,   0x40(%0)\n\t"
			"sd s7,   0x48(%0)\n\t"
			"sd s8,   0x50(%0)\n\t"
			"sd s9,   0x58(%0)\n\t"
			"sd s10,  0x60(%0)\n\t"
			"sd s11,  0x68(%0)\n\t"
			"fsw fs0,  0x70(%0)\n\t"
			"fsw fs1,  0x78(%0)\n\t"
			"fsw fs2,  0x80(%0)\n\t"
			"fsw fs3,  0x88(%0)\n\t"
			"fsw fs4,  0x90(%0)\n\t"
			"fsw fs5,  0x98(%0)\n\t"
			"fsw fs6,  0xA0(%0)\n\t"
			"fsw fs7,  0xA8(%0)\n\t"
			"fsw fs8,  0xB0(%0)\n\t"
			"fsw fs9,  0xB8(%0)\n\t"
			"fsw fs10, 0xC0(%0)\n\t"
			"fsw fs11, 0xC8(%0)\n\t"
			"flw fs11, 0xC8(%1)\n\t"
			"flw fs10, 0xC0(%1)\n\t"
			"flw fs9,  0xB8(%1)\n\t"
			"flw fs8,  0xB0(%1)\n\t"
			"flw fs7,  0xA8(%1)\n\t"
			"flw fs6,  0xA0(%1)\n\t"
			"flw fs5,  0x98(%1)\n\t"
			"flw fs4,  0x90(%1)\n\t"
			"flw fs3,  0x88(%1)\n\t"
			"flw fs2,  0x80(%1)\n\t"
			"flw fs1,  0x78(%1)\n\t"
			"flw fs0,  0x70(%1)\n\t"
			"ld s11,   0x68(%1)\n\t"
			"ld s10,   0x60(%1)\n\t"
			"ld s9,    0x58(%1)\n\t"
			"ld s8,    0x50(%1)\n\t"
			"ld s7,    0x48(%1)\n\t"
			"ld s6,    0x40(%1)\n\t"
			"ld s5,    0x38(%1)\n\t"
			"ld s4,    0x30(%1)\n\t"
			"ld s3,    0x28(%1)\n\t"
			"ld s2,    0x20(%1)\n\t"
			"ld s1,    0x18(%1)\n\t"
			"ld s0,    0x10(%1)\n\t"
			"ld sp,    0x08(%1)\n\t"
			"ld t0,    0x00(%1)\n\t"
			"jr t0\n\t"
			"1:\n\t"
			: : "r"(pFrom), "r"(pTo)
			: "memory", "t0", "t1", "t2", "t3", "t4", "t5", "t6",
			  "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
		);
	#else
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
	#endif
}

#endif



/* ================================ 后端实现: LoongArch64 内联汇编 ================================ */

#ifdef __XRT_CO_ASM_LA64

	#if defined(__loongarch_frlen) && (__loongarch_frlen == 32)
		#define __XRT_CO_LA64_FP32
		#define __XRT_CO_LA64_HAS_FP
	#elif defined(__loongarch_frlen) && (__loongarch_frlen >= 64)
		#define __XRT_CO_LA64_FP64
		#define __XRT_CO_LA64_HAS_FP
	#elif defined(__loongarch_single_float)
		#define __XRT_CO_LA64_FP32
		#define __XRT_CO_LA64_HAS_FP
	#elif defined(__loongarch_double_float)
		#define __XRT_CO_LA64_FP64
		#define __XRT_CO_LA64_HAS_FP
	#endif

/*
	LoongArch64 LP64 callee-saved 寄存器:
	arrReg[0] = 恢复点地址 (ra)
	arrReg[1] = sp ($r3)
	arrReg[2] = fp ($r22)
	arrReg[3..11] = s0~s8 ($r23~$r31)
	若使用硬浮点 ABI:
	arrReg[12..19] = fs0 ~ fs7（统一按 8 字节槽位存放，单精度 ABI 仅使用每槽低 4 字节）
*/

static void __xrt_co_swap(__xrt_co_ctx* pFrom, __xrt_co_ctx* pTo)
{
	#ifdef __XRT_CO_LA64_FP64
		__asm__ volatile (
			"la.local $t0, 1f\n\t"
			"st.d   $t0,   %0, 0x00\n\t"
			"st.d   $sp,   %0, 0x08\n\t"
			"st.d   $fp,   %0, 0x10\n\t"
			"st.d   $s0,   %0, 0x18\n\t"
			"st.d   $s1,   %0, 0x20\n\t"
			"st.d   $s2,   %0, 0x28\n\t"
			"st.d   $s3,   %0, 0x30\n\t"
			"st.d   $s4,   %0, 0x38\n\t"
			"st.d   $s5,   %0, 0x40\n\t"
			"st.d   $s6,   %0, 0x48\n\t"
			"st.d   $s7,   %0, 0x50\n\t"
			"st.d   $s8,   %0, 0x58\n\t"
			"fst.d  $fs0,  %0, 0x60\n\t"
			"fst.d  $fs1,  %0, 0x68\n\t"
			"fst.d  $fs2,  %0, 0x70\n\t"
			"fst.d  $fs3,  %0, 0x78\n\t"
			"fst.d  $fs4,  %0, 0x80\n\t"
			"fst.d  $fs5,  %0, 0x88\n\t"
			"fst.d  $fs6,  %0, 0x90\n\t"
			"fst.d  $fs7,  %0, 0x98\n\t"
			"fld.d  $fs7,  %1, 0x98\n\t"
			"fld.d  $fs6,  %1, 0x90\n\t"
			"fld.d  $fs5,  %1, 0x88\n\t"
			"fld.d  $fs4,  %1, 0x80\n\t"
			"fld.d  $fs3,  %1, 0x78\n\t"
			"fld.d  $fs2,  %1, 0x70\n\t"
			"fld.d  $fs1,  %1, 0x68\n\t"
			"fld.d  $fs0,  %1, 0x60\n\t"
			"ld.d   $s8,   %1, 0x58\n\t"
			"ld.d   $s7,   %1, 0x50\n\t"
			"ld.d   $s6,   %1, 0x48\n\t"
			"ld.d   $s5,   %1, 0x40\n\t"
			"ld.d   $s4,   %1, 0x38\n\t"
			"ld.d   $s3,   %1, 0x30\n\t"
			"ld.d   $s2,   %1, 0x28\n\t"
			"ld.d   $s1,   %1, 0x20\n\t"
			"ld.d   $fp,   %1, 0x10\n\t"
			"ld.d   $s0,   %1, 0x18\n\t"
			"ld.d   $sp,   %1, 0x08\n\t"
			"ld.d   $t0,   %1, 0x00\n\t"
			"jr $t0\n\t"
			"1:\n\t"
			: : "r"(pFrom), "r"(pTo)
			: "memory", "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8"
		);
	#elif defined(__XRT_CO_LA64_FP32)
		__asm__ volatile (
			"la.local $t0, 1f\n\t"
			"st.d   $t0,   %0, 0x00\n\t"
			"st.d   $sp,   %0, 0x08\n\t"
			"st.d   $fp,   %0, 0x10\n\t"
			"st.d   $s0,   %0, 0x18\n\t"
			"st.d   $s1,   %0, 0x20\n\t"
			"st.d   $s2,   %0, 0x28\n\t"
			"st.d   $s3,   %0, 0x30\n\t"
			"st.d   $s4,   %0, 0x38\n\t"
			"st.d   $s5,   %0, 0x40\n\t"
			"st.d   $s6,   %0, 0x48\n\t"
			"st.d   $s7,   %0, 0x50\n\t"
			"st.d   $s8,   %0, 0x58\n\t"
			"fst.s  $fs0,  %0, 0x60\n\t"
			"fst.s  $fs1,  %0, 0x68\n\t"
			"fst.s  $fs2,  %0, 0x70\n\t"
			"fst.s  $fs3,  %0, 0x78\n\t"
			"fst.s  $fs4,  %0, 0x80\n\t"
			"fst.s  $fs5,  %0, 0x88\n\t"
			"fst.s  $fs6,  %0, 0x90\n\t"
			"fst.s  $fs7,  %0, 0x98\n\t"
			"fld.s  $fs7,  %1, 0x98\n\t"
			"fld.s  $fs6,  %1, 0x90\n\t"
			"fld.s  $fs5,  %1, 0x88\n\t"
			"fld.s  $fs4,  %1, 0x80\n\t"
			"fld.s  $fs3,  %1, 0x78\n\t"
			"fld.s  $fs2,  %1, 0x70\n\t"
			"fld.s  $fs1,  %1, 0x68\n\t"
			"fld.s  $fs0,  %1, 0x60\n\t"
			"ld.d   $s8,   %1, 0x58\n\t"
			"ld.d   $s7,   %1, 0x50\n\t"
			"ld.d   $s6,   %1, 0x48\n\t"
			"ld.d   $s5,   %1, 0x40\n\t"
			"ld.d   $s4,   %1, 0x38\n\t"
			"ld.d   $s3,   %1, 0x30\n\t"
			"ld.d   $s2,   %1, 0x28\n\t"
			"ld.d   $s1,   %1, 0x20\n\t"
			"ld.d   $fp,   %1, 0x10\n\t"
			"ld.d   $s0,   %1, 0x18\n\t"
			"ld.d   $sp,   %1, 0x08\n\t"
			"ld.d   $t0,   %1, 0x00\n\t"
			"jr $t0\n\t"
			"1:\n\t"
			: : "r"(pFrom), "r"(pTo)
			: "memory", "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8"
		);
	#else
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
	#endif
}

#endif



/* ================================ 汇编后端: 入口 + 初始化 + swap 包装 ================================ */

#if defined(__XRT_CO_ASM_X64_WIN) || defined(__XRT_CO_ASM_X64) || defined(__XRT_CO_ASM_ARM64) || defined(__XRT_CO_ASM_RV64) || defined(__XRT_CO_ASM_LA64)

static bool __xrt_co_prepare_backend_main(xrtCoroRuntimeState* pRuntime)
{
	__xrt_co_ctx* pMainCtx = NULL;

	if ( pRuntime == NULL ) {
		return FALSE;
	}

	if ( pRuntime->pBackendMain == NULL ) {
		pMainCtx = (__xrt_co_ctx*)xrtCalloc(1, sizeof(__xrt_co_ctx));
		if ( pMainCtx == NULL ) {
			xrtSetError("failed to allocate coroutine main context.", FALSE);
			return FALSE;
		}
		pRuntime->pBackendMain = pMainCtx;
	}

	return TRUE;
}

// 协程入口包装（不接收参数，从线程状态读取当前协程）
static void __xrt_co_asm_entry()
{
	xcoro pCo = __xrt_co_get_current();
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime();

	if ( pCo == NULL || pRuntime == NULL || pRuntime->pBackendMain == NULL ) {
		return;
	}

	pCo->pfnEntry(pCo->pParam);
	__xrt_co_finish(pCo, __xrt_co_is_cancel_requested_flag(pCo) ? XRT_CO_TERM_CANCELLED : XRT_CO_TERM_RETURNED, 0);
	__xrt_co_swap(&pCo->__tCtx, (__xrt_co_ctx*)pRuntime->pBackendMain);
}

static bool __xrt_co_init_ctx(xcoro pCo)
{
	uint8* pStackTop = (uint8*)pCo->__pStack + pCo->iStackSize;

	// 对齐到 16 字节边界
	pStackTop = (uint8*)((uintptr)pStackTop & ~(uintptr)0x0F);

	#ifdef __XRT_CO_ASM_X64
		// x86_64: 模拟 call 指令压入返回地址的效果 (rsp mod 16 == 8)
		pStackTop -= 8;
	#elif defined(__XRT_CO_ASM_X64_WIN)
		// Win64: 需要模拟返回地址 + 32 字节 shadow space
		pStackTop -= 40;
	#endif

	memset(&pCo->__tCtx, 0, sizeof(__xrt_co_ctx));

	// arrReg[0] = 入口点, arrReg[1] = 栈顶
	pCo->__tCtx.arrReg[0] = (ptr)__xrt_co_asm_entry;
	pCo->__tCtx.arrReg[1] = (ptr)pStackTop;
	return TRUE;
}

static void __xrt_co_swap_to_co(xrtCoroRuntimeState* pRuntime, xcoro pCo)
{
	__xrt_co_swap((__xrt_co_ctx*)pRuntime->pBackendMain, &pCo->__tCtx);
}

static void __xrt_co_swap_to_main(xrtCoroRuntimeState* pRuntime)
{
	xcoro pCurrent = __xrt_co_get_current();

	if ( pRuntime && pRuntime->pBackendMain && pCurrent ) {
		__xrt_co_swap(&pCurrent->__tCtx, (__xrt_co_ctx*)pRuntime->pBackendMain);
	}
}

#endif



/* ================================ 内部销毁辅助 ================================ */

static void __xrt_co_destroy_raw(xcoro pCo)
{
	if ( pCo == NULL ) {
		return;
	}

	__xrt_co_stack_free(pCo);

	__xrt_co_free_cleanup_nodes(pCo);

	xrtFree(pCo);
}



/* ================================ 基础协程 API ================================ */

XXAPI xcoro xrtCoCreateEx(xco_entry pfnEntry, ptr pParam, const xco_create_args* pArgs)
{
	xrtThreadData* pThreadData = __xrt_co_require_thread_data(TRUE);
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime_from_thread(pThreadData);
	xcoro pCo = NULL;
	size_t iStackSize = 0;
	ptr pUserData = NULL;
	uint32 iFlags = 0;

	if ( pThreadData == NULL || pRuntime == NULL ) {
		return NULL;
	}

	if ( !pfnEntry ) {
		xrtSetError("coroutine entry is null.", FALSE);
		return NULL;
	}

	if ( pArgs != NULL ) {
		iStackSize = pArgs->iStackSize;
		pUserData = pArgs->pUserData;
		iFlags = pArgs->iFlags;
	}

	if ( iFlags != XRT_CO_CREATE_NONE ) {
		xrtSetError("unsupported coroutine create flags.", FALSE);
		return NULL;
	}

	// 栈大小处理
	if ( iStackSize == 0 ) iStackSize = XRT_CO_STACK_DEFAULT;
	if ( iStackSize < XRT_CO_STACK_MIN ) iStackSize = XRT_CO_STACK_MIN;
	if ( iStackSize > XRT_CO_STACK_MAX ) iStackSize = XRT_CO_STACK_MAX;

	// 分配协程结构体
	pCo = (xcoro)xrtMalloc(sizeof(xcoro_struct));
	if ( !pCo ) {
		return NULL;
	}

	memset(pCo, 0, sizeof(xcoro_struct));
	pCo->iState = XRT_CO_READY;
	pCo->__iFlags = 0;
	pCo->__iOwnerThreadId = pThreadData->iThreadId;
	pCo->pfnEntry = pfnEntry;
	pCo->pParam = pParam;
	pCo->pUserData = pUserData;
	pCo->pResult = NULL;
	pCo->iExitCode = 0;
	pCo->iTermReason = XRT_CO_TERM_NONE;
	pCo->iStackSize = iStackSize;

	if ( !__xrt_co_stack_alloc(pCo) ) {
		xrtFree(pCo);
		return NULL;
	}

	if ( !__xrt_co_prepare_backend_main(pRuntime) || !__xrt_co_init_ctx(pCo) ) {
		__xrt_co_destroy_raw(pCo);
		return NULL;
	}

	return pCo;
}

XXAPI xcoro xrtCoCreate(xco_entry pfnEntry, ptr pParam, size_t iStackSize)
{
	xco_create_args tArgs;

	memset(&tArgs, 0, sizeof(tArgs));
	tArgs.iStackSize = iStackSize;
	return xrtCoCreateEx(pfnEntry, pParam, &tArgs);
}


XXAPI void xrtCoDestroy(xcoro pCo)
{
	if ( pCo == NULL ) {
		return;
	}

	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return;
	}

	if ( pCo->__pSched != NULL ) {
		xrtSetError("cannot destroy coroutine while it is owned by a scheduler.", FALSE);
		return;
	}

	if ( (pCo->iState != XRT_CO_READY) && (pCo->iState != XRT_CO_DEAD) ) {
		xrtSetError("coroutine destroy requires READY or DEAD state.", FALSE);
		return;
	}

	__xrt_co_destroy_raw(pCo);
}


XXAPI bool xrtCoCancel(xcoro pCo)
{
	if ( pCo == NULL ) {
		xrtSetError("invalid coroutine handle.", FALSE);
		return FALSE;
	}

	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return FALSE;
	}

	if ( __xrt_co_is_terminal(pCo) ) {
		return TRUE;
	}

	pCo->__iFlags |= __XRT_CO_FLAG_CANCEL_REQUESTED;

	if ( pCo == __xrt_co_get_current() ) {
		return TRUE;
	}

	if ( pCo->iState == XRT_CO_READY && (pCo->__iFlags & __XRT_CO_FLAG_STARTED) == 0 ) {
		__xrt_co_finalize_ready_cancel(pCo);
		return TRUE;
	}

	if ( pCo->__pJoinTarget != NULL ) {
		__xrt_co_detach_join_waiter(pCo, TRUE);
	}

	if ( pCo->__iWaitKind == __XRT_CO_WAIT_EVENT && pCo->__pWaitObject != NULL ) {
		xcoevent pEvent = (xcoevent)pCo->__pWaitObject;

		if ( pEvent->pLock ) {
			xrtMutexLock(pEvent->pLock);
			__xrt_co_detach_event_waiter_locked(pEvent, pCo);
			xrtMutexUnlock(pEvent->pLock);
		}
		else {
			__xrt_co_clear_wait_state(pCo);
		}
	}

	if ( pCo->__iWakeTime > 0 ) {
		pCo->__iWakeTime = 0;
	}

	if ( pCo->__pSched != NULL ) {
		__xrt_co_sched_detach_timer((xcosched*)pCo->__pSched, pCo);
		__xrt_co_sched_mark_ready((xcosched*)pCo->__pSched, pCo);
	}

	return TRUE;
}


XXAPI bool xrtCoClose(xcoro pCo)
{
	if ( pCo == NULL ) {
		xrtSetError("invalid coroutine handle.", FALSE);
		return FALSE;
	}

	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return FALSE;
	}

	if ( pCo->__pSched != NULL ) {
		if ( pCo->iState == XRT_CO_DEAD ) {
			return __xrt_co_sched_release_dead(pCo);
		}
		pCo->__iFlags |= __XRT_CO_FLAG_CLOSE_REQUESTED;
		return xrtCoCancel(pCo);
	}

	if ( (pCo->iState != XRT_CO_READY) && (pCo->iState != XRT_CO_DEAD) ) {
		xrtSetError("close requires READY or DEAD coroutine when no scheduler owns it.", FALSE);
		return FALSE;
	}

	xrtCoDestroy(pCo);
	return TRUE;
}


XXAPI bool xrtCoResume(xcoro pCo)
{
	xrtCoroRuntimeState* pRuntime = NULL;

	if ( pCo == NULL ) {
		xrtSetError("invalid coroutine handle.", FALSE);
		return FALSE;
	}

	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return FALSE;
	}

	pRuntime = __xrt_co_require_runtime(TRUE);
	if ( pRuntime == NULL ) {
		return FALSE;
	}

	if ( !__xrt_co_prepare_backend_main(pRuntime) ) {
		return FALSE;
	}

	// 只能恢复 READY 或 SUSPENDED 状态的协程
	if ( (pCo->iState != XRT_CO_READY) && (pCo->iState != XRT_CO_SUSPENDED) ) {
		xrtSetError("coroutine is not in a resumable state.", FALSE);
		return FALSE;
	}

	// 不能在协程内部调用 Resume（不支持嵌套）
	if ( __xrt_co_get_current() != NULL ) {
		xrtSetError("nested coroutine resume is not supported.", FALSE);
		return FALSE;
	}

	if ( pCo->iState == XRT_CO_READY &&
		__xrt_co_is_cancel_requested_flag(pCo) &&
		(pCo->__iFlags & __XRT_CO_FLAG_STARTED) == 0 ) {
		__xrt_co_finish(pCo, XRT_CO_TERM_CANCELLED, -1);
		return TRUE;
	}

	pCo->__iWakeTime = 0;
	pCo->__iFlags |= __XRT_CO_FLAG_STARTED;
	__xrt_co_set_current(pCo);
	pCo->iState = XRT_CO_RUNNING;

	__xrt_co_swap_to_co(pRuntime, pCo);

	// 从这里恢复意味着协程 yield 了或者结束了
	__xrt_co_set_current(NULL);
	return TRUE;
}


XXAPI void xrtCoYield()
{
	xcoro pCo = __xrt_co_get_current();
	xrtCoroRuntimeState* pRuntime = __xrt_co_require_runtime(FALSE);

	if ( pCo == NULL || pRuntime == NULL ) {
		return;
	}

	if ( (pCo->__iFlags & __XRT_CO_FLAG_IN_CLEANUP) != 0 ) {
		xrtSetError("cannot yield while running coroutine cleanup.", FALSE);
		return;
	}

	pCo->iState = XRT_CO_SUSPENDED;
	__xrt_co_swap_to_main(pRuntime);
}


XXAPI int xrtCoGetState(xcoro pCo)
{
	if ( !pCo ) return XRT_CO_DEAD;
	return pCo->iState;
}

XXAPI xcoro xrtCoGetCurrent()
{
	return __xrt_co_get_current();
}

XXAPI bool xrtCoIsCancelRequested()
{
	xcoro pCo = __xrt_co_get_current();
	return pCo ? __xrt_co_is_cancel_requested_flag(pCo) : FALSE;
}

XXAPI bool xrtCoWasCancelled(xcoro pCo)
{
	if ( pCo == NULL ) {
		return FALSE;
	}

	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return FALSE;
	}

	return (pCo->__iFlags & __XRT_CO_FLAG_CANCELLED) != 0;
}

XXAPI int64 xrtCoGetExitCode(xcoro pCo)
{
	if ( pCo == NULL ) {
		return 0;
	}

	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return 0;
	}

	return pCo->iExitCode;
}

XXAPI void xrtCoSetResult(ptr pResult)
{
	xcoro pCo = __xrt_co_get_current();

	if ( pCo == NULL ) {
		xrtSetError("xrtCoSetResult must be called from inside a coroutine.", FALSE);
		return;
	}

	pCo->pResult = pResult;
}

XXAPI ptr xrtCoGetResult(xcoro pCo)
{
	if ( pCo == NULL ) {
		return NULL;
	}

	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return NULL;
	}

	return pCo->pResult;
}

XXAPI str xrtCoGetBackendName()
{
	return __XRT_CO_BACKEND_NAME;
}

XXAPI int xrtCoGetBackendTier()
{
	return XRT_CO_BACKEND_TIER_PRODUCTION;
}

XXAPI int xrtCoGetBackendStyle()
{
	return XRT_CO_BACKEND_STYLE_INLINE_ASM;
}

/* ================================ 协程调度器 ================================ */

#define __XRT_CO_SCHED_INIT_CAP  16
#define __XRT_CO_TIMER_INIT_CAP  16
#define __XRT_CO_SCHED_WAIT_FOREVER 0xFFFFFFFFu

typedef struct __xrt_co_post_item {
	xcoro pCo;
	struct __xrt_co_post_item* pNext;
} __xrt_co_post_item;

struct xrt_co_scheduler {
	xrtThreadData* pThreadData;
	uint64 iOwnerThreadId;
	xcoro* arrCoros;
	int iCount;
	int iCapacity;
	int iAlive;
	xcoro pReadyHead;
	xcoro pReadyTail;
	xcoro* arrTimers;
	int iTimerCount;
	int iTimerCapacity;
	xmutex pPostMutex;
	xcond pPostCond;
	__xrt_co_post_item* pPostHead;
	__xrt_co_post_item* pPostTail;
	__xrt_co_post_item* pPostFree;
};

static int __xrt_co_sched_find_index(xcosched* pSched, xcoro pCo)
{
	if ( pSched == NULL || pCo == NULL ) {
		return -1;
	}

	for ( int i = 0; i < pSched->iCount; i++ ) {
		if ( pSched->arrCoros[i] == pCo ) {
			return i;
		}
	}

	return -1;
}

static bool __xrt_co_sched_ready_unlink(xcosched* pSched, xcoro pCo)
{
	if ( pSched == NULL || pCo == NULL || (pCo->__iFlags & __XRT_CO_FLAG_READY_QUEUED) == 0 ) {
		return FALSE;
	}

	if ( pCo->__pReadyPrev ) {
		pCo->__pReadyPrev->__pReadyNext = pCo->__pReadyNext;
	}
	else {
		pSched->pReadyHead = pCo->__pReadyNext;
	}

	if ( pCo->__pReadyNext ) {
		pCo->__pReadyNext->__pReadyPrev = pCo->__pReadyPrev;
	}
	else {
		pSched->pReadyTail = pCo->__pReadyPrev;
	}

	pCo->__pReadyPrev = NULL;
	pCo->__pReadyNext = NULL;
	pCo->__iFlags &= ~__XRT_CO_FLAG_READY_QUEUED;
	return TRUE;
}

static bool __xrt_co_sched_timer_ensure_capacity(xcosched* pSched)
{
	xcoro* arrNew = NULL;
	int iNewCap = 0;

	if ( pSched == NULL ) {
		return FALSE;
	}

	if ( pSched->iTimerCount < pSched->iTimerCapacity ) {
		return TRUE;
	}

	iNewCap = (pSched->iTimerCapacity <= 0) ? __XRT_CO_TIMER_INIT_CAP : (pSched->iTimerCapacity * 2);
	arrNew = (xcoro*)xrtRealloc(pSched->arrTimers, sizeof(xcoro) * iNewCap);
	if ( arrNew == NULL ) {
		return FALSE;
	}

	pSched->arrTimers = arrNew;
	pSched->iTimerCapacity = iNewCap;
	return TRUE;
}

static void __xrt_co_sched_timer_swap(xcosched* pSched, int iA, int iB)
{
	xcoro pTmp = NULL;

	if ( pSched == NULL || iA == iB ) {
		return;
	}

	pTmp = pSched->arrTimers[iA];
	pSched->arrTimers[iA] = pSched->arrTimers[iB];
	pSched->arrTimers[iB] = pTmp;

	if ( pSched->arrTimers[iA] ) {
		pSched->arrTimers[iA]->__iTimerIndex = (uint32)(iA + 1);
	}
	if ( pSched->arrTimers[iB] ) {
		pSched->arrTimers[iB]->__iTimerIndex = (uint32)(iB + 1);
	}
}

static void __xrt_co_sched_timer_sift_up(xcosched* pSched, int iIndex)
{
	while ( pSched && iIndex > 0 ) {
		int iParent = (iIndex - 1) / 2;
		xcoro pNode = pSched->arrTimers[iIndex];
		xcoro pParent = pSched->arrTimers[iParent];

		if ( pNode == NULL || pParent == NULL || pParent->__iWakeTime <= pNode->__iWakeTime ) {
			break;
		}

		__xrt_co_sched_timer_swap(pSched, iIndex, iParent);
		iIndex = iParent;
	}
}

static void __xrt_co_sched_timer_sift_down(xcosched* pSched, int iIndex)
{
	while ( pSched ) {
		int iLeft = iIndex * 2 + 1;
		int iRight = iLeft + 1;
		int iSmallest = iIndex;

		if ( iLeft < pSched->iTimerCount &&
			pSched->arrTimers[iLeft] &&
			pSched->arrTimers[iSmallest] &&
			pSched->arrTimers[iLeft]->__iWakeTime < pSched->arrTimers[iSmallest]->__iWakeTime ) {
			iSmallest = iLeft;
		}

		if ( iRight < pSched->iTimerCount &&
			pSched->arrTimers[iRight] &&
			pSched->arrTimers[iSmallest] &&
			pSched->arrTimers[iRight]->__iWakeTime < pSched->arrTimers[iSmallest]->__iWakeTime ) {
			iSmallest = iRight;
		}

		if ( iSmallest == iIndex ) {
			break;
		}

		__xrt_co_sched_timer_swap(pSched, iIndex, iSmallest);
		iIndex = iSmallest;
	}
}

static bool __xrt_co_sched_detach_timer(xcosched* pSched, xcoro pCo)
{
	int iIndex = 0;
	int iLast = 0;

	if ( pSched == NULL || pCo == NULL || (pCo->__iFlags & __XRT_CO_FLAG_TIMER_QUEUED) == 0 || pCo->__iTimerIndex == 0 ) {
		return FALSE;
	}

	iIndex = (int)pCo->__iTimerIndex - 1;
	iLast = pSched->iTimerCount - 1;

	if ( iIndex < 0 || iIndex >= pSched->iTimerCount ) {
		pCo->__iTimerIndex = 0;
		pCo->__iFlags &= ~__XRT_CO_FLAG_TIMER_QUEUED;
		return FALSE;
	}

	if ( iIndex != iLast ) {
		__xrt_co_sched_timer_swap(pSched, iIndex, iLast);
	}

	pSched->arrTimers[iLast] = NULL;
	pSched->iTimerCount--;

	pCo->__iTimerIndex = 0;
	pCo->__iFlags &= ~__XRT_CO_FLAG_TIMER_QUEUED;

	if ( iIndex < pSched->iTimerCount ) {
		__xrt_co_sched_timer_sift_down(pSched, iIndex);
		__xrt_co_sched_timer_sift_up(pSched, iIndex);
	}

	return TRUE;
}

static bool __xrt_co_sched_attach_timer(xcosched* pSched, xcoro pCo)
{
	int iIndex = 0;

	if ( pSched == NULL || pCo == NULL || pCo->iState == XRT_CO_DEAD || pCo->__iWakeTime <= 0 ) {
		return FALSE;
	}

	__xrt_co_sched_ready_unlink(pSched, pCo);

	if ( pCo->__iFlags & __XRT_CO_FLAG_TIMER_QUEUED ) {
		iIndex = (int)pCo->__iTimerIndex - 1;
		if ( iIndex >= 0 && iIndex < pSched->iTimerCount ) {
			__xrt_co_sched_timer_sift_down(pSched, iIndex);
			__xrt_co_sched_timer_sift_up(pSched, iIndex);
			return TRUE;
		}
		pCo->__iFlags &= ~__XRT_CO_FLAG_TIMER_QUEUED;
		pCo->__iTimerIndex = 0;
	}

	if ( !__xrt_co_sched_timer_ensure_capacity(pSched) ) {
		return FALSE;
	}

	iIndex = pSched->iTimerCount++;
	pSched->arrTimers[iIndex] = pCo;
	pCo->__iFlags |= __XRT_CO_FLAG_TIMER_QUEUED;
	pCo->__iTimerIndex = (uint32)(iIndex + 1);
	pCo->iState = XRT_CO_SUSPENDED;
	__xrt_co_sched_timer_sift_up(pSched, iIndex);
	return TRUE;
}

static bool __xrt_co_sched_mark_ready(xcosched* pSched, xcoro pCo)
{
	if ( pSched == NULL || pCo == NULL || pCo->iState == XRT_CO_DEAD || pCo->__pSched != pSched ) {
		return FALSE;
	}

	if ( pCo->__pJoinTarget != NULL && !__xrt_co_is_terminal(pCo->__pJoinTarget) ) {
		return FALSE;
	}

	__xrt_co_sched_detach_timer(pSched, pCo);

	if ( pCo->__iFlags & __XRT_CO_FLAG_READY_QUEUED ) {
		pCo->iState = XRT_CO_READY;
		return TRUE;
	}

	pCo->__iWakeTime = 0;
	pCo->iState = XRT_CO_READY;
	pCo->__pReadyPrev = pSched->pReadyTail;
	pCo->__pReadyNext = NULL;

	if ( pSched->pReadyTail ) {
		pSched->pReadyTail->__pReadyNext = pCo;
	}
	else {
		pSched->pReadyHead = pCo;
	}

	pSched->pReadyTail = pCo;
	pCo->__iFlags |= __XRT_CO_FLAG_READY_QUEUED;
	return TRUE;
}

static void __xrt_co_sched_collect_expired_timers(xcosched* pSched, int64 iNow)
{
	while ( pSched && pSched->iTimerCount > 0 ) {
		xcoro pCo = pSched->arrTimers[0];

		if ( pCo == NULL ) {
			break;
		}

		if ( pCo->__iWakeTime > iNow ) {
			break;
		}

		__xrt_co_sched_detach_timer(pSched, pCo);
		if ( pCo->__iWaitKind == __XRT_CO_WAIT_EVENT && pCo->__pWaitObject != NULL ) {
			xcoevent pEvent = (xcoevent)pCo->__pWaitObject;

			if ( pEvent && pEvent->pLock ) {
				xrtMutexLock(pEvent->pLock);
				pCo->__iWaitResult = __XRT_CO_WAIT_RESULT_TIMEOUT;
				__xrt_co_detach_event_waiter_locked(pEvent, pCo);
				xrtMutexUnlock(pEvent->pLock);
			}
		}
		pCo->__iWakeTime = 0;
		__xrt_co_sched_mark_ready(pSched, pCo);
	}
}

static bool __xrt_co_sched_reap_dead(xcosched* pSched, int iIndex)
{
	xcoro pCo = NULL;

	if ( pSched == NULL || iIndex < 0 || iIndex >= pSched->iCount ) {
		return FALSE;
	}

	pCo = pSched->arrCoros[iIndex];
	if ( pCo == NULL || pCo->iState != XRT_CO_DEAD ) {
		return FALSE;
	}

	__xrt_co_sched_ready_unlink(pSched, pCo);
	__xrt_co_sched_detach_timer(pSched, pCo);

	if ( (pCo->__iFlags & __XRT_CO_FLAG_REAP_PENDING) == 0 ) {
		pCo->__iFlags |= __XRT_CO_FLAG_REAP_PENDING;
		if ( pSched->iAlive > 0 ) {
			pSched->iAlive--;
		}
	}

	if ( pCo->__iFlags & __XRT_CO_FLAG_JOIN_PINNED ) {
		return TRUE;
	}

	pCo->__pSched = NULL;
	__xrt_co_destroy_raw(pCo);
	pSched->arrCoros[iIndex] = NULL;
	return TRUE;
}

static bool __xrt_co_sched_release_dead(xcoro pCo)
{
	xcosched* pSched = NULL;

	if ( pCo == NULL || pCo->__pSched == NULL || pCo->iState != XRT_CO_DEAD ) {
		return FALSE;
	}

	pSched = (xcosched*)pCo->__pSched;
	int iIndex = __xrt_co_sched_find_index(pSched, pCo);

	if ( iIndex >= 0 ) {
		__xrt_co_sched_ready_unlink(pSched, pCo);
		__xrt_co_sched_detach_timer(pSched, pCo);
		if ( (pCo->__iFlags & __XRT_CO_FLAG_REAP_PENDING) == 0 && pSched->iAlive > 0 ) {
			pSched->iAlive--;
		}
		pCo->__iFlags &= ~__XRT_CO_FLAG_JOIN_PINNED;
		pCo->__iFlags |= __XRT_CO_FLAG_REAP_PENDING;
		pCo->__pSched = NULL;
		__xrt_co_destroy_raw(pCo);
		pSched->arrCoros[iIndex] = NULL;
		return TRUE;
	}

	return FALSE;
}

static void __xrt_co_sched_on_return(xcosched* pSched, xcoro pCo, int64 iNow)
{
	int iIndex = 0;

	if ( pSched == NULL || pCo == NULL ) {
		return;
	}

	if ( pCo->iState == XRT_CO_DEAD ) {
		iIndex = __xrt_co_sched_find_index(pSched, pCo);
		if ( iIndex >= 0 ) {
			__xrt_co_sched_reap_dead(pSched, iIndex);
		}
		return;
	}

	if ( pCo->__pSched != pSched ) {
		return;
	}

	if ( pCo->__pJoinTarget != NULL ) {
		if ( !__xrt_co_is_terminal(pCo->__pJoinTarget) ) {
			pCo->iState = XRT_CO_SUSPENDED;
			return;
		}
	}

	if ( pCo->__iWaitKind == __XRT_CO_WAIT_EVENT && pCo->__pWaitObject != NULL ) {
		if ( (pCo->__iWakeTime > 0) && (pCo->__iWakeTime > iNow) ) {
			pCo->iState = XRT_CO_SUSPENDED;
			__xrt_co_sched_attach_timer(pSched, pCo);
		}
		else if ( pCo->__iWakeTime > 0 ) {
			pCo->__iWakeTime = 0;
		}
		pCo->iState = XRT_CO_SUSPENDED;
		return;
	}

	if ( (pCo->__iWakeTime > 0) && (pCo->__iWakeTime > iNow) ) {
		pCo->iState = XRT_CO_SUSPENDED;
		__xrt_co_sched_attach_timer(pSched, pCo);
		return;
	}

	if ( pCo->__iWakeTime > 0 ) {
		pCo->__iWakeTime = 0;
	}

	__xrt_co_sched_mark_ready(pSched, pCo);
}

static int64 __xrt_co_sched_next_wake_time(xcosched* pSched, int64 iNow)
{
	if ( pSched == NULL ) {
		return 0;
	}

	if ( pSched->pReadyHead != NULL ) {
		return iNow;
	}

	if ( pSched->iTimerCount > 0 && pSched->arrTimers[0] ) {
		return pSched->arrTimers[0]->__iWakeTime;
	}

	return 0;
}

static bool __xrt_co_sched_has_pending_posts(xcosched* pSched)
{
	bool bPending = FALSE;

	if ( pSched == NULL || pSched->pPostMutex == NULL ) {
		return FALSE;
	}

	xrtMutexLock(pSched->pPostMutex);
	bPending = (pSched->pPostHead != NULL);
	xrtMutexUnlock(pSched->pPostMutex);
	return bPending;
}

static __xrt_co_post_item* __xrt_co_sched_post_item_alloc(xcosched* pSched)
{
	__xrt_co_post_item* pItem = NULL;

	if ( pSched == NULL ) {
		return NULL;
	}

	xrtMutexLock(pSched->pPostMutex);
	pItem = pSched->pPostFree;
	if ( pItem != NULL ) {
		pSched->pPostFree = pItem->pNext;
	}
	xrtMutexUnlock(pSched->pPostMutex);

	if ( pItem == NULL ) {
		pItem = (__xrt_co_post_item*)xrtMalloc(sizeof(__xrt_co_post_item));
	}

	if ( pItem != NULL ) {
		pItem->pCo = NULL;
		pItem->pNext = NULL;
	}

	return pItem;
}

static void __xrt_co_sched_post_item_free(xcosched* pSched, __xrt_co_post_item* pItem)
{
	if ( pSched == NULL || pItem == NULL || pSched->pPostMutex == NULL ) {
		if ( pItem != NULL ) {
			xrtFree(pItem);
		}
		return;
	}

	xrtMutexLock(pSched->pPostMutex);
	pItem->pCo = NULL;
	pItem->pNext = pSched->pPostFree;
	pSched->pPostFree = pItem;
	xrtMutexUnlock(pSched->pPostMutex);
}

static void __xrt_co_sched_drain_posts(xcosched* pSched)
{
	__xrt_co_post_item* pHead = NULL;

	if ( pSched == NULL || pSched->pPostMutex == NULL ) {
		return;
	}

	xrtMutexLock(pSched->pPostMutex);
	pHead = pSched->pPostHead;
	pSched->pPostHead = NULL;
	pSched->pPostTail = NULL;
	xrtMutexUnlock(pSched->pPostMutex);

	while ( pHead ) {
		__xrt_co_post_item* pNext = pHead->pNext;
		xcoro pCo = pHead->pCo;

		if ( pCo && pCo->__pSched == pSched && pCo->iState != XRT_CO_DEAD ) {
			if ( pCo->__pJoinTarget == NULL || __xrt_co_is_terminal(pCo->__pJoinTarget) ) {
				pCo->__iWakeTime = 0;
				__xrt_co_sched_detach_timer(pSched, pCo);
				__xrt_co_sched_mark_ready(pSched, pCo);
			}
		}

		__xrt_co_sched_post_item_free(pSched, pHead);
		pHead = pNext;
	}
}

static bool __xrt_co_sched_wait_for_post(xcosched* pSched, uint32 iTimeout)
{
	bool bReady = FALSE;

	if ( pSched == NULL || pSched->pPostMutex == NULL || pSched->pPostCond == NULL || iTimeout == 0 ) {
		return FALSE;
	}

	xrtMutexLock(pSched->pPostMutex);
	if ( pSched->pPostHead == NULL ) {
		if ( iTimeout == __XRT_CO_SCHED_WAIT_FOREVER ) {
			xrtCondWait(pSched->pPostCond, pSched->pPostMutex);
		}
		else {
			(void)xrtCondWaitTimeout(pSched->pPostCond, pSched->pPostMutex, iTimeout);
		}
	}
	bReady = (pSched->pPostHead != NULL);
	xrtMutexUnlock(pSched->pPostMutex);
	return bReady;
}

static uint32 __xrt_co_sched_compute_wait_timeout(xcosched* pSched, uint32 iTimeout)
{
	int64 iNow = 0;
	int64 iNextWake = 0;
	bool bInfinite = (iTimeout == __XRT_CO_SCHED_WAIT_FOREVER);

	if ( pSched == NULL ) {
		return 0;
	}

	if ( pSched->pReadyHead != NULL || __xrt_co_sched_has_pending_posts(pSched) ) {
		return 0;
	}

	iNow = __xrt_co_time_ms();
	iNextWake = __xrt_co_sched_next_wake_time(pSched, iNow);

	if ( iNextWake > iNow ) {
		int64 iDelta = iNextWake - iNow;
		if ( !bInfinite && iDelta > iTimeout ) {
			iDelta = iTimeout;
		}
		if ( iDelta < 0 ) {
			iDelta = 0;
		}
		if ( iDelta > 0xFFFFFFFEull ) {
			iDelta = 0xFFFFFFFEull;
		}
		return (uint32)iDelta;
	}

	if ( iNextWake > 0 ) {
		return 0;
	}

	return bInfinite ? __XRT_CO_SCHED_WAIT_FOREVER : iTimeout;
}

static bool __xrt_co_sched_run_until(xcosched* pSched, xcoro pTarget)
{
	if ( pSched == NULL || pTarget == NULL ) {
		return FALSE;
	}

	while ( !__xrt_co_is_terminal(pTarget) && pSched->iAlive > 0 ) {
		if ( !xrtCoSchedPollOnce(pSched, __XRT_CO_SCHED_WAIT_FOREVER) && !__xrt_co_is_terminal(pTarget) ) {
			break;
		}

		if ( __xrt_co_is_terminal(pTarget) ) {
			break;
		}
	}

	return __xrt_co_is_terminal(pTarget);
}

XXAPI bool xrtCoJoin(xcoro pCo)
{
	xcoro pCurrent = NULL;
	xrtCoroRuntimeState* pRuntime = NULL;

	if ( pCo == NULL ) {
		xrtSetError("invalid coroutine handle.", FALSE);
		return FALSE;
	}

	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return FALSE;
	}

	if ( __xrt_co_is_terminal(pCo) ) {
		return TRUE;
	}

	pRuntime = __xrt_co_require_runtime(TRUE);
	if ( pRuntime == NULL ) {
		return FALSE;
	}

	pCurrent = __xrt_co_get_current();
	if ( pCurrent != NULL ) {
		if ( pCurrent == pCo ) {
			xrtSetError("coroutine cannot join itself.", FALSE);
			return FALSE;
		}

		if ( pCurrent->__pSched == NULL || pCo->__pSched == NULL || pCurrent->__pSched != pCo->__pSched ) {
			xrtSetError("coroutine join inside coroutine requires the same scheduler.", FALSE);
			return FALSE;
		}

		if ( pCurrent->__pJoinTarget != NULL && pCurrent->__pJoinTarget != pCo ) {
			xrtSetError("current coroutine is already waiting for another join target.", FALSE);
			return FALSE;
		}

		if ( !__xrt_co_is_terminal(pCo) ) {
			__xrt_co_join_pin_acquire(pCo);
			pCurrent->__pJoinTarget = pCo;
			pCurrent->__iWaitKind = __XRT_CO_WAIT_JOIN;
			__xrt_co_wait_queue_push_tail(&pCo->__pJoinWaitHead, &pCo->__pJoinWaitTail, pCurrent);
			pCurrent->iState = XRT_CO_SUSPENDED;
			__xrt_co_swap_to_main(pRuntime);
		}

		if ( pCurrent->__pJoinTarget == pCo && __xrt_co_is_terminal(pCo) ) {
			pCurrent->__pJoinTarget = NULL;
			if ( pCurrent->__iWaitKind == __XRT_CO_WAIT_JOIN ) {
				pCurrent->__iWaitKind = __XRT_CO_WAIT_NONE;
			}
			__xrt_co_join_pin_release(pCo);
		}

		return __xrt_co_is_terminal(pCo);
	}

	if ( pCo->__pSched != NULL ) {
		bool bJoined = FALSE;
		__xrt_co_join_pin_acquire(pCo);
		bJoined = __xrt_co_sched_run_until((xcosched*)pCo->__pSched, pCo);
		__xrt_co_join_pin_release(pCo);
		return bJoined;
	}

	while ( !__xrt_co_is_terminal(pCo) ) {
		if ( !xrtCoResume(pCo) ) {
			return __xrt_co_is_terminal(pCo);
		}
	}

	return TRUE;
}

XXAPI void xrtCoExit(int64 iExitCode)
{
	xcoro pCo = __xrt_co_get_current();
	xrtCoroRuntimeState* pRuntime = __xrt_co_require_runtime(FALSE);

	if ( pCo == NULL || pRuntime == NULL ) {
		xrtSetError("xrtCoExit must be called from inside a coroutine.", FALSE);
		return;
	}

	__xrt_co_finish(
		pCo,
		__xrt_co_is_cancel_requested_flag(pCo) ? XRT_CO_TERM_CANCELLED : XRT_CO_TERM_RETURNED,
		iExitCode
	);
	__xrt_co_swap_to_main(pRuntime);
}

XXAPI void xrtCoSetUserData(xcoro pCo, ptr pData)
{
	if ( pCo == NULL ) {
		return;
	}

	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return;
	}

	pCo->pUserData = pData;
}

XXAPI ptr xrtCoGetUserData(xcoro pCo)
{
	if ( !pCo ) return NULL;
	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return NULL;
	}
	return pCo->pUserData;
}

XXAPI bool xrtCoPushCleanup(xco_cleanup_proc proc, ptr pArg)
{
	xcoro pCo = __xrt_co_get_current();
	xco_cleanup* pCleanup = NULL;

	if ( pCo == NULL ) {
		xrtSetError("xrtCoPushCleanup must be called from inside a coroutine.", FALSE);
		return FALSE;
	}

	if ( proc == NULL ) {
		xrtSetError("coroutine cleanup proc is null.", FALSE);
		return FALSE;
	}

	pCleanup = (xco_cleanup*)xrtMalloc(sizeof(xco_cleanup));
	if ( pCleanup == NULL ) {
		return FALSE;
	}

	pCleanup->Proc = proc;
	pCleanup->Arg = pArg;
	pCleanup->pPrev = pCo->__pCleanupTop;
	pCo->__pCleanupTop = pCleanup;
	return TRUE;
}

XXAPI bool xrtCoPopCleanup(xco_cleanup_proc proc, ptr pArg, bool bExecute)
{
	xcoro pCo = __xrt_co_get_current();
	xco_cleanup* pCleanup = NULL;

	if ( pCo == NULL ) {
		xrtSetError("xrtCoPopCleanup must be called from inside a coroutine.", FALSE);
		return FALSE;
	}

	if ( pCo->__pCleanupTop == NULL ) {
		xrtSetError("coroutine cleanup stack is empty.", FALSE);
		return FALSE;
	}

	pCleanup = pCo->__pCleanupTop;
	if ( pCleanup->Proc != proc || pCleanup->Arg != pArg ) {
		xrtSetError("coroutine cleanup pop requires the current top cleanup.", FALSE);
		return FALSE;
	}

	pCo->__pCleanupTop = pCleanup->pPrev;
	if ( bExecute && pCleanup->Proc ) {
		pCo->__iFlags |= __XRT_CO_FLAG_IN_CLEANUP;
		pCleanup->Proc(pCleanup->Arg);
		pCo->__iFlags &= ~__XRT_CO_FLAG_IN_CLEANUP;
	}
	xrtFree(pCleanup);
	return TRUE;
}



static bool __xrt_co_check_sched_owner(xcosched* pSched, str sError)
{
	if ( pSched == NULL ) {
		xrtSetError("invalid coroutine scheduler.", FALSE);
		return FALSE;
	}

	return __xrt_co_check_owner_tid(pSched->iOwnerThreadId, sError);
}


XXAPI xcosched* xrtCoSchedCreate()
{
	xrtThreadData* pThreadData = __xrt_co_require_thread_data(TRUE);
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime_from_thread(pThreadData);
	xcosched* pSched = NULL;

	if ( pThreadData == NULL || pRuntime == NULL ) {
		return NULL;
	}

	pSched = (xcosched*)xrtMalloc(sizeof(xcosched));
	if ( !pSched ) return NULL;

	memset(pSched, 0, sizeof(xcosched));

	pSched->arrCoros = (xcoro*)xrtMalloc(sizeof(xcoro) * __XRT_CO_SCHED_INIT_CAP);
	if ( !pSched->arrCoros ) {
		xrtFree(pSched);
		return NULL;
	}

	memset(pSched->arrCoros, 0, sizeof(xcoro) * __XRT_CO_SCHED_INIT_CAP);

	pSched->arrTimers = (xcoro*)xrtMalloc(sizeof(xcoro) * __XRT_CO_TIMER_INIT_CAP);
	if ( !pSched->arrTimers ) {
		xrtFree(pSched->arrCoros);
		xrtFree(pSched);
		return NULL;
	}

	memset(pSched->arrTimers, 0, sizeof(xcoro) * __XRT_CO_TIMER_INIT_CAP);

	pSched->pPostMutex = xrtMutexCreate();
	if ( pSched->pPostMutex == NULL ) {
		xrtFree(pSched->arrTimers);
		xrtFree(pSched->arrCoros);
		xrtFree(pSched);
		return NULL;
	}

	pSched->pPostCond = xrtCondCreate();
	if ( pSched->pPostCond == NULL ) {
		xrtMutexDestroy(pSched->pPostMutex);
		xrtFree(pSched->arrTimers);
		xrtFree(pSched->arrCoros);
		xrtFree(pSched);
		return NULL;
	}

	pSched->pThreadData = pThreadData;
	pSched->iOwnerThreadId = pThreadData->iThreadId;
	pSched->iCount = 0;
	pSched->iCapacity = __XRT_CO_SCHED_INIT_CAP;
	pSched->iAlive = 0;
	pSched->pReadyHead = NULL;
	pSched->pReadyTail = NULL;
	pSched->iTimerCount = 0;
	pSched->iTimerCapacity = __XRT_CO_TIMER_INIT_CAP;

	if ( pRuntime->pDefaultSched == NULL ) {
		pRuntime->pDefaultSched = pSched;
	}

	return pSched;
}


XXAPI xcosched* xrtCoSchedCurrent()
{
	xcoro pCurrent = __xrt_co_get_current();
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime();

	if ( pCurrent && pCurrent->__pSched ) {
		return (xcosched*)pCurrent->__pSched;
	}

	return pRuntime ? (xcosched*)pRuntime->pDefaultSched : NULL;
}


XXAPI void xrtCoSchedDestroy(xcosched* pSched)
{
	xrtCoroRuntimeState* pRuntime = NULL;

	if ( pSched == NULL ) {
		return;
	}

	if ( !__xrt_co_check_sched_owner(pSched, "scheduler belongs to another thread.") ) {
		return;
	}

	if ( __xrt_co_get_current() != NULL ) {
		xrtSetError("cannot destroy scheduler while a coroutine is running.", FALSE);
		return;
	}

	pRuntime = __xrt_co_get_runtime();

	__xrt_co_sched_drain_posts(pSched);
	while ( pSched->pPostFree != NULL ) {
		__xrt_co_post_item* pNext = pSched->pPostFree->pNext;
		xrtFree(pSched->pPostFree);
		pSched->pPostFree = pNext;
	}

	// 销毁所有协程
	for ( int i = 0; i < pSched->iCount; i++ ) {
		if ( pSched->arrCoros[i] ) {
			__xrt_co_sched_ready_unlink(pSched, pSched->arrCoros[i]);
			__xrt_co_sched_detach_timer(pSched, pSched->arrCoros[i]);
			pSched->arrCoros[i]->__pSched = NULL;
			__xrt_co_destroy_raw(pSched->arrCoros[i]);
			pSched->arrCoros[i] = NULL;
		}
	}

	if ( pRuntime && pRuntime->pDefaultSched == pSched ) {
		pRuntime->pDefaultSched = NULL;
	}

	if ( pSched->pPostCond ) {
		xrtCondDestroy(pSched->pPostCond);
	}
	if ( pSched->pPostMutex ) {
		xrtMutexDestroy(pSched->pPostMutex);
	}
	xrtFree(pSched->arrCoros);
	xrtFree(pSched->arrTimers);
	xrtFree(pSched);
}


XXAPI xcoro xrtCoSchedSpawn(xcosched* pSched, xco_entry pfnEntry, ptr pParam, size_t iStackSize)
{
	xcoro pCo = NULL;

	if ( pSched == NULL ) {
		xrtSetError("invalid coroutine scheduler.", FALSE);
		return NULL;
	}

	if ( !__xrt_co_check_sched_owner(pSched, "scheduler belongs to another thread.") ) {
		return NULL;
	}

	// 扩容检查
	if ( pSched->iCount >= pSched->iCapacity ) {
		int iNewCap = pSched->iCapacity * 2;
		xcoro* pNewArr = (xcoro*)xrtRealloc(pSched->arrCoros, sizeof(xcoro) * iNewCap);
		if ( !pNewArr ) return NULL;
		pSched->arrCoros = pNewArr;
		pSched->iCapacity = iNewCap;
	}

	pCo = xrtCoCreate(pfnEntry, pParam, iStackSize);
	if ( !pCo ) return NULL;

	pCo->__pSched = pSched;
	pSched->arrCoros[pSched->iCount] = pCo;
	pSched->iCount++;
	pSched->iAlive++;
	__xrt_co_sched_mark_ready(pSched, pCo);

	return pCo;
}


XXAPI bool xrtCoSchedPost(xcosched* pSched, xcoro pCo)
{
	__xrt_co_post_item* pItem = NULL;

	if ( pSched == NULL ) {
		xrtSetError("invalid coroutine scheduler.", FALSE);
		return FALSE;
	}

	if ( pCo && pCo->__pSched != pSched ) {
		xrtSetError("coroutine does not belong to target scheduler.", FALSE);
		return FALSE;
	}

	if ( pCo && pCo->iState == XRT_CO_DEAD ) {
		return TRUE;
	}

	pItem = __xrt_co_sched_post_item_alloc(pSched);
	if ( pItem == NULL ) {
		return FALSE;
	}

	pItem->pCo = pCo;
	pItem->pNext = NULL;

	xrtMutexLock(pSched->pPostMutex);
	if ( pSched->pPostTail ) {
		pSched->pPostTail->pNext = pItem;
	}
	else {
		pSched->pPostHead = pItem;
	}
	pSched->pPostTail = pItem;
	xrtCondSignal(pSched->pPostCond);
	xrtMutexUnlock(pSched->pPostMutex);
	return TRUE;
}


XXAPI bool xrtCoSchedPollOnce(xcosched* pSched, uint32 iTimeout)
{
	int64 iNow = 0;
	uint32 iWaitTimeout = 0;
	xcoro pCo = NULL;
	int iIndex = 0;

	if ( pSched == NULL ) {
		xrtSetError("invalid coroutine scheduler.", FALSE);
		return FALSE;
	}

	if ( !__xrt_co_check_sched_owner(pSched, "scheduler belongs to another thread.") ) {
		return FALSE;
	}

	if ( __xrt_co_get_current() != NULL ) {
		xrtSetError("cannot poll scheduler from inside a running coroutine.", FALSE);
		return FALSE;
	}

	if ( pSched->iAlive <= 0 ) return FALSE;

	iNow = __xrt_co_time_ms();
	__xrt_co_sched_drain_posts(pSched);
	__xrt_co_sched_collect_expired_timers(pSched, iNow);

	if ( pSched->pReadyHead == NULL ) {
		iWaitTimeout = __xrt_co_sched_compute_wait_timeout(pSched, iTimeout);
		if ( iWaitTimeout > 0 ) {
			(void)__xrt_co_sched_wait_for_post(pSched, iWaitTimeout);
			iNow = __xrt_co_time_ms();
			__xrt_co_sched_drain_posts(pSched);
			__xrt_co_sched_collect_expired_timers(pSched, iNow);
		}
	}

	while ( pSched->pReadyHead != NULL ) {
		pCo = pSched->pReadyHead;
		__xrt_co_sched_ready_unlink(pSched, pCo);

		if ( pCo == NULL ) {
			break;
		}

		if ( pCo->iState == XRT_CO_DEAD ) {
			iIndex = __xrt_co_sched_find_index(pSched, pCo);
			if ( iIndex >= 0 ) {
				__xrt_co_sched_reap_dead(pSched, iIndex);
			}
			continue;
		}

		if ( __xrt_co_is_cancel_requested_flag(pCo) && (pCo->__iFlags & __XRT_CO_FLAG_STARTED) == 0 ) {
			__xrt_co_finish(pCo, XRT_CO_TERM_CANCELLED, -1);
			iIndex = __xrt_co_sched_find_index(pSched, pCo);
			if ( iIndex >= 0 ) {
				__xrt_co_sched_reap_dead(pSched, iIndex);
			}
			continue;
		}

		xrtCoResume(pCo);
		__xrt_co_sched_on_return(pSched, pCo, __xrt_co_time_ms());
		break;
	}

	return pSched->iAlive > 0;
}


XXAPI bool xrtCoSchedStep(xcosched* pSched)
{
	return xrtCoSchedPollOnce(pSched, 0);
}


XXAPI void xrtCoSchedRun(xcosched* pSched)
{
	if ( pSched == NULL ) {
		return;
	}

	if ( !__xrt_co_check_sched_owner(pSched, "scheduler belongs to another thread.") ) {
		return;
	}

	while ( pSched->iAlive > 0 ) {
		if ( !xrtCoSchedPollOnce(pSched, __XRT_CO_SCHED_WAIT_FOREVER) ) {
			if ( pSched->iAlive <= 0 ) {
				break;
			}
		}
	}
}


XXAPI int xrtCoSchedGetAlive(xcosched* pSched)
{
	if ( !pSched ) return 0;
	return pSched->iAlive;
}

XXAPI void xrtCoSleepUntil(int64 iDeadlineMs)
{
	xcoro pCo = __xrt_co_get_current();

	if ( !pCo ) {
		xrtSetError("xrtCoSleepUntil must be called from inside a coroutine.", FALSE);
		return;
	}

	if ( pCo->__pSched == NULL ) {
		xrtSetError("xrtCoSleepUntil requires a scheduler-managed coroutine.", FALSE);
		return;
	}

	pCo->__iWakeTime = iDeadlineMs;
	pCo->__iWaitKind = __XRT_CO_WAIT_TIMER;
	xrtCoYield();
}

XXAPI bool xrtCoWaitDeadline(int64 iDeadlineMs)
{
	xcoro pCo = __xrt_co_get_current();

	if ( !pCo ) {
		xrtSetError("xrtCoWaitDeadline must be called from inside a coroutine.", FALSE);
		return FALSE;
	}

	if ( pCo->__pSched == NULL ) {
		xrtSetError("xrtCoWaitDeadline requires a scheduler-managed coroutine.", FALSE);
		return FALSE;
	}

	if ( __xrt_co_is_cancel_requested_flag(pCo) ) {
		return FALSE;
	}

	if ( iDeadlineMs > __xrt_co_time_ms() ) {
		pCo->__iWakeTime = iDeadlineMs;
		pCo->__iWaitKind = __XRT_CO_WAIT_TIMER;
		xrtCoYield();
	}

	return !__xrt_co_is_cancel_requested_flag(pCo);
}

XXAPI xcoevent xrtCoEventCreate(bool bManualReset, bool bInitialState)
{
	xcoevent pEvent = (xcoevent)xrtCalloc(1, sizeof(xcoevent_struct));

	if ( pEvent == NULL ) {
		return NULL;
	}

	pEvent->pLock = xrtMutexCreate();
	if ( pEvent->pLock == NULL ) {
		xrtFree(pEvent);
		return NULL;
	}

	pEvent->bManualReset = bManualReset;
	pEvent->bSignaled = bInitialState;
	pEvent->pWaitHead = NULL;
	pEvent->pWaitTail = NULL;
	return pEvent;
}

XXAPI void xrtCoEventDestroy(xcoevent pEvent)
{
	if ( pEvent == NULL ) {
		return;
	}

	if ( pEvent->pLock ) {
		xrtMutexLock(pEvent->pLock);
		if ( pEvent->pWaitHead != NULL ) {
			xrtMutexUnlock(pEvent->pLock);
			xrtSetError("cannot destroy coroutine event while a waiter is attached.", FALSE);
			return;
		}
		xrtMutexUnlock(pEvent->pLock);
		xrtMutexDestroy(pEvent->pLock);
	}

	xrtFree(pEvent);
}

XXAPI void xrtCoEventSet(xcoevent pEvent)
{
	if ( pEvent == NULL ) {
		xrtSetError("invalid coroutine event.", FALSE);
		return;
	}

	if ( pEvent->pLock == NULL ) {
		xrtSetError("coroutine event lock is missing.", FALSE);
		return;
	}

	for ( ;; ) {
		xcoro pWaiter = NULL;
		xcosched* pSched = NULL;
		bool bWakeAll = FALSE;

		xrtMutexLock(pEvent->pLock);
		bWakeAll = pEvent->bManualReset;
		if ( pEvent->bManualReset ) {
			pEvent->bSignaled = TRUE;
		}

		pWaiter = __xrt_co_pop_event_waiter_locked(pEvent);
		if ( pWaiter == NULL && !pEvent->bManualReset ) {
			pEvent->bSignaled = TRUE;
		}
		if ( pWaiter != NULL ) {
			pWaiter->__iWaitResult = __XRT_CO_WAIT_RESULT_SIGNAL;
			pSched = (xcosched*)pWaiter->__pSched;
		}
		xrtMutexUnlock(pEvent->pLock);

		if ( pWaiter != NULL && pSched != NULL ) {
			(void)xrtCoSchedPost(pSched, pWaiter);
		}

		if ( !bWakeAll || pWaiter == NULL ) {
			break;
		}
	}
}

XXAPI void xrtCoEventReset(xcoevent pEvent)
{
	if ( pEvent == NULL ) {
		xrtSetError("invalid coroutine event.", FALSE);
		return;
	}

	if ( pEvent->pLock == NULL ) {
		xrtSetError("coroutine event lock is missing.", FALSE);
		return;
	}

	xrtMutexLock(pEvent->pLock);
	pEvent->bSignaled = FALSE;
	xrtMutexUnlock(pEvent->pLock);
}

static bool __xrt_co_wait_event_core(xcoevent pEvent, bool bInfinite, int64 iDeadlineMs)
{
	xcoro pCo = __xrt_co_get_current();
	int64 iNowMs = 0;

	if ( pEvent == NULL ) {
		xrtSetError("invalid coroutine event.", FALSE);
		return FALSE;
	}

	if ( pCo == NULL ) {
		xrtSetError("xrtCoWaitEvent must be called from inside a coroutine.", FALSE);
		return FALSE;
	}

	if ( pCo->__pSched == NULL ) {
		xrtSetError("xrtCoWaitEvent requires a scheduler-managed coroutine.", FALSE);
		return FALSE;
	}

	if ( __xrt_co_is_cancel_requested_flag(pCo) ) {
		return FALSE;
	}

	if ( pEvent->pLock == NULL ) {
		xrtSetError("coroutine event lock is missing.", FALSE);
		return FALSE;
	}

	pCo->__iWaitResult = __XRT_CO_WAIT_RESULT_NONE;

	xrtMutexLock(pEvent->pLock);

	if ( __xrt_co_event_try_consume_locked(pEvent) ) {
		pCo->__iWaitResult = __XRT_CO_WAIT_RESULT_SIGNAL;
		xrtMutexUnlock(pEvent->pLock);
		return TRUE;
	}

	iNowMs = __xrt_co_time_ms();
	if ( !bInfinite && iDeadlineMs <= iNowMs ) {
		xrtMutexUnlock(pEvent->pLock);
		return FALSE;
	}

	__xrt_co_wait_queue_push_tail(&pEvent->pWaitHead, &pEvent->pWaitTail, pCo);
	pCo->__pWaitObject = pEvent;
	pCo->__iWaitKind = __XRT_CO_WAIT_EVENT;
	if ( bInfinite ) {
		pCo->__iWakeTime = 0;
	}
	else {
		pCo->__iWakeTime = iDeadlineMs;
	}
	xrtMutexUnlock(pEvent->pLock);

	xrtCoYield();

	if ( __xrt_co_is_cancel_requested_flag(pCo) ) {
		return FALSE;
	}

	return pCo->__iWaitResult == __XRT_CO_WAIT_RESULT_SIGNAL;
}

XXAPI bool xrtCoWaitEvent(xcoevent pEvent)
{
	return __xrt_co_wait_event_core(pEvent, TRUE, 0);
}

XXAPI bool xrtCoWaitEventUntil(xcoevent pEvent, int64 iDeadlineMs)
{
	return __xrt_co_wait_event_core(pEvent, FALSE, iDeadlineMs);
}

XXAPI bool xrtCoWaitEventTimeout(xcoevent pEvent, uint32 iTimeoutMs)
{
	if ( iTimeoutMs == XRT_CO_WAIT_INFINITE ) {
		return __xrt_co_wait_event_core(pEvent, TRUE, 0);
	}

	return __xrt_co_wait_event_core(pEvent, FALSE, __xrt_co_time_ms() + (int64)iTimeoutMs);
}


XXAPI void xrtCoSleep(uint32 iMs)
{
	xrtCoSleepUntil(__xrt_co_time_ms() + (int64)iMs);
}
