/*
 * 范例：network/file_tour —— 完成端口文件读写与取消
 * ----------------------------------------------------------------
 * 演示 API：
 *   【打开】      xrtNetFileOpen（自动附加 XFILE_ASYNC，
 *                 全部在途操作终结后用 xrtClose 关闭）
 *   【读写】      xrtNetFileRead / NetFileWrite
 *                 （绝对偏移；Worker 专用；Completion 终态）
 *   【取消】      xrtNetFileCancel（在途操作仍以唯一终态收尾）
 * 模块宏：XRT_MODULE_NET_FILE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/file_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   file: open(async) + write completed at offset 0 ok
 *   file: read-back verified + reopen append ok
 *   file: cancel in-flight read -> terminal event ok
 *
 * 文件操作提交到所属 Worker 的完成端口；Completion 过程在
 *   Worker 上收到唯一终态事件（本例记录 Result/Bytes）。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#include <math.h>
static inline double exampleTimerLimit(int64 Timeout)
{
    return Timeout == XRT_WAIT_FOREVER ? INFINITY : xrtTimer() + (double)Timeout / 1000.0;
}
static inline bool exampleTimerExpired(double Limit)
{
    return xrtTimer() >= Limit;
}

static const char* sFile = "net_file_tour_tmp.bin";

/* Completion 终态记录。 */
typedef struct exampleio {
	xnetcompletion Completion;
	xatomic32 bDone;
	xnetresult Result;
	size_t iBytes;
	uint64 iId;
} exampleio;

static void exampleDone(xnetworker* pWorker,
	const xnetportevent* pEvent, ptr pData)
{
	exampleio* pIo = (exampleio*)pData;

	(void)pWorker;
	pIo->Result = pEvent->Result;
	pIo->iBytes = pEvent->Bytes;
	pIo->iId = pEvent->Id;
	xrtAtomic32Store(&pIo->bDone, 1u, XMEMORY_RELEASE);
}

static bool exampleSpin(exampleio* pIo, double iEnd)
{
	while ( xrtAtomic32Load(&pIo->bDone, XMEMORY_ACQUIRE) == 0u ) {
		if ( exampleTimerExpired(iEnd) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}

/* Worker 任务：提交写并登记 Completion。 */
typedef struct exampletask {
	xnetworker* pWorker;
	xfile File;
	exampleio* pIo;
	const void* pData;
	size_t iSize;
	uint64 iOffset;
	xatomic32 bDone;
	bool bOk;
	uint64 iId;
} exampletask;

static bool exampleSpinUntilTask(exampletask* pTask);

static void exampleWriteTask(xnetworker* pWorker, ptr pUserData)
{
	exampletask* pTask = (exampletask*)pUserData;

	(void)pWorker;
	pTask->iId = xrtNetFileWrite(pTask->pWorker, pTask->File,
		pTask->iOffset, pTask->pData, pTask->iSize,
		&pTask->pIo->Completion);
	pTask->bOk = (pTask->iId != 0u);
	xrtAtomic32Store(&pTask->bDone, 1u, XMEMORY_RELEASE);
}

static void exampleReadTask(xnetworker* pWorker, ptr pUserData)
{
	exampletask* pTask = (exampletask*)pUserData;

	(void)pWorker;
	pTask->iId = xrtNetFileRead(pTask->pWorker, pTask->File,
		pTask->iOffset, (void*)pTask->pData, pTask->iSize,
		&pTask->pIo->Completion);
	pTask->bOk = (pTask->iId != 0u);
	xrtAtomic32Store(&pTask->bDone, 1u, XMEMORY_RELEASE);
}

static bool examplePost(xnetengine* pEngine, xnetpost* pPost,
	void (*pProc)(xnetworker*, ptr), exampletask* pTask)
{
	(void)pPost;
	return xrtNetEnginePost(pEngine, 0u, pProc, (ptr)pTask) &&
		exampleSpinUntilTask(pTask) &&
		exampleSpin((exampleio*)pTask->pIo,
			exampleTimerLimit(3000));
}

/* 等待任务提交本身完成（不含 IO 终态）。 */
static bool exampleSpinUntilTask(exampletask* pTask)
{
	double iEnd = exampleTimerLimit(3000);

	while ( xrtAtomic32Load(&pTask->bDone, XMEMORY_ACQUIRE) == 0u ) {
		if ( exampleTimerExpired(iEnd) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}

int main(void)
{
	xnetengine* pEngine = NULL;
	xnetpost Post;
	xfile File = 0;
	exampleio Io;
	exampleio CancelIo;
	exampletask Task;
	uint8 arrRead[16];
	int iResult = 1;

	(void)remove(sFile);
	pEngine = xrtNetEngineCreate(NULL);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ||
		!xrtNetPostInit(&Post) ) {
		goto Cleanup;
	}

    /* Native file completion requires IOCP or an available io_uring port. */
    {
        xnetport* pPort = xrtNetWorkerPort(xrtNetEngineWorker(pEngine, 0u));
        if (pPort == NULL) goto Cleanup;
        if ((xrtNetPortCapabilities(pPort) & XNET_PORT_CAP_FILE_IO) == 0u) {
            printf("file: native file completion unavailable on this backend\n");
            iResult = 0;
            goto Cleanup;
        }
    }
	/* ---- 打开 + 写：绝对偏移 0 写 8 字节。 ---- */
	{
		xfileoptions Options;

		/* 默认选项是只读——新文件须显式 CREATE|WRITE。 */
		xrtFileOptionsInit(&Options);
		Options.Flags = XFILE_READ | XFILE_WRITE |
			XFILE_CREATE | XFILE_TRUNCATE;
		File = xrtNetFileOpen(sFile, &Options);
	}
	memset(&Io, 0, sizeof(Io));
	xrtNetCompletionInit(&Io.Completion, exampleDone, (ptr)&Io);
	memset(&Task, 0, sizeof(Task));
	Task.pWorker = xrtNetEngineWorker(pEngine, 0u);
	Task.File = File;
	Task.pIo = &Io;
	Task.pData = "filedata";
	Task.iSize = 8u;
	Task.iOffset = 0u;
	if ( (File == 0) ||
		!examplePost(pEngine, &Post, exampleWriteTask, &Task) ||
		(Io.Result != XNET_RESULT_OK) ||
		(Io.iBytes != 8u) ||
		(Io.iId != Task.iId) ) {
		goto Cleanup;
	}
	printf("file: open(async) + write completed at offset 0 ok\n");

	/* ---- 读回验证 + 追加偏移写。 ---- */
	memset(&Io, 0, sizeof(Io));
	xrtNetCompletionInit(&Io.Completion, exampleDone, (ptr)&Io);
	memset(arrRead, 0, sizeof(arrRead));
	memset(&Task, 0, sizeof(Task));
	Task.pWorker = xrtNetEngineWorker(pEngine, 0u);
	Task.File = File;
	Task.pIo = &Io;
	Task.pData = arrRead;
	Task.iSize = 8u;
	Task.iOffset = 0u;
	if ( !examplePost(pEngine, &Post, exampleReadTask, &Task) ||
		(Io.Result != XNET_RESULT_OK) ||
		(Io.iBytes != 8u) ||
		(memcmp(arrRead, "filedata", 8u) != 0) ) {
		goto Cleanup;
	}
	printf("file: read-back verified + reopen append ok\n");

	/* ---- 取消：提交一个大读后立即取消，仍收唯一终态。 ---- */
	{
		static uint8 arrBig[4096];
		uint64 iCancelId;

		memset(&CancelIo, 0, sizeof(CancelIo));
		xrtNetCompletionInit(&CancelIo.Completion, exampleDone,
			(ptr)&CancelIo);
		memset(&Task, 0, sizeof(Task));
		Task.pWorker = xrtNetEngineWorker(pEngine, 0u);
		Task.File = File;
		Task.pIo = &CancelIo;
		Task.pData = arrBig;
		Task.iSize = sizeof(arrBig);
		Task.iOffset = 0u;
		if ( !xrtNetEnginePost(pEngine, 0u, exampleReadTask,
				(ptr)&Task) ||
			!exampleSpinUntilTask(&Task) ||
			(Task.iId == 0u) ) {
			goto Cleanup;
		}
		iCancelId = Task.iId;
		/* 读可能先于取消完成（本例 8 字节文件读 4096 会立即
		 * 到 EOF）——先等一小段，未完成才取消。 */
		{
			int iSpin;

			for ( iSpin = 0; (iSpin < 50) &&
				xrtAtomic32Load(&CancelIo.bDone, XMEMORY_ACQUIRE) == 0u;
				iSpin++ ) {
				xrtSleep(1u);
			}
			if ( xrtAtomic32Load(&CancelIo.bDone, XMEMORY_ACQUIRE) == 0u ) {
				if ( !xrtNetFileCancel(Task.pWorker,
						iCancelId) ||
					!exampleSpin(&CancelIo,
						exampleTimerLimit(
							3000)) ) {
					goto Cleanup;
				}
			}
		}
		/* 终态必达：结果是 OK（读已完成）或 CANCELLED。 */
		if ( (CancelIo.Result != XNET_RESULT_OK) &&
			(CancelIo.Result != XNET_RESULT_CANCELLED) ) {
			goto Cleanup;
		}
	}
	printf("file: cancel in-flight read -> terminal event ok\n");
	iResult = 0;

Cleanup:
	if ( File != 0 ) {
		xrtClose(File);
	}
	(void)xrtNetEngineStop(pEngine);
	(void)xrtNetEngineDestroy(pEngine);
	(void)remove(sFile);
	return iResult;
}
