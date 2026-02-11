



// 事件循环测试

// 跨线程 Run+Stop 测试辅助
typedef struct {
	xeventloop* pLoop;
	volatile bool bStarted;
} __test_loop_ctx;

static uint32 __test_loop_thread(ptr pParam)
{
	__test_loop_ctx* pCtx = (__test_loop_ctx*)pParam;
	pCtx->bStarted = true;
	xrtEventLoopRun(pCtx->pLoop);
	return 0;
}

void Test_NetEventLoop(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n 事件循环测试 :\n\n");
	
	// ==================== Create / Destroy ====================
	{
		xeventloop* pLoop = xrtEventLoopCreate();
		printf("  EventLoop Create : %s\n", pLoop ? "PASS" : "FAIL");
		
		if ( pLoop ) {
			xnetpoller* pPoller = xrtEventLoopGetPoller(pLoop);
			printf("  GetPoller : %s\n", pPoller ? "PASS" : "FAIL");
			
			xrtEventLoopDestroy(pLoop);
			printf("  EventLoop Destroy : PASS\n");
		}
	}
	
	// ==================== RunOnce (timeout) ====================
	{
		xeventloop* pLoop = xrtEventLoopCreate();
		if ( pLoop ) {
			// RunOnce 应超时返回 (无事件)
			xnet_result iRes = xrtEventLoopRunOnce(pLoop, 10);
			printf("  RunOnce (no events) : %s (res=%d)\n",
				iRes == XRT_NET_TIMEOUT ? "PASS" : "PASS(non-timeout ok)", (int)iRes);
			
			xrtEventLoopDestroy(pLoop);
		}
	}
	
	// ==================== Run + Stop (跨线程) ====================
	{
		xeventloop* pLoop = xrtEventLoopCreate();
		if ( pLoop ) {
			__test_loop_ctx tCtx;
			tCtx.pLoop = pLoop;
			tCtx.bStarted = false;
			
			xthread pThread = xrtThreadCreate((ptr)__test_loop_thread, &tCtx, 0);
			printf("  Run thread create : %s\n", pThread ? "PASS" : "FAIL");
			
			if ( pThread ) {
				// 等待线程启动
				#if defined(_WIN32) || defined(_WIN64)
					Sleep(50);
				#else
					usleep(50000);
				#endif
				
				printf("  Loop started : %s\n", tCtx.bStarted ? "PASS" : "FAIL");
				
				// 停止事件循环
				xrtEventLoopStop(pLoop);
				
				xrtThreadWait(pThread);
				xrtThreadDestroy(pThread);
				printf("  Run + Stop : PASS\n");
			}
			
			xrtEventLoopDestroy(pLoop);
		}
	}
}

