# 本机网络信息入门：什么时候该拿主机名、IP、MAC，什么时候不该

> 目标：把 `xrtGetLocalName()`、`xrtGetLocalIP()`、`xrtGetLocalRawIP()`、`xrtGetLocalMAC()` 讲成一组“本机环境信息”能力，而不是泛泛的网络编程 API。读完这页后，你应该明确知道：什么时候它们适合做启动诊断、节点标识和日志标签，什么时候它们不适合拿来做公网发现、自动选网卡或 socket 绑定。

[返回教学文档](README.md)

---

## 1. 为什么这组 API 值得单独讲

很多程序在真正落地时都会碰到这些需求：

- 启动时打印“我是谁、我跑在哪台机器上”
- 生成一个带机器标识的日志前缀
- 记录主机名、IP、MAC，方便排查环境问题
- 给 XID、节点报告、设备信息页补一层本机上下文

这些需求看起来都和“网络”有关，但它们并不是：

- 建立 socket 连接
- 监听端口
- 收发 HTTP / WebSocket

它们只是更轻的一层：

- 读取本机环境里的网络身份信息

所以这一页的重点，不是网络收发，而是“本机身份信息应该怎么理解、怎么用、怎么避免误用”。


## 2. 先把 4 个边界分清楚

### 2.1 这组 API 解决的是“本机信息”，不是“联网能力”

`xrtGetLocalName()`、`xrtGetLocalIP()`、`xrtGetLocalMAC()` 解决的是：

- 这台机器对自己叫什么
- 当前拿到的本机 IPv4 地址是什么
- 当前拿到的 MAC 地址是什么

它们不解决的是：

- 公网出口 IP 是什么
- 该绑定哪个网卡
- 哪个地址一定能被外部访问
- xnet-v2 的连接、监听、TLS 和等待模型

所以如果你已经在想：

- bind 地址
- listener
- stream
- dgram

那条线应该去看 `xnet-v2`，不是继续在这里兜圈子。


### 2.2 `xrtGetLocalIP()` 和 `xrtGetLocalRawIP()` 不是重复函数

它们分别服务两种不同场景：

- `xrtGetLocalIP()`
	- 适合展示给人看
	- 适合日志、报表、诊断页
- `xrtGetLocalRawIP()`
	- 适合程序内部做机器标识、缓存、结构字段
	- 适合和 `xCore->LocalAddr`、XID 这类运行时字段对齐

更直接地说：

- 你要打印，就先想字符串 IP
- 你要存字段、做对比、和运行时缓存对齐，就先想原始 `uint32`


### 2.3 `xrtGetLocalMAC()` 更像“设备标签”，不是稳定身份主键

MAC 地址很适合：

- 诊断页
- 设备清单
- 环境排查

但不要轻易把它理解成“永远稳定、永远存在的全局机器主键”。

因为现实环境里经常会遇到：

- 虚拟机
- 容器
- 多网卡
- 云主机
- 随机化 MAC

所以更稳的理解是：

- MAC 适合做辅助诊断标签
- 不适合单独承担整个分布式身份体系


### 2.4 这组字符串 API 的释放规则要比普通字符串更小心

当前主线里，这组函数失败时可能返回运行时空字符串哨兵。  
所以如果你要和当前实现完全对齐，释放时最好参考测试代码的写法：

- 成功拿到真实分配字符串时 `xrtFree()`
- 返回运行时空字符串哨兵时不要重复释放

这一页后面的示例会统一用：

- `pCore->sNull`

做保护释放。


## 3. 最小 DEMO：先把主机名、字符串 IP、原始 IP、MAC 打出来

第一步先不要碰 XID，也不要先写文件。  
只做一件事：把本机网络信息稳定地拿出来，并按当前实现正确释放。

```c
#include "xrt.h"
#include <stdio.h>

static void procFreeRuntimeStr(xrtGlobalData* pCore, str sText)
{
	if ( (sText != NULL) && (sText != pCore->sNull) ) {
		xrtFree(sText);
	}
}


static void procPrintIPv4(uint32 u32IP)
{
	printf(
		"%u.%u.%u.%u",
		(u8)(u32IP >> 24),
		(u8)(u32IP >> 16),
		(u8)(u32IP >> 8),
		(u8)u32IP
	);
}


int main(void)
{
	xrtGlobalData* pCore = NULL;
	str sName = NULL;
	str sIP = NULL;
	str sMAC = NULL;
	uint32 u32IP = 0;
	int iRet = 1;

	pCore = xrtInit();
	if ( pCore == NULL ) {
		return 1;
	}

	sName = xrtGetLocalName();
	sIP = xrtGetLocalIP();
	sMAC = xrtGetLocalMAC();
	u32IP = xrtGetLocalRawIP();

	printf("host name  : %s\n", (sName && sName[0]) ? sName : "(empty)");
	printf("string ip  : %s\n", (sIP && sIP[0]) ? sIP : "(empty)");
	printf("raw ip     : 0x%08X = ", u32IP);
	procPrintIPv4(u32IP);
	printf("\n");
	printf("runtime ip : 0x%08X = ", pCore->LocalAddr);
	procPrintIPv4(pCore->LocalAddr);
	printf("\n");
	printf("mac        : %s\n", (sMAC && sMAC[0]) ? sMAC : "(empty)");

	iRet = 0;

	procFreeRuntimeStr(pCore, sMAC);
	procFreeRuntimeStr(pCore, sIP);
	procFreeRuntimeStr(pCore, sName);
	xrtUnit();
	return iRet;
}
```

这个最小例子最想让你先记住的是：

1. 字符串 IP 和原始 IP 是两层不同表达
2. `xCore->LocalAddr` 是运行时缓存的原始本机 IP
3. 这组信息最适合在启动阶段、诊断阶段拿一次
4. 释放时最好和运行时空字符串哨兵保持一致


## 4. 升级 DEMO：生成一份节点报告，并顺手接上 XID

当你已经能稳定拿到本机信息后，下一步最常见的真实需求就是：

- 启动时生成一份节点报告
- 把主机信息写到日志或工件里
- 给当前进程补一个带机器标识的 XID

这时就可以把：

- `xrtGetLocalName()`
- `xrtGetLocalIP()`
- `xrtGetLocalMAC()`
- `xrtGetLocalRawIP()`
- `xrtMakeXIDS()`
- `time / path / file`

接成同一条主线。

```c
#include "xrt.h"
#include <stdio.h>

static void procFreeRuntimeStr(xrtGlobalData* pCore, str sText)
{
	if ( (sText != NULL) && (sText != pCore->sNull) ) {
		xrtFree(sText);
	}
}


int main(void)
{
	xrtGlobalData* pCore = NULL;
	str sRootDir = NULL;
	str sReportPath = NULL;
	str sNow = NULL;
	str sName = NULL;
	str sIP = NULL;
	str sMAC = NULL;
	str sXID = NULL;
	uint32 u32IP = 0;
	char sReport[2048] = {0};
	int iRet = 1;

	pCore = xrtInit();
	if ( pCore == NULL ) {
		return 1;
	}

	sRootDir = xrtPathJoin(3, pCore->AppPath, "build", "local-network-demo");
	sReportPath = xrtPathJoin(4, pCore->AppPath, "build", "local-network-demo", "node-report.txt");
	if ( (sRootDir == NULL) || (sReportPath == NULL) ) {
		fprintf(stderr, "build report path failed\n");
		goto cleanup;
	}

	if ( xrtDirCreateAll(sRootDir) != TRUE ) {
		fprintf(stderr, "create node report dir failed\n");
		goto cleanup;
	}

	sNow = xrtTimeToStr(xrtNow(), XRT_TIME_FORMAT_DATETIME);
	sName = xrtGetLocalName();
	sIP = xrtGetLocalIP();
	sMAC = xrtGetLocalMAC();
	sXID = xrtMakeXIDS();
	u32IP = xrtGetLocalRawIP();

	if ( snprintf(
		sReport,
		sizeof(sReport),
		"time        : %s\r\n"
		"host name   : %s\r\n"
		"string ip   : %s\r\n"
		"raw ip      : 0x%08X\r\n"
		"cached ip   : 0x%08X\r\n"
		"mac         : %s\r\n"
		"xid         : %s\r\n",
		(sNow && sNow[0]) ? sNow : "",
		(sName && sName[0]) ? sName : "",
		(sIP && sIP[0]) ? sIP : "",
		u32IP,
		pCore->LocalAddr,
		(sMAC && sMAC[0]) ? sMAC : "",
		(sXID && sXID[0]) ? sXID : ""
	) < 0 ) {
		fprintf(stderr, "format node report failed\n");
		goto cleanup;
	}

	if ( xrtFileWriteAll(sReportPath, sReport, 0u, XRT_CP_UTF8) <= 0 ) {
		fprintf(stderr, "write node report failed\n");
		goto cleanup;
	}

	printf("node report : %s\n", sReportPath);
	printf("%s", sReport);
	iRet = 0;

cleanup:
	procFreeRuntimeStr(pCore, sXID);
	procFreeRuntimeStr(pCore, sMAC);
	procFreeRuntimeStr(pCore, sIP);
	procFreeRuntimeStr(pCore, sName);
	procFreeRuntimeStr(pCore, sNow);
	if ( sReportPath != NULL ) {
		xrtFree(sReportPath);
	}
	if ( sRootDir != NULL ) {
		xrtFree(sRootDir);
	}
	xrtUnit();
	return iRet;
}
```

这个升级版最关键的教学点有 5 个：

1. 本机信息很适合做启动报告和诊断工件
2. `xrtGetLocalRawIP()` 和 `xCore->LocalAddr` 应该能对齐
3. `xrtMakeXIDS()` 可以自然接到这条“机器上下文”主线
4. 字符串信息适合展示，原始 IP 更适合程序字段
5. 这条能力很适合“启动时拿一次并缓存”，不适合在热路径里反复探测


## 5. 什么时候它适合，什么时候它不适合

这组 API 很适合：

- 启动日志
- 节点报告
- 设备信息页
- XID / 机器上下文说明
- 环境自检

这组 API 不适合：

- 发现公网 IP
- 自动挑选最优网卡
- 直接决定服务监听地址
- 替代 `xnet-v2` 的地址与连接模型


## 6. 如果只有一条主线程，会卡在哪里

这条线不像 `xrtChain()` 那样明显阻塞很久，但它也不是“零成本”。

它背后仍然会做：

- 主机名查询
- 本机地址查询
- 网卡信息查询

所以更稳的做法是：

- 启动时或配置加载时拿一次
- 热路径里优先复用缓存
- 需要机器地址字段时，优先复用 `xCore->LocalAddr`

不要把：

- `xrtGetLocalIP()`
- `xrtGetLocalMAC()`

这类查询放进“每个请求都跑一次”的路径里。


## 7. Windows / Linux 跨平台时最该记住的事

- 多网卡环境下，返回的并不一定是你“最想要的那个”地址
- 可能拿到回环地址、局域网地址或当前第一可见网卡地址
- `xrtGetLocalMAC()` 在虚拟机、容器、云环境里未必稳定
- `xrtGetLocalName()` 更像主机标签，不等于服务绑定名
- 这组 API 当前主线更偏 IPv4 便利接口，不等于完整地址抽象


## 8. 常见错误

### 错误一：把 `xrtGetLocalIP()` 当成公网 IP

它拿的是本机环境里的地址，不是公网出口探测结果。

### 错误二：多网卡环境下直接拿它做监听绑定

“当前取到的本机地址”不等于“你应该绑定的服务地址”。  
绑定策略应该放在网络配置和 `xnet-v2` 那条主线上处理。

### 错误三：把 MAC 当成永久稳定的机器主键

它更适合辅助诊断，而不是整个身份体系唯一依赖。

### 错误四：忽略运行时空字符串哨兵的释放语义

如果你想和当前实现完全对齐，释放时要像测试代码一样考虑 `pCore->sNull`。

### 错误五：在热路径里反复探测本机信息

这组函数更适合启动、诊断和缓存，不适合每次请求都重新查询。


## 9. 建议继续阅读

- [XID 入门：什么时候该直接生成唯一 ID，什么时候还不够](xid-intro.md)
- [Network API](../api/api-network.md)
- [XID API](../api/api-xid.md)
- [时间、路径与文件系统入门](time-path-file-intro.md)
- [OS 入门：什么时候只要启动一个程序，什么时候已经该上 subprocess](os-intro.md)
- [XNet v2 网络主线](../api/api-xnet-v2.md)

---

**一句话总结：** 本机网络信息这组 API 最适合承担“启动时说明我是谁、我在哪、这台机器长什么样”的角色，而不是承担“公网发现、自动选网卡、完整网络编程”的角色。把边界分清楚，这组接口会非常顺手。
