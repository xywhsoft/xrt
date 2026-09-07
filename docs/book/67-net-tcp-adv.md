---
num: 67
slug: net-tcp-adv
title: TCP（下）：背压、引用与服务端
volume: 卷七 网络
type: practice
lead: 发送四族与写预算、读缓冲与 Worker 约束、流控暂停恢复、双面 Accept 与共享端口的服务端形态。
api: tcp, tcp_server, net
---

## 导读

TCP 下册讲三块进阶：**发送四族**（Vec 聚集 / Ref 零拷贝+释放回调 / Take 接管 / File 内核文件发送）与**写预算**（WriteLimit 限额 / Writable 可写 / Pending 在途——发送背压的三件套）；**读取与 Worker 约束**（Buffer/Read/Consume 仅 Worker 内——直移缓冲的消费面；跨线程经 Post 投递）；**服务端形态**（xnetserver——拉取 Accept（队列取连接）、Future Accept 与阻塞 Accept 双面并存、多监听与共享端口）。金标准章（第 66 章）的四态与回显是基础形态——本章是它的完全体。

## 引入

三个进阶场景。场景一：文件服务器要把 1GB 文件发出去——read 整个文件到内存再 Send 是灾难（内存爆炸）；`xrtNetStreamSendFile`（内核文件发送+区间有界——第 45 章文件章预告的 sendfile 落点）不经过用户态缓冲。场景二：代理要把上游收到的段链转发下游——复制一份再发是浪费；`SendRef` 直接引用移交（零拷贝+释放回调）。场景三：服务端要限制单个连接的发送积压（恶意客户端不读——缓冲无限膨胀）；**写预算**（WriteLimit 设上限、Pending 查在途、Writable 查余量）是背压的发送侧执法。

这三个场景的共同主题是**发送面的所有权与预算**——第 5 章语言在 IO 出口的全集：Send 复制、SendVec 聚集复制、SendRef 借用+回调、SendTake 接管、SendFile 委托内核。五档按"数据从哪来、谁释放"选——与第 64 章缓冲链四类追加一一对应（那个是入链、这个是出链）。

## 概念

### 发送四族与文件发送

| 入口 | 所有权 | 适用 |
| --- | --- | --- |
| Send | 复制 | 小数据安全路径（金标准章） |
| SendVec | 聚集复制 | 多缓冲一次发出（第 21 章批量） |
| SendRef / SendRefs | 引用+释放回调 | 段链直移（零拷贝——金标准 SendBuffer 的族亲） |
| SendTake | 接管 | xrtMalloc 块（发完引擎释放） |
| SendFile | 委托内核 | 文件区间发送（sendfile/mmap 后端） |

**SendRef 的释放回调**：数据发完（或流销毁）回调恰好一次——与第 64 章 AppendRef 同款纪律；SendRefs 是多段变体（段链一次发）。**SendFile 的区间**：偏移+长度有界——文件的分块发送控制内存（每块内核缓冲而非整文件）。

### 写预算：发送背压三件套

```diagram flow
- WriteLimit(N)：单连接发送预算上限（在途+排队 ≤ N）
- Pending()：当前在途/排队字节数——水位表
- Writable()：余量 = Limit - Pending——可安全提交的量
- 超预算 Send：失败或阻塞（按调用形态）——背压生效点
- 流控配套：Pause/Resume——暂停后引擎停发（内存兜底）
```

**背压的完整链条**：对端不读 → TCP 窗口收缩 → 引擎缓冲积压 → Pending 逼近 Limit → 新 Send 被拒 → **上游感知**（处理者看到发送失败/需等待）——慢消费者无法拖垮服务内存。Pause/Resume 是手动挡（Pausable 语义：Pause 后引擎保留数据不推进——上游可决策丢弃或断开）。第 21 章队列背压的连接版。

### 读取与 Worker 约束

**Worker 内 API**（Buffer/Read/Consume 等——必须在 Stream 所属 Worker 线程调用）：直移缓冲的消费面（Buffer 取链、Consume 前进——第 64 章操作在引擎数据上直接用）；**跨线程入口**（`xrtNetPost` 投递任务到 Stream 所属 Worker——第 55 章调度器 Post 的引擎版）；**等待族**（Wait/WaitAvailable 阻塞式——非 Worker 线程的汇合用；WaitAvailableAsync Future 式——编排用）。**为什么约束**：直移缓冲操作免锁的前提是"单线程"——Worker 约束就是第 55 章调度器绑定纪律的引擎版；跨线程经 Post 传递执行权而非加锁——零锁纪律贯穿。

### 服务端：双面 Accept 与共享端口

`xnetserver`（TCP Server）：**拉取 Accept**（队列式取连接——来一个取一个，上层限流/分发直接控制——tcp_server 范例形态）；**Future Accept**（事件循环侧——编排友好）；**阻塞 Accept**（非 Worker 线程——管理线程等连接）。**双面并存**是迁移期的真实形态（tcp_server_sync 范例：同一 Server 上 Future 与阻塞两种取法同场）。**多监听与共享端口**：一个 Server 多端点（v4+v6 双栈）；SO_REUSEPORT 式共享端口（server_tour 的 `shared-port=1` 断言——多进程共享监听的平台支持）。**Accept 统计**（accepted/queued——服务观测字段）。

### 停机与风暴：服务端收尾

Listener Close（停止接入——在途 accept 排空）、连接上限（ServerConfig 的队列深度——接入背压：满了拒绝新连接而不是无限排）、停机顺序（Listener → 各 Stream → Engine——第 66 章顺序的服务端版）。**连接风暴**的处理位：Accept 队列深度限制 + 快速拒绝（错误成本低）——而不是接受后再断（已分配资源）。

## 示例

### 完整程序：Stream 全族巡礼

来自仓库范例 `examples/network/tcp_stream_tour/main.c`——发送四族+预算+流控+半关闭的完整验收：

```embed path="examples/network/tcp_stream_tour/main.c" title="examples/network/tcp_stream_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/tcp_stream_tour/main.c -lws2_32 -liphlpapi
stream-tour: config write-limit>=64k writable>=0
sends: vec+ref+refs+take+file = 53 bytes verified
worker-read: buffer=8 read="buf-" consume=4 socket+setevents+setdata=ok
waits: read=1 available=1 available-async=1 write=1
flow: pause=held resume=delivered
shutdown: peer-eof=1 stats(sent=65) error=(none)
```

**刚才发生了什么。** ① 配置层：WriteLimit 至少 64k、Writable 起点为 0——写预算三件套的起点。② **发送四族联合**：Vec+Ref+Refs+Take+File 各发一段、53 字节全量验证——五档所有权在一条流上全部走到。③ worker-read 行：Worker 内读缓冲三件（Buffer=8 字节可用、Read 取 "buf-"、Consume 前进 4）+ Socket/SetEvents/SetData——Worker 约束 API 的实操。④ waits 行：四种等待形态（阻塞读/Available 快照/Async Future/写等待）各验一次。⑤ flow 行：Pause 后数据 held（引擎保留不推进）、Resume 后 delivered——流控两拍。⑥ shutdown 行：对端读到 EOF、stats 的 sent 计数（65=53+12 等附加字节）、error=(none) 干净终态。这个 tour 是 Stream 数据面的全操作验收——生产代码的检查表。

### 完整程序：双面 Accept 同场

来自 `examples/network/tcp_server_sync/main.c`——Future 与阻塞取连接并存：

```embed path="examples/network/tcp_server_sync/main.c" title="examples/network/tcp_server_sync/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/tcp_server_sync/main.c -lws2_32 -liphlpapi
accepted Future and synchronous connections on port NNNNN
```

**刚才发生了什么。** ① 动态回环端口的 Server 启动（端口 0 + Local 取回——第 66 章姿势）。② **Future Accept**：事件循环侧取第一条连接——编排友好（后续操作可组合）。③ **阻塞 Accept**：管理线程侧取第二条——普通线程的汇合形态。④ 两种姿势**同一 Server 同场**——迁移期真实形态的实证（从阻塞模型渐进到事件模型不需要一步切换）。server_tour 范例补多端点/共享端口/Accept 统计——服务端全接口。

## 契约

- **发送五档**：Send 复制 / Vec 聚集 / Ref 引用+回调恰好一次 / Take 接管 / File 委托内核——按来源选（与第 64 章入链四类对应）。
- **写预算**：WriteLimit 上限 + Pending 水位 + Writable 余量——慢消费者背压的发送侧执法。
- **Worker 约束**：Buffer/Read/Consume 仅 Worker 内；跨线程经 Post——零锁纪律。
- **流控**：Pause/Resume 手动挡——暂停保留数据，上游决策丢弃或断开。
- **双面 Accept**：拉取/Future/阻塞三形态并存——迁移渐进。
- **接入背压**：Accept 队列深度限制+快速拒绝——风暴挡在门外。

## 避坑

### 坑 1：SendRef 后提前释放数据

症状：接收端偶发收到脏数据——发送回调还没来，数据源已被释放；与第 64 章坑 1 同族。

原因：SendRef 是引用发送——释放回调前数据冻结期（引擎正在读它）。

```c bad
char* Block = xrtMalloc(1024);
Fill(Block);
xrtNetStreamSendRef(Stream, Block, 1024, releaseCb, Ctx);
xrtFree(Block);   /* 回调没来就释放——引擎正在读 */
```

```c good
xrtNetStreamSendTake(Stream, TakeBlock, 1024);   /* 接管——引擎发完释放 */
/* 或 SendRef+回调：回调恰好在数据不再被引用后触发——不动它直到回调 */
```

### 坑 2：无预算发送打爆内存

症状：服务内存随慢客户端增长——某客户端不读，引擎缓冲无限积压；OOM 崩溃在深夜流量高峰。

原因：没设 WriteLimit——发送无上限，TCP 窗口收缩的积压全部堆在服务内存。

```c bad
xnetstreamconfig Config;
xrtNetStreamConfigInit(&Config);   /* 默认无预算限制 */
xrtNetStreamSend(Stream, HugeData, HugeSize);   /* 无界积压 */
```

```c good
xnetstreamconfig Config;
xrtNetStreamConfigInit(&Config);
Config.WriteLimit = 1u << 20;   /* 1MB 预算 */
/* 发送前查余量 */
if ( xrtNetStreamWritable(Stream) < Need ) {
	WaitOrPause(Stream);   /* 背压生效——等或暂停，不上冲 */
}
```

## 练习

### 基础：五档对照

同一数据分别用五档发送（File 档用临时文件）——接收端全量验证；各档的释放时机（回调/接管）打印确认。

### 进阶：背压实验

慢客户端（收 1 字节歇 100ms）+ 服务大量发送：有 WriteLimit vs 无限制对照——观察服务内存曲线（第 6 章统计）与 Pending 水位；超预算的处置路径验证。

### 挑战：文件分发服务

静态文件服务：多客户端并发下载同一文件——SendFile 区间分块（块预算）+ 每客户端 WriteLimit + 全局接受上限。验收标准：1GB 文件服务内存峰值 <100MB（分块+预算的实证）；慢客户端不影响其他客户端；停机全部收尾（三件套协议）；下载校验逐客户端 Hash64 一致。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 发送五档 | Send 复制/Vec 聚集/Ref 引用+回调/Take 接管/File 内核 |
| 写预算 | WriteLimit 上限 + Pending 水位 + Writable 余量——慢客户端背压 |
| Worker 约束 | Buffer/Read/Consume 仅 Worker 内——跨线程 Post 投递 |
| 流控 | Pause/Resume——保留数据的手动挡 |
| 半关闭 | ShutdownWrite——对端读 EOF（单向结束） |
| 双面 Accept | 拉取/Future/阻塞并存——迁移渐进 |
| 接入背压 | Accept 队列深限+快速拒绝——风暴挡门外 |
