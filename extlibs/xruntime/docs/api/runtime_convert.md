# 运行时类型转换

`runtime_convert` 为 XRT 的运行时类型描述提供可检查、失败原子且宽度安全的标量转换。它只依赖 `runtime_type`，不会把字符串、数值文本或时间文本实现带入最小运行时。

`runtime_convert_string` 是独立裁剪的文本扩展，依赖 `runtime_convert`、`runtime_type_string`、`number_integer`、`number_float` 和 `time_text`。

`value_convert` 是动态 `xvalue` 到运行时标量的轻量组合层，只依赖 `runtime_convert` 和 `value`；`value_convert_string` 再按需接入严格文本转换。它们不依赖 `typed_value`、类型容器或运行时对象图。

```c
#include <xrt/runtime_convert.h>
```

## 模式

`xtypeconvertmode` 逐级包含前一种模式的能力：

| 常量 | 契约 |
| --- | --- |
| `XTYPE_CONVERT_EXACT` | 只允许相同且可复制的类型，执行该类型的复制操作 |
| `XTYPE_CONVERT_WIDEN` | 额外允许来源类型全部有效值都能被目标无损表示的方向 |
| `XTYPE_CONVERT_EXPLICIT` | 额外允许经过运行时范围检查的缩窄和跨标量转换 |

`xrtTypeCanWiden(source, target)` 判断类型级无损方向。它检查完整整数宽度和浮点有效位，不把 `int64 -> float64`、`int32 -> float32` 等可能丢失精度的方向标记为无损。

`xrtTypeCanConvert(source, target, mode)` 判断指定模式是否存在内建路径。返回 `false` 既可能表示关系不支持，也可能表示参数错误；只有参数或类型描述无效时设置线程错误。

`xrtTypeConvert(source_type, source, target_type, target, mode)` 执行转换。`target` 必须已经按目标类型初始化；失败时保持目标原值。不同类型的来源与目标字节范围不得重叠。同类型转换调用 `xrtTypeCopyValue`，因此遵循类型操作的深复制和失败原子性契约。

## 核心转换

不启用文本扩展时支持：

- `bool` 与 `bool32` 双向无损转换，并可继续拓宽到定宽整数和浮点；写入 `bool32` 时规范化为 `0` 或 `1`。
- 有符号、无符号整数之间按真实 `1/2/4/8` 字节宽度转换。
- 整数到浮点的类型级无损判断，以及显式模式下允许的精度缩窄。
- `float32 -> float64` 无损拓宽；`float64 -> float32` 显式转换拒绝有限值溢出。
- 浮点到整数向零截断，并在 C 强制转换前拒绝 NaN、无穷和越界值。
- `time` 与整数之间的显式转换。
- `null` 到数值、时间和指针；`pointer` 到 `bool`。
- `type` 标识按无符号 64 位值参与显式数值转换。

核心层不提供整数到指针或文本到指针转换，避免把进程地址构造隐藏在通用值转换中。

## 文本扩展

文本扩展只绑定 `xrtTypeString()` 描述的拥有型 `str`，不会按 `Kind == XRT_TYPE_STRING` 接管应用自定义字符串类型。

字符串到标量采用完整、严格解析：

- 布尔只接受 ASCII 大小写不敏感的 `true`、`false`，以及精确的 `1`、`0`。
- 整数固定十进制，不接受空白、进制前缀或数字分隔符。
- 浮点接受完整十进制文本，以及规范的 `NaN`、`Infinity` 特殊值。
- 时间使用 `xrtTimeParseAny` 支持的完整时间格式。
- 解析为 64 位中间值后仍按目标实际宽度执行范围检查。

标量到字符串返回目标槽拥有的新字符串：

- `null` 和空指针输出 `null`。
- 布尔输出 `true` 或 `false`。
- 整数和类型标识输出十进制。
- 浮点使用最短往返表示。
- 时间输出 UTC RFC 3339。
- 非空指针输出带 `0x` 前缀的十六进制地址。

`xrtTypeFormat(type, value, writer, context)` 是底层流式入口。内建标量使用固定栈缓冲，
不会创建中间拥有型字符串；writer 收到的分块只在该次回调期间有效。writer 返回
`false` 后立即停止，已有的部分输出不回滚。若 writer 没有设置错误，转换层补充稳定的
`XTYPE_CONVERT_ERROR_OPERATION`。

自定义类型可设置 `xrttypeops::Format`。该回调可以分多次调用 writer，但不得保存
writer、context 或借用分块供返回后使用。存在该回调时，
`xrtTypeCanConvert(custom, xrtTypeString(), XTYPE_CONVERT_EXPLICIT)` 返回 `true`，并由
`xrtTypeConvert` 复用相同格式化路径。

`xrtTypeToString(type, value)` 是高层拥有型入口，返回由 `xrtFree` 释放的零结尾 UTF-8
字符串。它在流式结果之上只为最终输出分配内存。`xrtTypeConvert` 转换到
`xrtTypeString()` 时先完整构建新字符串，再原子替换目标旧值；格式化失败或 OOM 不会
释放或修改目标原值。

不透明的 callable、future、容器、record 等类型默认没有文本表示，也不会把进程地址
作为后备文本泄露出去。需要文本能力的自定义类型应显式提供 `xrttypeops::Format`。

## 动态 Value 适配

`xrtValueConvertTo(source, target_type, target, mode)` 把动态 Value 标量映射为规范来源类型，再复用 `xrtTypeConvert`。目标必须已经初始化，失败时保持原值：

| Value 类型 | 规范来源类型 |
| --- | --- |
| `XVALUE_NULL` | `xrtTypeNull()` |
| `XVALUE_BOOL` | `xrtTypeBool()` |
| `XVALUE_INT` | `xrtTypeInt64()` |
| `XVALUE_FLOAT` | `xrtTypeFloat64()` |
| `XVALUE_TIME` | `xrtTypeTime()` |
| `XVALUE_POINTER` | `xrtTypePointer()` |
| `XVALUE_STRING` | `xrtTypeString()`，仅在文本扩展启用时 |

因此动态 `int` 在 Exact 模式下只匹配 `int64`；转为 `int8`、`int16` 或 `int32` 必须使用 Explicit 并通过实际值范围检查。`int64 -> float64` 也不是类型级无损拓宽，即使某个具体值恰好可精确表示。

动态字符串直接借用 Value 已保证的末尾零，不为解析创建临时副本。包含内嵌零的 Value 字符串不能由 `str` 完整表达，会在解析或复制前拒绝。字节、句柄和四种动态容器没有统一标量意义，不进入该 API；同语义类型解码、应用自定义记录以及容器桥接继续使用 `xrtValueToTyped`。

```c
xvalue* Source = xrtValueString(XRT_STR_LITERAL("120"));
int8 Value = 0;

if ( !xrtValueConvertTo(
	Source, xrtTypeInt8(), &Value, XTYPE_CONVERT_EXPLICIT
) ) {
	xrtValueRelease(Source);
	return false;
}
xrtValueRelease(Source);
```

## 错误

错误域固定为 `xrt.type-convert`。`xtypeconverterror` 包含：

| 常量 | 含义 |
| --- | --- |
| `XTYPE_CONVERT_ERROR_ARGUMENT` | 空参数或重叠范围 |
| `XTYPE_CONVERT_ERROR_MODE` | 无效转换模式 |
| `XTYPE_CONVERT_ERROR_TYPE` | 无效类型描述或不支持的类型关系 |
| `XTYPE_CONVERT_ERROR_RANGE` | 来源值不能由目标表示 |
| `XTYPE_CONVERT_ERROR_PARSE` | 文本不能被完整解析 |
| `XTYPE_CONVERT_ERROR_OPERATION` | 类型复制或文本格式化下层操作失败 |

文本解析和格式化会保留下层错误作为原因；若错误来自 OOM，顶层错误种类仍为
`XERR_MEMORY`。自定义 `Format` 或 writer 静默返回 `false` 时，转换层发布
`xrt.type-convert`/`XTYPE_CONVERT_ERROR_OPERATION`，避免失败丢失异常信息。

## 裁剪

- `XRUNTIME_FEATURE_RUNTIME_CONVERT`：只启用固定标量转换核心。
- `XRUNTIME_FEATURE_RUNTIME_CONVERT_STRING`：启用可选文本转换并自动选择其直接依赖。
- `XRUNTIME_FEATURE_VALUE_CONVERT`：启用动态 Value 的非文本标量适配。
- `XRUNTIME_FEATURE_VALUE_CONVERT_STRING`：为动态 Value 适配启用文本解析、复制和格式化。

裁剪测试分别验证两个根的正向依赖闭包，并验证省略任何直接依赖都会编译失败。

## 范例

- `examples/runtime/convert/main.c`
- `examples/runtime/convert_string/main.c`
- `examples/runtime/value_convert/main.c`
- `examples/runtime/value_convert_string/main.c`
