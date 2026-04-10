# XRT 子进程与命令执行入门

> 目标：快速把 `subprocess` 用在“本地命令执行、工具编排、agent shell 底座”这些场景里。

[English](subprocess-intro.en.md) | [返回教学文档](README.md)

---

## 1. 先记住 4 个结论

1. 普通工具执行优先走 `XPROC_TARGET_EXEC`
2. 需要 shell 解释时再切 `XPROC_TARGET_SHELL`
3. 一次性执行优先 `xrtExecCapture()`
4. 交互式或流式场景优先 `xrtProcessSpawn()`


## 2. 最常用的两条路径

### 2.1 一次性执行并拿结果

适合：

- `git status`
- `gcc`
- `pytest`
- 短脚本

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

	if ( xrtExecCapture(&tConfig, &tResult, 5000u) ) {
		printf("exit=%d\n", tResult.ExitInfo.iExitCode);
		printf("kind=%d\n", tResult.ExitInfo.iKind);
		printf("stdout=%s\n", tResult.pStdout ? (char*)tResult.pStdout : "");
		printf("stderr=%s\n", tResult.pStderr ? (char*)tResult.pStderr : "");
		xrtProcessResultUnit(&tResult);
	}

	xrtUnit();
	return 0;
}
```

这条路径会自动把 stdout / stderr 变成 capture 模式，并在结束后收口成 `xprocessresult`。


### 2.2 手动控制生命周期

适合：

- 需要边跑边读输出
- 需要写 stdin
- 需要中断、终止、强杀
- 需要接 `future`

```c
static void procStdout(xprocess* pProcess, const void* pData, size_t iSize, ptr pUserData)
{
	(void)pProcess;
	(void)pUserData;
	fwrite(pData, 1u, iSize, stdout);
}

int main(void)
{
	xprocessconfig tConfig;
	xprocessevents tEvents;
	xprocess* pProcess;
	str arrArgs[] = { (str)"--version" };

	xrtInit();

	memset(&tEvents, 0, sizeof(tEvents));
	tEvents.OnStdout = procStdout;

	xrtProcessConfigInit(&tConfig);
	tConfig.sProgram = (str)"git";
	tConfig.arrArgs = arrArgs;
	tConfig.iArgCount = 1u;
	tConfig.Stdout.iMode = XPROC_STDIO_PIPE;
	tConfig.pEvents = &tEvents;

	pProcess = xrtProcessSpawn(&tConfig);
	if ( pProcess != NULL ) {
		(void)xrtProcessWait(pProcess);
		xrtProcessDestroy(pProcess);
	}

	xrtUnit();
	return 0;
}
```


## 3. 什么时候用 shell

如果命令本身依赖：

- shell 内建命令
- 管道、重定向、`&&`
- shell 展开规则

才切到：

```c
xrtProcessConfigInit(&tConfig);
tConfig.iTargetKind = XPROC_TARGET_SHELL;
tConfig.sCommand = (str)"echo shell-out";
```

你不需要自己拼：

- Windows `cmd /c ...`
- POSIX `/bin/sh -c ...`

`subprocess` 会按平台自动补上默认 shell。  
如果你明确要 `pwsh` 或别的 shell，可以把它放在 `sProgram`。


## 4. 如何写 stdin

要给子进程喂输入时：

```c
xrtProcessConfigInit(&tConfig);
tConfig.sProgram = (str)"python";
tConfig.Stdin.iMode = XPROC_STDIO_PIPE;
tConfig.Stdout.iMode = XPROC_STDIO_PIPE;
```

然后：

```c
xrtProcessWriteText(pProcess, (str)"line-1\nline-2\n", 0u);
xrtProcessCloseStdin(pProcess);
```

很多程序只有在看到 EOF 后才会继续退出，所以 `CloseStdin()` 往往是必要步骤。


## 5. 如何安全轮询输出

如果上层是 agent host，往往会定期轮询增量输出，而不是一次性拿完整快照。  
这时优先用：

```c
xprocessreadinfo tReadInfo;
size_t iSize = 0u;
uint64 iOffset = 0u;
ptr pChunk;

memset(&tReadInfo, 0, sizeof(tReadInfo));
pChunk = xrtProcessReadStdoutSince(pProcess, iOffset, 0u, &iSize, &tReadInfo);
if ( pChunk != NULL ) {
	fwrite(pChunk, 1u, iSize, stdout);
	xrtFree(pChunk);
}
iOffset = tReadInfo.iNextOffset;
```

这里几个字段要记住：

- `iBaseOffset`
	- 当前缓冲最早还能读到的偏移
- `iNextOffset`
	- 下一次读取应继续的位置
- `iStreamEndOffset`
	- 当前流尾位置
- `bDone`
	- 该流是否已经结束

如果你传入的 offset 小于 `iBaseOffset`，说明更早的数据已经不在当前保留窗口里了。


### 5.2 统一事件时间线

如果你要还原“进程启动 -> stdout/stderr/terminal 输出 -> 退出”的完整时间线，直接轮询事件流：

```c
xprocesseventreadinfo tEventInfo;
xprocessevent* arrEvents = NULL;
uint32 iEventCount = 0u;
uint64 iSeq = 0u;

memset(&tEventInfo, 0, sizeof(tEventInfo));
arrEvents = xrtProcessReadEventsSince(pProcess, iSeq, 0u, &iEventCount, &tEventInfo);
if ( arrEvents != NULL ) {
	for ( uint32 i = 0u; i < iEventCount; i++ ) {
		printf("seq=%llu kind=%d stream=%d size=%llu\n",
			(unsigned long long)arrEvents[i].iSeq,
			arrEvents[i].iKind,
			arrEvents[i].iStream,
			(unsigned long long)arrEvents[i].iSize);
	}
	xrtFree(arrEvents);
}
iSeq = tEventInfo.iNextSeq;
```

如果是 terminal 模式，`iStream` 会是 `XPROC_STREAM_TERMINAL`。


## 6. 环境变量和工作目录

### 6.1 环境变量覆盖

```c
str arrEnv[] = {
	(str)"LANG=C",
	(str)"XRT_TOOL_MODE=agent"
};

xrtProcessConfigInit(&tConfig);
tConfig.sProgram = (str)"python";
tConfig.arrEnv = arrEnv;
tConfig.iEnvCount = 2u;
tConfig.bInheritEnv = true;
```

如果你想完全隔离：

```c
tConfig.bInheritEnv = false;
```


### 6.2 严格工作目录

```c
xrtProcessConfigInit(&tConfig);
tConfig.sProgram = (str)"git";
tConfig.sWorkDir = (str)"D:/git/xrt";
```

如果目录无效，启动会直接失败，并在 `ExitInfo` 里体现为：

- `iKind = XPROC_EXIT_SPAWN_FAILED`
- `iStage = XPROC_STAGE_WORKDIR`

不会再静默回退到旧目录。


## 7. terminal 模式

如果你要跑交互式 shell、REPL、明确依赖 TTY 的程序，可以打开 terminal：

```c
xrtProcessConfigInit(&tConfig);
tConfig.iTargetKind = XPROC_TARGET_SHELL;
tConfig.bUseTerminal = true;
```

建议先判断：

```c
if ( xrtProcessTerminalSupported() ) {
	/* spawn */
}
```

如果已经启动成功，还可以在运行中调整尺寸：

```c
xrtProcessResizeTerminal(pProcess, 100u, 32u);
```

这里要特别记住一条：

- terminal 模式下拿到的是“原始终端字节流”
- 里面可能包含 ANSI / VT 控制序列、标题更新序列等
- 如果你要给 UI 展示纯文本，要自己做 terminal escape 过滤


## 8. 退出结果怎么判断

不要只看 `xrtProcessExitCode()`，更推荐看：

```c
xprocessexitinfo tExitInfo;

if ( xrtProcessGetExitInfo(pProcess, &tExitInfo) ) {
	printf("kind=%d\n", tExitInfo.iKind);
	printf("exit=%d\n", tExitInfo.iExitCode);
	printf("signal=%d\n", tExitInfo.iSignal);
	printf("stage=%d\n", tExitInfo.iStage);
	printf("os_error=%d\n", tExitInfo.iOsError);
	printf("stop=%d\n", tExitInfo.iStopReason);
	printf("timed_out=%d\n", tExitInfo.bTimedOut);
	printf("cancelled=%d\n", tExitInfo.bCancelled);
}
```

尤其在 agent/tool 场景下，下面几类要能区分：

- 命令没启动起来
- 命令启动了但失败退出
- 命令超时后被你收掉了
- 命令被信号结束


## 9. 停止语义

当前有四层停止动作：

- `xrtProcessInterrupt()`
	- 尽量发中断
- `xrtProcessTerminate()`
	- 尽量温和退出
- `xrtProcessKill()`
	- 强杀当前进程
- `xrtProcessKillTree()`
	- 强杀整棵进程树

推荐梯度：

1. `xrtProcessWaitTimeout()`
2. 超时后 `xrtProcessInterrupt()`
3. 再超时 `xrtProcessTerminate()`
4. 最后 `xrtProcessKillTree()`

`xrtExecCapture()` 的超时收口内部也遵循类似思路。


## 10. future 链接

如果编译包含 future 层，可以把“子进程完成”直接接到异步链路：

```c
xfuture* pFuture = xrtProcessWaitFuture(pProcess);
if ( pFuture != NULL ) {
	if ( xFutureWaitValueTimeout(pFuture, 3000) == pProcess ) {
		printf("done\n");
	}
	xFutureRelease(pFuture);
}
```


## 11. 几个容易踩坑的点

- `xrtProcessDestroy()` 不是 stop API
- `OnExit` 现在拿到的是 `xprocessexitinfo*`
- `GetStdout/GetStderr` 返回新内存，记得 `xrtFree()`
- `PIPE` 模式才适合流式读取和写 stdin
- terminal 模式拿到的是原始终端字节流，不是自动清洗后的纯文本
- 能不用 shell 就别用 shell


## 12. 进一步阅读

- [Subprocess API](../api/api-subprocess.md)
- [Thread](../api/api-thread.md)
- [Future / Task / Promise](../api/api-future-task-promise.md)
- [用 Subprocess + File Async 写一个工具链流水线](../case/subprocess-file-async-pipeline.md)
