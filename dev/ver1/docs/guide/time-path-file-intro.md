# 时间、路径与文件系统入门：先把程序落到真实世界

> 目标：在碰 JSON、HTTP、模板和多任务之前，先把最基础的一层讲稳。读完这页后，你应该明确知道：什么时候该用 `xrtNow()`，什么时候该用 `xrtTimer()`，为什么路径不能靠字符串硬拼，以及为什么文件写入前要先把目录和编码边界处理清楚。

[返回教学文档](README.md)

---

## 1. 为什么这一页应该比配置、模板更早学

很多程序一开始看起来只是在做“业务逻辑”，但真正落地时总会碰到这些问题：

- 文件放在哪里
- 第一次运行时目录是否存在
- Windows 和 Linux 的路径分隔符怎么兼容
- 现在这个时间是拿来展示，还是拿来算耗时
- 文本文件到底按什么编码读写

如果这层没讲清楚，后面的配置系统、模板输出、HTTP 服务都会一起变脆。

所以这一页要先建立 4 个最基本的边界：

1. 日历时间和耗时统计不是一回事
2. 路径拼接和路径解析不是一回事
3. 文本文件和二进制文件不是一回事
4. 同步文件 I/O 和异步文件 I/O 不是一回事


## 2. 先把 4 组边界分清楚

### 2.1 `xrtNow()` 和 `xrtTimer()` 分别解决什么

它们最容易被混用。

- `xrtNow()`
	- 用来拿“当前日期时间”
	- 适合写日志时间、文件落盘时间、展示给人看的时间
- `xrtTimer()`
	- 用来量“这段代码跑了多久”
	- 适合性能统计、耗时打印、超时预算

更直接地说：

- 你想知道“现在是几点”，用 `xrtNow()`
- 你想知道“这段代码跑了多少秒”，用 `xrtTimer()`

`xrtSleep()` 也要顺手记住：它会阻塞当前线程。  
它适合演示和简单节流，不适合拿来伪装异步流程。


### 2.2 `xrtPathJoin()` 和手工拼路径有什么区别

很多初学者会直接这样写：

```c
"C:\\project\\build\\" + "report.txt"
```

或者：

```c
"/opt/project/" + "report.txt"
```

问题是这很快会碰到：

- 平台分隔符不一致
- 前后片段有没有多余 `/` 或 `\\`
- 根目录到底该相对谁

当前主线更稳的做法是：

- 用 `xCore->AppPath` 作为程序工作根
- 用 `xrtPathJoin()` 负责拼路径
- 用 `xrtPathGetDir()` 取父目录
- 用 `xrtPathExists()` / `xrtFileExists()` / `xrtDirExists()` 判断对象是否存在

目录扫描也遵循同样的原则：

- 只需要完整路径时，用 `xrtDirScan()`
- 同时需要父目录、文件名和完整路径时，用 `xrtDirScanEx()`
- 需要列出平台根目录时，把扫描路径传空字符串 `""`

`xrtDirScan((str)"", ...)` 和 `xrtDirScanEx((str)"", ...)` 会扫描“虚拟根目录”：Windows 下是盘符列表，其他平台是 `/`。这让文件选择器、目录树这类控件不用在业务层写一套平台分支。`NULL` 不是合法的目录扫描路径，不要用它表示根目录。


### 2.3 文本文件和二进制文件怎么分

如果你处理的是：

- 日志
- 配置
- JSON
- 模板输出
- 提示词

优先走文本路径：

- `xrtFileWriteAll()`
- `xrtFileReadAll()`

如果你处理的是：

- 图片
- 压缩包
- 自定义二进制结构

优先走二进制路径：

- `xrtFilePutAll()`
- `xrtFileGetAll()`

入门阶段先记一个最稳的规则：

- 文本文件明确写 `XRT_CP_UTF8`
- 读取未知来源的文本时，再考虑 `XRT_CP_AUTO`


### 2.4 这页为什么先只讲同步文件 I/O

同步文件 I/O 最适合先把边界讲清楚：

- 路径从哪里来
- 目录什么时候创建
- 文本怎么读写
- 失败时该怎么兜底

等这条线稳了，再继续升级到：

- [XRT 异步文件与目录操作入门](file-async-intro.md)
- [XRT 子进程与工具执行入门](subprocess-intro.md)


## 3. 最小 DEMO：写一份运行报告

第一步先不要碰配置、JSON 和模板。  
只做一件事：把一份带时间戳的运行报告写到 `build/guide-basic/run-report.txt`，然后再读回来确认。

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	xrtGlobalData* pCore = NULL;
	str sFilePath = NULL;
	str sDirPath = NULL;
	str sNow = NULL;
	str sReadBack = NULL;
	char sText[1024] = {0};
	size_t iReadSize = 0;
	int iRet = 1;

	pCore = xrtInit();
	if ( pCore == NULL ) {
		return 1;
	}

	sFilePath = xrtPathJoin(4, pCore->AppPath, "build", "guide-basic", "run-report.txt");
	if ( sFilePath == NULL ) {
		fprintf(stderr, "build file path failed\n");
		goto cleanup;
	}

	sDirPath = xrtPathGetDir(sFilePath, 0u);
	if ( (sDirPath == NULL) || (xrtDirCreateAll(sDirPath) != TRUE) ) {
		fprintf(stderr, "create parent dir failed\n");
		goto cleanup;
	}

	sNow = xrtTimeToStr(xrtNow(), XRT_TIME_FORMAT_DATETIME);
	if ( sNow == NULL ) {
		fprintf(stderr, "format current time failed\n");
		goto cleanup;
	}

	if ( snprintf(
		sText,
		sizeof(sText),
		"app path   : %s\r\n"
		"report at  : %s\r\n"
		"file path  : %s\r\n",
		pCore->AppPath,
		sNow,
		sFilePath
	) < 0 ) {
		fprintf(stderr, "format report text failed\n");
		goto cleanup;
	}

	if ( xrtFileWriteAll(sFilePath, sText, 0u, XRT_CP_UTF8) <= 0 ) {
		fprintf(stderr, "write report failed\n");
		goto cleanup;
	}

	sReadBack = xrtFileReadAll(sFilePath, XRT_CP_UTF8, &iReadSize);
	if ( sReadBack == NULL ) {
		fprintf(stderr, "read report failed\n");
		goto cleanup;
	}

	printf("report exists : %d\n", xrtPathExists(sFilePath));
	printf("report bytes  : %llu\n", (unsigned long long)iReadSize);
	printf("%s", sReadBack);

	iRet = 0;

cleanup:
	if ( sReadBack != NULL ) {
		xrtFree(sReadBack);
	}
	if ( sNow != NULL ) {
		xrtFree(sNow);
	}
	if ( sDirPath != NULL ) {
		xrtFree(sDirPath);
	}
	if ( sFilePath != NULL ) {
		xrtFree(sFilePath);
	}

	xrtUnit();
	return iRet;
}
```

这个最小例子真正要你记住的是下面 5 个动作：

1. 路径永远先从 `xCore->AppPath` 起步
2. 用 `xrtPathJoin()` 拼最终文件路径
3. 用 `xrtPathGetDir()` 取父目录
4. 写文件前先 `xrtDirCreateAll()`
5. 文本文件读写显式写 `XRT_CP_UTF8`


## 4. 升级 DEMO：生成一个带时间戳的快照并统计耗时

当你已经能稳定写单个文件后，下一步最常见的需求就是：

- 每次运行生成一个快照文件
- 同时保留一份 `latest.txt`
- 顺手统计这次输出花了多久

这时就该把：

- `xrtTimeFormat()`
- `xrtTimer()`
- `xrtFileCopy()`
- `xrtFileGetSize()`

接到同一条线上。

```c
#include "xrt.h"
#include <stdio.h>

static bool procWriteSnapshot(xrtGlobalData* pCore)
{
	str sRootDir = NULL;
	str sStamp = NULL;
	str sSnapshotPath = NULL;
	str sLatestPath = NULL;
	str sDirName = NULL;
	str sBaseName = NULL;
	str sExt = NULL;
	char sFileName[128] = {0};
	char sText[1024] = {0};
	xtime tNow = 0;
	double fStart = 0.0;
	double fEnd = 0.0;
	bool bOk = false;

	tNow = xrtNow();
	fStart = xrtTimer();

	sRootDir = xrtPathJoin(3, pCore->AppPath, "build", "snapshot-demo");
	if ( (sRootDir == NULL) || (xrtDirCreateAll(sRootDir) != TRUE) ) {
		goto cleanup;
	}

	sStamp = xrtTimeFormat(tNow, "yyyymmdd_hhnnss");
	if ( sStamp == NULL ) {
		goto cleanup;
	}

	if ( snprintf(sFileName, sizeof(sFileName), "snapshot_%s.txt", sStamp) < 0 ) {
		goto cleanup;
	}

	sSnapshotPath = xrtPathJoin(2, sRootDir, sFileName);
	sLatestPath = xrtPathJoin(2, sRootDir, "latest.txt");
	if ( (sSnapshotPath == NULL) || (sLatestPath == NULL) ) {
		goto cleanup;
	}

	if ( snprintf(
		sText,
		sizeof(sText),
		"snapshot time : %s\r\n"
		"root dir      : %s\r\n"
		"snapshot path : %s\r\n",
		sStamp,
		sRootDir,
		sSnapshotPath
	) < 0 ) {
		goto cleanup;
	}

	if ( xrtFileWriteAll(sSnapshotPath, sText, 0u, XRT_CP_UTF8) <= 0 ) {
		goto cleanup;
	}
	if ( !xrtFileCopy(sSnapshotPath, sLatestPath, TRUE) ) {
		goto cleanup;
	}

	fEnd = xrtTimer();
	sDirName = xrtPathGetDir(sSnapshotPath, 0u);
	sBaseName = xrtPathGetNameExt(sSnapshotPath, 0u);
	sExt = xrtPathGetExt(sSnapshotPath, 0u);

	printf("snapshot ok   : %d\n", xrtFileExists(sSnapshotPath));
	printf("latest ok     : %d\n", xrtFileExists(sLatestPath));
	printf("snapshot size : %llu\n", (unsigned long long)xrtFileGetSize(sSnapshotPath));
	printf("dir name      : %s\n", sDirName ? sDirName : "");
	printf("base name     : %s\n", sBaseName ? sBaseName : "");
	printf("file ext      : %s\n", sExt ? sExt : "");
	printf("elapsed       : %.6f sec\n", fEnd - fStart);

	bOk = true;

cleanup:
	if ( sExt != NULL ) {
		xrtFree(sExt);
	}
	if ( sBaseName != NULL ) {
		xrtFree(sBaseName);
	}
	if ( sDirName != NULL ) {
		xrtFree(sDirName);
	}
	if ( sLatestPath != NULL ) {
		xrtFree(sLatestPath);
	}
	if ( sSnapshotPath != NULL ) {
		xrtFree(sSnapshotPath);
	}
	if ( sStamp != NULL ) {
		xrtFree(sStamp);
	}
	if ( sRootDir != NULL ) {
		xrtFree(sRootDir);
	}
	return bOk;
}


int main(void)
{
	xrtGlobalData* pCore = NULL;
	int iRet = 1;

	pCore = xrtInit();
	if ( pCore == NULL ) {
		return 1;
	}

	if ( !procWriteSnapshot(pCore) ) {
		fprintf(stderr, "write snapshot failed\n");
		goto cleanup;
	}

	iRet = 0;

cleanup:
	xrtUnit();
	return iRet;
}
```

这个升级版比最小 DEMO 多教了 4 件事：

- 文件名可以用 `xrtTimeFormat()` 做稳定时间戳
- 同一份结果可以顺手复制出 `latest.txt`
- `xrtFileGetSize()` 可以做最直接的落盘验证
- `xrtTimer()` 应该只拿来测耗时，不拿来展示“当前时间”


## 5. 什么时候该从这页继续升级

如果你当前需求是下面这些，就继续用这页的同步主线：

- 生成日志
- 写配置文件
- 落一份模板输出
- 做一个简单 CLI 工具

如果你开始遇到下面这些问题，就应该升级：

- 主线程不能被文件 I/O 卡住
	- 继续读 [XRT 异步文件与目录操作入门](file-async-intro.md)
- 需要启动子程序并收集输出
	- 继续读 [XRT 子进程与工具执行入门](subprocess-intro.md)
- 需要把文件系统结果再接到结构化数据
	- 继续读 [xvalue 与 JSON 入门](xvalue-json-intro.md)
- 需要做一个完整配置系统
	- 继续读 [用 `file + path + value + json` 构造配置系统](../case/json-config-system.md)


## 6. 常见错误

### 错误一：拿 `xrtNow()` 去算耗时

`xrtNow()` 是日历时间。  
算耗时应使用 `xrtTimer()`。

### 错误二：写文件前不创建父目录

`xrtFileWriteAll()` 不会替你神奇补齐所有父目录。  
更稳的顺序是先 `xrtPathGetDir()`，再 `xrtDirCreateAll()`。

### 错误三：路径直接写死成 `C:\\...` 或 `/opt/...`

教学里可以举这种例子，实际程序里不该这么落地。  
真实项目优先从 `xCore->AppPath` 出发，再用 `xrtPathJoin()` 拼。

### 错误四：未知文本也强行按固定编码读取

如果文件来源不稳定，优先 `XRT_CP_AUTO`。  
如果是你自己生成的输出，优先明确写成 `XRT_CP_UTF8`。

### 错误五：把 `xrtSleep()` 当成“异步等待”

它只是阻塞当前线程。  
一旦你开始在意吞吐和并发，就该切到 `future / file_async / queue` 那条主线。


## 7. Windows / Linux 跨平台时最该记住的事

- 路径根优先用 `xCore->AppPath`，不要假设程序一定跑在某个固定目录
- 路径拼接优先用 `xrtPathJoin()`，不要手工赌分隔符
- 文本文件默认统一落成 `UTF-8`
- 耗时统计统一用 `xrtTimer()`
- 展示给人的时间统一用 `xrtNow()`、`xrtTimeToStr()`、`xrtTimeFormat()`


## 8. 建议继续阅读

- [Path API](../api/api-path.md)
- [File API](../api/api-file.md)
- [Time API](../api/api-time.md)
- [Charset 入门：程序里的字符串、文件编码和平台宽字符不是一回事](charset-intro.md)
- [xvalue 与 JSON 入门](xvalue-json-intro.md)
- [用 `file + path + value + json` 构造配置系统](../case/json-config-system.md)
- [XRT 异步文件与目录操作入门](file-async-intro.md)

---

**一句话总结：** 真正稳定的程序，不是“终于写出一个文件”，而是“把时间、路径、目录、编码和文件生命周期分成清楚的几层”。这层一旦立住，后面的配置、模板、HTTP 和工具链都会顺很多。
