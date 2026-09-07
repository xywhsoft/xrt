---
num: 68
slug: net-udp
title: UDP 数据报
volume: 卷七 网络
type: practice
lead: 无连接与连接式双形态、事件驱动收发、批量族、组播与 PMTU——数据报的引擎形态。
api: udp, net
---

## 导读

UDP 模块把数据报装进引擎模型：**双形态**——无连接端（recvfrom 带来源地址——任意来源、自己过滤）与连接式（connect 后只收该对端——客户端形态）；**事件驱动收发**（引擎回调——与 TCP 同一的回调面）；**批量族**（udp_batch 范例——多报一次收发的摊还）；**组播**（加入/离开组——multicast 范例）；**PMTU 与异步错误**（udp_introspect/udp_errors 范例——ICMP 不可达等平台能力）。UDP 无流控无状态机——但引擎的回调、引用计数、停机协议全部同款。

## 引入

UDP 与 TCP 的本质差异不是"可靠 vs 不可靠"，是**数据边界**：TCP 是字节流（消息边界自管——第 64 章缓冲链+第 71 章分帧为此存在）；UDP 是数据报（一次 send 一个报文、一次 recv 一个完整报文——边界内核保管）。这个差异让 UDP 的 API 形态完全不同：没有发送预算（报文要么进队列要么不进）、没有接收链（一个报文一块缓冲）、没有流控（不可靠是特性——实时音视频宁可丢也不积压）。

两个形态的选择：**无连接端**（server 形态）——bind 后收任意来源（recvfrom 附来源地址——回复时 sendto 指定目的地）；**连接式**（client 形态）——connect"锁定"对端后只收它（过滤内核做）、send 直接发（无需每次指定）。UDP 的 connect 不握手（只是过滤器+默认目的地）——与 TCP connect（真连接建立）语义完全不同。

## 概念

### 双形态与生命周期

```diagram flow
- 创建：xnetudp 对象 → bind（无连接端）或 connect（连接式）
- 状态：UDP 状态机（BINDING/BOUND/CONNECTING/CONNECTED——异步 bind/connect 的过渡态）
- 收发：无连接端 recvfrom（带来源）/sendto（指定目的地）；连接式 recv/send（固定对端）
- 关闭：Close→CLOSED→Destroy——引用计数同 Stream（第 66 章契约延续）
```

**异步 bind/connect**：UDP 的绑定与连接也是引擎操作（状态机过渡——`xrtNetUdpState` 轮询或事件等待；udp 范例的 `WaitState` 辅助演示截止时间轮询）。

### 事件回调面

UDP 的事件表（与 Stream 同款形态）：Read（报文到达——recvfrom/recv 在回调里）、Write（可发）、Close（终态通知）。**收发在回调**：数据报到达回调触发、回调里拉取（拉取式 recv——报文在队列、回调是通知）；批量拉取（一次回调拉多个——积压清理的高效形态）。Worker 约束同 Stream（回调在 Worker、跨线程 Post——第 67 章契约的 UDP 版）。

### 批量族与报文结构

`xnetudppacket`（报文+来源地址的载体）；批量变体一次操作多个报文（数组提交/收取）——高 PPS 场景（DNS 服务器、监控上报）的摊还入口。**Msg 族**（SendMsg/RecvMsg——第 63 章端口层同族）：带目标地址的向量发送——UDP 的多缓冲变体。

### 组播与 PMTU

**组播**（multicast 范例）：加入/离开组播组（组地址 + 本地接口）、TTL 控制（跨网段跳数）；收发与单播同 API（组地址做目的地）。**PMTU 探测与异步错误**：send 的报文过大被中途丢弃（分片禁用时）——ICMP "frag needed" 异步回报（RecvError 按平台能力——第 63 章能力门控的 UDP 应用）；udp_errors 范例演示错误接收、udp_introspect 演示 PMTU/统计查询。

## 示例

### 完整程序：双形态回环

来自仓库范例 `examples/network/udp/main.c`——无连接端+连接式同场：

```embed path="examples/network/udp/main.c" title="examples/network/udp/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/udp/main.c -lws2_32 -liphlpapi
UDP server: 127.0.0.1:NNNNN
received: hello UDP
```

**刚才发生了什么。** ① 服务端无连接端 bind（端口 0 动态分配——Local 取回打印 `127.0.0.1:NNNNN`）；状态等待（`WaitState` 辅助在截止时间内轮询 BOUND——异步 bind 的过渡态管理）。② 客户端连接式 connect 到服务端——只收该对端（服务端的回包）。③ 客户端 send `hello UDP`——无目的地参数（connect 已锁定）；④ 服务端 Read 事件触发、recvfrom 拉取（来源是客户端）→ 打印 `received: hello UDP`——双形态各在其位。⑤ 回包 sendto 客户端地址——无连接端的回复要指定目的地。

### 完整程序：批量与组播

来自 `examples/network/udp_batch/main.c` 与 `examples/network/udp_multicast/main.c`（合并阅读）：

```embed path="examples/network/udp_batch/main.c" title="examples/network/udp_batch/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/udp_batch/main.c -lws2_32 -liphlpapi
batch: truncated prefix=16 of 64
pull batches: 3 packets in 1 batch
wait batch: 2 packets (borrow=take=ok)
future batch: 2 packets, ref kept size=2
error queue: empty pull=0/0, async terminal=4
writable: sync=1 async=1
```

**刚才发生了什么。** ① `truncated prefix=16 of 64`——截断前缀语义（小缓冲收大报：截到能装的、报尺寸可查——UDP 的边界内截断形态）。② `pull batches: 3 packets in 1 batch`——批量拉取：一次调用三报（单次回调消费积压的摊还实证）。③ wait/future 两行：阻塞等待与 Future 形态各做一次批量收（borrow=take=ok——报文缓冲的两种取出所有权都验证）。④ error queue 行：错误队列的空拉与异步终态——RecvError 族的 UDP 实测。⑤ writable 行：同步与异步的可写探测。multicast 范例补组播位（join→组地址 send→收到→leave）；sync/future 范例是阻塞/Future 姊妹形态——四形态与 TCP 家族对齐。

## 契约

- **数据边界**：一报一块缓冲、无链无预算——与 TCP 的根本差异；不丢边界也不丢序（序本来不保）。
- **双形态**：无连接端（任意来源+sendto 回复）/ 连接式（connect 过滤+固定目的地）——按角色选。
- **异步生命周期**：bind/connect 过渡态管理（State 轮询或事件等待+deadline）。
- **回调面同款**：Read/Write/Close 事件；拉取式收发；Worker 约束与 Post 同 Stream。
- **组播**：join/leave+TTL；收发同单播。批量族高 PPS 摊还。
- **异步错误**：ICMP/PMTU 类错误按平台能力门控（RecvError）——不可靠报文的"不可达"反馈。

### 从示例到工程：UDP 的三个宿主

**发现与注册类**（组播主场）：服务发现（multicast 范例的挑战版）、配置分发、心跳多播——小报文+组播+无连接的低开销形态；组播 TTL 控制范围（TTL=1 限本网段）。**实时流类**（不可靠是特性）：音视频/游戏/遥测——固定节奏发送、丢包不重传（重传过期数据无意义）、seq 号做乱序检测。**请求-响应类**（可靠性应用层自建）：DNS 类查询、简单 RPC——超时+重试+请求 ID 配对（进阶练习的标准层）。三宿主的共同分野是**丢包的代价**：发现类无所谓、实时类不要重传、请求类必须重试——按代价建应用层，而不是默认 TCP 一把梭或裸 UDP 裸奔。

### QUIC 时代的 UDP 地位

UDP 在现代网络栈的地位值得一章级别的认知更新：**QUIC/HTTP3 全部跑在 UDP 上**——可靠传输、流控、加密在应用层（UDP 之上）重建。为什么：TCP 的内核僵化（中间盒修改、升级慢、队头阻塞）让创新被迫下沉到 UDP 载体。XRT 的 UDP 模块为此提供引擎形态的底座（回调面/批量/组播/PMTU）——QUIC 类协议的实现原料。与 XRT 自身 TCP 栈（内核 TCP 的引擎封装）互补：标准 Web 用 TCP（金标准章）、自定义协议与低延迟场景从 UDP 原料起步。**选 TCP 还是 UDP 的现代答案**：标准协议需求 TCP、可控协议演进需求 UDP 原料——不再只是"可靠 vs 快"的旧二分。

### 观测与安全：UDP 的两个薄弱位

UDP 服务观测的薄弱位要主动补强（第 49 章管线）。**丢包不可见**：UDP 没有重传——丢了多少只有应用层知道（seq 号缺口分析——seq 连续性统计进日志）；**安全面**：UDP 易被伪造源地址（无握手验证）——放大反射攻击（DNS 放大是经典）的防御位：响应尺寸限制（请求 64 字节不回 4000 字节）、目的验证（连接式过滤）、速率限制。两个薄弱位在服务上线前过一遍——UDP 的"简单"是 API 简单，运维与安全的功课不减反增。

## 避坑

### 坑 1：UDP connect 当 TCP connect 理解

症状：以为"connect 成功=对端可达"——实际对端不存在 connect 也成功（UDP connect 只设过滤器）；或以为 connect 后可靠——丢包照旧。

原因：UDP connect 是本地过滤设置——不握手、不确认、不改变不可靠本性。

```c bad
if ( xrtNetUdpConnect(Udp, &Peer) ) {
	AssumeReachable(Peer);   /* 错——UDP connect 不验证对端存在 */
}
```

```c good
xrtNetUdpConnect(Udp, &Peer);   /* 设置过滤+默认目的地——本地操作 */
SendRequest(Udp);
if ( !WaitResponse(Udp, Deadline) ) {   /* 可达性靠应用层请求-响应验证 */
	RetryOrFail();
}
```

### 坑 2：报文尺寸超路径 MTU

症状：本机回环测试正常、跨网段丢包——大报文被丢弃（分片禁用或中间路由 MTU 小）；偶发部分到达（分片部分丢）。

原因：UDP 报文超过路径 MTU——内核分片（部分丢=整报丢）或 PMTU 黑洞（DF 位+ICMP 被过滤）。

```c bad
char Big[65000];
Fill(Big);
xrtNetUdpSend(Udp, Big, sizeof(Big));   /* 超 MTU——跨网段大概率整报丢弃 */
```

```c good
/* 应用层限制在安全尺寸内（常见 1200-1400 字节以下）；大消息走 TCP 或应用分片 */
char Packet[1200];
xrtNetUdpSend(Udp, Packet, sizeof(Packet));
```

## 练习

### 基础：双形态回环复现

复现主示例；再加一个"第二客户端"向服务端发报——验证无连接端收任意来源、连接式只收锁定对端（第二客户端的包被过滤）。

### 进阶：请求-响应超时重试

UDP 客户端的标准可靠性层：发请求→限时等响应→超时重试（指数退避）→N 次失败报告；服务端模拟丢包（随机丢弃 50%）验证重试收敛。提示：deadline 族+第 53 章取消。

### 挑战：组播发现服务

局域网服务发现：服务端 join 组播组定期广播自己、客户端 join 组收发现报、单播连接确认。验收标准：多服务端并发广播时客户端能枚举全部；join/leave 干净（无泄漏）；广播风暴控制（间隔+TTL）；发现到连接的全链路进日志可审计。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 数据边界 | 一报一缓冲——内核保管边界；无链无预算 |
| 双形态 | 无连接端（任意来源+sendto）/ 连接式（connect 过滤+固定目的地） |
| 生命周期 | 异步 bind/connect 状态机+deadline 轮询/事件等待 |
| 回调面 | Read/Write/Close 同 Stream；拉取式收发；Worker 约束同款 |
| 批量族 | 多报一次收发——高 PPS 摊还；Msg 族带地址向量 |
| 组播 | join/leave+TTL；收发同单播 |
| MTU | 报文限安全尺寸（~1200）；PMTU 错误按能力门控 |
