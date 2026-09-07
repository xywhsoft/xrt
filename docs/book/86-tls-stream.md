---
num: 86
slug: tls-stream
title: TLS 流：TCP 上的组合层
volume: 卷八 安全
type: practice
lead: StreamAccept 一步接管 TCP 流完成服务端握手，事件驱动明文读写与硬背压——会话机与传输的组合范本。
api: tls, tls_stream, net
---

## 导读

第 85 章的会话机自己不碰网络；第 81 章的客户端拨号已经偷偷用了 Stream 层。本章把 Stream 层（`xtlsstream`）正面讲完：**服务端形态**——TCP Accept 回调里 `xrtTlsStreamAccept` 一步建立"TCP 流 + TLS 会话机"的组合对象，之后业务只看到明文读写；**事件模型**——Open/Read/Writable/Close 四回调（第 81 章客户端同一套）；**背压**——明文接收 Available + 短写等待 Writable，慢客户端不会撑爆服务端内存；**生命周期**——共享配置引用计数，监听器与全部连接关闭后统一释放。这是全库"骨架组合"模式的又一次兑现：Engine（第 63 章）+ Listener/Stream（第 66 章）+ TLS 会话机（第 85 章）拼成一个可 `openssl s_client` 直连的完整服务。

## 引入

把第 85 章的回显对打变成真实服务，中间差的只是"传输接线"：Accept 到 TCP 流 → 建组合对象 → 握手自动跑 → 事件回调送明文。听起来薄薄一层，但每一处都是坑位：握手期间的字节谁喂？写背压卡住时明文消费停在哪？连接关闭与监听器关闭怎么协调才不泄漏？Stream 层把这层接线标准化——你写的是 `Echo` 回调里的业务逻辑，生命周期与背压由组合对象管理。

服务的形态也值得看：一个进程、一个 Engine、一个 Listener、N 条 TLS 连接**共享**同一份服务端配置（身份/ALPN）与事件表——配置引用计数让"装配一次、N 连接复用"零拷贝。这与第 82 章身份的共享、第 84 章验证器的共享连成一条线：**TLS 的全部重对象都是不可变共享值**。

## 概念

### 组合对象的建立：Accept 与 Dial 两侧

- **服务端**：`xrtTlsStreamAccept(TCP流, 服务端配置, 流配置, 事件表, 用户数据, &流)`——在 TCP Accept 回调内一步建立组合对象；握手自动跑，成功后 Open 回调触发。
- **客户端**：`xrtTlsDial/DialAsync`（第 81 章）内部就是"TCP 拨号 + StreamAttach 客户端方向"——客户端没有独立的 Attach 范例因为拨号已内联。

两侧共享 `xtlsstream` 类型与全部应用接口——Send/Available/Buffer/Consume/Close/Abort/State（第 81 章速查）。差异只在建立方式与方向。

### 事件模型与硬背压

四回调与 TCP 流（第 66 章）完全同构：`Open`（握手完成/READY）、`Read`（明文到达）、`Writable`（发送预算恢复）、`Close`（唯一终态：`xnetresult` + 结构化错误）。**硬背压的机制**：明文接收区有水位（`Available` 非零即待处理），业务没消费完之前接收侧不再推进——回显服务把"消费明文"与"发回明文"绑在同一循环：逐 Span `Send`，短写（`XTLS_AGAIN`）就**暂停消费**等 `Writable` 再续——慢客户端的 TCP 窗口收缩 → TLS 记录发不出去 → 明文消费暂停 → 接收水位不动 → 内存恒定。这就是第 61 章"泵零阻塞"承诺在 TLS 上的兑现。

### 配置共享与生命周期

```diagram flow
- 装配：身份（第 82 章）+ 服务端配置 + 流配置 + 事件表 —— 全部栈上或共享对象
- 接入：每条 TCP 连接 StreamAccept 一次，组合对象持有配置引用
- 运行：N 连接并发，回调在 Engine Worker 上执行（第 63 章）
- 关闭：每连接 Close 回调递减计数；监听器关闭置位；两者归零后共享对象统一释放
```

共享配置在 Listener 与全部连接关闭前不可释放——示例用原子计数（Connections/ListenerClosed）编排这个汇合点。单连接的规则与第 81 章一致：Close/Abort → 等 `XTLS_STREAM_CLOSED/FAILED` → Destroy。

### TLS 特有的接线细节

三个值得知道的实现衔接：**握手字节**由组合层自动喂（你从没调过 Feed——StreamAccept 后握手就是"传输的事"）；**传输查询**`xrtTlsStreamTransport(流)` 借出底层 TCP 流——Open 回调里查对端地址（`xrtNetStreamRemote`）打日志是标准动作；**两级关闭**——`StreamClose` 走 close_notify 排空（第 81 章），Close 回调在组合对象终态时触发一次。配置分两份：`xtlsserverconfig`（协议面：身份/ALPN）与 `xtlsstreamconfig`（流面：缓冲水位等）——协议与传输的关注点分离到配置层。

## 示例

### 第一个完整程序：事件驱动 TLS 回显服务

下面的程序来自 `examples/tls/stream/main.c`——可用 `openssl s_client` 直连的完整 TLS 1.3 Echo 服务：

```embed path="examples/tls/stream/main.c" title="examples/tls/stream/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/stream/main.c -lws2_32 -liphlpapi
usage: stream <rsa|p256|p384|ed25519> ...
```

**刚才发生了什么。** ① **Echo 主循环是硬背压的范本**：`Available → Buffer → Front 取 Span → Send(部分受理 iWritten) → Consume(iWritten) → AGAIN 即返回`——`Read` 与 `Writable` 都进这个无重复路径，慢客户端永远拖不爆内存。② **Accept 接线**：TCP Accept 回调里 `StreamAccept` 一步建组合对象——本例随即 `Destroy`（演示所有权：Accept 成功即拿到引用，事件表已接管后续；真实服务保持连接自然运行到关闭）。③ **Open 里的传输查询**：`StreamTransport` 借出 TCP 流查 `Remote` 地址打印"TLS client open: 地址"——TLS 层与传输层的桥就这么一座。④ **生命周期汇合**：Close 回调递减 Connections、ListenerClose 置位——主线程等两者归零再释放共享配置；这就是"N 连接共享一份装配"的关闭侧编排。⑤ 对照第 66 章的 TCP 回显并排读：事件模型、Span 消费、短写续传逐条同构——多出来的只有握手与两级关闭。

### 第二个完整程序：Stream 层全特性巡检

第二个程序来自 `examples/tls/stream_tour/main.c`——五行为你覆盖 Stream 层的进阶面：

```embed path="examples/tls/stream_tour/main.c" title="examples/tls/stream_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/stream_tour/main.c -lws2_32 -liphlpapi
stream-tour: two clients upgraded, echo verified
client(starttls): vec=9 bytes read=4 consume=5 session+data=ok
client(attach): async-vec=ok future-written=9 pending=0
bounds: 64 -> 86 ciphertext, error=(none)
http1-tls: request+response parsed on stream ok
```

**刚才发生了什么。** ① 第 1 行两条客户端接入路径同检：**starttls**（明文连接上协商升级——第 86 章练习的官方答案）与 **attach**（TCP 流直挂）——加上服务端 Accept，Stream 层的三种建立方式全集。② 第 2 行演示 Vec 批量发送（9 字节）与 Span 消费（读 4 消费 5——消费可以超前于读取边界，水位语义）。③ 第 3 行走异步 Vec 写入：Future 完成后 `Pending` 归零——第 67 章发送五档的 Vec/异步档在 TLS 流上的样子。④ 第 4 行测量密文边界：64 字节明文 → 86 字节密文（16 字节 AEAD 标签 + 记录头开销）——给容量规划一个真实锚点。⑤ 第 5 行在流上跑完整 HTTP/1 请求响应解析——把第 83～86 章的原语串成一条链的自检样本，也是卷九的预告。

## 契约

- **建立**：服务端 `StreamAccept`（TCP Accept 回调内一步）；客户端经 `TlsDial/DialAsync` 内联 attach；两侧共享 `xtlsstream` 全部应用接口。
- **事件**：Open/Read/Writable/Close 与 TCP 流同构；回调在 Engine Worker 执行；Close 是唯一终态通知（结果 + 结构化错误）。
- **硬背压**：明文消费停滞即接收停滞——Span 逐块回送 + 短写等 Writable 是标准回显形态；内存与最慢客户端解耦。
- **传输桥**：`StreamTransport` 借出底层 TCP 流（只读查询：地址/状态）；不可缓存跨回调使用。
- **两级关闭**：`Close` 走 close_notify 排空；`Abort` 立即中止；单连接等 CLOSED/FAILED 再 Destroy；服务级等"全部连接 + 监听器"归零再释放共享配置。
- **配置分离**：`xtlsserverconfig`（协议面）与 `xtlsstreamconfig`（流面）各自独立；配置对象共享引用计数。
- **握手自动化**：Feed/Drive 循环由组合层执行——应用从不见密文（`stream_tour` 的边界测量属诊断工具用法）。
- **裁剪**：`XRT_MODULE_TLS_STREAM` 独立（依赖 NET）——只要会话机不要传输组合时可裁掉。

## 避坑

### 坑 1：Read 回调里把明文复制完再慢慢处理

症状：高吞吐下内存尖峰——每条连接的 Read 回调都把接收区整份复制进自己的队列，背压机制被架空。

原因：Stream 层的背压建立在"消费明文（Consume）"上；复制不清费等于没消费，接收侧继续推进，你的队列成了无界缓冲。

```c bad
static void onRead(xtlsstream* pStream, const xnetbuf* pBuffer, ptr pData) {
	queue_push(g_MyQueue, pBuffer);   /* 复制走但不 Consume：背压失效 */
}
```

```c good
static void onRead(xtlsstream* pStream, const xnetbuf* pBuffer, ptr pData) {
	/* 能处理多少消费多少；处理不了的留给下一次 Read/Writable */
	while ( xrtTlsStreamAvailable(pStream) != 0 ) {
		/* ... Front 取 Span 处理 + Consume ... */
		if ( 下游满了 ) { return; }   /* 停消费＝停接收＝背压生效 */
	}
}
```

### 坑 2：监听器关了就立刻释放共享配置

症状：最后的连接回调里访问已释放的身份/配置——崩溃在最后一个客户端断开时。

原因：共享配置的引用计数在每条连接上——Listener 关闭不等于连接关闭；释放必须等两个计数都归零。

```c bad
listenerClose(...) {
	xrtTlsIdentityRelease(g_Identity);  /* 还有活跃连接在用 */
}
```

```c good
/* 原子计数汇合：Connections==0 && ListenerClosed 才统一释放 */
if ( (xrtAtomic32Load(&Connections, XMEMORY_ACQUIRE) == 0) &&
		(xrtAtomic32Load(&ListenerClosed, XMEMORY_ACQUIRE) != 0) ) {
	release_shared_assembly();
}
/* Close 与 ListenerClose 两个回调都做这个检查 */
```

### 坑 3：把 StreamTransport 借出的 TCP 流存起来长期用

症状：偶发错乱——缓存的 TCP 流指针在组合对象内部状态推进后失配。

原因：`StreamTransport` 是"此刻的传输桥"诊断/查询视图，不是长期句柄。要用传输能力（查地址、状态）就在需要的时刻现取现用。

```c bad
g_SavedTcp = xrtTlsStreamTransport(pStream);   /* 存起来 */
/* 若干回调之后 */
send_on_tcp(g_SavedTcp, ...);   /* 绕过 TLS 层直写传输：状态失配 */
```

```c good
/* 需要时现查（如 Open 里记日志）；发送永远走 TLS 层的 Send */
if ( xrtNetStreamRemote(xrtTlsStreamTransport(pStream), &Remote) ) {
	log_client(Remote);
}
xrtTlsStreamSend(pStream, Data, Size, &Written);  /* 明文走正门 */
```

## 练习

### 基础：openssl 联调

生成自签证书跑通 stream 服务，用 `openssl s_client -connect localhost:8443 -alpn http/1.1` 连接，输入文本验证回显；观察服务端打印的客户端地址。验收标准：回显逐字节正确；断开（Ctrl-C 或 Q）后服务端 Close 回调触发、连接计数归零、进程不退（继续服务下一个连接）。

### 进阶：明文代理（解密侧观察）

在 Echo 之前插一层"审计"：Read 回调里统计每条连接收到的明文字节数与 Span 数，Close 时输出汇总（总字节/平均 Span 大小/连接时长）。验收标准：并发 4 连接各自独立统计；数据与 openssl 侧发送量一致；统计不复制明文（零拷贝路径上完成）。

### 挑战：StartTLS 升级网关

实现"先明文握手命令再升级"的网关：TCP Accept 后不立即 TLS，先按行读命令（第 71 章行分帧），收到 `STARTTLS\r\n` 后调用 StreamAttach 升级为 TLS（其余命令按明文模式回显）。提示：升级前后的同一连接对象——查 `stream_tour` 的 starttls 客户端路径作对照。验收标准：明文阶段命令正常回显；升级后明文命令被拒绝；openssl 经 `starttls` 提示可完成升级后加密会话；连接计数与两种阶段的资源都正确清理。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 组合对象 | 服务端 `StreamAccept`（Accept 回调内一步）/ 客户端 `TlsDial` 内联 attach |
| 事件模型 | Open/Read/Writable/Close 与 TCP 流同构；Worker 线程执行 |
| 硬背压 | 停消费＝停接收；Span 回送 + 短写等 Writable；内存与最慢客户端解耦 |
| 应用接口 | Send（部分受理）/ Available/Buffer/Front/Consume / Close / Abort / State |
| 传输桥 | `StreamTransport` 借出 TCP 流——现查现用不缓存；发送走 TLS 层正门 |
| 配置分离 | xtlsserverconfig（协议面）/ xtlsstreamconfig（流面）；共享引用计数 |
| 服务级生命周期 | 全部连接 + 监听器归零后统一释放共享装配（原子计数汇合） |
| 单连接生命周期 | Close/Abort → 等 CLOSED/FAILED → Destroy（第 81 章规则） |
| 握手自动化 | Feed/Drive 由组合层执行，应用从不见密文 |
| 裁剪 | `XRT_MODULE_TLS_STREAM` 独立于会话机；stream_tour 是全特性自检 |
