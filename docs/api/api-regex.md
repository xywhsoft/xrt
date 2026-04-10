# Regex 正则表达式模块

> XRT 的 regex 模块当前以内置 `bbre` 引擎为实现基础，但公开接口已经收口为 XRT 自有 API。

[返回索引](README.md)

---

## 1. 定位

该模块面向以下场景：

- 文本匹配
- 子串搜索
- 捕获组提取
- 多模式批量扫描
- 配置、日志、HTTP 文本、模板片段等轻量规则识别

设计目标：

- 轻量
- 可嵌入
- 跨平台
- 公开 API 服从 XRT 命名和类型体系


## 2. 使用边界

当前主线里，`regex` 更适合：

- 轻量字段校验
- 局部文本提取
- 多规则分类

不适合直接接管：

- 完整 JSON / HTML / 模板语法解析
- 带状态机的协议解析

另外要先记住 4 条高频边界：

1. `xrtRegexCreate()` 失败时直接返回 `NULL`；如果你需要错误位置和错误消息，应该走 `Builder + xrtRegexCreateFromBuilder()`。
2. `xrtRegexCreateFromBuilder()` 失败时，`ppRegex` 仍可能拿到非 `NULL` 对象，此时可以继续调用 `xrtRegexGetErrorMsg()` / `xrtRegexGetErrorPos()`，然后仍要 `xrtRegexDestroy()`。
3. 匹配相关 API 应按 `1 = 命中`、`0 = 未命中`、`< 0 = 错误` 理解。
4. `xrtRegexCaptureCount()` 的数量包含 `capture[0]` 这个“整段命中”；命名捕获从 `1` 开始对应，`xrtRegexCaptureName(..., 0, ...)` 返回空名字。


## 3. 核心类型

### `xregex`

单个正则对象。

典型流程：

1. `xrtRegexCreate()` 或 `xrtRegexCreateFromBuilder()`
2. `xrtRegexIsMatch()` / `xrtRegexFind()` / `xrtRegexCaptures()`
3. `xrtRegexDestroy()`


### `xregexbuilder`

高级构建器。

适合：

- 显式传入 pattern 长度
- 设置标志位
- 指定自定义 allocator


### `xregexset`

多模式集合对象。

适合：

- 同时检查多条规则
- 批量扫描同一段文本


### `xregexsetbuilder`

多模式构建器。

适合：

- 先构建多个 `xregex`
- 再统一生成 `xregexset`


### `xregexspan`

匹配范围：

```c
typedef struct {
	size_t iBegin;
	size_t iEnd;
} xregexspan;
```

常用于：

- `xrtRegexFind()`
- `xrtRegexCaptures()`


### `xregexalloc`

自定义分配器：

```c
typedef ptr (*xregexallocproc)(ptr pUserData, ptr pMem, size_t iPrevSize, size_t iNextSize);

typedef struct {
	ptr pUserData;
	xregexallocproc procAlloc;
} xregexalloc;
```


## 4. 标志与错误码

### 3.1 标志位

```c
XRT_REGEX_FLAG_INSENSITIVE
XRT_REGEX_FLAG_MULTILINE
XRT_REGEX_FLAG_DOTNEWLINE
XRT_REGEX_FLAG_UNGREEDY
```

含义分别是：

- 不区分大小写
- 多行模式
- `.` 可匹配换行
- 非贪婪量词


### 3.2 错误码

```c
XRT_REGEX_ERR_MEM
XRT_REGEX_ERR_PARSE
XRT_REGEX_ERR_LIMIT
```

常见理解：

- `XRT_REGEX_ERR_MEM`
	- 内存不足
- `XRT_REGEX_ERR_PARSE`
	- pattern 解析失败
- `XRT_REGEX_ERR_LIMIT`
	- 编译或执行超出内部限制


## 5. 公开 API

### 4.1 单模式

```c
XXAPI xregex* xrtRegexCreate(const char* sPatternNt);
XXAPI int xrtRegexCreateFromBuilder(xregex** ppRegex, const xregexbuilder* pBuilder, const xregexalloc* pAlloc);
XXAPI void xrtRegexDestroy(xregex* pRegex);
XXAPI const char* xrtRegexGetErrorMsg(const xregex* pRegex);
XXAPI size_t xrtRegexGetErrorPos(const xregex* pRegex);

XXAPI int xrtRegexIsMatch(xregex* pRegex, const char* sText, size_t iTextSize);
XXAPI int xrtRegexFind(xregex* pRegex, const char* sText, size_t iTextSize, xregexspan* pOutSpan);
XXAPI int xrtRegexCaptures(xregex* pRegex, const char* sText, size_t iTextSize, xregexspan* pOutCaptures, uint32 iCaptureCount);
XXAPI int xrtRegexWhichCaptures(xregex* pRegex, const char* sText, size_t iTextSize, xregexspan* pOutCaptures, uint32* pOutCapturesDidMatch, uint32 iCaptureCount);
XXAPI int xrtRegexIsMatchAt(xregex* pRegex, const char* sText, size_t iTextSize, size_t iPos);
XXAPI int xrtRegexFindAt(xregex* pRegex, const char* sText, size_t iTextSize, size_t iPos, xregexspan* pOutSpan);
XXAPI int xrtRegexCapturesAt(xregex* pRegex, const char* sText, size_t iTextSize, size_t iPos, xregexspan* pOutCaptures, uint32 iCaptureCount);
XXAPI int xrtRegexWhichCapturesAt(xregex* pRegex, const char* sText, size_t iTextSize, size_t iPos, xregexspan* pOutCaptures, uint32* pOutCapturesDidMatch, uint32 iCaptureCount);
XXAPI uint32 xrtRegexCaptureCount(const xregex* pRegex);
XXAPI const char* xrtRegexCaptureName(const xregex* pRegex, uint32 iCaptureIndex, size_t* pOutNameSize);

XXAPI int xrtRegexClone(xregex** ppOut, const xregex* pRegex, const xregexalloc* pAlloc);
```

补充说明：

- `xrtRegexIsMatch()` 只回答“是否命中”
- `xrtRegexFind()` 返回整段命中范围
- `xrtRegexCaptures()` 返回各捕获组范围
- `xrtRegexWhichCaptures()` 额外告诉你哪些捕获组真的命中了
- `xrtRegexCaptureCount()` 返回值包含 `capture[0]`
- 当前命名组语法支持 `(?P<name>...)` 和 `(?<name>...)`


### 4.2 Builder

```c
XXAPI int xrtRegexBuilderCreate(xregexbuilder** ppBuilder, const char* sPattern, size_t iPatternSize, const xregexalloc* pAlloc);
XXAPI void xrtRegexBuilderDestroy(xregexbuilder* pBuilder);
XXAPI void xrtRegexBuilderSetFlags(xregexbuilder* pBuilder, xregexflags iFlags);
```

Builder 更适合：

- pattern 不是 NUL 结尾字符串
- 需要 flags
- 需要读取编译错误细节
- 需要自定义 allocator

注意：

- `iPatternSize` 是显式长度
- 当前这里不走 `0 = 自动计算` 的惯例


### 4.3 多模式

```c
XXAPI int xrtRegexSetBuilderCreate(xregexsetbuilder** ppBuilder, const xregexalloc* pAlloc);
XXAPI void xrtRegexSetBuilderDestroy(xregexsetbuilder* pBuilder);
XXAPI int xrtRegexSetBuilderAdd(xregexsetbuilder* pBuilder, const xregex* pRegex);

XXAPI xregexset* xrtRegexSetCreate(const char* const* arrPatternsNt, size_t iPatternCount);
XXAPI int xrtRegexSetCreateFromBuilder(xregexset** ppSet, const xregexsetbuilder* pBuilder, const xregexalloc* pAlloc);
XXAPI void xrtRegexSetDestroy(xregexset* pSet);
XXAPI const char* xrtRegexSetGetErrorMsg(const xregexset* pSet);
XXAPI size_t xrtRegexSetGetErrorPos(const xregexset* pSet);

XXAPI int xrtRegexSetIsMatch(xregexset* pSet, const char* sText, size_t iTextSize);
XXAPI int xrtRegexSetMatches(xregexset* pSet, const char* sText, size_t iTextSize, uint32* pOutIndexes, uint32 iMaxIndexes, uint32* pOutIndexCount);
XXAPI int xrtRegexSetIsMatchAt(xregexset* pSet, const char* sText, size_t iTextSize, size_t iPos);
XXAPI int xrtRegexSetMatchesAt(xregexset* pSet, const char* sText, size_t iTextSize, size_t iPos, uint32* pOutIndexes, uint32 iMaxIndexes, uint32* pOutIndexCount);
XXAPI int xrtRegexSetClone(xregexset** ppOut, const xregexset* pSet, const xregexalloc* pAlloc);
```

补充说明：

- `xrtRegexSetIsMatch()` / `xrtRegexSetMatches()` 只回答“哪些规则命中”
- `xregexset` 不直接返回 span，也不直接返回捕获组
- 如果要先分类再提取，通常是先 `RegexSet`，再回到具体 `xregex`


## 6. 示例

### 5.1 快速匹配

```c
xregex* pRegex = xrtRegexCreate("hello");
if ( pRegex ) {
	int iMatch = xrtRegexIsMatch(pRegex, "hello world", strlen("hello world"));
	xrtRegexDestroy(pRegex);
}
```


### 5.2 查找首个匹配范围

```c
xregexspan tSpan;
xregex* pRegex = xrtRegexCreate("[0-9]+");

if ( pRegex ) {
	if ( xrtRegexFind(pRegex, "id=12345", strlen("id=12345"), &tSpan) > 0 ) {
		printf("begin=%zu end=%zu\n", tSpan.iBegin, tSpan.iEnd);
	}
	xrtRegexDestroy(pRegex);
}
```


### 5.3 获取捕获组

```c
xregexspan arrCaps[8];
xregex* pRegex = xrtRegexCreate("(?P<key>[A-Za-z]+)=(?P<value>[0-9]+)");

if ( pRegex ) {
	int iRet = xrtRegexCaptures(pRegex, "count=42", strlen("count=42"), arrCaps, 8u);
	if ( iRet > 0 ) {
		printf("all=%zu..%zu\n", arrCaps[0].iBegin, arrCaps[0].iEnd);
		printf("key=%zu..%zu\n", arrCaps[1].iBegin, arrCaps[1].iEnd);
		printf("val=%zu..%zu\n", arrCaps[2].iBegin, arrCaps[2].iEnd);
	}
	xrtRegexDestroy(pRegex);
}
```

### 5.4 用 Builder 读取编译错误

```c
xregexbuilder* pBuilder = NULL;
xregex* pRegex = NULL;
int iErr = xrtRegexBuilderCreate(&pBuilder, "(", 1u, NULL);

if ( (iErr == 0) && (pBuilder != NULL) ) {
	iErr = xrtRegexCreateFromBuilder(&pRegex, pBuilder, NULL);
	if ( (iErr < 0) && (pRegex != NULL) ) {
		printf("error=%s at %zu\n", xrtRegexGetErrorMsg(pRegex), xrtRegexGetErrorPos(pRegex));
		xrtRegexDestroy(pRegex);
	}
	xrtRegexBuilderDestroy(pBuilder);
}
```


### 5.5 多模式批量扫描

```c
const char* arrPatterns[] = {
	"error",
	"warn",
	"timeout"
};
uint32 arrIndexes[8];
uint32 iCount = 0;
const char* sText = "warn: network timeout";
xregexset* pSet = xrtRegexSetCreate(arrPatterns, 3u);

if ( pSet ) {
	xrtRegexSetMatches(pSet, sText, strlen(sText), arrIndexes, 8u, &iCount);
	xrtRegexSetDestroy(pSet);
}
```


## 7. 说明

- 当前引擎实现来自 `bbre`，但不建议再面向业务代码直接暴露 `bbre_*`。
- 对外统一使用 `xregex`、`xregexset`、`xregexspan` 和 `xrtRegex*`。
- `xrtRegexFind()` 和 `xrtRegexCaptures()` 的公开包装层会处理输出缓冲区适配，不直接把上层调用暴露给底层断言约束。
- 多线程场景下，如需独立对象，请使用 `xrtRegexClone()` 或 `xrtRegexSetClone()`。
