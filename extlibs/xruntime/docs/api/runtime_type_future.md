# Future 运行时类型

`runtime_type_future` 把 Future 消费端引用表示为可复制、可移动、可销毁的运行时类型槽，供类型容器、反射系统和动态宿主使用。启用宏为 `XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE`，只依赖 `future` 与 `runtime_type`，不依赖动态 `value`。

```c
#include <xrt/runtime_type_future.h>

const xrttype* xrtTypeFuture(void);
```

## 类型契约

`xrtTypeFuture()` 返回进程期稳定、只读的类型描述：

| 字段 | 值 |
| --- | --- |
| `AbiName` | `xrt.future` |
| `Kind` | `XRT_TYPE_FUTURE` |
| C ABI 槽 | `xfuture*` |
| `Size` / `Align` | 一个 Future 指针的大小和对齐 |
| 标志 | `COPYABLE`、`REFERENCE`、`NULLABLE`、`FINAL`、`RELOCATABLE` |

描述符 ID 等于 `xrtTypeId(XRT_STR_LITERAL("xrt.future"))`。描述符、名称和操作表均为静态只读对象，调用方只借用其地址，不得修改或释放。

## 槽所有权

- `Init` 把槽初始化为 `NULL`。
- `Copy` 和 `Clone` 增加来源 Future 的一个消费端引用，成功后替换目标旧引用。
- `Move` 把引用移交给目标并清空来源，同时释放目标原有引用。
- `Drop` 释放槽拥有的引用并把槽恢复为 `NULL`。
- 自复制和自移动成功且不改变槽。
- 复制失败时目标保持原值，来源始终不变。

比较和散列使用 Future 对象的进程内身份。同一个 Future 的不同消费端引用比较相等并具有相同散列；结果不能作为跨进程或持久化标识。

## 类型容器

`xrtTypeFuture()` 可以直接交给 `xtypedarray`、`xtypedlist`、`xtypedset` 和 `xtypeddict`。容器通过类型操作表维护每一个槽的引用，不需要引入动态 Value：

```c
xtypedarray Futures;
xfuture* Future = NULL;
xpromise* Promise = xrtPromiseCreate(&Future, NULL);

if (
	(Promise == NULL) ||
	!xrtTypedArrayInit(&Futures, xrtTypeFuture()) ||
	!xrtTypedArrayPush(&Futures, &Future)
) {
	return false;
}
xrtFutureDestroy(Future);
xrtTypedArrayUnit(&Futures);
xrtPromiseDestroy(Promise);
```

只有需要把 Future 放入 `xvalue` 时才启用 `runtime_value_future`。该上层模块提供 `xrtValueFuture`、`xrtValueFutureTake` 和 `xrtValueGetFuture`。

## 线程与错误

Future 引用计数和完成状态遵循 `future` 模块的并发契约。类型描述符可以跨线程共享；同一个可变槽或类型容器仍需要调用方同步。

类型槽参数、类型能力和操作错误使用 `runtime_type` 的错误契约。Future 增加引用失败时保留下层错误；销毁空槽是安全操作。

## 范例

- `examples/runtime/type_future/main.c`
