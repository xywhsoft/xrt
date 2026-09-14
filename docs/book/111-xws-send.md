---
num: 111
slug: xws-send
title: xws（中）：发送路径——writer、引用与压缩
volume: 卷十一 其他扩展库
type: practice
lead: 长消息分片的 xwswriter、压缩发送的同一套背压语义、三态所有权的完整矩阵——发送侧的全部形态。
api: xws-websocket_runtime, xws-websocket_http
---

## 导读

第 110 章立了发送所有权三态（copy/ref/take）；本章把发送路径走完整。**writer**（`xwswriter`）：长消息的流式分片——`xrtWsConnBeginText` 开启、`WriterWrite` 逐块、`WriterFinish` 终结；writer 的 Header 与压缩状态尺寸稳定（不因内容改变网络对象布局）；**压缩发送**：`BeginTextCompressed` 的压缩 writer（第 97 章协商的 permessage-deflate 在发送侧的形态——压缩启用时**同一套背压与所有权语义**）；**所有权矩阵**：copy/ref/take × text/binary × 同步/Future 的完整组合——巡检第 3 行的“九种发送”就是这张矩阵。发送失败遵守原子所有权：未受理归调用方、受理后连接释放。

## 引入

发送一条 10 MB 的日志快照：copy 路径先复制 10 MB（内存×2 再等排队）；ref 路径要求“一块连续内存”（快照本来就有——合适）；但快照是**流式生成**的（还没攒完）呢？writer 路径：边生成边 `WriterWrite`——每块直接进帧流，内存恒定、分片协议自动（首帧 opcode、continuation、FIN——第 95 章分片规则的发送侧自动化）。再进一步：快照是高重复文本，压缩能省 90%——`BeginTextCompressed` 开启压缩 writer，生成端接口不变（还是 Write/Finish），压缩在 writer 内部发生——**“流式”与“压缩”正交组合**。

writer 的工程细节见真章：“Header 和压缩状态尺寸稳定，不因 hover、事件或动态内容改变网络对象布局”——网络对象的内存形态可预测（第 64 章 Worker 经济学在对象布局层的延伸）；失败路径的原子性（写一半失败——已受理块由连接释放、未受理块归你）让错误恢复不需要猜测状态。

## 概念

### writer 生命周期

```diagram state
空闲 -> 写入中: BeginText/BeginBinary/BeginTextCompressed（连接所属 Worker）
写入中 -> 写入中: WriterWrite × N（任意分块——协议分片自动）
写入中 -> 完成: WriterFinish（末块写入 + FIN + 消息终结）
写入中 -> 放弃: WriterDestroy（未完成消息的丢弃路径）
```

三入口对应消息类型与压缩：`BeginText`/`BeginBinary`（普通）、`BeginTextCompressed`/`BeginBinaryCompressed`（压缩——要求连接协商了 permessage-deflate）。**块的边界与帧的边界无关**：你按业务节奏 Write，writer 按协议策略分帧——大块自动拆、小块可聚合；FIN 只在 Finish 出现（一次消息恰好一个终结）。

### 压缩 writer：同一套语义

压缩启用时 writer 内部挂 deflater（第 97 章 `xwsdeflater` 的连接内集成）：每块压缩后进帧流、RSV1 只标首帧、no-context-takeover 按协商在消息边界复位字典——**Write/Finish 的调用方接口与普通 writer 完全一致**。“压缩启用时保持同一套背压与所有权语义”——AGAIN 还是那个 AGAIN、释放还是那个恰好一次——第 97 章压缩对象的行为在连接发送路径上的无损延续。

### 所有权矩阵与原子规则

| 形态 | copy | ref | take |
| --- | --- | --- | --- |
| 入队时 | 立即复制 | 记引用零复制 | 移交已分配内存 |
| 排队期间 | 连接持有副本 | 原块借用 | 连接独占 |
| 完成/失败 | - | release 恰好一次 | 连接释放 |
| 未受理失败 | 调用方无损 | 调用方保留 | 归还调用方 |

九种同步发送（巡检）= 三所有权 × {text, binary, ping/控制}；Future 形态再加一维等待方式。**选择法则**：短消息 copy（复制成本可忽略）；静态/共享大块 ref（一次分配多处发送——第 112 章广播的原语）；已分配要移交 take（省最后一次复制）。

### writer 与缓冲链

take/buffer 形态可把 **XRT 网络缓冲链**（第 65 章）直接交给连接——生成的块链不经“扁平化复制”直达发送队列；writer 的分帧头与缓冲链头一次 Vec 发送（第 68 章）拼装。发送路径的零复制从“引用”延伸到“链”。

## 示例

### 第一个完整程序：分块流式发送

下面的程序来自 `examples/websocket/writer`——多块消息的 writer 模板：

```embed path="extlibs/xws/examples/websocket/writer/main.c" title="extlibs/xws/examples/websocket/writer/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xws/single -include xws.h impl.c extlibs/xws/examples/websocket/writer/main.c -lws2_32 -liphlpapi
WebSocket Writer example is ready
```

**刚才发生了什么。** ① `xrtWsConnBeginText(连接)` 开启文本 writer；循环对每块调 `WriterWrite`，**最后一块用 `WriterFinish`**（写入+FIN 二合一）——这是 writer 循环的标准形状。② 中途失败 `WriterDestroy`——未完成消息的放弃路径（连接继续可用，只是这条消息没发完）。③ 函数签名带块数组——本示例是模板形态（main 验证就绪）；真实调用在 Worker 回调里（第 110 章纪律）。**为什么 writer 而不是攒完一次发**：块来自流式生成（游标/管道/转储）——攒完整消息就是内存峰值；writer 让“生成的节奏”与“发送的节奏”解耦。

### 第二个完整程序：压缩发送路径

第二个程序来自 `examples/websocket/writer_deflate`——压缩 writer 的同构验证：

```embed path="extlibs/xws/examples/websocket/writer_deflate/main.c" title="extlibs/xws/examples/websocket/writer_deflate/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xws/single -include xws.h impl.c extlibs/xws/examples/websocket/writer_deflate/main.c -lws2_32 -liphlpapi
（压缩 writer 就绪自检通过后正常退出）
```

**刚才发生了什么。** ① 唯一差异在入口：`xrtWsConnBeginTextCompressed`——之后的 Write/Finish/Destroy 循环与普通 writer **逐字相同**。“压缩是 writer 的属性不是发送流程的改写”——调用方代码零改动获得压缩。② 前置条件：连接协商过 permessage-deflate（第 97 章协商层、第 113 章路由配置）；未协商连接上开压缩 writer 会在入口失败。③ 压缩语义由 writer 保证：RSV1 首帧、字典按协商复位、解压上限在对端——**发送侧无需关心第 97 章的压缩规则细节**，协商定了行为就定了。配合 `writer_ref`（引用块的 writer 变体——静态大块的流式终结），发送路径三示例构成完整矩阵。

## 契约

- **writer 生命周期**：Begin（三入口：text/binary/compressed）→ Write×N → Finish（FIN）或 Destroy（放弃）；连接所属 Worker 上调用。
- **块帧解耦**：调用方的分块节奏与协议分帧无关；FIN 恰好一次；控制帧不可穿插在自己 writer 的消息内（第 95 章旁路规则的发送侧——要发 Ping 先 Finish 或用连接级控制入口）。
- **压缩 writer**：入口即差异；Write/Finish 接口与普通 writer 一致；前置为协商；RSV1/字典/上限由 writer 保证。
- **尺寸稳定**：writer 的 Header 与压缩状态尺寸固定——网络对象布局不因内容变化。
- **所有权原子**：未受理归调用方、受理后连接释放恰好一次；take 可交缓冲链（链头零复制直达）。
- **三态选择**：短=copy；静态/共享=ref；已分配移交=take——九种同步组合加 Future 族。
- **背压统一**：writer 写入同样受水位约束——AGAIN 时块未受理（可原样重试）。
- **裁剪**：`websocket_writer`/`websocket_deflater` 独立——不用 writer 的连接零成本。

## 避坑

### 坑 1：writer 开了不收尾（泄漏或半消息）

症状：连接上再也不能发新消息——上一个 writer 还挂着（消息未终结）；或 Destroy 忘调——writer 状态占着连接。

原因：writer 是连接上的**独占发送事务**：Begin 之后必须 Finish（发完）或 Destroy（放弃）二选一——没有第三态“先放着”。

```c bad
pWriter = xrtWsConnBeginText(pConn);
write_some(pWriter);
/* 忘了 Finish/Destroy：连接发送侧被占死 */
```

```c good
pWriter = xrtWsConnBeginText(pConn);
if ( pWriter == NULL ) { return XNET_RESULT_ERROR; }
for ( ... ) {
	if ( WriterWrite(...) != XNET_RESULT_OK ) {
		xrtWsWriterDestroy(pWriter);   /* 放弃路径必走 */
		return Result;
	}
}
return xrtWsWriterFinish(pWriter, LastChunk);
```

### 坑 2：压缩 writer 用在未协商的连接上

症状：`BeginTextCompressed` 返回 NULL——连接没协商 permessage-deflate，压缩入口直接失败。

原因：压缩是**协商后能力**（第 97 章）：连接建立时扩展协商决定了这条连接有没有压缩。发送侧的“想压”不能凭空创造能力。

```c bad
/* 连接协商时没提 deflate */
pWriter = xrtWsConnBeginTextCompressed(pConn);  /* NULL：能力不存在 */
```

```c good
/* 服务端/客户端配置声明 deflate（第 113 章路由/客户端配置）*/
/* 协商成功后按连接能力选择入口 */
if ( conn_deflate_negotiated(pConn) ) {
	pWriter = xrtWsConnBeginTextCompressed(pConn);
} else {
	pWriter = xrtWsConnBeginText(pConn);   /* 回退普通 */
}
```

### 坑 3：ref 块在 release 后又被碰

症状：释放回调后偶发崩溃——调用方在 release 之后又读/写了那块内存。

原因：ref 的契约是“release 之后所有权彻底转移”——release 回调是最后一步。跨回调保存（第 110 章视图纪律）叠加 ref 的释放时序，容易在异步路径上踩。

```c bad
xrtWsConnBinaryRef(pConn, &Ref);
/* ... 异步路径某处 ... */
use(Buf);   /* release 可能已发生：悬空 */
```

```c good
/* release 之后不再碰该块；确要重用，在 release 回调里归还池
   （第 22 章）并只通过池再取 */
static void releaseToPool(ptr pCtx, cbytes pData, size_t iSize) {
	pool_return((pool*)pCtx, (ptr)pData);   /* 归还而非释放 */
}
```

## 练习

### 基础：三入口对照

同一份多块内容分别经 BeginText/BeginBinary/BeginTextCompressed 发送（协商 deflate 的连接），对端按类型与压缩接收验证。验收标准：三种消息对端解出内容一致；压缩版线上字节显著少（统计）。

### 进阶：流式生成→发送管线

游标式数据源（模拟每毫秒产 4 KB）经 writer 直发：Write 跟随生成节奏、Finish 在源头终结；测量峰值内存。验收标准：峰值与总量无关（MB 级）；中途 AGAIN 的处理不丢块（重试语义）。

### 挑战：三态成本对拍

同一 1 MB 消息以 copy/ref/take 三态各发 100 次：统计分配次数、复制字节、耗时（第 6 章工具）。验收标准：三组数据与契约推导一致（copy 复制、ref 零复制一次分配、take 零复制零分配）；写出你的场景的选择结论。

## 速查

| 知识点 | 速查 |
| --- | --- |
| writer 三入口 | BeginText / BeginBinary / Begin*Compressed（协商前置） |
| 生命周期 | Write×N → Finish（FIN）或 Destroy（放弃）——二选一必走 |
| 块帧解耦 | 调用方节奏 ≠ 协议分帧；FIN 恰好一次；控制帧走连接级入口 |
| 压缩同构 | 入口即差异；Write/Finish 接口一致；RSV1/字典/上限 writer 保证 |
| 尺寸稳定 | Header 与压缩状态固定——网络对象布局可预测 |
| 所有权矩阵 | copy（复制）/ref（release 一次）/take（移交/缓冲链）；未受理归调用方 |
| 三态选择 | 短 copy、静态共享 ref、已分配 take |
| 背压 | writer 同受水位约束；AGAIN 块未受理可重试 |
| 裁剪 | writer/deflater 独立宏 |
