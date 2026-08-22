# OS 入门：什么时候只要启动一个程序，什么时候已经该上 subprocess

> 目标：把 `xrtStart()`、`xrtChain()`、`xrtRun()` 讲成三种不同边界，而不是三个看起来差不多的函数。读完这页后，你应该明确知道：什么时候只是想“打开一个文件或 URL”，什么时候需要同步拿退出码，什么时候只想后台启动，以及什么时候这三者都不够，应该直接升级到 `subprocess`。

[返回教学文档](README.md)

---

## 1. 为什么需要这一页

很多程序迟早都会碰到这些系统边界：

- 打开一份本地文档
- 在浏览器里打开一个帮助页
- 跑一个外部命令并等它结束
- 启动一个后台工具，让当前线程继续往下跑

这几件事看起来都像“启动外部程序”，但它们真正要的语义完全不同：

- 有的要“交给系统默认程序”
- 有的要“阻塞等待退出码”
- 有的要“立刻返回，不等它结束”

`os` 这组 API 的价值，就在于把这三种边界分清楚。


## 2. 先把 3 个入口分清楚

### 2.1 `xrtStart()`：交给系统默认程序打开

它适合：

- 打开本地文件
- 打开目录
- 打开网页
- 打开 `mailto:` 之类的系统协议

你真正要表达的是：

- “像双击一样把它交给系统”

而不是：

- “我要自己控制这个进程怎么跑”


### 2.2 `xrtChain()`：同步执行，并等待退出码

它适合：

- 想明确知道命令有没有跑成功
- 当前线程允许阻塞
- 不需要把 stdout / stderr 收回程序内部

你真正要表达的是：

- “把这条命令跑完，再告诉我退出码”

所以它最重要的边界只有一条：

- 会阻塞当前线程


### 2.3 `xrtRun()`：异步启动，当前线程立刻继续

它适合：

- 启动一个后台工具
- 当前线程不想等
- 只要知道“它已经发起启动”就够了

你真正要表达的是：

- “发起启动后，我马上继续干别的”

所以它和 `xrtChain()` 最大的区别不是参数，而是：

- `xrtChain()` 等结束
- `xrtRun()` 不等结束


## 3. 这页为什么不直接从 subprocess 讲起

因为 `subprocess` 解决的是更重的一层问题：

- 捕获 stdout / stderr
- 写 stdin
- future 等待
- 明确生命周期管理

而这一页只讲“最轻的系统边界”：

- 打开
- 同步执行
- 异步启动

更直接地说：

- 只想打开文件 / URL：先看这页
- 只想同步拿退出码：先看这页
- 只想后台启动：先看这页
- 一旦需要捕获输出、超时、future、结构化控制：直接升级到 [XRT 子进程与工具执行入门](subprocess-intro.md)


## 4. 最小 DEMO：生成一个说明文件，再用系统默认程序打开

第一步先不要碰命令执行。  
先只做一件事：把一份说明文件落到 `build/os-demo/open-me.txt`，然后用 `xrtStart()` 把它交给系统默认程序打开。

```c
#include "xrt.h"
#include <stdio.h>
#include <stdint.h>

static bool procStartOk(ptr hResult)
{
#if defined(_WIN32) || defined(_WIN64)
	return ((intptr_t)hResult > 32);
#else
	return (hResult != NULL);
#endif
}


int main(void)
{
	xrtGlobalData* pCore = NULL;
	str sRootDir = NULL;
	str sNotePath = NULL;
	ptr hResult = NULL;
	const char* sText =
		"XRT OS demo\r\n"
		"this file should be opened by the system default app.\r\n";
	int iRet = 1;

	pCore = xrtInit();
	if ( pCore == NULL ) {
		return 1;
	}

	sRootDir = xrtPathJoin(3, pCore->AppPath, "build", "os-demo");
	sNotePath = xrtPathJoin(4, pCore->AppPath, "build", "os-demo", "open-me.txt");
	if ( (sRootDir == NULL) || (sNotePath == NULL) ) {
		fprintf(stderr, "build path failed\n");
		goto cleanup;
	}

	if ( xrtDirCreateAll(sRootDir) != TRUE ) {
		fprintf(stderr, "create os demo dir failed\n");
		goto cleanup;
	}

	if ( xrtFileWriteAll(sNotePath, (str)sText, 0u, XRT_CP_UTF8) <= 0 ) {
		fprintf(stderr, "write note file failed\n");
		goto cleanup;
	}

	hResult = xrtStart(sNotePath, 0u);
	if ( !procStartOk(hResult) ) {
		fprintf(stderr, "xrtStart failed\n");
		goto cleanup;
	}

	printf("opened with default app : %s\n", sNotePath);
	iRet = 0;

cleanup:
	if ( sNotePath != NULL ) {
		xrtFree(sNotePath);
	}
	if ( sRootDir != NULL ) {
		xrtFree(sRootDir);
	}
	xrtUnit();
	return iRet;
}
```

这个最小例子最想让你先记住的是：

1. `xrtStart()` 表达的是“交给系统默认程序”
2. 它很适合打开本地帮助文件、报表、HTML 结果
3. 它不负责把目标程序的输出带回你的程序
4. 在无桌面或无默认程序的环境里，它可能失败


## 5. 升级 DEMO：同一段程序里比较 `xrtChain()` 和 `xrtRun()`

第二步再来区分“同步执行”和“异步启动”。

这次要解决的真实问题是：

- 我有一条命令，必须等它结束并拿退出码
- 我还有一条命令，只想把它发出去，不想阻塞当前线程

这正是：

- `xrtChain()`
- `xrtRun()`

最容易讲清楚差别的地方。

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
#if defined(_WIN32) || defined(_WIN64)
	const char* sSyncCmd = "cmd.exe /c echo xrt-chain-demo";
	const char* sAsyncCmd = "cmd.exe /c timeout /t 3 > nul";
#else
	const char* sSyncCmd = "printf 'xrt-chain-demo\\n'";
	const char* sAsyncCmd = "sleep 3";
#endif
	double fStart = 0.0;
	double fEnd = 0.0;
	ptr hAsync = NULL;
	int iExitCode = -1;

	xrtInit();

	fStart = xrtTimer();
	iExitCode = xrtChain((str)sSyncCmd, 0u);
	fEnd = xrtTimer();

	printf("chain exit code : %d\n", iExitCode);
	printf("chain elapsed   : %.6f sec\n", fEnd - fStart);

	hAsync = xrtRun((str)sAsyncCmd, 0u);
	printf("run token       : %p\n", hAsync);

	xrtSleep(300);
	printf("main thread keeps running\n");

	xrtUnit();
	return 0;
}
```

这个升级例子要你真正看懂的是：

1. `xrtChain()` 会阻塞当前线程，直到命令结束
2. `xrtRun()` 发起后就返回，当前线程可以立刻继续
3. `xrtTimer()` 很适合量 `xrtChain()` 这类同步等待的耗时
4. 这两条路径都不是“捕获输出”的方案


## 6. 三者到底怎么选

可以先按这个最短判断表来记：

- 想打开文件、目录、URL
	- 用 `xrtStart()`
- 想同步执行，并拿退出码
	- 用 `xrtChain()`
- 想后台启动，不想等结束
	- 用 `xrtRun()`
- 想抓 stdout / stderr、写 stdin、异步等待
	- 用 [subprocess-intro.md](subprocess-intro.md)


## 7. 如果只有一条主线程，会卡在哪里

这一页最关键的主线程问题，只有一条：

- `xrtChain()` 会卡住当前线程

这意味着：

- CLI 小工具里通常没问题
- GUI 主线程里要谨慎
- HTTP / WebSocket / coroutine 调度主线里更不该随便直接调

一旦你开始在意：

- 响应性
- 吞吐
- 超时控制
- 输出捕获

就不要继续往 `xrtChain()` 上堆逻辑，而应该升级到：

- [XRT 子进程与工具执行入门](subprocess-intro.md)


## 8. Windows / Linux 跨平台时最该记住的事

- `xrtStart()` 在桌面环境里最自然，在纯服务环境里不一定有意义
- Windows 内建命令通常要通过 `cmd.exe /c ...` 包起来
- Linux / macOS 上，`xrtStart()` 依赖 `xdg-open`
- `xrtRun()` / `xrtChain()` 的返回语义跨平台有差异
	- Windows 更接近进程句柄 / ShellExecute 结果
	- Linux 更接近 PID 或退出状态
- 跨平台教学和业务主线里，先围绕“语义差别”理解，不要过度依赖某个平台特有返回值细节


## 9. 常见错误

### 错误一：把 `xrtStart()` 当成命令执行框架

`xrtStart()` 解决的是“交给系统默认程序打开”，不是“可靠执行一条受控命令”。

### 错误二：在服务主线程里直接跑 `xrtChain()`

它会阻塞。  
如果这条线程还承担 UI、事件循环或网络处理，响应性会直接掉下来。

### 错误三：以为 `xrtRun()` 能替代 `subprocess`

`xrtRun()` 只解决“发起启动后立刻返回”。  
一旦你要 stdout / stderr、stdin、future、超时、取消，应该直接换 `subprocess`。

### 错误四：把 shell 内建命令当成独立可执行文件

例如 Windows 上很多命令要通过 `cmd.exe /c` 执行。  
不要假设所有命令都能像独立 EXE 一样直接启动。

### 错误五：在无桌面环境里强依赖 `xrtStart()`

如果程序运行在 CI、服务端或纯终端环境，默认程序打开这条路可能根本不存在。


## 10. 建议继续阅读

- [OS API](../api/api-os.md)
- [时间、路径与文件系统入门](time-path-file-intro.md)
- [XRT 子进程与工具执行入门](subprocess-intro.md)
- [用 Subprocess + File Async 写一个工具链流水线](../case/subprocess-file-async-pipeline.md)

---

**一句话总结：** `os` 这组 API 真正要解决的不是“随便启动点什么”，而是把“默认程序打开”“同步执行等退出”“异步启动马上返回”三种完全不同的边界拆清楚。边界一旦清楚，你就知道什么时候够用，什么时候该马上升级到 `subprocess`。
