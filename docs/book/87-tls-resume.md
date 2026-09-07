---
num: 87
slug: tls-resume
title: 会话恢复：票据、PSK 与对象契约
volume: 卷八 安全 · 卷八收官
type: practice
lead: 恢复对象把"再来一次握手"打包成不可变值——票据签发、客户端接管、下一连接的 PSK+DHE 闭环，卷八的收官一章。
api: tls, tls_resume, crypto
---

## 导读

卷八收官。会话恢复解决一个纯性能问题：完整握手要一次证书验证加多次非对称运算（1-2 个 RTT），恢复握手用上次会话派生的 PSK 把它压到近一个 RTT 且零证书传输——移动网络与高频短连接的体验分水岭。第 81 章讲过客户端的用法面（票据缓存/`TakeResume`）；本章讲完整闭环与对象契约：**`xtlsresume` 恢复对象**（票据 + PSK + 路由绑定的不可变值，跨线程共享、缓存策略归你）、服务端的**签发与接受**（第 85 章已铺垫的两侧）、以及一次双握手闭环示例——会话一签发票据、客户端接管、会话二 PSK+DHE 恢复。安全边界也讲透：恢复始终保留 ECDHE（前向保密不降级）、binder 错误即认证失败、路由绑定防票据串门。

## 引入

你的 API 网关每秒承接近万条 TLS 连接，每条完整握手约 2 RTT + 一次证书链验证——握手开销超过了多数请求本身的处理时间。客户端呢？移动端每次唤醒都重握手，弱网下 300 毫秒的握手就是可感知的卡顿。TLS 1.3 的答案：握手结束时服务端发一张 **NewSessionTicket**（票据），客户端保存"票据 + 从握手派生的 PSK"；下次连接 ClientHello 带上票据与 **binder**（用 PSK 算的 HMAC——证明"我真是上次那个客户端"），服务端验 binder 通过则跳过证书阶段，双方用 PSK + 一次新的 ECDHE 派生新密钥——快、省、且不牺牲前向保密。

工程上真正的设计题在"票据归谁管"：XRT 的答案是**值对象 + 你的缓存**——`xtlsresume` 把恢复资产打包成不可变对象（深拷贝、引用计数、释放清零），存内存 Map、磁盘还是 Redis 由应用决定；协议层不做全局缓存（多进程/多租户/持久化的需求差异太大）。这个"库给值、应用给策略"的分界是本章的主线。

## 概念

### 恢复对象：一块不可变的恢复资产

`xtlsresume` 创建时深拷贝 ticket、PSK、SNI、ALPN 与可选对端身份，一次精确分配后**不可变**；引用计数跨线程共享；最后一个引用释放前整块清零（复用 `xrtSecureZero`）。约束（当前只接受 TLS 1.3）：ticket 1..65535 字节、PSK 长度等于套件摘要长度、寿命 1..604800 秒、ALPN ≤255 字节、SNI 拒绝内嵌空字节。有效期 `[IssuedAt, ExpiresAt)` 半开区间，**墙钟回退到签发前安全失效**（不延长寿命）；`xrtTlsResumeTicketAge` 验证有效期后按毫秒取整并与 `AgeAdd` 做 32 位模加——协议的年龄混淆计算内建。`ResumeInfo` 发布的视图只在引用存活期间有效，其中 **`Secret` 是敏感只读视图**——不得修改、不得无保护地记录（日志脱敏，第 76 章纪律）。

可选 `PeerIdentity` 是不参与线路编码的调用方身份域——存上次会话已验证叶证书的摘要，恢复连接"继承已认证身份"就有了落点（第 81 章：新票据继承原 `PeerIdentity`）。

### 闭环：签发 → 接管 → 恢复

```diagram flow
- 会话一（完整握手）：证书验证 + ECDHE + 双向 Finished → READY
- 签发：服务端 ServerTicket(ticket, 寿命, &恢复对象) → NewSessionTicket 发给客户端；恢复对象所有权交给服务端调用方（入库由你）
- 接管：客户端收到票据 → 深拷贝票据/SNI/ALPN/身份 → TakeResume 把引用转移给应用
- 缓存：应用自定策略（内存 Map / 磁盘 / Redis；容量、淘汰、租户隔离）
- 会话二：ClientConfig.Resume = 对象 → ClientHello 带票据 + binder + 正常 key share
- 接受：服务端 Resume 回调查票据 → 验版本/套件/SNI/ALPN/有效期/年龄 → PSK+DHE → 跳过证书
```

四个关键点。① **binder 在 ClientHello 里**——"证明持有 PSK"发生在发出 Hello 时（`accepted=not yet`：服务端是否接受要等 ServerHello，第 81 章的语义）。② **服务端可拒绝**：票据过期、路由不匹配、容量策略——客户端安全回退完整握手（`TlsClientResumed` 在服务端接受后才是 true）。③ **始终 PSK+DHE**：恢复连接仍带 x25519 key share，新 ECDHE 混入派生——即使 PSK 泄露，会话密钥仍被当次 ECDHE 保护（前向保密不降级）。④ **路由绑定**：省略 SNI/ALPN 时客户端**精确继承**对象的绑定；显式 SNI 必须完全匹配、显式 ALPN 列表必须包含票据协议——票据不能"串门"到别的域名或协议。

### 两端的 API 面

- **服务端签发**（第 85 章）：`xrtTlsServerTicket`（自带 ticket）或 `TicketNew`（32 字节安全随机 + 86400 秒默认）；`XTLS_AGAIN` 时输出为 `NULL` 且状态不变——排空重试。
- **客户端缓存**：`ResumeLimit`（0..64，默认 4）限保存量、满淘汰最旧；`TakeResume` 按接收顺序转移引用；`ResumeDropped` 统计禁用/淘汰/OOM 丢弃——缓存 OOM 不影响已就绪连接。
- **下一连接**：`ClientConfig.Resume = 对象`（会话持独立引用，你的引用可立即 Release）；过期/未生效/套件被策略禁用/路由不匹配都在**会话分配前拒绝**——坏票据不进状态机。

## 示例

### 第一个完整程序：恢复对象的生命周期

下面的程序来自 `examples/tls/resume/main.c`——创建、查询、校验、释放的最小闭环：

```embed path="examples/tls/resume/main.c" title="examples/tls/resume/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/resume/main.c -lws2_32 -liphlpapi
ticket=4 bytes, secret=32 bytes, lifetime=3600 seconds
```

**刚才发生了什么。** ① 配置五要素齐活：Cipher（恢复套件）、Ticket（不透明字节）、Secret（PSK——长度必须等于套件摘要长度，这里是 32）、ServerName 与 Protocol（路由绑定）、Lifetime。② `ResumeInfo` 的视图检查：ticket 4 字节、secret 32 字节、寿命 3600——对象把"恢复所需的一切"变成可查询的不可变事实。③ 这是**值对象的单元样本**：不涉及网络，纯粹的创建→读→放；缓存策略（你打算存哪、多久、怎么淘汰）从这个对象开始设计。

### 第二个完整程序：带票据的下一连接

第二个程序来自 `examples/tls/client_resume/main.c`——恢复对象交给 ClientConfig 后的首个 ClientHello 检查：

```embed path="examples/tls/client_resume/main.c" title="examples/tls/client_resume/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/client_resume/main.c -lws2_32 -liphlpapi
client hello=~290 bytes, accepted=not yet
```

**刚才发生了什么。** ① ClientHello 约 290 字节（随密钥材料 1–2 字节浮动）——比完整握手的 Hello 略大（多出 pre_shared_key 扩展与 binder），但换来的是服务端可跳过整个证书航班。② **`accepted=not yet` 是本章最值得咀嚼的输出**：binder 已写入 Hello（客户端证明持有 PSK），但"服务端接不接受"要等 ServerHello——恢复是**协商**不是命令；客户端为此同时携带正常 key share，被拒就走完整握手。③ 省略 SNI/ALPN 时客户端从恢复对象精确继承——路由绑定从"配置"变成"票据自带"。

### 第三个完整程序：双握手闭环

第三个程序来自 `examples/tls/resume_tour/main.c`——同一进程内两次握手的全链路自检：

```embed path="examples/tls/resume_tour/main.c" title="examples/tls/resume_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -I examples/tls -include xrt.h impl.c examples/tls/resume_tour/main.c -lws2_32 -liphlpapi
resume: handshake #1 + ticket-new issued ok
resume: client took ticket + retain/validat/ticketage ok
resume: custom ticket via server-ticket ok
resume: handshake #2 resumed + dropped counted ok
```

**刚才发生了什么。** 四行对应闭环四站。① **会话一**：完整证书握手 + `TicketNew` 签发（随机票据 + 默认寿命）。② **客户端接管**：`TakeResume` 拿到引用后 `Retain`（缓存一份）、`Info` 校验、`TicketAge` 年龄计算——值对象的全部操作面。③ **自定义票据**：服务端 `ServerTicket` 用调用方自己的 ticket 字节签发——多实例共享后端存储时票据格式由你定。④ **会话二**：恢复握手成立（`Resumed` 为 true）且"显式禁用后的丢弃被计数"——`ResumeDropped` 统计口径的验证。两次握手之间没有任何全局状态——闭环靠的就是"对象在两端之间传递"。配套的 `examples/tls/session_tour`（6 行输出）则是会话机全特性巡检：四种 Feed 形态、Span 收发、close_notify——本章三示例之外的补充样本。

## 契约

- **对象契约**：深拷贝一次分配、不可变、引用计数共享、末引用释放前清零；仅 TLS 1.3；字段边界（ticket 1..65535 / PSK=套件摘要长 / 寿命 1..604800 / ALPN≤255 / SNI 无内嵌空字节）。
- **时间语义**：有效期半开区间 `[IssuedAt, ExpiresAt)`；墙钟回退安全失效不延长；`TicketAge` 先验证有效期再做 32 位模加，失败不改输出。
- **敏感视图**：`Info().Secret` 只读敏感——不修改、不无保护记录；`PeerIdentity` 为调用方身份域不进线路。
- **服务端签发**：对象所有权转移；无全局缓存；AGAIN 输出 NULL 状态不变可重试；硬上限预检。
- **客户端缓存**：`ResumeLimit` 0..64 默认 4 满淘汰最旧；`TakeResume` 转移引用；丢弃计数不误报；缓存 OOM 不影响就绪连接。
- **恢复协商**：binder 随 ClientHello 证明持有 PSK；接受与否由服务端定；被拒安全回退完整握手（key share 始终携带）。
- **PSK+DHE 强制**：始终保留 ECDHE，前向保密不降级；无纯 PSK、无 0-RTT。
- **路由绑定**：省略即精确继承；显式 SNI 全等、显式 ALPN 须包含票据协议；不匹配在会话分配前拒绝。
- **认证边界**：票据元数据匹配而 binder 错误 = 认证失败（fatal，不降级）——服务端状态机内置，回调不得伪造"找到"。

## 避坑

### 坑 1：把 PSK 当普通缓冲打进日志/指标

症状：排障时打印恢复对象内容——PSK 秘密进了日志系统，等于把"下次握手的身份证明"永久存档。

原因：`ResumeInfo` 的 Secret 是敏感只读视图；值对象"不可变"不等于"不敏感"。第 76 章密钥不入日志的纪律对它同样成立。

```c bad
xrtTlsResumeInfo(pResume, &Info);
log_debug("ticket=%.*s psk=%.*s",   /* PSK 进日志：泄露 */
	(int)Info.Ticket.Size, Info.Ticket.Data,
	(int)Info.Secret.Size, Info.Secret.Data);
```

```c good
xrtTlsResumeInfo(pResume, &Info);
/* ticket 不保密可打摘要；PSK 只打长度与存在性 */
log_debug("ticket=%zu bytes psk=%zu bytes expires=%lld",
	Info.Ticket.Size, Info.Secret.Size,
	(long long)Info.ExpiresAt);
```

### 坑 2：恢复被拒不重试就报错

症状：服务端重启（内存票据丢失）后客户端全部失败——把"回退完整握手"误当成错误路径。

原因：恢复是协商不是保证——服务端票据丢了、过期了、容量策略拒了都正常；协议设计的期望行为就是**自动走完整握手**（ClientHello 本就同时带 key share）。把它当异常，是没理解 `accepted=not yet` 的语义。

```c bad
if ( !xrtTlsClientResumed(pSession) ) {
	return error("resume rejected");   /* 正常回退被当成故障 */
}
```

```c good
/* 完成握手后查询：resumed 只是性能提示，连接本身同样安全 */
bool bResumed = xrtTlsClientResumed(pSession);
metrics_record("tls_resumed", bResumed);
/* 无论恢复与否，连接都已通过完整或 PSK 认证——直接使用 */
```

### 坑 3：跨"路由域"复用票据

症状：内部服务网格里同一套客户端代码连多个环境（prod/staging），票据串门——连接失败或（更糟）身份误继承。

原因：票据绑定 SNI/ALPN，但**多个环境若共用同一 SNI**，对象层无法区分——串门后的 binder 可能恰好通过，`PeerIdentity` 继承的是另一环境的身份。路由隔离是应用层责任。

```c bad
/* 一个全局缓存服务所有环境 */
g_ResumeCache = cache_create();
resume = cache_take(g_ResumeCache, host);   /* host 相同、环境不同 */
```

```c good
/* 缓存键加入路由域（环境/租户/集群），或每域独立缓存 */
resume = cache_take(cache_for(env), host);
/* 更彻底：构造对象时用 PeerIdentity 存环境标签，
   恢复后校验标签一致才使用 */
```

## 练习

### 基础：对象边界探针

对 resume 示例做三组负向实验：PSK 长度改成 31（≠套件摘要长）、寿命 0、SNI 内嵌 `\0`——验证创建拒绝并读错误链。验收标准：三组都在创建期失败；错误消息能区分字段类别。

### 进阶：带 TTL 的内存票据缓存

实现 `resume_cache`：以（SNI, ticket 哈希）为键、容量 64、TTL 过期淘汰、线程安全（第 52 章原语）；淘汰计数与 `ResumeDropped` 对账。接入双握手闭环（resume_tour 改造）：会话二从缓存取票据。验收标准：重复往返 10 次全部恢复成功；容量压到 1 时按预期淘汰；全程 PSK 不出缓存边界（代码审查）。

### 挑战：恢复性能对比基准

用第 85 章内存桥测量：完整握手 vs 恢复握手各 1000 次的（a）Drive 轮数、（b）双向总字节、（c）有无证书消息。输出对比表并解释每项差异的来源（证书航班的有无、往返次数、Hello 大小）。验收标准：数据可复现（固定时间源）；差异方向与 TLS 1.3 设计预期一致；结论写清"什么负载下恢复收益最大"。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 恢复对象 | 票据+PSK+路由绑定（SNI/ALPN）+可选身份；不可变值、引用共享、释放清零 |
| 对象边界 | 仅 TLS 1.3；ticket 1..65535；PSK=套件摘要长；寿命 1..604800；ALPN≤255 |
| 时间语义 | `[IssuedAt, ExpiresAt)` 半开；墙钟回退安全失效；TicketAge 32 位模加 |
| 闭环五站 | 完整握手 → ServerTicket 签发 → TakeResume 接管 → 应用缓存 → 下一连接 PSK |
| binder 语义 | 随 ClientHello 证明持有 PSK；接受与否等 ServerHello（accepted=not yet） |
| 回退语义 | 被拒自动完整握手（key share 始终带）；resumed 是性能提示非安全判断 |
| PSK+DHE | 恢复仍带 ECDHE；前向保密不降级；无纯 PSK/0-RTT |
| 路由绑定 | 省略即继承；显式须全等/包含；不匹配在分配前拒；跨域隔离归应用 |
| 缓存边界 | 库给值对象：Limit 0..64/淘汰/丢弃计数在客户端；服务端签发后归你管 |
| 认证边界 | 元数据匹配+binder 错=认证失败 fatal；协议内置不可绕过 |
