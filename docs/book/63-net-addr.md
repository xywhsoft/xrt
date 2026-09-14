---
num: 63
slug: net-addr
title: 网络引擎与地址模型
volume: 卷七 网络
type: practice
lead: 五后端网络引擎总览 + xnetaddr 地址模型：解析、端点、分类、区域 ID 与 sockaddr 往返。
api: net, tcp
---

## 导读

卷七开卷讲两件事：**网络引擎总览**（xnetengine——五后端事件引擎的架构鸟瞰，第 61 章骨架的工业化承载）与**地址模型**（`xnetaddr`——IPv4/IPv6 统一的地址抽象：解析、端点序列化、分类判定、区域 ID、sockaddr 往返）。地址是全卷的地基——TCP/UDP/DNS/代理全部以 `xnetaddr` 为货币；引擎总览让后续各章知道“自己在引擎的哪一层”。

## 引入

网络编程的第一道门槛不是 socket，是**地址**：`"[fe80::1%3]:8080"` 这样的字符串——IPv6 要方括号、`%3` 是区域 ID（链路本地地址必须指明挂哪块网卡——Windows 用数字、Linux 用接口名）、跨平台表示各异。手写 sockaddr_in6 的填充与解析是每个网络程序员的入门噩梦，也是 IPv6 迁移失败的头号原因。`xnetaddr` 把这些收口：**一个类型装下 IPv4/IPv6 + 端口 + 区域**，解析与序列化跨平台一致，分类判定（回环/私网/组播/链路本地）一行一个。

第二件事——引擎鸟瞰：XRT 网络引擎的分层（Engine → 事件端口 → Stream/Listener → 协议层）与五后端（IOCP/epoll/kqueue/io_uring/select）。第 61 章的骨架说“调度器泵+外挂”——本章展开泵的底层就是事件端口（第 64 章），Stream 是骨架里“连接协程”的引擎形态（第 67 章金标准）。

## 概念

### 地址模型四件

| 操作 | 入口 | 说明 |
| --- | --- | --- |
| 解析 | `xrtNetAddrParse` / `ParseEndpoint` | 文本→地址；端点形态含端口 |
| 序列化 | `xrtNetAddrString` / `EndpointString` | 地址→文本；端点自动方括号 |
| 分类 | `IsLoopback/IsPrivate/IsMulticast/IsLinkLocal/...` | 判定族一行一类 |
| 原生往返 | `xrtNetAddrToNative` / `FromNative` | sockaddr 结构互转 |

**区域 ID（zone）**是地址模型的硬骨头：链路本地地址（fe80::）必须指明网卡，否则同地址在不同网卡有歧义——解析保留区域、序列化原样回写（`[fe80::1%3]:8080`）、`IsLinkLocal` 区域感知——跨平台差异被收口。**端口 0 语义**：监听时填 0 由系统分配、`xrtNetListenerLocal` 取回真实端口（第 67 章金标准的标准姿势）。

### 引擎分层鸟瞰

```diagram flow
- Engine 层：xnetengine——Worker 池 + 生命周期（Create/Start/Stop/Destroy）
- 事件端口层：xnetport——五后端（IOCP/epoll/kqueue/io_uring/select）统一接口
- Stream/Listener 层：连接与监听的回调模型（第 67 章金标准）
- 协议层：TCP/UDP/代理/帧（第 67-70 章）+ 上层 TLS/HTTP（卷八/九）
```

分层的价值在**替换自由**：事件端口统一接口让引擎在五后端间自动选择最优（Windows IOCP、Linux epoll/io_uring、macOS kqueue、兜底 select）；业务只见 Stream 回调模型——后端变化零感知。这与第 61 章“骨架组件可单独替换”的装配思想一脉相承。

### readiness 与 completion 双形态

事件端口有两种编程模型（第 64 章展开）：**readiness**（“可读了/可写了”的通知——你去读；epoll/select/kqueue 天然形态）与 **completion**（“读完了”——结果直接给你；IOCP/io_uring 天然形态）。XRT 把两种都收进统一接口——能力按后端声明（`xrtNetPortCapabilities`），Windows 的 IOCP 只有 completion、Linux 的 epoll 只有 readiness——**写跨平台网络代码要按能力分支或走 Stream 层（它抹平了差异）**。

### happy-eyeballs：地址列表的消费方式

`xrtNetResolve` 返回**地址列表**（一个域名多个地址——双栈/多网卡/负载均衡）——连接策略按序尝试（IPv6 优先、失败回退 IPv4 的 happy-eyeballs）。列表模型 + `xrtNetAddrListCount/Get` 遍历是消费标准形态；单地址便捷入口是列表的一等选择。resolver 的异步版（Future 形态）在 resolver_future 范例——第 61 章骨架的 DNS 外挂位。

## 示例

### 完整程序：端点解析与分类

来自仓库范例 `examples/network/address/main.c`——区域 ID 的完整处理：

```embed path="examples/network/address/main.c" title="examples/network/address/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/address/main.c -lws2_32 -liphlpapi
endpoint=[fe80::1%3]:8080
family=6
link-local=yes
```

**刚才发生了什么。** ① `xrtNetAddrParseEndpoint` 吃进 `[fe80::1%3]:8080`——方括号内的 IPv6 + `%3` 区域 ID + 端口，一个调用全解析。② `xrtNetAddrEndpointString` 序列化回原样（方括号与区域自动补全——往返一致）。③ `family=6`——地址族查询（IPv6）；④ `IsLinkLocal` 区域感知判定——**链路本地 + 区域 ID 的完整闭环**是地址模型收口跨平台差异的实证。addr_tour 范例是全接口巡礼：相等/比较、分类全族、IPv6-mapped 解映射、sockaddr 往返各一组 ok。

### 完整程序：地址列表解析

来自 `examples/network/dns/main.c`——双栈域名的列表遍历：

```embed path="examples/network/dns/main.c" title="examples/network/dns/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/dns/main.c -lws2_32 -liphlpapi
[::1]:443
127.0.0.1:443
```

**刚才发生了什么。** ① `xrtNetResolve("localhost", 443, UNSPEC)`——域名+端口+族不限 → **地址列表**（拥有式——`xrtNetAddrListDestroy` 释放）。② `ListCount/ListGet` 遍历——localhost 双栈（::1 与 127.0.0.1，顺序可能互换）；每项 `EndpointString` 序列化打印。③ 列表模型就是 happy-eyeballs 的输入：连接方按序尝试直到一个成功——不是“第一个地址一定可用”的假设。这个范例属于 net 模块（不是独立 DNS 模块）——第 66 章展开异步 resolver。

## 契约

- **类型统一**：`xnetaddr` 一装 IPv4/IPv6+端口+区域；全卷 API 以它为货币。
- **区域保真**：解析保留 zone、序列化原样回写；链路本地判定区域感知。
- **端口 0**：监听填 0 系统分配、ListenerLocal 取回——并发测试标准姿势（第 67 章）。
- **列表语义**：解析返回列表；消费按序尝试（happy-eyeballs），不假设单地址。
- **引擎分层**：Engine→端口→Stream→协议；业务写 Stream 回调层，后端自动选择。
- **双形态声明**：readiness/completion 按后端能力；跨平台直接代码先查 Capabilities。

### 从示例到工程：地址模型的三个宿主

**配置解析**：服务配置文件里的 "0.0.0.0:8080"、"[::]:443"——`ParseEndpoint` 是配置装载的固定一环（第 43 章配置装配管线串联到这里）；端口填 0 的测试环境由 ListenerLocal 回填报告。**连接发起**：resolve 列表 → happy-eyeballs 按序尝试——第 67 章 TCP 连接章的固定开头；失败的地址记录进日志（哪一栈、什么错）供网络诊断。**网络诊断工具**：分类判定族 + 序列化三形态——iface 范例与本卷后续 net-misc 章的接口枚举都以它为底。三宿主的共同点：**地址的文本表示只在边界出现**（配置/日志/命令行），内部一律 `xnetaddr` 值传递——文本↔地址的转换收在边界两处（入口 Parse、出口 String），中间零转换。

### 与第 61 章骨架的对照

第 61 章说骨架装上 TCP/TLS/HTTP 就是卷七——本章引擎鸟瞰完成第一块拼图：**调度器泵 → 事件端口**（骨架 PollFor ↔ 端口的等待接口）；**连接协程 → Stream 回调**（骨架的 xrtCoGo 连接协程 ↔ Stream 的 Accept/Read/Close 回调表——表达不同（协程直线 vs 回调表）但承载的职责一致）；**Worker 池 → Engine Workers**（骨架的任务池 ↔ 引擎的 Worker 配置）。对照价值：骨架的五项检查清单（泵零阻塞/等待可取消/终态三分/生命周期对齐/观测贯穿）在网络引擎的每一层逐条适用——第 64 章端口的 Cancel、第 67 章 Stream 的状态机、第 68 章服务端的停机——都是清单条目的引擎版执法。**骨架是卷七的地图**。

### IPv6 迁移的工程清单

XRT 的地址模型让 IPv6 就绪（dual-stack ready）成为默认而非补丁——但上线路径仍有一份清单要过。**地址表示**：全部走 xnetaddr（本章坑 2 的执法）；配置模板用 `[::]` 监听双栈（而非 0.0.0.0 的 v4-only）。**列表消费**：happy-eyeballs 按序尝试（坑 1）——v6 优先、v4 回退。**区域 ID**：链路本地地址必须有 zone——配置文档写清本环境的网卡标识规则（数字/接口名按平台）。**mapped 地址**：v4 客户端连双栈服务端可能看到 ::ffff:x.y.z.w 形态——日志与判断前先 Unmap（否则 v4 客户端被误判为 v6）。四条过完，v6 不是迁移而是特性开关。这份清单在卷七收官（net-misc 章）与卷十二工程实践章会各回收一次。

## 避坑

### 坑 1：忽略地址列表直接取第一个

症状：IPv6 配置异常的环境连接失败——第一个地址是 ::1 但 v6 路由坏了；换台机器又好了——环境相关、难复现。

原因：假设“域名对应一个地址”——实际列表可能多地址且顺序不定；只试第一个放弃了回退机会。

```c bad
xnetaddrlist* List = xrtNetResolve(Host, Port, XNET_FAMILY_UNSPEC);
xnetaddr First;
xrtNetAddrListGet(List, 0, &First);   /* 只拿第一个 */
Connect(&First);                       /* v6 坏则整个连接失败 */
```

```c good
xnetaddrlist* List = xrtNetResolve(Host, Port, XNET_FAMILY_UNSPEC);
for ( size_t i = 0; i < xrtNetAddrListCount(List); ++i ) {
	xnetaddr Addr;
	xrtNetAddrListGet(List, i, &Addr);
	if ( TryConnect(&Addr) ) { break; }   /* 按序尝试——v4 回退自然发生 */
}
```

### 坑 2：手填 sockaddr 绕过地址模型

症状：IPv6 区域丢失、字节序错乱、跨平台 ifdefs 满天飞——手写 sockaddr_in6 填充的经典三连。

原因：绕过 `xnetaddr` 直接操作原生结构——模型收口的跨平台处理全部手工重做。

```c bad
struct sockaddr_in6 Sin;
memset(&Sin, 0, sizeof(Sin));
Sin.sin6_family = AF_INET6;
Sin.sin6_port = htons(8080);
inet_pton(AF_INET6, "fe80::1", &Sin.sin6_addr);
Sin.sin6_scope_id = 3;   /* 手填区域——平台差异自己扛 */
bind(Fd, (struct sockaddr*)&Sin, sizeof(Sin));
```

```c good
xnetaddr Addr;
xrtNetAddrParseEndpoint(&Addr, "[fe80::1%3]:8080", 0);
uint8 Native[64];   /* 原生 sockaddr 存储（不透明缓冲） */
size_t iSize = 0;
xrtNetAddrToNative(&Addr, Native, &iSize);   /* 原生往返——区域/字节序由模型管 */
BindVia(Native, iSize);
```

## 练习

### 基础：往返律验证

对五个端点形态（v4/v6/v6+zone/仅地址/带端口）做解析→序列化→再解析——断言两次解析的地址相等（往返一致律）。

### 进阶：分类全族

构造每类地址各一个（unspec/loopback/private/multicast/linklocal/mapped），逐类打印判定；对 mapped 地址（::ffff:1.2.3.4）验证 Unmap 后变 v4。

### 挑战：地址表工具

实现 `addrinfo 工具`：输入任意端点，输出族/分类/是否公网/序列化三种形态（地址/端点/带zone）；对十个真实环境地址（本机网卡/公网 DNS/组播保留段）输出报告。验收标准：与系统 getaddrinfo 的结果分类一致；区域 ID 保留；判定族全部经测试。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 四件 | Parse/ParseEndpoint、String/EndpointString、判定族、ToNative/FromNative |
| 区域 ID | v6 链路本地必须；解析保留、序列化回写、判定感知 |
| 端口 0 | 监听系统分配 + ListenerLocal 取回 |
| 列表 | Resolve 返回列表；按序尝试（happy-eyeballs），不假设单地址 |
| 引擎分层 | Engine→端口→Stream→协议；业务在 Stream 层 |
| 双形态 | readiness（可读了）/completion（读完了）；Capabilities 声明、Stream 抹平 |
