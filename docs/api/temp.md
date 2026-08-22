# 临时内存 API

## 分层模型

临时内存提供两层 API：`xtemparena` 是可显式放入对象、请求或协程上下文的原语；`xrtTemp`、`xrtTempCurrent` 和 `xrtTempClear` 使用当前执行上下文的默认 arena，覆盖常见的一行式临时分配。

每个原生线程通过 Windows FLS 或 POSIX pthread TLS 拥有独立默认 arena，并在线程退出时自动释放。协程和任务调度器通过内部上下文切换绑定自己的 arena，因此临时指针可以跨 yield 保持，但不会污染宿主线程。

## 分配策略

默认常规块为 4096 字节，请求对齐到 16 字节。对齐后大于 2048 字节的请求使用独立 spill 块；spill 在作用域结束或 reset 时立即释放。常规块供后续复用，reset 后默认最多保留 65536 字节，避免偶发峰值永久滞留。

`xrtTempAlloc(0)` 返回可释放但不可解引用的零尺寸临时地址。所有临时地址在所属作用域结束、arena reset、trim、unit 或执行上下文退出后失效。

显式 arena 不包含并发锁，同一时间只能由一个执行上下文操作。不同 arena 可以并发使用。

## 类型

### `xtempconfig`

`BlockSize` 是新常规块容量，`SpillLimit` 是进入独立大块的阈值，`RetainLimit` 是 reset 后常规块保留上限。前两项必须非零，`RetainLimit` 可以为零。

### `xtemparena`

可栈上分配的 arena 状态。显式调用 `xrtTempInit` 前无需预置字段；使用惰性 API 时必须零初始化。结束使用后调用 `xrtTempUnit`。

### `xtempmark`

保存作用域回退位置和不可复用的作用域标识。作用域必须在同一 arena 上严格后进先出结束。复制 mark 不会复制结束权；旧 mark 即使遇到相同嵌套深度也不能结束后续作用域。重复结束已经成功结束的同一个 mark 是幂等操作。

### `xtempinfo`

包含常规块数、spill 数、保留字节、当前/峰值用量、reset 次数和作用域深度。

## 函数

### `xrtTempInit` / `xrtTempUnit`

初始化和销毁显式 arena。已经初始化的对象必须先调用 `xrtTempUnit`，不得直接重复初始化。`xrtTempUnit` 会使所有地址和未结束 mark 立即失效。

### `xrtTempAlloc`

分配临时内存。失败返回 `NULL` 并设置结构化错误。

### `xrtTempDup` / `xrtTempStr`

把二进制数据或字符串视图复制到指定 arena。`xrtTempStr` 总会追加零字符，返回视图长度不包含该字符。零长度二进制和空字符串都返回有效临时地址。

### `xrtTempReset`

回收全部当前分配、释放 spill 并执行有界保留。存在活动作用域时返回 `false` 和 `XERR_STATE`。

### `xrtTempSecureReset` / `xrtTempSecureUnit`

安全版本先擦除全部常规块和 spill 块的完整用户区，再重置或释放 arena。密码握手、认证令牌等敏感临时数据应使用安全版本；普通解析工作区继续使用非安全版本，避免无意义的整块写零成本。

### `xrtTempTrim`

在没有活动分配、spill 和作用域时缩减常规块。参数是本次希望保留的容量，不修改配置中的 `RetainLimit`。

### `xrtTempBegin` / `xrtTempEnd`

建立和回退嵌套作用域。乱序结束返回 `false`，原作用域继续有效，调用方可以按正确顺序恢复。

### `xrtTempEndDup` / `xrtTempEndStr`

先保存子作用域结果，成功结束 mark，再把结果复制到同一 arena 的父作用域。这样解析器和构建器可以丢弃全部中间临时数据，只保留最终二进制或零结尾字符串。保存结果或结束作用域失败时 mark 仍然有效；父作用域分配失败时 mark 已经结束，函数返回 `NULL`。

### `xrtTempGet`

复制 arena 诊断信息，不分配内存。

### `xrtTempCurrent` / `xrtTemp` / `xrtTempClear`

当前执行上下文的便捷 API。常见短生命周期代码只需 `xrtTemp(size)`，在逻辑边界调用 `xrtTempClear()`。

## 旧版资产决策

旧版 arena 的常规块复用、大请求 spill、嵌套作用域、字符串结果提升、线程隔离、协程跨 yield 保留以及调试事件全部保留。新版把这些能力从线程运行时私有结构中提取为可公开嵌入的 `xtemparena`，默认上下文只作为上层便捷入口。

新版补充了有界保留、显式 `trim`、二进制与字符串复制、通用结果提升、完整诊断信息、算术溢出检查和不可复用的作用域标识。旧 mark 只校验嵌套深度，复制后可能错误结束未来同深度作用域；新版使用单调作用域编号消除该状态漏洞。

旧范例中关于固定槽位回绕和地址复用的展示不再保留。地址是否复用不是公共契约，而且会诱导调用方在 reset 后继续观察失效指针；新版范例只展示明确的生命周期、结果提升和主动归还容量。

## 范例

完整范例位于 `examples/memory/temp/main.c`，并由 `python tools/build.py --suite temp_memory` 自动编译运行。
