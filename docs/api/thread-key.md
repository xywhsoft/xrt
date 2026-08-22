# 动态线程局部键

`thread_key` 模块为库和嵌入式运行时提供动态原生线程局部值。值按操作系统线程隔离，同一原生线程上的宿主、Fiber 和 XRT 协程看见同一个值，不会因执行上下文切换而变化。

## 裁剪与依赖

| 项目 | 值 |
| --- | --- |
| 裁剪宏 | `XRT_FEATURE_THREAD_KEY` |
| 直接依赖 | `XRT_FEATURE_CORE` |
| 头文件 | `<xrt/thread.h>` 或 `<xrt.h>` |

## 类型

### `xthreadkey`

不透明动态键。一个键可以在多个线程中分别保存一个值。键对象必须活到所有访问它的线程都已退出或停止访问。

### `xthreadkeyproc`

```c
typedef void (*xthreadkeyproc)(ptr pValue);
```

可选值析构过程。非空值被替换、清除、随线程退出或随键销毁时调用。调用过程取得值的所有权。

## 所有权与线程规则

- `xrtThreadKeySet` 成功后，键取得非空值的所有权。
- 设置同一个指针是无操作，不会重复析构。
- `xrtThreadKeyTake` 把所有权交还调用方，不执行析构。
- 每个原生线程的值互相隔离，同一线程上的全部协程共享线程值。
- XRT 创建的线程退出时自动清理。POSIX 外部线程也由 `pthread` 自动清理；Windows 外部线程必须在退出前调用 `xrtThreadKeysClear`，这是静态库和单头文件无法注册通用线程退出析构的明确平台边界。
- 销毁键前，调用方必须保证其他线程已退出或不再访问该键。
- 析构过程可以操作其他键。不要从值析构过程中再次访问正在销毁的键。

## 函数

### `xrtThreadKeyCreate`

```c
xthreadkey* xrtThreadKeyCreate(xthreadkeyproc pDestroy);
```

创建动态键。`pDestroy` 可以为空。失败返回空指针并设置结构化错误。

### `xrtThreadKeyDestroy`

```c
bool xrtThreadKeyDestroy(xthreadkey* pKey);
```

销毁键并析构当前线程仍拥有的值。空指针视为成功。平台拒绝销毁时返回 `false`，键保持有效；成功后不能再访问键。

### `xrtThreadKeyGet`

```c
ptr xrtThreadKeyGet(const xthreadkey* pKey);
```

返回当前线程值的借用指针；未设置时返回空指针。键为空时也返回空指针，但会设置 `XERR_ARGUMENT`。

### `xrtThreadKeySet`

```c
bool xrtThreadKeySet(xthreadkey* pKey, ptr pValue);
```

成功时转移新值所有权，并在替换完成后析构旧值。空值用于清除。分配或平台写入失败时返回 `false`，原值和新值的所有权均不改变。

### `xrtThreadKeyTake`

```c
ptr xrtThreadKeyTake(xthreadkey* pKey);
```

移除并返回当前线程的值，不执行析构。无值时返回空指针。平台清除失败时保留原值并设置错误。

### `xrtThreadKeysClear`

```c
bool xrtThreadKeysClear(void);
```

析构并移除当前原生线程保存的全部动态键值。函数幂等；析构过程重新安装值时最多继续清理四轮，与 POSIX 线程键退出语义一致。四轮后仍有值时返回 `false`、保留剩余值并设置 `XERR_STATE`，避免恶意或错误析构过程造成无限循环。

## 示例

```c
xthreadkey* pKey = xrtThreadKeyCreate(xrtFree);
int* pValue = (int*)xrtMalloc(sizeof(int));

if ( (pKey == NULL) || (pValue == NULL) ) {
	return false;
}
*pValue = 42;
if ( !xrtThreadKeySet(pKey, pValue) ) {
	xrtFree(pValue);
	xrtThreadKeyDestroy(pKey);
	return false;
}
printf("%d\n", *(int*)xrtThreadKeyGet(pKey));
return xrtThreadKeyDestroy(pKey);
```

完整示例位于 `examples/concurrency/thread_key/main.c`。
