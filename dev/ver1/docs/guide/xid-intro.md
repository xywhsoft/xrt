# XID 入门：什么时候该直接生成唯一 ID，什么时候还不够

> 目标：把 `xrtMakeXID()`、`xrtMakeXIDS()`、`xrtEncodeXID()`、`xrtDecodeXID()`、`xrtCompXID()` 讲成一条“无中心协调唯一 ID”主线。读完这页后，你应该明确知道：什么时候默认直接用字符串版 XID，什么时候需要保留结构体版，为什么它比“时间戳 + 随机数”更稳，以及它又不应该被拿来替代什么。

[返回教学文档](README.md)

---

## 1. 为什么 XID 值得单独讲

很多程序都会很快碰到同一种需求：

- 给订单、任务、工单、批次生成唯一 ID
- 给日志链路、请求上下文、trace/span 生成追踪 ID
- 给临时文件、导出任务、缓存工件生成不冲突名字
- 希望多个节点都能自己生成 ID，而不是每次都去问中心服务

如果这件事处理得太草率，通常会掉进几种老路：

- 只拿当前时间戳
- 只拼一段随机数
- 只在数据库里等自增主键
- 先生成一个字符串，后面又完全不知道它来自哪里

XRT 里的 XID，就是专门用来解决这类“我要一个可直接使用、又不必中心协调的唯一 ID”的。


## 2. 先把 5 个边界分清楚

### 2.1 XID 解决的是“唯一标识”，不是“安全令牌”

XID 很适合：

- 业务 ID
- trace ID
- 工件名
- 文件名
- 队列任务 ID

但它不适合直接承担：

- 登录令牌
- 重置密码 token
- 防伪签名
- 需要保密的随机秘密

原因很简单：XID 的设计目标是“稳定地产生唯一标识”，不是“提供不可推断的安全秘密”。


### 2.2 默认先想 `xrtMakeXIDS()`，不是默认先想 `xrtMakeXID()`

这两个 API 解决的是两层不同需求：

- `xrtMakeXIDS()`
	- 直接生成字符串
	- 最适合业务层、日志、JSON、数据库、URL、文件名
	- 大多数时候它就是默认选项
- `xrtMakeXID()`
	- 生成结构体对象
	- 适合你要检查 `Time / Addr / Tick / Rand`
	- 适合做结构级比较、调试、诊断

更直接地说：

- 你只是想“拿一个能用的唯一 ID”，先用 `xrtMakeXIDS()`
- 你想“看清 ID 里装了什么”，再用 `xrtMakeXID()`


### 2.3 `xrtEncodeXID()` / `xrtDecodeXID()` 是“对象层”和“交换层”的桥

它们适合做两件事：

- 你手里有 `xid` 结构体，准备写进日志、JSON、HTTP、文件
- 你手里拿到字符串，想再还原回结构体做检查或比对

所以可以这样理解：

- `xrtMakeXID()` / `xrtMakeXIDS()` 负责生成
- `xrtEncodeXID()` / `xrtDecodeXID()` 负责在“结构体”和“字符串”之间来回转换


### 2.4 `xrtCompXID()` 比“自己拆字段比”更稳

当你已经拿到两个 `xid` 对象时，比较是否相同最直接的方式就是：

- `xrtCompXID(pXID1, pXID2)`

这样你就不需要自己再去写：

- `Time`
- `Addr`
- `Tick`
- `Rand`

四个字段的逐项对比逻辑。


### 2.5 XID 里的 `Addr` 来自运行时本机地址，不等于公网身份

XID 结构里有：

- `Time`
- `Addr`
- `Tick`
- `Rand`

其中 `Addr` 本质上是运行时本机地址字段。  
这让 XID 很适合承担“带一点机器上下文的唯一 ID”，但不要把它误解成：

- 公网出口 IP
- 绝对稳定的机器身份
- 自动网卡选择结果

如果你还没把这条边界看清，建议先读：

- [本机网络信息入门：什么时候该拿主机名、IP、MAC，什么时候不该](local-network-intro.md)


## 3. 最小 DEMO：先把 XID 当成业务字符串 ID 用起来

第一步不要先去拆字段。  
先把最常见、也最实用的路线走通：

- 直接生成一个字符串版 XID
- 把它写进任务报告
- 让它立刻变成日志 / 文件 / JSON 都能复用的业务 ID

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	xrtGlobalData* pCore = NULL;
	str sTaskId = NULL;
	str sOutDir = NULL;
	str sOutPath = NULL;
	str sNow = NULL;
	char sReport[1024] = {0};
	int iRet = 1;

	pCore = xrtInit();
	if ( pCore == NULL ) {
		return 1;
	}

	sTaskId = xrtMakeXIDS();
	if ( (sTaskId == NULL) || (sTaskId[0] == 0) ) {
		fprintf(stderr, "make xid failed\n");
		goto cleanup;
	}

	sOutDir = xrtPathJoin(3, pCore->AppPath, "build", "xid-demo");
	sOutPath = xrtPathJoin(4, pCore->AppPath, "build", "xid-demo", "task-report.txt");
	if ( (sOutDir == NULL) || (sOutPath == NULL) ) {
		fprintf(stderr, "build output path failed\n");
		goto cleanup;
	}

	if ( xrtDirCreateAll(sOutDir) != TRUE ) {
		fprintf(stderr, "create output dir failed\n");
		goto cleanup;
	}

	sNow = xrtTimeToStr(xrtNow(), XRT_TIME_FORMAT_DATETIME);
	if ( snprintf(
		sReport,
		sizeof(sReport),
		"time    : %s\r\n"
		"task id : %s\r\n",
		(sNow && sNow[0]) ? sNow : "",
		sTaskId
	) < 0 ) {
		fprintf(stderr, "format report failed\n");
		goto cleanup;
	}

	if ( xrtFileWriteAll(sOutPath, sReport, 0u, XRT_CP_UTF8) <= 0 ) {
		fprintf(stderr, "write report failed\n");
		goto cleanup;
	}

	printf("task id  : %s\n", sTaskId);
	printf("report   : %s\n", sOutPath);
	iRet = 0;

cleanup:
	if ( sNow != NULL ) {
		xrtFree(sNow);
	}
	if ( sOutPath != NULL ) {
		xrtFree(sOutPath);
	}
	if ( sOutDir != NULL ) {
		xrtFree(sOutDir);
	}
	if ( sTaskId != NULL ) {
		xrtFree(sTaskId);
	}
	xrtUnit();
	return iRet;
}
```

这个最小例子最想让你先记住的是：

1. 大多数业务代码并不需要先碰 `xid` 结构体
2. `xrtMakeXIDS()` 往往就是默认主线
3. 一个 XID 字符串可以直接进日志、文件、JSON、URL、数据库
4. 这一步已经能覆盖“任务号、追踪号、工件号”这类最常见需求


## 4. 升级 DEMO：把 XID 解码回结构体，看清它到底带了什么

当你已经会直接用字符串版 XID 以后，下一步才值得做：

- 看 `Time` 是什么时候
- 看 `Addr` 和当前运行时缓存 IP 是否对齐
- 看 `Tick / Rand` 只是辅助去重与分散碰撞，不要把它们想成业务字段
- 验证 `encode -> decode -> compare` 这条链是稳定的

```c
#include "xrt.h"
#include <stdio.h>

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
	xid pXID = NULL;
	xid pDecoded = NULL;
	str sEncoded = NULL;
	str sCreatedAt = NULL;
	str sOutDir = NULL;
	str sOutPath = NULL;
	char sReport[2048] = {0};
	int iRet = 1;

	pCore = xrtInit();
	if ( pCore == NULL ) {
		return 1;
	}

	pXID = xrtMakeXID();
	if ( pXID == NULL ) {
		fprintf(stderr, "make xid failed\n");
		goto cleanup;
	}

	sEncoded = xrtEncodeXID(pXID);
	if ( sEncoded == NULL ) {
		fprintf(stderr, "encode xid failed\n");
		goto cleanup;
	}

	pDecoded = xrtDecodeXID(sEncoded);
	if ( pDecoded == NULL ) {
		fprintf(stderr, "decode xid failed\n");
		goto cleanup;
	}

	sCreatedAt = xrtTimeToStr(pDecoded->Time, XRT_TIME_FORMAT_DATETIME);
	sOutDir = xrtPathJoin(3, pCore->AppPath, "build", "xid-demo");
	sOutPath = xrtPathJoin(4, pCore->AppPath, "build", "xid-demo", "xid-inspect.txt");
	if ( (sOutDir == NULL) || (sOutPath == NULL) ) {
		fprintf(stderr, "build output path failed\n");
		goto cleanup;
	}

	if ( xrtDirCreateAll(sOutDir) != TRUE ) {
		fprintf(stderr, "create output dir failed\n");
		goto cleanup;
	}

	if ( snprintf(
		sReport,
		sizeof(sReport),
		"xid         : %s\r\n"
		"time        : %s\r\n"
		"addr(raw)   : 0x%08X\r\n"
		"cached ip   : 0x%08X\r\n"
		"tick        : %d\r\n"
		"rand        : %lld\r\n"
		"same object : %s\r\n",
		sEncoded,
		(sCreatedAt && sCreatedAt[0]) ? sCreatedAt : "",
		(uint32)pDecoded->Addr,
		(uint32)pCore->LocalAddr,
		pDecoded->Tick,
		(long long)pDecoded->Rand,
		xrtCompXID(pXID, pDecoded) ? "yes" : "no"
	) < 0 ) {
		fprintf(stderr, "format report failed\n");
		goto cleanup;
	}

	printf("xid string  : %s\n", sEncoded);
	printf("created at  : %s\n", (sCreatedAt && sCreatedAt[0]) ? sCreatedAt : "");
	printf("addr        : 0x%08X = ", (uint32)pDecoded->Addr);
	procPrintIPv4((uint32)pDecoded->Addr);
	printf("\n");
	printf("cached ip   : 0x%08X = ", (uint32)pCore->LocalAddr);
	procPrintIPv4((uint32)pCore->LocalAddr);
	printf("\n");
	printf("same object : %s\n", xrtCompXID(pXID, pDecoded) ? "yes" : "no");

	if ( xrtFileWriteAll(sOutPath, sReport, 0u, XRT_CP_UTF8) <= 0 ) {
		fprintf(stderr, "write report failed\n");
		goto cleanup;
	}

	printf("report      : %s\n", sOutPath);
	iRet = 0;

cleanup:
	if ( sOutPath != NULL ) {
		xrtFree(sOutPath);
	}
	if ( sOutDir != NULL ) {
		xrtFree(sOutDir);
	}
	if ( sCreatedAt != NULL ) {
		xrtFree(sCreatedAt);
	}
	if ( pDecoded != NULL ) {
		xrtFree(pDecoded);
	}
	if ( sEncoded != NULL ) {
		xrtFree(sEncoded);
	}
	if ( pXID != NULL ) {
		xrtFree(pXID);
	}
	xrtUnit();
	return iRet;
}
```

这个升级例子最关键的教学点有 6 个：

1. `xrtMakeXID()` 适合调试、诊断和字段级检查
2. `xrtEncodeXID()` 让结构体回到交换层字符串
3. `xrtDecodeXID()` 让字符串重新进入结构体层
4. `xrtCompXID()` 可以直接验证 round-trip 是否保持一致
5. `Addr` 通常应和运行时缓存的 `pCore->LocalAddr` 对齐
6. `Time / Addr / Tick / Rand` 是生成机制的一部分，不是你业务对象的字段模型


## 5. 它和“时间戳 / 自增 ID / 纯随机串”到底有什么区别

### 5.1 只用时间戳，太容易撞

如果你只拿：

- 秒级时间戳
- 毫秒时间戳

在同一时刻高频创建多个对象时，很容易碰撞。


### 5.2 只用数据库自增，扩展性会卡在中心协调

自增主键在单库里很好用，但它天然依赖：

- 同一个数据库
- 同一个中心分配点

一旦你想离线生成、边缘节点生成、多服务并行生成，它就不再是最轻的答案。


### 5.3 只拼随机串，通常缺少“可解释性”

纯随机串当然也能做唯一 ID，但它往往没有：

- 生成时间上下文
- 本机地址上下文
- 结构级可检查性

而 XID 的好处是：

- 你平时可以把它当普通字符串用
- 真出问题时，又能 decode 回去做诊断


### 5.4 XID 的重点是“本地生成 + 足够稳 + 可回看”

这也是它最适合承担：

- 业务 ID
- 追踪 ID
- 工件 ID
- 导出任务 ID

这类场景的原因。


## 6. 如果只有一条主线程，问题会卡在哪里

XID 这条线和“多任务”不一样。  
它主要解决的，不是主线程阻塞，而是“唯一 ID 还要不要依赖中心协调”。

如果没有 XID，一条主线程常见会卡在这些地方：

- 每次都要先请求数据库拿自增号
- 每次都要先请求中心服务分配 ID
- 每次都得自己重新拼时间戳和随机串规则

所以这里真正省掉的成本是：

- 协调成本
- 设计分裂成本
- 多节点下的 ID 分配耦合

而不是“把一个阻塞 I/O 变成异步等待”。


## 7. Windows / Linux 跨平台时最该记住的事

- `Time` 是时间戳，适合做大致创建时间追踪
- `Tick` 的底层来源会随平台不同，但你通常不需要自己解释它
- `Addr` 取自运行时本机地址，可能受当前网络环境影响
- 如果本机地址不可用，`Addr` 可能不会像你期待的那样有辨识度
- 字符串版 XID 更适合跨模块、跨进程、跨文件格式流动


## 8. 常见错误

### 错误一：把 XID 当成安全口令

它适合做唯一标识，不适合直接承担保密令牌。

### 错误二：默认先用 `xrtMakeXID()`，然后到处手工再编码

如果你最终只是想拿一个字符串，大多数时候应该直接 `xrtMakeXIDS()`。

### 错误三：把 `Addr` 当成公网地址或绝对机器身份

它只是运行时本机地址上下文，不应该被过度解读。

### 错误四：假设不同机器上的 XID 能当成严格时间序列

它有时间上下文，但这不等于“跨机器严格全局时钟排序”。

### 错误五：忘记释放返回对象

`xrtMakeXID()`、`xrtMakeXIDS()`、`xrtEncodeXID()`、`xrtDecodeXID()` 返回的对象都应按接口说明释放。


## 9. 建议继续阅读

- [本机网络信息入门：什么时候该拿主机名、IP、MAC，什么时候不该](local-network-intro.md)
- [Math 与 Hash 入门：随机数、约等于和稳定指纹不是一回事](math-hash-intro.md)
- [XID API](../api/api-xid.md)
- [Network API](../api/api-network.md)
- [用 `file + path + value + json` 构造配置系统](../case/json-config-system.md)
- [多任务总论：线程、队列、协程与 Future 怎么选](multitask-overview.md)

---

**一句话总结：** XID 最适合承担“让我在任何节点上都能立刻拿到一个够稳、够通用、出问题时还能回看结构的唯一 ID”，但它不应该被误用成安全秘密、公网身份或严格全局时钟。 
