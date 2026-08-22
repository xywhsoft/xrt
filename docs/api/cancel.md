# 取消令牌

`cancel` 模块提供与网络、任务或协程无关的通用取消状态。令牌可以组成不可变父子链；监听任一子令牌时，父链上的首次取消也会同步触发该监听。

## 裁剪与依赖

| 项目 | 值 |
| --- | --- |
| 裁剪宏 | `XRT_FEATURE_CANCEL` |
| 直接依赖 | `XRT_FEATURE_COND`、`XRT_FEATURE_MUTEX` |
| 头文件 | `<xrt/cancel.h>` 或 `<xrt.h>` |

该模块复用 XRT 的互斥锁和条件变量完成监听生命周期同步，不创建线程，也不依赖网络引擎、任务调度器或协程运行时。

## 类型

### `xcancel`

引用计数取消令牌。取消状态只能从“未请求”变为“已请求”，不能复位。子令牌持有父令牌引用，因此父链在子令牌存活期间保持有效。

### `xcancelwatch`

一次监听句柄。它同时挂接到目标令牌及其全部祖先，任一节点首次取消时把监听标记为已触发。每个监听的回调至多执行一次。

### `xcancelproc`

```c
typedef void (*xcancelproc)(ptr pData);
```

回调由命中取消的线程同步执行。回调不得跨越 C 调用栈跳转，必须正常返回。耗时工作应由回调唤醒其他执行单元处理，避免延长 `xrtCancelRequest`。

## 核心契约

- `xrtCancelRequest` 只对当前令牌的第一次本地请求返回 `true`。父令牌已经取消后，子令牌仍可完成自己的第一次本地请求，但同一监听不会重复回调。
- `xrtCancelRequested` 查询当前令牌和完整父链，因此父取消对子查询立即可见；子取消不会向父传播。
- 父关系在创建后不可变，避免传播期间修改拓扑和额外同步。
- 在已经取消的令牌上注册监听，会在 `xrtCancelWatch` 返回前同步执行一次回调，不会遗漏取消。
- `xrtCancelUnwatch` 从其他线程调用时，会等待已经开始的回调返回；函数返回后该监听不会再执行回调。
- 回调可以注销自己的监听。此时注销不会等待自己，实际内存回收延迟到回调返回后完成；调用方不能再次访问该监听。
- 同一个监听句柄只能完成一次所有权注销。并发发起的注销会被安全串行化，但首次注销完成后再次使用旧指针属于无效访问。

## 函数

### `xrtCancelCreate`

```c
xcancel* xrtCancelCreate(void);
```

创建独立令牌并返回一个调用方拥有的引用。失败返回空指针并保留结构化错误。

### `xrtCancelChild`

```c
xcancel* xrtCancelChild(xcancel* pParent);
```

创建子令牌并持有父引用。`pParent` 可以为空，此时等价于创建新的根令牌。

### `xrtCancelRef` / `xrtCancelDestroy`

```c
xcancel* xrtCancelRef(xcancel* pCancel);
void xrtCancelDestroy(xcancel* pCancel);
```

增加和释放令牌引用。`xrtCancelDestroy(NULL)` 是空操作。每一个成功创建或增加的引用必须释放一次。

### `xrtCancelRequest`

```c
bool xrtCancelRequest(xcancel* pCancel);
```

原子完成当前令牌的首次本地取消请求，摘除该节点上的监听后在令牌锁外同步通知。首次请求返回 `true`，重复请求返回 `false` 且不设置错误。空令牌返回 `false` 并设置 `XERR_ARGUMENT`。

### `xrtCancelRequested`

```c
bool xrtCancelRequested(const xcancel* pCancel);
```

查询当前令牌或祖先是否已取消。空指针表示没有取消源，返回 `false` 且不设置错误，便于可选取消参数直接使用。

### `xrtCancelWatch`

```c
xcancelwatch* xrtCancelWatch(
	xcancel* pCancel,
	xcancelproc pProc,
	ptr pData
);
```

注册一次同步回调。监听持有目标令牌引用，并用一次连续分配保存完整父链节点。令牌或回调为空时失败并设置 `XERR_ARGUMENT`；分配失败设置 `XERR_MEMORY`。

### `xrtCancelTriggered`

```c
bool xrtCancelTriggered(const xcancelwatch* pWatch);
```

无锁查询监听是否已经命中取消。空监听返回 `false` 并设置 `XERR_ARGUMENT`。

### `xrtCancelUnwatch`

```c
void xrtCancelUnwatch(xcancelwatch* pWatch);
```

注销并释放调用方拥有的监听。空指针是空操作。普通路径会保证返回时没有正在执行或未来可能执行的回调；回调自身注销使用上文说明的延迟回收规则。

## 示例

```c
xcancel* pGroup = xrtCancelCreate();
xcancel* pOperation = xrtCancelChild(pGroup);
xcancelwatch* pWatch = xrtCancelWatch(pOperation, stopWork, pState);

if ( (pGroup == NULL) || (pOperation == NULL) || (pWatch == NULL) ) {
	return false;
}
xrtCancelRequest(pGroup);
xrtCancelUnwatch(pWatch);
xrtCancelDestroy(pOperation);
xrtCancelDestroy(pGroup);
```

完整示例位于 `examples/concurrency/cancel/main.c`。
