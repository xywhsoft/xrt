# Cookie 与 Set-Cookie

`cookie` 模块提供 HTTP `Cookie` 请求字段的无状态解析与写出；`set_cookie` 模块提供 `Set-Cookie` 响应字段的兼容接收解析、严格生成和扩展属性处理。两层都只处理字段值，不依赖 HTTP 客户端、服务器、请求对象或响应对象。

调用方可以直接把固定字段值交给 HTTP 层，也可以使用这里的结构化 API。结构化协议层是可选工具，不是发送响应的必经路径。

## 裁剪与依赖

```c
#define XRT_FEATURE_HTTP
#define XRT_FEATURE_COOKIE
#define XRT_FEATURE_TIME_TEXT
#define XRT_FEATURE_SET_COOKIE
```

`cookie` 依赖共享 HTTP 字段语法；`set_cookie` 额外依赖 HTTP 日期格式化与时间转换。客户端、服务器和 CookieJar 不属于这两个协议模块。

## Cookie 请求字段

### 逐项扫描与查找

```c
xcookienext xrtCookieNext(
	xstrview text, size_t* offset, xcookiepair* pair
);

xcookienext xrtCookieFind(
	xstrview text, xstrview name,
	size_t* offset, xcookiepair* pair
);
```

`xrtCookieNext` 返回 `ITEM`、`END` 或 `ERROR`，不会把正常结束与语法错误混为一谈。成功项中的名称和值都是借用输入的非零结尾视图；输入必须在视图使用期间保持有效。Offset 仅在返回一项时提交，失败不破坏调用方状态。

`xrtCookieFind` 会先验证完整字段，再查找区分大小写的名称，因此不会因提前命中而忽略尾部注入或畸形语法。重复名称可使用同一个 Offset 继续遍历。

HTTP 允许一条请求携带多个 `Cookie` 字段。调用方应按字段分别调用解析器，不能先用逗号合并字段值。

### 批量解析与限额

```c
bool xrtCookieValidate(
	xstrview text, const xcookielimits* limits, size_t* count
);

bool xrtCookieParse(
	xstrview text, xcookiepair* pairs, size_t capacity,
	size_t* count, const xcookielimits* limits
);
```

`xcookielimits` 可限制 pair 数、名称长度、值长度和原始字段字节数；零表示不限制。`xrtCookieParse` 采用两阶段提交：`pairs == NULL && capacity == 0` 成功返回所需数量；非空数组容量不足时也返回所需数量，但数组保持不变；成功数组仍借用原始字段。

Limits、Offset、Pair、Count 与 Size 描述符只要求位于完整可访问范围，不要求自然对齐；实现通过局部快照读取并一次发布结果。描述符与输入文本、数组或输出缓冲重叠，以及末地址回绕的伪造视图都会被拒绝。

### 写出与构建

```c
bool xrtCookieWrite(
	const xcookiepair* pairs, size_t count,
	void* output, size_t capacity, size_t* size
);

char* xrtCookieBuild(
	const xcookiepair* pairs, size_t count, size_t* size
);
```

`xrtCookieWrite` 生成规范的 `name=value; name=value` 字段值，不写字段名、冒号、CRLF 或零结尾。空输出可查询精确长度，短缓冲不会留下半成品，输入与输出重叠会被拒绝。

`xrtCookieBuild` 返回零结尾拥有字符串，由 `xrtFree` 释放。需要直接发送固定字段、手工拼包或零额外分配时，应继续使用原始 HTTP 字段接口。

## Set-Cookie 接收

```c
bool xrtSetCookieParse(xstrview text, xsetcookie* cookie);

xcookieattributenext xrtSetCookieAttributeNext(
	xstrview text, size_t* offset, xcookieattribute* attribute
);

bool xrtCookieDateParse(xstrview text, xtime* time);
```

`xrtSetCookieParse` 实现用户代理接收算法：允许历史上已部署但不适合新服务端生成的值；无效的单个已知属性会被忽略；重复已知属性以最后一个有效值为准。控制字符、超过 4096 字节的名称和值总和会使整个字段失败，超过 1024 字节的属性值会被忽略。

解析结果全部借用输入。`RawAttributes` 保存原始属性区，可用 `xrtSetCookieAttributeNext` 遍历未知扩展以及区分 `Foo` 与 `Foo=`。`SameParty` 等非标准属性不固化进核心结构，仍可通过这一通用路径保留。

`xrtCookieDateParse` 接受 RFC cookie-date 的兼容历史格式，并验证真实公历日期。它不等价于服务端生成时要求的 IMF-fixdate。

## Set-Cookie 生成

```c
bool xrtSetCookieValidate(xstrview text);

bool xrtSetCookieWrite(
	const xsetcookie* cookie,
	void* output, size_t capacity, size_t* size
);

char* xrtSetCookieBuild(
	const xsetcookie* cookie, size_t* size
);
```

`xrtSetCookieValidate` 严格验证服务端生产语法和可部署安全组合。它用于检查已拼好的字段值；兼容解析成功不代表该值适合新服务端生成。`Partitioned` 必须是无值属性，`Priority` 只接受 `Low`、`Medium` 或 `High`。

`xrtSetCookieWrite` 与 `xrtSetCookieBuild` 从结构化数据生成字段值，支持 Domain、Path、Expires、Max-Age、SameSite、Secure、HttpOnly、Partitioned、Priority 和任意受校验扩展。文本验证器与结构化构建器共享以下安全约束：

- `SameSite=None` 必须同时设置 `Secure`。
- `Partitioned` 必须同时设置 `Secure`。
- `__Secure-` 名称必须设置 `Secure`。
- `__Host-` 名称必须设置 `Secure`、`Path=/`，并且不能设置 Domain。
- 扩展属性不能重复，也不能覆盖结构中的已知属性。

`Max-Age=0` 或负数可由结构化构建器用于删除 Cookie；严格文本验证器按新服务端生产语法要求正数。生命周期上限、公共后缀、站点上下文、分区键、Domain/Path 匹配和驱逐策略属于后续 CookieJar，而不是字段解析器。

`RawAttributes` 只属于解析结果，构建时必须为空；`Extensions` 只属于构建输入。这个边界避免把未经检查的网络输入原样反射回响应。

每条 Cookie 必须使用独立的 `Set-Cookie` 字段，不能用逗号合并。模块故意不提供 `Set-Cookie:` 整行包装函数；字段名和行结束由 HTTP 字段写出层统一处理。

启用独立的 `http_client_set_cookie` 后，`xrtHttpResponseSetCookieNext` 可直接按响应
Header 的线路顺序逐条调用本模块解析器。该便利层仍不合并字段，解析结果借用响应；
不使用客户端对象时继续直接调用 `xrtSetCookieParse`。

## 错误与原子性

参数错误、语法错误、范围限制、容量不足和内存不足通过统一错误系统表达。所有 Write/Parse 批量接口先完成验证和计长，再提交输出；失败时不会产生部分协议数据。无分配接口不会调用分配器，分配便利函数只进行一次结果分配。

## 旧资产与验证

实现保留了旧版 `xhttp_util` 与 `xhttp_cookie` 中经过验证的日期 token、属性扫描、Cookie 查找及写出思路，去除了 URL、CookieJar、客户端状态和字段语法之间的重复实现，并补齐了三态迭代、借用期、原子失败、显式限额及通用扩展契约。

验证入口：

- `examples/http/cookie/main.c`
- `examples/http/set_cookie/main.c`
- `tests/http/test_cookie.c`
- `tests/http/test_cookie_mutation.c`
- `tests/http/test_cookie_noalloc.c`
- `tests/http/test_cookie_oom.c`
- `tests/http/test_set_cookie.c`
- `tests/http/test_set_cookie_mutation.c`
- `tests/http/test_set_cookie_noalloc.c`
- `tests/http/test_set_cookie_oom.c`
- `tests/single/test_single_cookie.c`
- `tests/single/test_single_set_cookie.c`

协议依据：[RFC 6265: HTTP State Management Mechanism](https://www.rfc-editor.org/rfc/rfc6265.html)。`Partitioned` 为已部署扩展；协议字段层只保存和生成该属性，CookieJar 仍需提供顶级站点分区上下文。
