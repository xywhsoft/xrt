# Future / Task / Promise 正式异步模型

> XRT 统一异步运行时：future、promise、task、wait-source、task group

[返回索引](README.md)

---

## 📑 目录

- [设计目标](#设计目标)
- [核心对象](#核心对象)
- [快速规则](#快速规则)
- [基础 future / promise](#基础-future--promise)
- [task 执行模型](#task-执行模型)
- [continuation](#continuation)
- [组合 future](#组合-future)
- [task group 与嵌套 scope](#task-group-与嵌套-scope)
- [ownership 规则](#ownership-规则)
- [推荐写法](#推荐写法)

---

## 设计目标

这套模型用于统一：

- 线程任务结果
- 协程任务结果
- engine worker task 结果
- wait-source 等待结果
- continuation 链
- 组合等待
- structured concurrency

核心原则：

- 一个正式结果对象：`xfuture`
- 一个正式写端：`xpromise`
- 一个正式执行对象：`xtask`
- 一个正式等待桥：`xwaitsrc`

---

## 核心对象

### `xfuture`

表示一个未来会完成的结果。

负责：

- 状态
- 值
- 错误
- 等待
- continuation
- cancel request


### `xpromise`

表示 `future` 的写端。

负责：

- `resolve`
- `reject`
- `cancel`
- `close`

规则：

- 一个 promise 只能把 future 推进到一次终态


### `xtask`

表示一个可调度执行的异步任务。

当前正式执行入口：

- `xTaskRunThread`
- `xTaskRunCo`
- `xTaskRunEngine`
- `xTaskRunDelayed`


### `xwaitsrc`

表示一个统一等待源。

当前主线里它主要用于：

- future
- stream wait
- dgram recv

---

## 快速规则

### 1. `Wait / WaitTimeout / WaitUntil` 是统一等待三件套

正式等待接口默认都按这组语义设计。


### 2. `Get*` 和 `Peek*` 不是一回事

- `Get*`：返回持有引用，调用者后续要 `xFutureRelease()`
- `Peek*`：返回借用引用，只在原对象仍有效时可用


### 3. `current-thread deferred continuation` 不要求总是手工 pump

如果你在当前线程使用：

- `xFutureWait`
- `xFutureWaitTimeout`
- `xFutureWaitUntil`

等待主路径会自动推进当前线程 deferred continuation。


### 4. `task group` 是 scope，不只是 future 容器

当前 `task group` 已经有正式 scope 语义：

- `Close`
- `ReapCompleted`
- `Destroy => Close + Cancel pending children`
- `JoinFuture / Join / JoinTimeout / JoinUntil`
- `CreateChild`


### 5. nested scope 走 `xTaskGroupCreateChild`

不要自己手工拼：

- parent bind
- child join add

正式嵌套入口是：

- `xTaskGroupCreateChild`


### 6. 当前主线推荐理解顺序

建议按下面顺序理解这套模型：

1. `xfuture / xpromise`
2. `Wait / WaitTimeout / WaitUntil`
3. `xtask`
4. `Then / Catch / Finally`
5. `WhenAny / WhenAll / Race`
6. `task group / nested scope`

---

## 基础 future / promise

### 创建与完成

```c
xfuture* pFuture = xFutureCreate();
xpromise* pPromise = xPromiseCreate(pFuture);

int iValue = 123;
(void)xPromiseResolve(pPromise, &iValue);

if ( xFutureWaitTimeout(pFuture, 1000) ) {
	ptr pValue = xFutureValue(pFuture);
}

xPromiseDestroy(pPromise);
xFutureRelease(pFuture);
```

如果结果值需要交给 `future` 释放，使用 `xPromiseResolveOwned()`：

```c
ptr pValue = xrtMalloc(sizeof(int));
*(int*)pValue = 123;

(void)xPromiseResolveOwned(pPromise, pValue);
```

使用 owned 结果后，结果内存由完成后的 `future` 持有，调用者不要再手动释放 `pValue`。


### 失败、取消、关闭

```c
(void)xPromiseReject(pPromise, XRT_NET_ERROR, (str)"task failed");
(void)xPromiseCancel(pPromise, (str)"task cancelled");
(void)xPromiseClose(pPromise, (str)"task closed");
```


### 读取结果

```c
xfuture_result tResult;

if ( xFutureGetResult(pFuture, &tResult) ) {
	/* 成功完成 */
}
```

常用读取：

- `xFutureState`
- `xFutureStatus`
- `xFutureValue`
- `xFutureError`
- `xFutureValueIsOwned`

---

## task 执行模型

### 线程任务

```c
static int32 procTask(ptr pArg, xfuture_result* pOut)
{
	int* pValue = (int*)pArg;

	if ( pOut == NULL || pValue == NULL ) {
		return XRT_NET_ERROR;
	}

	pOut->pValue = pValue;
	return XRT_NET_OK;
}

int iValue = 100;
xfuture* pFuture = xTaskRunThread(procTask, &iValue, 0);
```


### 协程任务

```c
xfuture* pFuture = xTaskRunCo(pSched, procTask, &iValue, 0);
```


### engine worker task

```c
static int32 procEngineTask(xnetworker* pWorker, ptr pArg, xfuture_result* pOut)
{
	(void)pWorker;
	pOut->pValue = pArg;
	return XRT_NET_OK;
}

xfuture* pFuture = xTaskRunEngine(pEngine, 0, procEngineTask, pArg);
```


### 与网络 / 协程主线的关系

这套正式异步模型不是独立存在的附加层，而是当前主线里：

- 协程等待
- `xnet-v2` wait-source
- engine worker task
- stream / dgram / future bridge

共同使用的统一异步语言。

如果你在写新的异步代码，优先思路应当是：

- 结果统一落到 `xfuture`
- 执行统一落到 `xtask`
- 等待统一走 `Wait / WaitTimeout / WaitUntil`
- 组合统一走 `WhenAny / WhenAll / Race`

---

## continuation

当前正式 continuation：

- `Then*`
- `Catch*`
- `Finally*`

执行目标：

- `Inline`
- `Current`
- `Engine`
- `Co`


### `ThenCurrent`

```c
xfuture* pNext = xFutureThenCurrent(pFuture, procThen, pArg);
```

适合：

- 当前线程内串联异步步骤
- 不想额外切到 engine / coroutine scheduler


### `ThenEngine`

```c
xfuture* pNext = xFutureThenEngine(pFuture, pEngine, 0, procThen, pArg);
```

适合：

- 后续逻辑必须回到 engine worker 上执行


### `ThenCo`

```c
xfuture* pNext = xFutureThenCo(pFuture, pSched, procThen, pArg, 0);
```

适合：

- continuation 明确属于协程调度上下文

---

## 组合 future

### `WhenAny`

任一 source 完成就完成。

```c
xfuture* arrFuture[2] = { pA, pB };
xfuture* pAny = xFutureWhenAny(arrFuture, 2);
```

完成后可读取：

- `xFutureGetGroupSourceIndex`
- `xFutureGetGroupSource`
- `xFuturePeekGroupSource`


### `Race`

任一 source 完成就完成，并对其余 source 发取消请求。

```c
xfuture* pRace = xFutureRace(arrFuture, 2);
```


### `WhenAll`

全部 source 完成后完成。

```c
xfuture* pAll = xFutureWhenAll(arrFuture, 2);
```

成功时：

- `xFutureValue(pAll)` 为 `xfuture_all_value*`
- 可用：
	- `xFuturePeekAllValue`
	- `xFutureGetAllValueCount`
	- `xFutureGetAllValueItem`

失败时：

- 返回首个失败结果
- 并记录失败源 `index/source future`

---

## task group 与嵌套 scope

### 基础 group

```c
xtaskgroup* pGroup = xTaskGroupCreate();

xfuture* pA = xTaskGroupRunThread(pGroup, procTask, &iA, 0);
xfuture* pB = xTaskGroupRunEngine(pGroup, pEngine, 0, procEngineTask, &iB);

xTaskGroupClose(pGroup);
xTaskGroupJoinTimeout(pGroup, 1000);
xTaskGroupDestroy(pGroup);

xFutureRelease(pA);
xFutureRelease(pB);
```


### 动态 join

`xTaskGroupJoinFuture()` 当前是 `close-aware dynamic join`：

- group open 阶段不会提前完成
- 后续新增 child 仍会纳入 join
- 只有 `Close` 且无 pending child 后才完成


### nested scope

```c
xtaskgroup* pParent = xTaskGroupCreate();
xtaskgroup* pChild = xTaskGroupCreateChild(pParent);
```

当前合同：

- child 自动绑定 parent scope
- child join 自动纳入 parent
- parent `Close/Cancel/Destroy` 会向 child 传播 `Close + Cancel`

---

## ownership 规则

### promise 侧

- `xPromiseGetFuture`：返回持有引用
- `xPromisePeekFuture`：返回借用引用
- `xPromiseResolve`：结果值为借用指针，调用方仍负责结果值生命周期
- `xPromiseResolveOwned`：结果值所有权转交给完成后的 `future`


### combinator 侧

- `xFutureGetGroupSource`：返回持有引用
- `xFuturePeekGroupSource`：返回借用引用
- `xFutureValueIsOwned`：返回当前 `future` 是否持有结果值所有权

`WhenAny` 和 `Race` 的组合结果会借用获胜 source future 的结果值；source future 仍负责 owned 结果值的释放。

### 推荐记忆方式

凡是：

- `Get*` => 你持有它
- `Peek*` => 你只是看看
- `ResolveOwned` => `future` 持有结果值

---

## 推荐写法

### 当前最推荐的组合方式

当前主线最推荐的理解方式不是单独使用某一个对象，而是把它们组合起来：

- 单个异步结果：`future + promise`
- 可执行异步工作：`task -> future`
- 底层事件桥接：`wait-source -> future`
- 多个异步步骤串联：`Then / Catch / Finally`
- 多个异步任务组合：`WhenAny / WhenAll / Race`
- 一个作用域下的任务集合：`task group / child scope`

### 1. 普通异步任务

```c
xfuture* pFuture = xTaskRunThread(procTask, pArg, 0);
if ( xFutureWaitTimeout(pFuture, 1000) ) {
	ptr pValue = xFutureValue(pFuture);
}
xFutureRelease(pFuture);
```


### 2. continuation 链

```c
xfuture* pA = xTaskRunThread(procTaskA, pArgA, 0);
xfuture* pB = xFutureThenCurrent(pA, procThenB, pArgB);

xFutureWait(pB);

xFutureRelease(pB);
xFutureRelease(pA);
```


### 3. group scope

```c
xtaskgroup* pGroup = xTaskGroupCreate();

(void)xTaskGroupRunThread(pGroup, procTask, &iA, 0);
(void)xTaskGroupRunThread(pGroup, procTask, &iB, 0);

xTaskGroupClose(pGroup);
xTaskGroupJoin(pGroup);
xTaskGroupDestroy(pGroup);
```


### 4. nested scope

```c
xtaskgroup* pRoot = xTaskGroupCreate();
xtaskgroup* pChild = xTaskGroupCreateChild(pRoot);

(void)xTaskGroupRunThread(pChild, procTask, &iWork, 0);

xTaskGroupClose(pRoot);
xTaskGroupJoin(pRoot);

xTaskGroupDestroy(pChild);
xTaskGroupDestroy(pRoot);
```

---

## 当前已进入默认主测试流

这套正式异步模型当前已经：

- 有独立 core harness
- 有压力/竞态回归
- 已纳入默认主测试流执行

当前后续重点不再是零散补接口，而是继续做：

- 更大规模压力验证
- public 使用文档补齐
- 聚合结果合同继续深化
