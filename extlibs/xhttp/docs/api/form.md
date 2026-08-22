# Form URL Encoded

`form_urlencoded` 实现 `application/x-www-form-urlencoded` 的字节层 codec、零分配原地解析和字段序列化。它适用于 HTML GET 参数、POST 表单和 JavaScript `URLSearchParams` 对应的线路格式，但不会把 HTTP 请求对象、URL 对象或动态字典强加给调用方。

## 裁剪与依赖

```c
#define XRT_FEATURE_CODEC_PERCENT
#define XHTTP_FEATURE_QUERY
#define XHTTP_FEATURE_FORM_URLENCODED
```

Form 依赖 percent 的共享字节引擎和 Query 的结构扫描器。依赖是直接且真实的，不通过函数表间接连接；公开语义仍分为独立 API，Query 和 percent 都能单独裁剪。

## 精确编码规则

XRT 使用现行 WHATWG URL Standard 的 form-urlencoded ASCII 规则：

- 字母、数字、`*`、`-`、`.`、`_` 直接保留；
- SPACE 编码为 `+`；
- `+` 编码为 `%2B`；
- `~` 编码为 `%7E`，这与通用 RFC 3986 percent codec 不同；
- 其他字节使用大写 `%HH`。

XRT 的低层接口接收字节，不隐式转换本地字符集。UTF-8 文本应先由调用方或上层宿主确认编码，再交给 Form；这样 C 调用者也能处理明确编码或二进制边界。

## 字节 Codec

```c
bool xrtFormEncode(
	const void* data, size_t size,
	char* output, size_t capacity, size_t* outputSize
);

bool xrtFormDecode(
	xstrview text,
	void* output, size_t capacity, size_t* outputSize
);
```

编码输出包含零结尾，返回长度不计零。解码输出是任意字节，可以包含零。两者都支持输出与输入同址，其他部分重叠被拒绝；空输出可验证并查询长度。

解析器严格要求每个 `%` 后有两个十六进制数字。WHATWG 浏览器算法对畸形 `%` 较宽松，但标准库服务端默认不能静默接受歧义输入；需要完全保留异常线路文本时，可使用原始 Query 迭代并自行选择兼容策略。

分配型便利函数为：

```c
char* xrtFormEncodeNew(
	const void* data, size_t size, size_t* outputSize
);

unsigned char* xrtFormDecodeNew(
	xstrview text, size_t* outputSize
);
```

结果由 `xrtFree` 释放。解码长度必须接收，额外的尾部零只是哨兵。

## 原地解析

```c
bool xrtFormParse(
	void* data, size_t size,
	xformfield* fields, size_t capacity,
	size_t* count, const xformlimits* limits
);
```

第一次把 `fields` 传为 `NULL`、容量传零，可严格预检并查询字段数，不修改输入。容量足够时，函数在原始正文各字段范围内原地执行 `+` 和 `%HH` 解码，`xformfield` 视图借用该缓冲，因此没有每字段分配。

解析规则与 WHATWG tuple parser 对齐：按 `&` 拆分、跳过空段、首个 `=` 分隔；没有等号的字段也产生空 value。Form 正文不识别 URL 的前导问号，因此 `?a=b` 的 name 是 `?a`。

`xformlimits` 可以限制字段数、单个已解码 name/value 和已解码总字节数。零表示不限，`limits == NULL` 不加入隐藏上限。格式、限额或字段数组容量失败不会修改正文；容量不足时 `count` 返回所需字段数。

## 直接查找

```c
xformfind xrtFormFind(
	xstrview text, xbytesview name, size_t* offset,
	void* value, size_t capacity, size_t* size
);
```

`xrtFormFind` 是无需复制正文或建立字段数组的常用快捷路径。它按 form 规则解码后比较 name，因而 `a+b`、`a%20b` 都能匹配字节名称 `a b`；value 同样执行 `+` 和 `%HH` 解码。共享 offset 可以继续查找同名重复字段，返回值明确区分 `XFORM_FIND_FOUND`、`XFORM_FIND_END` 和 `XFORM_FIND_ERROR`。

函数在返回首个匹配项前严格验证整个正文，避免尾部畸形转义因提前命中而被忽略。`value == NULL` 可查询精确长度；容量不足时 `size` 返回所需长度，offset 与 value 不变。批量处理或需要显式资源限额时使用 `xrtFormParse`，它只进行一次预检和一次原地解码。

## 写出与构建

```c
bool xrtFormWrite(
	const xformfield* fields, size_t count,
	void* output, size_t capacity, size_t* size
);

char* xrtFormBuild(
	const xformfield* fields, size_t count, size_t* size
);
```

每个字段都序列化为 `name=value`，包括空 name 或空 value，并按输入顺序用 `&` 连接。`Write` 不写零结尾，可精确查询容量；`Build` 返回零结尾拥有字符串。两者直接接受字节视图，不要求先构建字典或响应对象。

## 旧资产与验证

旧版 `xrtPercent*` 的 form 布尔模式、`xrtFormUrlEncoded*` 重复扫描入口和 `QueryAppendPair(..., bPlusAsSpace)` 被拆成 Query、percent 和 Form 三层。旧直接缓冲、分配便利、借用扫描、字段构建和显式限额能力全部保留，并补齐失败原子性与二进制语义。

旧 `xrtQueryFindValueTo` 的单次调用手感由 `xrtFormFind` 保留，同时修复旧实现只比较未解码 key、无法遍历重复字段以及结束与错误混淆的问题。

- `examples/url/form/main.c`
- `tests/url/test_form.c`
- `tests/url/test_form_mutation.c`
- `tests/url/test_form_noalloc.c`
- `tests/url/test_form_oom.c`
- `tests/single/test_single_form.c`

测试覆盖 WHATWG 安全集合、UTF-8 字节向量、全部 256 字节、`+`/`%2B`/`~`、嵌入零、空段、空字段、无等号字段、前导问号、晚到畸形转义、限额、短容量、重叠、6000 组随机字段往返、无分配、OOM 与单头文件。

规范依据：[WHATWG URL Standard, application/x-www-form-urlencoded](https://url.spec.whatwg.org/#application/x-www-form-urlencoded)。
