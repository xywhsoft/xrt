# XRT 子进程与工具执行入门

> 目标：快速上手 `xrtExecCapture()`、`xrtProcessSpawn()`、stdin/stdout/stderr 管道、以及 `xrtProcessWaitFuture()` 这一组能力。

建议先读：

- [OS 入门：什么时候只要启动一个程序，什么时候已经该上 subprocess](os-intro.md)

[English](subprocess-intro.en.md) | [返回教学文档](README.md)

---

## 1. 这组 API 解决什么问题

当前 XRT 主线现在提供了一层轻量子进程能力，适合这些场景：

- 直接运行另一个程序
- 抓取程序的 stdout / stderr
- 在程序运行过程中流式消费输出
- 向子进程 stdin 写入输入
- 同步等待或通过 future 异步等待

这套接口很适合：

- CLI 工具编排
- 构建脚本
- 自动化执行链
- AI Agent 工具调用链

---

## 2. 两个主要入口

### 2.1 `xrtExecCapture()`

当你要的是：

- 一次性执行
- 完整收集 stdout / stderr
- 最后拿 exit code

优先用它。

```c
#include "xrt.h"

int main(void)
{
	xprocessconfig tConfig;
	xprocessresult tResult;
	str arrArgs[] = { (str)"status", (str)"--short" };

	xrtInit();

	xrtProcessConfigInit(&tConfig);
	tConfig.sProgram = (str)"git";
	tConfig.arrArgs = arrArgs;
	tConfig.iArgCount = 2;

	if ( xrtExecCapture(&tConfig, &tResult, 5000) ) {
		printf("exit = %d\n", tResult.iExitCode);
		printf("stdout = %s\n", (char*)tResult.pStdout);
		printf("stderr = %s\n", (char*)tResult.pStderr);
		xrtProcessResultUnit(&tResult);
	}

	xrtUnit();
	return 0;
}
```

### 2.2 `xrtProcessSpawn()`

当你要的是：

- 流式回调
- 手动写 stdin
- 显式控制进程生命周期
- future 异步等待

优先用它。

---

## 3. 最小流式模式

```c
static void procStdout(xprocess* pProcess, const void* pData, size_t iSize, ptr pUserData)
{
	(void)pProcess;
	(void)pUserData;
	fwrite(pData, 1, iSize, stdout);
}

int main(void)
{
	xprocessconfig tConfig;
	xprocessevents tEvents;
	xprocess* pProcess;
	str arrArgs[] = { (str)"-c", (str)"print('hello from child')" };

	xrtInit();

	memset(&tEvents, 0, sizeof(tEvents));
	tEvents.OnStdout = procStdout;

	xrtProcessConfigInit(&tConfig);
	tConfig.iFlags = XPROC_F_PIPE_STDOUT;
	tConfig.sProgram = (str)"python";
	tConfig.arrArgs = arrArgs;
	tConfig.iArgCount = 2;
	tConfig.pEvents = &tEvents;

	pProcess = xrtProcessSpawn(&tConfig);
	if ( pProcess ) {
		xrtProcessWait(pProcess);
		xrtProcessDestroy(pProcess);
	}

	xrtUnit();
	return 0;
}
```

---

## 4. 给子进程写入 stdin

如果子进程需要输入：

1. 打开 `XPROC_F_PIPE_STDIN`
2. 调用 `xrtProcessWrite()` 或 `xrtProcessWriteText()`
3. 写完后调用 `xrtProcessCloseStdin()`

第三步很重要，因为很多命令行程序只有收到 EOF 才会继续往下跑。

```c
xrtProcessWriteText(pProcess, (str)"line-1\nline-2\n", 0);
xrtProcessCloseStdin(pProcess);
```

---

## 5. 用 future 等待子进程

如果当前编译包含 future 层，那么还可以这样接：

```c
xfuture* pFuture = xrtProcessWaitFuture(pProcess);
if ( pFuture ) {
	if ( xFutureWaitValueTimeout(pFuture, 3000) == pProcess ) {
		printf("exit = %d\n", xrtProcessExitCode(pProcess));
	}
	xFutureRelease(pFuture);
}
```

这对需要把“子进程完成”接到其他 async/future 链路里的场景会很方便。

---

## 6. 几个关键语义

- `xrtProcessDestroy()` 不是 kill。它只负责销毁已经退出的进程对象。
- `OnStdout`、`OnStderr`、`OnExit` 运行在内部工作线程上，不会自动切回调用线程。
- `XPROC_F_MERGE_STDERR` 会把 stderr 合并进 stdout。
- `XPROC_F_NO_CAPTURE` 用于只流式消费、不做内存缓存的场景。
- `iMaxCaptureBytes == 0` 表示不限制缓存大小。
- `XPROC_F_USE_SHELL` 会从 direct argv 模式切到 shell 模式。

大多数工具调用链，建议默认走 direct argv 模式。

---

## 7. 工具链场景下的推荐默认值

如果你的目标是一般工具编排，可以先从这组默认习惯开始：

- 默认优先 direct argv
- 开启 `XPROC_F_PIPE_STDOUT | XPROC_F_PIPE_STDERR`
- 只有确实要输入时再开 `XPROC_F_PIPE_STDIN`
- 可能拉起子 worker 的工具，建议带上 `XPROC_F_KILL_TREE`
- 短命令优先 `xrtExecCapture()`
- 长命令、交互式命令、流式命令优先 `xrtProcessSpawn()`

---

## 8. 延伸阅读

- [Base](../api/api-base.md)
- [Thread](../api/api-thread.md)
- [Future / Task / Promise](../api/api-future-task-promise.md)
- [用 Subprocess + File Async 写一个工具链流水线](../case/subprocess-file-async-pipeline.md)
- [XRT 运行时与线程附加入门](runtime-thread-attach.md)
