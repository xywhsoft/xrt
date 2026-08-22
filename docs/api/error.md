# 错误 API

## 设计契约

`xerror` 是不可变、可跨线程持有的结构化错误对象。错误由通用类别、稳定域、模块代码、系统代码、操作名、UTF-8 消息、可选数据和原因链组成。通用类别用于跨模块控制流，域与代码用于模块精确判断，消息只用于展示。

Core 错误 API 不依赖容器、字符串模块或 printf 运行时。需要结构化附加数据的模块应为
`Data` 定义稳定格式，或在自己的结果对象中保存详细字段，不能要求调用方解析展示消息。
动态消息格式化是独立的 `error_format` 便利模块，不增加最小 Core 的代码体积。

## 通用类别

- `XERR_ARGUMENT`、`XERR_TYPE`、`XERR_VALUE`、`XERR_RANGE`：输入不符合契约。
- `XERR_STATE`：对象或运行时状态不允许当前操作。
- `XERR_MEMORY`：内存不足。
- `XERR_IO`、`XERR_NOT_FOUND`、`XERR_EXISTS`、`XERR_PERMISSION`：系统资源错误。
- `XERR_AGAIN`：当前无法推进，但对象仍有效，调用方可等待后重试。
- `XERR_TIMEOUT`、`XERR_CANCELLED`、`XERR_CLOSED`：等待终止或资源关闭原因。
- `XERR_PROTOCOL`、`XERR_UNSUPPORTED`、`XERR_INTERNAL`：协议、能力或内部不变量错误。

`XERR_NONE` 只表示没有错误，不能用于创建错误对象。

## 创建与所有权

### `xrtErrorBuild`

从 `xerrordesc` 创建错误。所有字符串都会复制，`Cause` 会增加引用。成功返回一个调用方持有的引用；失败返回 `NULL` 并设置当前错误。

### `xrtErrorCreate`

创建只包含类别、域、代码和消息的常用错误。

### `xrtErrorWrap`

创建带原因链的常用错误。传入的原因是只读借用，返回对象持有它自己的引用。

### `xrtErrorRef` 与 `xrtErrorFree`

`xrtErrorRef` 增加引用并返回可持有指针；`xrtErrorFree` 释放一个持有引用，允许传入 `NULL`。原因链按迭代方式释放，用户可控深度不会消耗等量 C 栈。`xrtGetError`、字段访问函数和查询函数返回的都是借用指针，跨越清错、替换当前错误或相关对象释放时必须先增加引用。

## 字段与原因链

`xrtErrorKind`、`xrtErrorDomain`、`xrtErrorCode`、`xrtErrorSystemCode`、`xrtErrorOperation`、`xrtErrorMessage`、`xrtErrorData` 和 `xrtErrorCause` 返回不可变字段。空错误指针返回零值或空字符串。

`xrtErrorIs` 沿原因链查找通用类别。`xrtErrorFind` 沿原因链精确匹配域和代码。两者返回借用指针，未找到时返回 `NULL`。

## 当前错误

每个执行上下文拥有一个当前错误。普通线程默认使用线程上下文；协程和任务调度器切换自己的错误槽，因此迁移协程不会污染承载线程或其他任务。原生线程退出时仍留在默认错误槽中的对象会自动释放，C 扩展线程不需要为防止泄漏而强制清错。

- `xrtGetError` 借用当前错误。
- `xrtTakeError` 取走当前引用并清空错误槽。
- `xrtSetError` 增加传入对象的引用并替换当前错误，传入 `NULL` 等价于清除。
- `xrtClearError` 清除并释放当前引用。

常见失败不需要手工创建、设置再释放临时对象：

```c
xrtSetErrorInfo(XERR_ARGUMENT, "app.config", 1, "path is empty");
```

需要动态消息时选择 `XRT_MODULE_ERROR_FORMAT`：

```c
#define XRT_MODULE_ERROR_FORMAT
#include <xrt.h>

xrtSetErrorFormat(XERR_NOT_FOUND, "app.config", 2,
	"file does not exist: %s", path);
```

两个 Helper 都会创建不可变错误并直接移交给当前上下文。`xrtSetErrorFormat` 使用 printf
规则，支持任意长度消息，拒绝具有写入副作用的 `%n`，并把 `NULL` 格式报告为参数错误。
格式化测量与写入和字符串格式化模块复用同一内部实现，不重复维护解析器。构造失败时保留
无分配的 `XERR_MEMORY`，不会发布部分消息。

成功操作不隐式清除旧错误。调用方只能在函数通过返回值报告失败后读取当前错误；需要长期保留时使用 `xrtTakeError` 或 `xrtErrorRef`。

## 错误处理器

`xrtSetErrorHandler` 安装一个进程级观察处理器。设置错误时，XRT 先更新当前错误，再使用并发一致的“回调+用户数据”快照通知处理器。回调只借用错误对象，可以读取或增加引用；处理器内部再次设置错误不会递归调用自身。

处理器在隔离的错误边界内执行。处理器调用的 XRT API 即使失败，或者处理器主动设置、清除、取走当前错误，回调返回后仍会恢复正在通知的主错误。处理器需要保留主错误时仍应调用 `xrtErrorRef`；通过 `xrtTakeError` 取得的引用由处理器自行释放。

处理器用于日志、调试和语言运行时桥接，不能替代函数返回值。并发替换处理器时，已经开始的通知可以使用替换前的完整快照；调用方必须让旧处理器的用户数据存活到全部在途回调结束。

## OOM 保证

参数、状态、范围和内存不足等核心错误使用静态不可变对象。即使自定义分配器
已经失败，`xrtMalloc()` 仍能无分配地设置 `XERR_MEMORY`，错误构造失败也
不会用第二次分配覆盖这个原因。

## 范例

```c
xerror* pCause = xrtErrorCreate(XERR_TIMEOUT, "app.net", 1, "connect timeout");
xerror* pError = xrtErrorWrap(pCause, XERR_IO, "app.client", 2, "request failed");

xrtErrorFree(pCause);
if ( pError == NULL ) {
	return 1;
}
xrtSetError(pError);
xrtErrorFree(pError);

if ( xrtErrorIs(xrtGetError(), XERR_TIMEOUT) != NULL ) {
	/* 根据原因链决定是否重试。 */
}
xrtClearError();
```

结构化原因链范例位于 `examples/core/error/main.c`，动态消息范例位于
`examples/core/error_format/main.c`。

## 旧版资产决策

旧版只保存线程局部 UTF-8 字符串，并通过 `bFree` 把消息所有权交给错误槽；
模块只能依赖消息文本，上层宿主也无法稳定区分错误类别。新版保留线程隔离、
设置/读取/清除的简短手感和进程级观察回调，替换为不可变引用对象、稳定域与
代码、系统代码、操作、机器数据和原因链。

旧 `test_base.h` 的字符串错误、UTF-16/UTF-32 转换入口不进入核心错误层；
字符转换由 Charset 完成，模块直接构造 UTF-8 结构化错误。当前回归额外覆盖
错误深复制、原因查询、并发线程隔离、线程退出析构、协程执行上下文和 OOM
无分配报告。
