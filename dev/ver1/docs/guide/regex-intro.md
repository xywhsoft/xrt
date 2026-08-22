# Regex 入门：什么时候该用正则做验证、提取和路由，而不是手搓字符串扫描

> 目标：把 `xrtRegexCreate()`、`xrtRegexCreateFromBuilder()`、`xrtRegexIsMatch()`、`xrtRegexFind()`、`xrtRegexCaptures()`、`xrtRegexWhichCaptures()`、`xrtRegexSetCreate()`、`xrtRegexSetMatches()` 讲成第 3 阶段里“验证 / 提取 / 路由”这条正式主线。读完这页后，你应该明确知道：什么时候正则是最低成本方案，什么时候它已经不适合接管整段语法解析，以及单模式、命名捕获、多模式集合各自负责什么。

[返回教学文档](README.md)

---

## 1. 为什么 `regex` 要单独讲

学完 [JNUM 入门](jnum-intro.md)、[xvalue、JSON 与 XSON 入门](xvalue-json-intro.md) 和 [Template 入门](template-intro.md) 以后，真实项目里很快还会出现另一类需求：

- 校验一个字段像不像邮箱、路由、版本号
- 从一行文本里提取 `key=value`
- 在多条规则里判断“这条日志命中了哪一类”

这些问题如果都自己手搓：

- `strstr()`
- `strncmp()`
- 指针前移
- 一堆 `if`

短期能写，长期会越来越脆。

但反过来，如果你把整段协议、整段 JSON、整段 HTML 都交给正则，也会很快失控。

所以 `regex` 这页真正要解决的不是“怎么写花哨表达式”，而是：

- 什么时候该用正则
- 什么时候根本不该让正则接管
- XRT 当前 `regex` API 的真实边界是什么


## 2. 先把 8 个边界分清楚

### 2.1 正则适合做验证、提取、路由，不适合接管完整语法树

推荐你把 `regex` 的主要职责限制在这 3 类：

| 场景 | 适合度 |
|------|--------|
| 轻量字段校验 | 高 |
| 从局部文本里提取字段 | 高 |
| 在多条规则里判断分类 | 高 |
| 解析完整 JSON / HTML / 模板语法 | 低 |

所以：

- 一条路由规则
- 一条日志行
- 一个配置字段

很适合上正则。

但：

- 整个 JSON 文档
- 整个模板文件
- 完整协议状态机

就不该指望正则独立解决。


### 2.2 `xrtRegexCreate()` 适合快路径，`Builder` 适合可控创建

当前单模式入口有两条：

- `xrtRegexCreate()`
- `xrtRegexCreateFromBuilder()`

更稳的理解是：

| 入口 | 适合场景 |
|------|----------|
| `xrtRegexCreate("...")` | 快速创建、零 flags、NUL 结尾 pattern |
| `xrtRegexCreateFromBuilder()` | 需要 flags、pattern 长度、错误细节、定制分配器 |

这里最容易忽略的一个边界是：

- `xrtRegexCreate()` 失败时直接返回 `NULL`
- `xrtRegexCreateFromBuilder()` 失败时，`ppRegex` 仍可能拿到一个非 `NULL` 对象
	- 可以继续用 `xrtRegexGetErrorMsg()` / `xrtRegexGetErrorPos()` 读取错误
	- 之后仍要 `xrtRegexDestroy()`

还有一个很容易写错的点：

- `xrtRegexBuilderCreate(..., iPatternSize, ...)`
	- 这里的长度是显式长度
	- 不是很多 XRT 文本 API 里常见的 `0 = 自动计算`

也就是说，如果你需要明确知道：

- 错在什么位置
- 为什么编译失败

就不要只用 `xrtRegexCreate()`。


### 2.3 匹配 API 的返回值要按 `1 / 0 / <0` 三态理解

当前主线里，匹配相关 API 最稳的判断方式是：

- `1`
	- 命中
- `0`
	- 没命中
- `< 0`
	- 错误

这适用于：

- `xrtRegexIsMatch()`
- `xrtRegexFind()`
- `xrtRegexCaptures()`
- `xrtRegexWhichCaptures()`
- `xrtRegexSetIsMatch()`
- `xrtRegexSetMatches()`

所以不要偷懒只写：

```c
if ( xrtRegexFind(...) ) {
	/* 省略了错误分支 */
}
```

更稳的写法是：

- `> 0`
	- 命中
- `== 0`
	- 未命中
- `< 0`
	- 错误


### 2.4 `IsMatch / Find / Captures / WhichCaptures` 解决的是 4 个不同问题

推荐你这样拆：

| API | 适合问题 |
|-----|----------|
| `xrtRegexIsMatch()` | 只想知道“有没有命中” |
| `xrtRegexFind()` | 想要整体命中范围 |
| `xrtRegexCaptures()` | 想要所有捕获组的范围 |
| `xrtRegexWhichCaptures()` | 想知道哪些捕获组真的命中了 |

如果你只想做校验，就别一上来就拿 `Captures()`。  
如果你要提取字段，就不要只停在 `IsMatch()`。


### 2.5 捕获槽 `0` 永远是整段命中，命名组从 `1` 开始

这是当前 `regex` 最值得先记牢的规则。

当前主线里：

- `capture[0]`
	- 整段命中范围
- `capture[1...]`
	- 才是分组

而 `xrtRegexCaptureCount()` 返回的数量也包含这个整段槽位。  
例如一个有 2 个分组的模式：

- `xrtRegexCaptureCount()`
	- 会返回 `3`

另外：

- `xrtRegexCaptureName(pRegex, 0, ...)`
	- 返回的是空名字
- 命名组的名字从 `1` 开始对应

当前命名组写法两种都能用：

- `(?P<name>...)`
- `(?<name>...)`


### 2.6 flags 可以走 `Builder`，也可以走模式内联，但教学更推荐 Builder

当前公开 flags 包括：

- `XRT_REGEX_FLAG_INSENSITIVE`
- `XRT_REGEX_FLAG_MULTILINE`
- `XRT_REGEX_FLAG_DOTNEWLINE`
- `XRT_REGEX_FLAG_UNGREEDY`

模式内部也能用：

- `(?i)`
- `(?m)`
- `(?s)`
- `(?U)`

但正式教学和长期代码里更推荐优先用 `Builder` 设置 flags，因为：

- flags 更显式
- pattern 文本更容易复用
- 出问题时更容易排查


### 2.7 `RegexSet` 负责“哪条规则命中”，不负责 span 和捕获提取

`xregexset` 很适合：

- 日志分类
- 路由分类
- 危险词规则集合
- 批量规则筛选

但它解决的问题是：

- 哪几个 pattern 命中了

不是：

- 具体匹配到了哪一段
- 某个命中的捕获组是什么

所以如果你既要：

- 批量筛选
- 又要精细提取

通常做法是：

1. 先用 `RegexSet` 定位命中的规则
2. 再回到具体 `xregex` 做 `Find / Captures`


### 2.8 多线程里需要独立对象时，用 `Clone()` 或每线程独立创建

当前 API 提供了：

- `xrtRegexClone()`
- `xrtRegexSetClone()`

如果你的使用场景是：

- 多线程各自独立持有对象
- 想复用同一份编译结果语义

那就不要靠共享一个对象去赌实现细节，直接：

- clone
- 或每线程独立创建

这条规则和并发主线是连着的；如果你已经开始跨线程共享复杂对象，建议回看：

- [多任务总论](multitask-overview.md)


## 3. 最小 DEMO：先把“验证 + 命名提取”跑通

第一步先不要碰 `RegexSet`。  
先学会最常见的一条主线：

- 用一个模式校验文本
- 再把字段提出来

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

static void PrintSpan(const char* sText, xregexspan tSpan)
{
	printf("%.*s", (int)(tSpan.iEnd - tSpan.iBegin), sText + tSpan.iBegin);
}

int main(void)
{
	const char* sLine = "timeout=3000";
	xregex* pRegex = NULL;
	xregexspan arrCaps[3] = {0};
	int iRet = 0;

	xrtInit();

	pRegex = xrtRegexCreate("(?P<key>[A-Za-z_]+)=(?P<value>[0-9]+)");
	if ( pRegex == NULL ) {
		xrtUnit();
		return 1;
	}

	iRet = xrtRegexCaptures(pRegex, sLine, strlen(sLine), arrCaps, 3u);
	if ( iRet > 0 ) {
		size_t iNameSize = 0;
		printf("all   = ");
		PrintSpan(sLine, arrCaps[0]);
		printf("\n");

		printf("%.*s = ", (int)xrtRegexCaptureName(pRegex, 1u, &iNameSize) ? (int)iNameSize : 0, xrtRegexCaptureName(pRegex, 1u, &iNameSize));
		PrintSpan(sLine, arrCaps[1]);
		printf("\n");

		printf("%.*s = ", (int)xrtRegexCaptureName(pRegex, 2u, &iNameSize) ? (int)iNameSize : 0, xrtRegexCaptureName(pRegex, 2u, &iNameSize));
		PrintSpan(sLine, arrCaps[2]);
		printf("\n");
	}

	xrtRegexDestroy(pRegex);
	xrtUnit();
	return 0;
}
```

这段 DEMO 最关键的不是表达式本身，而是 3 条规则：

1. `capture[0]` 是整段命中。
2. 命名组名字要用 `xrtRegexCaptureName()` 取。
3. 捕获范围只是 `begin/end`，真正的文本切片仍然由你自己决定怎么打印。


## 4. 升级 DEMO：Builder flags、错误定位和 `RegexSet` 各讲一次

真正进入项目以后，最常见的升级是下面两条。

### 4.1 用 Builder 做大小写不敏感匹配，并保留错误细节

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
	xregexbuilder* pBuilder = NULL;
	xregex* pRegex = NULL;
	int iErr = 0;

	xrtInit();

	iErr = xrtRegexBuilderCreate(&pBuilder, "hello", 5u, NULL);
	if ( (iErr == 0) && (pBuilder != NULL) ) {
		xrtRegexBuilderSetFlags(pBuilder, XRT_REGEX_FLAG_INSENSITIVE);
		iErr = xrtRegexCreateFromBuilder(&pRegex, pBuilder, NULL);
		if ( (iErr == 0) && (pRegex != NULL) ) {
			printf("match = %d\n", xrtRegexIsMatch(pRegex, "HeLLo", strlen("HeLLo")));
		}
	}

	if ( pRegex != NULL ) {
		xrtRegexDestroy(pRegex);
	}
	if ( pBuilder != NULL ) {
		xrtRegexBuilderDestroy(pBuilder);
	}

	xrtUnit();
	return 0;
}
```

如果你想保留编译失败细节，模式写错时更推荐：

- `xrtRegexBuilderCreate()`
- `xrtRegexCreateFromBuilder()`

因为失败后你仍可能从对象里拿到：

- `xrtRegexGetErrorMsg()`
- `xrtRegexGetErrorPos()`


### 4.2 用 `RegexSet` 做分类，而不是提取

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
	const char* arrPatterns[] = {"error", "warn", "timeout"};
	const char* sText = "warn: network timeout";
	xregexset* pSet = NULL;
	uint32 arrIndexes[8] = {0};
	uint32 iCount = 0;
	int iRet = 0;

	xrtInit();

	pSet = xrtRegexSetCreate(arrPatterns, 3u);
	if ( pSet == NULL ) {
		xrtUnit();
		return 1;
	}

	iRet = xrtRegexSetMatches(pSet, sText, strlen(sText), arrIndexes, 8u, &iCount);
	if ( iRet > 0 ) {
		for ( uint32 i = 0; i < iCount; i++ ) {
			printf("matched pattern index = %u\n", arrIndexes[i]);
		}
	}

	xrtRegexSetDestroy(pSet);
	xrtUnit();
	return 0;
}
```

这段代码最想让你记住的是：

- `RegexSet` 回答的是“哪几条规则命中了”
- 不是“命中了哪一段”


## 5. 什么时候该继续用字符串函数，而不是先上正则

也要把边界说清楚。下面这些场景，先别急着上正则：

- 只是在固定前缀后面截一段
- 只是判断是否等于某个常量
- 只是按固定分隔符切割
- 规则非常稳定而且比正则更容易直接表达

例如：

- `strncmp("GET ", ...)`
- `strchr(line, '=')`
- 固定后缀检查

这些都不需要为了“统一风格”强行改成 regex。


## 6. 常见错误

### 6.1 用正则去解析整个 JSON / HTML / 模板

错。  
这已经超出了正则当前最合适的职责边界。

### 6.2 把 `xrtRegexCreate()` 和 `Builder` 路径混成一回事

错。  
快路径创建失败只会给你 `NULL`，而 Builder 路径可以保留错误细节。

### 6.3 忘了 `capture[0]` 是整段命中

错。  
第一个真正的分组从 `1` 开始。

### 6.4 想从 `RegexSet` 直接拿 span 或捕获组

错。  
`RegexSet` 只负责返回命中的规则索引。

### 6.5 在 `if ( xrtRegexFind(...) )` 里把错误和命中混在一起

不稳。  
更推荐按 `> 0 / == 0 / < 0` 三态分开处理。


## 7. 建议继续阅读

- [Regex API](../api/api-regex.md)
- [Template 入门：什么时候该用模板，而不是拼字符串](template-intro.md)
- [xvalue、JSON 与 XSON 入门](xvalue-json-intro.md)
- [最小 HTTP 服务](../case/minimal-http-service.md)
