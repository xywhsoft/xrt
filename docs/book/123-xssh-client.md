---
num: 123
slug: xssh-client
title: SSH（七）：客户端运行时
volume: 卷十一 其他扩展库 · 卷十一收官
type: practice
lead: 无隐藏 Engine 的客户端组合、ReadyTimeout 全程预算、主机信任与认证的事件交互、Drain 语义与 Packet 逃生口——七章零件的总装，卷十一收官。
api: xssh-ssh_client, xssh-ssh_client_core, xssh-ssh_session_stream
---

## 导读

SSH 系列收官——前六章的零件在此总装。`ssh_client` 在**调用方提供的 `xnetstream`** 上组合 core/流会话/动态通道：**不创建隐藏 Engine、不阻塞 Worker、不预分配 channel 或报文缓冲、不引入第二套状态机**（四不——SSH 版的"无第二实现"）。连接生命周期：`ClientInit` 后事件表与数据指针直接交给 `xrtNetStreamConnect`（或对已建流 `ClientAttach`）——TCP 打开后 `ReadyTimeout`（默认 30 秒）统一约束版本交换→KEX→主机信任→认证全程；**Ready 事件是 SSH 可用状态的唯一发布点**。交互点三个：主机信任（默认拒绝——DEFER 后 HostKey 事件给决定权）、认证（provider NEED_MORE 时 Authenticate 事件）、未知协议扩展（Packet 回调保留完整处理能力）。Dial 便利层（DNS/Happy-Eyeballs/超时/取消）独立裁剪——代理与自定义传输走 Attach 不受限。

## 引入

"总装层"的难点是**职责收口**：连接的全部状态（版本/KEX/认证/通道/转发）在六章零件里各有其主，客户端把它们串成一个事件驱动的整体——但每一层的行为边界不变（core 还是那个 core、窗口还是那个窗口）。收口的胶水是**事件表**（client 的 events 配置结构）：Ready/Error/Close/HostKey/Authenticate/Data/Drain/Global/Packet——应用的全部交互面；与第 103 章 HTTP 服务端事件链同构。

背压的**两层串接**值得先看：TCP 队列 AGAIN 时，完整加密 packet 仍由 transport 唯一持有、同时暂停新的 SSH 输入；LowWater/Drain 回调先重试内部 packet，**内部事务已提交且 TCP 队列确实排空后才发布客户端 Drain**——应用在 Drain 里提交的新报文不会越过先前保留的 packet（顺序保证——转发隧道的正确性根基）。OOM 同样精确：内部 DATA 预留失败连接 HOLD、释放内存后 `PacketRetry`——"OOM 不提交半个 packet"。

## 概念

### 连接与超时预算

```diagram flow
- 装配：ConfigInit+ClientInit → 事件表交给 NetStreamConnect（或 Attach 已建流）
- TCP 打开：Worker 绑定（channel/控制报文 scratch 此时才接缓冲池）
- ReadyTimeout（默认 30s，微秒）覆盖：版本交换→KEX→主机信任→认证
- Ready：SSH 可用——通道/转发的起点
- 错误/超时：结构化错误（XSSH_ERROR_TIMEOUT/XERR_TIMEOUT/域）同达 Error/Close/全部未决 Future
- TCP 侧：DNS/建连截止归 xnetdialconfig.Timeout（独立控制）
```

### 三个交互点

- **主机信任**：默认**拒绝**——core 的 HostKey 返回 DEFER 后，应用经 `HostKey` 事件拿到决定时机，`xrtSshClientHostKeyAccept/Reject` 回话（第 118/119 章的两步信任在客户端的交互形态——事件不是回调里的同步判断，可以查库、问用户、带外核对后异步决定）。
- **认证**：provider 返回 NEED_MORE 触发 `Authenticate` 事件——凭据就绪（问了用户/取了 agent）后 `xrtSshClientContinue` 继续（第 120 章方法的异步口）。
- **Packet 逃生口**：`Packet` 回调在底层读事务提交前执行——未知协议扩展、自定义 channel 类型的完整处理能力（第 122 章转发的解析就挂这里）——**客户端不吞未知消息**。

### 数据与控制报文路径

`xrtSshClientSend` 是 payload 快速路径：只增加 SSH packet 编码+一次向 TCP 队列的所有权转移（零中转复制）。`xrtSshClientBuild` 为可变控制报文提供从零增长、可复用、硬上限的连续 scratch（自定义报文的构建缓冲）。标准 DATA/stderr 先进 channel I/O staging、提交后触发 `Data` 事件（应用零复制检查或显式读取）。**全局回复**：`GlobalReplies/ReplyReserve` 的动态有界 FIFO 配 Global 事件（token 关联——第 122 章已见）。

### 通道与生命周期收尾

`xrtSshClientChannelOpen` 是**自定义 channel type 的基础入口**：调用方写 type 专用字段、客户端管动态编号/窗口/回复关联/提交/回滚——经典 session 与 direct-tcpip helper 都建在其上（**不维护另一套 channel 状态**）。`ChannelFlush` 从通道发送队列构建受"远端窗口+max-packet+TCP 背压"共同限制的片段——三层预算的合流点。收尾：`OwnsChannel` 校验归属（防把另一连接的指针交给发送队列）；Drain/Abort 走第 109 章同款语义。

## 示例

### 第一个完整程序：无隐藏运行时的声明

下面的程序来自 `examples/client`——最小客户端的资源形态：

```embed path="extlibs/xssh/examples/client/main.c" title="extlibs/xssh/examples/client/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/client/main.c -lws2_32 -liphlpapi
（输出初始状态与通道上限的自检结果）
```

**刚才发生了什么。** ① `xrtSshClientConfigInit + xrtSshClientInit(&Client, &Config, NULL, NULL)` 栈上客户端——`xrtSshClientState` 初始态与 `Channels.MaxChannels`（默认 1024）打印。② 头注释即设计宣言："由外部 Engine 和 Stream 驱动，不创建隐藏运行时"——第 117 章 transport core"无缓冲"、第 118 章 KEX"无 socket"、这里"无 Engine"——**七层每层都零隐藏**，总装的透明是零件透明的累积。③ `ClientClear` 收尾——未连接的客户端清理零负担。真实装配（事件表进 NetStreamConnect）在 `client_session`/`client_core` 两示例。

### 第二个完整程序：Dial 便利层配置

第二个程序来自 `examples/client_dial`——连接前置的预算：

```embed path="extlibs/xssh/examples/client_dial/main.c" title="extlibs/xssh/examples/client_dial/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/client_dial/main.c -lws2_32 -liphlpapi
（输出拨号超时、回退延迟与客户端状态的配置自检结果）
```

**刚才发生了什么。** ① `xrtNetDialConfigInit` 的 `Timeout=10s`/`FallbackDelay=250ms`——TCP 侧预算（DNS/建连/Happy-Eyeballs 回退），与 `ReadyTimeout`（SSH 侧）**两级独立**：TCP 10 秒+握手认证 30 秒，总预算清晰可调（第 98 章两类截止的 SSH 版）。② Dial 完成回调只表示 TCP 终态——**SSH 可用仍只由 Ready 发布**（回调语义不偷换）。③ 代理/自定义传输不被 Dial 限制——`ClientAttach` 路径始终开放（第 69 章代理隧道+SSH 的组合口）。配套示例族：`client_core`（core+口令认证装配）、`client_session`（会话流）、`client_future`（Future 形态）、`client_auth_ed25519`（公钥认证）、`client_pty`（终端协商）。

## 契约

- **四不**：无隐藏 Engine/不阻塞 Worker/不预分配 channel 与报文缓冲/无第二套状态机。
- **连接装配**：事件表交给 NetStreamConnect；已建流在所属 Worker `ClientAttach`；scratch 在 TCP 打开后绑定 Worker 缓冲池。
- **两级超时**：ReadyTimeout（默认 30s，零禁用）管版本→KEX→信任→认证；Dial 的 Timeout 管 DNS/建连——独立。
- **Ready 唯一**：SSH 可用状态只由 Ready 事件发布；Dial 完成回调≠SSH 可用。
- **主机默认拒绝**：DEFER→HostKey 事件→Accept/Reject 异步决定。
- **认证异步口**：NEED_MORE→Authenticate 事件→Continue。
- **Packet 逃生口**：读事务提交前执行；未知扩展与自定义 channel 的完整能力；客户端不吞未知消息。
- **发送路径**：Send=零中转快速路径；Build=可复用 scratch；三层预算合流于 ChannelFlush。
- **Drain 顺序**：内部 packet 优先重试；事务提交且队列排空才发布——新报文不越界。
- **OOM 精确**：不提交半个 packet；预留失败 HOLD；释放后 PacketRetry。
- **全局回复**：动态有界 FIFO+token 关联的 Global 事件。

## 避坑

### 坑 1：把 Dial 完成当 SSH 就绪

症状：Dial 回调里直接开通道——报错或崩溃，SSH 还在握手。

原因：Dial 回调只表示 TCP 通了；版本/KEX/信任/认证在后面——**Ready 才是可用信号**。

```c bad
on_dial_done(...) {
	xrtSshClientChannelOpen(Client, "session", ...);  /* 握手中：错 */
}
```

```c good
on_dial_done(...) { /* 只是 TCP——等 Ready */ }
on_ready(...) {
	/* 这里才开通道/发转发请求 */
	xrtSshClientChannelOpen(Client, "session", ...);
}
```

### 坑 2：HostKey 事件里忘了回话（挂到超时）

症状：连接卡死最后 ReadyTimeout——事件没人 Accept/Reject。

原因：HostKey 事件是**必须回话**的决定点（默认拒绝是"没配置信任策略"的保护，不是"事件可以不理"）。查库/问用户后务必回。

```c bad
on_host_key(...) {
	audit_log(Key);   /* 只记录不回话：连接挂到超时 */
}
```

```c good
on_host_key(...) {
	audit_log(Key);
	if ( known_hosts_check(Key) == TRUST ) {
		xrtSshClientHostKeyAccept(Client);
	} else {
		xrtSshClientHostKeyReject(Client);
	}
}
```

### 坑 3：READY 前后乱用通道预算默认值

症状：万级并发目标下 1024 通道上限悄悄拦截——一半连接建不了新通道；或反向，嵌入式目标没调小预算内存吃紧。

原因：`Channels.MaxChannels` 等默认是通用值（第 121 章集合契约"按负载调，不能依赖无限增长"）——部署参数不是库的私事。

```c bad
xrtSshClientConfigInit(&Config);   /* 全默认上生产 */
```

```c good
xrtSshClientConfigInit(&Config);
Config.Channels.MaxChannels = my_model.channels;
Config.Channels.ReceiveWindow = my_model.window;
Config.GlobalReplyLimit = my_model.global_tokens;
/* 按负载模型显式声明——预算是部署契约 */
```

## 练习

### 基础：状态机观察

对真实 SSH 服务端（本机 sshd 或容器）连接全程打印事件序列：Dial→（TCP）→版本→KEX→HostKey→认证→Ready→通道→Close。验收标准：序列与本章流程图一致；两级超时各自可独立触发（短值实验）。

### 进阶：异步信任决策

HostKey 事件里发起异步查证（模拟带外核对延迟 200ms）后 Accept——验证事件模型的异步决定能力。对照同步立即拒路径。验收标准：两路径的 Ready/Close 时序正确；决策期间连接保持（不超时）。

### 挑战：完整隧道工具

总装练习：Dial+密码认证+HostKey（known_hosts 集成）+本地转发（`-L` 形态）+并发 3 通道+优雅 Drain——一个可用的 mini ssh -L。验收标准：HTTP 请求经隧道往返成功；三通道并发互不干扰；Ctrl-C 触发 Drain 全链干净退出（预算与引用对账）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 四不 | 无隐藏 Engine/不阻塞 Worker/零预分配/无第二状态机 |
| 装配 | 事件表给 NetStreamConnect 或 Worker 上 Attach |
| 两级超时 | ReadyTimeout（SSH 全程，默认 30s）独立于 Dial Timeout |
| Ready 唯一 | SSH 可用只由 Ready 发布——Dial 完成≠就绪 |
| 主机信任 | 默认拒绝；DEFER→HostKey 事件→Accept/Reject 必回话 |
| 认证异步 | NEED_MORE→Authenticate→Continue |
| Packet 逃生口 | 提交前执行；未知消息不吞——自定义 channel 的能力 |
| 发送 | Send 零中转/Build scratch/Flush 三层预算合流 |
| Drain 顺序 | 内部 packet 优先；排空才发布——新报文不越界 |
| 全局回复 | 有界 FIFO+token 的 Global 事件 |
| 总装图 | 117 传输→118 KEX→119 信任→120 认证→121 通道→122 转发→123 客户端 |
