---
num: 122
slug: xssh-forward
title: SSH（六）：端口转发
volume: 卷十一 其他扩展库
type: practice
lead: direct-tcpip 与 forwarded-tcpip 两形态、tcpip-forward 全局请求与动态端口、端口语义边界与地址借用——隧道协议的全部零件。
api: xssh-ssh_client_forward, xssh-ssh_forward_message
---

## 导读

SSH 最日常的用法不是开 shell，而是**挖隧道**：`ssh -L 8080:db:80`（本地转发——把本地 8080 的流量经 SSH 送到服务端能到的 db:80）与 `ssh -R 9000:localhost:9000`（远程转发——让服务端把 9000 的流量送回你这边）。协议形态两块：**direct-tcpip** 通道（本地转发的载体——客户端主动开的带目标地址的通道）与 **forwarded-tcpip** 通道（远程转发的载体——服务端在收到你的 `tcpip-forward` 全局请求后主动打开的通道）。xssh 的分层照旧：`ssh_forward_message`（纯协议 payload——报文编解码、端口语义边界、不建 listener 不解析 DNS）与 `ssh_client_forward`（客户端组合层——`DirectTcpipOpen`/`ForwardedTcpipAccept`/`TcpipForward`/`TcpipForwardCancel` 四入口，不藏本地 listener/线程/等待）。真正的转发服务在上层组合 XRT listener/stream 与通道窗口——本章给你零件与拼法。

## 引入

转发协议的地址语义值得细读：`direct-tcpip` 通道携带 `要连的地址:端口 + 来源地址:端口`——目标由**客户端**声明（服务端策略层决定允不允许——"目标访问控制继续由服务策略层决定"是契约原话）；`forwarded-tcpip` 反向——服务端发来的通道携带 `被连的目标 + 发起者来源`。**端口语义边界**：RFC 4254 线路用 uint32 承载端口，但 xssh 在语义编解码边界统一限 0..65535——非法写入不推进 writer、非法读取协议错误（"uint32 承载但实际 16 位"的规范化——比线路格式更严的语义层）。**端口 0 的动态分配**：`tcpip-forward` 请求端口 0=让服务端分——成功响应里带真实端口（`TcpipForwardSuccessRead` 取回——远端动态端口的取法）。

全局请求的 **token 关联**：`tcpip-forward` 是 want-reply 全局请求——发的时候领 token、成功/失败经客户端 Global 事件回来时带同一 token——多条全局请求在途时的关联器（第 121 章 replyqueue 的全局版）。

## 概念

### 两形态与数据流

```diagram flow
- 本地转发（-L）：本地 listener 收连接 → 每连接开一条 direct-tcpip 通道
  （目标=配置的 host:port，来源=本地连接方）→ 通道数据面双向搬运
- 远程转发（-R）：TcpipForward(监听地址, 端口) 全局请求 → 服务端起 listener
  → 外部连入 → 服务端开 forwarded-tcpip 通道回来（目标=入连者）
  → ForwardedTcpipAccept 解析 → confirmation 自动回 → 数据面搬运
- 数据面：统一用通道 I/O（第 121 章窗口/背压/关闭）——转发不另设一套
```

### 报文层：五类 payload 与借用语义

`ssh_forward_message` 实现五类：`tcpip-forward`（发）/`cancel-tcpip-forward`（撤）/动态端口成功响应（读）/`direct-tcpip` open（发读）/`forwarded-tcpip` open（读）。**地址借用**：地址与来源按 SSH string 原样借用——**不强迫先转 socket address**（目标可能是服务端视角的主机名——客户端解析它既无意义也常不可能）。**复用公共构建器**：writer 直接复用 global/channel 公共前缀——最终 payload 一次完成、不建临时 Fields 缓冲（零分配路径）。**严格读取**：通用 envelope 先解析、类型专用字段严格读、**拒绝尾随内容**（走私防御的第 N 次出现）。

### 客户端组合层：四入口

- `xrtSshClientDirectTcpipOpen`：开 direct-tcpip（本地转发/自定义隧道的起点）。
- `xrtSshClientForwardedTcpipAccept`：在 `Packet` 回调或 HOLD 期间解析服务端主动开来的 forwarded-tcpip——**读事务提交后客户端自动发 confirmation**（顺序契约："提交读事务、发送响应、提交 channel 状态"严格三步——应用不应在 Packet 回调里直接发 confirmation）。
- `xrtSshClientTcpipForward`：发 want-reply 的全局请求（端口 0 动态分配）；token 从动态有界 FIFO 领。
- `xrtSshClientTcpipForwardCancel`：取消同一 remote 地址。
- 拒绝未知/不允许的 peer 通道：`xrtSshClientChannelReject`；**未决定直接接受 Packet 时客户端默认回 `UNKNOWN_CHANNEL_TYPE`**——不让 peer 永久等待（协议礼貌的默认化）。

### 组装完整转发服务

零件齐了，服务形态=上层组装：本地转发=第 66 章 listener + 每连接 DirectTcpipOpen + 双向搬运（通道 I/O ↔ TCP 流）；远程转发=TcpipForward + 本地 listener（对应服务端连入）+ ForwardedTcpipAccept。搬运的死法（背压联动、半关、取消）全部复用已有词汇——本章新东西只在"协议报文与两端语义"。

## 示例

### 第一个完整程序：direct-tcpip 报文构建

下面的程序来自 `examples/forward_message`——本地转发的通道 open 报文：

```embed path="extlibs/xssh/examples/forward_message/main.c" title="extlibs/xssh/examples/forward_message/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/forward_message/main.c -lws2_32 -liphlpapi
（输出 direct-tcpip open 报文构建的自检结果）
```

**刚才发生了什么。** ① `xrtSshDirectTcpipOpenWrite(&Writer, 通道号, 初始窗口 1 MiB, max-packet 32 KiB, 目标 "db.internal":5432, 来源 "127.0.0.1":50000)` ——本地转发场景的典型参数：目标=**服务端视角**的数据库地址（主机名原样借用——服务端去解析）、来源=本地连入方。② 窗口/max-packet 即第 121 章的通道能力声明——转发通道的初始预算。③ 端口参数全部落在 0..65535 的语义边界内——传 65536 会在写入层被拒（writer 不推进）。这个报文配上通道框架（第 121 章）就是一条完整隧道请求。

### 第二个完整程序：转发配置面

第二个程序来自 `examples/client_forward`——组合层的资源声明：

```embed path="extlibs/xssh/examples/client_forward/main.c" title="extlibs/xssh/examples/client_forward/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xssh/single -include xssh.h impl.c extlibs/xssh/examples/client_forward/main.c -lws2_32 -liphlpapi
（输出全局回复 FIFO 上限的配置自检结果）
```

**刚才发生了什么。** ① `xrtSshClientConfigInit` 的默认 `GlobalReplyLimit` 打印——**全局请求 token FIFO 的容量**（在途 tcpip-forward/cancel 的上限）：有界 FIFO 是全局请求的预算（第 120 章 auth guard 之后的又一道预算——转发请求也要限量）。② 头注释重申设计边界："forwarding 发起端不引入隐藏 listener 或线程"——本地 listener 是你的组装件不是库的私产。③ 真实转发服务的完整组装在测试与第 123 章运行时里——本示例锁定的是"转发开销=一个 FIFO 上限"的资源事实。

## 契约

- **两形态**：direct-tcpip（客户端开，目标声明）/forwarded-tcpip（服务端开回来，源自监听）。
- **全局请求**：tcpip-forward want-reply；token 动态有界 FIFO；端口 0=服务端动态分配（SuccessRead 取回）。
- **取消**：TcpipForwardCancel 撤销同一 remote 地址。
- **端口边界**：语义层统一 0..65535；非法写入不推进、非法读取协议错误（线路 uint32 之上的规范化）。
- **地址借用**：string 原样——不强迫转 socket address；目标解析权在服务端、访问控制归服务策略层。
- **confirm 顺序**：读事务提交→响应→channel 状态提交——应用不在 Packet 回调直接发 confirmation。
- **默认拒绝**：未决定而接受 Packet 时默认回 UNKNOWN_CHANNEL_TYPE——peer 不空等。
- **数据面复用**：通道 I/O/背压/窗口/关闭统一第 121 章词汇——转发不另设状态机。
- **零隐藏**：不建本地 listener/线程/等待——服务组装归上层。
- **报文零分配**：复用公共前缀构建器一次成文；reader 严格拒绝尾随。

## 避坑

### 坑 1：把目标地址当本地地址解析

症状：客户端尝试解析 `db.internal` 失败——拒绝建隧道；或解析成了客户端视角的同名机器——连错目标。

原因：direct-tcpip 的目标地址是**服务端视角**的（主机名/IP 原样借用）。客户端不解析——解析权与访问控制都在服务端。

```c bad
if ( !xrtNetAddrParse(Target, &Addr) ) {
	fail();   /* 客户端解析服务端视角地址：无意义且常失败 */
}
```

```c good
/* 地址原样进报文——服务端解析与鉴权 */
xrtSshDirectTcpipOpenWrite(&Writer, Id, Win, MP,
	TargetHostView, TargetPort, SourceView, SourcePort);
```

### 坑 2：在 Packet 回调里抢发 confirmation

症状：偶发通道状态错乱——你发的 confirmation 与客户端自动发的竞争重复。

原因：顺序契约是客户端严格三步（提交读事务→发响应→提交通道状态）；`ForwardedTcpipAccept` 解析后 confirmation 是**自动**的。应用抢发破坏事务边界。

```c bad
onPacket(...) {
	parse_forwarded(&Msg);
	send_confirmation(Chan);   /* 抢发：与自动路径竞争 */
}
```

```c good
onPacket(...) {
	if ( xrtSshClientForwardedTcpipAccept(Client, &Msg, ...) ) {
		/* confirmation 已由客户端在读事务提交后自动发送 */
		register_tunnel(&Msg);   /* 应用只登记 */
	}
}
```

### 坑 3：忘了 cancel 就断开（服务端 listener 泄漏）

症状：客户端断开后服务端还听着远程转发端口——占端口、留攻击面。

原因：`tcpip-forward` 的注册在服务端持久；断开前应 `TcpipForwardCancel`（连接正常断开时多数服务端会清理，但显式取消是确定的 polite 路径——尤其长驻连接池里反复建转发场景）。

```c bad
/* 建了远程转发后直接断开 */
```

```c good
/* 关闭序列：先撤转发再关连接 */
xrtSshClientTcpipForwardCancel(Client, BindAddr, Port, ...);
drain_and_close(Client);
```

## 练习

### 基础：五类报文往返

构建/解析五类转发报文（forward/cancel/success/direct/forwarded）——含端口 0 与 65536 两类边界。验收标准：五类往返字段一致；65536 写入被拒不推进。

### 进阶：本地转发最小服务

组装：本地 listener（第 66 章）→ 每连入 DirectTcpipOpen → 双向搬运（通道 I/O ↔ TCP）→ 双端关闭传播。对真实 SSH 服务端转发到其 localhost 的 HTTP 端口。验收标准：curl 经隧道正常取回；慢消费下两端内存恒定；半关传播（一侧 EOF 另一侧照常收尾）。

### 挑战：远程转发表

`TcpipForward(端口 0)` 取动态端口 → 记录映射 → 外部连入的 ForwardedTcpipAccept 按来源分派到本地目标 → cancel 清理。多转发并发。验收标准：动态端口正确取回并记录；cancel 后服务端停止监听（再连被拒）；多隧道并发互不干扰。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 两形态 | direct-tcpip（-L，客户端开）/forwarded-tcpip（-R，服务端开回） |
| 全局请求 | tcpip-forward want-reply；token FIFO；端口 0=动态分配 |
| 取动态端口 | TcpipForwardSuccessRead——成功响应里的 uint32 |
| 端口边界 | 语义层 0..65535；非法写不推进/读报协议错 |
| 地址借用 | string 原样；服务端解析与访问控制——客户端不碰 |
| confirm 顺序 | 读提交→响应→通道状态；应用不抢发 |
| 默认拒绝 | 未决定接受 Packet→UNKNOWN_CHANNEL_TYPE |
| 数据面 | 通道 I/O/窗口/背压全复用第 121 章 |
| 零隐藏 | 不建 listener/线程——服务组装归上层 |
| 组装件 | -L=listener+Direct+搬运；-R=Forward+listener+Accept |
