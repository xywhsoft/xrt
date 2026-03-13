// 可通过定义对 xrt 库进行裁剪
#define XRT_NO_FILE
#define XRT_NO_REGEX
#define XRT_NO_COROUTINE
#define XRT_NO_NETWORK
#define XRT_NO_CRYPTO
#define XRT_NO_NETSOCK
#define XRT_NO_NETPOLL
#define XRT_NO_NETTLS
#define XRT_NO_NETLOOP
#define XRT_NO_NETTCP
#define XRT_NO_NETUDP
#define XRT_NO_NETPROXY
#define XRT_NO_XID
#define XRT_NO_BUFFER
#define XRT_NO_NETHTTP
#define XRT_NO_NETWS
#define XRT_NO_NETHTTPD
#define XRT_NO_STACK
#define XRT_NO_JSON
#define XRT_NO_TEMPLATE

// 定义 XRT_IMPLEMENTATION 导入功能实现
#define XRT_IMPLEMENTATION
#include "xrt.h"

typedef struct {
	xvalue Table;
	xvalue Tags;
	volatile int FailCount;
} singlehead_shared_ctx;

static void singlehead_print_result(const char* sName, bool bPassed)
{
	printf("  [%s] %s\n", bPassed ? "OK" : "FAIL", sName);
}

static bool singlehead_has_shared_value_error(void)
{
	str sError = xrtGetError();
	return sError && strstr(sError, "real shared") != NULL;
}

static uint32 singlehead_worker(ptr pParam)
{
	singlehead_shared_ctx* pCtx = (singlehead_shared_ctx*)pParam;
	char* pTemp = NULL;
	xvalue pStoredTags = NULL;

	if ( !xrtThreadIsAttached() ) {
		pCtx->FailCount++;
		return 21;
	}

	pTemp = (char*)xrtTempMemory(32);
	if ( pTemp == NULL ) {
		pCtx->FailCount++;
		return 22;
	}
	memcpy(pTemp, "worker-ok", 10);

	xrtClearError();
	xvoAddRef(pCtx->Table);
	xvoUnref(pCtx->Table);
	xvoAddRef(pCtx->Tags);
	xvoUnref(pCtx->Tags);
	if ( xrtGetError() != NULL && xrtGetError() != xCore.sNull && xrtGetError()[0] != '\0' ) {
		pCtx->FailCount++;
		return 23;
	}

	if ( !xvoTableExists(pCtx->Table, "tags", 4) ) {
		pCtx->FailCount++;
		return 24;
	}
	pStoredTags = xvoTableGetValue(pCtx->Table, "tags", 4);
	if ( pStoredTags != pCtx->Tags ) {
		pCtx->FailCount++;
		return 25;
	}
	if ( xvoCollItemCount(pCtx->Tags) != 1 ) {
		pCtx->FailCount++;
		return 26;
	}

	return 20;
}

int main(void)
{
	bool bOk = TRUE;
	char* pTemp = NULL;
	xvalue pTable = NULL;
	xvalue pTags = NULL;
	xvalue pLocalColl = NULL;
	xthread pThread = NULL;
	singlehead_shared_ctx tCtx;

	memset(&tCtx, 0, sizeof(tCtx));

	printf("========================================\n");
	printf("  XRT Single Header Runtime Smoke Test\n");
	printf("========================================\n\n");

	printf("Initializing XRT...\n");
	xrtInit();

	singlehead_print_result("bootstrap thread attached", xrtThreadIsAttached());
	if ( !xrtThreadIsAttached() ) {
		bOk = FALSE;
		goto cleanup;
	}

	pTemp = (char*)xrtTempMemory(64);
	if ( pTemp == NULL ) {
		singlehead_print_result("thread-local temp memory", FALSE);
		bOk = FALSE;
		goto cleanup;
	}
	memcpy(pTemp, "singlehead-main", 16);
	singlehead_print_result("thread-local temp memory", TRUE);

	xrtSetError((str)"singlehead-error", FALSE);
	singlehead_print_result("thread-local error state", strcmp(xrtGetError(), "singlehead-error") == 0);
	if ( strcmp(xrtGetError(), "singlehead-error") != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtClearError();

	pTable = xvoCreateTableEx(XRT_OBJMODE_SHARED);
	pTags = xvoCreateCollEx(XRT_OBJMODE_SHARED);
	singlehead_print_result("shared value roots", pTable != NULL && pTags != NULL);
	if ( pTable == NULL || pTags == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}

	xrtClearError();
	if ( !xvoCollSetText(pTags, "singlehead", 0, FALSE) ) {
		singlehead_print_result("shared coll write", FALSE);
		bOk = FALSE;
		goto cleanup;
	}
	singlehead_print_result("shared coll write", TRUE);

	xrtClearError();
	if ( !xvoTableSetValue(pTable, "tags", 4, pTags, FALSE) ) {
		singlehead_print_result("shared nested value store", FALSE);
		bOk = FALSE;
		goto cleanup;
	}
	singlehead_print_result("shared nested value store", TRUE);

	tCtx.Table = pTable;
	tCtx.Tags = pTags;
	pThread = xrtThreadCreate((ptr)singlehead_worker, &tCtx, 0);
	singlehead_print_result("auto-attached worker thread", pThread != NULL);
	if ( pThread == NULL ) {
		bOk = FALSE;
		goto cleanup;
	}
	xrtThreadWait(pThread);
	singlehead_print_result("worker runtime access", xrtThreadGetExitCode(pThread) == 20 && tCtx.FailCount == 0);
	if ( xrtThreadGetExitCode(pThread) != 20 || tCtx.FailCount != 0 ) {
		bOk = FALSE;
		goto cleanup;
	}

	pLocalColl = xvoCreateColl();
	if ( pLocalColl == NULL ) {
		singlehead_print_result("local coll construction", FALSE);
		bOk = FALSE;
		goto cleanup;
	}
	if ( !xvoCollSetInt(pLocalColl, 1) ) {
		singlehead_print_result("local coll construction", FALSE);
		bOk = FALSE;
		goto cleanup;
	}
	singlehead_print_result("local coll construction", TRUE);

	xrtClearError();
	if ( xvoTableSetValue(pTable, "bad", 3, pLocalColl, FALSE) ) {
		singlehead_print_result("reject local nested container on shared root", FALSE);
		bOk = FALSE;
		goto cleanup;
	}
	singlehead_print_result("reject local nested container on shared root", singlehead_has_shared_value_error());
	if ( !singlehead_has_shared_value_error() ) {
		bOk = FALSE;
		goto cleanup;
	}

cleanup:
	if ( pThread ) {
		xrtThreadDestroy(pThread);
	}
	if ( pLocalColl ) {
		xvoUnref(pLocalColl);
	}
	if ( pTable ) {
		xvoUnref(pTable);
	}
	if ( pTags ) {
		xvoUnref(pTags);
	}
	xrtClearError();
	xrtUnit();

	printf("\n========================================\n");
	printf("  %s\n", bOk ? "All tests passed!" : "Test failed!");
	printf("========================================\n\n");

	return bOk ? 0 : 1;
}
