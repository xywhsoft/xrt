# 用 Subprocess + File Async 写一个工具链流水线

> 这个案例聚焦“启动外部工具 -> 等待完成 -> 异步落盘 -> 再异步读取结果”这条主线，把 `subprocess + file_async + future` 串成一个完整可落地的工具链模型。

[返回案例索引](README.md)

---

## 1. 场景

假设你要写一个“工具编排程序”，它有 5 个约束：

1. 需要启动一个外部 CLI 工具
2. 需要向工具 stdin 写入输入
3. 需要拿到 stdout / stderr 结果
4. 需要把结果保存到工作目录
5. 保存完后，还要继续把结果接到后续异步步骤里

如果没有一条清晰主线，代码很容易变成：

- `system()` 或 shell 管道临时拼命令
- 子进程结束点、exit code 和 timeout 没有正式对象
- 文件落盘全都写成同步阻塞 I/O
- stdout / stderr 的所有权和释放时机越写越乱

这个案例要解决的，正是“一个外部工具怎样被稳定地接进 XRT 的 async/future 主线”。


## 2. 为什么这次不用别的方案

这一页只选当前最适合“工具链流水线”的几层能力。

### 2.1 为什么不是 `system()` 或 shell 字符串拼接

因为这次的问题不只是“把命令跑起来”。

真正要解决的是：

- 程序怎样 direct argv 启动
- stdin / stdout / stderr 怎样显式管道化
- 子进程怎样变成一个正式等待对象
- 输出结果怎样继续进入异步文件链路

`system()` 对这种需求太粗了。


### 2.2 为什么不是全程同步文件 I/O

因为这里的目标不是“写一个最短 demo”，而是：

- 让工具执行结果继续留在 future 链路里
- 让多个文件操作仍然可以并发组合
- 让后续步骤继续用统一等待模型往下接

所以这页刻意把保存输出、发布结果、读取预览都写进异步文件主线。


### 2.3 为什么这次不用 `task group`

`task group` 更适合：

- 一批结构化 child task
- 一个逻辑 scope 里的 fan-out / join

这个案例的核心不是“收口一批并发 child”，而是：

- 一个子进程完成后
- 把产物继续交给文件异步链路

所以这里更像：

- `subprocess` 负责外部工具执行
- `future` 负责等待与组合
- `file_async` 负责产物落盘和后续读取


## 3. 这条链里每一层负责什么

| 层 | 这次承担的角色 | 你真正得到的能力 |
|----|----------------|------------------|
| `xprocess` | 启动和管理外部工具 | direct argv、stdin/stdout/stderr、exit code |
| `xrtProcessWaitFuture()` | 把“进程完成”变成 future | timeout、统一等待 |
| `xrtFilePutAllAsync()` | 并发保存 stdout / stderr | 产物落盘继续在 future 主线里 |
| `xFutureWhenAll()` | 把多条写盘 future 合成一个 barrier | 批量文件结果统一等待 |
| `xrtFileMoveAsync()` | 发布最终产物 | 从临时文件切换到正式文件 |
| `xasyncfile` | 按 offset 异步读取产物预览 | 句柄层文件任务 |

可以先记一句话：

> `subprocess` 负责把工具跑起来，`future` 负责把它接进统一等待模型，`file_async` 负责把工具产物继续推进到下一步。


## 4. 代码目标

下面这段完整代码会做 7 件事：

1. 主程序在“父模式”和“子工具模式”之间分支
2. 父程序异步创建工作目录
3. 父程序 direct argv 启动自己，作为一个子工具
4. 父程序向子工具 stdin 写入文本，并通过 future 等待退出
5. 父程序复制 stdout / stderr 捕获缓冲区，并并发写入两个文件
6. 父程序把临时 stdout 文件异步移动成正式报告
7. 父程序再用 `xasyncfile` 按 offset 异步读取预览


## 5. 完整代码

```c
#include "xrt.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void procPrintFutureFailure(str sStep, xfuture* pFuture)
{
	const char* sError;

	sError = (pFuture != NULL) ? xFutureError(pFuture) : NULL;
	printf(
		"%s failed: state=%d status=%d error=%s\n",
		(sStep != NULL) ? sStep : "future",
		(int)((pFuture != NULL) ? xFutureState(pFuture) : -1),
		(int)((pFuture != NULL) ? xFutureStatus(pFuture) : XRT_NET_ERROR),
		(sError != NULL) ? sError : "(null)"
	);
}

static bool procWaitFutureResolved(str sStep, xfuture* pFuture, int64 iTimeoutMs)
{
	if ( pFuture == NULL ) {
		printf("%s failed: future missing\n", (sStep != NULL) ? sStep : "future");
		return FALSE;
	}

	if ( !xFutureWaitTimeout(pFuture, iTimeoutMs) ) {
		printf("%s timeout\n", (sStep != NULL) ? sStep : "future");
		return FALSE;
	}

	if ( xFutureState(pFuture) != XFUTURE_RESOLVED ) {
		procPrintFutureFailure(sStep, pFuture);
		return FALSE;
	}

	return TRUE;
}

static bool procWaitIoFuture(str sStep, xfuture** ppFuture)
{
	xasyncfileio* pInfo;

	if ( ppFuture == NULL || *ppFuture == NULL ) {
		printf("%s failed: future missing\n", (sStep != NULL) ? sStep : "io");
		return FALSE;
	}

	if ( !procWaitFutureResolved(sStep, *ppFuture, 5000) ) {
		xFutureRelease(*ppFuture);
		*ppFuture = NULL;
		return FALSE;
	}

	pInfo = (xasyncfileio*)xFutureValue(*ppFuture);
	if ( pInfo == NULL ) {
		printf("%s failed: io result missing\n", (sStep != NULL) ? sStep : "io");
		xFutureRelease(*ppFuture);
		*ppFuture = NULL;
		return FALSE;
	}

	xrtAsyncFileIoDestroy(pInfo);
	xFutureRelease(*ppFuture);
	*ppFuture = NULL;
	return TRUE;
}

static void procReleaseFuture(xfuture** ppFuture)
{
	if ( ppFuture == NULL || *ppFuture == NULL ) {
		return;
	}

	xFutureRelease(*ppFuture);
	*ppFuture = NULL;
}

static void procReleaseIoFutureIfReady(xfuture** ppFuture)
{
	xasyncfileio* pInfo;

	if ( ppFuture == NULL || *ppFuture == NULL ) {
		return;
	}

	if ( xFutureState(*ppFuture) == XFUTURE_RESOLVED ) {
		pInfo = (xasyncfileio*)xFutureValue(*ppFuture);
		if ( pInfo != NULL ) {
			xrtAsyncFileIoDestroy(pInfo);
		}
	}

	xFutureRelease(*ppFuture);
	*ppFuture = NULL;
}

static void procAsciiUpper(char* sText)
{
	size_t i;

	if ( sText == NULL ) {
		return;
	}

	for ( i = 0; sText[i] != '\0'; i++ ) {
		sText[i] = (char)toupper((unsigned char)sText[i]);
	}
}

static int procChildToolMain(void)
{
	char arrLine[256];
	int32 iLineNo = 0;

	while ( fgets(arrLine, sizeof(arrLine), stdin) != NULL ) {
		size_t iLen = strlen(arrLine);

		if ( (iLen > 0u) && (arrLine[iLen - 1u] == '\n') ) {
			arrLine[iLen - 1u] = '\0';
			iLen--;
		}

		if ( iLen == 0u ) {
			fprintf(stderr, "skip empty line\n");
			continue;
		}

		procAsciiUpper(arrLine);
		iLineNo++;
		printf("ITEM %d: %s\n", (int)iLineNo, arrLine);
	}

	fprintf(stderr, "processed=%d\n", (int)iLineNo);
	return 0;
}

static int procParentPipeline(void)
{
	static const char sInput[] =
		"alpha\n"
		"beta\n"
		"\n"
		"charlie\n";

	xfuture* pDirFuture = NULL;
	xfuture* pWaitFuture = NULL;
	xfuture* pWriteStdoutFuture = NULL;
	xfuture* pWriteStderrFuture = NULL;
	xfuture* pWriteAllFuture = NULL;
	xfuture* pMoveFuture = NULL;
	xfuture* pReadFuture = NULL;
	xprocess* pProcess = NULL;
	xasyncfile* pFile = NULL;
	xasyncfilebuf* pPreview = NULL;
	xprocessconfig tConfig;
	xasyncfileconfig tFileCfg;
	str arrArgs[] = { (str)"--child-tool" };
	ptr pStdout = NULL;
	ptr pStderr = NULL;
	ptr pStdoutCopy = NULL;
	ptr pStderrCopy = NULL;
	size_t iStdoutSize = 0u;
	size_t iStderrSize = 0u;
	bool bWriteFutureStarted = FALSE;

	pDirFuture = xrtDirCreateAllAsync((str)"demo_pipeline");
	if ( !procWaitIoFuture((str)"create work dir", &pDirFuture) ) {
		goto cleanup;
	}

	xrtProcessConfigInit(&tConfig);
	tConfig.iFlags =
		XPROC_F_PIPE_STDIN |
		XPROC_F_PIPE_STDOUT |
		XPROC_F_PIPE_STDERR |
		XPROC_F_KILL_TREE;
	tConfig.sProgram = xCore.AppFile;
	tConfig.arrArgs = arrArgs;
	tConfig.iArgCount = 1u;
	tConfig.iReadChunkSize = 4096u;
	tConfig.iMaxCaptureBytes = 64u * 1024u;

	pProcess = xrtProcessSpawn(&tConfig);
	if ( pProcess == NULL ) {
		printf("spawn child failed: %s\n", xrtGetError());
		goto cleanup;
	}

	if ( xrtProcessWriteText(pProcess, (str)sInput, 0) != (int64)(sizeof(sInput) - 1u) ) {
		printf("write stdin failed\n");
		goto cleanup;
	}

	if ( !xrtProcessCloseStdin(pProcess) ) {
		printf("close stdin failed\n");
		goto cleanup;
	}

	pWaitFuture = xrtProcessWaitFuture(pProcess);
	if ( pWaitFuture == NULL ) {
		printf("wait future create failed\n");
		goto cleanup;
	}

	if ( !procWaitFutureResolved((str)"wait child process", pWaitFuture, 5000) ) {
		if ( xrtProcessIsRunning(pProcess) ) {
			(void)xrtProcessKillTree(pProcess);
			(void)xrtProcessWait(pProcess);
		}
		goto cleanup;
	}

	if ( xFutureValue(pWaitFuture) != pProcess ) {
		printf("wait future returned unexpected value\n");
		goto cleanup;
	}

	if ( xrtProcessExitCode(pProcess) != 0 ) {
		printf("child exit code = %d\n", xrtProcessExitCode(pProcess));
		goto cleanup;
	}

	pStdout = xrtProcessGetStdout(pProcess, &iStdoutSize);
	pStderr = xrtProcessGetStderr(pProcess, &iStderrSize);
	if ( pStdout == NULL || iStdoutSize == 0u ) {
		printf("captured stdout is empty\n");
		goto cleanup;
	}
	if ( pStderr == NULL || iStderrSize == 0u ) {
		printf("captured stderr is empty\n");
		goto cleanup;
	}

	/*
	 * 当前实现和测试表明，GetStdout/GetStderr 返回的是由 xprocess 持有的借用缓冲区。
	 * 这里先复制一份，再把副本交给 file_async，后续文件任务就不再依赖 xprocess 生命周期。
	 */
	pStdoutCopy = xrtMalloc(iStdoutSize);
	pStderrCopy = xrtMalloc(iStderrSize);
	if ( pStdoutCopy == NULL || pStderrCopy == NULL ) {
		printf("capture copy alloc failed\n");
		goto cleanup;
	}

	memcpy(pStdoutCopy, pStdout, iStdoutSize);
	memcpy(pStderrCopy, pStderr, iStderrSize);

	xrtProcessDestroy(pProcess);
	pProcess = NULL;
	procReleaseFuture(&pWaitFuture);

	pWriteStdoutFuture = xrtFilePutAllAsync((str)"demo_pipeline/stdout.tmp", pStdoutCopy, iStdoutSize);
	pWriteStderrFuture = xrtFilePutAllAsync((str)"demo_pipeline/stderr.log", pStderrCopy, iStderrSize);
	bWriteFutureStarted = (pWriteStdoutFuture != NULL) || (pWriteStderrFuture != NULL);
	if ( pWriteStdoutFuture == NULL || pWriteStderrFuture == NULL ) {
		printf("write future create failed\n");
		goto cleanup;
	}

	{
		xfuture* arrWriteFuture[2];

		arrWriteFuture[0] = pWriteStdoutFuture;
		arrWriteFuture[1] = pWriteStderrFuture;
		pWriteAllFuture = xFutureWhenAll(arrWriteFuture, 2);
		if ( pWriteAllFuture == NULL ) {
			printf("write barrier create failed\n");
			goto cleanup;
		}
	}

	if ( !procWaitFutureResolved((str)"write captured files", pWriteAllFuture, 5000) ) {
		goto cleanup;
	}

	procReleaseFuture(&pWriteAllFuture);

	if ( !procWaitIoFuture((str)"write stdout temp", &pWriteStdoutFuture) ) {
		goto cleanup;
	}

	if ( !procWaitIoFuture((str)"write stderr log", &pWriteStderrFuture) ) {
		goto cleanup;
	}

	xrtFree(pStdoutCopy);
	pStdoutCopy = NULL;
	xrtFree(pStderrCopy);
	pStderrCopy = NULL;
	bWriteFutureStarted = FALSE;

	pMoveFuture = xrtFileMoveAsync((str)"demo_pipeline/stdout.tmp", (str)"demo_pipeline/report.txt", TRUE);
	if ( !procWaitIoFuture((str)"publish final report", &pMoveFuture) ) {
		goto cleanup;
	}

	xrtAsyncFileConfigInit(&tFileCfg);
	tFileCfg.iFlags = XAFILE_F_READ;
	tFileCfg.iShareFlags = XAFILE_SHARE_READ;
	tFileCfg.sPath = (str)"demo_pipeline/report.txt";

	pFile = xrtAsyncFileOpen(&tFileCfg);
	if ( pFile == NULL ) {
		printf("open final report failed: %s\n", xrtGetError());
		goto cleanup;
	}

	pReadFuture = xrtAsyncFileReadAt(pFile, 0u, 64u);
	if ( pReadFuture == NULL ) {
		printf("preview read future create failed\n");
		goto cleanup;
	}

	if ( !procWaitFutureResolved((str)"read report preview", pReadFuture, 5000) ) {
		goto cleanup;
	}

	pPreview = (xasyncfilebuf*)xFutureValue(pReadFuture);
	if ( pPreview == NULL ) {
		printf("preview read result missing\n");
		goto cleanup;
	}

	printf("report preview (%u bytes):\n", (unsigned)pPreview->iSize);
	printf("%.*s", (int)pPreview->iSize, (char*)pPreview->pData);
	if ( pPreview->iSize == 0u || ((char*)pPreview->pData)[pPreview->iSize - 1u] != '\n' ) {
		printf("\n");
	}

	xrtAsyncFileBufDestroy(pPreview);
	pPreview = NULL;
	procReleaseFuture(&pReadFuture);
	xrtAsyncFileClose(pFile);
	pFile = NULL;
	return 0;

cleanup:
	if ( bWriteFutureStarted ) {
		if ( pWriteAllFuture != NULL ) {
			(void)xFutureWaitTimeout(pWriteAllFuture, 5000);
		} else {
			if ( pWriteStdoutFuture != NULL ) {
				(void)xFutureWaitTimeout(pWriteStdoutFuture, 5000);
			}
			if ( pWriteStderrFuture != NULL ) {
				(void)xFutureWaitTimeout(pWriteStderrFuture, 5000);
			}
		}
	}

	if ( pPreview != NULL ) {
		xrtAsyncFileBufDestroy(pPreview);
		pPreview = NULL;
	}
	procReleaseFuture(&pReadFuture);
	if ( pFile != NULL ) {
		xrtAsyncFileClose(pFile);
		pFile = NULL;
	}
	procReleaseIoFutureIfReady(&pMoveFuture);
	procReleaseFuture(&pWriteAllFuture);
	procReleaseIoFutureIfReady(&pWriteStdoutFuture);
	procReleaseIoFutureIfReady(&pWriteStderrFuture);
	xrtFree(pStdoutCopy);
	xrtFree(pStderrCopy);
	procReleaseFuture(&pWaitFuture);

	if ( pProcess != NULL ) {
		if ( xrtProcessIsRunning(pProcess) ) {
			(void)xrtProcessKillTree(pProcess);
			(void)xrtProcessWait(pProcess);
		}
		xrtProcessDestroy(pProcess);
	}

	return 1;
}

int main(int argc, char** argv)
{
	int iRet;

	xrtInit();

	if ( argc >= 2 && strcmp(argv[1], "--child-tool") == 0 ) {
		iRet = procChildToolMain();
		xrtUnit();
		return iRet;
	}

	iRet = procParentPipeline();
	xrtUnit();
	return iRet;
}
```


## 6. 这段代码最重要的 4 个边界

### 6.1 进程执行边界

父程序并没有把业务逻辑直接写在本进程里，而是显式做了两件事：

1. 用 direct argv 启动子进程
2. 用 `xrtProcessWriteText()` 和 `xrtProcessCloseStdin()` 把输入交给子工具

也就是说：

- 子进程负责“真正执行外部工具逻辑”
- 父进程负责“编排这条工具链”


### 6.2 等待边界

这里没有直接调用：

- `xrtProcessWait()`

而是把“进程完成”转成：

- `xrtProcessWaitFuture()`

这样 timeout、失败、后续组合等待，都继续留在 future 主线里。


### 6.3 捕获缓冲区边界

这一点非常关键。

从当前实现和测试可以看出：

- `xrtProcessGetStdout()`
- `xrtProcessGetStderr()`

返回的是由 `xprocess` 持有的借用缓冲区，而不是调用方自己释放的独立对象。

所以本例先做了一步：

- 复制 stdout / stderr 到自己的堆缓冲区

再把这些副本交给：

- `xrtFilePutAllAsync()`

这样后面的异步文件任务，就不再依赖 `xprocess` 的生命周期。


### 6.4 文件链路边界

这段代码故意同时演示了两层文件 async：

- 路径 helper 层：`xrtDirCreateAllAsync()`、`xrtFilePutAllAsync()`、`xrtFileMoveAsync()`
- 句柄层：`xrtAsyncFileOpen()`、`xrtAsyncFileReadAt()`

可以这样理解：

- 一段式工具链产物处理，优先用路径 helper
- 需要显式 offset、复用句柄、继续按块读取时，再切到 `xasyncfile`


## 7. 运行过程怎么理解

可以把这段代码按下面顺序读：

1. 父程序异步创建工作目录
2. 父程序启动子工具
3. 父程序把输入文本写进子工具 stdin
4. 子工具把每一行转成大写，并把结果写到 stdout
5. 子工具把处理统计写到 stderr
6. 父程序通过 `wait future` 等到子工具结束
7. 父程序复制捕获缓冲区，并并发写入 `stdout.tmp` 和 `stderr.log`
8. 父程序把 `stdout.tmp` 异步移动成 `report.txt`
9. 父程序再用 `xasyncfile` 异步读取报告预览

也就是说，这条链不是：

- “子进程结束后，回到同步阻塞文件 I/O”

而是：

- `subprocess -> wait future -> file async futures -> final preview`


## 8. 这条模型为什么适合工具链

很多工具编排代码一开始都能跑，但很快会变成：

- shell 命令字符串越来越长
- 标准输出和错误输出混在一起
- 文件落盘变成阻塞 I/O
- timeout、失败、退出码和产物路径没有统一边界

而这条模型里：

- 启动方式走 direct argv
- 子进程完成点走 future
- 产物落盘走 file_async
- 多个写盘操作可以继续用 `WhenAll`

所以工具链会更容易扩展成：

- 批处理程序
- 自动化构建任务
- agent 工具执行链


## 9. 常见错误

### 9.1 错误一：默认直接用 shell 命令字符串

这会让参数转义、跨平台行为和错误定位都变糊。


### 9.2 错误二：把 `xrtProcessGetStdout()` 当成调用方自有内存

这会把所有权写错。当前实现下，它更像借用视图。


### 9.3 错误三：刚拿到捕获输出就立刻销毁 `xprocess`

如果后续异步任务还在依赖那块缓冲区，这样很危险。


### 9.4 错误四：把整条工具链做到一半，又切回同步文件 I/O

这样就把等待模型打断了，后续不容易继续组合。


### 9.5 错误五：只看 stdout，不看 stderr 和 exit code

很多工具真正的错误信息都在 stderr，而不是 stdout。


## 10. 建议继续阅读

- [XRT 子进程与工具执行入门](../guide/subprocess-intro.md)
- [XRT 异步文件与目录操作入门](../guide/file-async-intro.md)
- [Future / Task / Promise](../api/api-future-task-promise.md)
- [File API](../api/api-file.md)
- [用 Queue + Future 写一个多生产者 Worker](queue-worker-future.md)
- [线程、协程与 Future 协同模型](thread-coroutine-future.md)

---

**一句话总结：** 这条案例主线的关键不在“把命令跑起来”，而在“子进程完成点要进入 future，工具产物要继续走 file_async，最后整条链仍然保持统一等待模型”；这正是 XRT 里工具程序和 agent 工作流最值得掌握的一条写法。
