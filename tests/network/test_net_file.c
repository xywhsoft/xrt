#include "../test.h"



typedef struct testnetfile {
	xnetengine* Engine;
	xfile File;
	xnetcompletion Completion;
	xatomic32 Done;
	xatomic32 CancelTerminals;
	uint64 WriteId;
	uint64 ReadId;
	uint64 CancelId;
	char Read[16];
} testnetfile;



static void testNetFileCancelStart(xnetworker* pWorker, ptr pData);



#if defined(__ANDROID__) || (!defined(_WIN32) && !defined(__linux__))
/* 非 io_uring/IOCP 平台不提供普通文件完成，必须明确拒绝而不是伪异步。 */
static void testNetFileUnsupported(xnetworker* pWorker, ptr pData)
{
	testnetfile* pTest = (testnetfile*)pData;
	uint64 Id = xrtNetFileRead(
		pWorker,
		pTest->File,
		0,
		pTest->Read,
		8,
		&pTest->Completion
	);

	testRequire(
		(Id == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
		"native file capability mismatch"
	);
	xrtClearError();
	xrtAtomic32Store(&pTest->Done, 1, XMEMORY_RELEASE);
}
#endif



/* 第二个 Worker 必须在进入系统调用前拒绝已经绑定的文件。 */
static void testNetFileCrossWorker(xnetworker* pWorker, ptr pData)
{
	testnetfile* pTest = (testnetfile*)pData;
	uint64 Id = xrtNetFileRead(
		pWorker,
		pTest->File,
		0,
		pTest->Read,
		8,
		&pTest->Completion
	);

	testRequire(
		(Id == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"native file accepted a second worker"
	);
	xrtClearError();
	testRequire(
		xrtNetEnginePost(pTest->Engine, 0, testNetFileCancelStart, pTest),
		"native file cancel start post failed"
	);
}



/* 写完成后在同一 Worker 链式提交读取，验证完成对象可以立即复用。 */
static void testNetFileCompletion(
	xnetworker* pWorker,
	const xnetportevent* pEvent,
	ptr pData
)
{
	testnetfile* pTest = (testnetfile*)pData;

	if ( pEvent->Id == pTest->CancelId ) {
		testRequire(
			(pEvent->Type == XNET_PORT_EVENT_FILE_READ) &&
			((pEvent->Result == XNET_RESULT_OK) ||
			 (pEvent->Result == XNET_RESULT_CANCELLED)),
			"native file cancel terminal mismatch"
		);
		(void)xrtAtomic32FetchAdd(
			&pTest->CancelTerminals,
			1,
			XMEMORY_ACQ_REL
		);
		xrtAtomic32Store(&pTest->Done, 1, XMEMORY_RELEASE);
		return;
	}
	testRequire(
		(pEvent->Result == XNET_RESULT_OK) &&
		(pEvent->SystemCode == 0),
		"native file operation failed"
	);
	if ( pEvent->Type == XNET_PORT_EVENT_FILE_WRITE ) {
		testRequire(
			(pEvent->Id == pTest->WriteId) &&
			(pEvent->Bytes == 6),
			"native file write completion mismatch"
		);
		pTest->ReadId = xrtNetFileRead(
			pWorker,
			pTest->File,
			0,
			pTest->Read,
			8,
			&pTest->Completion
		);
		testRequire(
			pTest->ReadId != 0,
			"native file chained read submit failed"
		);
		return;
	}
	testRequire(
		(pEvent->Type == XNET_PORT_EVENT_FILE_READ) &&
		(pEvent->Id == pTest->ReadId) &&
		(pEvent->Bytes == 8) &&
		(pTest->Read[0] == 0) &&
		(pTest->Read[1] == 0) &&
		(memcmp(pTest->Read + 2, "native", 6) == 0),
		"native file read completion mismatch"
	);
	testRequire(
		xrtNetEnginePost(pTest->Engine, 1, testNetFileCrossWorker, pTest),
		"native file cross-worker post failed"
	);
}



/* 在指定 Worker 上启动文件完成链。 */
static void testNetFileStart(xnetworker* pWorker, ptr pData)
{
	testnetfile* pTest = (testnetfile*)pData;
	uint64 InvalidId;

	InvalidId = xrtNetFileRead(
		pWorker,
		pTest->File,
		(uint64)INT64_MAX,
		pTest->Read,
		2,
		&pTest->Completion
	);
	testRequire(
		(InvalidId == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"native file accepted an overflowing range"
	);
	xrtClearError();

	pTest->WriteId = xrtNetFileWrite(
		pWorker,
		pTest->File,
		2,
		"native",
		6,
		&pTest->Completion
	);
	testRequire(
		pTest->WriteId != 0,
		"native file write submit failed"
	);
}



/* 取消与已经完成的系统调用竞态时仍只允许一个终态。 */
static void testNetFileCancelStart(xnetworker* pWorker, ptr pData)
{
	testnetfile* pTest = (testnetfile*)pData;

	pTest->CancelId = xrtNetFileRead(
		pWorker,
		pTest->File,
		0,
		pTest->Read,
		8,
		&pTest->Completion
	);
	testRequire(
		pTest->CancelId != 0,
		"native file cancel read submit failed"
	);
	testRequire(
		xrtNetFileCancel(pWorker, pTest->CancelId),
		"native file cancel request failed"
	);
}



/* 原生文件完成路径不占任务线程、不复制载荷，并保持绝对偏移语义。 */
int main(void)
{
	static const char sPath[] = "test_net_file.tmp";
	xnetengineconfig Config;
	xfileoptions Options;
	testnetfile Test;
	xnetengine* pEngine;
	xdeadline Deadline;
	char Probe = 0;
	size_t iRead = 0;

	memset(&Test, 0, sizeof(Test));
	xrtAtomic32Init(&Test.Done, 0);
	xrtAtomic32Init(&Test.CancelTerminals, 0);
	xrtNetCompletionInit(
		&Test.Completion,
		testNetFileCompletion,
		&Test
	);
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_READ | XFILE_WRITE |
		XFILE_CREATE | XFILE_TRUNCATE;
	Test.File = xrtNetFileOpen(sPath, &Options);
	testRequire(Test.File != NULL, "native file open failed");
	testRequire(
		(xrtFileFlags(Test.File) & XFILE_ASYNC) != 0u,
		"native file open did not preserve async mode"
	);
	testRequire(
		!xrtReadAt(Test.File, 0, &Probe, 1, &iRead) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"synchronous read accepted an async file handle"
	);
	xrtClearError();

	xrtNetEngineConfigInit(&Config);
	Config.Workers = 2;
	#if defined(__ANDROID__)
		Config.Backend = XNET_PORT_EPOLL;
	#elif defined(__linux__)
		Config.Backend = XNET_PORT_URING;
	#elif defined(_WIN32)
		Config.Backend = XNET_PORT_IOCP;
	#endif
	pEngine = xrtNetEngineCreate(&Config);
	testRequire(
		(pEngine != NULL) && xrtNetEngineStart(pEngine),
		"native file engine start failed"
	);
	Test.Engine = pEngine;
	#if defined(__ANDROID__) || (!defined(_WIN32) && !defined(__linux__))
		testRequire(
			xrtNetEnginePost(pEngine, 0, testNetFileUnsupported, &Test),
			"native file capability post failed"
		);
		Deadline = xrtDeadlineAfter(UINT64_C(5000000));
		while ( xrtAtomic32Load(&Test.Done, XMEMORY_ACQUIRE) == 0 ) {
			testRequire(
				!xrtDeadlineExpired(Deadline),
				"native file capability check timed out"
			);
			xrtThreadYield();
		}
		testRequire(xrtClose(Test.File), "native file close failed");
		testRequire(
			xrtNetEngineDestroy(pEngine),
			"native file engine destroy failed"
		);
		testRequire(xrtFileDelete(sPath), "native file cleanup failed");
		printf("[PASS] native file capability boundary\n");
		return 0;
	#endif
	testRequire(
		xrtNetEnginePost(pEngine, 0, testNetFileStart, &Test),
		"native file start post failed"
	);
	Deadline = xrtDeadlineAfter(UINT64_C(5000000));
	while ( xrtAtomic32Load(&Test.Done, XMEMORY_ACQUIRE) == 0 ) {
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"native file completion timed out"
		);
		xrtThreadYield();
	}
	xrtSleepUs(50000);
	testRequire(
		xrtAtomic32Load(&Test.CancelTerminals, XMEMORY_ACQUIRE) == 1,
		"native file cancel produced multiple terminals"
	);
	testRequire(xrtClose(Test.File), "native file close failed");
	testRequire(xrtNetEngineDestroy(pEngine), "native file engine destroy failed");
	testRequire(xrtFileDelete(sPath), "native file cleanup failed");
	printf("[PASS] native file completion\n");
	return 0;
}
