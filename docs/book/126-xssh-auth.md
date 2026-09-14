---
num: 126
slug: xssh-auth
title: SSH（四）：认证
volume: 卷十一 其他扩展库
type: practice
lead: USERAUTH 编排、password/publickey/keyboard/hostbased 四方法、免签名 probe 与外部 signer、纯预算层 auth guard——证明你是谁。
api: xssh-ssh_auth_session, xssh-ssh_auth_publickey, xssh-ssh_auth_keyboard
---

## 导读

KEX 之后连接是加密的，但你还没证明身份。SSH 认证（RFC 4252）的形态是**方法菜单**：客户端逐个试服务端允许的方法——password（用户名口令）、publickey（私钥签名——免签名 probe 是它的精华）、keyboard-interactive（多轮挑战——OTP/PAM 的载体）、hostbased（主机担保）。xssh 的分层：**方法模块**（password/publickey/keyboard/hostkey 四份契约卡——只管各自 payload 的构建与解析，不存凭据不管轮次）在**公共消息层**（`ssh_auth_message`——USERAUTH_REQUEST/FAILURE/SUCCESS/BANNER 的编解码）之上；**会话编排**（`ssh_auth_session`——service 请求、阶段校验、预算、方向一致性）把它们与 transport core（第 121 章）缝合；**预算层**（`ssh_auth_guard`——纯资源预算，共享于客户端服务端）限尝试数/轮次/字节/时间。未知方法消息（60..79）统一透传——**新认证方法直接组合现有会话**、不改状态机。

## 引入

publickey 方法的 **probe 机制**值得开场：客户端先发**免签名**的"我有这个密钥吗？"探问（`AuthPublicKeyWrite`）——服务端回 PK_OK（接受此密钥）或 FAILURE；只有确认接受才发**带签名**请求（省一次昂贵的签名，也避免对不支持的服务端泄露签名原文）。签名原文由 `AuthPublicKeySignDataWrite` 生成 `string(session_id) || USERAUTH_REQUEST`——**连续结果交给调用方的 signer**：硬件密钥、ssh-agent、XRT 密码算法或自定义实现。这又是"确定性核心+外部 signer"的第 83 章模式。

keyboard-interactive 的多轮形态是第二个看点：服务端发挑战（prompt 列表——"密码？""OTP 第 3 位？"）、客户端回响应、可能再挑战——**多因子认证的协议载体**。约束：同一时刻最多一个未完成挑战；总尝试/轮次/字节/截止时间由预算层限——"防暴力"是预算不是运气。

## 概念

### 会话编排：分层与事务

```diagram flow
- 方法模块：向调用方 payload 缓冲写报文（无分配、失败原子）
- 会话层：WritePrepare（验证阶段+预留预算）→ transport 构建线路包 → 网络可靠接管
  → 提交 core → WriteCommit；取消则先弃 transport 写事务再 WriteAbort（预算不消费）
- 读方向：core 验证 packet → ReadPrepare（阶段校验+视图借出）→ 上层接受
  → 先提 core 再提 session；拒绝则 ReadAbort+关 transport（已认证输入不可回滚）
```

职责四分：方法模块管 payload、会话管阶段与预算、core 管 framing 加密顺序、驱动管网络——**没有一层越界**。banner（服务端欢迎/警告文本）不改变主事件；failure 允许新请求（方法菜单的"下一个"）；success 只能由 server 方向提交（方向一致性——客户端不能"自我宣布成功"）。rekey 期间驱动把 transport 控制消息（KEXINIT/NEWKEYS/DISCONNECT）先路由给 KEX/transport 状态机、完成后**继续同一个 auth session**——认证跨 rekey 存活。

### 四方法卡

- **password**（`ssh_auth_password`）：USERAUTH_REQUEST + 明文口令（连接已加密——第 106 章 Basic 的对照：加密隧道内是可接受的）；`PasswordWrite/Read` 直构直解。
- **publickey**：probe（免签名）→ PK_OK → signed（签名原文 SignDataWrite→外部 signer→签名入请求）；公钥 blob 算法与请求算法可不同（RSA SHA-2 请求用 `ssh-rsa` blob——协议的现实兼容细节），但**签名 blob 算法必须与请求一致**。
- **keyboard-interactive**（RFC 4256）：请求（空 language 规范建议）→ 挑战（name/instruction/prompt 列表）→ 响应；"完整预验证+借用迭代"接口（prompt 非空 UTF-8、response 可空；数量由协议字段与调用方缓冲定，**无库内固定上限、不分配对象数组**）；多轮由状态机保证单挑战 + 预算限总轮次。
- **hostbased**（`ssh_auth_hostbased`）：客户端**主机**的密钥签名 + 主机名担保——集群环境的机器互信；格式与 publickey 同族。

### 预算层：auth guard

`ssh_auth_guard` 是纯资源预算层——不解析消息不存用户名：**尝试数**（USERAUTH_REQUEST 计数）、**交互轮次**（server 方法消息计轮、client 响应计消息）、**字节预算**、**截止时间**。共享于两端：服务端防暴力（限客户端的尝试）、客户端防服务器拖拽（限自己被坑的时长）——预算是双向的防御。

### 视图纪律（又一次）

Request/Failure/Banner/Method 的视图**借用 packet、只在读提交前有效**——异步认证后端（问 OTP 要时间）必须复制要跨等待保存的字段。与第 96/104/110 章同一纪律——SSH 层是它的第四次出场，你已经在每个网络层见过它。

## 示例

### 第一个完整程序：公钥认证的报文构建

下面的程序来自 `examples/auth_publickey`——probe 与公钥格式的组合：

```embed path="extlibs/xssh/examples/auth_publickey/main.c" title="extlibs/xssh/examples/auth_publickey/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/auth_publickey/main.c -lws2_32 -liphlpapi
（输出 algorithm=ssh-ed25519 与报文尺寸的自检结果）
```

**刚才发生了什么。** ① 第 123 章 `hostkey` 示例产的公钥 blob 在这里成为认证材料——**同一份编码贯穿握手（K_S）、存储（known_hosts）、认证（publickey blob）**，xssh 的格式层复用形态。② `xrtSshAuthPublicKeyWrite` 构建免签名 probe——打印的 algorithm 与 bytes 是"这个密钥能不能用"的问询报文。③ 真实序列（probe→PK_OK→SignData→signer→signed request）的每一步都是独立入口——外部 signer 在 SignData 与 signed 之间插入（HSM/agent 的插槽）。配套：`auth_password`（口令报文）、`auth_keyboard`（挑战/响应的迭代接口）、`auth_hostbased`（主机担保形态）、`auth_message`（公共层）、`auth_guard`（预算四线）。

### 第二个完整程序：会话起点与 service 请求

第二个程序来自 `examples/auth_session`——编排层的起点：

```embed path="extlibs/xssh/examples/auth_session/main.c" title="extlibs/xssh/examples/auth_session/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/auth_session/main.c -lws2_32 -liphlpapi
（输出会话结构尺寸、service 请求报文尺寸与初始事件的自检结果）
```

**刚才发生了什么。** ① `xrtSshAuthSessionInit(&Session, XSSH_ROLE_CLIENT)` 栈上会话——sizeof 输出（与 117/118 章同款零负担声明）。② `xrtSshServiceRequestWrite` 写 `ssh-userauth` service 请求——**认证的第一条报文**（先声明用哪个 service 再谈方法）；`Writer.Size` 是报文尺寸。③ `xrtSshAuthSessionEvent` 查当前主事件——会话是**事件驱动**的（当前该发什么/等什么），驱动层按事件分派方法模块——第 127 章运行时就是这个分派的完整形态。④ Begin 的前置：**只接受已完成首轮 KEX 的同角色 transport**——分层时序的强制的（没加密谈什么认证）。

## 契约

- **职责四分**：方法管 payload、会话管阶段+预算、core 管 framing、驱动管网络——互不越界。
- **写事务**：方法写 payload→SessionPrepare（阶段+预算）→core 包→网络接管→双提交；取消先弃 core 再 Abort（预算不消费）。
- **读事务**：core 验证→SessionPrepare（视图借出）→接受双提交；拒绝 ReadAbort+关 transport。
- **视图纪律**：Request/Failure/Banner/Method 借 packet，读提交前有效——异步后端复制所需。
- **banner/failure/success**：banner 不改主事件；failure 允许新请求；success 仅 server 方向提交。
- **publickey**：probe 免签名（PK_OK 后才签）；签名原文 `session_id||REQUEST` 交外部 signer；blob 算法可异于请求算法、签名算法必须一致。
- **keyboard**：单未完成挑战；prompt 非空 UTF-8/response 可空；无库内上限不分配数组；多轮由状态机+预算管。
- **未知方法**（60..79）：透传 `PACKET_METHOD` + Method 借出——新方法组合现有会话。
- **预算双向**：尝试/轮次/字节/截止四线；服务端防暴力、客户端防拖拽。
- **rekey 共存**：控制消息路由 KEX/transport，完成后同一 auth session 继续。

## 避坑

### 坑 1：跳过 probe 直接发签名请求

症状：每次认证都做昂贵签名——HSM 高延迟被放大；且对不接受该密钥的服务端也产出了签名原文（签名 oracle 风险面）。

原因：probe 的设计就是"先问后签"——PK_OK 才值得签。跳过它省一次往返、付签名成本与安全面。

```c bad
/* 每次直接 signed request */
sign_and_send(Signer, Key);   /* 服务端拒绝也签了 */
```

```c good
xrtSshAuthPublicKeyWrite(&Writer, Key, ...);    /* probe：免签名 */
send_and_wait();
if ( got_pk_ok() ) {
	xrtSshAuthPublicKeySignDataWrite(&Sig, ...);  /* 原文 */
	Signature = signer_sign(Signer, &Sig);        /* 外部 signer */
	xrtSshAuthPublicKeySignedWrite(&Writer, Key, Signature, ...);
}
```

### 坑 2：keyboard 挑战的 prompt 视图存到下一轮

症状：多轮认证时第二轮读到乱码 prompt——第一轮的视图在 packet 释放后失效。

原因：挑战/响应视图借 packet；跨轮（跨等待）保存必须复制。异步 OTP 后端是高发地。

```c bad
on_challenge(Prompt) {
	g_Prompts = Prompt;   /* 存视图：下轮 packet 已换 */
}
```

```c good
on_challenge(xsshauthkeyboard* pAuth) {
	copy_prompts(pCh, &g_State);   /* 跨等待的都复制 */
}
```

### 坑 3：客户端没设预算被恶意服务器拖

症状：连上恶意服务器，认证永远"再试一轮"——客户端被拖在无限挑战里。

原因：预算不是服务端才需要的——客户端的截止时间与轮次上限是自我保护。

```c bad
/* 客户端不设 guard——服务器要几轮给几轮 */
```

```c good
xsshauthguardpolicy Policy;
xrtSshAuthGuardPolicyInit(&Policy);   /* RFC 推荐默认 */
Policy.MaxAttempts = 3;      /* 最多 3 次尝试 */
Policy.MaxRounds = 5;        /* 最多 5 轮交互 */
xrtSshAuthGuardInit(&Guard, &Policy, DeadlineMs);
/* 超限主动断开——拖拽攻击止损 */
```

## 练习

### 基础：四方法报文巡检

跑通 auth_password/publickey/keyboard/hostbased 四示例，标注每示例构建的报文类型与字段。验收标准：能画出 USERAUTH_REQUEST 的方法域差异（password 明文/publickey blob+签名/keyboard submethods）。

### 进阶：probe-签名的完整序列

确定性驱动：probe→模拟 PK_OK→SignDataWrite→假 signer（固定签名）→SignedWrite→模拟 SUCCESS。对照 budget 全程计数。验收标准：序列各步报文可解析回读；SUCCESS 前预算消耗恰为两次请求。

### 挑战：双因子认证端到端

keyboard 三轮：口令→OTP→新口令确认（模拟 PAM）；客户端预算（3 轮 30 秒）；服务端预算（防暴力）。正确与错误 OTP 两路。验收标准：正确路三轮过 SUCCESS；错误路 FAILURE 且预算正确扣减；客户端超时路径主动断开。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 分层 | 方法（payload）→公共消息层→会话编排（阶段+预算）→transport core→网络 |
| 写事务 | 方法写→SessionPrepare→core 包→接管→双提交；取消预算不消费 |
| 读事务 | core 验→SessionPrepare（视图借出）→双提交/Abort+关 |
| 四方法 | password（隧道内明文）/publickey（probe+外部 signer）/keyboard（多轮挑战）/hostbased |
| probe | 免签名探问→PK_OK 才签；签名原文 session_id‖REQUEST |
| 算法细节 | blob 算法可异于请求（RSA SHA-2 用 ssh-rsa blob）；签名算法必须一致 |
| keyboard | 单未完挑战；prompt 非空/response 可空；无上限不分配；预算限轮次 |
| 未知方法 | 60..79 透传+Method 借出——新方法不改状态机 |
| 预算双向 | 尝试/轮次/字节/截止；防暴力（S）+防拖拽（C） |
| 视图纪律 | 借 packet、提交前有效——异步后端复制 |
