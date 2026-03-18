# Regex 正则表达式模块

> 基于 `bbre` 集成的当前正则表达式主线，面向轻量、高性能、可嵌入的文本匹配场景。

[返回索引](README.md)

---

## 1. 定位

XRT 当前正则模块建立在 [bbre](https://github.com/max-nurzia/bbre) 之上，并直接把核心能力纳入主线。

它适合这些场景：

- 文本匹配
- 子串搜索
- 捕获组提取
- 多模式匹配
- 配置、日志、HTTP 文本、模板片段的规则识别

它的目标不是做最重的“脚本语言级正则生态”，而是：

- 轻量
- 可嵌入
- 跨平台
- 直接服务于 XRT 的文本处理与网络应用层


## 2. 核心能力

当前主线提供了 4 类能力：

- 单模式编译与匹配
- 捕获组提取
- 模式克隆（适合多线程场景）
- 多模式集合匹配


## 3. 核心类型

### `bbre`

单个正则表达式对象。

典型流程：

1. 编译
2. 匹配 / 查找 / 捕获
3. 销毁


### `bbre_builder`

高级构建器。

适合：

- 按需设置标志
- 显式控制编译选项


### `bbre_set`

多模式集合匹配对象。

适合：

- 同时检查多条规则
- 批量扫描文本


### `bbre_span`

匹配范围：

```c
typedef struct bbre_span {
	size_t begin;
	size_t end;
} bbre_span;
```

常用于：

- `find`
- `captures`


### `bbre_flags`

当前公开标志包括：

```c
BBRE_FLAG_INSENSITIVE
BBRE_FLAG_MULTILINE
BBRE_FLAG_DOTNEWLINE
BBRE_FLAG_UNGREEDY
```

含义分别是：

- 不区分大小写
- 多行模式
- `.` 可匹配换行
- 非贪婪量词


## 4. 当前正式 API

### 4.1 快速创建

```c
XXAPI bbre* bbre_init_pattern(const char* pat_nt);
```

适合：

- 单条简单规则
- 快速验证
- 脚本式开发


### 4.2 完整创建与销毁

```c
XXAPI int bbre_init(bbre** preg, const bbre_builder* build, const bbre_alloc* alloc);
XXAPI void bbre_destroy(bbre* reg);
```


### 4.3 匹配 API

```c
XXAPI int bbre_is_match(bbre* reg, const char* text, size_t text_size);
XXAPI int bbre_find(bbre* reg, const char* text, size_t text_size, bbre_span* out_bounds);
XXAPI int bbre_captures(
	bbre* reg,
	const char* text,
	size_t text_size,
	bbre_span* out_captures,
	unsigned int out_captures_size
);
```

用途：

- `is_match`：判断是否命中
- `find`：返回首个命中范围
- `captures`：返回捕获组范围


### 4.4 Builder API

```c
XXAPI int bbre_builder_init(
	bbre_builder** pbuild,
	const char* pat,
	size_t pat_size,
	const bbre_alloc* alloc
);
XXAPI void bbre_builder_destroy(bbre_builder* build);
XXAPI void bbre_builder_flags(bbre_builder* build, bbre_flags flags);
```


### 4.5 克隆 API

```c
XXAPI int bbre_clone(bbre** pout, const bbre* reg, const bbre_alloc* alloc);
```

用途：

- 在多线程场景中复制一份独立 regex 对象
- 避免不同执行上下文共享同一内部状态


### 4.6 多模式集合 API

```c
XXAPI int bbre_set_builder_init(bbre_set_builder** pbuild, const bbre_alloc* alloc);
XXAPI void bbre_set_builder_destroy(bbre_set_builder* build);
XXAPI int bbre_set_builder_add(bbre_set_builder* build, const bbre* reg);
XXAPI bbre_set* bbre_set_init_patterns(const char* const* ppats_nt, size_t num_pats);
XXAPI int bbre_set_init(bbre_set** pset, const bbre_set_builder* build, const bbre_alloc* alloc);
XXAPI void bbre_set_destroy(bbre_set* set);
XXAPI int bbre_set_is_match(bbre_set* set, const char* text, size_t text_size);
XXAPI int bbre_set_matches(
	bbre_set* set,
	const char* text,
	size_t text_size,
	unsigned int* out_idxs,
	unsigned int out_idxs_size,
	unsigned int* out_num_idxs
);
XXAPI int bbre_set_clone(bbre_set** pout, const bbre_set* set, const bbre_alloc* alloc);
```

适合：

- 多规则扫描
- 黑名单 / 白名单匹配
- 需要快速判断“命中了哪几条规则”的场景


### 4.7 版本

```c
XXAPI const char* bbre_version(void);
```


## 5. 常见用法

### 5.1 简单匹配

```c
bbre* pReg = bbre_init_pattern("hello");
if ( pReg != NULL ) {
	int iMatch = bbre_is_match(pReg, "hello world", strlen("hello world"));
	bbre_destroy(pReg);
}
```


### 5.2 查找首个命中

```c
bbre_span tSpan;
bbre* pReg = bbre_init_pattern("[0-9]+");

if ( pReg != NULL ) {
	if ( bbre_find(pReg, "id=12345", strlen("id=12345"), &tSpan) > 0 ) {
		/* tSpan.begin ~ tSpan.end 为命中范围 */
	}
	bbre_destroy(pReg);
}
```


### 5.3 使用捕获组

```c
bbre_span arrCaps[8];
bbre* pReg = bbre_init_pattern("([A-Za-z]+)=([0-9]+)");

if ( pReg != NULL ) {
	int iRet = bbre_captures(pReg, "count=42", strlen("count=42"), arrCaps, 8u);
	if ( iRet > 0 ) {
		/* arrCaps 中包含整体匹配和捕获组范围 */
	}
	bbre_destroy(pReg);
}
```


### 5.4 多模式匹配

```c
const char* arrPatterns[] = {
	"error",
	"warning",
	"timeout"
};

bbre_set* pSet = bbre_set_init_patterns(arrPatterns, 3u);
if ( pSet != NULL ) {
	unsigned int arrIdx[8];
	unsigned int iCount = 0u;
	bbre_set_matches(pSet, sText, strlen(sText), arrIdx, 8u, &iCount);
	bbre_set_destroy(pSet);
}
```


## 6. 使用建议

- 简单规则优先使用 `bbre_init_pattern`
- 需要 flags 时使用 builder
- 多线程下优先 clone，而不是共享同一个 `bbre*`
- 多模式扫描优先使用 `bbre_set`
- 处理大文本时，优先先缩小搜索范围，再做 regex 匹配


## 7. 与其他模块的关系

### 与字符串模块

- `regex` 负责规则匹配
- `string` 负责构造、切片、格式化

### 与网络应用层

- 可用于 HTTP header / 路由片段 / WebSocket 文本内容匹配

### 与模板 / JSON / 日志处理

- 适合做抽取、校验、替换前的规则判断


## 8. 当前边界

当前文档覆盖的是：

- 正则对象
- builder
- clone
- set 匹配

当前主线的定位是：

- 轻量 regex 基础设施
- 不是脚本引擎级 regex 包装层

