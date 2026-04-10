# JNUM 数值转换库

> 当前 `jnum` 的正式合同是“数字文本 token 解析 + 数值格式化”。它不是完整 JSON 字面量解析器，也不是字段级 schema 校验器。

[English](api-jnum.en.md) | [中文](api-jnum.md) | [返回索引](README.md)

---

## 目录

- [模块定位](#模块定位)
- [类型](#类型)
- [数字转字符串](#数字转字符串)
- [解析数字 token](#解析数字-token)
- [便捷转换函数](#便捷转换函数)
- [选型与边界](#选型与边界)
- [相关文档](#相关文档)

---

## 模块定位

`jnum` 主要解决 3 类问题：

1. 把已有 C 数值写成文本
2. 从一段更长文本里扫描一个数字 token
3. 在“输入受控、默认值可接受”的前提下，直接把文本转成 C 数值

它不直接负责：

- 解析 `true / false / null`
- 校验整段字段必须完全合法
- 业务层面的取值范围和 schema 验证


## 类型

### `jnum_type_t`

```c
typedef enum {
	JNUM_NULL,
	JNUM_BOOL,
	JNUM_INT,
	JNUM_HEX,
	JNUM_LINT,
	JNUM_LHEX,
	JNUM_DOUBLE,
} jnum_type_t;
```

说明：

- `JNUM_INT` / `JNUM_LINT`
	- 十进制有符号整数
- `JNUM_HEX` / `JNUM_LHEX`
	- 十六进制无符号整数
- `JNUM_DOUBLE`
	- 小数、指数形式，或超出有符号 64 位范围的十进制整数
- `JNUM_BOOL`
	- 枚举中存在，但当前 `xrtParseNum()` 主线不是 `true / false` 解析入口

### `jnum_value_t`

```c
typedef union {
	bool vbool;
	int32_t vint;
	uint32_t vhex;
	int64_t vlint;
	uint64_t vlhex;
	double vdbl;
} jnum_value_t;
```

读取规则：

- 必须先看 `jnum_type_t`
- 再决定读取联合体的哪个成员


## 数字转字符串

### `xrtI32ToStr()` / `xrtI64ToStr()`

```c
XXAPI int xrtI32ToStr(int32_t num, char* buffer);
XXAPI int xrtI64ToStr(int64_t num, char* buffer);
```

合同：

- 输出十进制有符号文本
- 返回写入长度，不含结尾 `\0`
- 由调用方提供缓冲区

### `xrtU32ToStr()` / `xrtU64ToStr()`

```c
XXAPI int xrtU32ToStr(uint32_t num, char* buffer);
XXAPI int xrtU64ToStr(uint64_t num, char* buffer);
```

合同：

- 输出带 `0x` 前缀的十六进制文本
- 使用小写十六进制字母
- 不是十进制无符号格式化函数

示例：

```c
char sBuf[32];

xrtU32ToStr(255, sBuf);       /* sBuf == "0xff" */
xrtU64ToStr(0x1234ULL, sBuf); /* sBuf == "0x1234" */
```

### `xrtNumToStr()`

```c
XXAPI int xrtNumToStr(double num, char* buffer);
```

合同：

- 输出可读、可 round-trip 的浮点文本
- 整数值形式的 `double` 也会保留 `.0`
- 特殊值会输出 `nan` / `inf` / `-inf`

示例：

```c
char sBuf[64];

xrtNumToStr(0.0, sBuf);   /* "0.0" */
xrtNumToStr(3.5, sBuf);   /* "3.5" */
xrtNumToStr(1e15, sBuf);  /* 可能是 "1000000000000000.0" */
```


## 解析数字 token

### `xrtParseNum()`

```c
XXAPI int xrtParseNum(const char* str, jnum_type_t* type, jnum_value_t* value);
```

合同：

- 从 `str` 当前位置开始解析一个数字 token
- 返回消费长度
- 失败时返回 `0`，并把 `type` 设为 `JNUM_NULL`

当前主线支持：

- 十进制整数：`42`、`-17`
- 十六进制整数：`0xFF`、`0x1234abcd`
- 浮点：`3.14`、`.5`
- 指数：`1e5`、`-2.5E-3`
- 前导符号：`+123`、`-456`

自动类型规则：

- 落在 `int32` 范围内：`JNUM_INT`
- 落在 `int64` 范围内：`JNUM_LINT`
- 十六进制 32 位：`JNUM_HEX`
- 十六进制 64 位：`JNUM_LHEX`
- 小数、指数或超出有符号 64 位的十进制：`JNUM_DOUBLE`

重要边界：

- 这是前缀解析器，不是整段字段校验器
- 如果你要确认整段都合法，必须比较消费长度

示例：

```c
const char* sToken = "123abc";
jnum_type_t eType = JNUM_NULL;
jnum_value_t sValue = {0};
int iLen = xrtParseNum(sToken, &eType, &sValue);

/* iLen == 3, eType == JNUM_INT, sValue.vint == 123 */
/* 如果要校验整段，必须再比较 iLen == strlen(sToken) */
```

### `xrtParseNumSkipSpace()`

```c
static inline int xrtParseNumSkipSpace(const void* str, jnum_type_t* type, jnum_value_t* value);
```

合同：

- 先跳过前导空白，再解析数字 token
- 返回长度包含被跳过的空白字符

这使它很适合：

- 流式扫描
- 在解析后直接 `p += iLen`

示例：

```c
jnum_type_t eType = JNUM_NULL;
jnum_value_t sValue = {0};
int iLen = xrtParseNumSkipSpace("   42", &eType, &sValue);

/* iLen == 5, 不是 2 */
```


## 便捷转换函数

### `xrtStrToI32()` / `xrtStrToI64()` / `xrtStrToU32()` / `xrtStrToU64()` / `xrtStrToNum()`

```c
XXAPI int32_t  xrtStrToI32(const void* pStr);
XXAPI int64_t  xrtStrToI64(const void* pStr);
XXAPI uint32_t xrtStrToU32(const void* pStr);
XXAPI uint64_t xrtStrToU64(const void* pStr);
XXAPI double   xrtStrToNum(const void* pStr);
```

合同：

- 内部基于 `xrtParseNumSkipSpace()`
- 接受前导空白
- 对解析出的类型做一次直接 C 数值转换
- 无法区分“无效输入”和“结果刚好是 0”

典型行为：

```c
xrtStrToI32("123")     /* 123 */
xrtStrToI32("123abc")  /* 123 */
xrtStrToI32("abc")     /* 0 */
xrtStrToU64("0xFF")    /* 255 */
```

使用建议：

- 受控输入、默认值为 `0` 也能接受：可以直接用 wrapper
- 需要严格校验或类型判断：回到 `xrtParseNum()` / `xrtParseNumSkipSpace()`


## 选型与边界

### 选型表

| 需求 | 推荐 API |
|------|----------|
| 十进制有符号格式化 | `xrtI32ToStr()` / `xrtI64ToStr()` |
| 十六进制无符号格式化 | `xrtU32ToStr()` / `xrtU64ToStr()` |
| 浮点格式化 | `xrtNumToStr()` |
| 在更长文本里扫描 token | `xrtParseNum()` |
| 输入前可能有空白 | `xrtParseNumSkipSpace()` |
| 受控输入快速转值 | `xrtStrTo*()` |

### 当前边界

- `xrtU32ToStr()` / `xrtU64ToStr()` 不是十进制无符号格式化函数
- `xrtParseNum()` 是前缀解析器，校验整段必须比较消费长度
- 当前 `xrtParseNum()` 不负责解析 `true / false / null`
- 十进制超出有符号 64 位后会转成 `JNUM_DOUBLE`
- 如果你需要识别“非法输入”和“结果为 0”，不要只用 `xrtStrTo*()`


## 相关文档

- [JNUM 教学页](../guide/jnum-intro.md)
- [JSON 处理](api-json.md)
- [XSON 处理](api-xson.md)
- [Value 动态类型](api-value.md)
- [主索引](README.md)
