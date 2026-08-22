# Runtime Value

`runtime_value` 把运行时对象、callable、Future 和弱引用接入不透明 `xvalue`，供脚本宿主、插件系统和通用 C 动态调用使用。桥接复用 Value Handle 的所有权、Hash/Equal 和深克隆策略，不修改 Value 核心枚举，也不让基础 Value 反向依赖运行时对象层。该模块属于 `xruntime` 扩展。

## 裁剪层次

| 特性宏 | 直接依赖 | 能力 |
| --- | --- | --- |
| `XRUNTIME_FEATURE_RUNTIME_VALUE_OBJECT` | `runtime_object`、`value` | 运行时对象强引用装箱 |
| `XRUNTIME_FEATURE_RUNTIME_VALUE_CALLABLE` | `runtime_call` | callable 装箱和动态调用 |
| `XRUNTIME_FEATURE_RUNTIME_VALUE_FUTURE` | `runtime_type_future`、`value` | Future 消费端引用装箱 |
| `XRUNTIME_FEATURE_RUNTIME_VALUE_WEAK` | `runtime_object`、`value` | 弱引用装箱、复制、过期和锁定 |
| `XRUNTIME_FEATURE_RUNTIME_VALUE_TYPE` | `runtime_type`、`value_graph` | 拥有 `xvalue*` 槽的稳定运行时类型 |
| `XRUNTIME_FEATURE_RUNTIME_VALUE_TRACE` | `runtime_value_type`、`runtime_value_object` | 枚举 Value 图中的运行时对象强引用 |
| `XRUNTIME_FEATURE_RUNTIME_VALUE_ROOTS` | `runtime_value_trace`、`runtime_object_graph` | 把宿主 Value 图作为对象图显式根 |

各特性共用 `runtime_value.h` 和一个实现文件，生成系统只编译一次实现，不会复制公共逻辑。对象、callable、Future 和弱引用桥接可以独立裁剪；Value 类型和追踪按依赖逐层启用。Future 的类型描述由更低层的 `runtime_type_future` 提供，动态装箱不会为了深克隆支持强制引入 `value_graph`。

## 对象桥接

```c
xvalue* xrtValueRuntimeObject(xrtobject* object);
xvalue* xrtValueRuntimeObjectTake(xrtobject** object);
xrtobject* xrtValueGetRuntimeObject(const xvalue* value);
```

普通构造增加一个对象强引用；`Take` 成功时转移现有强引用并清空来源。Getter 返回借用对象，生命周期由 Value 维持。对象 Handle 按对象地址提供进程内身份 Hash/Equal。

`XVALUE_OBJECT` 始终表示字符串键动态字典，不表示类实例。`xrtValueIsRuntimeObject` 精确识别桥接 Handle，避免协议层和语言运行时对“object”产生两套隐式解释。

运行时对象负载可以修改，因此 Value Graph 深克隆只复制对象引用载体并增加强引用，始终保持原对象身份；它不会伪装成独立类实例。普通 `xrtValueClone` 仍只增加不可变 Value 外壳引用。需要复制类实例时，应由类型或语言层定义显式 Clone 语义。

## Value 类型与对象追踪

`xrtTypeValue()` 返回进程期稳定的 `xrt.Value` 类型描述。该类型拥有一个已初始化 `xvalue*` 槽：`Copy` 为标量增加引用、为容器创建共享 backing 的独立 COW 外壳，适合强类型容器的常规插入和替换；`Clone` 深复制完整 Value 图并保留共享子图拓扑；`Move` 转移来源并恢复为 null；`Drop` 释放槽位。两种复制都先准备新值再替换目标，失败时目标保持不变。运行时对象值在深克隆后仍保持对象身份，但每个载体各自拥有强引用。

通用 Value 图并不天然可排序，也只有部分标量可散列，因此 `xrt.Value` 不提供 `Compare/Hash` 类型操作。需要键语义时，应选用具体的可比较、可散列类型，或在应用层把 Value 规范化为明确键类型。

`xrtValueTraceRuntimeObjects()` 遍历一个 Value 图直接拥有的运行时对象引用，并按 Value 外壳和容器 backing 身份去重。它不会报告弱引用、裸指针或 callable 环境。语言闭包环境应建模为可追踪运行时对象，而不是隐藏在不可见句柄中。

同一个对象 Handle 外壳被容器引用多次时只报告一次，因为对象强引用实际存放在该外壳中；两个独立外壳即使指向同一个对象，也分别拥有强引用并分别报告。小图使用 32 项栈内身份表，大图按需创建集合。追踪深度与 Value Graph 共用 `XRT_VALUE_DEPTH_MAX`，超过限制返回 `XRUNTIME_VALUE_ERROR_TRACE`。访问器返回 `false` 时应设置自己的错误；没有设置新错误时，追踪层生成明确的 `XERR_STATE`，不会误用调用前遗留的旧错误。

成功遍历恢复调用前错误，并丢弃访问器在成功路径留下的临时错误，因此用户回调不会污染外层错误上下文。

动态字段使用深复制写入隔离容器 backing，保证每个字段节点报告的对象边与其实际所有权一致。详见 `docs/api/runtime_dynamic_field.md`。

## Value 根与对象图

`xrtValueRetain()` 共享同一个 Value 外壳，`xrtValueClone()` 对容器创建共享 backing 的 COW
外壳。这些引用保证 Value 本身的生命周期，但不会为其内部每个运行时对象复制强引用。因此，
宿主栈、全局变量、挂起生成器或 native 状态中仍存活的外部 Value 图必须在对象图安全点作为根
报告；只比较 `xrtobject` 强引用数无法发现外层 Value backing 的共享所有权。

```c
xrtobjectgraphresult Result;
const xvalue* Roots[] = { StackValue, PendingResult };

if ( !xrtObjectGraphCollectValueRoots(
	Graph, Roots, sizeof(Roots) / sizeof(Roots[0]), &Result
) ) {
	return false;
}
```

`xrtObjectGraphCollectValueRoot()` 是单根快捷入口；批量入口接受借用数组并按实际对象身份去重。
空数组等价于 `xrtObjectGraphCollect()`。数组和每个根在收集完成前必须保持稳定且非空，失败时
结果结构、对象图和 Value 图保持不变。成功路径保留调用方原有错误。

动态字段 `Get` 和迭代器返回的值是借用视图。若视图只在字段对象存活期间立即读取，不需要
额外根；若通过 `xrtValueRetain()` 或 COW `xrtValueClone()` 把它带入宿主栈、任务帧或挂起状态，
安全点必须使用 Value 根入口。需要完全独立且可脱离原字段对象的值时，使用
`xrtDynamicFieldsCopy()`，其深克隆会为运行时对象建立独立强引用载体。

## Callable 桥接

```c
xvalue* xrtValueCallable(xrtcallable* callable);
xvalue* xrtValueCallableTake(xrtcallable** callable);
bool xrtValueIsCallable(const xvalue* value);
xrtcallable* xrtValueGetCallable(const xvalue* value);
const xrtfunctionsig* xrtValueCallableSignature(const xvalue* value);
bool xrtValueInvoke(
	const xvalue* callable,
	const xrtcallframe* frame,
	xrtcallresult* result
);
```

callable 创建后不可变，因此 Handle 深克隆可以安全地共享 callable 引用。`xrtValueCallableSignature` 返回借用签名；动态 callable 返回 `NULL`。`xrtValueInvoke` 只负责精确解箱，然后保留 `xrtCallableInvoke` 的帧验证、失败原子结果和 `xrt.call` 原因链。

同步流操作可用 `xrtprogresscall` 把动态 callable 直接适配为 `xrtprogressproc`：

```c
xrtprogresscall Progress;

xrtProgressCallInit(&Progress, callbackValue);
operation(
	Progress.Callback != NULL ? xrtProgressCallInvoke : NULL,
	&Progress
);
if ( Progress.InvokeFailed ) {
	/* callable 调用失败或没有返回 bool。 */
}
```

桥只借用 `callbackValue`，调用方必须让它覆盖整个同步操作。回调接收
`(processed, total, output)` 三个整数并返回是否继续；null Value 会关闭报告且始终允许继续。
异步操作不得把栈上的 `xrtprogresscall` 留给后台线程，应由异步对象持有 callable 强引用和上下文。

## Future 桥接

```c
xvalue* xrtValueFuture(xfuture* future);
xvalue* xrtValueFutureTake(xfuture** future);
bool xrtValueIsFuture(const xvalue* value);
xfuture* xrtValueGetFuture(const xvalue* value);
```

Future 的类型描述和类型容器用法见 [Future 运行时类型](runtime_type_future.md)。该描述属于独立 `runtime_type_future` 层；只使用类型容器的程序不会引入动态 Value。

`xrtValueFuture` 增加一个消费端引用，`xrtValueFutureTake` 只在 Value 创建成功后移交引用并清空来源。Getter 返回由 Value 保活的借用 Future。Value 深克隆创建独立外壳并增加 Future 引用，但所有外壳仍观察同一个完成状态、结果、错误和取消源。

## 弱引用桥接

`xrtValueWeak` 复制弱控制块引用，`xrtValueWeakTake` 转移它。`xrtValueIsWeak` 精确判断桥接类型。三者都不会增加对象强引用。空弱引用也是有效弱 Value，立即报告过期。

`xrtValueGetWeak` 把弱引用复制到已初始化目标；`xrtValueWeakExpired` 查询瞬时状态；`xrtValueWeakLock` 成功时返回新的对象强引用，过期时返回 `NULL` 且不设置错误。弱 Handle 的 Hash/Equal 使用控制块身份，生命周期内保持稳定，即使对象已经过期。

## 所有权

- 非 `Take` 构造增加来源引用，来源仍由调用方持有。
- `Take` 只有成功才清空来源；OOM 和参数错误均保持来源不变。
- Getter 返回借用指针，不增加引用。
- Value 最终释放时恰好释放一个对象、callable、Future 或弱控制块引用。
- 所有桥接 Handle 的策略描述是模块内进程期静态对象。

## 线程

Value 外壳、运行时对象、callable、Future 和弱控制块引用计数均为原子。对同一个不可变桥接 Value 的 Retain、Release 和读取遵循 Value 的线程规则；对象负载的并发访问仍由对象类型或调用方同步。Future 的完成、等待和取消继续遵循 Future 契约。弱引用 Lock 与最终强引用释放可以并发，只会得到完整对象或过期结果。

## 错误

`xruntimevalueerror` 定义桥接类型和所有权错误的稳定代码，错误域为 `xrt.runtime-value`：

- `XRUNTIME_VALUE_ERROR_TYPE`
- `XRUNTIME_VALUE_ERROR_OBJECT`
- `XRUNTIME_VALUE_ERROR_CALLABLE`
- `XRUNTIME_VALUE_ERROR_FUTURE`
- `XRUNTIME_VALUE_ERROR_WEAK`
- `XRUNTIME_VALUE_ERROR_OWNERSHIP`
- `XRUNTIME_VALUE_ERROR_TRACE`
- `XRUNTIME_VALUE_ERROR_ROOTS`

下层对象、callable、弱引用和 Value 错误作为原因链保留。分配失败保持 `XERR_MEMORY`，避免在 OOM 路径继续分配包装错误。

## 旧资产承接

保留并增强：

- 旧 `xvoCreateCallable` / `xvoGetCallable` / `xvoInvoke` 的完整调用路径。
- 旧 Future 运行时类型、Value 装箱和消费端引用生命周期能力。
- 类对象装箱后保持稳定身份与强生命周期。
- 弱引用 Value 的复制、移动、过期和竞争安全边界。
- callable/object 的 Take 与非 Take 两种所有权手感。

修订：

- 不再把类实例、动态字典、函数和任意句柄塞进公开 `xvalue` 联合体。
- 不再依赖公开类型标签和内部字段判断对象种类。
- 不再通过布尔 `bColloc` 参数表达所有权，改为函数名明确区分普通构造与 `Take`。
- 对象与 callable 分开裁剪；弱引用也不强制引入调用系统。
- Value Graph 不会悄悄共享可变对象冒充深拷贝。

## 范例

- `examples/runtime/value_object/main.c`
- `examples/runtime/value_callable/main.c`
- `examples/runtime/value_future/main.c`
- `examples/runtime/value_weak/main.c`
- `examples/runtime/value_type/main.c`
- `examples/runtime/value_trace/main.c`
- `examples/runtime/value_roots/main.c`
