---
num: 63
slug: net-port
title: 事件端口与五后端
volume: 卷七 网络
type: practice
lead: IOCP/epoll/kqueue/io_uring/select 的统一抽象——readiness 与 completion 双形态、能力声明与控制事件。
api: net, tcp
---

## 导读

事件端口（`xnetport`）是网络引擎的**心脏**：五平台后端（IOCP/epoll/kqueue/io_uring/select）统一成一套接口，`xrtNetPortBackend` 查询当前后端、`Capabilities` 声明能力集。两种编程形态（第 62 章预告）：**readiness**（Watch 可读/可写通知——epoll/select/kqueue）与 **completion**（Connect/Accept/Recv/Send 直接给结果——IOCP/io_uring）；**控制事件**（Post 自定义事件、Wake 唤醒、Cancel 取消在途——与调度器/通道联动的三扇门）。多数业务代码经 Stream 层（第 66 章）间接使用端口——本章是理解引擎 internals 与直用端口的参考层。

## 引入

五平台各有原生事件机制：Windows IOCP（完成端口——completion 模型）、Linux epoll（就绪通知——readiness）与 io_uring（完成队列——completion）、macOS kqueue（readiness）、跨平台兜底 select。裸写五套适配是网络库的经典工作量——且能力差异（IOCP 没有 readiness、select 没有 completion）让“一份代码五平台”需要能力分支。

XRT 端口的答案：**统一接口 + 能力声明**——接口覆盖两种形态的全部操作（Watch/Unwatch 属 readiness；Connect/Accept/Recv/Send 族属 completion），`Capabilities`（返回位标志）查询当前后端支持什么，跨平台代码按能力分支或上移到 Stream 层（它内部已做分支）。port_tour 范例的输出直接展示：Windows 上 `iocp=completion select=readiness`——两个端口各演示一种；Linux 上 epoll/uring 分属两种——平台差异从“陷阱”变成“声明”。

## 概念

### 端口操作全景

| 分族 | 入口 | 形态 | 说明 |
| --- | --- | --- | --- |
| 自省 | Backend/Capabilities/GetConfig | — | 后端与能力查询 |
| readiness | Watch/Unwatch | 可读写通知 | fd 就绪时回调 |
| 流完成 | Connect/Accept/ReadProbe/Recv/RecvVec/Send/SendVec | completion | 结果直接交付 |
| 数据报完成 | RecvFromVec/RecvMsg( Vec)/SendToVec/SendMsgVec | completion | UDP 面向报文 |
| 控制 | Post/Wake/Cancel/RecvError | — | 自定义事件/唤醒/取消/错误收包 |

**控制三门的用途**：`Post` 投递 USER 事件（外部逻辑注入事件流——定时器到期、配置变更）；`Wake` 唤醒等待中的端口（跨线程安全——第 60 章骨架的 Wake 通道落点）；`Cancel` 取消在途操作（以 CANCELLED 终结——第 53 章取消树的端口方言）。

### readiness vs completion 详解

```diagram flow
- readiness（epoll/select/kqueue）：注册关注 → 就绪通知 → 你去读写
  责任在调用方：读写时机、缓冲管理、非阻塞处理
- completion（IOCP/io_uring）：提交操作+缓冲 → 完成时结果交付
  责任在端口：操作排队、缓冲锁定、完成回传
- 能力声明：Capabilities 查询——不假设后端支持双形态
```

两种形态的**缓冲归属**差异最值得注意：readiness 下缓冲归你（就绪了你填/读）；completion 下缓冲在提交到完成期间**冻结**（第 47 章异步文件的同一纪律——端口版）。Windows 上 IOCP 只有 completion——Watch 不可用；select 只有 readiness——提交操作不可用；Linux epoll 与 uring 分属两种。**跨平台直用端口的代码模式**：能力分支（if Capabilities.completion → 提交式 else → Watch 式）或统一上移 Stream 层。

### 事件类型与等待

端口的等待返回**事件**（结构体携带类型+关联数据）：READABLE/WRITABLE（readiness）、CONNECT 完成/ACCEPT 到来/读写结果（completion 各操作）、USER（Post）、WAKE（Wake）、ERROR（平台错误如 ICMP 不可达——RecvError 按平台能力门控）。等待形态：阻塞等（下一事件）、带超时等（引擎泵的节拍——第 60 章 PollFor 的底层）。事件消费循环就是引擎 Worker 的主循环骨架。

### 端口与 Stream 的关系

第 66 章 Stream 在端口之上：Stream 的回调（Accept/Read/Close）由端口事件驱动；Stream 内部做双形态适配（completion 后端直接提交、readiness 后端就绪后读写）+ 状态机（第 66 章四态）+ 引用计数。**选层判据**：要精细控制（自定义协议底座、UDP 批量收发——udp_batch 范例直用端口）→ 本章端口；标准连接处理 → Stream 层。port_epoll/port_iocp/port_kqueue/port_select/port_uring 五个范例各演示一个后端的直用形态。

## 示例

### 完整程序：双形态与能力声明

来自仓库范例 `examples/network/port_tour/main.c`——两个端口分别演示两种形态：

```embed path="examples/network/port_tour/main.c" title="examples/network/port_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/port_tour/main.c -lws2_32 -liphlpapi
port: backends iocp=completion select=readiness ok
port: watch -> READY -> unwatch ok
port: tcp connect + accept + readprobe + recv/send ok
port: stream vec recv/send ok
port: dgram recvfromvec/recvmsg/recvmsgvec ok
port: dgram sendtovec/sendmsgvec ok
port: cancel in-flight recv -> CANCELLED ok
port: post USER + wake WAKE ok
port: recv-error gated by platform ok
```

**刚才发生了什么。** ① `Backend/Capabilities` 的直接证言：`iocp=completion select=readiness`——Windows 上两个端口各一种形态（能力声明防止假设）。② readiness 路：Watch 注册 → 事件 READABLE → Unwatch 注销——三步完成一次就绪观察。③ completion 路全家族：TCP 的 Connect/Accept/ReadProbe/Recv/Send（含 Vec 向量变体——多缓冲一次提交）与 UDP 的 RecvFrom/SendTo 族（数据报面向报文——每个操作对应一个数据报）。④ 控制三门各验证：Cancel 在途接收以 CANCELLED 终结（第 53 章方言）、Post/Wake 的自定义与唤醒事件到达。⑤ `recv-error gated by platform`——RecvError 的平台能力门控（能力外的调用被拒绝而非崩溃——声明式设计的执法）。

### 完整程序：单后端直用

来自 `examples/network/port_select/main.c`——readiness 后端的直接使用：

```embed path="examples/network/port_select/main.c" title="examples/network/port_select/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/port_select/main.c -lws2_32 -liphlpapi
（select 后端的就绪观察输出——跨平台兜底形态）
```

**刚才发生了什么。** ① 单后端范例聚焦 readiness 循环：Watch → 等待事件 → 处理 → 注销——最简端口使用骨架。② select 的价值是**兜底**（任何平台可用）——开发调试（不支持专用后端的环境）与教学的落点；生产首选 IOCP/epoll/uring（引擎自动选择）。③ 五个 port_X 范例（epoll/iocp/kqueue/select/uring）结构相同——**同一套调用跨五后端**是统一接口的价值实证：换后端零改代码（或只改创建参数）。

## 契约

- **能力声明**：直用端口先查 Capabilities——不假设双形态；能力外调用被拒绝（声明式执法）。
- **缓冲冻结**：completion 提交到完成期间缓冲不可触碰（第 47 章纪律的端口版）。
- **控制三门**：Post 注入 USER/Wake 跨线程唤醒/Cancel 取消在途——与骨架三件（外挂注入/泵唤醒/取消树）一一对应。
- **Vec 变体**：RecvVec/SendVec 多缓冲一次提交——批量 IO 的摊还入口（第 21 章批量思想的 IO 版）。
- **层选择**：标准连接上移 Stream（第 66 章）；自定义底座/UDP 批量直用端口。
- **事件循环**：端口等待即引擎 Worker 主循环——事件消费是骨架泵的底层形态。

## 避坑

### 坑 1：completion 后端上用 Watch

症状：调用被拒（"no readiness capability"）或编译时看似正常运行时失败——Windows IOCP 上没有就绪通知。

原因：假设后端双形态——IOCP 只有 completion、select 只有 readiness；能力没查就调用。

```c bad
xnetport* Port = xrtNetPortCreate(...);   /* Windows 默认 IOCP */
xrtNetPortWatch(Port, Fd, EVENTS, Callback, Data);   /* 被拒——无 readiness 能力 */
```

```c good
uint32 Caps = xrtNetPortCapabilities(Port);   /* 位标志 */
if ( Caps & XNET_PORT_CAP_READINESS ) {
	xrtNetPortWatch(Port, Fd, EVENTS, Callback, Data);
} else if ( Caps & XNET_PORT_CAP_COMPLETION ) {
	xrtNetPortRecv(Port, Fd, Buffer, Size);   /* 提交式——能力分支 */
}
```

### 坑 2：提交后动缓冲

症状：收到脏数据或偶发完成错误——与第 47 章文件异步坑 2 同族；时序依赖难复现。

原因：completion 形态下缓冲在提交到完成期间被端口锁定使用——提前复写就是数据竞争。

```c bad
xrtNetPortRecv(Port, Fd, Buffer, Size);   /* 提交接收 */
memset(Buffer, 0, Size);                    /* 立刻清——端口正在往里写 */
```

```c good
xrtNetPortRecv(Port, Fd, Buffer, Size);
/* 缓冲冻结直到完成事件到达；要提前复用换缓冲池（第 22 章）——每块绑一次 IO */
```

## 练习

### 基础：能力报告器

创建端口 → 打印 Backend/Capabilities/GetConfig——在你能接触的每个平台跑一遍，记录五后端能力矩阵。

### 进阶：readiness 回声

select 端口 + Watch：监听一个 UDP socket，收到就绪后 recvfrom 并回发——readiness 模型的最小完整回路（对照 port_select 范例）。

### 挑战：双形态 TCP 客户端

同一 TCP 连接逻辑写两版：completion 版（IOCP/uring——提交式）与 readiness 版（select/epoll——就绪式）——对比代码形态与缓冲归属差异，输出对照报告。验收标准：两版在支持的后端各跑通；缓冲冻结纪律在 completion 版落实（池化缓冲）；报告含两种模型的代码量与复杂度对比。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 五后端 | IOCP(epoll/kqueue/select/uring——统一接口、能力声明 |
| 双形态 | readiness（就绪通知你读写）/ completion（提交即等结果）；Capabilities 分支 |
| 控制三门 | Post USER / Wake 唤醒 / Cancel→CANCELLED——骨架三件的落点 |
| Vec 族 | RecvVec/SendVec 多缓冲一次提交——批量 IO |
| 缓冲纪律 | completion 冻结期不可碰——池化每块绑一次 |
| 层判据 | 标准 TCP → Stream 层；自定义底座/UDP 批量 → 直用端口 |
| 平台范例 | port_epoll/iocp/kqueue/select/uring 五范例同构直用 |
