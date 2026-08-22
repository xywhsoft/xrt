# XRT Wait-Source 入门：把 Future 和网络等待说成同一种语言

> 目标：讲清 `wait-source` 为什么存在、它和 `future` 有什么区别、它如何把线程等待与协程等待统一起来，以及它为什么是 XRT 当前异步主线里的“统一等待桥”。

[返回教学文档](README.md) | [返回文档中心](../README.md)

---

## 0. 建议先读什么

如果你还没有建立 XRT 多任务整体心智模型，建议先读：

- [多任务总论：线程、队列、协程与 Future 怎么选](multitask-overview.md)
- [协程、Future 与 Task 入门](coroutine-future-task-intro.md)

本文只聚焦一件事：

- 为什么“等待什么”也需要成为一个正式对象
- `wait-source` 和 `future` 的边界在哪里
- 怎样从最简单的 `future` 等待，一步走到网络对象等待


## 1. 为什么需要 `wait-source`

很多程序一开始只有一种等待对象：

- 等线程结束
- 等 future 完成
- 等 socket 可读

这时你很容易觉得：

> 直接调各自的等待函数就够了。

问题会在程序开始同时等待多种对象时冒出来。

例如：

1. 一部分逻辑在等 `future`
2. 另一部分逻辑在等 `stream readable`
3. 还有一部分逻辑在等 `listener accept`
4. 协程层又想把这些等待写成同一种顺序代码

如果每一类对象都保留完全不同的等待协议，代码很快会出现这些问题：

- 调用点被具体对象类型绑死
- 超时、截止时间、协程等待会被拆成几套语义
- 上层流程要不停判断“这次等的是 future 还是网络对象”
- API 越写越像“同一件事重复实现很多遍”

这时你真正缺的，不是新线程，也不是更多回调，而是：

> 一个正式的“统一等待入口”。


## 2. 单主线程和零散等待为什么会卡住

### 2.1 单主线程的局限

一条主线程顺序执行时，等待通常像这样：

1. 发起一个异步动作
2. 专门等这个动作
3. 等完再继续下一步

只要同时要等的对象只有一种，这还算简单。


### 2.2 多种等待对象混在一起的问题

一旦程序同时存在这些对象：

- `future`
- `stream`
- `dgram recv`
- `listener accept`

如果没有统一等待对象，上层代码就得不停分叉：

- 这个分支用 `xFutureWait*`
- 那个分支用 `xrtNetStreamWait*`
- 另一个分支用 `xrtNetListenerAccept*`

这样会导致：

- 你的“业务流程”里混进大量“底层对象种类判断”
- 线程侧和协程侧都要各记一套入口
- 后续越写越难把等待逻辑收口


## 3. `future`、`wait-source`、`task group` 各自负责什么

这 3 个概念很容易混在一起，先拆开。

| 对象 | 它回答的问题 | 你真正得到的能力 |
|------|--------------|------------------|
| `future` | 结果是什么 | 状态、值、错误、取消、continuation |
| `wait-source` | 现在等谁 | 统一等待入口、统一超时/截止时间、统一协程等待 |
| `task group` | 这一批任务怎么收口 | scope、join、close、cancel、父子传播 |

初学时可以记成一句话：

> `future` 负责结果，`wait-source` 负责等待对象统一，`task group` 负责一批任务的收口。

所以：

- `future` 不是 `wait-source`
- `wait-source` 也不是“轻量版 future”
- `task group` 更不是“future 数组”


## 4. XRT 当前 `wait-source` 能包装什么

XRT 当前正式 `wait-source` 主线已经覆盖：

- `future`
- `stream`
- `dgram recv`
- `listener accept`

通用入口是：

```c
XXAPI xwaitsrc xWaitSourceNone(void);
XXAPI xwaitsrc xWaitSourceFromFuture(xfuture* pFuture);
XXAPI xwaitsrc xWaitSourceFromStream(xnetstream* pStream, uint32 iWaitKind);
XXAPI xwaitsrc xWaitSourceFromDgramRecv(xdgramsock* pSock);
XXAPI xwaitsrc xWaitSourceFromListenerAccept(xnetlistener* pListener);
```

如果你已经明确在网络层工作，也可以看到同一家族的网络命名版本：

```c
XXAPI xnetwaitsrc xrtNetWaitSourceFuture(xnetfuture* pFuture);
XXAPI xnetwaitsrc xrtNetWaitSourceStream(xnetstream* pStream, uint32 iWaitKind);
XXAPI xnetwaitsrc xrtNetWaitSourceDgramRecv(xdgramsock* pSock);
XXAPI xnetwaitsrc xrtNetWaitSourceListenerAccept(xnetlistener* pListener);
```

这里可以这样理解：

- `xWaitSourceFrom*` 更适合写“统一等待模型”教程和跨模块代码
- `xrtNetWaitSource*` 更适合明确处在 `xnet` 主线里的代码

这是一种基于现有接口分层方式的使用建议。


## 5. 统一等待接口怎么读

### 5.1 线程 / 同步侧

通用等待入口是：

```c
XXAPI bool xWaitSourceWait(const xwaitsrc* pSrc);
XXAPI bool xWaitSourceWaitTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs);
XXAPI bool xWaitSourceWaitUntil(const xwaitsrc* pSrc, int64 iDeadlineMs);
```

如果你还想直接拿值：

```c
XXAPI ptr xWaitSourceWaitValue(const xwaitsrc* pSrc);
XXAPI ptr xWaitSourceWaitValueTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs);
XXAPI ptr xWaitSourceWaitValueUntil(const xwaitsrc* pSrc, int64 iDeadlineMs);
```


### 5.2 协程侧

协程侧是同一组语义，只是把“阻塞当前线程”改成“挂起当前协程”：

```c
XXAPI bool xWaitSourceWaitCo(const xwaitsrc* pSrc);
XXAPI bool xWaitSourceWaitCoTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs);
XXAPI bool xWaitSourceWaitCoUntil(const xwaitsrc* pSrc, int64 iDeadlineMs);

XXAPI ptr xWaitSourceWaitCoValue(const xwaitsrc* pSrc);
XXAPI ptr xWaitSourceWaitCoValueTimeout(const xwaitsrc* pSrc, int64 iTimeoutMs);
XXAPI ptr xWaitSourceWaitCoValueUntil(const xwaitsrc* pSrc, int64 iDeadlineMs);
```


### 5.3 网络专名版本

网络层还有：

```c
XXAPI xnet_result xrtNetWaitSourceWait(const xnetwaitsrc* pSrc);
XXAPI xnet_result xrtNetWaitSourceWaitTimeout(const xnetwaitsrc* pSrc, uint32 iTimeoutMs);
XXAPI xnet_result xrtNetWaitSourceWaitCo(const xnetwaitsrc* pSrc);
```

这组接口的意义是：

- 保留 `xnet_result` 这类更网络化的结果语义
- 适合你本来就在写 `xnet` 代码，希望保留网络状态细节

如果你只是想学“统一等待模型”，优先先理解 `xWaitSource*` 这一层即可。


## 6. 什么时候直接等 `future`，什么时候上 `wait-source`

这是最常见的疑问。

### 6.1 只有一个 `future`，而且调用点明确知道它就是 `future`

优先直接用：

- `xFutureWait*`
- `xFutureWaitValue*`
- `xFutureWaitCo*`

因为这最直接。


### 6.2 调用点不想关心底层对象种类

如果你的调用点希望保持统一写法：

- 今天等的是 `future`
- 明天等的是 `stream`
- 后天等的是 `listener accept`

这时就该优先切到：

- `wait-source`


### 6.3 已经在写网络对象等待，而且只是想要最短路径

这时可以优先用：

- `xrtNetStreamWaitReadableCo*`
- `xrtNetStreamWaitWritableCo*`
- `xrtNetStreamWaitDrainCo*`

因为这些 helper 更短、更像业务代码。

但要记住：

> 这些 helper 的底层思路，仍然是统一等待主线。


## 7. DEMO 1：先用 `future` 学 `wait-source`

先从最小、最稳的入口开始：

- 在线程里运行一个任务
- 任务返回一个结果
- 主线程不直接 `xFutureWaitValueTimeout()`，而是先转成 `wait-source`

```c
#include "xrt.h"
#include <stdio.h>

static int32 procSquare(ptr pArg, xfuture_result* pOut)
{
	int32* pInput = (int32*)pArg;
	int32* pResult = NULL;

	if ( (pInput == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	pResult = (int32*)xrtMalloc(sizeof(int32));
	if ( pResult == NULL ) {
		return XRT_NET_ERROR;
	}

	*pResult = (*pInput) * (*pInput);
	pOut->pValue = pResult;
	return XRT_NET_OK;
}

int main(void)
{
	int32 iInput;
	xfuture* pFuture;
	xwaitsrc tSrc;
	int32* pResult;

	xrtInit();

	iInput = 12;
	pFuture = xTaskRunThread(procSquare, &iInput, 0);
	if ( pFuture == NULL ) {
		xrtUnit();
		return 1;
	}

	tSrc = xWaitSourceFromFuture(pFuture);
	pResult = (int32*)xWaitSourceWaitValueTimeout(&tSrc, 3000);

	if ( pResult != NULL ) {
		printf("square = %d\n", (int)*pResult);
		xrtFree(pResult);
	}

	xFutureRelease(pFuture);
	xrtUnit();
	return 0;
}
```

这个 DEMO 最重要的不是“平方算出来了”，而是下面这条线：

1. `task` 负责执行
2. `future` 负责承接结果
3. `wait-source` 负责把“等这个结果”统一成一种等待对象

也就是说，`wait-source` 并没有取代 `future`，它只是站在“等待入口”这一层再做了一次抽象。


## 8. DEMO 2：把同一套等待入口搬到协程里

第二步要看的不是“再算一次平方”，而是：

> 同一个 `wait-source`，既可以在线程侧等，也可以在协程侧等。

```c
#include "xrt.h"
#include <stdio.h>

typedef struct
{
	int32 iInput;
} DemoWaitSourceCoCtx;

static int32 procSquare(ptr pArg, xfuture_result* pOut)
{
	int32* pInput = (int32*)pArg;
	int32* pResult = NULL;

	if ( (pInput == NULL) || (pOut == NULL) ) {
		return XRT_NET_ERROR;
	}

	pResult = (int32*)xrtMalloc(sizeof(int32));
	if ( pResult == NULL ) {
		return XRT_NET_ERROR;
	}

	*pResult = (*pInput) * (*pInput);
	pOut->pValue = pResult;
	return XRT_NET_OK;
}

static void procMainCo(ptr pArg)
{
	DemoWaitSourceCoCtx* pCtx = (DemoWaitSourceCoCtx*)pArg;
	xfuture* pFuture;
	xwaitsrc tSrc;
	int32* pResult;

	pFuture = xTaskRunThread(procSquare, &pCtx->iInput, 0);
	if ( pFuture == NULL ) {
		return;
	}

	tSrc = xWaitSourceFromFuture(pFuture);
	pResult = (int32*)xWaitSourceWaitCoValueTimeout(&tSrc, 3000);

	if ( pResult != NULL ) {
		printf("co square = %d\n", (int)*pResult);
		xrtFree(pResult);
	}

	xFutureRelease(pFuture);
}

int main(void)
{
	xcosched* pSched;
	DemoWaitSourceCoCtx tCtx;

	xrtInit();

	pSched = xrtCoSchedCreate();
	if ( pSched == NULL ) {
		xrtUnit();
		return 1;
	}

	tCtx.iInput = 21;
	if ( xrtCoSchedSpawn(pSched, procMainCo, &tCtx, 0) == NULL ) {
		xrtCoSchedDestroy(pSched);
		xrtUnit();
		return 1;
	}

	xrtCoSchedRun(pSched);
	xrtCoSchedDestroy(pSched);
	xrtUnit();
	return 0;
}
```

这里要重点理解 3 件事：

### 8.1 `wait-source` 没变

你仍然在等：

- `xWaitSourceFromFuture(...)`

变的只是消费入口：

- 线程侧用 `xWaitSourceWait*`
- 协程侧用 `xWaitSourceWaitCo*`


### 8.2 协程没有去承担重活

真正执行平方计算的还是线程任务。

协程做的是：

- 编排
- 等待
- 收结果


### 8.3 这正是 `wait-source` 的价值

它让“等待对象”从“某类 API 的私有概念”变成了“整条异步主线都能识别的统一接口”。


## 9. DEMO 3：把同一套等待入口接到 `stream`

到了这一步，才适合把网络对象接进来。

下面这个片段演示的不是完整建连，而是：

> 换成 `stream` 以后，等待写法本身并没有变。

```c
xwaitsrc tSrc;

tSrc = xWaitSourceFromStream(pStream, XNET_STREAM_WAIT_READABLE);

if ( xWaitSourceWaitCoTimeout(&tSrc, 5000) ) {
	/* stream 已进入 readable 状态，下一步继续走 recv / read 链路 */
}
```

如果你更想保留网络返回状态，也可以写成：

```c
xnetwaitsrc tNetSrc;

tNetSrc = xrtNetWaitSourceStream(pStream, XNET_STREAM_WAIT_READABLE);

if ( xrtNetWaitSourceWaitCoTimeout(&tNetSrc, 5000) == XRT_NET_OK ) {
	/* readable */
}
```

这一段真正想让你看懂的是：

- 对上层流程来说，“等 future” 和 “等 stream readable” 已经开始说同一种语言
- 对底层对象来说，具体发生了什么仍由对象自己的 API 去处理

也就是：

- `wait-source` 统一等待入口
- 真正的读写、接收、accept 仍由对应模块负责


## 10. DEMO 4：换成 `listener accept` 或 `dgram recv`，写法仍然不变

这是 `wait-source` 最值得记住的一点：

> 换等待源，不换等待心智模型。

例如监听器：

```c
xwaitsrc tSrc;

tSrc = xWaitSourceFromListenerAccept(pListener);

if ( xWaitSourceWaitTimeout(&tSrc, 3000) ) {
	/* 说明 accept 事件已就绪，下一步继续走 listener accept API */
}
```

例如数据报接收：

```c
xwaitsrc tSrc;

tSrc = xWaitSourceFromDgramRecv(pSock);

if ( xWaitSourceWaitCoTimeout(&tSrc, 3000) ) {
	/* 说明 dgram recv 已可继续推进 */
}
```

这一步虽然只是短片段，但它很关键，因为它告诉你：

- `wait-source` 不是某个单独模块的小技巧
- 它是 XRT 当前异步主线的统一等待桥


## 11. 一张选型表：应该用哪一层等待

| 场景 | 更推荐 | 原因 |
|------|--------|------|
| 我只是在等一个 `future` | `xFutureWait*` | 最直接，少一层抽象 |
| 我希望调用点不关心底层对象种类 | `xWaitSource*` | 把 future / stream / listener 等待统一起来 |
| 我已经明确在写 `xnet` 业务 | `xrtNetWaitSource*` 或 `xrtNetStreamWait*` | 更贴近网络合同，状态更清楚 |
| 我在协程里等 | `xWaitSourceWaitCo*` / `xFutureWaitCo*` | 不阻塞线程，只挂起协程 |

可以记成一句话：

> 对象越明确，接口越专名；等待语义越想统一，就越该往 `wait-source` 收。


## 12. Windows / Linux 跨平台注意点

### 12.1 不要把等待结果和对象所有权混为一谈

`wait-source` 负责的是：

- 等待

它不负责：

- 替你托管对象生命周期

例如：

- `xWaitSourceFromFuture()` 不会替你接管 `xfuture*`
- 等待结束后，该 `xFutureRelease()` 的仍然要你自己做


### 12.2 超时和截止时间要保持合同一致

同一条流程里，如果你已经决定：

- 外层按 timeout 管

就尽量整条链都用 timeout 语义。

如果你已经决定：

- 外层按 deadline 管

就尽量整条链都用 until 语义。

这样跨平台时，行为更容易对齐，也更好排查。


### 12.3 不要在协程里回退成忙轮询

如果你已经有：

- `xWaitSourceWaitCo*`

就不要再写：

```c
while ( !bReady ) {
	xrtCoYield();
}
```

这种代码短期能跑，长期会把等待语义重新打散。


## 13. 常见错误

### 13.1 错误一：把 `wait-source` 当成 `future` 的别名

不是。

`future` 是结果对象，`wait-source` 是等待对象。


### 13.2 错误二：有了 `wait-source`，就不需要对象自己的 API

也不是。

`wait-source` 只告诉你“可以继续等/可以继续推进”。

真正的：

- 读取
- 写入
- accept
- recv

仍然要回到各自模块的 API。


### 13.3 错误三：只记住大量专名 helper，却忘了统一主线

如果你只会：

- `xrtNetStreamWaitReadableCoTimeout`
- `xrtNetStreamWaitDrainCoUntil`

那你很容易继续把每种等待都当成独立小世界。

更稳的做法是先理解：

- `wait-source` 是统一等待桥

然后再把专名 helper 当成薄封装看待。


### 13.4 错误四：在普通线程里误用 `WaitCo*`

协程等待入口只能在协程上下文里使用。

线程侧和协程侧虽然语义接近，但不是同一个执行环境。


### 13.5 错误五：等完以后忘了收口真正的结果对象

例如：

- `future` 等完后忘了 `xFutureRelease()`

`wait-source` 不会替你做这类生命周期回收。


## 14. 下一步应该读什么

推荐顺序：

1. [Task Group 入门：从统一等待走到结构化收口](task-group-intro.md)
2. [xnet-v2 Stream Wait-Source 入门](xnet-stream-wait-source-intro.md)
3. [Future / Task / Promise](../api/api-future-task-promise.md)
4. [XNet V2 API](../api/api-xnet-v2.md)
5. [xnet-v2 Stream Wait-Source 实战](../case/xnet-stream-wait-source.md)

---

**一句话总结：** `wait-source` 解决的不是“结果怎么存”，而是“等待入口怎么统一”；在 XRT 当前主线里，它把 `future`、`stream`、`dgram recv`、`listener accept` 这些本来容易分裂的等待对象收进同一种语言，让线程和协程都能沿同一条语义链继续写下去。
