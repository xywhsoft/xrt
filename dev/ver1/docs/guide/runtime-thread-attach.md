# XRT 运行时与线程附加入门

> 目标：理解 `xrtInit()` / `xrtUnit()`、为什么有线程附加模型、什么时候需要 `xrtThreadAttachCurrent()`。

[返回教学文档](README.md)

---

## 1. 为什么 XRT 需要运行时初始化

XRT 不是一组完全无状态的工具函数。

当前主线里，下面这些能力都依赖运行时：

- 当前线程错误状态
- 当前线程临时内存
- 当前线程默认随机数状态
- 当前线程协程运行时
- 当前线程 deferred continuation 队列

因此，使用这些能力之前，线程必须先进入 XRT 运行时上下文。

主线程最常见的做法就是：

```c
#include "xrt.h"

int main()
{
	xrtInit();

	printf( "hello xrt\n" );

	xrtUnit();
	return 0;
}
```

`xrtInit()` 做两件事：

- 初始化全局运行时
- 把当前线程附加到 XRT 运行时

`xrtUnit()` 则负责：

- 当前线程退出运行时
- 当最后一个引用释放时清理全局运行时


## 2. 什么是线程附加

“线程附加”可以简单理解为：

> 告诉 XRT：这个线程接下来要使用运行时能力，请为它准备线程级状态。

XRT 当前把一部分状态拆成了线程级对象，例如：

- `xrtGetError()`
- `xrtTempMemory()`
- 协程当前调度器状态
- future 的 current-thread deferred continuation 队列

如果线程没有附加，这些能力就没有合法的线程状态可以使用。


## 3. 哪些线程需要手工附加

### 3.1 主线程

主线程通常不需要你显式调用 `xrtThreadAttachCurrent()`，因为：

- `xrtInit()` 已经自动完成附加

### 3.2 由 XRT 创建的线程

如果线程是通过 `xrtThreadCreate()` 创建的，那么：

- XRT 会在线程启动时自动附加
- 线程退出时自动清理线程级状态

也就是说，在线程函数里可以像单线程开发一样直接使用大多数 XRT 运行时能力。

### 3.3 宿主线程 / 外部线程

如果线程不是由 XRT 创建，而是来自：

- 宿主程序线程池
- 第三方库回调线程
- GUI 线程
- 插件宿主线程

那么在使用运行时能力前，应显式附加：

```c
void procHostThread()
{
	xrtThreadAttachCurrent();

	printf( "%s\n", xrtGetError() );

	xrtThreadDetachCurrent();
}
```


## 4. 什么情况下必须考虑线程附加

下面这些情况最容易踩坑：

- 在线程回调里调用 `xrtGetError()`
- 在线程里用 `xrtTempMemory()`
- 在线程里启动协程
- 在线程里等待 future 的 current-thread continuation
- 在线程里依赖默认随机数状态

如果只是调用完全无状态的纯函数，一般不需要关心线程附加；但只要进入 runtime-bound API，就应该先确认线程是否已附加。


## 5. 推荐写法

### 写法一：普通程序主线程

```c
int main()
{
	xrtInit();

	/* 这里直接使用 XRT */

	xrtUnit();
	return 0;
}
```

### 写法二：使用 XRT 线程

```c
uint32 procWorker( ptr pArg )
{
	/* 线程已自动附加 */
	printf( "%s\n", xrtGetError() );
	return 0;
}

void procRun()
{
	xthread pThread = xrtThreadCreate( procWorker, NULL, 0 );
	xrtThreadWait( pThread );
	xrtThreadDestroy( pThread );
}
```

### 写法三：接入外部线程

```c
void procForeignThread()
{
	xrtThreadAttachCurrent();

	/* 这里才安全使用 runtime-bound API */

	xrtThreadDetachCurrent();
}
```


## 6. 常见误区

### 误区一：`xrtInit()` 只初始化全局，不管线程

错误。

当前主线中，`xrtInit()` 会自动附加调用它的线程，因此主线程一般不需要再额外 `AttachCurrent()`。

### 误区二：`xrtThreadDestroy()` 会终止线程

错误。

`xrtThreadDestroy()` 负责销毁线程管理对象，不负责强行终止线程执行。

推荐顺序通常是：

1. `xrtThreadWait()`
2. `xrtThreadDestroy()`

### 误区三：只要能调用某个函数，就说明线程状态一定存在

错误。

很多纯函数看起来能正常调用，但 runtime-bound API 会依赖线程状态。不要把两者混为一谈。


## 7. 建议继续阅读

- [Base API](../api/api-base.md)
- [Thread API](../api/api-thread.md)
- [Coroutine API](../api/api-coroutine.md)
- [Future / Task / Promise](../api/api-future-task-promise.md)

---

**一句话总结：** `xrtInit()` 负责让主线程进入 XRT 运行时，`xrtThreadCreate()` 负责让 XRT 线程自动进入运行时，外部线程则需要显式 `AttachCurrent()`，这样才能安全使用线程级运行时能力。
