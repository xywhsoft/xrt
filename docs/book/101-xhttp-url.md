---
num: 101
slug: xhttp-url
title: URL 解析：xurl 与 RFC 3986
volume: 卷十 扩展库：xhttp
type: practice
lead: 零拷贝的通用 URI-reference 解析、存在位语义、端口词法、target 写出与引用解析——不止 scheme://host/path。
api: xhttp-url, xhttp-query, net
---

## 导读

`xurl` 是 xhttp 的 URL 层，但它**不止是 HTTP URL**：`xrtUrlParse` 解析完整的 RFC 3986 URI-reference——`mailto:user@example.com`、`urn:isbn:...`、网络路径 `//host/path`、相对引用 `../items?page=2` 全部支持，不把 URL 限制为 `scheme://host/path`。与全库一脉相承的设计：**零拷贝**（全部视图借用输入）、**存在位语义**（`Path` 恒在，Query/Fragment/UserInfo/端口是否有必须看 `Flags`——不能只看视图长度）、**词法保留**（`PortText` 原样保留 `:00080`，`port` 是任意长度十进制文本——`:65536` 不因超 uint16 而拒绝解析）、**无分配写出**（Write 族拒绝重叠、短缓冲零写入）。加上 RFC 3986 引用解析（`xrtUrlResolve`——相对地址展开）与 target 写出（HTTP 请求行形态），这就是 URL 的完整词汇表。

## 引入

"解析 URL"听起来是字符串切分，专业的坑位在哪？**其一：存在性与空值的区分**——`https://host` 没有 query，`https://host?` 有一个**空的** query：视图长度都是 0，语义不同（后者写出时问号要保留）。`xurl` 用 `Flags` 存在位解决：`XURL_HAS_QUERY` 区分"没有"与"空"。**其二：端口词法与数值**——RFC 的 port 是任意长度十进制文本：`:65536` 是**合法 URL**（解析不能拒），只是没有 uint16 数值；`XURL_PORT_VALUE` 标志"Port 成员含可表达数值"——`:00080` 词法保留、数值 80。**其三：相对引用**——HTTP 重定向的 `Location: ../health?full=1` 要按当前 URL 展开（第 99 章），引用解析是协议正确性的零件。

这三类问题共同指向"URL 是结构化文本不是字符串"——解析器把语法判定做完，调用方拿到的是无歧义的结构。

## 概念

### 结构与存在位

`xurl` 的字段全为视图：`Scheme/Authority/UserInfo/Host/PortText/Path/Query/Fragment` + `Flags`（存在位）+ `Port`（uint16 数值）。核心纪律：**`Path` 恒在但可空**；Query/Fragment/UserInfo/port 的有无看 `XURL_HAS_QUERY` 等存在位标志，**不能只看视图长度或数值**——显式空 query（`?`）与无 query 是两种结构。手工构造时的对称规则：数值端口设 `XURL_HAS_PORT | XURL_PORT_VALUE` 填 `Port` 留空 `PortText`；词法端口填 `PortText` 不设 `PORT_VALUE`。

### 解析边界与非法输入

- **ASCII URI**：非 ASCII 文本应先经 IRI/percent 编码层转换——未编码 UTF-8 字节不是合法 URI（中文域名要先 punycode、空格要先 `%20`）。
- **IPv6 ZoneID**：RFC 9844 已撤销 `[fe80::1%25eth0]` 这类扩展——解析器拒绝（本机 scope 表达见第 70 章网络地址层，两件事分开）。
- 视图借输入（输入失效不得再读）；解析输出不覆盖输入；可写区间末地址回绕按参数错误拒绝。

### 写出族：无分配、原子、拒重叠

`xrtUrlWrite/UrlAuthorityWrite/UrlHostWrite/UrlTargetWrite`：结构 → 文本，**不写 `\0`**、`NULL/0` 容量查精确长度、短缓冲零写入、**输出与输入对象/视图/长度输出重叠一律拒绝**。`Target` 写出的是 HTTP 请求行用的 origin-form（`/api/items?page=2`）——请求构造的直通件。`PathNormalize` 支持原地与有限重叠；`Resolve` 拒绝输出覆盖 Base/Reference。**Build 族**（`UrlResolveBuild` 等）返回 `xrtFree` 释放的零结尾字符串——只在要"一份独立字符串"时才付这一次分配。

### 引用解析：相对地址展开

`xrtUrlResolve(Base, Reference, 输出)` 按 RFC 3986 §5 展开相对引用：`../health?full=1` 对 `https://example.test/api/items?page=2` 解析为 `https://example.test/api/health?full=1`——路径逐段消解（`..` 上翻）、query 覆盖、无 scheme 继承 base 的 scheme/authority。fragment 语义由调用方（HTTP 重定向的继承/阻断规则在第 99 章——那是协议层策略，本层只管语法展开）。长度查询与直接写出均不分配；失败不修改输出。

### 与第 88 章 Host、第 102 章查询的分工

`xurl` 的 `Host` 视图给谁用？HTTP 侧的 Host 字段拆解（第 88 章 `xhttpauthority`）处理 `Host:` 头的 authority 形态；URL 里的 host 是 URI 组件——两者共享"方括号 IPv6/端口"的语法但入口不同。query 串的键值遍历（`xrtQueryNext`）作用在 `Url.Query` 视图上——第 102 章展开。**层内不做的事**：编码/解码（percent 层）、punycode（charset 层）——xurl 只管结构。

### xurl 的分层位置

```diagram flow
- xurl（本章）：RFC 3986 结构解析/写出/引用展开——零拷贝、存在位、词法保留
- query 两层（第 102 章）：零分配遍历 + 拥有型容器（& 域）
- HTTP 特化（第 88/91 章）：Host 头 authority、字段值分号参数（; 域）
- 编解码（第 28 章）：percent/punycode——xurl 之上的字符层
```

## 示例

### 第一个完整程序：解析、target 写出与引用解析

下面的程序来自 `examples/url/main.c`——三步走完核心面：

```embed path="extlibs/xhttp/examples/url/main.c" title="extlibs/xhttp/examples/url/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/url/main.c -lws2_32 -liphlpapi
host: example.test
target: /api/items?page=2
resolved: https://example.test/api/health?full=1
```

**刚才发生了什么。** ① `UrlParse` 一次解析完整 URL——`Host` 视图（`example.test`）与 Query（`page=2`）全部借用输入，零分配。② `UrlTargetWrite` 写出 origin-form target：`/api/items?page=2`——HTTP 请求行直接可用（fragment `#result` 不进 target——它是客户端本地语义）。③ `UrlResolveBuild` 把相对引用 `../health?full=1` 对当前 URL 展开：路径 `api/items` 被 `..` 上翻到 `api/`、拼上 `health`、query 换成 `full=1`——一次分配只产最终字符串（无中间合并对象）。这三行就是重定向 Location 处理（第 99 章）的完整底层。

### 第二个完整程序：URI-Reference 参数校验

第二个程序来自 `examples/url_param/main.c`——不建临时字符串的引用校验：

```embed path="extlibs/xhttp/examples/url_param/main.c" title="extlibs/xhttp/examples/url_param/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/url_param/main.c -lws2_32 -liphlpapi
valid: yes
```

**刚才发生了什么。** ① 输入是 HTTP 参数值形态的 URI-reference（quoted-string 里的 `../items?page=2`——`Link` 头的常见载体）；`xrtUrlParamValid` 在**参数解码后的视图**上直接校验合法性——不复制、不建临时字符串。② 这类"校验不落地"的入口是中间件/代理的高频路径：每个请求头里的 URL 都要验，分配就是开销。③ 配套巡检在 `examples/url/query*`（第 102 章展开）与 `examples/http/origin`/`forward`/`link`（origin 计算、Forwarded 处理、Link 头解析——同一语法家族的 HTTP 特化）。

## 契约

- **解析范围**：完整 RFC 3986 URI-reference（非仅 http 形态）；ASCII 输入；ZoneID IPv6 拒绝（RFC 9844）。
- **存在位**：`Path` 恒在可空；Query/Fragment/UserInfo/Port 有无看 `XURL_HAS_QUERY` 等标志——显式空与缺失是不同结构。
- **端口语义**：`PortText` 词法保留（`:00080` 原样）；`XURL_PORT_VALUE` 表 Port 含 uint16 数值；无端口/零/空/超范围都可能 Port==0——结合存在位判断；手工构造的两种对称填法。
- **写出族**：无 `\0`；空容量查长；短缓冲零写入；与输入重叠拒绝；Target 产 origin-form（无 fragment）。
- **PathNormalize/Resolve**：零分配；失败不改输出；重叠契约（原地/有限重叠/拒绝覆盖 Base）。
- **Build 族**：`xrtFree` 释放；ResolveBuild 只分配最终结果。
- **视图借用**：输入存活期内有效；输出不覆盖输入对象。
- **未对齐安全**：结构与 size_t 输出允许未对齐存储；可写区间回绕拒绝。
- **分工边界**：percent 编解码、punycode、Host 头拆解、query 键值——各自独立层；xurl 只管结构。

## 避坑

### 坑 1：用视图长度判断组件存在

症状：`https://host?` 的空 query 被当"无 query"——重写 URL 时问号丢了；或反向：把无 fragment 写成 `#`。

原因：空与缺失靠 `Flags` 区分，视图长度两者都是 0。这是 URL 往返（parse→改→write）保真的关键。

```c bad
if ( Url.Query.Size != 0 ) {   /* 空 query（有?）被误判为无 */
	write_url_with_query(...);
}
```

```c good
if ( (Url.Flags & XURL_HAS_QUERY) != 0 ) {
	/* 有 query（可能为空）——写出保留问号 */
}
```

### 坑 2：Port 数值直接当"有端口"

症状：`:0`、空端口 `:`、`:99999` 三种输入的 `Port` 都是 0——被当成"无端口"处理，默认端口判断错。

原因：`Port` 数值只在 `XURL_PORT_VALUE` 置位时才有意义；三种 Port==0 的输入语义各不同（显式零/空/超范围词法保留）。存在位+数值位两步走。

```c bad
uint16 Port = Url.Port ? Url.Port : 443;   /* 三种 Port==0 全走默认 */
```

```c good
uint16 Port;
if ( !xrtUrlPort(&Url, &Port) ) {   /* 数值可用性一步判定（含默认补齐） */
	Port = 443;                      /* 无可表达端口才走默认 */
}
```

### 坑 3：未编码的 UTF-8/中文直接进 URL

症状：拼进 URL 的中文路径解析失败或线上乱码——对端按 ASCII URI 解析出另一回事。

原因：URI 是 ASCII 语法；非 ASCII 必须先 percent 编码（路径/查询）或 punycode（host）。xurl 不猜不修——**非法输入明确拒绝**，修正是上层（charset/codec 层）的责任。

```c bad
snprintf(Url, "%s/search?q=%s", Base, "中文关键词");  /* 裸 UTF-8 进 URL */
```

```c good
/* 先 percent 编码再入 URL（第 28 章 codec 层职责） */
percent_encode("中文关键词", Encoded, sizeof(Encoded));
snprintf(Url, "%s/search?q=%s", Base, Encoded);
```

## 练习

### 基础：五种形态解析矩阵

解析五组输入：完整 https URL、`mailto:user@example.com`、`urn:isbn:0451450523`、`//host/path`（网络路径）、`../rel?x=1`（相对引用）——打印每组的 Flags 与非空组件。验收标准：五组的结构差异（scheme/authority/相对性）与 RFC 3986 对照一致。

### 进阶：往返保真测试

选十个"刁钻" URL（含空 query、空 fragment、`:00080` 端口、大小写 scheme、IPv6 host）——parse→write→再 parse，对比两次结构逐字段一致。验收标准：十个全部往返保真（含空组件的存在位不丢）；`:00080` 词法保留。

### 挑战：重定向 Location 展开器

实现 `next_url(当前URL, Location头值)`：覆盖五种 Location 形态（绝对、网络路径、绝对路径、相对路径、空 fragment 继承），用 `UrlResolve` 展开——对照第 99 章 fragment 继承/阻断规则处理 `#`。用一组真实站点的重定向链（curl -L 记录）回放验证。验收标准：展开结果与 curl 跟随的最终 URL 一致；fragment 继承规则与 RFC/第 99 章一致；全程零中间分配（Resolve 直写形态）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 解析范围 | RFC 3986 全 URI-reference（mailto/urn/网络路径/相对）；ASCII；拒 ZoneID |
| 存在位 | Path 恒在可空；其余组件看 XURL_HAS_QUERY 等标志——空≠缺失 |
| 端口 | PortText 词法保留；PORT_VALUE 才有数值；三种 Port==0 语义各异；UrlPort 一步判 |
| 写出族 | 无 \0；空容量查长；短缓冲零写入；拒重叠；Target=origin-form 无 fragment |
| 引用解析 | UrlResolve 展开 ../rel 形态；零分配失败不改；Build 族一次分配产终串 |
| 未对齐安全 | 结构/长度输出允许未对齐；回绕拒绝 |
| 视图纪律 | 全部借用输入；输出不覆盖输入 |
| 分工边界 | percent/punycode/Host 头/query 键值——独立层；xurl 只管结构 |
| HTTP 特化 | origin/Forwarded/Link 头解析是同族独立模块 |
