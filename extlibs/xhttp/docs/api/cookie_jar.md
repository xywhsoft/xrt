# CookieJar

CookieJar 是线程安全的 HTTP 用户代理 Cookie 存储。它建立在
`cookie`、`set_cookie`、`url` 和 `mutex` 之上，但不依赖 HTTP 客户端或
Header 容器。应用可以直接处理字段文本，也可以按需启用动态 Header 适配。

对应裁剪模块：

- `XHTTP_FEATURE_COOKIE_JAR`：存储、策略、请求选择和稳定快照。
- `XHTTP_FEATURE_COOKIE_JAR_HEADERS`：动态 Header 批量接收和请求字段应用。
- `cookie_jar_threads`：并发测试模块，不引入公开功能。

## 设计口径

- Jar 拥有 Cookie 文本，公开调用可以跨线程执行。
- `Set-Cookie` 字段必须逐行处理，禁止先用逗号合并。
- 接收和发送上下文显式表达 HTTP 来源、SameSite、导航类型与分区键。
- 策略拒绝返回 `XCOOKIE_STORE_REJECTED` 和 `xcookiereject`，不伪装成系统错误。
- 分配、参数、URL 或锁错误返回 `XCOOKIE_STORE_ERROR`，详情由统一错误 API 取得。
- 持久 Cookie 的过期时间始终钳制到接收时间之后最多 400 天。
- Jar 不把 Header 对象当成唯一入口；原始字段、结构化 Cookie 和 Header 适配可以独立使用。

## 配置

先用 `xrtCookieJarConfigInit` 初始化 `xcookiejarconfig`，再修改需要覆盖的字段。
配置没有版本字段，也没有历史兼容分支。
配置初始化和创建都支持合法的未对齐存储：初始化通过本地对齐值一次性发布，创建时
立即复制配置，Jar 不借用配置结构体。非空配置必须覆盖完整且不会地址回绕的连续范围。

| 字段 | 含义 | 默认值 |
| --- | --- | ---: |
| `Flags` | `XCOOKIE_JAR_*` 策略标志 | `0` |
| `InitialCookies` | 第一次增长的条目容量 | `16` |
| `MaxCookies` | Jar 总条目上限 | `3000` |
| `MaxCookiesPerDomain` | 同一规范 Domain 的条目上限 | `180` |
| `MaxCookieBytes` | name 与 value 总字节上限 | `4096` |
| `MaxNameBytes` | name 上限 | `4096` |
| `MaxValueBytes` | value 上限 | `4096` |
| `MaxDomainBytes` | Domain 上限 | `253` |
| `MaxPathBytes` | Path 上限 | `1024` |
| `MaxPartitionKeyBytes` | 分区键上限 | `1024` |
| `LaxUnsafeAge` | Default SameSite 的顶层非安全兼容窗口 | `2 分钟` |
| `IsPublicSuffix` | 公共后缀判断回调 | `NULL` |
| `PublicSuffixContext` | 回调上下文 | `NULL` |

`XCOOKIE_JAR_ALLOW_UNVERIFIED_DOMAIN` 只在没有 `IsPublicSuffix` 时生效。
默认安全策略只接受同主机的 `Domain` 属性并把它降级为 HostOnly；跨子域
Cookie 会以 `XCOOKIE_REJECT_PUBLIC_SUFFIX` 拒绝。明确设置该标志后可以接受
未经公共后缀表验证的跨子域 Cookie。

完整浏览器或爬虫应接入当前 Public Suffix List。PSL 是频繁更新的数据资产，
因此 CookieJar 通过真实策略回调扩展，而不把一份容易过期的大表固化进核心库。
回调收到不带前导点、已经转为小写的 ASCII Domain。

## 创建与生命周期

```c
xcookiejarconfig Config;
xcookiejar* pJar;

xrtCookieJarConfigInit(&Config);
Config.MaxCookies = 1000;
pJar = xrtCookieJarCreate(&Config);
```

- `xrtCookieJarCreate`：复制配置并创建 Jar；空配置使用默认值。
- `xrtCookieJarRetain`：增加引用计数。
- `xrtCookieJarRelease`：释放引用；最后一个引用销毁 Jar。
- `xrtCookieJarClear`：清空内容并保留数组容量。
- `xrtCookieJarCount`：读取条目数，不隐式清理过期条目。
- `xrtCookieJarPurge`：按显式 `xtime` 清理过期条目。

调用公开操作时，调用方必须持有一个 Jar 引用。`Release` 不能和不持有引用的
并发调用形成生命周期竞争。

## 接收 Cookie

`xcookiestorecontext` 字段：

| 字段 | 含义 |
| --- | --- |
| `Flags` | 接收来源、站点关系、导航类型和时间存在位 |
| `URL` | 产生响应的绝对 URL |
| `PartitionKey` | Partitioned Cookie 的顶层站点键 |
| `Now` | `XCOOKIE_STORE_HAS_NOW` 存在时使用的接收时间 |

标志：

- `XCOOKIE_STORE_HTTP_API`：字段来自 HTTP API，允许接收或覆盖 HttpOnly Cookie。
- `XCOOKIE_STORE_HAS_NOW`：使用 `Now`；未设置时调用 `xrtNow()`。
- `XCOOKIE_STORE_SAME_SITE`：响应来自同站请求。
- `XCOOKIE_STORE_TOP_LEVEL`：响应来自顶层导航。

低层路径 `xrtCookieJarSet` 接收已经由 `xrtSetCookieParse` 得到的 `xsetcookie`。
便捷路径 `xrtCookieJarStore` 接收完整字段值并完成宽松解析。

```c
xcookiestorecontext Context = { 0 };
xcookiereject Reject;
xcookiestorestatus Status;

Context.Flags = XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE;
Context.URL = XRT_STR_LITERAL("https://api.example.com/login");
Status = xrtCookieJarStore(
	pJar,
	&Context,
	XRT_STR_LITERAL("sid=abc; Path=/; Secure; HttpOnly"),
	&Reject
);
```

`xrtCookieJarStoreUrl` 是普通同站 HTTP 客户端快捷路径：使用当前时间、HTTP API
来源、同站关系和空分区键。跨站、顶层导航或 Partitioned Cookie 必须改用完整上下文。

### 存储结果

| 值 | 含义 |
| --- | --- |
| `XCOOKIE_STORE_ERROR` | 参数、URL、分配或锁错误 |
| `XCOOKIE_STORE_REJECTED` | Cookie 被语法或安全策略拒绝 |
| `XCOOKIE_STORE_STORED` | 新增或覆盖完成 |
| `XCOOKIE_STORE_REMOVED` | 过期指令删除了已有条目 |
| `XCOOKIE_STORE_IGNORED` | 过期指令没有找到对应条目 |

`xcookiereject` 提供稳定拒绝原因：

- `XCOOKIE_REJECT_SYNTAX`
- `XCOOKIE_REJECT_LIMIT`
- `XCOOKIE_REJECT_EMPTY`
- `XCOOKIE_REJECT_DOMAIN`
- `XCOOKIE_REJECT_PUBLIC_SUFFIX`
- `XCOOKIE_REJECT_SECURE`
- `XCOOKIE_REJECT_HTTP_ONLY`
- `XCOOKIE_REJECT_SAME_SITE`
- `XCOOKIE_REJECT_PREFIX`
- `XCOOKIE_REJECT_PARTITION`
- `XCOOKIE_REJECT_SECURE_OVERWRITE`

`XCOOKIE_REJECT_NONE` 表示没有策略拒绝。

## 生成请求字段

`xcookierequestcontext` 字段：

| 字段 | 含义 |
| --- | --- |
| `Flags` | 请求来源、站点关系、导航和时间存在位 |
| `URL` | 目标绝对 URL |
| `PartitionKey` | 当前顶层站点的分区键 |
| `Now` | `XCOOKIE_REQUEST_HAS_NOW` 存在时使用的请求时间 |

标志：

- `XCOOKIE_REQUEST_HTTP_API`：请求由 HTTP API 发出，可以发送 HttpOnly Cookie。
- `XCOOKIE_REQUEST_HAS_NOW`：使用显式 `Now`。
- `XCOOKIE_REQUEST_SAME_SITE`：请求与 Cookie 上下文同站。
- `XCOOKIE_REQUEST_TOP_LEVEL`：这是顶层导航。
- `XCOOKIE_REQUEST_SAFE_METHOD`：方法属于安全方法，例如 GET 或 HEAD。

CookieJar 不尝试从两个 URL 猜测 SameSite。站点边界需要 PSL、导航来源和上层
安全模型，必须由 HTTP 客户端、浏览器容器或调用方明确提供。

```c
xcookierequestcontext Context = { 0 };
char Buffer[1024];
size_t Size;

Context.Flags = XCOOKIE_REQUEST_HTTP_API |
	XCOOKIE_REQUEST_SAME_SITE |
	XCOOKIE_REQUEST_SAFE_METHOD;
Context.URL = XRT_STR_LITERAL("https://api.example.com/data");

if ( xrtCookieJarWrite(
	pJar, &Context, Buffer, sizeof(Buffer), &Size
) ) {
	/* Buffer[0..Size) 是不含字段名和零结尾的 Cookie 字段值。 */
}
```

- `xrtCookieJarWrite`：写入调用方缓冲；空输出查询精确长度。
- `xrtCookieJarBuild`：分配零结尾字段值，由 `xrtFree` 释放。
- `xrtCookieJarWriteUrl`：当前时间、同站、安全方法和 HTTP API 快捷路径。
- `xrtCookieJarBuildUrl`：对应的分配型快捷路径。

快捷 URL API 适合没有浏览器导航语义的普通同站 HTTP 客户端。跨站请求、脚本
访问和 Partitioned Cookie 必须使用完整请求上下文。

发送顺序为较长 Path 优先，同 Path 按较早创建优先。选择和写出在同一次锁定
中完成；短缓冲不修改输出。实际写出会更新时间访问时间，纯长度查询不会。

## SameSite

- 接收时，`Strict`、`Lax` 和 `Default` Cookie 只允许来自同站请求或顶层导航；跨站子资源只能创建 `SameSite=None; Secure` Cookie。
- `Strict`：只在 `XCOOKIE_REQUEST_SAME_SITE` 时发送。
- `Lax`：同站发送；跨站只允许顶层安全请求。
- `None`：不受 SameSite 限制，但存储时要求 Secure。
- `Default`：按 Lax 处理；在 `LaxUnsafeAge` 内额外允许跨站顶层非安全请求。

把 `LaxUnsafeAge` 设为零可禁用兼容窗口。

## Partitioned

Partitioned Cookie 必须同时满足：

- Set-Cookie 含 `Secure` 和 `Partitioned`。
- 接收上下文提供非空 `PartitionKey`。
- 请求上下文提供逐字节相同的 `PartitionKey`。

分区键和 HostOnly 位都是存储主键的一部分，因此同名、同 Domain、同 Path
Cookie 可以分别作为 HostOnly、Domain 或不同分区条目同时存在。CookieJar
把分区键视为不透明文本；规范化顶层站点是上层职责。

## 安全规则

存储路径执行以下规则：

- Secure Cookie 不能从非安全 URL 设置。
- 非 HTTP API 不能设置或覆盖 HttpOnly Cookie。
- 非安全来源不能用重叠 Domain/Path 的普通 Cookie 覆盖 Secure Cookie。
- `SameSite=None` 必须同时为 Secure。
- `__Secure-` 和 `__Host-` 前缀按 ASCII 大小写不敏感规则执行；无名 Cookie
  按规范检查值的前缀，不能用兼容语法绕过约束。
- `__Host-` 必须 Secure、HostOnly、显式 `Path=/` 且不能含 Domain。
- Domain Cookie 不能用于 IP 主机。
- Host、Domain 只接受规范 ASCII DNS 名称；IDNA 转换应在 URL 进入 Jar 前完成。

容量不足时，Jar 先清理过期条目，再按 Priority 和最后访问时间淘汰。未指定
Priority 按 Medium 处理。扩容失败不会提前淘汰现有 Cookie。

## 稳定快照

旧式“锁内取得借用视图、解锁后返回”无法在线程安全 Jar 中成立。新 API 通过
拥有文本的不可变快照导出内容：

```c
xcookiesnapshot* pSnapshot;
const xcookieinfo* pInfo;

pSnapshot = xrtCookieJarSnapshot(pJar, xrtNow());
pInfo = xrtCookieSnapshotAt(pSnapshot, 0);
/* pInfo 及其视图在 SnapshotDestroy 前稳定。 */
xrtCookieSnapshotDestroy(pSnapshot);
```

- `xrtCookieJarSnapshot`：清理过期条目后，在锁内复制全部条目。
- `xrtCookieSnapshotCount`：返回快照数量。
- `xrtCookieSnapshotAt`：越界返回空指针，不设置错误。
- `xrtCookieSnapshotDestroy`：释放快照。

`xcookieinfo` 包含 Flags、SameSite、Priority、Expires、Created、Accessed、Name、
Value、Domain、Path 和 PartitionKey。Flags 可以包含：

- `XCOOKIE_INFO_HOST_ONLY`
- `XCOOKIE_INFO_SECURE`
- `XCOOKIE_INFO_HTTP_ONLY`
- `XCOOKIE_INFO_PERSISTENT`
- `XCOOKIE_INFO_PARTITIONED`

快照可用于诊断、持久化适配和上层对象枚举。核心模块不绑定某一种磁盘格式。

## Header 适配

启用 `XHTTP_FEATURE_COOKIE_JAR_HEADERS` 后：

- `xrtCookieJarStoreHeaders`：遍历每个独立 `Set-Cookie` 字段并返回
  `xcookiestorereport`。运行时错误前已提交的字段不会回滚。
- `xrtCookieJarApply`：构建一个 Cookie 字段并使用 `xrtHttpHeadersSet` 替换同名
  字段；没有可发送 Cookie 时删除已有字段。

```c
xcookiestorereport Report;

xrtCookieJarStoreHeaders(pJar, &StoreContext, pResponseHeaders, &Report);
xrtCookieJarApply(pJar, &RequestContext, pRequestHeaders);
```

`xcookiestorereport` 分别统计 `Fields`、`Stored`、`Removed`、`Ignored` 和
`Rejected`。不需要动态 Header 的程序可以不启用该模块，并直接调用核心 API。

启用 `XHTTP_FEATURE_HTTP_CLIENT_COOKIES` 后，高层 Client 直接组合这两个适配入口：
请求提交前按逐次上下文调用 `Apply`，每个最终响应到达时调用 `StoreHeaders`。
该组合层不复制 Cookie 规则，也不建立第二套存储。显式 `Cookie` 字段优先，
自动字段在重定向每一跳重新选择；完整所有权、SameSite、分区键和错误契约见
`docs/api/http_client.md` 的“自动 Cookie”章节。

## 示例与测试

- `examples/http/cookie_jar/main.c`：原始字段快捷路径。
- `examples/http/cookie_jar_headers/main.c`：动态 Header 适配。
- `examples/http/client_cookies/main.c`：高层 Client 共享 Jar 与逐次策略。
- `tests/http/test_cookie_jar.c`：生命周期、路径、覆盖、寿命和淘汰。
- `tests/http/test_cookie_jar_policy.c`：SameSite、HttpOnly、Partitioned 和安全策略。
- `tests/http/test_cookie_jar_mutation.c`：6000 轮确定性变异。
- `tests/http/test_cookie_jar_oom.c`：存储、构建和快照事务 OOM。
- `tests/http/test_cookie_jar_threads.c`：共享 Jar 并发存取。
- `tests/http/test_http_client_cookie_oom.c`：客户端选择、生成、接收和错误链 OOM。
