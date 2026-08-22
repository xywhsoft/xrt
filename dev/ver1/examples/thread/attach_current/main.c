/*
 * XRT Example - Thread Attach Current
 * XRT 范例 - 附加当前线程
 *
 * Description / 说明:
 *   EN: Demonstrates attaching a host-created native thread to the XRT runtime,
 *       using thread-bound APIs, and detaching it again.
 *   CN: 演示将宿主自行创建的原生线程附加到 XRT 运行时、使用线程绑定 API，
 *       然后再分离。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/thread_attach_current.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/thread_attach_current -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>
#include <string.h>

typedef struct {
	bool bAttachedBefore;
	bool bAttachedDuring;
	bool bAttachedAfter;
	uint64 iRuntimeThreadId;
	char aTempText[64];
	char aErrorText[128];
} ex_attach_ctx;


#if defined(_WIN32) || defined(_WIN64)
static DWORD WINAPI procNativeThread(LPVOID pArg)
#else
static void* procNativeThread(void* pArg)
#endif
{
	ex_attach_ctx* pCtx = (ex_attach_ctx*)pArg;
	xrtThreadData* pThreadData = NULL;
	char* sTemp = NULL;

	if ( pCtx == NULL ) {
		#if defined(_WIN32) || defined(_WIN64)
			return 1u;
		#else
			return NULL;
		#endif
	}

	pCtx->bAttachedBefore = xrtThreadIsAttached();
	pThreadData = xrtThreadAttachCurrent();
	pCtx->bAttachedDuring = pThreadData != NULL && xrtThreadIsAttached();

	if ( pThreadData != NULL ) {
		pCtx->iRuntimeThreadId = xrtThreadGetCurrentId();
		sTemp = (char*)xrtTempMemory(64u);
		if ( sTemp != NULL ) {
			snprintf(sTemp, 64u, "native-thread-%llu", (unsigned long long)pCtx->iRuntimeThreadId);
			snprintf(pCtx->aTempText, sizeof(pCtx->aTempText), "%s", sTemp);
		}

		xrtSetError("native thread attached to XRT", FALSE);
		snprintf(pCtx->aErrorText, sizeof(pCtx->aErrorText), "%s", xrtGetError());
		xrtThreadDetachCurrent();
	}

	pCtx->bAttachedAfter = xrtThreadIsAttached();

	#if defined(_WIN32) || defined(_WIN64)
		return 0u;
	#else
		return NULL;
	#endif
}


int main(void)
{
	ex_attach_ctx tCtx;
	int iRet = 0;

	memset(&tCtx, 0, sizeof(tCtx));
	xrtInit();

	#if defined(_WIN32) || defined(_WIN64)
	{
		HANDLE hThread = CreateThread(NULL, 0u, procNativeThread, &tCtx, 0u, NULL);

		if ( hThread == NULL ) {
			printf("native_thread = failed\n");
			iRet = 1;
			goto cleanup;
		}

		WaitForSingleObject(hThread, INFINITE);
		CloseHandle(hThread);
	}
	#else
	{
		pthread_t hThread;

		if ( pthread_create(&hThread, NULL, procNativeThread, &tCtx) != 0 ) {
			printf("native_thread = failed\n");
			iRet = 1;
			goto cleanup;
		}

		(void)pthread_join(hThread, NULL);
	}
	#endif

	printf("attached_before = %s\n", tCtx.bAttachedBefore ? "TRUE" : "FALSE");
	printf("attached_during = %s\n", tCtx.bAttachedDuring ? "TRUE" : "FALSE");
	printf("attached_after = %s\n", tCtx.bAttachedAfter ? "TRUE" : "FALSE");
	printf("runtime_thread_id = %llu\n", (unsigned long long)tCtx.iRuntimeThreadId);
	printf("temp_text = %s\n", tCtx.aTempText);
	printf("error_text = %s\n", tCtx.aErrorText);

	if ( tCtx.bAttachedBefore || !tCtx.bAttachedDuring || tCtx.bAttachedAfter ) {
		iRet = 1;
	}

cleanup:
	xrtUnit();
	return iRet;
}
