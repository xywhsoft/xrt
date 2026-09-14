---
num: 84
slug: tls-handshake
title: 握手时序图解：从 ClientHello 到 Finished
volume: 卷八 安全
type: practice
lead: TLS 1.3 的四条核心消息、密钥调度的 HKDF 链、HelloRetryRequest 与版本协商——把握手从黑盒变成可读的时序图。
api: tls, crypto
---

## 导读

第 82 章你按下一个函数、握手自己跑完了；本章打开黑盒。TLS 1.3 的握手其实只有四条核心消息：ClientHello、ServerHello、（EncryptedExtensions、Certificate、CertificateVerify、）Finished，再加客户端一条 Finished——每条消息"谁发的、带什么、验证什么"都讲清楚，配上贯穿全程的**密钥调度链**（transcript 摘要 → handshake secret → 双向 traffic secret → 应用 secret——全部 HKDF 驱动，第 74/77 章的原语在此汇合）。然后是三个工程分支：HelloRetryRequest（服务端换组）、版本与套件协商（1.2/1.3 双栈）、KeyUpdate（长连接换钥）。本章示例用密钥交换原语和证书消息编解码两个独立入口，把握手的两块核心机械拆开单测——第 77 章的会话骨架在协议层的完全展开。

## 引入

调试 TLS 失败时，"握手挂了"四个字没有任何信息量。是 ClientHello 的组不被支持（HelloRetryRequest 循环）？ServerHello 后证书验证失败？CertificateVerify 签名不成立？还是 Finished 摘要不匹配（transcript 被中间设备改了字节）？每一类失败对应不同的处置——而不懂时序的人只能重启碰运气。

理解时序的钥匙是一个洞察：**TLS 1.3 握手的每一步都在"累积 transcript"**——双方把收发的每条握手消息喂进同一份运行摘要，密钥从 transcript 派生、Finished 验证 transcript、恢复票据绑定 transcript。改任何一条消息的一个字节，后面所有密钥全变——这就是"握手防篡改"的机制本体，不是魔法是哈希链。带着这个视角读四条消息，每个字段都在回答"它为什么必须在这里"。

## 概念

### 四条核心消息的时序图

```diagram flow
- ClientHello（明文）：最高支持版本+随机数+会话ID+套件列表+组列表+签名方案列表+key share（首选组公钥）+SNI/ALPN/PSK
- ServerHello（明文）：选定版本+套件+随机数+回显会话ID+选定key share —— 双方在此算出 handshake secret
- [服务端密文开始] EncryptedExtensions：SNI 确认、ALPN 选定、组协商补充
- Certificate：叶+中间证书链（条目式，每条可带 OCSP 扩展）
- CertificateVerify：用证书私钥对"角色前缀+transcript 摘要"签名 —— 身份证明本体
- Finished：对"加入本条之前的 transcript"的 HMAC —— 防篡改证明本体
- [客户端验证链与摘要] 客户端 Finished（服务端写密钥保护）：双向确认后切换应用密钥
```

三条阅读线。**明文/密文边界**在 ServerHello 之后——服务端的 EE/Cert/CV/Finished 全部加密，窃听者只见证书长度不见内容；**每条消息验证什么**：EE 验证协商一致性、CV 验证身份（第 80/81 章的验证在这里触发）、Finished 验证全局完整性；**密钥切换点**两处——ServerHello 后双方各自派生握手密钥（EE 起加密）、客户端 Finished 后切换应用密钥。

### 密钥调度：一条 HKDF 链

```diagram flow
- ECDHE：ClientHello 与 ServerHello 的 key share → 共享秘密（第 76 章 ECDH）
- Early Secret = HKDF-Extract(0, PSK 或 0)
- Handshake Secret = HKDF-Extract(Early, ECDHE 共享秘密)
- 双向 handshake traffic secret = HKDF-Expand(Handshake, "c hs traffic"/"s hs traffic", transcript摘要)
- Master Secret = HKDF-Extract(Handshake, 0)
- 双向 application traffic secret = Expand(Master, "c ap traffic"/"s ap traffic", 新transcript摘要)
- 每方向 secret → HKDF-Expand → 记录层的 key 与 IV（第 75 章 AEAD 就位）
```

对照第 77 章的会话示例：同样的"ECDH → HKDF → AEAD"骨架，TLS 把它做成多级阶梯——每级 Extract/Expand 都把**当时的 transcript 摘要**绑进密钥，所以密钥不仅锁住秘密，还锁住"我们聊过的每一句话"。收发方向各有独立 secret（客户端写/服务端读用同一个，反向亦然）——单向换钥（KeyUpdate）因此成为可能。客户端临时私钥在握手密钥切换成功后**立即擦除**——前向保密的落点。

### HelloRetryRequest：服务端说"换个组再来"

客户端首选组（如 x25519）服务端不支持时，服务端回 HRR 要求换组重发 ClientHello。XRT 客户端与服务端支持单次 HRR——重发的新 ClientHello 换成服务端指定的组（如 P-256）并带 `h2` 重试标记。对应用完全透明；对诊断重要：连接日志里"两次 ClientHello"不是重连，是 HRR。

### 版本与套件协商

双栈（1.2/1.3）协商规则：对端 offered 列表 ∩ 本地支持 → **最高版本**；套件再过三重过滤——对端 offered 顺序 + 本地支持 + **身份类型兼容**（RSA 身份排除纯 ECDSA 套件）。本地策略数组（第 85 章）就是这两步的输入基线；服务端每连接都跑这两步。`xrtTlsVersionSelect/CipherSelect` 把协商独立成可单测的纯函数——本章示例演示全流程。TLS 1.2 侧的要求：EMS（extended master secret）与 RFC 5746 空初始绑定必须，重新协商不提供——老协议的已知弱点被工程决策直接关闭。

### KeyUpdate：长连接的换钥节奏

应用数据流到一定量，记录层自动换钥（对应用透明）；也可主动 `xrtTlsClientKeyUpdate(方向)`。规则：KeyUpdate 消息独占一条记录；`update_not_requested` 只换读方向密钥；`update_requested` 先用旧写密钥排队应答再切换——**任何 AGAIN 中断都不产生半更新状态**（旧密钥、序列号、队列原样保留，输出排空后重试）。已排队数据的 epoch 顺序由线路队列保证——第 75 章"nonce 计数器"纪律在协议层的自动化。

### 证书消息：条目模型

TLS 1.3 的 Certificate 按**条目**组织（每条 = DER 证书 + 可选扩展如 OCSP 状态），1.2 是裸列表——`xrtTlsCertificateEncode/Parse/Entries` 统一处理两个版本（首参数给版本）。条目是 DER 视图，解析交给第 79 章的 `xrtX509Parse`。客户端状态机对条目扩展有严格规则：未请求的每条目扩展直接拒绝。

## 示例

### 第一个完整程序：密钥交换的元数据驱动双向闭环

下面的程序来自 `examples/tls/key_exchange/main.c`——握手核心机械（key share 生成与派生）的独立单测：

```embed path="examples/tls/key_exchange/main.c" title="examples/tls/key_exchange/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/key_exchange/main.c -lws2_32 -liphlpapi
group=29 private=32 public=32 shared=32
```

**刚才发生了什么。** ① `exampleGroup` 按 x25519 → P-256 → x448 → P-384 的顺序找**当前构建可用**的组（`xrtTlsGroupAvailable` 反映裁剪与后端能力）——group=29 是 x25519 的 TLS 组号。② `xtlsgroupinfo` 给出三种缓冲的精确尺寸——私钥 32/公钥 32/共享 32（x25519 全 32；P-384 会是 48/97/48）：**缓冲按元数据开，不猜常数**，同一份代码跑任何组。③ 双向 `Derive` 后 memcmp 相等——ECDH 对称性（第 76 章）在 TLS 组封装下的验证。这就是 ClientHello/ServerHello 里 key share 扩展的全部数学：Generate 两次、交换公钥、Derive 得到 ECDHE 共享秘密——密钥调度链的第一环。

### 第二个完整程序：Certificate 消息的编码与条目遍历

第二个程序来自 `examples/tls/messages/main.c`，证书链 → 消息正文 → 条目游标的完整往返：

```embed path="examples/tls/messages/main.c" title="examples/tls/messages/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/messages/main.c -lws2_32 -liphlpapi
certificate[0]: 3 bytes
certificate[1]: 3 bytes
```

**刚才发生了什么。** ① 两遍式编码：`CertificateSize` 先测正文长度（`request_context` 空视图是客户端方向；服务端方向的 CV 上下文非空），`CertificateEncode` 写入——两元素链（叶+签发者，这里是 3 字节玩具 DER）编码成 TLS 1.3 条目格式。② `CertificateParse` 产消息视图、`Entries` 建游标、`Read` 三态逐条——与 DER/PEM 游标同一套词汇（第 78 章）。③ 每条 `Entry.Data` 是 **DER 视图**——真正的解析交给第 79 章，两层各司其职：TLS 层管条目结构，X.509 层管证书语义。代理、抓包器、证书透明度日志都用这对入口处理线路证书。

## 契约

- **协议范围**：TLS 1.2/1.3 双栈；1.2 要求 EMS 与空初始绑定、无重新协商；Ed448 不发布；P-521/SHA-512 不进 1.3 offer。
- **transcript 不可绕过**：全部握手消息进摘要；密钥派生与 Finished 都绑定 transcript；CertificateVerify 的签名对象是"角色前缀 + 加入 CV 前的摘要"。
- **密钥切换原子性**：任一步失败进 FAILED、不发布半组密钥；客户端临时私钥在切换成功后立即擦除（前向保密）。
- **HRR**：客户端与服务端支持单次；重试 ClientHello 换组并带标记；对应用透明。
- **协商规则**：版本取交集最高；套件过 offered 顺序+本地支持+身份兼容三重过滤；策略数组是基线（第 85 章）。
- **KeyUpdate**：独占记录；两种请求模式；应答用旧写密钥；AGAIN 无半更新态；队列保证 epoch 顺序。
- **证书条目**：1.3 条目式（可带扩展）、1.2 裸列表，Encode/Parse 统一入口；条目 DER 视图交 X.509 层；未请求的条目扩展拒绝。
- **错误映射**：失败进 `XTLS_STATE_FAILED` 并带结构化错误；`XTLS_AGAIN` 只是"要更多输入/输出空间"。

## 避坑

### 坑 1：把 HRR 当成连接失败重试

症状：监控里看到同一连接两条 ClientHello，误判为"客户端反复重连"，触发告警风暴；或自写代理把 HRR 当错误断开。

原因：HelloRetryRequest 是协议内的正常分支——服务端要求换组重发。1.3 允许（XRT 支持单次）；超过一次才是异常。

```c bad
/* 抓包/日志层：见到第二次 ClientHello 就告警 */
if ( hello_count(client) > 1 ) {
	alert("client stuck in reconnect loop");
}
```

```c good
/* HRR 有明确特征：ServerHello 的特殊随机数值 + h2 重试标记。
   会话状态机已把它折叠成正常握手——日志层看状态而不是数包 */
if ( tls_state(session) == XTLS_STATE_FAILED ) {
	alert(failure_reason(session));
}
```

### 坑 2：想当然地"帮"协议优化 transcript

症状：自写中间层（代理、日志、加速器）改动了握手记录的边界——合并或拆分记录——之后 Finished 永远验证失败。

原因：transcript 摘要的对象是**握手消息**而非记录，但消息完整性依赖记录层的字节保真；任何对线路字节的"优化"（重组、重编码）都会改变 transcript，Finished 的 HMAC 因此失配——这正是协议要检测的篡改。

```c bad
/* 中间层"整理"记录边界后转发 */
merge_small_records(&Inbound);   /* transcript 变了 */
```

```c good
/* 透明转发原始字节；要解析就用记录解析层只读处理 */
/* （第 87 章的记录层只读入口就是为这类工具准备的） */
```

### 坑 3：在 1.3 策略里塞进 TLS 1.2 专用方案

症状：`xrtTlsPolicyValid` 拒绝配置——"签名列表包含 1.2 专用方案"；或反过来，纯 1.2 部署被塞了 1.3-only 套件。

原因：策略校验保证每条列表与启用版本自洽：每个套件对应启用版本、非空签名列表能用于至少一个启用版本——1.2 的 PKCS#1 方案混进纯 1.3 策略会被拦截。这是策略层的价值：把配置错误拦在启动期。

```c bad
static const xtlsversion Versions[] = { XTLS_VERSION_13 };
Policy.Versions = Versions;
Policy.VersionCount = 1;
/* SignatureCount 没改：还是默认全家（含 1.2 专用）→ Valid 拒绝 */
```

```c good
static const xtlsversion Versions[] = { XTLS_VERSION_13 };
static const xtlssignature Sigs13[] = { /* 只放 1.3 方案 */ };
Policy.Versions = Versions;
Policy.VersionCount = 1;
Policy.Signatures = Sigs13;
Policy.SignatureCount = /* 与数组一致 */;
if ( !xrtTlsPolicyValid(&Policy) ) { /* 启动期报配置错误 */ }
```

## 练习

### 基础：跑通两组密钥交换

把 `exampleGroup` 的组顺序改成 P-256 优先，重跑 key_exchange——观察 group 号与三种尺寸的变化（97 字节公钥）。再注释掉所有组验证 `Available` 的拒绝路径。验收标准：两种组都闭环成功；尺寸变化与 `xtlsgroupinfo` 文档一致。

### 进阶：证书消息版本对照

用 `xrtTlsCertificateEncode/Parse` 分别以 `XTLS_VERSION_12` 与 `XTLS_VERSION_13` 编码同一条目链，dump 两份正文的十六进制对比结构差异（1.2 无 request_context 与条目扩展）。验收标准：两份正文都能被对应版本的 `Parse+Entries` 遍历；能指出 1.3 多出的字段。

### 挑战：握手时序观察器

用记录解析层（第 87 章）或原始 socket 抓取一次完整握手（本机 `dial` 对任意站点），按本章时序图逐条标注：每条记录的类型、明文/密文边界、握手消息类型序列。验收标准：标注出的消息序列与本章时序图完全对应（含可能的一次 HRR）；能指出 ServerHello 之后哪些内容你已经看不见了（密文开始）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 四消息 | ClientHello / ServerHello /（EE+Cert+CV）/ Finished + 客户端 Finished |
| 明密边界 | ServerHello 之后服务端消息全加密；窃听只见长度 |
| transcript | 全部握手消息的运行摘要；密钥与 Finished 都绑定它 |
| 密钥调度 | ECDHE → Extract 链（Early/Handshake/Master）→ 双向 traffic secret → key/IV |
| 方向独立 | 收发各有 secret；单向换钥（KeyUpdate）由此可能 |
| 前向保密 | 客户端临时私钥在握手密钥切换后立即擦除 |
| HRR | 服务端换组请求；单次；新 ClientHello 带标记；对应用透明 |
| 协商 | 版本取交集最高；套件过 offered+本地+身份兼容三重过滤 |
| KeyUpdate | 独占记录；应答用旧写密钥；AGAIN 无半更新；epoch 顺序有保证 |
| 证书条目 | 1.3 条目式 / 1.2 裸列表统一入口；条目 DER 视图交 X.509 层 |
| 1.2 边界 | EMS 必须无重新协商；1.2 专用方案进不了纯 1.3 策略 |
