# Number

数值模块把整数和 IEEE-754 `double` 拆成两个独立裁剪单元，两者都只依赖 `core`：

```c
#define XRT_FEATURE_NUMBER_INTEGER
#define XRT_FEATURE_NUMBER_FLOAT
```

基础写入和解析路径不分配内存、不依赖当前区域设置，并接收显式长度。`String` 结尾的便捷函数才会分配返回文本。

展示格式是第三个可选层，同时依赖整数和浮点模块：

```c
#define XRT_FEATURE_NUMBER_FORMAT
```

## 选择边界

| 需求 | 入口 |
| --- | --- |
| 把整数按显式基数写入文本 | `xrtIntWrite` / `xrtUIntWrite` |
| 写出最短可往返 `double` | `xrtNumWrite` |
| 生成拥有型基础数字文本 | 对应 `String` 入口 |
| 严格解析完整字段 | `xrtIntParse` / `xrtUIntParse` / `xrtNumParse` |
| 生成宽度、分组、精度等展示文本 | 对应 `FormatTo` / `Format` 入口 |

三个 `Parse` 入口都要求完整消费传入视图。Number 不保留旧 JNUM 的前缀解析器，也不会把非法输入和数值零折叠为同一个结果。在 CSV、DSL 或协议流中扫描 token 时，词法层应先确定 token 边界，再把精确子视图交给 Number；不能用成功解析前缀来证明整个字段合法。

JSON 数字由 JSON 词法层施加更窄语法，再复用精确数值转换。`true`、`false` 和 `null` 不是数字，也不会由 Number 解析。十进制整数超出 `uint64` 或 `int64` 时返回范围错误，不会静默退化为可能丢失精度的 `double`。需要任意精度整数时应保留原始文本或接入独立 BigInt 实现。

## 整数

`number_integer` 保留旧版 `jnum` 两位十进制查表的高效思路，但不保留错误契约。旧 `xrtU32ToStr` / `xrtU64ToStr` 的名称与十六进制行为不一致，旧 `xrtI64ToStr` 对 `INT64_MIN` 取负会产生未定义行为，旧 `xrtStrTo*` 还会把非法输入和数值零折叠成同一个结果；新 API 均已消除这些边界。

### 写入

```c
bool xrtUIntWrite(
	uint64 value,
	uint32 base,
	char* output,
	size_t capacity,
	size_t* outputSize,
	uint32 flags
);

bool xrtIntWrite(
	int64 value,
	uint32 base,
	char* output,
	size_t capacity,
	size_t* outputSize,
	uint32 flags
);
```

`base` 支持 2 到 36。传入 `output == NULL && capacity == 0` 时只查询文本长度；实际写入时容量必须额外包含末尾零字节。容量不足会通过 `outputSize` 返回所需长度，并保持输出缓冲不变。

- `XNUMBER_UPPER`：使用大写字母数字。
- `XNUMBER_PREFIX`：为二、八、十六进制添加 `0b`、`0o`、`0x`；其他基数报告配置错误。
- `XNUMBER_PLUS`：为非负值添加 `+`。

`xrtUIntString` / `xrtIntString` 分配末尾补零文本，返回值由 `xrtFree` 释放。

### 解析

```c
bool xrtUIntParse(xstrview text, uint32 base, uint32 flags, uint64* value);
bool xrtIntParse(xstrview text, uint32 base, uint32 flags, int64* value);
```

默认要求完整文本合法，不跳过空白、不识别前缀、不允许分隔符。失败不会修改结果。

- `XNUMBER_PARSE_SPACE`：允许两端 SP、HT、VT、FF、CR、LF。
- `XNUMBER_PARSE_PREFIX`：允许 `0b`、`0o`、`0x`。`base == 0` 时据此选择基数，否则前缀必须与显式基数一致。
- `XNUMBER_PARSE_SEPARATOR`：允许两个数字之间的 `_`。

无符号解析拒绝正负号。有符号解析完整覆盖 `INT64_MIN` 到 `INT64_MAX`。自动基数默认仍是十进制，文本 `077` 不会被隐式解释为八进制。

## 浮点

`number_float` 保留旧版适合动态宿主和跨平台 C 的使用语义：

- 整数型 `double` 默认保留 `.0`；
- 保留 `-0.0` 的符号；
- 使用小写 `e` 和显式指数符号；
- 稳定写出 `nan`、`inf`、`-inf`。

旧版浮点核心不能保证任意 IEEE-754 位模式往返，且解析存在严格别名和静默截断问题。新实现的语法层由 XRT 编写，正确舍入内核精炼自 yyjson 的 Eisel-Lemire、固定栈 BigInt 和 Schubfach 实现。它不依赖 libc `strtod`、`printf` 或当前区域设置。

### 写入

```c
bool xrtNumWrite(
	double value,
	char* output,
	size_t capacity,
	size_t* outputSize,
	uint32 flags
);

str xrtNumString(double value, uint32 flags);
```

`xrtNumWrite` 写出最短往返有效数字。查询、容量和失败原子契约与整数写入一致。基础路径最多使用 40 字节栈空间。

- 默认：`1.0`、`-0.0`、`1e+21`。
- `XNUMBER_FLOAT_COMPACT`：整数型值省略 `.0`，例如 `1`、`-0`。

`xrtNumString` 分配返回文本，调用方使用 `xrtFree` 释放。

### 解析

```c
bool xrtNumParse(xstrview text, uint32 flags, double* value);
```

默认语法为：

```text
[+-]? ( digits [ "." [digits] ] | "." digits ) [ [eE] [+-]? digits ]
```

解析必须消费完整 `xstrview`。它接受 `+1.5`、`.5`、`1.`，但默认拒绝空白、分隔符和特殊值。协议层可以先施加 JSON 等更窄语法，再复用同一数值转换。

- `XNUMBER_PARSE_SPACE`：允许两端 ASCII 空白。
- `XNUMBER_PARSE_SEPARATOR`：允许尾数和指数中两个数字之间的 `_`。
- `XNUMBER_PARSE_SPECIAL`：不区分 ASCII 大小写地允许 `nan`、`inf`、`infinity`。

任意长尾数均不分配内存。实现只保留正确舍入所需的 769 位高位有效数字，并把更低的非零信息折叠为 sticky 位。有限文本向零下溢时成功返回带正确符号的零；向无穷溢出时返回 `XNUMBER_ERROR_RANGE` 并保持输出不变。

## 展示格式

`number_format` 复用整数写出和精确 double 底座，提供调用方缓冲区与一次分配两层 API：

```c
bool xrtIntFormatTo(int64 value, xstrview format,
	char* output, size_t capacity, size_t* outputSize);
bool xrtUIntFormatTo(uint64 value, xstrview format,
	char* output, size_t capacity, size_t* outputSize);
bool xrtNumFormatTo(double value, xstrview format,
	char* output, size_t capacity, size_t* outputSize);

str xrtIntFormat(int64 value, xstrview format);
str xrtUIntFormat(uint64 value, xstrview format);
str xrtNumFormat(double value, xstrview format);
```

三个 `To` 接口都支持 `output == NULL && capacity == 0` 的精确长度查询。容量不足时通过 `outputSize` 返回所需长度，目标保持不变；实际容量必须包含末尾零。三个分配接口返回由 `xrtFree` 释放的字符串。

### 格式语法

```text
[+|-][#][0][width][,|_][.precision][type]
```

字段顺序固定，未知字符、多余字符、缺失的精度数字和溢出的数字字段都会返回 `XNUMBER_ERROR_FORMAT`，不会像旧实现一样静默忽略。

- `+`：非负值也显示正号；`-` 显式选择默认符号规则。
- `#`：非十进制整数显示 `0b`、`0o`、`0x` 前缀；`g/G` 保留小数点和末尾零。
- `0`：在符号和进制前缀之后补零。
- `width`：最小总宽度；没有 `0` 时在左侧补空格。
- `,`：十进制每三位分组。
- `_`：十进制每三位、二/八/十六进制每四位分组。
- `.precision`：浮点精度，范围为 0 至 1000；整数格式不接受精度。

整数类型：

| 类型 | 含义 |
| --- | --- |
| `d` 或省略 | 十进制 |
| `x` / `X` | 小写/大写十六进制 |
| `o` | 八进制 |
| `b` / `B` | 二进制；大写形式影响可选前缀 |
| `c` | 把整数解释为 Unicode 标量并写出 UTF-8 |

`c` 只接受可选的最小宽度，宽度不足时在左侧补空格。符号、`#`、补零、分组和精度均返回 `XNUMBER_ERROR_FORMAT`。负数、`U+0000`、代理区以及大于 `U+10FFFF` 的值生成空文本；这样零结尾字符串不会包含不可表达的内嵌零字节。

浮点类型：

| 类型 | 精度含义 | 未指定精度 |
| --- | --- | --- |
| 省略 | 未指定精度时是最短精确往返文本；指定精度后等同 `f` | 最短往返 |
| `f` / `F` | 小数位数 | 6 |
| `e` / `E` | 科学计数的小数位数 | 6 |
| `g` / `G` | 有效数字数；零按一位处理 | 6 |
| `%` | 小数点精确右移两位后追加 `%`，精度是小数位数 | 6 |

大小写类型同时控制指数标记和 `INF` / `NAN`。负零保留符号；NaN 的 IEEE-754 符号位被忽略，以保证不同编译器和平台输出一致。`%` 直接移动精确值的小数点，不先执行一次可能溢出或再次舍入的二进制 `value * 100.0`。

浮点展示先把有限 double 精确展开为十进制，再执行 IEEE-754 最近偶数舍入。它不调用 `printf`、`strtod` 或区域设置，因此小数点、分组、指数和舍入结果不随进程 locale 或 C 运行库变化。内部固定工作区覆盖 `DBL_MAX`、最小次正规数和 1000 位精度，不发生堆分配。

```c
str sCount = xrtIntFormat(
	INT64_C(-123456789), XRT_STR_LITERAL(",d"));
str sBits = xrtUIntFormat(
	UINT64_C(0xDEADBEEF), XRT_STR_LITERAL("#_X"));
str sCharacter = xrtIntFormat(
	20320, XRT_STR_LITERAL("c"));
str sPrice = xrtNumFormat(
	1234567.895, XRT_STR_LITERAL(",.2f"));
str sRatio = xrtNumFormat(
	0.125, XRT_STR_LITERAL(".1%"));

/* -123,456,789 / 0XDEAD_BEEF / 1,234,567.90 / 12.5% */
xrtFree(sCount);
xrtFree(sBits);
xrtFree(sPrice);
xrtFree(sRatio);
```

## 错误

错误域为 `xrt.number`：

- `XNUMBER_ERROR_CONFIG`：不支持的基数或标志。
- `XNUMBER_ERROR_FORMAT`：文本语法错误或存在未消费字符。
- `XNUMBER_ERROR_RANGE`：整数溢出或有限浮点文本溢出为无穷。

空指针和无效视图使用公共参数错误。容量不足使用公共范围错误。

## 示例与测试

- `examples/number/integer/main.c`
- `examples/number/float/main.c`
- `examples/number/format/main.c`
- `tests/number/test_integer.c`
- `tests/number/test_float.c`
- `tests/number/test_float_long.c`
- `tests/number/test_format.c`
- `tests/number/test_format_property.c`
- `tests/number/test_integer_oom.c`
- `tests/number/test_float_oom.c`
- `tests/number/test_format_oom.c`
- `tests/single/test_single_number_integer.c`
- `tests/single/test_single_number_float.c`
- `tests/single/test_single_number_format.c`
