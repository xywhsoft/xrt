---
num: 118
slug: xssh-kex
title: SSH（二）：密钥交换
volume: 卷十一 其他扩展库
type: practice
lead: KEXINIT 协商、ECDH 报文、SHA-256 transcript 与 A-F 派生、主机签名验证与 NEWKEYS 切换——一次完整的 SSH 握手。
api: xssh-ssh_kex_session, xssh-ssh_kex_exchange, xssh-ssh_kexinit
---

## 导读

传输层（第 117 章）提供了加密的包管道，但密钥从哪来？**KEX（密钥交换）**回答这个问题——它是 SSH 版的"TLS 握手"（第 83 章），但形态更分步。`ssh_kex_session` 把双方 KEXINIT、Curve25519、SHA-256 exchange hash、Ed25519 主机签名验证、A-D 密钥派生与 NEWKEYS 方向切换组合成**确定性状态机**——不创建 socket/Engine/任务/等待对象，不保存 KEXINIT 大缓冲。底层拆成四层独立模块：ECDH 报文（消息号+SSH string 的编解码）、SHA-256 transcript（流式 exchange hash 与 A-F 密钥扩展）、Curve25519 原语（纯数学、无随机依赖）、安全随机密钥对（独立裁剪层）。学完本章你读得懂一次真实 SSH 握手的每条报文。

## 引入

SSH 握手与 TLS 1.3 的同与异。**同**：ECDHE（x25519 曲线）+ transcript 哈希 + 派生链 +Finished/NEWKEYS 切换——第 83 章的"密钥从 transcript 派生、篡改任何消息全变"思想一致。**异**：SSH 的主机签名验证与密钥信任是**两步**——KEX 会话先做密码学验签（这个签名真的是这个主机密钥签的吗），然后发 `VERIFY_HOST_KEY` 事件让**应用**做信任判断（这个主机密钥是我认识的吗——known_hosts，第 119 章）；TLS 把两步捏在证书验证里。这个分步让 SSH 可以无证书体系运行——信任来自"你第一次连时记下了这个密钥"（TOFU）或预先分发。

分步还有第三个受益者：**测试与专用设备**。`xrtSshKexSessionBeginWithPrivate` 显式注入临时私钥——确定性核心可测；随机便利层 `Begin` 独立成模块（只额外引入系统安全随机源）——标准向量测试、专用密钥设备不必拖随机依赖。这是"确定性核心+便利层分离"在 KEX 的贯彻。

## 概念

### KEXINIT 与算法协商

连接建立（版本串交换后）双方互发 KEXINIT：各自支持的 kex 算法/加密/MAC/压缩/主机密钥算法列表（按优先级）。协商规则与 TLS 不同——**SSH 取客户端列表中服务端也支持的第一个**（客户端优先，与第 93 章子协议协商同款）。`ssh_kexinit` 模块提供 KEXINIT 报文的构建（`xrtSshKexInitWrite`——写协商列表）与解析。会话对象吸收两份 KEXINIT 后确定本轮算法组合。

### ECDH 报文与 Curve25519

```diagram flow
- C→S ECDH_INIT：Q_C（客户端临时公钥，32B）
- S→C ECDH_REPLY：K_S（主机公钥 blob）|| Q_S（服务端临时公钥）|| 签名(exchange hash)
- 双方：SharedSecret = x25519 标量乘(自己私钥, 对方公钥)——服务端拒绝低阶公钥的全零结果
```

报文层（`xrtSshEcdhInitWrite/Read`、`EcdhReplyWrite/Read`）只处理消息号与 SSH string 编码——结果借用原 payload、拒绝错误消息号/截断/尾随。Curve25519 原语三入口：`xrtSshCurve25519Public`（私→公）、`Curve25519Shared`（共享秘密，拒绝低阶公钥全零——第 75 章恶意公钥防御）、`Curve25519KeyPair`（随机密钥对——独立裁剪）。**私钥清理**：transport 使用结束必须 `xrtSecureZero`。

### Exchange Hash 与 A-F 派生

`xrtSshKexHashSha256` **流式**写入 transcript（不构造中间大缓冲）：`V_C || V_S || I_C || I_S || K_S || Q_C || Q_S || K`——版本串、双方 KEXINIT、主机公钥、双方临时公钥、共享秘密；前七项 SSH string 编码，K 编码为**规范非负 mpint**（RFC 8731 的细节：x25519 的 32 字节输出直接按网络序整数解释、不反转字节再 mpint 编码——契约原文级精确）。`xrtSshKexDeriveSha256` 实现 RFC 4253 的 **A-F 密钥扩展**（初始 IV×2、C2S/S2C 加密密钥、MAC 密钥×2）：`HASH(K || H || char || session_id)` 链式扩展、支持超 32 字节输出；输出不得与输入重叠、共享秘密必须非零。

### 主机签名：验签与信任的两步

服务端侧：`xrtSshKexSessionExchangeHash` 取摘要 → 本地私钥/HSM 签名 → 签名 blob 交给 `EcdhReplyPrepare`——**该函数用公开主机密钥复验签名**，错误签名进不了线路（服务端自检）。客户端侧：ECDH_REPLY 验签通过后主机公钥**复制到调用方存储**、产生 `XSSH_KEX_EVENT_VERIFY_HOST_KEY` 事件——known_hosts/证书/应用策略接受则 `xrtSshKexSessionHostKeyAccept`，拒绝则关闭 transport 并 `xrtSshKexSessionFail`。**两步的意义**：密码学验证（库）与信任判断（策略）分离——策略可以是 TOFU、known_hosts 比对、证书链（第 119/120 章展开）。

### 事务与方向切换

与 transport core 同款的事务纪律：ECDH_INIT/ECDH_REPLY/NEWKEYS 都先 `Prepare`——transport core 可靠提交包后再 `WriteCommit`（取消则 `WriteAbort`）；接收先 core 认证、再 `ReadPrepare`、按相同顺序提交 core 与 session；`ReadAbort` **终止会话**（已认证输入不可回滚——第 117 章 ReadAbort 关 core 的会话层呼应）。**密钥切换方向独立**：本端 NEWKEYS 提交后 `ActivateWrite`（写向切换）、对端认证后 `ActivateRead`——函数按角色选 C2S/S2C 材料，**core 接管后清除会话中的密钥副本**（最小驻留——第 76 章纪律）。Transcript 四段视图须本轮 KEX 内有效——要脱离网络输入生命周期先用 `KexTranscriptMeasure/Write` 精确复制。

## 示例

### 第一个完整程序：会话对象的资源形态

下面的程序来自 `examples/kex_session`——KEX 会话的零负担声明：

```embed path="extlibs/xssh/examples/kex_session/main.c" title="extlibs/xssh/examples/kex_session/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/kex_session/main.c -lws2_32 -liphlpapi
（输出 KEX 会话结构尺寸的自检结果）
```

**刚才发生了什么。** ① `xrtSshKexSessionInit(&Session, XSSH_ROLE_CLIENT)` 栈上建客户端会话——与第 117 章 transport core 同款声明：sizeof 输出即"这不是堆怪兽"的文档。② 头注释点明："不创建网络、任务或大块固定缓冲"——KEX 的全部状态（协商结果、transcript 上下文、派生中间量）都在这个栈对象里。③ 真实驱动序列（Begin→两份 KEXINIT→ECDH 往返→主机验证事件→NEWKEYS→双 Activate）由上层客户端组装——第 123 章的运行时把这串状态机接到 socket；本示例确认的是"零件本身可预测"。

### 第二个完整程序：KEXINIT 的构建

第二个程序来自 `examples/kexinit`——协商列表的写出：

```embed path="extlibs/xssh/examples/kexinit/main.c" title="extlibs/xssh/examples/kexinit/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/kexinit/main.c -lws2_32 -liphlpapi
（输出 KEXINIT 报文尺寸与所选 kex 算法名的自检结果）
```

**刚才发生了什么。** ① `xrtSshKexInitWrite` 把算法列表（kex/加密/主机密钥/压缩/MAC 的名字-逗号列表）编码成 SSH 报文——`Writer.Size` 是报文尺寸、打印的算法名来自构建后的选择或首选项。② KEXINIT 是**握手的身份声明**：列表顺序即偏好顺序（协商取客户端序）——配置它就是配置客户端的算法立场（第 84 章策略的 SSH 对应物）。③ 配套示例族逐层可测：`kex_curve25519`（纯原语+标准向量）、`kex_ecdh`（报文层）、`kex_sha256`（transcript 与派生的向量）、`kex_exchange`（四层组合的静态往返）、`*_random` 族（随机便利层的确定性对照）——"每层都有独立样本"是分层设计的验收形态。

## 契约

- **确定性核心**：会话不创建 socket/Engine/任务/等待对象；随机便利层独立模块（只引入系统安全随机源）。
- **协商规则**：客户端列表序 ∩ 服务端支持，取客户端序首个；KEXINIT 由 `ssh_kexinit` 独立编解码。
- **ECDH 报文**：只处理消息号与 string；借用 payload；拒绝错号/截断/尾随。
- **Curve25519**：Shared 拒绝低阶公钥全零；KeyPair 随机层独立裁剪；私钥用毕 SecureZero。
- **Exchange Hash**：流式写入八段 transcript（V_C..K）；K 为规范非负 mpint（x25519 输出不反转直接 mpint）；共享秘密非零。
- **A-F 派生**：RFC 4253 扩展链；支持超 32B 输出；输出不与输入重叠。
- **验签两步**：服务端 Reply 前自验签名；客户端验签后产 VERIFY_HOST_KEY 事件——信任判断归应用（Accept/Fail）。
- **事务纪律**：Prepare→core 提交→Commit/Abort；ReadAbort 终止会话（已认证输入不可回滚）。
- **方向切换**：ActivateWrite/Read 独立；按角色选材料；core 接管即清会话密钥副本。
- **Transcript 生命周期**：四段视图本轮有效；脱离需先精确复制（Measure/Write）。

## 避坑

### 坑 1：跳过 VERIFY_HOST_KEY 事件直接 Accept

症状：代码图省事在事件回调里无条件 `HostKeyAccept`——中间人攻击畅通（任何主机密钥都被"信任"）。

原因：验签（密码学）与信任（策略）两步中，第二步才是防中间人的那步——签名验证只证明"密钥与签名一致"，不证明"这是你要连的服务器"。

```c bad
case XSSH_KEX_EVENT_VERIFY_HOST_KEY:
	xrtSshKexSessionHostKeyAccept(&Session);  /* 无条件信：MITM 畅通 */
	break;
```

```c good
case XSSH_KEX_EVENT_VERIFY_HOST_KEY:
	if ( known_hosts_check(Host) == TRUST ) {   /* 第 119 章的判断 */
		xrtSshKexSessionHostKeyAccept(&Session);
	} else {
		xrtSshKexSessionFail(&Session, REJECT);
	}
```

### 坑 2：transcript 视图跨过 KEX 轮次使用

症状：第二轮 rekey 时哈希错乱——第一轮的 KEXINIT 视图早已失效。

原因：transcript 四段视图借用网络输入缓冲，**本轮 KEX 内有效**；rekey 新一轮有新 transcript。

```c bad
/* 第一轮的 I_C 视图存下来，第二轮 Activate 时还在用 */
```

```c good
/* 需要跨轮保存：先精确复制 */
xrtSshKexTranscriptMeasure(...);
xrtSshKexTranscriptWrite(...);   /* 复制到会话工作区 */
```

### 坑 3：临时私钥忘了清零

症状：内存审计发现 x25519 私钥残留——密钥材料生命周期违规。

原因：KEX 临时私钥是敏感材料（虽是短命）；契约明示"transport 使用结束后必须 xrtSecureZero"。

```c bad
/* KEX 完成，私钥数组自然离开作用域——内容还在栈上 */
```

```c good
xrtSshKexSessionActivateRead(&Session, &Core);
/* core 接管后清理会话密钥副本（自动），自己的临时私钥手动清： */
xrtSecureZero(Private, sizeof(Private));
```

## 练习

### 基础：四层静态往返

跑通 `kex_curve25519`、`kex_sha256`、`kex_exchange` 三示例并用 RFC 7748/8731 的测试向量对照（私钥注入形态）。验收标准：共享秘密与 exchange hash 都命中标准向量。

### 进阶：主机验证事件的策略注入

确定性驱动一轮完整 KEX（注入私钥），在 VERIFY_HOST_KEY 事件分别注入"信任/拒绝"两策略——观察 Accept 后 NEWKEYS 流程与 Fail 后 transport 关闭。验收标准：两路径的最终 `KexComplete`/关闭状态与契约一致。

### 挑战：rekey 循环

完成首轮后触发 rekey（第 117 章 rekey 预算）：新 KEXINIT→新 ECDH→双 Activate——连续三轮。验收标准：每轮密钥都变化（派生输出对比）；旧密钥副本每轮被清（内存扫描验证）；transcript 每轮独立。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 四层拆分 | ECDH 报文 / SHA-256 transcript / Curve25519 原语 / 随机密钥对（独立裁剪） |
| 协商 | 客户端列表序 ∩ 服务端支持；KEXINIT 独立编解码 |
| exchange hash | 流式八段（V_C..K）；K 规范非负 mpint；x25519 输出不反转 |
| A-F 派生 | RFC 4253 扩展链；初始 IV×2+加密×2+MAC×2；超 32B 支持 |
| 验签两步 | 库验密码学；应用判信任（VERIFY_HOST_KEY 事件） |
| 服务端自验 | EcdhReplyPrepare 用公钥复验——错误签名不上线 |
| 低阶防御 | Curve25519Shared 拒绝全零结果（恶意公钥） |
| 事务 | Prepare→core 提交→Commit/Abort；ReadAbort 终止会话 |
| 方向切换 | ActivateWrite/Read 独立；接管即清副本；双活才完成 |
| 随机分离 | BeginWithPrivate 确定性 / Begin 随机便利层独立模块 |
