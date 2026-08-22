# Query

`query` 是 URL query component 的原始结构层。它只拆分 `&` 和每个非空段中的第一个 `=`，不执行 percent 解码，也不套用 `application/x-www-form-urlencoded` 的 `+` 规则。该边界让路由器、代理、签名算法和需要保留词法形式的程序可以直接处理原始字节，同时让表单层在它上面提供解码便利。

## 裁剪与依赖

```c
#define XHTTP_FEATURE_QUERY
```

模块只依赖 `core`，对应：

- `include/xrt/query.h`
- `src/url/query.c`

启用 Query 不会拉入 URL、percent codec、HTTP、字符串对象或容器。

需要从未编码字段直接构建 RFC 3986 Query 时，再启用独立的便捷层：

```c
#define XRT_FEATURE_CODEC_PERCENT
#define XHTTP_FEATURE_QUERY_CODEC
```

`query_codec` 依赖 `query` 和 `codec_percent`，但 raw 解析与转发路径仍可单独裁剪。

## 数据模型

```c
typedef struct xquerypair {
	uint32 Flags;
	xstrview Key;
	xstrview Value;
} xquerypair;
```

`Key` 和 `Value` 都借用输入或调用方提供的原始文本。`XQUERY_HAS_VALUE` 明确区分：

- `a`：没有值，标志未设置；
- `a=`：存在空值，标志已设置且 `Value.Size == 0`；
- `=v`：空 key 和非空值；
- `=`：空 key 和空值。

重复 key 保持原顺序。空查询没有条目。前导问号可以省略；连续、前导或尾随 `&` 形成的空段按既有 XRT 行为和 form-urlencoded 拆分规则跳过。

## 迭代与查找

```c
xquerynext xrtQueryNext(
	xstrview query, size_t* offset, xquerypair* pair
);

xquerynext xrtQueryFind(
	xstrview query, xstrview key,
	size_t* offset, xquerypair* pair
);

bool xrtQueryCount(xstrview query, size_t* count);

bool xrtQueryValidate(
	xstrview query,
	const xquerylimits* limits,
	size_t* count
);
```

`offset` 初始为零。`XQUERY_NEXT_ITEM`、`XQUERY_NEXT_END` 和 `XQUERY_NEXT_ERROR` 分别表示条目、正常结束和调用错误，因此结束不会掩盖错误。`Find` 使用同一个偏移模型，可以连续查找同名重复项；比较按原始字节区分大小写。

这些函数不会验证 percent 转义或字符编码。需要已解码字段时，先迭代，再使用 percent 或 form codec；需要完整 URL 语法验证时使用 URL 层。

`xrtQueryValidate` 在同一遍扫描中检查 `MaxPairs`、`MaxKey` 和 `MaxValue`。限额字段为零表示不限，`limits == NULL` 表示没有隐藏上限。HTTP 客户端/服务器可以把自身配置映射到这里，而无需让低层 Query 固化某个请求大小。

## 写出与构建

```c
bool xrtQueryRawWrite(
	const xquerypair* pairs, size_t count,
	void* output, size_t capacity, size_t* size
);

char* xrtQueryRawBuild(
	const xquerypair* pairs, size_t count, size_t* size
);
```

`RawWrite` 不写前导问号和零结尾；空输出可查询精确长度。`RawBuild` 返回由 `xrtFree` 释放的零结尾字符串。

写出函数假定字段已经按调用方需要编码，不会隐式 percent 编码。为防止结构注入，key 不能包含 `&` 或 `=`，value 不能包含 `&`；value 中的后续 `=` 会原样保留。空 key 必须带显式值标志，因为没有等号的空段会被解析规则跳过。

容量、值或重叠失败不会修改输出。容量不足时 `size` 返回精确所需长度；其他失败保持原值。输出不能覆盖 pair 数组或任一借用视图。

## RFC 3986 构建

```c
bool xrtQueryWrite(
	const xquerypair* pairs, size_t count,
	void* output, size_t capacity, size_t* size
);

char* xrtQueryBuild(
	const xquerypair* pairs, size_t count, size_t* size
);
```

这是构建普通 URI Query 的推荐入口。键和值中的任意字节都会按 RFC 3986 component 规则编码：unreserved 字节直接保留，空格写成 `%20`，`~` 保留，`&` 和 `=` 被转义。`XQUERY_HAS_VALUE` 继续区分 `flag` 与 `flag=`，输出不包含前导问号。

`query_codec` 复用 `codec_percent` 的位图、长度计算和写出内核，没有另一份 `%HH` 实现。它与 form-urlencoded 是并列上层：Form 使用 `+` 表示空格且会编码 `~`，不能混用。

## 分层使用

- 原始签名、代理转发、保留 `%2F` 与 `%2f` 差异：直接使用 Query。
- 单个 RFC 3986 组件解码：组合 `codec_percent`。
- HTML GET/POST 表单：使用独立 form-urlencoded 层，它负责 `+`、专用安全字符集和字段编码。
- URL 只负责切出整个 query 视图，不负责解释字段。

## 旧资产与验证

旧版 `lib/xurl.h` 的借用视图和 offset 扫描被保留，N/非 N 双入口、`bool` 结束歧义、只检查 key 的不完整校验和固定数组 ParseTo 被移除。旧版 `lib/xhttp_util.h` 的 RFC 3986 构建能力由 `query_codec` 保留；原始已编码内容使用显式的 `RawWrite/RawBuild`。旧版控制 URI/form 行为的布尔参数被拆成两个独立协议层，避免调用点猜测模式。

- `examples/url/query/main.c`
- `tests/url/test_query.c`
- `tests/url/test_query_mutation.c`
- `tests/url/test_query_noalloc.c`
- `tests/url/test_query_oom.c`
- `tests/single/test_single_query.c`
- `examples/url/query_codec/main.c`
- `tests/url/test_query_codec.c`
- `tests/url/test_query_codec_mutation.c`
- `tests/url/test_query_codec_noalloc.c`
- `tests/url/test_query_codec_oom.c`
- `tests/single/test_single_query_codec.c`

测试覆盖前导问号、连续分隔符、缺失/空值、空 key、重复 key、值内等号、查找游标、结构注入、短缓冲原子性、重叠、6000 组构建往返、无分配、OOM 与单头文件。
