# URL

`url` 是独立于 HTTP 客户端和服务器的 URI 基础层，负责零拷贝解析、结构化写出、路径规范化和相对引用解析。启用宏为 `XHTTP_FEATURE_URL`，只依赖 `core`。

可选的 `url_param` 裁剪层启用 `XHTTP_FEATURE_URL_PARAM`，在 `url` 之上增加
HTTP token/quoted-string 参数的 URI-reference 语义验证；普通 URL 用户不会因此引入
HTTP 参数解析。

## 契约

- `xrtUrlParse` 解析 RFC 3986 URI-reference，不把 URL 限制为 `scheme://host/path`，因此支持 `mailto:user@example.com`、`urn:isbn:...`、网络路径和相对引用。
- 解析器接受 ASCII URI。非 ASCII 文本应先由后续 IRI/percent 编码层转换，不能把未经编码的 UTF-8 字节直接当成可在线传输 URI。
- RFC 9844 已撤销 URI 中的 IPv6 ZoneID 扩展，所以 `[fe80::1%25eth0]` 不属于该解析器的合法 IP-literal。网络地址层可以独立表达本机 scope。
- `xurl` 的视图全部借用输入；输入失效后，不得继续读取这些视图。
- 解析结构和 `size_t` 长度输出允许使用未对齐存储。实现按字节发布完整结果，不会通过未对齐的结构体或整数左值访问内存。
- 所有公开可写区间必须完整可表示，末地址回绕会作为参数错误拒绝；解析输出不能覆盖输入，写出结果和长度输出不能覆盖输入对象或其借用视图。
- `Path` 始终存在，但可以为空。`Query`、`Fragment`、`UserInfo` 和端口是否存在必须查看 `Flags`，不能只看视图长度或数值。
- RFC 3986 的 `port` 是任意长度十进制文本。解析器完整保留合法 `PortText`，不会为了适配网络层而拒绝 `:65536` 或更长端口。
- `XURL_PORT_VALUE` 表示 `Port` 成员含有可由 `uint16` 表达的数值。没有端口、端口零、显式空端口和超范围端口的 `Port` 都可能为零，必须结合存在位判断。
- `PortText` 保留词法形式，解析再写出会保留 `:00080`。手工构造数值端口时设置 `XURL_HAS_PORT | XURL_PORT_VALUE`、填写 `Port` 并把 `PortText` 留空；手工构造长端口时填写 `PortText` 且不设置 `XURL_PORT_VALUE`。
- 所有 `Write` 函数都不写 `\0`。传入 `pOutput == NULL`、`iCapacity == 0` 可以查询精确长度。
- `xrtUrlWrite`、`xrtUrlAuthorityWrite`、`xrtUrlHostWrite` 和 `xrtUrlTargetWrite` 拒绝与输入对象、输入视图或长度输出重叠的输出，短缓冲不会写入部分结果。
- `xrtUrlPathNormalize` 与 `xrtUrlResolve` 的长度查询和直接写出均不分配，失败不修改输出。
- `xrtUrlPathNormalize` 支持原地和输出起点不晚于输入的重叠。反向重叠要求容量至少等于原始路径长度，以便先安全搬移完整输入。
- `xrtUrlResolve` 拒绝输出覆盖 `Base`、`Reference` 或它们借用的视图；这与结构化 URL writer 的别名契约一致。
- `Build` 函数返回由 `xrtFree` 释放的零结尾字符串；`xrtUrlResolveBuild` 只分配一次最终结果，不创建合并路径或规范化路径中间对象。

## 类型

```c
typedef struct xurl {
	uint32 Flags;
	uint16 Port;
	xstrview Scheme;
	xstrview Authority;
	xstrview UserInfo;
	xstrview Host;
	xstrview PortText;
	xstrview Path;
	xstrview Query;
	xstrview Fragment;
} xurl;
```

组件存在位：

| 标志 | 含义 |
|---|---|
| `XURL_HAS_SCHEME` | 存在 scheme |
| `XURL_HAS_AUTHORITY` | 存在 `//authority` |
| `XURL_HAS_USERINFO` | 存在 `userinfo@`，允许空 userinfo |
| `XURL_HAS_HOST` | authority 已解析出 host，允许空 host |
| `XURL_HAS_PORT` | 存在端口冒号 |
| `XURL_HAS_QUERY` | 存在 `?query`，允许空 query |
| `XURL_HAS_FRAGMENT` | 存在 `#fragment`，允许空 fragment |
| `XURL_HOST_IP_LITERAL` | host 是 IPv6 或 IPvFuture，`Host` 不含方括号 |
| `XURL_PORT_EMPTY` | 端口只有冒号，没有数字 |
| `XURL_PORT_VALUE` | `Port` 已无损转换为 `uint16` |

## 解析与查询

```c
bool xrtUrlParse(xstrview Text, xurl* pUrl);
bool xrtUrlAuthorityParse(xstrview Authority, xurl* pUrl);
uint16 xrtUrlDefaultPort(xstrview Scheme);
bool xrtUrlPort(const xurl* pUrl, uint16* pPort);
bool xrtUrlSchemeIs(const xurl* pUrl, xstrview Scheme);
bool xrtUrlSecure(const xurl* pUrl);
bool xrtUrlPortIsDefault(const xurl* pUrl);
bool xrtUrlParamValid(const xhttpparam* pParam);
```

`xrtUrlParamValid` 通过参数语义值游标直接识别 quoted-pair 后的 URL 分隔符，
不创建解码副本；它适合 Link 的 `anchor`、扩展关系 URI 等协议参数。该函数验证
完整 URI-reference，但不构造 `xurl`，因此借用视图仍应使用 `xrtUrlParse` 获得。

`xrtUrlDefaultPort` 只承诺 xrt 自身协议体系使用的 HTTP、HTTPS、WS 和 WSS。`xrtUrlPort` 把非空显式端口转换为网络端口；端口被省略或显式为空时使用已知 scheme 默认值。超出 `uint16` 返回 `XERR_RANGE`，既没有显式端口也没有已知默认值返回 `XERR_VALUE`，失败不修改 `*pPort`。`xrtUrlPortIsDefault` 只判断显式端口，空端口对已知 scheme 也属于默认端口。

```c
xurl Url;
uint16 Port;

if ( xrtUrlParse(
	XRT_STR_LITERAL("https://user@example.test:8443/api?q=1#result"),
	&Url
) ) {
	if ( xrtUrlPort(&Url, &Port) ) {
		printf("%.*s:%u\n", (int)Url.Host.Size, Url.Host.Data,
			(unsigned)Port);
	}
}
```

## 写出

```c
bool xrtUrlWrite(const xurl* pUrl, void* pOutput,
	size_t iCapacity, size_t* pSize);
str xrtUrlBuild(const xurl* pUrl, size_t* pSize);
bool xrtUrlAuthorityWrite(const xurl* pUrl, void* pOutput,
	size_t iCapacity, size_t* pSize);
bool xrtUrlHostWrite(const xurl* pUrl, void* pOutput,
	size_t iCapacity, size_t* pSize);
bool xrtUrlTargetWrite(const xurl* pUrl, void* pOutput,
	size_t iCapacity, size_t* pSize);
```

`xrtUrlTargetWrite` 写 HTTP origin-form：空路径写成 `/`，保留 query，明确丢弃 fragment。它不强迫 HTTP 用户创建请求对象，可以直接把 target 写入自己的封包缓冲。

```c
char Target[256];
size_t iTarget;

if ( xrtUrlTargetWrite(&Url, Target, sizeof(Target), &iTarget) ) {
	/* Target[0..iTarget) 可以直接进入 HTTP 请求行。 */
}
```

`xrtUrlHostWrite` 保留显式端口，包括默认端口。HTTP `Host` 字段是否省略默认端口属于 HTTP 协议 Helper 的职责。

## 路径与引用

```c
bool xrtUrlPathNormalize(xstrview Path, void* pOutput,
	size_t iCapacity, size_t* pSize);
str xrtUrlPathNormalizeBuild(xstrview Path, size_t* pSize);
bool xrtUrlResolve(const xurl* pBase, xstrview Reference,
	void* pOutput, size_t iCapacity, size_t* pSize);
str xrtUrlResolveBuild(const xurl* pBase, xstrview Reference,
	size_t* pSize);
```

路径规范化严格执行 RFC 3986 `remove_dot_segments`，不会把重复斜杠折叠：`/a//b/../c` 得到 `/a//c`。重复斜杠可能具有服务器语义，不能擅自清理。

直接 API 先完成语法校验和精确测长，再检查容量与别名，最后一次写出。因此短缓冲、非法输入和不安全重叠都不会留下部分结果。相对引用解析以两段借用路径表示 Base 目录与引用路径，合并和点段规范化都不需要临时字符串。

```c
xurl Base;
str sResolved;

if ( xrtUrlParse(XRT_STR_LITERAL("https://example.test/api/v1/"), &Base) ) {
	sResolved = xrtUrlResolveBuild(
		&Base, XRT_STR_LITERAL("../health?full=1"), NULL
	);
	if ( sResolved != NULL ) {
		printf("%s\n", sResolved);
		xrtFree(sResolved);
	}
}
```

## 规范化与资源身份

URL 层不提供一个声称适用于所有场景的 `Canonicalize` 函数。规范化不是单一的
语法操作：抓取去重通常移除 fragment，HTTP 签名必须保留签名算法指定的线路形式，
缓存键可能需要保留原始 query 顺序，应用路由还可能区分重复斜杠或编码形式。把这些
策略固定在基础层会错误地把两个资源视为同一资源。

调用方应按用途组合公开的低层能力：

- 用 `xrtUrlParse` 保留组件存在位和原始词法形式；不得把整个 URL 转为小写。
- 用 `xrtUrlResolve` 解析相对引用，用 `xrtUrlPathNormalize` 处理 RFC 3986 点段。
- 用 `xrtHttpOriginFromUrl`、`xrtHttpOriginSame` 和 `xrtHttpOriginWrite` 处理 HTTP
  的 scheme、host、默认端口与同源判断，不要用 host 子串判断域名归属。
- 抓取队列可以在自己的键策略中移除 fragment、规范化路径并保留 query 原始字节；
  是否解码 percent-encoding、排序参数或合并重复 URL 必须由该策略明确决定。
- HTML 链接发现、`robots.txt`、站点范围、每 Origin 限速和抓取队列属于应用协议与
  调度层，不应进入 RFC 3986 URL 解析器。

因此，URL 层既不隐藏实现所需的基础函数，也不替调用方做不可逆的资源等价决策。

## 错误

- 空指针与无效视图使用统一的 `XERR_ARGUMENT`。
- 非法 URI、非法结构组合和无 scheme 的 base 使用 `XERR_VALUE`。
- `xrtUrlPort` 遇到合法但超出 `uint16` 的端口文本使用 `XERR_RANGE`。
- 输出容量不足使用 `XERR_RANGE`，`*pSize` 保留所需长度。
- 长度加法溢出使用 `XERR_RANGE`，错误域和操作可进一步区分尺寸错误。
- 分配型函数失败使用 `XERR_MEMORY`。

错误可通过 `xrtGetError`、`xrtErrorKind`、`xrtErrorDomain`、`xrtErrorOperation` 和 `xrtErrorMessage` 读取，不存在 URL 私有的第二套错误状态。

## 标准依据

- [RFC 3986: URI Generic Syntax](https://www.rfc-editor.org/rfc/rfc3986.html)
- [RFC 9110: HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110.html)
- [RFC 9844: Entering IPv6 Zone Identifiers in User Interfaces](https://www.rfc-editor.org/rfc/rfc9844.html)
