


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
#define __XRT_CO_BACKEND_STYLE_FIBER		3

#if defined(_MSC_VER) && !defined(__clang__) && (defined(_WIN32) || defined(_WIN64))
	#define __XRT_CO_FIBER_WIN
	#if defined(_WIN64)
		#define __XRT_CO_BACKEND_NAME	"fiber-win64-msvc"
	#else
		#define __XRT_CO_BACKEND_NAME	"fiber-win32-msvc"
	#endif
	#define __XRT_CO_BACKEND_TIER	__XRT_CO_BACKEND_TIER_PRODUCTION
	#define __XRT_CO_BACKEND_STYLE	__XRT_CO_BACKEND_STYLE_FIBER
#elif defined(__TINYC__)
	#if (defined(_WIN64)) && (defined(__x86_64__) || defined(_M_X64))
		#define __XRT_CO_ASM_X64_WIN
		#define __XRT_CO_BACKEND_NAME	"asm-x64-win64-tcc"
		#define __XRT_CO_BACKEND_TIER	__XRT_CO_BACKEND_TIER_PRODUCTION
		#define __XRT_CO_BACKEND_STYLE	__XRT_CO_BACKEND_STYLE_INLINE_ASM
	#elif !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
		#define __XRT_CO_ASM_X64
		#define __XRT_CO_BACKEND_NAME	"asm-x64-sysv-tcc"
		#define __XRT_CO_BACKEND_TIER	__XRT_CO_BACKEND_TIER_PRODUCTION
		#define __XRT_CO_BACKEND_STYLE	__XRT_CO_BACKEND_STYLE_INLINE_ASM
	#endif
#elif defined(__GNUC__) || defined(__clang__)
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

#ifdef __XRT_CO_FIBER_WIN
	#define __XRT_CO_BACKEND_NEEDS_STACK_ALLOC	0
#else
	#define __XRT_CO_BACKEND_NEEDS_STACK_ALLOC	1
#endif



/* ================================ 线程级协程运行时 ================================ */

static xrtCoroRuntimeState* __xrt_co_get_runtime_from_thread(xrtThreadData* pThreadData)
{
	if ( pThreadData == NULL ) {
		return NULL;
	}
	return &pThreadData->tCoro;
}


// 内部函数：协程 require 线程数据相关处理
static xrtThreadData* __xrt_co_require_thread_data(bool bSetError)
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();

	if ( pThreadData == NULL && bSetError ) {
		xrtSetError("current thread is not attached to xrt runtime.", FALSE);
	}

	return pThreadData;
}


// 内部函数：获取协程运行时
static xrtCoroRuntimeState* __xrt_co_get_runtime()
{
	return __xrt_co_get_runtime_from_thread(xrtThreadGetCurrent());
}


// 内部函数：__xrt_co_require_runtime
static xrtCoroRuntimeState* __xrt_co_require_runtime(bool bSetError)
{
	xrtThreadData* pThreadData = __xrt_co_require_thread_data(bSetError);
	return __xrt_co_get_runtime_from_thread(pThreadData);
}


// 内部函数：获取协程当前
static xcoro __xrt_co_get_current()
{
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime();

	if ( pRuntime == NULL ) {
		return NULL;
	}

	return (xcoro)pRuntime->pCurrent;
}


// 内部函数：设置协程当前
static void __xrt_co_set_current(xcoro pCo)
{
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime();

	if ( pRuntime ) {
		pRuntime->pCurrent = pCo;
	}
}


// 内部函数：检查协程所有权 tid
static bool __xrt_co_check_owner_tid(uint64 iOwnerThreadId, const char* sError)
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


// 内部函数：检查协程所有权
static bool __xrt_co_check_owner(xcoro pCo, const char* sError)
{
	if ( pCo == NULL ) {
		xrtSetError("invalid coroutine handle.", FALSE);
		return FALSE;
	}

	return __xrt_co_check_owner_tid(pCo->__iOwnerThreadId, sError);
}


// 内部函数：初始化 coro 运行时线程
static void __xrtCoroRuntimeInitThread(xrtThreadData* pThreadData)
{
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime_from_thread(pThreadData);

	if ( pRuntime == NULL ) {
		return;
	}

	memset(pRuntime, 0, sizeof(xrtCoroRuntimeState));
	pRuntime->iOwnerThreadId = pThreadData->iThreadId;
}


// 内部函数：释放 coro 运行时线程
static void __xrtCoroRuntimeUnitThread(xrtThreadData* pThreadData)
{
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime_from_thread(pThreadData);

	if ( pRuntime == NULL ) {
		return;
	}

	#ifdef __XRT_CO_FIBER_WIN
		if ( (pRuntime->iFlags & XRT_CO_RUNTIME_FIBER_CONVERTED) != 0 ) {
			(void)ConvertFiberToThread();
		}
	#elif defined(__XRT_CO_ASM_X64_WIN) || defined(__XRT_CO_ASM_X64) || defined(__XRT_CO_ASM_ARM64) || defined(__XRT_CO_ASM_RV64) || defined(__XRT_CO_ASM_LA64)
		xrtFree(pRuntime->pBackendMain);
	#endif

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

// 内部函数：__xrt_co_is_terminal
static bool __xrt_co_is_terminal(xcoro pCo)
{
	return pCo && pCo->iState == XRT_CO_DEAD;
}


// 内部函数：__xrt_co_is_cancel_requested_flag
static bool __xrt_co_is_cancel_requested_flag(xcoro pCo)
{
	return pCo && ((pCo->__iFlags & __XRT_CO_FLAG_CANCEL_REQUESTED) != 0);
}

static bool __xrt_co_sched_mark_ready(xcosched* pSched, xcoro pCo);
static bool __xrt_co_sched_detach_timer(xcosched* pSched, xcoro pCo);
static bool __xrt_co_sched_release_dead(xcoro pCo);
static bool __xrt_co_sched_run_until(xcosched* pSched, xcoro pTarget);


// 内部函数：等待协程 clear 状态
static void __xrt_co_clear_wait_state(xcoro pCo)
{
	if ( pCo == NULL ) {
		return;
	}

	pCo->__pWaitObject = NULL;
	pCo->__iWaitKind = __XRT_CO_WAIT_NONE;
}


// 内部函数：__xrt_co_wait_link_clear
static void __xrt_co_wait_link_clear(xcoro pCo)
{
	if ( pCo == NULL ) {
		return;
	}

	pCo->__pWaitPrev = NULL;
	pCo->__pWaitNext = NULL;
}


// 内部函数：__xrt_co_wait_queue_push_tail
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


// 内部函数：删除协程 wait 队列
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


// 内部函数：__xrt_co_wait_queue_pop_head
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


// 内部函数：__xrt_co_join_pin_acquire
static void __xrt_co_join_pin_acquire(xcoro pTarget)
{
	if ( pTarget == NULL ) {
		return;
	}

	pTarget->__iJoinRefCount++;
	pTarget->__iFlags |= __XRT_CO_FLAG_JOIN_PINNED;
}


// 内部函数：__xrt_co_join_pin_release
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


// 内部函数：__xrt_co_detach_join_waiter
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


// 内部函数：__xrt_co_detach_event_waiter_locked
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


// 内部函数：__xrt_co_pop_event_waiter_locked
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


// 内部函数：__xrt_co_event_try_consume_locked
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


// 内部函数：__xrt_co_free_cleanup_nodes
static void __xrt_co_free_cleanup_nodes(xcoro pCo)
{
	while ( pCo && pCo->__pCleanupTop ) {
		xco_cleanup* pCleanup = pCo->__pCleanupTop;
		pCo->__pCleanupTop = pCleanup->pPrev;
		xrtFree(pCleanup);
	}
}


// 内部函数：__xrt_co_run_cleanup
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


// 内部函数：获取协程栈页大小
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


// 内部函数：__xrt_co_align_up
static size_t __xrt_co_align_up(size_t iValue, size_t iAlign)
{
	if ( iAlign == 0 ) {
		return iValue;
	}
	return (iValue + iAlign - 1) & ~(iAlign - 1);
}


// 内部函数：规范化协程栈大小
static size_t __xrt_co_normalize_stack_size(size_t iStackSize)
{
	size_t iPageSize = __xrt_co_stack_page_size();

	if ( iStackSize == 0 ) {
		iStackSize = XRT_CO_STACK_DEFAULT;
	}
	if ( iStackSize < XRT_CO_STACK_MIN ) {
		iStackSize = XRT_CO_STACK_MIN;
	}
	if ( iStackSize > XRT_CO_STACK_MAX ) {
		iStackSize = XRT_CO_STACK_MAX;
	}

	return __xrt_co_align_up(iStackSize, iPageSize);
}


// 内部函数：分配协程栈
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
	
	// 计算保护页大小并规范化协程栈大小
	iPageSize = __xrt_co_stack_page_size();
	iGuardSize = iPageSize;
	iStackSize = __xrt_co_normalize_stack_size(pCo->iStackSize);
	
	// 检查栈大小是否溢出
	if ( iStackSize > (SIZE_MAX - iGuardSize) ) {
		xrtSetError("coroutine stack size overflow.", FALSE);
		return FALSE;
	}
	iAllocSize = iStackSize + iGuardSize;
	
	// 分配栈内存并设置栈底保护页
	#if defined(_WIN32) || defined(_WIN64)
		{
			DWORD iOldProtect = 0;
			
			// 提交可读写内存区域
			pBase = VirtualAlloc(NULL, iAllocSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			if ( pBase == NULL ) {
				xrtSetError("failed to reserve coroutine stack.", FALSE);
				return FALSE;
			}
			
			// 将栈底页面设为不可访问，防止栈溢出
			if ( !VirtualProtect(pBase, iGuardSize, PAGE_NOACCESS, &iOldProtect) ) {
				VirtualFree(pBase, 0, MEM_RELEASE);
				xrtSetError("failed to protect coroutine guard page.", FALSE);
				return FALSE;
			}
		}
	#else
		// 映射可读写匿名内存区域
		pBase = mmap(NULL, iAllocSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if ( pBase == MAP_FAILED ) {
			xrtSetError("failed to reserve coroutine stack.", FALSE);
			return FALSE;
		}
		
		// 将栈底页面设为不可访问，防止栈溢出
		if ( mprotect(pBase, iGuardSize, PROT_NONE) != 0 ) {
			munmap(pBase, iAllocSize);
			xrtSetError("failed to protect coroutine guard page.", FALSE);
			return FALSE;
		}
	#endif
	
	// 记录栈内存布局信息
	pCo->__pStackMem = pBase;
	pCo->__iStackAllocSize = iAllocSize;
	pCo->__iStackGuardSize = iGuardSize;
	pCo->__pStack = (uint8*)pBase + iGuardSize;
	pCo->iStackSize = iStackSize;
	return TRUE;
}


// 内部函数：释放协程栈
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


// 内部函数：__xrt_co_wake_join_waiters
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


// 内部函数：完成协程
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


// 内部函数：__xrt_co_finalize_ready_cancel
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


// 内部函数：休眠协程毫秒
static void UNUSED_ATTR __xrt_co_sleep_ms(int iMs)
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

#define __XRT_CO_JMP_RDX	"jmp *0x00(%%rdx)\n\t"
#define __XRT_CO_JMP_RSI	"jmp *0x00(%%rsi)\n\t"

#if defined(__TINYC__) && defined(__XRT_CO_ASM_X64_WIN)
	/*
		TCC x64 Windows inline asm 只能解析部分 XMM 指令/寄存器名。
		这里让 xmm6/xmm7 继续走可读语法，其余高位寄存器使用 .byte 编码。
	*/
	#define __XRT_CO_WIN64_SAVE_XMM6	"movups %%xmm6,  0x50(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM7	"movups %%xmm7,  0x60(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM8	".byte 0x44,0x0f,0x11,0x41,0x70\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM9	".byte 0x44,0x0f,0x11,0x89,0x80,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM10	".byte 0x44,0x0f,0x11,0x91,0x90,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM11	".byte 0x44,0x0f,0x11,0x99,0xA0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM12	".byte 0x44,0x0f,0x11,0xA1,0xB0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM13	".byte 0x44,0x0f,0x11,0xA9,0xC0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM14	".byte 0x44,0x0f,0x11,0xB1,0xD0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM15	".byte 0x44,0x0f,0x11,0xB9,0xE0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM15	".byte 0x44,0x0f,0x10,0xBA,0xE0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM14	".byte 0x44,0x0f,0x10,0xB2,0xD0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM13	".byte 0x44,0x0f,0x10,0xAA,0xC0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM12	".byte 0x44,0x0f,0x10,0xA2,0xB0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM11	".byte 0x44,0x0f,0x10,0x9A,0xA0,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM10	".byte 0x44,0x0f,0x10,0x92,0x90,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM9	".byte 0x44,0x0f,0x10,0x8A,0x80,0x00,0x00,0x00\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM8	".byte 0x44,0x0f,0x10,0x42,0x70\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM7	"movups 0x60(%%rdx), %%xmm7\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM6	"movups 0x50(%%rdx), %%xmm6\n\t"
#else
	#define __XRT_CO_WIN64_SAVE_XMM6	"movdqu %%xmm6,  0x50(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM7	"movdqu %%xmm7,  0x60(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM8	"movdqu %%xmm8,  0x70(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM9	"movdqu %%xmm9,  0x80(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM10	"movdqu %%xmm10, 0x90(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM11	"movdqu %%xmm11, 0xA0(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM12	"movdqu %%xmm12, 0xB0(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM13	"movdqu %%xmm13, 0xC0(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM14	"movdqu %%xmm14, 0xD0(%%rcx)\n\t"
	#define __XRT_CO_WIN64_SAVE_XMM15	"movdqu %%xmm15, 0xE0(%%rcx)\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM15	"movdqu 0xE0(%%rdx), %%xmm15\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM14	"movdqu 0xD0(%%rdx), %%xmm14\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM13	"movdqu 0xC0(%%rdx), %%xmm13\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM12	"movdqu 0xB0(%%rdx), %%xmm12\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM11	"movdqu 0xA0(%%rdx), %%xmm11\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM10	"movdqu 0x90(%%rdx), %%xmm10\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM9	"movdqu 0x80(%%rdx), %%xmm9\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM8	"movdqu 0x70(%%rdx), %%xmm8\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM7	"movdqu 0x60(%%rdx), %%xmm7\n\t"
	#define __XRT_CO_WIN64_LOAD_XMM6	"movdqu 0x50(%%rdx), %%xmm6\n\t"
#endif

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

// 协程上下文切换 - Windows x64 ABI 版本
// 流程：保存当前寄存器到 pFrom → 从 pTo 恢复目标寄存器 → 跳转到目标恢复点
// rcx = pFrom, rdx = pTo，所有 callee-saved 寄存器按固定偏移存取
static void __xrt_co_swap(__xrt_co_ctx* pFrom, __xrt_co_ctx* pTo)
{
	__asm__ volatile (
		// --- 保存当前上下文到 pFrom ---
		"leaq 1f(%%rip), %%rax\n\t"       // 计算恢复点地址（标号1）
		"movq %%rax, 0x00(%%rcx)\n\t"     // 保存 rip
		"movq %%rsp, 0x08(%%rcx)\n\t"     // 保存 rsp
		"movq %%rbp, 0x10(%%rcx)\n\t"     // 保存 rbp
		"movq %%rbx, 0x18(%%rcx)\n\t"     // 保存 rbx
		"movq %%rdi, 0x20(%%rcx)\n\t"     // 保存 rdi
		"movq %%rsi, 0x28(%%rcx)\n\t"     // 保存 rsi
		"movq %%r12, 0x30(%%rcx)\n\t"     // 保存 r12
		"movq %%r13, 0x38(%%rcx)\n\t"     // 保存 r13
		"movq %%r14, 0x40(%%rcx)\n\t"     // 保存 r14
		"movq %%r15, 0x48(%%rcx)\n\t"     // 保存 r15
		__XRT_CO_WIN64_SAVE_XMM6          // 保存 xmm6 ~ xmm15
		__XRT_CO_WIN64_SAVE_XMM7
		__XRT_CO_WIN64_SAVE_XMM8
		__XRT_CO_WIN64_SAVE_XMM9
		__XRT_CO_WIN64_SAVE_XMM10
		__XRT_CO_WIN64_SAVE_XMM11
		__XRT_CO_WIN64_SAVE_XMM12
		__XRT_CO_WIN64_SAVE_XMM13
		__XRT_CO_WIN64_SAVE_XMM14
		__XRT_CO_WIN64_SAVE_XMM15
		// --- 从 pTo 恢复目标上下文 ---
		__XRT_CO_WIN64_LOAD_XMM15         // 恢复 xmm15 ~ xmm6
		__XRT_CO_WIN64_LOAD_XMM14
		__XRT_CO_WIN64_LOAD_XMM13
		__XRT_CO_WIN64_LOAD_XMM12
		__XRT_CO_WIN64_LOAD_XMM11
		__XRT_CO_WIN64_LOAD_XMM10
		__XRT_CO_WIN64_LOAD_XMM9
		__XRT_CO_WIN64_LOAD_XMM8
		__XRT_CO_WIN64_LOAD_XMM7
		__XRT_CO_WIN64_LOAD_XMM6
		"movq 0x48(%%rdx), %%r15\n\t"     // 恢复 r15
		"movq 0x40(%%rdx), %%r14\n\t"     // 恢复 r14
		"movq 0x38(%%rdx), %%r13\n\t"     // 恢复 r13
		"movq 0x30(%%rdx), %%r12\n\t"     // 恢复 r12
		"movq 0x28(%%rdx), %%rsi\n\t"     // 恢复 rsi
		"movq 0x20(%%rdx), %%rdi\n\t"     // 恢复 rdi
		"movq 0x18(%%rdx), %%rbx\n\t"     // 恢复 rbx
		"movq 0x10(%%rdx), %%rbp\n\t"     // 恢复 rbp
		"movq 0x08(%%rdx), %%rsp\n\t"     // 恢复 rsp
		__XRT_CO_JMP_RDX                   // 跳转到 pTo 中保存的恢复点
		"1:\n\t"                           // 恢复点：被 swap 回来时从此处继续
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

// 协程上下文切换 - x86_64 System V ABI 版本
// 流程：保存当前寄存器到 pFrom → 从 pTo 恢复目标寄存器 → 跳转到目标恢复点
// rdi = pFrom, rsi = pTo，所有 callee-saved 寄存器按固定偏移存取
static void __xrt_co_swap(__xrt_co_ctx* pFrom, __xrt_co_ctx* pTo)
{
	__asm__ volatile (
		// --- 保存当前上下文到 pFrom ---
		"leaq 1f(%%rip), %%rax\n\t"       // 计算恢复点地址（标号1）
		"movq %%rax, 0x00(%%rdi)\n\t"     // 保存 rip
		"movq %%rsp, 0x08(%%rdi)\n\t"     // 保存 rsp
		"movq %%rbp, 0x10(%%rdi)\n\t"     // 保存 rbp
		"movq %%rbx, 0x18(%%rdi)\n\t"     // 保存 rbx
		"movq %%r12, 0x20(%%rdi)\n\t"     // 保存 r12
		"movq %%r13, 0x28(%%rdi)\n\t"     // 保存 r13
		"movq %%r14, 0x30(%%rdi)\n\t"     // 保存 r14
		"movq %%r15, 0x38(%%rdi)\n\t"     // 保存 r15
		// --- 从 pTo 恢复目标上下文 ---
		"movq 0x38(%%rsi), %%r15\n\t"     // 恢复 r15
		"movq 0x30(%%rsi), %%r14\n\t"     // 恢复 r14
		"movq 0x28(%%rsi), %%r13\n\t"     // 恢复 r13
		"movq 0x20(%%rsi), %%r12\n\t"     // 恢复 r12
		"movq 0x18(%%rsi), %%rbx\n\t"     // 恢复 rbx
		"movq 0x10(%%rsi), %%rbp\n\t"     // 恢复 rbp
		"movq 0x08(%%rsi), %%rsp\n\t"     // 恢复 rsp
		__XRT_CO_JMP_RSI                   // 跳转到 pTo 中保存的恢复点
		"1:\n\t"                           // 恢复点：被 swap 回来时从此处继续
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

// 协程上下文切换 - ARM64 AAPCS64 版本
// 流程：保存当前寄存器到 pFrom → 从 pTo 恢复目标寄存器 → 跳转到目标恢复点
static __attribute__((noinline)) void __xrt_co_swap(__xrt_co_ctx* pFrom, __xrt_co_ctx* pTo)
{
	__asm__ volatile (
		"mov x9, %0\n\t"
		"mov x10, %1\n\t"
		// --- 保存当前上下文到 pFrom ---
		"adr x2, 1f\n\t"                  // 计算恢复点地址
		"mov x3, sp\n\t"                   // 获取当前 sp
		"stp x2,  x3,  [x9, #0x00]\n\t"   // 保存 恢复点 + sp
		"stp x19, x20, [x9, #0x10]\n\t"   // 保存 x19, x20
		"stp x21, x22, [x9, #0x20]\n\t"   // 保存 x21, x22
		"stp x23, x24, [x9, #0x30]\n\t"   // 保存 x23, x24
		"stp x25, x26, [x9, #0x40]\n\t"   // 保存 x25, x26
		"stp x27, x28, [x9, #0x50]\n\t"   // 保存 x27, x28
		"stp x29, x30, [x9, #0x60]\n\t"   // 保存 fp(x29) + lr(x30)
		"stp q8,  q9,  [x9, #0x70]\n\t"   // 保存 q8, q9
		"stp q10, q11, [x9, #0x90]\n\t"   // 保存 q10, q11
		"stp q12, q13, [x9, #0xB0]\n\t"   // 保存 q12, q13
		"stp q14, q15, [x9, #0xD0]\n\t"   // 保存 q14, q15
		// --- 从 pTo 恢复目标上下文 ---
		"ldp q14, q15, [x10, #0xD0]\n\t"  // 恢复 q14, q15
		"ldp q12, q13, [x10, #0xB0]\n\t"  // 恢复 q12, q13
		"ldp q10, q11, [x10, #0x90]\n\t"  // 恢复 q10, q11
		"ldp q8,  q9,  [x10, #0x70]\n\t"  // 恢复 q8, q9
		"ldp x29, x30, [x10, #0x60]\n\t"  // 恢复 fp(x29) + lr(x30)
		"ldp x27, x28, [x10, #0x50]\n\t"  // 恢复 x27, x28
		"ldp x25, x26, [x10, #0x40]\n\t"  // 恢复 x25, x26
		"ldp x23, x24, [x10, #0x30]\n\t"  // 恢复 x23, x24
		"ldp x21, x22, [x10, #0x20]\n\t"  // 恢复 x21, x22
		"ldp x19, x20, [x10, #0x10]\n\t"  // 恢复 x19, x20
		"ldp x2,  x3,  [x10, #0x00]\n\t"  // 读取 恢复点 + sp
		"mov sp, x3\n\t"                   // 恢复 sp
		"br x2\n\t"                        // 跳转到恢复点
		"1:\n\t"                           // 恢复点：被 swap 回来时从此处继续
		: : "r"(pFrom), "r"(pTo)
		: "memory", "x2", "x3", "x9", "x10"
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

// 协程上下文切换 - RISC-V 64 版本
// 流程：保存当前寄存器到 pFrom → 从 pTo 恢复目标寄存器 → 跳转到目标恢复点
static void __xrt_co_swap(__xrt_co_ctx* pFrom, __xrt_co_ctx* pTo)
{
	#ifdef __XRT_CO_RV64_FP64
		// 双精度浮点 ABI 版本：额外保存/恢复 fs0 ~ fs11
		__asm__ volatile (
			// --- 保存当前上下文到 pFrom ---
			"la t0, 1f\n\t"
			"sd t0,   0x00(%0)\n\t"           // 保存恢复点地址
			"sd sp,   0x08(%0)\n\t"           // 保存 sp
			"sd s0,   0x10(%0)\n\t"           // 保存 s0(fp)
			"sd s1,   0x18(%0)\n\t"           // 保存 s1
			"sd s2,   0x20(%0)\n\t"           // 保存 s2
			"sd s3,   0x28(%0)\n\t"           // 保存 s3
			"sd s4,   0x30(%0)\n\t"           // 保存 s4
			"sd s5,   0x38(%0)\n\t"           // 保存 s5
			"sd s6,   0x40(%0)\n\t"           // 保存 s6
			"sd s7,   0x48(%0)\n\t"           // 保存 s7
			"sd s8,   0x50(%0)\n\t"           // 保存 s8
			"sd s9,   0x58(%0)\n\t"           // 保存 s9
			"sd s10,  0x60(%0)\n\t"           // 保存 s10
			"sd s11,  0x68(%0)\n\t"           // 保存 s11
			"fsd fs0,  0x70(%0)\n\t"          // 保存 fs0
			"fsd fs1,  0x78(%0)\n\t"          // 保存 fs1
			"fsd fs2,  0x80(%0)\n\t"          // 保存 fs2
			"fsd fs3,  0x88(%0)\n\t"          // 保存 fs3
			"fsd fs4,  0x90(%0)\n\t"          // 保存 fs4
			"fsd fs5,  0x98(%0)\n\t"          // 保存 fs5
			"fsd fs6,  0xA0(%0)\n\t"          // 保存 fs6
			"fsd fs7,  0xA8(%0)\n\t"          // 保存 fs7
			"fsd fs8,  0xB0(%0)\n\t"          // 保存 fs8
			"fsd fs9,  0xB8(%0)\n\t"          // 保存 fs9
			"fsd fs10, 0xC0(%0)\n\t"          // 保存 fs10
			"fsd fs11, 0xC8(%0)\n\t"          // 保存 fs11
			// --- 从 pTo 恢复目标上下文 ---
			"fld fs11, 0xC8(%1)\n\t"          // 恢复 fs11
			"fld fs10, 0xC0(%1)\n\t"          // 恢复 fs10
			"fld fs9,  0xB8(%1)\n\t"          // 恢复 fs9
			"fld fs8,  0xB0(%1)\n\t"          // 恢复 fs8
			"fld fs7,  0xA8(%1)\n\t"          // 恢复 fs7
			"fld fs6,  0xA0(%1)\n\t"          // 恢复 fs6
			"fld fs5,  0x98(%1)\n\t"          // 恢复 fs5
			"fld fs4,  0x90(%1)\n\t"          // 恢复 fs4
			"fld fs3,  0x88(%1)\n\t"          // 恢复 fs3
			"fld fs2,  0x80(%1)\n\t"          // 恢复 fs2
			"fld fs1,  0x78(%1)\n\t"          // 恢复 fs1
			"fld fs0,  0x70(%1)\n\t"          // 恢复 fs0
			"ld s11,   0x68(%1)\n\t"          // 恢复 s11
			"ld s10,   0x60(%1)\n\t"          // 恢复 s10
			"ld s9,    0x58(%1)\n\t"          // 恢复 s9
			"ld s8,    0x50(%1)\n\t"          // 恢复 s8
			"ld s7,    0x48(%1)\n\t"          // 恢复 s7
			"ld s6,    0x40(%1)\n\t"          // 恢复 s6
			"ld s5,    0x38(%1)\n\t"          // 恢复 s5
			"ld s4,    0x30(%1)\n\t"          // 恢复 s4
			"ld s3,    0x28(%1)\n\t"          // 恢复 s3
			"ld s2,    0x20(%1)\n\t"          // 恢复 s2
			"ld s1,    0x18(%1)\n\t"          // 恢复 s1
			"ld s0,    0x10(%1)\n\t"          // 恢复 s0(fp)
			"ld sp,    0x08(%1)\n\t"          // 恢复 sp
			"ld t0,    0x00(%1)\n\t"          // 读取恢复点地址
			"jr t0\n\t"                       // 跳转到恢复点
			"1:\n\t"                          // 恢复点：被 swap 回来时从此处继续
			: : "r"(pFrom), "r"(pTo)
			: "memory", "t0", "t1", "t2", "t3", "t4", "t5", "t6",
			  "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
		);
	#elif defined(__XRT_CO_RV64_FP32)
		// 单精度浮点 ABI 版本：使用 fsw/flw 保存/恢复浮点寄存器
		__asm__ volatile (
			// --- 保存当前上下文到 pFrom ---
			"la t0, 1f\n\t"
			"sd t0,   0x00(%0)\n\t"           // 保存恢复点地址
			"sd sp,   0x08(%0)\n\t"           // 保存 sp
			"sd s0,   0x10(%0)\n\t"           // 保存 s0(fp)
			"sd s1,   0x18(%0)\n\t"           // 保存 s1
			"sd s2,   0x20(%0)\n\t"           // 保存 s2
			"sd s3,   0x28(%0)\n\t"           // 保存 s3
			"sd s4,   0x30(%0)\n\t"           // 保存 s4
			"sd s5,   0x38(%0)\n\t"           // 保存 s5
			"sd s6,   0x40(%0)\n\t"           // 保存 s6
			"sd s7,   0x48(%0)\n\t"           // 保存 s7
			"sd s8,   0x50(%0)\n\t"           // 保存 s8
			"sd s9,   0x58(%0)\n\t"           // 保存 s9
			"sd s10,  0x60(%0)\n\t"           // 保存 s10
			"sd s11,  0x68(%0)\n\t"           // 保存 s11
			"fsw fs0,  0x70(%0)\n\t"          // 保存 fs0
			"fsw fs1,  0x78(%0)\n\t"          // 保存 fs1
			"fsw fs2,  0x80(%0)\n\t"          // 保存 fs2
			"fsw fs3,  0x88(%0)\n\t"          // 保存 fs3
			"fsw fs4,  0x90(%0)\n\t"          // 保存 fs4
			"fsw fs5,  0x98(%0)\n\t"          // 保存 fs5
			"fsw fs6,  0xA0(%0)\n\t"          // 保存 fs6
			"fsw fs7,  0xA8(%0)\n\t"          // 保存 fs7
			"fsw fs8,  0xB0(%0)\n\t"          // 保存 fs8
			"fsw fs9,  0xB8(%0)\n\t"          // 保存 fs9
			"fsw fs10, 0xC0(%0)\n\t"          // 保存 fs10
			"fsw fs11, 0xC8(%0)\n\t"          // 保存 fs11
			// --- 从 pTo 恢复目标上下文 ---
			"flw fs11, 0xC8(%1)\n\t"          // 恢复 fs11
			"flw fs10, 0xC0(%1)\n\t"          // 恢复 fs10
			"flw fs9,  0xB8(%1)\n\t"          // 恢复 fs9
			"flw fs8,  0xB0(%1)\n\t"          // 恢复 fs8
			"flw fs7,  0xA8(%1)\n\t"          // 恢复 fs7
			"flw fs6,  0xA0(%1)\n\t"          // 恢复 fs6
			"flw fs5,  0x98(%1)\n\t"          // 恢复 fs5
			"flw fs4,  0x90(%1)\n\t"          // 恢复 fs4
			"flw fs3,  0x88(%1)\n\t"          // 恢复 fs3
			"flw fs2,  0x80(%1)\n\t"          // 恢复 fs2
			"flw fs1,  0x78(%1)\n\t"          // 恢复 fs1
			"flw fs0,  0x70(%1)\n\t"          // 恢复 fs0
			"ld s11,   0x68(%1)\n\t"          // 恢复 s11
			"ld s10,   0x60(%1)\n\t"          // 恢复 s10
			"ld s9,    0x58(%1)\n\t"          // 恢复 s9
			"ld s8,    0x50(%1)\n\t"          // 恢复 s8
			"ld s7,    0x48(%1)\n\t"          // 恢复 s7
			"ld s6,    0x40(%1)\n\t"          // 恢复 s6
			"ld s5,    0x38(%1)\n\t"          // 恢复 s5
			"ld s4,    0x30(%1)\n\t"          // 恢复 s4
			"ld s3,    0x28(%1)\n\t"          // 恢复 s3
			"ld s2,    0x20(%1)\n\t"          // 恢复 s2
			"ld s1,    0x18(%1)\n\t"          // 恢复 s1
			"ld s0,    0x10(%1)\n\t"          // 恢复 s0(fp)
			"ld sp,    0x08(%1)\n\t"          // 恢复 sp
			"ld t0,    0x00(%1)\n\t"          // 读取恢复点地址
			"jr t0\n\t"                       // 跳转到恢复点
			"1:\n\t"                          // 恢复点：被 swap 回来时从此处继续
			: : "r"(pFrom), "r"(pTo)
			: "memory", "t0", "t1", "t2", "t3", "t4", "t5", "t6",
			  "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
		);
	#else
	// 软浮点 ABI 版本：仅保存/恢复整数 callee-saved 寄存器
	__asm__ volatile (
		// --- 保存当前上下文到 pFrom ---
		"la t0, 1f\n\t"
		"sd t0,  0x00(%0)\n\t"               // 保存恢复点地址
		"sd sp,  0x08(%0)\n\t"               // 保存 sp
		"sd s0,  0x10(%0)\n\t"               // 保存 s0(fp)
		"sd s1,  0x18(%0)\n\t"               // 保存 s1
		"sd s2,  0x20(%0)\n\t"               // 保存 s2
		"sd s3,  0x28(%0)\n\t"               // 保存 s3
		"sd s4,  0x30(%0)\n\t"               // 保存 s4
		"sd s5,  0x38(%0)\n\t"               // 保存 s5
		"sd s6,  0x40(%0)\n\t"               // 保存 s6
		"sd s7,  0x48(%0)\n\t"               // 保存 s7
		"sd s8,  0x50(%0)\n\t"               // 保存 s8
		"sd s9,  0x58(%0)\n\t"               // 保存 s9
		"sd s10, 0x60(%0)\n\t"               // 保存 s10
		"sd s11, 0x68(%0)\n\t"               // 保存 s11
		// --- 从 pTo 恢复目标上下文 ---
		"ld s11, 0x68(%1)\n\t"               // 恢复 s11
		"ld s10, 0x60(%1)\n\t"               // 恢复 s10
		"ld s9,  0x58(%1)\n\t"               // 恢复 s9
		"ld s8,  0x50(%1)\n\t"               // 恢复 s8
		"ld s7,  0x48(%1)\n\t"               // 恢复 s7
		"ld s6,  0x40(%1)\n\t"               // 恢复 s6
		"ld s5,  0x38(%1)\n\t"               // 恢复 s5
		"ld s4,  0x30(%1)\n\t"               // 恢复 s4
		"ld s3,  0x28(%1)\n\t"               // 恢复 s3
		"ld s2,  0x20(%1)\n\t"               // 恢复 s2
		"ld s1,  0x18(%1)\n\t"               // 恢复 s1
		"ld s0,  0x10(%1)\n\t"               // 恢复 s0(fp)
		"ld sp,  0x08(%1)\n\t"               // 恢复 sp
		"ld t0,  0x00(%1)\n\t"               // 读取恢复点地址
		"jr t0\n\t"                          // 跳转到恢复点
		"1:\n\t"                             // 恢复点：被 swap 回来时从此处继续
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

// 协程上下文切换 - LoongArch64 版本
// 流程：保存当前寄存器到 pFrom → 从 pTo 恢复目标寄存器 → 跳转到目标恢复点
static void __xrt_co_swap(__xrt_co_ctx* pFrom, __xrt_co_ctx* pTo)
{
	#ifdef __XRT_CO_LA64_FP64
		// 双精度浮点 ABI 版本：额外保存/恢复 fs0 ~ fs7
		__asm__ volatile (
			// --- 保存当前上下文到 pFrom ---
			"la.local $t0, 1f\n\t"
			"st.d   $t0,   %0, 0x00\n\t"     // 保存恢复点地址
			"st.d   $sp,   %0, 0x08\n\t"     // 保存 sp
			"st.d   $fp,   %0, 0x10\n\t"     // 保存 fp
			"st.d   $s0,   %0, 0x18\n\t"     // 保存 s0
			"st.d   $s1,   %0, 0x20\n\t"     // 保存 s1
			"st.d   $s2,   %0, 0x28\n\t"     // 保存 s2
			"st.d   $s3,   %0, 0x30\n\t"     // 保存 s3
			"st.d   $s4,   %0, 0x38\n\t"     // 保存 s4
			"st.d   $s5,   %0, 0x40\n\t"     // 保存 s5
			"st.d   $s6,   %0, 0x48\n\t"     // 保存 s6
			"st.d   $s7,   %0, 0x50\n\t"     // 保存 s7
			"st.d   $s8,   %0, 0x58\n\t"     // 保存 s8
			"fst.d  $fs0,  %0, 0x60\n\t"     // 保存 fs0
			"fst.d  $fs1,  %0, 0x68\n\t"     // 保存 fs1
			"fst.d  $fs2,  %0, 0x70\n\t"     // 保存 fs2
			"fst.d  $fs3,  %0, 0x78\n\t"     // 保存 fs3
			"fst.d  $fs4,  %0, 0x80\n\t"     // 保存 fs4
			"fst.d  $fs5,  %0, 0x88\n\t"     // 保存 fs5
			"fst.d  $fs6,  %0, 0x90\n\t"     // 保存 fs6
			"fst.d  $fs7,  %0, 0x98\n\t"     // 保存 fs7
			// --- 从 pTo 恢复目标上下文 ---
			"fld.d  $fs7,  %1, 0x98\n\t"     // 恢复 fs7
			"fld.d  $fs6,  %1, 0x90\n\t"     // 恢复 fs6
			"fld.d  $fs5,  %1, 0x88\n\t"     // 恢复 fs5
			"fld.d  $fs4,  %1, 0x80\n\t"     // 恢复 fs4
			"fld.d  $fs3,  %1, 0x78\n\t"     // 恢复 fs3
			"fld.d  $fs2,  %1, 0x70\n\t"     // 恢复 fs2
			"fld.d  $fs1,  %1, 0x68\n\t"     // 恢复 fs1
			"fld.d  $fs0,  %1, 0x60\n\t"     // 恢复 fs0
			"ld.d   $s8,   %1, 0x58\n\t"     // 恢复 s8
			"ld.d   $s7,   %1, 0x50\n\t"     // 恢复 s7
			"ld.d   $s6,   %1, 0x48\n\t"     // 恢复 s6
			"ld.d   $s5,   %1, 0x40\n\t"     // 恢复 s5
			"ld.d   $s4,   %1, 0x38\n\t"     // 恢复 s4
			"ld.d   $s3,   %1, 0x30\n\t"     // 恢复 s3
			"ld.d   $s2,   %1, 0x28\n\t"     // 恢复 s2
			"ld.d   $s1,   %1, 0x20\n\t"     // 恢复 s1
			"ld.d   $fp,   %1, 0x10\n\t"     // 恢复 fp
			"ld.d   $s0,   %1, 0x18\n\t"     // 恢复 s0
			"ld.d   $sp,   %1, 0x08\n\t"     // 恢复 sp
			"ld.d   $t0,   %1, 0x00\n\t"     // 读取恢复点地址
			"jr $t0\n\t"                      // 跳转到恢复点
			"1:\n\t"                          // 恢复点：被 swap 回来时从此处继续
			: : "r"(pFrom), "r"(pTo)
			: "memory", "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8"
		);
	#elif defined(__XRT_CO_LA64_FP32)
		// 单精度浮点 ABI 版本：使用 fst.s/fld.s 保存/恢复浮点寄存器
		__asm__ volatile (
			// --- 保存当前上下文到 pFrom ---
			"la.local $t0, 1f\n\t"
			"st.d   $t0,   %0, 0x00\n\t"     // 保存恢复点地址
			"st.d   $sp,   %0, 0x08\n\t"     // 保存 sp
			"st.d   $fp,   %0, 0x10\n\t"     // 保存 fp
			"st.d   $s0,   %0, 0x18\n\t"     // 保存 s0
			"st.d   $s1,   %0, 0x20\n\t"     // 保存 s1
			"st.d   $s2,   %0, 0x28\n\t"     // 保存 s2
			"st.d   $s3,   %0, 0x30\n\t"     // 保存 s3
			"st.d   $s4,   %0, 0x38\n\t"     // 保存 s4
			"st.d   $s5,   %0, 0x40\n\t"     // 保存 s5
			"st.d   $s6,   %0, 0x48\n\t"     // 保存 s6
			"st.d   $s7,   %0, 0x50\n\t"     // 保存 s7
			"st.d   $s8,   %0, 0x58\n\t"     // 保存 s8
			"fst.s  $fs0,  %0, 0x60\n\t"     // 保存 fs0
			"fst.s  $fs1,  %0, 0x68\n\t"     // 保存 fs1
			"fst.s  $fs2,  %0, 0x70\n\t"     // 保存 fs2
			"fst.s  $fs3,  %0, 0x78\n\t"     // 保存 fs3
			"fst.s  $fs4,  %0, 0x80\n\t"     // 保存 fs4
			"fst.s  $fs5,  %0, 0x88\n\t"     // 保存 fs5
			"fst.s  $fs6,  %0, 0x90\n\t"     // 保存 fs6
			"fst.s  $fs7,  %0, 0x98\n\t"     // 保存 fs7
			// --- 从 pTo 恢复目标上下文 ---
			"fld.s  $fs7,  %1, 0x98\n\t"     // 恢复 fs7
			"fld.s  $fs6,  %1, 0x90\n\t"     // 恢复 fs6
			"fld.s  $fs5,  %1, 0x88\n\t"     // 恢复 fs5
			"fld.s  $fs4,  %1, 0x80\n\t"     // 恢复 fs4
			"fld.s  $fs3,  %1, 0x78\n\t"     // 恢复 fs3
			"fld.s  $fs2,  %1, 0x70\n\t"     // 恢复 fs2
			"fld.s  $fs1,  %1, 0x68\n\t"     // 恢复 fs1
			"fld.s  $fs0,  %1, 0x60\n\t"     // 恢复 fs0
			"ld.d   $s8,   %1, 0x58\n\t"     // 恢复 s8
			"ld.d   $s7,   %1, 0x50\n\t"     // 恢复 s7
			"ld.d   $s6,   %1, 0x48\n\t"     // 恢复 s6
			"ld.d   $s5,   %1, 0x40\n\t"     // 恢复 s5
			"ld.d   $s4,   %1, 0x38\n\t"     // 恢复 s4
			"ld.d   $s3,   %1, 0x30\n\t"     // 恢复 s3
			"ld.d   $s2,   %1, 0x28\n\t"     // 恢复 s2
			"ld.d   $s1,   %1, 0x20\n\t"     // 恢复 s1
			"ld.d   $fp,   %1, 0x10\n\t"     // 恢复 fp
			"ld.d   $s0,   %1, 0x18\n\t"     // 恢复 s0
			"ld.d   $sp,   %1, 0x08\n\t"     // 恢复 sp
			"ld.d   $t0,   %1, 0x00\n\t"     // 读取恢复点地址
			"jr $t0\n\t"                      // 跳转到恢复点
			"1:\n\t"                          // 恢复点：被 swap 回来时从此处继续
			: : "r"(pFrom), "r"(pTo)
			: "memory", "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8"
		);
	#else
	// 软浮点 ABI 版本：仅保存/恢复整数 callee-saved 寄存器
	__asm__ volatile (
		// --- 保存当前上下文到 pFrom ---
		"la.local $t0, 1f\n\t"
		"st.d $t0,  %0, 0x00\n\t"               // 保存恢复点地址
		"st.d $sp,  %0, 0x08\n\t"               // 保存 sp
		"st.d $fp,  %0, 0x10\n\t"               // 保存 fp
		"st.d $s0,  %0, 0x18\n\t"               // 保存 s0
		"st.d $s1,  %0, 0x20\n\t"               // 保存 s1
		"st.d $s2,  %0, 0x28\n\t"               // 保存 s2
		"st.d $s3,  %0, 0x30\n\t"               // 保存 s3
		"st.d $s4,  %0, 0x38\n\t"               // 保存 s4
		"st.d $s5,  %0, 0x40\n\t"               // 保存 s5
		"st.d $s6,  %0, 0x48\n\t"               // 保存 s6
		"st.d $s7,  %0, 0x50\n\t"               // 保存 s7
		"st.d $s8,  %0, 0x58\n\t"               // 保存 s8
		// --- 从 pTo 恢复目标上下文 ---
		"ld.d $s8,  %1, 0x58\n\t"               // 恢复 s8
		"ld.d $s7,  %1, 0x50\n\t"               // 恢复 s7
		"ld.d $s6,  %1, 0x48\n\t"               // 恢复 s6
		"ld.d $s5,  %1, 0x40\n\t"               // 恢复 s5
		"ld.d $s4,  %1, 0x38\n\t"               // 恢复 s4
		"ld.d $s3,  %1, 0x30\n\t"               // 恢复 s3
		"ld.d $s2,  %1, 0x28\n\t"               // 恢复 s2
		"ld.d $s1,  %1, 0x20\n\t"               // 恢复 s1
		"ld.d $s0,  %1, 0x18\n\t"               // 恢复 s0
		"ld.d $fp,  %1, 0x10\n\t"               // 恢复 fp
		"ld.d $sp,  %1, 0x08\n\t"               // 恢复 sp
		"ld.d $t0,  %1, 0x00\n\t"               // 读取恢复点地址
		"jr $t0\n\t"                            // 跳转到恢复点
		"1:\n\t"                                // 恢复点：被 swap 回来时从此处继续
		: : "r"(pFrom), "r"(pTo)
		: "memory", "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8"
	);
	#endif
}

#endif



/* ================================ 后端实现: Windows Fiber ================================ */

#ifdef __XRT_CO_FIBER_WIN

// 内部函数：__xrt_co_prepare_backend_main
static bool __xrt_co_prepare_backend_main(xrtCoroRuntimeState* pRuntime)
{
	if ( pRuntime == NULL ) {
		return FALSE;
	}

	if ( pRuntime->pBackendMain != NULL ) {
		return TRUE;
	}

	if ( IsThreadAFiber() ) {
		pRuntime->pBackendMain = GetCurrentFiber();
		pRuntime->iFlags |= XRT_CO_RUNTIME_FIBER_HOSTED;
		return TRUE;
	}

	pRuntime->pBackendMain = ConvertThreadToFiber(NULL);
	if ( pRuntime->pBackendMain == NULL ) {
		xrtSetError("failed to convert current thread to fiber.", FALSE);
		return FALSE;
	}

	pRuntime->iFlags |= XRT_CO_RUNTIME_FIBER_CONVERTED;
	return TRUE;
}


// 内部函数：__xrt_co_fiber_entry
static VOID CALLBACK __xrt_co_fiber_entry(LPVOID pParameter)
{
	xcoro pCo = (xcoro)pParameter;
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime();

	if ( pCo == NULL || pRuntime == NULL || pRuntime->pBackendMain == NULL ) {
		return;
	}

	pCo->pfnEntry(pCo->pParam);
	__xrt_co_finish(pCo, __xrt_co_is_cancel_requested_flag(pCo) ? XRT_CO_TERM_CANCELLED : XRT_CO_TERM_RETURNED, 0);
	SwitchToFiber(pRuntime->pBackendMain);
}


// 内部函数：__xrt_co_init_ctx
static bool __xrt_co_init_ctx(xcoro pCo)
{
	if ( pCo == NULL ) {
		return FALSE;
	}

	pCo->__hFiber = CreateFiber(pCo->iStackSize, __xrt_co_fiber_entry, pCo);
	if ( pCo->__hFiber == NULL ) {
		xrtSetError("failed to create coroutine fiber.", FALSE);
		return FALSE;
	}

	return TRUE;
}


// 内部函数：__xrt_co_swap_to_co
static void __xrt_co_swap_to_co(xrtCoroRuntimeState* pRuntime, xcoro pCo)
{
	if ( pRuntime && pRuntime->pBackendMain && pCo && pCo->__hFiber ) {
		SwitchToFiber(pCo->__hFiber);
	}
}


// 内部函数：__xrt_co_swap_to_main
static void __xrt_co_swap_to_main(xrtCoroRuntimeState* pRuntime)
{
	if ( pRuntime && pRuntime->pBackendMain ) {
		SwitchToFiber(pRuntime->pBackendMain);
	}
}

#endif



/* ================================ 汇编后端: 入口 + 初始化 + swap 包装 ================================ */

#if defined(__XRT_CO_ASM_X64_WIN) || defined(__XRT_CO_ASM_X64) || defined(__XRT_CO_ASM_ARM64) || defined(__XRT_CO_ASM_RV64) || defined(__XRT_CO_ASM_LA64)

// 内部函数：__xrt_co_prepare_backend_main
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
static void __xrt_co_asm_entry(void)
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


// 内部函数：__xrt_co_init_ctx
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


// 内部函数：__xrt_co_swap_to_co
static void __xrt_co_swap_to_co(xrtCoroRuntimeState* pRuntime, xcoro pCo)
{
	__xrt_co_swap((__xrt_co_ctx*)pRuntime->pBackendMain, &pCo->__tCtx);
}


// 内部函数：__xrt_co_swap_to_main
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

	#ifdef __XRT_CO_FIBER_WIN
		if ( pCo->__hFiber != NULL ) {
			DeleteFiber(pCo->__hFiber);
			pCo->__hFiber = NULL;
		}
	#endif

	__xrt_co_stack_free(pCo);

	__xrt_co_free_cleanup_nodes(pCo);
	__xrtTempArenaFreeDetached(&pCo->__tTemp);

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
	iStackSize = __xrt_co_normalize_stack_size(iStackSize);

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

	if ( !__xrt_co_prepare_backend_main(pRuntime) ) {
		xrtFree(pCo);
		return NULL;
	}

	#if __XRT_CO_BACKEND_NEEDS_STACK_ALLOC
		if ( !__xrt_co_stack_alloc(pCo) ) {
			xrtFree(pCo);
			return NULL;
		}
	#endif

	if ( !__xrt_co_init_ctx(pCo) ) {
		__xrt_co_destroy_raw(pCo);
		return NULL;
	}

	return pCo;
}


// 创建协程
XXAPI xcoro xrtCoCreate(xco_entry pfnEntry, ptr pParam, size_t iStackSize)
{
	xco_create_args tArgs;

	memset(&tArgs, 0, sizeof(tArgs));
	tArgs.iStackSize = iStackSize;
	return xrtCoCreateEx(pfnEntry, pParam, &tArgs);
}


// 销毁协程
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


// 取消协程
XXAPI bool xrtCoCancel(xcoro pCo)
{
	if ( pCo == NULL ) {
		xrtSetError("invalid coroutine handle.", FALSE);
		return FALSE;
	}

	// 检查协程所有权
	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return FALSE;
	}

	// 已经终止的协程直接返回成功
	if ( __xrt_co_is_terminal(pCo) ) {
		return TRUE;
	}

	// 设置取消请求标志
	pCo->__iFlags |= __XRT_CO_FLAG_CANCEL_REQUESTED;

	// 如果是当前协程自己取消自己，只需设置标志
	if ( pCo == __xrt_co_get_current() ) {
		return TRUE;
	}

	// 对未启动的 READY 协程直接完成取消
	if ( pCo->iState == XRT_CO_READY && (pCo->__iFlags & __XRT_CO_FLAG_STARTED) == 0 ) {
		__xrt_co_finalize_ready_cancel(pCo);
		return TRUE;
	}

	// 解除 join 等待关系
	if ( pCo->__pJoinTarget != NULL ) {
		__xrt_co_detach_join_waiter(pCo, TRUE);
	}

	// 解除事件等待关系
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

	// 清除定时器唤醒时间
	if ( pCo->__iWakeTime > 0 ) {
		pCo->__iWakeTime = 0;
	}

	// 从调度器中移除定时器并标记为就绪
	if ( pCo->__pSched != NULL ) {
		__xrt_co_sched_detach_timer((xcosched*)pCo->__pSched, pCo);
		__xrt_co_sched_mark_ready((xcosched*)pCo->__pSched, pCo);
	}

	return TRUE;
}


// 关闭协程
XXAPI bool xrtCoClose(xcoro pCo)
{
	if ( pCo == NULL ) {
		xrtSetError("invalid coroutine handle.", FALSE);
		return FALSE;
	}

	// 检查协程所有权
	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return FALSE;
	}

	// 若协程被调度器管理，走调度器的关闭路径
	if ( pCo->__pSched != NULL ) {
		if ( pCo->iState == XRT_CO_DEAD ) {
			return __xrt_co_sched_release_dead(pCo);
		}
		pCo->__iFlags |= __XRT_CO_FLAG_CLOSE_REQUESTED;
		return xrtCoCancel(pCo);
	}

	// 无调度器时要求协程处于 READY 或 DEAD 状态
	if ( (pCo->iState != XRT_CO_READY) && (pCo->iState != XRT_CO_DEAD) ) {
		xrtSetError("close requires READY or DEAD coroutine when no scheduler owns it.", FALSE);
		return FALSE;
	}

	xrtCoDestroy(pCo);
	return TRUE;
}


// xrtCoResume 相关处理
XXAPI bool xrtCoResume(xcoro pCo)
{
	xrtThreadData* pThreadData = NULL;
	xrtCoroRuntimeState* pRuntime = NULL;

	if ( pCo == NULL ) {
		xrtSetError("invalid coroutine handle.", FALSE);
		return FALSE;
	}

	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return FALSE;
	}

	pThreadData = __xrt_co_require_thread_data(TRUE);
	pRuntime = __xrt_co_get_runtime_from_thread(pThreadData);
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

	// coroutine 与宿主拥有独立临时内存区；切换时只移动状态，不复制区块。
	pRuntime->tHostTemp = pThreadData->tTemp;
	pRuntime->bTempArenaSwapped = TRUE;
	pThreadData->tTemp = pCo->__tTemp;
	memset(&pCo->__tTemp, 0, sizeof(pCo->__tTemp));
	__xrt_co_swap_to_co(pRuntime, pCo);
	pCo->__tTemp = pThreadData->tTemp;
	pThreadData->tTemp = pRuntime->tHostTemp;
	memset(&pRuntime->tHostTemp, 0, sizeof(pRuntime->tHostTemp));
	pRuntime->bTempArenaSwapped = FALSE;

	// 从这里恢复意味着协程 yield 了或者结束了
	pRuntime->pCurrent = NULL;
	return TRUE;
}


// xrtCoYield 相关处理
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


// 获取协程状态
XXAPI int xrtCoGetState(xcoro pCo)
{
	if ( !pCo ) { return XRT_CO_DEAD; }
	return pCo->iState;
}


// 获取协程当前
XXAPI xcoro xrtCoGetCurrent()
{
	return __xrt_co_get_current();
}


// xrtCoIsCancelRequested 相关处理
XXAPI bool xrtCoIsCancelRequested()
{
	xcoro pCo = __xrt_co_get_current();
	return pCo ? __xrt_co_is_cancel_requested_flag(pCo) : FALSE;
}


// xrtCoWasCancelled 相关处理
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


// xrtCoGetExitCode 相关处理
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


// 设置协程结果
XXAPI void xrtCoSetResult(ptr pResult)
{
	xcoro pCo = __xrt_co_get_current();

	if ( pCo == NULL ) {
		xrtSetError("xrtCoSetResult must be called from inside a coroutine.", FALSE);
		return;
	}

	pCo->pResult = pResult;
}


// 获取协程结果
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


// 获取协程 backend 名称
XXAPI str xrtCoGetBackendName()
{
	return (str)__XRT_CO_BACKEND_NAME;
}


// xrtCoGetBackendTier 相关处理
XXAPI int xrtCoGetBackendTier()
{
	return __XRT_CO_BACKEND_TIER;
}


// xrtCoGetBackendStyle 相关处理
XXAPI int xrtCoGetBackendStyle()
{
	return __XRT_CO_BACKEND_STYLE;
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


// 内部函数：__xrt_co_sched_find_index
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


// 内部函数：__xrt_co_sched_ready_unlink
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


// 内部函数：确保协程 sched 定时器容量
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


// 内部函数：__xrt_co_sched_timer_swap
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


// 内部函数：__xrt_co_sched_timer_sift_up
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


// 内部函数：定时器堆向下调整（小根堆）
static void __xrt_co_sched_timer_sift_down(xcosched* pSched, int iIndex)
{
	while ( pSched ) {
		int iLeft = iIndex * 2 + 1;
		int iRight = iLeft + 1;
		int iSmallest = iIndex;

		// 找左子节点中的最小唤醒时间
		if ( iLeft < pSched->iTimerCount &&
			pSched->arrTimers[iLeft] &&
			pSched->arrTimers[iSmallest] &&
			pSched->arrTimers[iLeft]->__iWakeTime < pSched->arrTimers[iSmallest]->__iWakeTime ) {
			iSmallest = iLeft;
		}

		// 找右子节点中的最小唤醒时间
		if ( iRight < pSched->iTimerCount &&
			pSched->arrTimers[iRight] &&
			pSched->arrTimers[iSmallest] &&
			pSched->arrTimers[iRight]->__iWakeTime < pSched->arrTimers[iSmallest]->__iWakeTime ) {
			iSmallest = iRight;
		}

		// 如果当前节点已经是最小，堆性质满足
		if ( iSmallest == iIndex ) {
			break;
		}

		// 交换并继续向下调整
		__xrt_co_sched_timer_swap(pSched, iIndex, iSmallest);
		iIndex = iSmallest;
	}
}


// 内部函数：从定时器堆中移除指定协程
static bool __xrt_co_sched_detach_timer(xcosched* pSched, xcoro pCo)
{
	int iIndex = 0;
	int iLast = 0;

	if ( pSched == NULL || pCo == NULL || (pCo->__iFlags & __XRT_CO_FLAG_TIMER_QUEUED) == 0 || pCo->__iTimerIndex == 0 ) {
		return FALSE;
	}

	// 计算堆中的位置索引
	iIndex = (int)pCo->__iTimerIndex - 1;
	iLast = pSched->iTimerCount - 1;

	// 索引越界检查
	if ( iIndex < 0 || iIndex >= pSched->iTimerCount ) {
		pCo->__iTimerIndex = 0;
		pCo->__iFlags &= ~__XRT_CO_FLAG_TIMER_QUEUED;
		return FALSE;
	}

	// 将目标元素交换到堆尾
	if ( iIndex != iLast ) {
		__xrt_co_sched_timer_swap(pSched, iIndex, iLast);
	}

	// 移除堆尾元素
	pSched->arrTimers[iLast] = NULL;
	pSched->iTimerCount--;

	// 清除协程的定时器状态
	pCo->__iTimerIndex = 0;
	pCo->__iFlags &= ~__XRT_CO_FLAG_TIMER_QUEUED;

	// 重新调整堆以恢复堆性质
	if ( iIndex < pSched->iTimerCount ) {
		__xrt_co_sched_timer_sift_down(pSched, iIndex);
		__xrt_co_sched_timer_sift_up(pSched, iIndex);
	}

	return TRUE;
}


// 内部函数：将协程加入定时器堆（或更新已有位置）
static bool __xrt_co_sched_attach_timer(xcosched* pSched, xcoro pCo)
{
	int iIndex = 0;

	if ( pSched == NULL || pCo == NULL || pCo->iState == XRT_CO_DEAD || pCo->__iWakeTime <= 0 ) {
		return FALSE;
	}

	// 先从就绪队列中移除
	__xrt_co_sched_ready_unlink(pSched, pCo);

	// 如果已在定时器堆中，更新位置
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

	// 确保堆容量足够
	if ( !__xrt_co_sched_timer_ensure_capacity(pSched) ) {
		return FALSE;
	}

	// 将协程追加到堆尾并向上调整
	iIndex = pSched->iTimerCount++;
	pSched->arrTimers[iIndex] = pCo;
	pCo->__iFlags |= __XRT_CO_FLAG_TIMER_QUEUED;
	pCo->__iTimerIndex = (uint32)(iIndex + 1);
	pCo->iState = XRT_CO_SUSPENDED;
	__xrt_co_sched_timer_sift_up(pSched, iIndex);
	return TRUE;
}


// 内部函数：将协程标记为就绪并加入就绪队列
static bool __xrt_co_sched_mark_ready(xcosched* pSched, xcoro pCo)
{
	if ( pSched == NULL || pCo == NULL || pCo->iState == XRT_CO_DEAD || pCo->__pSched != pSched ) {
		return FALSE;
	}

	// join 目标尚未终止时不允许就绪
	if ( pCo->__pJoinTarget != NULL && !__xrt_co_is_terminal(pCo->__pJoinTarget) ) {
		return FALSE;
	}

	// 从定时器堆中移除
	__xrt_co_sched_detach_timer(pSched, pCo);

	// 如果已在就绪队列中，仅更新状态
	if ( pCo->__iFlags & __XRT_CO_FLAG_READY_QUEUED ) {
		pCo->iState = XRT_CO_READY;
		return TRUE;
	}

	// 加入就绪队列尾部
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


// 内部函数：收集已过期的定时器协程并标记为就绪
static void __xrt_co_sched_collect_expired_timers(xcosched* pSched, int64 iNow)
{
	while ( pSched && pSched->iTimerCount > 0 ) {
		xcoro pCo = pSched->arrTimers[0];
		
		if ( pCo == NULL ) {
			break;
		}
		
		// 堆顶元素的唤醒时间未到，无需继续
		if ( pCo->__iWakeTime > iNow ) {
			break;
		}
		
		// 从定时器堆中移除
		__xrt_co_sched_detach_timer(pSched, pCo);
		
		// 处理事件等待超时
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


// 内部函数：回收已终止的协程资源
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
	
	// 从就绪队列和定时器堆中移除
	__xrt_co_sched_ready_unlink(pSched, pCo);
	__xrt_co_sched_detach_timer(pSched, pCo);
	
	// 标记为待回收并减少存活计数
	if ( (pCo->__iFlags & __XRT_CO_FLAG_REAP_PENDING) == 0 ) {
		pCo->__iFlags |= __XRT_CO_FLAG_REAP_PENDING;
		if ( pSched->iAlive > 0 ) {
			pSched->iAlive--;
		}
	}
	
	// 如果仍有 join 引用，保留结构体等待 join 释放
	if ( pCo->__iFlags & __XRT_CO_FLAG_JOIN_PINNED ) {
		return TRUE;
	}
	
	// 无 join 引用，直接销毁
	pCo->__pSched = NULL;
	__xrt_co_destroy_raw(pCo);
	pSched->arrCoros[iIndex] = NULL;
	return TRUE;
}


// 内部函数：释放已终止的协程（外部调用入口）
static bool __xrt_co_sched_release_dead(xcoro pCo)
{
	xcosched* pSched = NULL;
	
	if ( pCo == NULL || pCo->__pSched == NULL || pCo->iState != XRT_CO_DEAD ) {
		return FALSE;
	}

	// 在调度器数组中查找并回收
	pSched = (xcosched*)pCo->__pSched;
	int iIndex = __xrt_co_sched_find_index(pSched, pCo);
	
	if ( iIndex >= 0 ) {
		__xrt_co_sched_ready_unlink(pSched, pCo);
		__xrt_co_sched_detach_timer(pSched, pCo);
		
		// 减少存活计数
		if ( (pCo->__iFlags & __XRT_CO_FLAG_REAP_PENDING) == 0 && pSched->iAlive > 0 ) {
			pSched->iAlive--;
		}
		
		// 清除 join 引用后销毁
		pCo->__iFlags &= ~__XRT_CO_FLAG_JOIN_PINNED;
		pCo->__iFlags |= __XRT_CO_FLAG_REAP_PENDING;
		pCo->__pSched = NULL;
		__xrt_co_destroy_raw(pCo);
		pSched->arrCoros[iIndex] = NULL;
		return TRUE;
	}

	return FALSE;
}


// 内部函数：协程从 Resume 返回后的后处理（状态转换和调度）
static void __xrt_co_sched_on_return(xcosched* pSched, xcoro pCo, int64 iNow)
{
	int iIndex = 0;
	
	if ( pSched == NULL || pCo == NULL ) {
		return;
	}

	// 协程已终止，回收资源
	if ( pCo->iState == XRT_CO_DEAD ) {
		iIndex = __xrt_co_sched_find_index(pSched, pCo);
		if ( iIndex >= 0 ) {
			__xrt_co_sched_reap_dead(pSched, iIndex);
		}
		return;
	}

	// 协程不再属于当前调度器
	if ( pCo->__pSched != pSched ) {
		return;
	}

	// join 等待：目标未终止则挂起当前协程
	if ( pCo->__pJoinTarget != NULL ) {
		if ( !__xrt_co_is_terminal(pCo->__pJoinTarget) ) {
			pCo->iState = XRT_CO_SUSPENDED;
			return;
		}
	}

	// 事件等待且带超时
	if ( pCo->__iWaitKind == __XRT_CO_WAIT_EVENT && pCo->__pWaitObject != NULL ) {
		// 超时未到：挂起并挂到定时器堆
		if ( (pCo->__iWakeTime > 0) && (pCo->__iWakeTime > iNow) ) {
			pCo->iState = XRT_CO_SUSPENDED;
			__xrt_co_sched_attach_timer(pSched, pCo);
			return;
		}
		// 超时已到：解除事件等待并标记就绪
		else if ( pCo->__iWakeTime > 0 ) {
			xcoevent pEvent = (xcoevent)pCo->__pWaitObject;
			if ( pEvent && pEvent->pLock ) {
				xrtMutexLock(pEvent->pLock);
				pCo->__iWaitResult = __XRT_CO_WAIT_RESULT_TIMEOUT;
				__xrt_co_detach_event_waiter_locked(pEvent, pCo);
				xrtMutexUnlock(pEvent->pLock);
			}
			pCo->__iWakeTime = 0;
			__xrt_co_sched_mark_ready(pSched, pCo);
			return;
		}
		// 事件等待无超时：保持挂起
		pCo->iState = XRT_CO_SUSPENDED;
		return;
	}

	// 纯定时器等待且未超时：挂起并挂到定时器堆
	if ( (pCo->__iWakeTime > 0) && (pCo->__iWakeTime > iNow) ) {
		pCo->iState = XRT_CO_SUSPENDED;
		__xrt_co_sched_attach_timer(pSched, pCo);
		return;
	}

	// 超时已到或无条件等待：清除定时器并标记就绪
	if ( pCo->__iWakeTime > 0 ) {
		pCo->__iWakeTime = 0;
	}

	__xrt_co_sched_mark_ready(pSched, pCo);
}


// 内部函数：__xrt_co_sched_next_wake_time
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


// 内部函数：__xrt_co_sched_has_pending_posts
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


// 内部函数：分配 post 链表节点（优先复用空闲链表）
static __xrt_co_post_item* __xrt_co_sched_post_item_alloc(xcosched* pSched)
{
	__xrt_co_post_item* pItem = NULL;

	if ( pSched == NULL ) {
		return NULL;
	}

	// 从空闲链表中获取节点
	xrtMutexLock(pSched->pPostMutex);
	pItem = pSched->pPostFree;
	if ( pItem != NULL ) {
		pSched->pPostFree = pItem->pNext;
	}
	xrtMutexUnlock(pSched->pPostMutex);

	// 空闲链表为空则分配新节点
	if ( pItem == NULL ) {
		pItem = (__xrt_co_post_item*)xrtMalloc(sizeof(__xrt_co_post_item));
	}

	if ( pItem != NULL ) {
		pItem->pCo = NULL;
		pItem->pNext = NULL;
	}

	return pItem;
}


// 内部函数：__xrt_co_sched_post_item_free
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


// 内部函数：处理跨线程 post 队列中积累的唤醒请求
static void __xrt_co_sched_drain_posts(xcosched* pSched)
{
	__xrt_co_post_item* pHead = NULL;

	if ( pSched == NULL || pSched->pPostMutex == NULL ) {
		return;
	}

	// 原子地取走整个 post 链表
	xrtMutexLock(pSched->pPostMutex);
	pHead = pSched->pPostHead;
	pSched->pPostHead = NULL;
	pSched->pPostTail = NULL;
	xrtMutexUnlock(pSched->pPostMutex);

	// 逐个处理 post 请求
	while ( pHead ) {
		__xrt_co_post_item* pNext = pHead->pNext;
		xcoro pCo = pHead->pCo;

		// 将有效的协程标记为就绪
		if ( pCo && pCo->__pSched == pSched && pCo->iState != XRT_CO_DEAD ) {
			if ( pCo->__pJoinTarget == NULL || __xrt_co_is_terminal(pCo->__pJoinTarget) ) {
				pCo->__iWakeTime = 0;
				__xrt_co_sched_detach_timer(pSched, pCo);
				__xrt_co_sched_mark_ready(pSched, pCo);
			}
		}

		// 回收 post 节点
		__xrt_co_sched_post_item_free(pSched, pHead);
		pHead = pNext;
	}
}


// 内部函数：__xrt_co_sched_wait_for_post
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


// 内部函数：计算下一次 poll 应等待的超时时间
static uint32 __xrt_co_sched_compute_wait_timeout(xcosched* pSched, uint32 iTimeout)
{
	int64 iNow = 0;
	int64 iNextWake = 0;
	bool bInfinite = (iTimeout == __XRT_CO_SCHED_WAIT_FOREVER);

	if ( pSched == NULL ) {
		return 0;
	}

	// 有就绪协程或待处理 post 时无需等待
	if ( pSched->pReadyHead != NULL || __xrt_co_sched_has_pending_posts(pSched) ) {
		return 0;
	}

	// 计算最近的定时器唤醒时间
	iNow = __xrt_co_time_ms();
	iNextWake = __xrt_co_sched_next_wake_time(pSched, iNow);

	// 定时器堆中有未来需要唤醒的协程
	if ( iNextWake > iNow ) {
		int64 iDelta = iNextWake - iNow;
		// 取定时器超时与用户指定超时的较小值
		if ( !bInfinite && iDelta > iTimeout ) {
			iDelta = iTimeout;
		}
		if ( iDelta < 0 ) {
			iDelta = 0;
		}
		if ( iDelta > 0xFFFFFFFELL ) {
			iDelta = 0xFFFFFFFELL;
		}
		return (uint32)iDelta;
	}

	// 定时器堆中有已过期的协程（应该立即处理）
	if ( iNextWake > 0 ) {
		return 0;
	}

	// 无定时器协程，使用用户指定的超时
	return bInfinite ? __XRT_CO_SCHED_WAIT_FOREVER : iTimeout;
}


// 内部函数：__xrt_co_sched_run_until
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


// 加入协程（等待目标协程终止）
XXAPI bool xrtCoJoin(xcoro pCo)
{
	xcoro pCurrent = NULL;
	xrtCoroRuntimeState* pRuntime = NULL;
	
	if ( pCo == NULL ) {
		xrtSetError("invalid coroutine handle.", FALSE);
		return FALSE;
	}
	
	// 检查协程所有权
	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return FALSE;
	}
	
	// 目标已终止，直接返回
	if ( __xrt_co_is_terminal(pCo) ) {
		return TRUE;
	}
	
	pRuntime = __xrt_co_require_runtime(TRUE);
	if ( pRuntime == NULL ) {
		return FALSE;
	}
	
	pCurrent = __xrt_co_get_current();
	
	// --- 在协程内部 join ---
	if ( pCurrent != NULL ) {
		// 禁止自 join
		if ( pCurrent == pCo ) {
			xrtSetError("coroutine cannot join itself.", FALSE);
			return FALSE;
		}
		
		// 要求在同一个调度器内
		if ( pCurrent->__pSched == NULL || pCo->__pSched == NULL || pCurrent->__pSched != pCo->__pSched ) {
			xrtSetError("coroutine join inside coroutine requires the same scheduler.", FALSE);
			return FALSE;
		}
		
		// 检查是否已在等待其他目标
		if ( pCurrent->__pJoinTarget != NULL && pCurrent->__pJoinTarget != pCo ) {
			xrtSetError("current coroutine is already waiting for another join target.", FALSE);
			return FALSE;
		}
		
		// 注册 join 等待并挂起当前协程
		if ( !__xrt_co_is_terminal(pCo) ) {
			__xrt_co_join_pin_acquire(pCo);
			pCurrent->__pJoinTarget = pCo;
			pCurrent->__iWaitKind = __XRT_CO_WAIT_JOIN;
			__xrt_co_wait_queue_push_tail(&pCo->__pJoinWaitHead, &pCo->__pJoinWaitTail, pCurrent);
			pCurrent->iState = XRT_CO_SUSPENDED;
			__xrt_co_swap_to_main(pRuntime);
		}
		
		// 恢复后清理 join 状态
		if ( pCurrent->__pJoinTarget == pCo && __xrt_co_is_terminal(pCo) ) {
			pCurrent->__pJoinTarget = NULL;
			if ( pCurrent->__iWaitKind == __XRT_CO_WAIT_JOIN ) {
				pCurrent->__iWaitKind = __XRT_CO_WAIT_NONE;
			}
			__xrt_co_join_pin_release(pCo);
		}
		
		return __xrt_co_is_terminal(pCo);
	}
	
	// --- 在调度器管理的协程外 join（走调度器轮询） ---
	if ( pCo->__pSched != NULL ) {
		bool bJoined = FALSE;
		__xrt_co_join_pin_acquire(pCo);
		bJoined = __xrt_co_sched_run_until((xcosched*)pCo->__pSched, pCo);
		__xrt_co_join_pin_release(pCo);
		return bJoined;
	}
	
	// --- 在非调度器协程外 join（手动 Resume 循环） ---
	while ( !__xrt_co_is_terminal(pCo) ) {
		if ( !xrtCoResume(pCo) ) {
			return __xrt_co_is_terminal(pCo);
		}
	}

	return TRUE;
}


// xrtCoExit 相关处理
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


// 设置协程 user 数据
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


// 获取协程 user 数据
XXAPI ptr xrtCoGetUserData(xcoro pCo)
{
	if ( !pCo ) { return NULL; }
	if ( !__xrt_co_check_owner(pCo, "coroutine belongs to another thread.") ) {
		return NULL;
	}
	return pCo->pUserData;
}


// xrtCoPushCleanup 相关处理
XXAPI bool xrtCoPushCleanup(xco_cleanup_proc proc, ptr pArg)
{
	xcoro pCo = __xrt_co_get_current();
	xco_cleanup* pCleanup = NULL;
	
	if ( pCo == NULL ) {
		xrtSetError("xrtCoPushCleanup must be called from inside a coroutine.", FALSE);
		return FALSE;
	}
	
	// 参数校验
	if ( proc == NULL ) {
		xrtSetError("coroutine cleanup proc is null.", FALSE);
		return FALSE;
	}
	
	// 分配清理节点并压入清理栈
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


// xrtCoPopCleanup 相关处理
XXAPI bool xrtCoPopCleanup(xco_cleanup_proc proc, ptr pArg, bool bExecute)
{
	xcoro pCo = __xrt_co_get_current();
	xco_cleanup* pCleanup = NULL;
	
	if ( pCo == NULL ) {
		xrtSetError("xrtCoPopCleanup must be called from inside a coroutine.", FALSE);
		return FALSE;
	}
	
	// 检查清理栈是否为空
	if ( pCo->__pCleanupTop == NULL ) {
		xrtSetError("coroutine cleanup stack is empty.", FALSE);
		return FALSE;
	}
	
	// 校验栈顶是否匹配
	pCleanup = pCo->__pCleanupTop;
	if ( pCleanup->Proc != proc || pCleanup->Arg != pArg ) {
		xrtSetError("coroutine cleanup pop requires the current top cleanup.", FALSE);
		return FALSE;
	}
	
	// 弹出栈顶节点
	pCo->__pCleanupTop = pCleanup->pPrev;
	
	// 可选执行清理回调
	if ( bExecute && pCleanup->Proc ) {
		pCo->__iFlags |= __XRT_CO_FLAG_IN_CLEANUP;
		pCleanup->Proc(pCleanup->Arg);
		pCo->__iFlags &= ~__XRT_CO_FLAG_IN_CLEANUP;
	}
	xrtFree(pCleanup);
	return TRUE;
}



// 内部函数：检查协程 sched 所有权
static bool __xrt_co_check_sched_owner(xcosched* pSched, const char* sError)
{
	if ( pSched == NULL ) {
		xrtSetError("invalid coroutine scheduler.", FALSE);
		return FALSE;
	}

	return __xrt_co_check_owner_tid(pSched->iOwnerThreadId, sError);
}


// xrtCoSchedCreate 相关处理
XXAPI xcosched* xrtCoSchedCreate()
{
	xrtThreadData* pThreadData = __xrt_co_require_thread_data(TRUE);
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime_from_thread(pThreadData);
	xcosched* pSched = NULL;

	if ( pThreadData == NULL || pRuntime == NULL ) {
		return NULL;
	}
	
	// 分配调度器结构体
	pSched = (xcosched*)xrtMalloc(sizeof(xcosched));
	if ( !pSched ) { return NULL; }
	
	memset(pSched, 0, sizeof(xcosched));
	
	// 分配协程数组
	pSched->arrCoros = (xcoro*)xrtMalloc(sizeof(xcoro) * __XRT_CO_SCHED_INIT_CAP);
	if ( !pSched->arrCoros ) {
		xrtFree(pSched);
		return NULL;
	}
	
	memset(pSched->arrCoros, 0, sizeof(xcoro) * __XRT_CO_SCHED_INIT_CAP);
	
	// 分配定时器堆数组
	pSched->arrTimers = (xcoro*)xrtMalloc(sizeof(xcoro) * __XRT_CO_TIMER_INIT_CAP);
	if ( !pSched->arrTimers ) {
		xrtFree(pSched->arrCoros);
		xrtFree(pSched);
		return NULL;
	}
	
	memset(pSched->arrTimers, 0, sizeof(xcoro) * __XRT_CO_TIMER_INIT_CAP);
	
	// 创建 post 互斥锁
	pSched->pPostMutex = xrtMutexCreate();
	if ( pSched->pPostMutex == NULL ) {
		xrtFree(pSched->arrTimers);
		xrtFree(pSched->arrCoros);
		xrtFree(pSched);
		return NULL;
	}
	
	// 创建 post 条件变量
	pSched->pPostCond = xrtCondCreate();
	if ( pSched->pPostCond == NULL ) {
		xrtMutexDestroy(pSched->pPostMutex);
		xrtFree(pSched->arrTimers);
		xrtFree(pSched->arrCoros);
		xrtFree(pSched);
		return NULL;
	}
	
	// 初始化调度器字段
	pSched->pThreadData = pThreadData;
	pSched->iOwnerThreadId = pThreadData->iThreadId;
	pSched->iCount = 0;
	pSched->iCapacity = __XRT_CO_SCHED_INIT_CAP;
	pSched->iAlive = 0;
	pSched->pReadyHead = NULL;
	pSched->pReadyTail = NULL;
	pSched->iTimerCount = 0;
	pSched->iTimerCapacity = __XRT_CO_TIMER_INIT_CAP;
	
	// 设为线程默认调度器
	if ( pRuntime->pDefaultSched == NULL ) {
		pRuntime->pDefaultSched = pSched;
	}

	return pSched;
}


// xrtCoSchedCurrent 相关处理
XXAPI xcosched* xrtCoSchedCurrent()
{
	xcoro pCurrent = __xrt_co_get_current();
	xrtCoroRuntimeState* pRuntime = __xrt_co_get_runtime();

	if ( pCurrent && pCurrent->__pSched ) {
		return (xcosched*)pCurrent->__pSched;
	}

	return pRuntime ? (xcosched*)pRuntime->pDefaultSched : NULL;
}


// xrtCoSchedDestroy 相关处理
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


// xrtCoSchedSpawn 相关处理
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
		if ( !pNewArr ) { return NULL; }
		pSched->arrCoros = pNewArr;
		pSched->iCapacity = iNewCap;
	}

	pCo = xrtCoCreate(pfnEntry, pParam, iStackSize);
	if ( !pCo ) { return NULL; }

	pCo->__pSched = pSched;
	pSched->arrCoros[pSched->iCount] = pCo;
	pSched->iCount++;
	pSched->iAlive++;
	__xrt_co_sched_mark_ready(pSched, pCo);

	return pCo;
}


// xrtCoSchedPost 相关处理
XXAPI bool xrtCoSchedPost(xcosched* pSched, xcoro pCo)
{
	__xrt_co_post_item* pItem = NULL;

	if ( pSched == NULL ) {
		xrtSetError("invalid coroutine scheduler.", FALSE);
		return FALSE;
	}
	
	// 检查协程是否属于目标调度器
	if ( pCo && pCo->__pSched != pSched ) {
		xrtSetError("coroutine does not belong to target scheduler.", FALSE);
		return FALSE;
	}
	
	// 已终止的协程无需 post
	if ( pCo && pCo->iState == XRT_CO_DEAD ) {
		return TRUE;
	}
	
	// 分配 post 节点
	pItem = __xrt_co_sched_post_item_alloc(pSched);
	if ( pItem == NULL ) {
		return FALSE;
	}
	
	pItem->pCo = pCo;
	pItem->pNext = NULL;
	
	// 加入 post 链表尾部并通知等待线程
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


// xrtCoSchedPollOnce 相关处理
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
	
	// 检查调度器所有权
	if ( !__xrt_co_check_sched_owner(pSched, "scheduler belongs to another thread.") ) {
		return FALSE;
	}
	
	// 不允许在协程内调用 poll
	if ( __xrt_co_get_current() != NULL ) {
		xrtSetError("cannot poll scheduler from inside a running coroutine.", FALSE);
		return FALSE;
	}
	
	if ( pSched->iAlive <= 0 ) { return FALSE; }
	
	// 收集当前时刻需要处理的事件
	iNow = __xrt_co_time_ms();
	__xrt_co_sched_drain_posts(pSched);
	__xrt_co_sched_collect_expired_timers(pSched, iNow);
	
	// 无就绪协程时等待新事件
	if ( pSched->pReadyHead == NULL ) {
		iWaitTimeout = __xrt_co_sched_compute_wait_timeout(pSched, iTimeout);
		if ( iWaitTimeout > 0 ) {
			(void)__xrt_co_sched_wait_for_post(pSched, iWaitTimeout);
			// 等待结束后重新收集
			iNow = __xrt_co_time_ms();
			__xrt_co_sched_drain_posts(pSched);
			__xrt_co_sched_collect_expired_timers(pSched, iNow);
		}
	}
	
	// 依次处理就绪队列中的协程（每次 poll 只执行一个）
	while ( pSched->pReadyHead != NULL ) {
		pCo = pSched->pReadyHead;
		__xrt_co_sched_ready_unlink(pSched, pCo);
		
		if ( pCo == NULL ) {
			break;
		}
		
		// 跳过已终止的协程并回收
		if ( pCo->iState == XRT_CO_DEAD ) {
			iIndex = __xrt_co_sched_find_index(pSched, pCo);
			if ( iIndex >= 0 ) {
				__xrt_co_sched_reap_dead(pSched, iIndex);
			}
			continue;
		}
		
		// 对未启动但有取消请求的协程直接完成取消
		if ( __xrt_co_is_cancel_requested_flag(pCo) && (pCo->__iFlags & __XRT_CO_FLAG_STARTED) == 0 ) {
			__xrt_co_finish(pCo, XRT_CO_TERM_CANCELLED, -1);
			iIndex = __xrt_co_sched_find_index(pSched, pCo);
			if ( iIndex >= 0 ) {
				__xrt_co_sched_reap_dead(pSched, iIndex);
			}
			continue;
		}
		
		// 恢复协程执行并进行后处理
		xrtCoResume(pCo);
		__xrt_co_sched_on_return(pSched, pCo, __xrt_co_time_ms());
		break;
	}

	return pSched->iAlive > 0;
}


// xrtCoSchedStep 相关处理
XXAPI bool xrtCoSchedStep(xcosched* pSched)
{
	return xrtCoSchedPollOnce(pSched, 0);
}


// xrtCoSchedRun 相关处理
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


// xrtCoSchedGetAlive 相关处理
XXAPI int xrtCoSchedGetAlive(xcosched* pSched)
{
	if ( !pSched ) { return 0; }
	return pSched->iAlive;
}


// 休眠协程直到指定时刻
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


// xrtCoWaitDeadline 相关处理
XXAPI bool xrtCoWaitDeadline(int64 iDeadlineMs)
{
	xcoro pCo = __xrt_co_get_current();

	if ( !pCo ) {
		xrtSetError("xrtCoWaitDeadline must be called from inside a coroutine.", FALSE);
		return FALSE;
	}
	
	// 必须在调度器管理的协程中调用
	if ( pCo->__pSched == NULL ) {
		xrtSetError("xrtCoWaitDeadline requires a scheduler-managed coroutine.", FALSE);
		return FALSE;
	}
	
	// 已有取消请求，立即返回
	if ( __xrt_co_is_cancel_requested_flag(pCo) ) {
		return FALSE;
	}
	
	// 截止时间未到则挂起等待
	if ( iDeadlineMs > __xrt_co_time_ms() ) {
		pCo->__iWakeTime = iDeadlineMs;
		pCo->__iWaitKind = __XRT_CO_WAIT_TIMER;
		xrtCoYield();
	}

	return !__xrt_co_is_cancel_requested_flag(pCo);
}


// 创建协程事件
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


// 销毁协程事件
XXAPI void xrtCoEventDestroy(xcoevent pEvent)
{
	xmutex pLock = NULL;
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
		pLock = pEvent->pLock;
		pEvent->pLock = NULL;
		xrtMutexUnlock(pLock);
		xrtMutexDestroy(pLock);
	}

	xrtFree(pEvent);
}


// 设置协程事件（唤醒等待的协程）
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
	
	// 循环唤醒等待的协程
	for ( ;; ) {
		xcoro pWaiter = NULL;
		xcosched* pSched = NULL;
		bool bWakeAll = FALSE;
		
		xrtMutexLock(pEvent->pLock);
		bWakeAll = pEvent->bManualReset;
		
		// 手动重置模式：设置信号状态
		if ( pEvent->bManualReset ) {
			pEvent->bSignaled = TRUE;
		}
		
		// 取出一个等待者
		pWaiter = __xrt_co_pop_event_waiter_locked(pEvent);
		
		// 自动重置模式：无等待者时才设置信号
		if ( pWaiter == NULL && !pEvent->bManualReset ) {
			pEvent->bSignaled = TRUE;
		}
		if ( pWaiter != NULL ) {
			pWaiter->__iWaitResult = __XRT_CO_WAIT_RESULT_SIGNAL;
			pSched = (xcosched*)pWaiter->__pSched;
		}
		xrtMutexUnlock(pEvent->pLock);
		
		// 通过 post 将等待者标记为就绪
		if ( pWaiter != NULL && pSched != NULL ) {
			(void)xrtCoSchedPost(pSched, pWaiter);
		}
		
		// 非手动重置模式或无更多等待者时退出
		if ( !bWakeAll || pWaiter == NULL ) {
			break;
		}
	}
}


// 重置协程事件
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


// 内部函数：等待协程事件 core
static bool __xrt_co_wait_event_core(xcoevent pEvent, bool bInfinite, int64 iDeadlineMs)
{
	xcoro pCo = __xrt_co_get_current();
	int64 iNowMs = 0;

	if ( pEvent == NULL ) {
		xrtSetError("invalid coroutine event.", FALSE);
		return FALSE;
	}
	
	// 必须在协程内部调用
	if ( pCo == NULL ) {
		xrtSetError("xrtCoWaitEvent must be called from inside a coroutine.", FALSE);
		return FALSE;
	}
	
	// 必须在调度器管理的协程中调用
	if ( pCo->__pSched == NULL ) {
		xrtSetError("xrtCoWaitEvent requires a scheduler-managed coroutine.", FALSE);
		return FALSE;
	}
	
	// 已有取消请求，直接返回失败
	if ( __xrt_co_is_cancel_requested_flag(pCo) ) {
		return FALSE;
	}
	
	if ( pEvent->pLock == NULL ) {
		xrtSetError("coroutine event lock is missing.", FALSE);
		return FALSE;
	}
	
	pCo->__iWaitResult = __XRT_CO_WAIT_RESULT_NONE;
	
	xrtMutexLock(pEvent->pLock);
	
	// 尝试消费已有信号（快速路径）
	if ( __xrt_co_event_try_consume_locked(pEvent) ) {
		pCo->__iWaitResult = __XRT_CO_WAIT_RESULT_SIGNAL;
		xrtMutexUnlock(pEvent->pLock);
		return TRUE;
	}
	
	// 超时时间已过，直接返回
	iNowMs = __xrt_co_time_ms();
	if ( !bInfinite && iDeadlineMs <= iNowMs ) {
		xrtMutexUnlock(pEvent->pLock);
		return FALSE;
	}
	
	// 加入事件等待队列并挂起
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
	
	// 让出执行权，等待被事件信号或超时唤醒
	xrtCoYield();
	
	// 恢复后检查是否有取消请求
	if ( __xrt_co_is_cancel_requested_flag(pCo) ) {
		return FALSE;
	}

	return pCo->__iWaitResult == __XRT_CO_WAIT_RESULT_SIGNAL;
}


// 等待协程事件
XXAPI bool xrtCoWaitEvent(xcoevent pEvent)
{
	return __xrt_co_wait_event_core(pEvent, TRUE, 0);
}


// 等待协程事件直到指定时刻
XXAPI bool xrtCoWaitEventUntil(xcoevent pEvent, int64 iDeadlineMs)
{
	return __xrt_co_wait_event_core(pEvent, FALSE, iDeadlineMs);
}


// 等待协程事件超时
XXAPI bool xrtCoWaitEventTimeout(xcoevent pEvent, uint32 iTimeoutMs)
{
	if ( iTimeoutMs == XRT_CO_WAIT_INFINITE ) {
		return __xrt_co_wait_event_core(pEvent, TRUE, 0);
	}

	return __xrt_co_wait_event_core(pEvent, FALSE, __xrt_co_time_ms() + (int64)iTimeoutMs);
}


// 休眠协程
XXAPI void xrtCoSleep(uint32 iMs)
{
	xrtCoSleepUntil(__xrt_co_time_ms() + (int64)iMs);
}
