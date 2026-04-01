# JNUM 入门：什么时候该直接解析数字文本，而不是把所有输入都先塞进 `atoi / strtod / sprintf`

> 目标：把 `xrtI32ToStr()`、`xrtI64ToStr()`、`xrtU32ToStr()`、`xrtU64ToStr()`、`xrtNumToStr()`、`xrtParseNum()`、`xrtParseNumSkipSpace()`、`xrtStrTo*()` 讲成第 3 阶段里“数字文本边界”这条正式主线。读完这页后，你应该明确知道：什么时候只是格式化一个数字，什么时候是在扫描一个 token，什么时候必须检查“是否整段都解析完”，以及为什么 `xrtU64ToStr()` 看起来不像 `sprintf("%llu")`。

[返回教学文档](README.md)

---

## 1. 为什么 `jnum` 要单独讲

很多程序里，数字一开始都不是数字，而是文本：

- 配置文件里的 `8080`
- 命令行里的 `--timeout=30000`
- JSON 里的 `3.14`
- 日志或协议里的 `0xFF`

如果你只有一条主线程，这里真正的问题通常也不是“会不会等住线程”，而是：

- 你到底是在格式化数字
- 还是在扫描一段更长文本里的数字 token
- 还是在校验“整个字段必须是合法数字”

把这三件事混在一起，后面就很容易出现两类 bug：

1. 看起来能转，其实只吃掉了前缀。  
例如：`"123abc"` 被你当成了完整整数。

2. 看起来是“无符号转字符串”，其实输出根本不是十进制。  
例如：`xrtU64ToStr()` 输出的是 `0x...`。

所以 `jnum` 不是“又一组数字工具函数”。  
它真正解决的是：

- 数字文本边界
- 数字 token 识别
- 数字字符串格式化


## 2. 先把 8 个边界分清楚

### 2.1 格式化、解析、校验不是一件事

推荐你先硬拆成 3 类问题：

| 问题 | 典型 API |
|------|----------|
| 把已有 C 数值写成文本 | `xrtI32ToStr()` / `xrtI64ToStr()` / `xrtU32ToStr()` / `xrtU64ToStr()` / `xrtNumToStr()` |
| 从文本前缀里扫描一个数字 token | `xrtParseNum()` / `xrtParseNumSkipSpace()` |
| 把一整段受控输入直接转成某个 C 数值 | `xrtStrToI32()` / `xrtStrToI64()` / `xrtStrToU32()` / `xrtStrToU64()` / `xrtStrToNum()` |

如果你要的是“整段字段必须合法”，就不能只看有没有转出一个值，还要检查：

- 实际消费长度
- 是否等于整段 token 长度


### 2.2 `xrtU32ToStr()` / `xrtU64ToStr()` 输出的是十六进制，不是十进制

这是 `jnum` 最容易让人误会的点。

当前实现里：

- `xrtI32ToStr()` / `xrtI64ToStr()`
	- 输出十进制有符号文本
- `xrtU32ToStr()` / `xrtU64ToStr()`
	- 输出带 `0x` 前缀的十六进制文本

也就是说：

```c
xrtU64ToStr(255, sBuf);
```

得到的是：

```text
0xff
```

不是：

```text
255
```

如果你要的是十进制无符号展示，就不要直接拿 `xrtU64ToStr()` 当 `sprintf("%llu")` 替代品。


### 2.3 `xrtNumToStr()` 追求的是可读和可逆，不是固定格式模板

当前实现里，`xrtNumToStr()` 的输出更接近：

- 可读
- 能 round-trip
- 不强制固定为科学计数法

所以你会看到：

- `0.0` 输出成 `0.0`
- `1.0` 输出成 `1.0`
- `1e15` 可能展开成 `1000000000000000.0`
- 特殊值会输出成 `nan` / `inf` / `-inf`

如果你业务上要求的是：

- 固定小数位
- 固定科学计数法
- 固定宽度

那是格式模板问题，不是 `jnum` 的合同目标。


### 2.4 `xrtParseNum()` 是前缀解析器，不是“整段字段校验器”

`xrtParseNum()` 的返回值不是“成功 / 失败”，而是：

- 从当前位置开始，一共消费了多少字符

所以它更适合：

- 在更长文本里扫描一个数字 token
- 逐个前移指针

例如：

- `"123abc"`
	- 可以先读出 `123`
- `"0xFF,"`
	- 可以先读出 `0xFF`

但如果你要校验一个完整字段，就必须额外比较：

- `iLen == token_length`

否则“只吃掉前缀”也会被你误判成合法整段输入。


### 2.5 `xrtParseNumSkipSpace()` 的返回长度包含前导空白

这也是一个很容易写错的位置。

`xrtParseNumSkipSpace()` 会先跳过前导空白，然后再解析数字。  
但它返回的长度是：

- 跳过的空白长度
- 加上数字 token 长度

也就是说：

```c
int iLen = xrtParseNumSkipSpace("   42", &eType, &sValue);
```

这里 `iLen` 是：

- `5`

不是：

- `2`

这个设计很适合做流式扫描，因为你可以直接：

- `p += iLen`


### 2.6 `xrtStrTo*()` 是方便入口，不是严格校验入口

`xrtStrToI32()`、`xrtStrToI64()`、`xrtStrToU32()`、`xrtStrToU64()`、`xrtStrToNum()` 本质上都是：

- 先调用 `xrtParseNumSkipSpace()`
- 再按返回类型做一次 C 数值转换

这就带来两个非常重要的边界：

1. 无法区分“无效输入”和“结果确实是 0”  
例如：
	- `"abc"`
	- `"0"`
	- 最后都可能得到 `0`

2. 会接受前缀解析  
例如：
	- `xrtStrToI32("123abc")`
		- 会得到 `123`

所以这些 wrapper 更适合：

- 受控输入
- 默认值就是 0 也能接受的场景

不适合：

- 面向用户的严格字段校验
- 对“整段都要合法”有强要求的协议解析


### 2.7 `JNUM_BOOL` 在枚举里存在，但 `xrtParseNum()` 不是 `true/false` 解析器

当前 `jnum_type_t` 里确实有：

- `JNUM_BOOL`

但这不意味着：

- `xrtParseNum("true", ...)`
	- 就会返回布尔值

当前公开的 `xrtParseNum()` 主线是：

- 数字 token 解析器

它处理的是：

- 十进制整数
- 十六进制整数
- 浮点和指数形式

如果你要解析：

- `true / false / null`

那已经是：

- JSON / XSON 字面量问题

应该回到：

- [xvalue、JSON 与 XSON 入门](xvalue-json-intro.md)


### 2.8 十进制超出有符号 64 位后，会退到 `JNUM_DOUBLE`

当前 `jnum` 的十进制整数主线是：

- `JNUM_INT`
- `JNUM_LINT`

如果十进制值再大，当前实现会转成：

- `JNUM_DOUBLE`

这意味着：

- 很大的十进制 ID
- 很大的计数器

如果你还想“精确保留整数语义”，就不能随手把它当普通 `double` 或 wrapper 结果来用。

更稳的做法通常是：

- 保留原始文本
- 或明确用十六进制协议表示
- 或在上层业务里自己做更严格的整段校验策略


## 3. 最小 DEMO：先把 4 类数字格式化出来

第一步先不要碰解析。  
先把“已有数值 -> 字符串”这条线走通。

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	char sBuf[64];

	xrtInit();

	xrtI32ToStr(-42, sBuf);
	printf("i32  = %s\n", sBuf);

	xrtI64ToStr(9223372036854775807LL, sBuf);
	printf("i64  = %s\n", sBuf);

	xrtU64ToStr(0xfeedbeefULL, sBuf);
	printf("u64  = %s\n", sBuf);

	xrtNumToStr(3.5, sBuf);
	printf("num  = %s\n", sBuf);

	xrtNumToStr(1e15, sBuf);
	printf("num2 = %s\n", sBuf);

	xrtUnit();
	return 0;
}
```

这个 DEMO 最想让你先记住的只有两件事：

1. `I32/I64` 是十进制有符号格式化。
2. `U32/U64` 是带 `0x` 的十六进制格式化。


## 4. 升级 DEMO：把“前缀解析”和“整段校验”一起讲明白

真正进入 `jnum` 主线后，最重要的不是“能不能转出一个值”，而是：

- 你到底吃掉了多少字符
- 这是不是整个字段

下面这个 DEMO 把这件事一次讲清楚：

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

static const char* GetJNumTypeName(jnum_type_t eType)
{
	switch ( eType ) {
	case JNUM_NULL:		return "JNUM_NULL";
	case JNUM_BOOL:		return "JNUM_BOOL";
	case JNUM_INT:		return "JNUM_INT";
	case JNUM_HEX:		return "JNUM_HEX";
	case JNUM_LINT:		return "JNUM_LINT";
	case JNUM_LHEX:		return "JNUM_LHEX";
	case JNUM_DOUBLE:	return "JNUM_DOUBLE";
	default:			return "UNKNOWN";
	}
}

static void PrintParsedValue(jnum_type_t eType, const jnum_value_t* pValue)
{
	switch ( eType ) {
	case JNUM_BOOL:
		printf("%s", pValue->vbool ? "true" : "false");
		break;
	case JNUM_INT:
		printf("%d", pValue->vint);
		break;
	case JNUM_HEX:
		printf("0x%X", pValue->vhex);
		break;
	case JNUM_LINT:
		printf("%lld", (long long)pValue->vlint);
		break;
	case JNUM_LHEX:
		printf("0x%llX", (unsigned long long)pValue->vlhex);
		break;
	case JNUM_DOUBLE:
		printf("%.17g", pValue->vdbl);
		break;
	default:
		printf("(null)");
		break;
	}
}

int main(void)
{
	const char* arrTokens[] = {
		"42",
		"0x2a",
		"3.5",
		"123abc",
		"0x",
		"18446744073709551615",
	};
	int i;

	xrtInit();

	for ( i = 0; i < (int)(sizeof(arrTokens) / sizeof(arrTokens[0])); i++ ) {
		jnum_type_t eType = JNUM_NULL;
		jnum_value_t sValue = {0};
		int iLen = xrtParseNum(arrTokens[i], &eType, &sValue);
		int bFull = (iLen == (int)strlen(arrTokens[i]));

		printf("token = \"%s\"\n", arrTokens[i]);
		printf("  len  = %d\n", iLen);
		printf("  type = %s\n", GetJNumTypeName(eType));
		printf("  full = %s\n", bFull ? "yes" : "no");
		printf("  value = ");
		PrintParsedValue(eType, &sValue);
		printf("\n\n");
	}

	{
		jnum_type_t eType = JNUM_NULL;
		jnum_value_t sValue = {0};
		int iLen = xrtParseNumSkipSpace("   -17", &eType, &sValue);

		printf("skip-space token = \"   -17\"\n");
		printf("  len  = %d\n", iLen);
		printf("  type = %s\n", GetJNumTypeName(eType));
		printf("  value = ");
		PrintParsedValue(eType, &sValue);
		printf("\n");
	}

	xrtUnit();
	return 0;
}
```

这段代码最关键的观察点是：

- `"123abc"`
	- 能先读出 `123`
	- 但 `full = no`
- `"0x"`
	- 也不会被你自动当成“完整合法十六进制”
- `"18446744073709551615"`
	- 当前会落到 `JNUM_DOUBLE`
- `xrtParseNumSkipSpace("   -17", ...)`
	- 返回长度包含前导空格

这就是为什么“能转出一个值”和“整段字段合法”必须分开判断。


## 5. 什么时候该用 `xrtStrTo*()`，什么时候一定要回到 `xrtParseNum()`

这是最实际的选型问题。

### 5.1 可以直接用 `xrtStrTo*()`

满足下面这些条件时，用 wrapper 很顺：

- 输入来源受控
- 默认值就是 `0`
- 允许前导空白
- 不需要区分“无效输入”和“真实 0”

例如：

```c
int iPort = xrtStrToI32("8080");
double fRatio = xrtStrToNum("0.85");
```

### 5.2 必须回到 `xrtParseNum()`

如果你满足下面任意一条，就不要偷懒只用 wrapper：

- 要校验整段字段都合法
- 要在更长文本里逐个扫描 token
- 要知道这次到底是 `int / hex / double`
- 要对“不合法输入”和“结果刚好是 0”做区分

例如下面这类场景：

- 配置校验
- 协议字段解析
- CSV / DSL / 表达式扫描

都更适合：

- `xrtParseNum()`
- `xrtParseNumSkipSpace()`


## 6. 常见错误

### 6.1 把 `xrtU32ToStr()` / `xrtU64ToStr()` 当十进制格式化

错。  
当前合同是十六进制输出，带 `0x` 前缀。

### 6.2 只看“有没有解析出值”，不看消费长度

错。  
`"123abc"` 这类输入会让你误把前缀解析当成整段合法。

### 6.3 用 `xrtStrToI32("abc") == 0` 当“字段合法”的证明

错。  
wrapper 的 `0` 既可能是合法结果，也可能是无效输入后的默认值。

### 6.4 以为 `xrtParseNum("true", ...)` 会返回布尔值

错。  
当前主线里它是数字 token 解析器，不是 JSON 字面量解析器。

### 6.5 以为超大十进制还能一直保持精确整数语义

错。  
十进制超出有符号 64 位以后，当前实现会退到 `JNUM_DOUBLE`。


## 7. 建议继续阅读

- [JNUM API](../api/api-jnum.md)
- [xvalue、JSON 与 XSON 入门](xvalue-json-intro.md)
- [XSON 入门：什么时候该从 JSON 升级到完整 `xvalue` 序列化](xson-intro.md)
- [用 xvalue + json 构造配置系统](../case/json-config-system.md)
