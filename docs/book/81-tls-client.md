---
num: 81
slug: tls-client
title: TLS 客户端：拨号、握手与应用流
volume: 卷八 安全
type: practice
lead: DNS→TCP→TLS 一次托管拨号、回调与 Future 两种消费形态、READY 后的收发与关闭——客户端的全部生命周期。
api: tls, tls_verify, net
---

## 导读

从本章起的六章是 TLS——卷八原语的最终汇合点。本章讲**客户端**：发起连接的那一方。`xrtTlsDial` 一次托管"DNS 解析 → 竞争 TCP 地址 → TLS 握手"全链路，成功后把加密流（`xtlsstream`）交给你；回调与 Future 两种消费形态对应事件驱动与命令式两种代码风格。章内把客户端状态机（`HANDSHAKE → READY → CLOSED`）、配置三件套（验证器/ServerName/ALPN）、READY 后的应用数据收发与认证关闭走完整——第 83 章再钻进握手内部的时序与密钥调度，第 84 章展开验证器与策略。前置依赖全部来自本卷：证书验证（79/80 章）、AEAD 与密钥派生（73/74 章）、承载的 TCP 流（66 章）。

## 引入

写一个 HTTPS 客户端要串起多少步？解析主机名（第 65 章的 DNS）、按 happy-eyeballs 竞争连接多个地址（第 62 章）、TCP 建立后完成 TLS 握手（证书验证 + 密钥交换 + 双向 Finished）、然后才是发送请求接收响应——任何一步失败都要清理前面已建的全部资源。手写这套编排是经典的句柄泄漏重灾区；`xrtTlsDial` 把它收成一次调用：失败自动回滚中间资源，成功把流引用转移给完成回调，主线程只处理终态。

编排交给库，**决策留在你手里**：信任哪个 CA（验证器，第 84 章）、连哪个名字（SNI 自动取自主机名）、说什么协议（ALPN 数组）、总超时多少。这个分层是全库一贯的"骨架托管、策略注入"——第 61 章的骨架工业化思想在 TLS 层的再现。

## 概念

### 客户端状态机与拨号链

```diagram flow
- 提交：xrtTlsDial(Engine, Resolver, 主机名, 端口, TLS 配置, 拨号配置, 流回调...)
- 解析：Resolver 异步解析主机名（第 65 章）
- 竞连：多个地址按序尝试建立 TCP（失败自动换下一个）
- 握手：ClientHello → ... → 双向 Finished（第 83 章图解）
- READY：Stream 引用转移给完成回调，开始应用收发
- 终态：认证关闭（close_notify）或失败 → 释放
```

拨号对象 `xtlsdial` 本身可销毁——`xrtTlsDialDestroy` 不影响已在途的流；总超时（`DialConfig.Timeout`）覆盖全部阶段。**失败路径的契约**：拨号或握手失败时不会发布 Stream 回调——你不会收到一个半开的流；清理已由托管层完成。

### 配置三件套

- **验证器**（`TlsConfig.Verifier`）：完整证书握手**必须显式绑定**，否则创建时失败——"不配置验证"不是选项而是错误，这是与很多库最大的态度差异。验证器深复制信任库快照、可被多个客户端共享（第 84 章展开）。
- **ServerName**：SNI 与证书验证名自动取自主机名参数——拨号场景不用手填；直接用 `xrtTlsClientCreate` 的低层入口时在 `xtlsclientconfig.ServerName` 指定。
- **ALPN**（`Protocols` 数组）：按优先级声明应用协议（`h2`、`http/1.1`）；协商结果在握手后可查。恢复连接的 ALPN 必须与票据绑定一致。

### READY 之后：应用数据与关闭

READY 后的 `xtlsstream` 是一条加密的双向流，API 面与第 66 章 TCP 流同构，外加 TLS 特有件：`xrtTlsStreamSend`（明文进、密文出，`iWritten` 报告受理量）、`xrtTlsStreamAvailable`/`Buffer`/`Consume`（接收明文的零拷贝消费——Span 逐段取用，与第 64 章缓冲链同一套词汇）、`xrtTlsStreamClose`（**认证关闭**：排队 close_notify、等对端应答、排空后进 CLOSED）、`xrtTlsStreamAbort`（立即中止，错误路径专用）。收到对端 close_notify 时客户端自动排队一次应答、忽略随后数据、排空后进 CLOSED——关闭协议由状态机执行，你只调用一次 Close。

两级发送预算（TLS 记录层 `SendLimit` 与 TCP 写预算，第 67 章）都会让 Send 返回"部分受理"——从 `iWritten` 偏移继续是标准写法（本章示例的 `exampleTlsDialSend` 就是模板）。KeyUpdate 的自动轮换（记录用量临界点自动换钥）对应用透明；主动轮换用 `xrtTlsClientKeyUpdate`。

### 两种消费形态：回调与 Future

- **回调式**（`xrtTlsDial` + 事件表：`Open`/`Read`/`Writable`/`Close` 四个回调）：`Open`（READY 即触发，开始发请求）、`Read`（明文到达）、`Writable`（发送预算恢复，续传未完前缀）、`Close`（唯一终态，`xnetresult` + 结构化错误）。事件驱动服务与工具的主形态。
- **Future 式**（`xrtTlsDialAsync`）：返回 `xfuture`，任意线程 `xrtFutureWaitFor` 等待；`XFUTURE_RESOLVED` 后 `xrtFutureValue` 取流并 `xrtTlsStreamRef` 接管引用。命令式代码、协程集成的主形态。两种形态共享同一拨号配置与流对象——与第 65 章 DNS 解析、第 67 章拨号的双形态传统一致。

### 会话恢复：票据与 PSK

启用 `XRT_FEATURE_TLS_CLIENT_RESUME` 后，客户端从最终 transcript 派生恢复主秘密，按每张 `NewSessionTicket` 派生独立 PSK 并深拷贝票据（含实际 SNI/ALPN/已验证叶证书身份）。`ResumeLimit`（默认 4，最大 64，0 禁用）限制保存量，满队列淘汰最旧；`xrtTlsClientTakeResume` 把会话持有的引用转移给调用方——**跨连接缓存归你管**。下一次 ClientHello 携带票据 + 正常 key share：服务端接受则跳过证书阶段（PSK+DHE，仍前向安全），不接受则安全回退完整握手——`xrtTlsClientResumed` 报告实际选择。TLS 1.2 不提供恢复。

## 示例

### 第一个完整程序：回调式 HTTPS 拨号全链路

下面的程序来自 `examples/tls/dial/main.c`——系统信任库 + 主机名验证 + 托管拨号 + 认证关闭，裸内核层的完整范本（真实业务建议直接用第 97 章的 xhttp）：

```embed path="examples/tls/dial/main.c" title="examples/tls/dial/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/dial/main.c -lws2_32 -liphlpapi
usage: dial <host> [port]
```

**刚才发生了什么。** ① **验证器装配**：`StoreSystem` 快照 → `TlsVerifierCreate`（深复制，Store 随即释放）——三行就是"HTTPS 默认信任"的全部，第 84 章拆开讲。② **拨号提交**：`TlsDial(Engine, Resolver, host, 443, TlsConfig, DialConfig, Events...)` 一个调用串起 DNS/竞连/握手；`DialConfig.Timeout = 15s` 覆盖全阶段，主机名自动进 SNI 与验证名。③ **READY 回调编排**：`Open` 里发送最小 HTTP 请求——`exampleTlsDialSend` 是两级背压的标准模板（部分受理记 `Sent` 偏移，`XTLS_AGAIN` 即返回等 `Writable`）；请求排完 `StreamClose` 认证关闭写侧、继续收响应。④ **接收零拷贝**：`Read` 回调里 `Available/Buffer/Front/Consume` 逐 Span 打印——不复制整份响应，第 64 章的段链消费原样复用。⑤ **终态与清理**：`Close` 回调记录唯一终态（原子标志让主线程安全退出）；Cleanup 段按依赖逆序销毁 Dial/Stream/Resolver/Engine/Verifier——任何一步失败都走同一段，无句柄泄漏。

### 第二个完整程序：Future 式拨号与资源回收

第二个程序来自 `examples/tls/dial_future/main.c`，同样的链路换 Future 消费：

```embed path="examples/tls/dial_future/main.c" title="examples/tls/dial_future/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/dial_future/main.c -lws2_32 -liphlpapi
usage: dial_future <host> [port]
```

**刚才发生了什么。** ① `xrtTlsDialAsync` 返回 Future 立即返回——发起处不等待，任何线程可以 `xrtFutureWaitFor`（16 秒略宽于拨号 15 秒超时，等待窗口要盖住拨号窗口）。② 成功路径三步接管：`WaitFor → XFUTURE_RESOLVED 检查 → xrtFutureValue 取流 + xrtTlsStreamRef 接引用`，随后立即 `FutureDestroy`——Future 与流的引用在这一点交接完毕。③ **Abort 后的资源回收等待**是本章独有知识点：`Abort` 请求立即中止，但后台网络资源仍在释放——自旋等 `XTLS_STREAM_CLOSED/FAILED` 再 `Destroy`，这是"流对象与 Future 生命周期的交界点"（第 66 章"等 CLOSED 再销毁"的规则在 TLS 流上原样成立）。④ 与 dial 并排读：验证器/Engine/Resolver 的装配完全一致——两种形态只差"怎么拿到流"。

## 契约

- **验证强制**：完整证书握手必须绑定 Verifier，否则创建期失败；纯 PSK 恢复构建（无验签后端）可省略。
- **拨号原子性**：任一阶段失败自动清理中间资源；失败不发布 Stream；总超时覆盖全部阶段；Dial 可先销毁不影响在途流。
- **状态机**：`HANDSHAKE → READY → CLOSED`（或 FAILED）只向前推进；`XTLS_AGAIN` 只是"需要更多输入或输出空间"，不是错误。
- **发送语义**：`Send` 部分受理以 `iWritten` 报告；TLS 记录层 `SendLimit` 与 TCP 写预算（第 67 章）两级背压；KeyUpdate 自动轮换透明。
- **关闭语义**：`Close` 排队 close_notify 并等对端应答排空；收到对端 close_notify 自动应答并忽略后续数据；错误路径一律 `Abort`。
- **恢复边界**：票据缓存 `ResumeLimit` 0..64（默认 4）；`TakeResume` 转移引用、跨连接缓存归调用方；恢复 ALPN 必须与票据一致；1.2 无恢复；0-RTT 未实现。
- **未实现面**：CertificateRequest、客户端证书认证、0-RTT、TLS 1.2 重新协商——需要客户端证书的场景当前不可用。
- **线程**：回调在 Engine Worker 上执行（第 63 章）；Future 式可在任意线程等待；Verifier 可多会话共享（第 84 章）。

## 避坑

### 坑 1：不配验证器（或验证器配了空信任库）

症状：想"先连上再说"，跳过验证——`xrtTlsClientCreate`/拨号直接失败；或自建验证器配了空 store，同样创建期失败。

原因：XRT 的立场是**不验证不是选项**。开放"可选验证"的库每年贡献大量中间人事故；这里把缺省路径焊死——要么给出验证器，要么明说你在做什么（PSK 恢复构建）。

```c bad
xrtTlsClientConfigInit(&Config);
/* 不配 Verifier：完整握手构建下创建直接失败 */
Session = xrtTlsClientCreate(&Config, pPool);
```

```c good
xx509store* pStore = xrtX509StoreSystem();       /* 系统信任 */
xrtTlsVerifierConfigInit(&VerifierConfig);
VerifierConfig.Store = pStore;
pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
xrtX509StoreFree(pStore);                        /* 深复制后即可释放 */
xrtTlsClientConfigInit(&Config);
Config.Verifier = pVerifier;
Session = xrtTlsClientCreate(&Config, pPool);
xrtTlsVerifierRelease(pVerifier);                /* 会话持引用，自己的可放 */
```

### 坑 2：Send 只看返回值不看 iWritten

症状：大请求偶发被截断——`Send` 返回 `XTLS_OK` 就认为全部发出，实际两级预算只受理了一部分。

原因：TLS 记录层与 TCP 写预算都可能让一次 `Send` 只受理前缀——`XTLS_OK` 的语义是"调用成功"，不是"全部排队"。这与第 67 章写预算的语义完全同构。

```c bad
if ( xrtTlsStreamSend(pStream, pRequest, iSize,
		&iWritten) == XTLS_OK ) {
	/* iWritten < iSize 时后半截丢了 */
}
```

```c good
while ( Sent < RequestSize ) {
	if ( xrtTlsStreamSend(pStream, pRequest + Sent,
			RequestSize - Sent, &iWritten) ==
			XTLS_AGAIN ) {
		return;   /* 预算满：等 Writable 回调续传 */
	}
	Sent += iWritten;
}
```

### 坑 3：不等流终态就销毁 Engine

症状：程序退出时 `xrtNetEngineDestroy` 卡住，或流回调在已释放的上下文上执行。

原因：流的回调跑在 Engine Worker 上；流未到 CLOSED/FAILED 就销毁 Engine，等于抽走回调的执行线程。第 66 章"请求关闭 → 等 CLOSED → Destroy → 最后销毁 Engine"的顺序在 TLS 流上原样成立，外加 Abort 后要等后台释放完成。

```c bad
xrtTlsStreamAbort(pStream);
xrtTlsStreamDestroy(pStream);
xrtNetEngineDestroy(pEngine);   /* 流可能仍在收尾 */
```

```c good
xrtTlsStreamAbort(pStream);
while ( (xrtTlsStreamState(pStream) != XTLS_STREAM_CLOSED) &&
		(xrtTlsStreamState(pStream) != XTLS_STREAM_FAILED) ) {
	xrtThreadYield();
}
xrtTlsStreamDestroy(pStream);
xrtNetEngineDestroy(pEngine);
```

## 练习

### 基础：最小 HTTPS GET

跑通 `dial <某真实站点>`，观察输出：请求发出、响应逐段打印、认证关闭。把 `DialConfig.Timeout` 调到 1 微秒验证总超时路径与错误消息。验收标准：正常路径完整收完响应后程序退出码 0；超时路径错误消息包含阶段信息。

### 进阶：ALPN 协商探测

给 `TlsConfig.Protocols` 配 `{"h2", "http/1.1"}`，在 `Open` 回调里查询并打印协商出的协议名。再改成只提供 `{"http/1.1"}`，对同一站点对比结果。提示：协商结果属于会话的公开属性；握手期 ALPN 不匹配会导致握手失败——记录两种失败形态的区别。

### 挑战：带恢复的两次连接

启用 `XRT_FEATURE_TLS_CLIENT_RESUME`：第一次连接结束后 `TakeResume` 取出票据，第二次拨号把票据交给配置，用 `xrtTlsClientResumed` 确认走了 PSK 路径（无证书消息）。对比两次握手的耗时与字节数。验收标准：恢复连接确实跳过证书阶段；票据 ALPN 与第二次配置不一致时能解释失败原因；票据内存最终全部释放（第 6 章统计）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 托管拨号 | `TlsDial`：DNS→竞连→握手一次完成；失败自动清理、不发布半开流 |
| 配置三件套 | Verifier（必须显式）/ ServerName（自动取主机名）/ ALPN 数组 |
| 双形态 | 回调式（Events：Open/Read/Writable/Close）/ Future 式（DialAsync+WaitFor） |
| 发送语义 | 部分受理看 `iWritten`；TLS SendLimit + TCP 写预算两级背压 |
| 接收消费 | `Available/Buffer/Front/Consume` 零拷贝逐 Span，与第 64 章同词汇 |
| 关闭协议 | `Close` 认证关闭（close_notify 排空）；`Abort` 错误路径专用 |
| 退出顺序 | 请求关闭 → 等 CLOSED/FAILED → Destroy 流 → 最后销毁 Engine |
| KeyUpdate | 记录临界自动换钥；主动用 `TlsClientKeyUpdate`；AGAIN 无半更新态 |
| 会话恢复 | 票据+PSK+key share；`TakeResume` 转移引用；ALPN 须与票据一致；1.2 无恢复 |
| 未实现 | 客户端证书、0-RTT、1.2 重新协商——这些场景当前不可用 |
