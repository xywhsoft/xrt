---
num: 64
slug: net-buf
title: 缓冲与缓冲池
volume: 卷七 网络
type: practice
lead: xnetbuf 引用链缓冲——四类追加（复制/借用/接管/回调）、拼合与消费、池化复用与释放回调。
api: net
---

## 导读

`xnetbuf` 是网络层的**引用链缓冲**：数据以 span（段）链组织而非连续块——网络数据的天然形态（分段到达、部分消费）；**四类追加**按所有权分（复制/借用/接管/回调释放）——第 5 章所有权语言在 IO 缓冲上的全集落地；**拼合与消费**（Pullup 前缀连续化、Consume 前缀消费——零拷贝前进）；**缓冲池**（xnetbufpool——段块复用、分配收口）。它是引擎收发的直接载体——第 66 章金标准章的 `SendBuffer` 零拷贝回显正是它的舞台。

## 引入

一段网络数据的一生：分段到达（TCP 分段、UDP 数据报）→ 拼装成消息 → 部分消费（协议头读完剩体）→ 发往下游。连续缓冲（第 15 章 xbuffer）适合"攒齐再取”，但网络数据的“分段到达+部分消费”形态让连续化每一步都在拷贝——头到达时拷一次、体追加再拷一次、消费头后又搬一次。

引用链缓冲换模型：数据以段链保存——**到达即成段（零拷贝）、消费即推进指针（零拷贝）、只在需要连续视图时 Pullup（局部拷贝）**。四类追加把各种来源（收到的一段、栈上的头、malloc 的块、带回调的外部内存）统一进一条链——所有权差异在入链时声明，出链时（Clear）各按各的释放。池化让段块（链节点）复用——高频收发的分配次数降到接近零。

## 概念

### 四类追加：所有权的全集

| 入口 | 所有权 | 典型来源 |
| --- | --- | --- |
| `xrtNetBufAppend` | 复制 | 任意内存的安全入链 |
| `xrtNetBufAppendBorrow` | 借用 | 调用方栈/静态数据（出链不释放） |
| `xrtNetBufAppendTake` | 接管 | xrtMalloc 的块（出链 xrtFree） |
| `xrtNetBufAppendRef` | 回调 | 外部内存+释放函数（出链回调） |

四类 = 第 5 章**拥有/借用/托管**的缓冲版全集（复制是拥有的一种、Take 是拥有转移、Borrow 是借用、Ref 是自定义托管）。buf_tour 范例四类各拼一段组成 "hello world"——**释放正确性**有断言：AppendRef 的回调在 Clear 后必须恰好一次。**Prepend**（链首插入）与 **Move**（整块转移）补充形态操作。

### 形状与消费

```diagram flow
- 形状查询：Empty / Size / SpanCount / Spans——链的总览
- Pullup(前缀N)：前缀 N 字节连续化（跨段局部拷贝——解析头的前提）
- Peek(偏移)：偏移复制查看（不动链）
- Find(字节)：跨段查找（分隔符定位，未命中 XRT_NPOS）
- Consume(N)：消费前缀 N 字节（链头推进——零拷贝前进）
```

**Pullup 是链与连续视图的桥**：协议解析需要连续内存看头（ memcmp/struct 读取）——Pullup 把前缀 N 字节拼进第一段（不足则拷贝合并），链其余部分不动；**Consume 是零拷贝前进**：消费 N 字节只是释放走过的段——协议状态机的读头→消费循环零多余拷贝。这两个操作合起来是**协议解析的标准引擎**（第 83 章 HTTP 头解析用它组装）。

### 预留与提交

`Reserve + Commit/Cancel`：向链尾预留 N 字节的**可写空间**（直接写入网络的输出路径——引擎要发数据时预留、填入、提交成段）；Cancel 放弃未提交的预留。这是发送侧的零拷贝写入通道——引擎 Send 前的填充姿势。

### 缓冲池

`xrtNetBufPoolCreate`（ConfigInit 配置段大小/水位）——段块从池取、Clear 归还池——收发高频路径的分配次数逼近零（第 22 章池化思想在 IO 段块上的落地）。`PoolTrim` 收紧空段；池的信息查询带活跃/空闲计数（观测字段）。**池与缓冲的关系**：缓冲（链逻辑）不强制用池——裸链也能跑（每次段 malloc）；引擎与高频场景配池——`PoolGet` 取带段的缓冲、Clear 自动回池。

## 示例

### 完整程序：四类追加与链操作

来自仓库范例 `examples/network/buf_tour/main.c`——全接口六组断言：

```embed path="examples/network/buf_tour/main.c" title="examples/network/buf_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/buf_tour/main.c -lws2_32 -liphlpapi
buf: pool create + info ok
buf: append x4 borrow/take/ref -> 11 bytes in >=4 spans
buf: prepend+pullup+peek+find+consume ok
buf: reserve-cancel keeps 11 bytes ok
buf: move source->target ok
buf: trim released >=1 block, release-cb fired once
```

**刚才发生了什么。** ① 池创建与信息查询——`pool create + info`。② **四类追加的联合演出**：复制 5 + 借用 2 + 接管 2 + 回调 2 = 11 字节、至少 4 段——所有权在入链时声明、段数与字节数双重断言。③ 消费三件套：Prepend（链首插）、Pullup（前缀连续化——跨段头解析的前提）、Peek/Find/Consume（查看/查找/前进）——协议解析引擎的完整操作面。④ Reserve+Cancel：预留后放弃、链保持 11 字节（预留不污染已提交数据）。⑤ Move：源链整体移入目标、源恢复空——缓冲交接零拷贝（第 66 章引用移交的底座）。⑥ Trim 收回空段 + **release-cb fired once**——AppendRef 回调恰好一次的释放正确性证言（所有权的执法验收）。

### 完整程序：基础收发

来自 `examples/network/buffer/main.c`——写入内容打印的简单验证：

```embed path="examples/network/buffer/main.c" title="examples/network/buffer/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/buffer/main.c -lws2_32 -liphlpapi
（写入内容逐段打印——链的内容遍历）
```

**刚才发生了什么。** ① 基础 Append 与内容打印——最简链生命周期（Init→Append→查看→Unit）。② 这是 buf_tour 的原子版——四类追加与消费三件套都从这里长出来。**两范例的分工**：入门先跑本例（Init/Append/Unit 三步），buf_tour 是全接口验收（六组 ok 对应六个操作族）。第 66 章金标准的 `SendBuffer`（段链直移回显——零拷贝）会在引擎实战里再见本类型——缓冲链是引擎数据面的货币。

## 契约

- **所有权四类**：复制/借用/接管/回调——入链声明、出链各按各的（Ref 回调恰好一次）。
- **段链形态**：分段零拷贝；连续视图用 Pullup（局部）、消费用 Consume（零拷贝前进）。
- **预留通道**：Reserve→填→Commit 成段 / Cancel 放弃——发送侧零拷贝写入。
- **池化**：段块回池复用；Trim 收紧；池信息带活跃/空闲（观测）。
- **Move 交接**：整链转移零拷贝——引用移交的缓冲版（第 66 章 SendBuffer 底座）。

### 从示例到工程：缓冲链的三个宿主

**引擎收发面**（最热路径）：引擎回调交来 `xnetbuf`（段链——数据已在段里零拷贝）、处理者消费（Consume 零拷贝前进）、回显直移（SendBuffer 引用移交——金标准章的核心演示）；发送侧 Reserve/Commit 填充。全程一次拷贝都没有——引用链的完全体。**协议解析器**：Pullup(头长) → 解析 → Consume(头) → 循环——第 83 章 HTTP 头解析的底座；Find 定界符（分隔符协议）、Peek 查看（判型后分发）。**数据搬运**：文件→网络（第 47 章异步读出的块 AppendTake 入链——接管零拷贝）、网络→文件（链 Move 到写通道）——跨 IO 域的数据交接以链为集装箱。三宿主共同点：**链是数据面货币**——与值树是卷四控制面货币形成对照（控制面值树、数据面链——两条货币各走各的通道，经域名（解析结果地址）握手。

### 与第 15 章 xbuffer、第 47 章异步的分工

三个缓冲工具容易混淆，一张对照定分寸。**xbuffer（第 15 章）**：连续字节累积器——"攒齐再取"（文件读入、协议解码的攒包阶段）；拷贝语义、Take 整块取走连续。**xnetbuf（本章）**：引用段链——"分段到达、部分消费"（网络收发、协议状态机）；零拷贝前进、段级所有权。**异步文件结果（第 47 章）**：`xfiledata`——一次 IO 的一块数据（拥有式）——它的 Data 可 AppendTake 入链（文件→网络零拷贝）。三者的选择看数据形态：连续需求用 xbuffer、段链消费用 xnetbuf、单块交接用 filedata→Take 入链。协议栈的典型流水线：端口收段→链→头解析→体攒 xbuffer→值树——三工具各在其位。

### 一个设计观：零拷贝的边界

本章全在讲零拷贝——但要认清它的边界。**零拷贝的正确位置**：热路径（每消息执行）的数据移动——引擎到处理者、处理者到下游；**拷贝的合理位置**：跨界（协议边界连续化——Pullup 局部）、所有权转换（Borrow 源不稳改 Append 复制）、小数据（几字节的头拷贝比段管理便宜）。**判断式**：数据量大且生命周期清晰→零拷贝；数据小或生命周期复杂→拷贝换简单。buf_tour 里 11 字节分四段是教学夸张——真实场景链段的粒度由引擎缓冲策略决定（段大小池配置）；处理者见到的链已经合理分段。**滥用零拷贝的代价是心智负担**（冻结期、借用契约、回调释放）——第 5 章所有权语言的每一课都在这里兑现。

## 避坑

### 坑 1：借用段过源释放

症状：链里读到悬空数据——Borrow 的段在源释放后成野指针；时序相关难复现。

原因：Borrow 的契约是"源在链存续期间存活"——借用段的源提前释放违反契约（第 3 章借用纪律的链版）。

```c bad
void appendStack(xnetbuf* Buf)
{
	char Header[4] = { 0, 1, 2, 3 };
	xrtNetBufAppendBorrow(Buf, Header, 4);   /* 借用栈数组 */
}   /* 函数返回——栈帧回收，链里的段悬空 */
```

```c good
void appendCopy(xnetbuf* Buf)
{
	char Header[4] = { 0, 1, 2, 3 };
	xrtNetBufAppend(Buf, Header, 4);   /* 复制入链——源生命周期无关 */
	/* 或 Take（接管 malloc 块）/ Ref（自定义释放）按来源选 */
}
```

### 坑 2：忘 Pullup 直接当连续内存用

症状：协议头解析偶发错乱——跨段的头读到拼接错误的值；单段时正常、多段时翻车。

原因：链不保证连续——直接 `memcpy(buf, Spans[0].Data, HeaderSize)` 只看到第一段（头可能横跨两段）。

```c bad
xnetspan Spans[8];   /* 调用方数组（返回实际段数） */
size_t nSpans = xrtNetBufSpans(Buf, Spans, 8);
memcpy(Header, Spans[0].Data, 8);   /* 只拷第一段——头跨段时错乱 */
```

```c good
if ( xrtNetBufSize(Buf) >= 8 ) {
	xrtNetBufPullup(Buf, 8);   /* 前缀 8 字节连续化（跨段局部拷贝） */
	xnetspan Out[1];
xrtNetBufPullup(Buf, 8, Out);
memcpy(Header, Out[0].Data, 8);
}
```

## 练习

### 基础：四类复现

用四类追加各拼一段组成 "hello world"，打印 Size 与 SpanCount；Clear 后验证 Ref 回调恰好一次。

### 进阶：协议头消费循环

模拟 TLV 流：随机分段的字节流入链 → Pullup(4) 看长度 → Find 定界 → Consume 消费整条 → 循环到不足一条——段链协议解析的标准循环（第 83 章预演）。

### 挑战：池化收发基准

同一段收发负载（100 万次 1KB 分段入链/消费）分别用裸链与池化链跑——对比分配次数（第 6 章统计）与耗时。验收标准：池化版分配次数低两个数量级；耗时显著占优；Trim 后池空闲归零（收尾干净）；对照表进报告。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 四类追加 | Append 复制 / Borrow 借用 / Take 接管 / Ref 回调——所有权全集 |
| 形状 | Size / SpanCount / Spans(填调用方数组)——链总览；Prepend 链首插 |
| 消费三件 | Pullup 前缀连续化 / Peek 查看 / Find 查找（XRT_NPOS）/ Consume 零拷贝前进 |
| 预留 | Reserve→填→Commit / Cancel——发送侧零拷贝写入 |
| 池 | PoolGet 取带段缓冲 / Clear 回池 / Trim 收紧——分配收口 |
| Move | 整链转移零拷贝——SendBuffer 的底座（第 66 章） |
