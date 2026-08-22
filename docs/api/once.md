# Once 一次初始化

`once` 模块提供可失败、可重试的并发一次初始化。它使用固定存储，不分配内存，也不依赖完整线程模块。

## 裁剪与依赖

| 项目 | 值 |
| --- | --- |
| 裁剪宏 | `XRT_FEATURE_ONCE` |
| 直接依赖 | `XRT_FEATURE_CORE` |
| 头文件 | `<xrt/thread.h>` 或 `<xrt.h>` |

## 类型与常量

### `xonce`

不透明的一次初始化对象。对象必须使用 `XRT_ONCE_INIT`、静态零初始化或完整清零初始化，初始化开始后不能复制、移动或再次清零。

### `xonceproc`

```c
typedef bool (*xonceproc)(ptr pData);
```

初始化过程返回 `true` 表示永久完成；返回 `false` 表示本次失败，后续调用可以重新执行。过程应在失败前设置能够说明原因的线程错误。

### `XRT_ONCE_INIT`

静态和自动对象的初始化值。`XRT_ONCE_STORAGE_SIZE` 表示公开不透明存储的字节数，不应依赖其内部布局。

## 函数

### `xrtOnce`

```c
bool xrtOnce(xonce* pOnce, xonceproc pProc, ptr pData);
```

并发调用时只有一个线程执行 `pProc`，其他线程等待该次初始化结束。成功后所有调用直接返回 `true`。失败后一个后续调用可以重试。同一线程从初始化过程递归进入相同对象会立即返回 `false`，错误种类为 `XERR_STATE`。

`pOnce` 或 `pProc` 为空时返回 `false`，错误种类为 `XERR_ARGUMENT`。不同的 `xonce` 对象可以并行初始化。初始化过程必须正常返回，不得使用跨栈跳转绕过返回路径。

递归元数据图不需要第二套 Once API。初始化过程进入另一个 `xonce` 后，如果沿同线程回边再次进入当前对象，内层调用会以 `XERR_STATE` 返回 `false`；图构建器可以确认该回边表示“节点正在构建”，清除该错误并继续。真正的初始化失败仍返回 `false`，保留后续重试能力。跨线程形成相互等待的初始化环属于调用方死锁，必须通过固定初始化顺序或单线程图构建避免。

## 示例

```c
static xonce gOnce = XRT_ONCE_INIT;
static int gValue;

static bool initialize(ptr pData)
{
	*(int*)pData = 42;
	return true;
}

if ( !xrtOnce(&gOnce, initialize, &gValue) ) {
	return false;
}
```

完整示例位于 `examples/concurrency/once/main.c`。
