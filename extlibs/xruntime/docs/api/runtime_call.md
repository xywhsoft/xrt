# Runtime Call

`runtime_call` 是 XRT 类型描述和动态值之上的轻量调用层。它承接旧版 callable 的签名、环境、统一入口和多返回值能力，但不公开 callable 内部布局，也不混入无法通用安全调用的原始 C ABI。

## 裁剪与依赖

启用宏：`XRUNTIME_FEATURE_RUNTIME_CALL`

直接依赖：`XRUNTIME_FEATURE_RUNTIME_TYPE`、`XRT_FEATURE_VALUE`

该模块不依赖 Value 容器、线程、任务、协程或网络。关键字参数使用名称和值的借用数组表达，调用方不需要为了传递 kwargs 构造字典。

## 调用帧

`xrtcallframe` 借用以下数据：

- `Self`：可选的动态接收者。
- `Signature`：入口看到的有效签名；`xrtCallableInvoke` 会在局部帧中补齐或规范化。
- `Arguments`：全部位置参数，超过固定形参的部分是 varargs。
- `KeywordNames` / `KeywordValues`：一一对应的关键字参数。
- `Context`：调用链自行解释的借用上下文。

`xrtCallFrameValidate` 检查数组完整性、空值、重复关键字、必需参数、重复传参、未知关键字以及 varargs/kwargs 标志。普通非空参数名和命名专用参数名都参与签名身份，并且必须唯一；名称为空的参数只能按位置传递。

`xrtCallFrameArgument` 和 `xrtCallFrameKeyword` 暴露原始传参视图。`xrtCallFrameParameter` 按有效签名的形参下标完成位置/关键字选择，是普通入口的首选读取方式；可选参数未提供时返回 `NULL`。关键字不存在时返回 `NULL` 且不设置错误，非法名称或非法帧会报告 `xrt.call` 错误。

## 调用结果

`xrtcallresult` 拥有其中的 `xvalue` 引用。零初始化和 `XRT_CALL_RESULT_INIT` 均为有效空结果。

- 前四个结果直接内联，不发生结果数组分配。
- 第五个及后续结果使用轻量指针数组，不创建额外的动态 Value 容器。
- `Set` / `Push` 增加引用。
- `SetTake` / `PushTake` 成功时转移引用并清空来源槽。
- `Clear` 释放值但保留容量，适合热路径复用。
- `Unit` 释放值和容量。
- `Move` 转移完整结果并清空来源。

结果只能替换现有下标或追加到末尾，不允许稀疏写入。这样可以避免旧版为填充空洞构造多个 null 值，也使返回数量始终明确。

## Callable

```c
xrtcallable* xrtCallableCreate(
	const xrtfunctionsig* pSignature,
	xrtcallproc pEntry,
	ptr pEnvironment,
	xrtcalldrop pDropEnvironment
);
```

callable 创建后不可变，引用计数是原子的。非空签名和签名引用的类型、参数及返回数组由声明方持有，生命周期必须覆盖 callable。环境由 callable 持有，最后一个引用释放时析构一次。

`xrtTypeCallable()` 返回拥有一个 `xrtcallable*` 强引用的稳定 C ABI 槽类型。`Copy/Clone` 增加引用后替换目标，`Move` 转移引用并清空来源，`Drop` 释放引用；比较和散列均使用进程内 callable 身份。类型名和 ABI 名分别为 `callable` 与 `xrt.callable`。类型描述属于 `runtime_call`，不会强制启用 Value callable 装箱；后者仍由 `runtime_value_callable` 独立裁剪。

`xrtCallableInvoke` 支持空帧，并按以下顺序工作：

1. 复制调用帧，把 callable 签名补入局部帧，并拒绝不匹配的显式帧签名。
2. 验证调用帧与有效签名。
3. 使用私有临时结果调用入口。
4. 入口失败时释放全部部分结果，并把入口错误包装为 `xrt.call` 原因链。
5. 有签名时检查精确返回数量。
6. 成功后一次性替换调用方结果。

因此，失败调用不会破坏调用方原有结果。入口可以并发和重入调用，但环境本身是否支持并发由入口实现负责。

## 原始 ABI

旧版 callable 同时保存 `cdecl`、`stdcall`、`fastcall` 指针和动态入口，但 C 无法在没有准确原型或 libffi 的前提下安全通用调用任意函数。本模块只提供一种类型明确的动态入口。编译器、绑定生成器或 FFI 模块应生成一个 `xrtcallproc` 适配器，再由适配器调用原始函数；这条边界避免未定义行为，也不把平台 ABI 成本施加给基础调用层。

## 错误

稳定错误域：`xrt.call`

错误代码：

- `XCALL_ERROR_CALLABLE`
- `XCALL_ERROR_SIGNATURE`
- `XCALL_ERROR_FRAME`
- `XCALL_ERROR_RESULT`
- `XCALL_ERROR_ENTRY`
- `XCALL_ERROR_REFERENCE`

入口返回 `false` 时应设置具体 XRT 错误。调用层会把该错误保留为原因；入口没有设置错误时会生成状态错误。

## 范例

完整可运行范例：`examples/runtime/call/main.c`

```c
xrtcallresult Result = XRT_CALL_RESULT_INIT;

if ( xrtCallableInvoke(pCallable, &Frame, &Result) ) {
	xvalue* pFirst = xrtCallResultGet(&Result, 0);
	/* 使用借用值。 */
}
xrtCallResultUnit(&Result);
```

## 旧资产承接

保留并增强的能力：

- 借用且不可变的函数签名。
- callable 持有环境并在最终释放时析构。
- 统一动态入口。
- 四项内联多返回值和溢出返回值。
- 位置参数、关键字参数及多返回值测试场景。

修订的历史问题：

- 非原子公开引用计数改为不透明原子引用对象。
- 公开可变入口、ABI 和环境字段改为创建后不可变。
- 布尔失败改为结构化错误及原因链。
- 动态数组 Value 溢出结果改为轻量指针数组。
- 稀疏结果和隐式 null 填充改为连续结果契约。
- 入口失败直接污染目标结果改为失败原子提交。
- 普通命名参数未参与签名身份及唯一性检查的问题已经修复。
