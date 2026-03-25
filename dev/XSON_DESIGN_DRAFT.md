# XSON 设计文档

版本: `0.2`
状态: `draft`
更新时间: `2026-03-25`



## 1. 定位

`XSON` 是 `JSON` 的私有扩展格式，用于完整序列化和反序列化 `xvalue`。

设计定位如下：

- `JSON` 仍然是通用标准格式
- `XSON` 是面向 `xvalue` 的扩展格式
- 所有合法 `JSON` 文本都必须是合法 `XSON` 文本
- 所有合法 `JSON` 文本在 `XSON` 中的语义必须保持不变

因此：

- `xrtParseJSON()` 只处理 JSON
- `xrtParseXSON()` 处理 XSON
- `XSON` 可以读取纯 JSON
- `JSON` 不需要理解 XSON 扩展语法



## 2. 支持范围

`XSON` 当前支持以下 `xvalue` 类型：

- `null`
- `bool`
- `int`
- `float`
- `text`
- `time`
- `array`
- `list`
- `dict(table)`
- `set(coll)`
- `class`

`XSON` 明确不支持以下类型的序列化和反序列化：

- `ptr(point)`
- `func`
- `custom`

约束：

- 默认情况下，序列化时遇到不支持类型应返回错误
- 默认情况下，反序列化时遇到不支持类型语法应返回错误
- 可通过解析和序列化参数忽略不支持类型错误
- 不允许静默退化为 `null`



## 3. 总体语法

### 3.1 JSON 兼容

以下 JSON 写法在 XSON 中保持原义：

```xson
null
true
123
3.14
"hello"
[1, 2, 3]
{"name":"xrt","ok":true}
```


### 3.2 XSON 扩展前缀

XSON 在 JSON 基础上增加以下前缀语法：

- `array[ ... ]`
- `list[ ... ]`
- `dict{ ... }`
- `set{ ... }`
- `time( ... )`
- `class( ... )`

约束：

- 前缀和后续开启符之间不允许插入空白
- `array[`、`list[`、`dict{`、`set{`、`time(`、`class(` 视为完整语法入口
- 扩展前缀只在 `XSON` 入口生效



## 4. 各类型表示

### 4.1 `array`

显式写法：

```xson
array[1, 2, 3]
```

兼容写法：

```xson
[1, 2, 3]
```

空值：

```xson
[]
array[]
```

说明：

- `[]` 在 XSON 中固定表示空数组
- `array[...]` 强制按数组解析
- 不带前缀的非空 `[...]` 可通过判定规则识别为数组或列表


### 4.2 `list`

显式写法：

```xson
list[1:"a", 2:100, -5:time(2026-03-25 12:00:00)]
```

兼容写法：

```xson
[1:"a", 2:100, -5:time(2026-03-25 12:00:00)]
```

空值：

```xson
list[]
```

说明：

- `list` 是“整数索引到值”的映射结构，不是 JSON array 的别名
- 空列表必须写为 `list[]`
- 不带前缀的非空 `[...]` 若命中列表判定规则，则按 `list` 解析


### 4.3 `dict(table)`

显式写法：

```xson
dict{"name":"xrt","ok":true}
```

兼容写法：

```xson
{"name":"xrt","ok":true}
```

空值：

```xson
{}
dict{}
```

说明：

- `{}` 在 XSON 中固定表示空字典
- `dict{...}` 强制按字典解析
- 不带前缀的非空 `{...}` 可通过判定规则识别为字典或集合
- 字典键语法沿用 JSON object 的键语法


### 4.4 `set(coll)`

显式写法：

```xson
set{1, 2, "abc", time(2026-03-25 12:00:00)}
```

兼容写法：

```xson
{1, 2, "abc", time(2026-03-25 12:00:00)}
```

空值：

```xson
set{}
```

说明：

- `set` 是集合语义，不保留字典键值结构
- 空集合必须写为 `set{}`
- 不带前缀的非空 `{...}` 若命中集合判定规则，则按 `set` 解析


### 4.5 `time`

写法：

```xson
time(2000-01-02 12:00:00)
```

说明：

- `time(...)` 内部文本固定为 `YYYY-MM-DD HH:MM:SS`
- V1 不支持毫秒、时区偏移和其他时间字面量格式
- `time(...)` 可以出现在任意值位置，也可以作为根值


### 4.6 `class`

写法：

```xson
class(SGVsbG8=)
```

说明：

- `class(...)` 内部内容为结构体原始二进制的 `BASE64`
- `BASE64` 解码后的字节长度就是结构体数据长度
- `class(...)` 可以出现在任意值位置，也可以作为根值

边界：

- 当前 `class` 只定义为原始内存快照
- 不携带类型名、字段信息、ABI 信息和版本信息
- 仅保证“同构快照”可逆，不保证跨平台、跨 ABI、跨编译器可移植



## 5. 容器判定规则

### 5.1 显式前缀优先

如果存在显式前缀，则不再做歧义判断：

- `array[...]` 一定是 `array`
- `list[...]` 一定是 `list`
- `dict{...}` 一定是 `dict`
- `set{...}` 一定是 `set`


### 5.2 非空 `[...]` 的判定

对于不带前缀的非空 `[...]`：

- 如果首个顶层元素匹配 `int64:value`，则整个容器按 `list` 解析
- 否则按 `array` 解析

附加约束：

- 一旦按 `list` 进入解析，则所有顶层元素都必须满足 `int64:value`
- `list` 索引允许负数
- `list` 索引不允许浮点数


### 5.3 非空 `{...}` 的判定

对于不带前缀的非空 `{...}`：

- 如果首个顶层元素匹配 `key:value`，则整个容器按 `dict` 解析
- 否则按 `set` 解析

附加约束：

- 一旦按 `dict` 进入解析，则所有顶层元素都必须满足 `key:value`
- `dict` 的键规则沿用 JSON object


### 5.4 空容器的固定语义

空容器不参与歧义判断：

- `[]` 固定为 `array`
- `{}` 固定为 `dict`
- `list[]` 固定为 `list`
- `set{}` 固定为 `set`



## 6. 推荐输出规则

为保证可读性、稳定性和与 JSON 的共存，`xrtStringifyXSON()` 的推荐输出形式如下：

- `array` 输出为 `[...]`
- `dict` 输出为 `{...}`
- `list` 输出为 `list[...]`
- `set` 输出为 `set{...}`
- `time` 输出为 `time(YYYY-MM-DD HH:MM:SS)`
- `class` 输出为 `class(BASE64)`

说明：

- 解析器同时接受显式前缀写法和无前缀兼容写法
- 生成器优先输出无歧义形式
- 因此对 `list / set / time / class`，生成器应使用显式前缀



## 7. 根值规则

`XSON` 根值允许为任意单个值，包括：

- 标量
- `array`
- `list`
- `dict`
- `set`
- `time`
- `class`

示例：

```xson
time(2000-01-02 12:00:00)
```

```xson
set{1, 2, 3}
```

```xson
class(AAECAwQ=)
```



## 8. 与 JSON 的关系

`XSON` 与 `JSON` 的关系明确如下：

- `JSON` 是 `XSON` 的子集
- 纯 JSON 文本可以直接由 `xrtParseXSON()` 解析
- `xrtParseJSON()` 不要求支持 `XSON`
- `xrtStringifyJSON()` 不负责表达 `time / list / set / class`
- `xrtStringifyXSON()` 才负责完整表达这些扩展类型



## 9. 错误处理边界

以下情况必须报错：

- `list[...]` 中存在非 `int64:value` 元素
- `dict{...}` 中存在非法 `key:value` 元素
- `time(...)` 中时间文本不符合 `YYYY-MM-DD HH:MM:SS`
- `class(...)` 中内容不是合法 `BASE64`
- 序列化遇到 `ptr(point) / func / custom`
- 反序列化遇到未定义前缀类型

但允许通过参数改为“忽略不支持类型”模式。

建议增加类似 flag：

```c
#define XSON_F_IGNORE_UNSUPPORTED_ENCODE	0x0001
#define XSON_F_IGNORE_UNSUPPORTED_DECODE	0x0002
```

建议语义：

- `XSON_F_IGNORE_UNSUPPORTED_ENCODE`
	- 序列化遇到 `ptr(point) / func / custom` 时跳过该值，而不是整体失败
- `XSON_F_IGNORE_UNSUPPORTED_DECODE`
	- 反序列化遇到未支持类型前缀时跳过该值，而不是整体失败

建议约束：

- 根值如果是不支持类型，即使开启忽略模式，也应返回错误
- 容器内部元素若被跳过，不应自动补成 `null`
- `dict` 中被跳过的键值对应整体移除
- `set` / `array` / `list` 中被跳过的元素应直接省略
- “忽略不支持类型”只针对类型不支持，不应用于普通语法错误
- 例如括号不匹配、时间格式错误、BASE64 非法，仍应报错

不允许的行为：

- 把不支持类型自动转成字符串
- 把普通语法错误自动转成 `null`
- 在 `JSON` 入口偷偷接受 `XSON` 扩展



## 10. `class` 的边界说明

`class(BASE64)` 只表示原始字节数据，不表示可移植对象模型。

当前明确边界如下：

- 适合进程内快照、调试落盘、同 ABI 回灌
- 不承诺 Windows 和 Linux 间可直接互通
- 不承诺 32 位和 64 位间可直接互通
- 不承诺不同编译器打包布局一致
- 若结构体中包含指针，恢复结果仅是原始字节恢复，不代表指针目标可用

因此：

- `class` 当前是“二进制快照能力”
- 不是“可移植结构体协议”



## 11. API 边界

建议保留独立 XSON API：

```c
xvalue xrtParseXSON( str sText, size_t iSize );
xvalue xrtParseXSONEx( str sText, size_t iSize, uint32 iFlags );
xvalue xrtParseXSON_File( str sFile );
xvalue xrtParseXSON_FileEx( str sFile, uint32 iFlags );

char* xrtStringifyXSON( xvalue varVal, int bFormat, uint32 iFlags, size_t* pRetSize );
int xrtStringifyXSON_File( str sFile, xvalue varVal, int bFormat, uint32 iFlags );
```

说明：

- `JSON` API 和 `XSON` API 分离
- 默认入口保留无 flags 版本，带 flags 的能力通过 `Ex` 版本提供
- `XSON` 内部可以复用现有 JSON 的字符串、数字、格式化输出设施
- 但语法入口和类型语义保持独立



## 12. 最终结论

当前 XSON 的明确设计如下：

- `XSON` 是 `JSON` 的超集，并保持对 JSON 的完全兼容
- 扩展类型通过前缀表示：`array / list / dict / set / time / class`
- `[]` 固定为空数组，`{}` 固定为空字典
- `list[]` 固定为空列表，`set{}` 固定为空集合
- 非空无前缀 `[...]` 和 `{...}` 仍允许通过 `:` 规则判定为数组、列表、字典或集合
- `time` 使用 `time(YYYY-MM-DD HH:MM:SS)`
- `class` 使用 `class(BASE64)`
- `ptr(point) / func / custom` 不支持序列化和反序列化
- `class` 仅提供二进制快照能力，不承诺跨平台可移植
