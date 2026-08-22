# String 运行时类型

`runtime_type_string` 为 XRT 的拥有型 `str` 提供稳定运行时类型描述。启用宏为 `XRUNTIME_FEATURE_RUNTIME_TYPE_STRING`，依赖 `string`、`hash64` 和 `runtime_type`。

```c
#include <xrt/runtime_type_string.h>

const xrttype* xrtTypeString(void);
```

## 表示

该描述只表示以下 C ABI：

- 槽类型是 `str`。
- 非空值必须是零结尾字符串。
- 槽拥有字符串，最终由 `xrtFree` 释放。
- `NULL` 是规范空值，并且在比较和散列语义上与 `""` 相等。

借用字符串应使用 `xstrview` 或应用自定义类型描述，不能把借用指针放入 `xrtTypeString()` 槽。需要保留内嵌零或明确二进制长度时应使用 bytes 类型，不应使用 `str`。

| 字段 | 值 |
| --- | --- |
| `AbiName` | `xrt.string` |
| `Kind` | `XRT_TYPE_STRING` |
| C ABI 槽 | `str` |
| 标志 | `COPYABLE`、`REFERENCE`、`NULLABLE`、`FINAL`、`RELOCATABLE` |

## 所有权

- `Init` 把槽初始化为 `NULL`。
- `Copy` 和 `Clone` 创建独立字符串，成功后才替换目标旧值。
- `Move` 移交指针、清空来源并释放目标旧值。
- `Drop` 释放字符串并清空槽。
- OOM 时来源和目标均保持原值。

比较使用无符号字节词典序；散列使用确定性的 `xrtHash64` 内容散列。内容相等的独立字符串具有相同散列。

## 裁剪

基础 `runtime_type` 不依赖字符串模块。只有显式启用 `runtime_type_string` 才会引入字符串复制和内容散列。

`runtime_type_string_value` 是额外的组合层，依赖 `runtime_type_string + typed_value`。启用后，`xrtValueToTyped` 和 `xrtValueFromTyped` 可以在动态字符串与拥有型 `str` 之间复制转换。动态字符串含有内嵌零时转换失败，因为 `str` 无法无损表达该内容；这类数据应保留为动态字符串或使用 bytes 类型。

## 范例

- `examples/runtime/type_string/main.c`
