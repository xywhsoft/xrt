---
num: 99
slug: xhttp-redirect
title: xhttp 自动行为：重定向、重试与 Cookie
volume: 卷十 扩展库：xhttp
type: practice
lead: 301/302/303 的方法改写与 307/308 的正文重放、幂等判定的自动重试、跨域凭据剥离——客户端的三个自动挡。
api: xhttp-http_client, xhttp-http_retry, xhttp-cookie_jar
---

## 导读

第 98 章的运行时之上叠着三个**自动行为层**——它们把"每个客户端都要写的胶水"变成配置开关。**重定向**（`http_client_redirect`）：默认最多十跳；301/302/303 后 POST 改 GET、307/308 保留方法但要求可重放正文；跨 origin 剥离凭据、HTTPS→HTTP 降级默认拒绝。**自动重试**（`http_client_retry`）：默认**关闭**——只有幂等方法且正文可重放才重试，退避尊重 `Retry-After`；"要不要重复副作用"的决策权归你。**自动 Cookie**（`http_client_cookies`）：Cookie Jar 挂进配置，Set-Cookie 自动进罐、后续请求自动带上。三个层各自独立裁剪、语义显式——"自动"不等于"魔法"，每一跳的行为都可预测、可诊断（第 98 章 Info 的 Wire 字段累计整条重定向链就是证据）。

## 引入

重定向看似简单（"跟着 Location 走"），坑密度却是 HTTP 客户端之最。POST 收到 302 后发 GET 还是 POST？（历史分歧：浏览器改 GET、规范说保留——现代语义 303 明确改 GET、307 明确保留。）重定向到别的域名还带 `Authorization` 吗？（不带——凭据跟随是新攻击面。）HTTPS 重定向到 HTTP 跟不跟？（默认不跟——降级剥掉加密。）正文重定向要重发——正文是流式的怎么办？（无法重放就失败，不能悄悄截断。）每一条都是真实 CVE 的来源。xhttp 把这些决策做成**默认安全的协议实现**：符合主流客户端语义、危险路径显式开关。

重试的默认值则是刻意的保守：**默认关闭**。因为"重试是否安全"是业务判断——GET 重试无害，POST 重试可能重复扣款。库能判定的是"技术上可重试"（幂等方法+可重放正文），"业务上允许"必须由你表态。

## 概念

### 重定向：方法语义与安全边界

```diagram flow
- 301/302/303 + 标准 POST → 改写为 GET（删正文/framing/表示类型/摘要字段）
- 307/308 → 保留方法与正文——正文必须可重放，否则 XHTTP_CLIENT_ERROR_REDIRECT_REPLAY
- 相对 Location：按当前有效 URL 解析（第 101 章 RFC 3986 引用解析）
- 跨 origin：默认剥 Authorization/Proxy-Authorization/Cookie；显式 FORWARD_CREDENTIALS 才保留
- HTTPS→HTTP：默认拒绝；ALLOW_DOWNGRADE 显式开放
- fragment 语义：Location 无 # 继承当前 fragment；显式空 # 阻止继承；fragment 不进 request-target
```

方法改写的精确边界：只匹配**标准方法** `POST`——自定义方法 `post` 不算（HTTP 方法大小写敏感，第 90 章同源规则）；307/308 的保留路径要求正文声明可重放（第 98 章 `xhttpbody` 的能力位）——流式生产者正文没法重来，明确失败好过悄悄丢。整链共享同一总截止时间（第 98 章）——十跳不放大预算。每跳可独立选择：继承/跟随/返回原响应/视为错误（调用选项中的重定向字段所属的 `xhttpcalloptions` 四模式）。

### 自动重试：幂等判定与三重条件

重试一次必须同时满足：**额度未用完**（`MaxRetries`）、**方法幂等**（`xrtHttpMethodIdempotent()` 判定——GET/HEAD/PUT/DELETE 等安全可重试，POST 不在列）、**正文可重放**（存在正文时 `xhttpbody` 声明）。三条件缺一不可——这是技术护栏；业务护栏是你设置额度这个动作本身。退避计算复用纯协议模块 `http_retry`：指数退避（`BaseDelay`/`MaxDelay`）+ 尊重服务端 `Retry-After`。调用级四模式：`DEFAULT` 继承 Client / `DISABLED` 本次关 / `ENABLED`（Client 零额度时用默认上限）；配置错误（未知模式、零 MaxDelay、Base>Max）**在网络操作前失败**——启动期拦截。

### 自动 Cookie：Jar 的挂载

`xhttpclientconfig` 的 Cookie 配置挂入 `xcookiejar` 后：响应的 Set-Cookie 自动进罐（域/路径/过期/安全属性按 RFC 6265 处理——第 32 章讲过的语义在这里自动化）、后续请求自动匹配携带。Jar 是独立对象（持久化、多 Client 共享、按分区隔离都归你——与第 100 章缓存的 Store 同一形态）。不挂 Jar 就没有自动行为——显式装配。

### 自动行为的公共形态：显式、可诊断、独立裁剪

三个层共享三条设计纪律：**默认安全**（重定向十跳+剥凭据+拒降级；重试关闭；Cookie 不挂不启用）；**失败显式**（不可重放、配置非法、降级——都是明确错误类别，不是静默跳过）；**诊断可见**（Info 的 `ResponseWireBytes` 累计整链、`Phase` 保留实际结束阶段——"重定向了几跳"从结果对象可查：最终有效 URL 与请求 URL 不同即跟过）。

## 示例

### 第一个完整程序：重试配置的显式装配

下面的程序来自 `examples/http/client_retry`——默认关闭与显式启用的对照：

```embed path="extlibs/xhttp/examples/http/client_retry/main.c" title="extlibs/xhttp/examples/http/client_retry/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/client_retry/main.c -lws2_32 -liphlpapi
retries=3 base=250000 max=5000000
```

**刚才发生了什么。** ① `ConfigInit` 默认 `MaxRetries=0`——**自动重试默认不存在**；设置 3 次/250ms 起/5s 封顶后重试层才上线。② 输出回显配置——这个示例本身就是"配置面"的文档化：三参数（额度/基础退避/最大退避）就是全部 knobs。③ 调用级可临时覆盖：`Options.Retry.Mode` 四模式让单次调用关闭或强制启用——批量任务里"这一条不要重试"不用另建 Client。④ 与重试的判定护栏对照：这个配置只表达"技术上限"；每个请求实际可否重试仍走幂等+可重放判定——配置与判定分层，谁也不越界。

### 第二个完整程序：Cookie Jar 挂载

第二个程序来自 `examples/http/client_cookies`——自动 Cookie 的装配面：

```embed path="extlibs/xhttp/examples/http/client_cookies/main.c" title="extlibs/xhttp/examples/http/client_cookies/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/client_cookies/main.c -lws2_32 -liphlpapi
（示例校验 Jar 挂载后 Set-Cookie 自动进罐与后续自动携带路径）
```

**刚才发生了什么。** ① Jar 创建后放入 Client 的 Cookie 配置——Client 持引用，自己的引用可放。② 请求照常构建提交——**Set-Cookie 的收取与 Cookie 的回带全部自动**：响应到达进罐（域/路径/过期匹配语义由 Jar 实现）、下一条对同一站点的请求自动携带匹配项。③ 显式装配的意义：不挂 Jar 行为完全不存在（无隐藏全局罐）；挂了之后 Jar 的持久化（写盘）、共享（多 Client 一罐）、隔离（分区键）都是你手里的普通对象操作——第 32 章的 Cookie 语义与第 100 章缓存 Store 同一款"库给对象、你给策略"。

## 契约

- **重定向默认**：最多十跳；301/302/303+标准 POST 改 GET；307/308 保留方法（正文须可重放，否则 REPLAY 失败）。
- **凭据边界**：跨 origin 默认剥 Authorization/Proxy-Authorization/Cookie；`FORWARD_CREDENTIALS` 显式保留；降级默认拒绝、`ALLOW_DOWNGRADE` 显式开放。
- **fragment 语义**：无 # 继承、显式空 # 阻断；fragment 只留最终 URL 不进 target。
- **重试默认关闭**：启用须设额度；一次重试需额度+幂等方法+可重放正文三条件同时成立。
- **重试配置校验**：未知模式/Flags、零 MaxDelay、Base>Max 在网络操作前失败（启动期拦截）。
- **调用级覆盖**：重试四模式、重定向四模式（继承/跟随/返回原响应/视为错误）单调用可独立。
- **Cookie 挂载**：Jar 进配置即自动收发；不挂无行为；Jar 为公开对象（持久化/共享/隔离归调用方）。
- **链路预算**：整条重定向链共享总截止时间；Wire 字节累计整链。
- **裁剪**：`HTTP_CLIENT_REDIRECT`/`_RETRY`/`_COOKIES` 独立宏。

## 避坑

### 坑 1：给非幂等请求开自动重试

症状：POST 开了重试，网络抖动导致重复下单——库存扣了两次，客诉进来了。

原因：重试的三重条件里"幂等方法"是**技术判定**，拦不住业务语义：你的 POST 若有唯一键保护可安全重放，但你没声明可重放正文；若改成"GET 化的去重查询"又改变了语义。正确路径：默认关闭；确需重试的非幂等请求，在确认业务有幂等保护后**为该调用显式启用**（契约允许调用方确认后设置）。

```c bad
Config.Retry.MaxRetries = 3;   /* 全局开——POST 也进重试判定（可重放正文则真重试） */
```

```c good
Config.Retry.MaxRetries = 3;   /* Client 层给额度 */
/* 非幂等调用逐条关闭： */
Options.Retry.Mode = XHTTP_RETRY_DISABLED;
pCall = xrtHttpClientDo(pClient, pOrderReq, &Options, ...);
/* 仅对确认可重放的调用显式启用 */
```

### 坑 2：跟随跨域重定向时凭据泄漏

症状：api.example.com 的请求（带 Authorization）被 302 到 attacker.net——默认配置下头被剥掉（安全）；但"为了兼容旧网关"你开了 FORWARD_CREDENTIALS——token 到了第三方域。

原因：跨域凭据跟随本质是把信任扩展到重定向目标。默认剥离是保护；显式开放时你必须同时信任**自己的重定向源**（它控制 Location 指向哪）。

```c bad
/* 全局打开：任何跳转都带 token */
Options.Redirect.Flags |= XHTTP_REDIRECT_FORWARD_CREDENTIALS;  /* 全局沿用 */
```

```c good
/* 保持默认剥离；确需跟随的少数受控调用逐条开，
   且只对同站受控跳转源 */
if ( is_trusted_redirect_source(Req) ) {
	Opts.Redirect.Flags |= XHTTP_REDIRECT_FORWARD_CREDENTIALS;
}
```

### 坑 3：307/308 重定向配流式正文

症状：带生产者流正文的 PUT 收到 307——请求以 `REDIRECT_REPLAY` 失败；有人"修复"成把已读部分截断重发——服务端收到损坏正文。

原因：307/308 语义要求原样重发正文；流式正文（生产者回调）无法倒带——库的正确行为就是失败。截断重发是把协议错误变成数据损坏。

```c bad
/* 自作聪明：把流缓冲已读部分当完整正文重发 */
redirect_with_partial_body(...);   /* 服务端数据损坏 */
```

```c good
/* 两条正路：
   1) 需要跟 307/308 的请求用可重放正文（SetBytes/SetBody 带重放声明）
   2) 流式正文收到 REPLAY 失败时：读完整正文后以新请求重发 */
if ( error_is(REDIRECT_REPLAY) ) {
	buffer_body_then_retry_as_new_call();
}
```

## 练习

### 基础：四类状态码的行为矩阵

本地服务分别对 GET/POST 返回 301/302/303/307/308，客户端跟随并打印每跳的最终方法、最终 URL、正文有无。验收标准：十种组合的行为与契约矩阵一致；`Info.ResponseWireBytes` 与手数跳数吻合。

### 进阶：重试护栏验证

配置重试 3 次，分别提交：GET（幂等）、POST 字节正文（可重放）、POST 流式正文（不可重放）——对间歇性 503 的服务测量各自的重试次数。验收标准：GET 与可重放 POST 重试至成功；流式 POST 首败即终态；`Retry-After` 生效时退避尊重服务端值。

### 挑战：登录会话流程

用 Cookie Jar 实现三步流程：登录端点（收 Set-Cookie 会话）→ 携带会话请求受保护资源 → 登出。中途插入一次跨域重定向验证凭据剥离（默认无 Cookie 到第三方）。验收标准：三步会话自动延续；跨域跳转的请求里无会话 Cookie；Jar 生命周期与 Client 正确分离（Destroy 顺序）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 重定向默认 | 十跳；301/302/303+POST→GET；307/308 保留方法 |
| 正文重放 | 307/308 须可重放正文；不可重放=REPLAY 失败（不截断） |
| 凭据边界 | 跨 origin 剥三类凭据；FORWARD_CREDENTIALS 显式；降级默认拒 |
| fragment | 无 # 继承/空 # 阻断；不进 request-target |
| 重试默认 | 关闭；启用设额度；单次重试=额度+幂等+可重放三条件 |
| 重试退避 | Base/MaxDelay 指数退避+尊重 Retry-After；配置错在网络前失败 |
| 调用级覆盖 | 重试四模式/重定向四模式——批量里逐条控制 |
| Cookie | Jar 进配置即自动收发；不挂无行为；持久化/共享归你 |
| 链路预算 | 整链共享总截止；Wire 累计；最终 URL 可查跳转结果 |
| 裁剪 | REDIRECT/RETRY/COOKIES 独立宏 |
