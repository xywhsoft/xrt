---
num: 124
slug: xssh-channel
title: SSH（五）：通道与窗口
volume: 卷十一 其他扩展库
type: practice
lead: RFC 4254 的双向流控窗口、open/confirm/close 状态、两阶段窗口返还与动态通道集合——多路复用的数据面。
api: xssh-ssh_channel_core, xssh-ssh_channel_window, xssh-ssh_channels
---

## 导读

认证（第 123 章）之后连接是"你的"了——而 SSH 的强大之处在于一条连接上开**多条通道**（channel）：session（shell/exec/sftp）、direct-tcpip（第 125 章转发的基础）、自定义类型。本章讲数据面三件套：**窗口**（`ssh_channel_window`：RFC 4254 双向流控——远端窗口+max-packet 限发送、本地窗口按消费返还，无分配无网络所有权）；**通道核心**（`ssh_channel_core`：open/confirm/failure、EOF/CLOSE 状态、数据提交与窗口事务——不拥有缓冲不碰 transport，可放数组/哈希按 recipient O(1) 路由）；**通道集合**（`ssh_channels`：动态所有权层——按需创建节点、稳定地址、硬上限，无固定数组零空闲成本）。窗口机制是 SSH 版的"背压"——与第 67 章写预算、第 95 章流控同一家族，但双向且各通道独立。

## 引入

为什么通道要窗口？一条 SSH 连接的 TCP 缓冲是全连接共享的——如果通道 A 的对端不读，TCP 窗口收缩，通道 B 的数据也发不动（队头阻塞）。**通道级窗口**把流控细化到每通道：发送方最多发"对端给的窗口"字节，收到 WINDOW_ADJUST 才继续；接收方消费数据后返还窗口。效果：慢通道只堵自己（自己的窗口耗尽），快通道照跑——多路复用的公平性由协议保证。

实现细节的讲究处：**两阶段返还**——应用消费数据后额度先进 `ReceivePending`（还没告诉远端）；达到阈值或窗口耗尽时组装 WINDOW_ADJUST、**消息可靠排队后**才 `AdjustCommit`——"避免发送失败却提前扩大本地窗口"（提前通告=超发窗口=协议错乱）。**uint64 统计**：内部未消费/待返还字节用 64 位计数（长连接不因累计超 4 GiB 回绕），线路窗口仍严格 uint32（RFC 要求）。

## 概念

### 窗口：双向流控状态

```diagram flow
- 发送：SendLimit = min(远端窗口, 远端 max-packet) → 数据入可靠队列 → SendCommit 扣减
  （队列失败不提交）→ 收 WINDOW_ADJUST → SendAdjust（回绕即协议错误）
- 接收：收到数据 → ReceiveCommit（校验本地窗口与 max-packet）→ 应用消费 → ReceiveConsume
  （额度入 Pending）→ 达阈值/窗口耗尽 → AdjustReady → AdjustLimit（一条 ADJUST 可安全携带的量）
  → 消息可靠排队 → AdjustCommit（两阶段：排队成功才扩大窗口）
- 动态容量：ReceiveGrantCommit 新增接收容量——零窗口启动与动态内存预算
```

零窗口启动是个精致能力：初始本地窗口可以设 0——等应用真正备好缓冲再 `Grant`——"先建通道后配预算"的懒初始化。

### 通道核心：open 与状态机

```diagram state
CLOSED -> OPEN_PENDING: OpenInit（本端发起，线路提交后等 peer 响应）
OPEN_PENDING -> OPEN: ConfirmationCommit（peer 确认）/ FailureCommit（失败即终态）
CLOSED -> ACCEPT_PENDING: AcceptInit（peer 发起，策略层复制类型字段）
ACCEPT_PENDING -> OPEN: AcceptCommit / RejectCommit
OPEN: 数据面唯一开放阶段（EOF/close 独立推进）
OPEN -> CLOSED: 双向 close 完成（CloseSend+CloseReceive）
```

**EOF 与 CLOSE 分离**：EOF 只关一个数据方向（"我发完了"但还能收）——第 96 章关闭语义的通道版；CLOSE 双向终结。**close 后的数据**：此前已交给应用的数据仍可消费，但不再产生 WINDOW_ADJUST（返还机制随通道死亡）。**request 能力独立查询**（CanSendRequest/CanReceiveRequest）——通道上的 shell/exec/x11 请求与数据面是两个正交能力轴。**无内置 request token**：每个通道可独立组合 `xsshreplyqueue`（want-reply 的关联队列）——容量与存储位置由应用按并发定，空闲通道零 FIFO 成本。

### 数据事务：接收提交与消费分离

收到 data：`DataReceiveCommit`（应用接管视图**前**提交——窗口记账）→ 应用消费 → `DataConsume`（额度 Pending）→ 窗口事务返还。发送侧对称：`SendLimit` 限长 → packet 可靠排队 → `DataSendCommit`。**提交与消费分离**正是背压的机制化：不消费=不返还=远端窗口耗尽=远端停发——第 109 章 pause/resume 的窗口版，协议自动执行。

### 通道集合：动态所有权

`ssh_channels` 按**本端 channel id** 存稳定地址的 `xsshchannel`（每项=core+可选动态 I/O+回复 FIFO）：**按需创建**（Open/Accept 才建映射节点——空连接零成本、空通道零固定缓冲）；**地址稳定**（删除前可安全借用——connection session/异步等待/应用状态挂指针）；**硬上限**（默认 1024 通道/每通道 64 待回复/2 MiB 窗口/32 KiB packet/收发各 2 MiB 预算——按负载调，不依赖无限增长）；**可直接作 resolveproc**（recipient→channel 的 O(1) 路由）。

## 示例

### 第一个完整程序：窗口三件套

下面的程序来自 `examples/channel_window`——流控状态的直接操作：

```embed path="extlibs/xssh/examples/channel_window/main.c" title="extlibs/xssh/examples/channel_window/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/channel_window/main.c -lws2_32 -liphlpapi
（输出 chunk=32768 remaining=100000 形态的发送限额自检结果）
```

**刚才发生了什么。** ① `xrtSshChannelWindowInit(&窗, 100000, 32768, 100000, 32768, 50000)` 六参数：发送窗口/发送 max-packet/接收窗口/接收 max-packet/返还阈值——本地与远端能力一次声明。② `xrtSshChannelSendLimit` 取首笔发送限额——`min(远端窗口 100000, 远端 max-packet 32768)=32768`：**max-packet 是每报文上限、窗口是总量**，两者取小决定单次可发量。③ 打印 chunk 与剩余窗口——发送循环的"一次能发多少"由这个查询驱动。真实序列（send→commit→adjust→consume→adjust-ready→adjust-commit）在测试里全路径覆盖；本示例验证的是限额计算的确定形态。

### 第二个完整程序：通道核心的 open 事务

第二个程序来自 `examples/channel_core`——open 与预算声明：

```embed path="extlibs/xssh/examples/channel_core/main.c" title="extlibs/xssh/examples/channel_core/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/channel_core/main.c -lws2_32 -liphlpapi
（输出 channel 结构尺寸、本地编号与阶段的自检结果）
```

**刚才发生了什么。** ① 初始化入口（参数含本地 recipient 起始、初始窗口/max-packet、request 预算）——栈上通道核心，sizeof 输出（第 117-120 章同款零负担声明）。② 打印 `local`（本端编号）与 `phase`（初始阶段）——核心的公开状态就是路由与调度的输入。③ open 事务的形状：`OpenInit` → 构建线路 CHANNEL_OPEN → 提交后等 confirmation → `ConfirmationCommit` 进 OPEN——**数据面只在 OPEN 开放**，阶段查询（phase）就是发送前的检查。配套示例族：`channels`（集合与稳定地址）、`channel_request`（shell/exec 请求）、`channel_pty`（终端协商）、`channel_io`（动态 I/O staging）、`channel_state`/`channel_message`（状态与报文）、`reply_queue`（want-reply FIFO）。

## 契约

- **窗口无分配**：不保存 payload、不设固定缓冲——视图直交消费方/环形缓冲/零拷贝队列。
- **发送限额**：`SendLimit=min(远端窗口, 远端 max-packet)`；可靠排队后才 `SendCommit`；回绕即协议错误。
- **两阶段返还**：消费→Pending→（阈值/耗尽）→AdjustLimit→**可靠排队后**AdjustCommit——失败不提前扩窗。
- **动态容量**：Grant 新增接收额度；零窗口启动；不消耗已消费额度。
- **64 位统计**：内部字节 uint64 计数不回绕；线路窗口严格 uint32（RFC）。
- **open 事务**：OpenInit→线路提交→Confirmation/FailureCommit；Accept/Reject 对称——每步可靠提交后才推进。
- **阶段纪律**：数据面仅 OPEN 开放；EOF 单向关；CLOSE 双向；close 后已有数据可消费但不再 ADJUST。
- **request 正交**：CanSend/ReceiveRequest 独立查询；token 由 replyqueue 按需组合（空闲零成本）。
- **集合**：按需创建、地址稳定（删除前）、硬上限（默认 1024/64/2MiB/32KiB/2MiB+2MiB）、O(1) 路由。
- **不越界**：core 不碰 transport；集合不建 socket/Engine/future/队列——层间只经显式调用。

## 避坑

### 坑 1：消费了数据没返还窗口

症状：对端发一批后"卡住"——它窗口耗尽等你 ADJUST，你消费了却没触发返还逻辑。

原因：Consume 只记账不返还——返还要等阈值或耗尽触发（也可主动查 AdjustReady）。忘了这层，流控单边失效。

```c bad
while ( receive_commit(&Ch, Data) == OK ) {
	consume_app(Data);
	xrtSshChannelCoreDataConsume(&Ch->Core, Data.Size);
	/* 没查 AdjustReady：远端窗口枯死 */
}
```

```c good
while ( ... ) {
	consume_app(Data);
	xrtSshChannelCoreDataConsume(&Ch->Core, Data.Size);
	if ( xrtSshChannelCoreAdjustReady(&Ch->Core) ) {
		n = xrtSshChannelCoreAdjustLimit(&Ch->Core);
		if ( send_window_adjust(&Conn, Ch, n) ) {
			xrtSshChannelCoreAdjustSendCommit(&Ch->Core, n);
		}
	}
}
```

### 坑 2：窗口返还先提交后排队（顺序反了）

症状：偶发对端超发——本地窗口被"扩大"了但 ADJUST 丢在排队失败里。

原因：契约明确两阶段——**消息可靠排队后**才 AdjustCommit。先 Commit 后排队失败=窗口已扩、消息没到——远端按扩后窗口发，本地校验拒绝，协议错乱。

```c bad
AdjustCommit(&Ch, n);      /* 先扩窗 */
if ( !send_adjust(n) ) { /* 失败：窗口已回不去 */ }
```

```c good
if ( send_window_adjust(&Conn, Ch, n) ) {   /* 先可靠排队 */
	xrtSshChannelCoreAdjustSendCommit(&Ch->Core, n);  /* 成功才记账 */
}
```

### 坑 3：保存了集合删除后的通道指针

症状：Remove/Discard 后访问通道崩溃——稳定地址的"稳定"有边界。

原因：地址稳定到**对应项删除为止**——集合清理（连接关闭）后全部失效。跨连接保存通道指针是设计外用法。

```c bad
g_SavedChannel = xrtSshChannelsGet(&Set, Id);   /* 跨连接保存 */
/* 连接关闭、集合清理后仍用 g_SavedChannel */
```

```c good
/* 通道生命周期跟随连接：状态挂通道内的应用区（集合管理），
   连接关闭回调里完成清理——不留悬挂引用 */
```

## 练习

### 基础：限额扫描

窗口 100000/max-packet 32768 下模拟发送：循环 SendLimit→扣减→直到窗口耗尽；注入 ADJUST 后恢复。验收标准：每次限额=min 正确；耗尽与恢复路径清晰；回绕注入被拒。

### 进阶：两阶段返还时序

模拟"消费一批→阈值触发→排队成功/失败"两路——验证 Commit 只在排队成功后推进（失败后 Pending 保留、可重试）。验收标准：两路的 Pending/窗口内部状态与契约一致。

### 挑战：三通道公平复用

一条连接开三条通道（不同窗口），A 对端慢消费（大 ADJUST 延迟）、B/C 正常——并发灌数据。验收标准：B/C 吞吐不受 A 窗口耗尽影响（通道级隔离）；A 恢复后继续；全程集合路由无错（recipient O(1)）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 三件套 | window（流控）/core（open+状态+数据事务）/channels（集合） |
| 发送限额 | min(远端窗口, 远端 max-packet)；排队后 Commit；回绕即错 |
| 两阶段返还 | Consume→Pending→阈值→排队→Commit——失败不提前扩窗 |
| 零窗口启动 | 初始 0 窗口+Grant 懒配——先建通道后备缓冲 |
| 统计位宽 | 内部 uint64 不回绕；线路 uint32 严格 RFC |
| open 事务 | OpenInit→提交→Confirm/Failure；Accept/Reject 对称 |
| EOF/CLOSE | EOF 单向；CLOSE 双向；close 后可消费不再 ADJUST |
| request 正交 | 能力独立查询；token 由 replyqueue 按需组合 |
| 集合 | 按需创建/地址稳定（删除前）/硬上限默认 1024·64·2MiB·32KiB |
| 流控本质 | 不消费=不返还=远端停发——pause/resume 的协议自动化 |
