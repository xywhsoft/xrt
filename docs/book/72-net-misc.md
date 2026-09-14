---
num: 72
slug: net-misc
title: 分帧与异步文件：卷七收官
volume: 卷七 网络 · 卷七收官
type: practice
lead: 行分帧与长度前缀帧把字节流还原成消息，原生异步文件把磁盘挂进同一个 Engine——卷七的两件收官工具。
api: net_frame, net_file, net
---

## 导读

卷七的主线是"让数据流起来"：地址（62）、端口（63）、缓冲（64）、DNS（65）、TCP（66–67）、UDP（68）、代理（69）、接口（70）。收官这一章补两件**拼装级工具**，它们不引入新的连接对象，而是把已有部件黏合成完整方案：**帧解析器**回答"TCP 这条没有边界的字节流怎么还原成一条条消息"——第 65 章的 `xnetbuf` 是它的输入，第 67 章的接收回调是它的驱动；**原生异步文件 I/O** 回答"文件服务器怎么把磁盘读写挂进与网络同一个事件循环"——第 64 章的 Worker 归属规则在这里再次兑现。两者都是零引擎依赖的小模块，却分别支撑着文本协议（Redis/SMTP 风格）、私有二进制协议（RPC/游戏服）与代理缓存、静态文件服务这几大类真实负载。

## 引入

两个场景，一个共性。场景一：你在 TCP 上实现一个文本协议，客户端发来 `PING\r\n`，服务端回 `PONG\r\n`。听起来简单，但 TCP 不保证"一次 send 对应一次 recv"——三次发送可能粘成一次到达（粘包），一次发送也可能拆成两半到达（半包）。`PING\r` 先到怎么办？裸写就是手忙脚乱的下标算术。场景二：你在写一个代理缓存或文件服务，网络的读写都挂在 Engine 上异步推进，但磁盘读文件是阻塞调用——Worker 一读盘，同 Worker 上的几百条连接全部卡住。

两个场景的共性是：**边界问题**。前者缺消息边界，帧解析器补上；后者缺"磁盘与网络统一的事件边界"，原生异步文件补上——文件操作直接提交到 Worker 的完成端口，Windows 走 IOCP 的 `ReadFile`/`WriteFile`，Linux 走 io_uring 的 `READV`/`WRITEV`，与 socket 事件同池分发，Worker 永不因磁盘而阻塞。

## 概念

### 三态解析：增量帧器的统一节奏

两种帧器（行分隔、长度前缀）共享同一个三态返回模型：

```diagram state
MORE -> READY: 输入足够，解析出一条完整帧
MORE -> MORE: 输入不足，保留前缀等待追加
READY -> MORE: 消费当前帧后继续解析下一条
任意 -> ERROR: 配置非法 / 状态破坏 / 帧超限
```

`XNET_FRAME_MORE` 是**正常增量控制结果**，不设置错误——它就是"粘包半包被状态机吸收"的体现。`XNET_FRAME_READY` 时输出的 `xnetframe` 描述当前输入头部的一条完整帧：`PayloadOffset`/`PayloadSize` 定位载荷、`FrameSize` 是整帧字节数、`Declared` 保存协议长度字段原值。`XNET_FRAME_ERROR` 携带结构化错误（`FRAME_CONFIG`/`FRAME_STATE`/`FRAME_LIMIT`/`FRAME_LENGTH` 四类），通常意味着协议流已经不可信，应关闭连接或丢弃输入后重置帧器。

### 行分帧：任意分隔符的增量搜索

`xnetlineconfig` 三要素：`Delimiter` 分隔符（**借用视图**，必须存活到帧器不再使用；没有长度上限，`\n`、`\r\n`、甚至多字节哨兵都可以）、`MaxPayload` 载荷上限（默认 8192；`SIZE_MAX` 是显式无界）、`IncludeDelimiter` 是否保留分隔符（默认去除）。默认配置就是 LF 分隔。

帧器内部保存块指针与块内偏移（`xnetlineframer`），因此即使输入分散在大量单字节引用块里，定位旧偏移也不必反复遍历链头——这是它与"每次从头扫描"的分水岭，也是第 65 章引用块追加的正确消费方式。分隔符跨缓冲块、自重叠（如分隔符 `\r\n` 遇到输入 `\r\r\n`）、恰好落在载荷上限上，全部受支持。

一条关键契约：`Next` 返回 `MORE` 之后，调用方**只能保留原输入前缀并在同一个 `xnetbuf` 尾部追加**；消费、替换、换缓冲、重排输入之前必须 `xrtNetLineReset`（保留配置、丢弃增量进度）。`Reset` 的另一用途是同一条连接"重新开始对话"时复用帧器，省一次初始化。

### 长度前缀分帧：一条公式走天下

私有二进制协议最常用的分帧：头部 1–8 字节长度字段 + 载荷。帧总长由一条公式决定：

```text
FrameSize = LengthOffset + LengthSize + Declared + Adjustment
```

- `LengthOffset`/`LengthSize`：长度字段在帧内的位置与宽度（1..8 字节）；`Order` 明确大小端，独立于主机字节序。
- `Adjustment`：有符号调整。协议长度字段若已包含头部，用负值扣回。
- `Strip`：载荷起点——保留完整头部、只去掉长度字段、或去掉整个应用头，一个字段三种选择。
- `MaxFrame`：总帧硬上限（默认 1 MiB）——恶意或损坏的声明长度在这里被拦住。

默认配置是"4 字节大端载荷长度、去除长度字段、1 MiB 上限"，正是多数 RPC 的格式。所有边界——字段末端、无符号声明值、有符号调整、`size_t` 转换、strip 与上限——都在访问载荷前验证，不存在"先信长度再越界读"的路径。

### 帧的两种取用：复制与零拷贝

`Next` 产出帧描述后，载荷有两种取法。**便利路径** `xrtNetFrameCopy`：把不超过输出容量的载荷字节复制进调用方缓冲，小消息一行搞定。**零拷贝路径**：直接用第 65 章的 `xrtNetBufSpans`/`xrtNetBufPeek` 拿载荷视图——大帧不必复制。取用完成后 `xrtNetFrameConsume` 从输入头部精确移除整帧（它会先验证帧范围仍完整位于头部，输入被改动过就拒绝并报 `FRAME_STATE`）。注意帧描述**借用当前输入**：输入前缀被消费、替换或重排后，旧帧描述不得继续使用。

### 原生异步文件：完成端口上的磁盘

`net_file` 模块把普通文件的定位读写提交到 Network Engine 的完成端口，用于文件服务、代理缓存与自定义存储管线——不创建 Future、不占用 TaskPool、不复制调用方载荷。五个要点：

- **打开**：`xrtNetFileOpen` 自动附加 `XFILE_ASYNC` 标志，选项与第 45 章的 `xrtFileOpen` 同一套（默认只读，新建文件要显式 `XFILE_CREATE|XFILE_WRITE`）。
- **Worker 归属**：`Read`/`Write` 只能在**所属 Worker 线程**提交——同一文件首次提交后固定由同一个 Worker 使用；跨线程提交收到 `XERR_STATE`。主线程想发起，就用 `xrtNetEnginePost` 把提交动作投递到 Worker 上（本章示例的标准姿势）。
- **绝对偏移**：读写都带 `iOffset`，不依赖文件游标——同一文件多个并发读互不干扰。
- **唯一终态**：每个操作返回非零标识，`xnetcompletion` 回调在 Worker 上收到**恰好一次**终态事件（`xnetportevent` 的 `Result`/`Bytes`/`Id` 描述结果）。文件、缓冲与 completion 必须保持到终态到达。`xrtNetFileCancel` 只请求取消，原操作仍通过 completion 收尾——终态可能是 `XNET_RESULT_CANCELLED`，也可能操作已完成而是 `OK`。
- **能力位**：Worker 端口后端无原生文件 I/O 能力时（如 SELECT 兜底后端），提交收到 `XERR_UNSUPPORTED`——明确报告，不退化为阻塞 Worker。

全部操作终结后用 `xrtClose` 关闭文件——句柄类型就是第 45 章的 `xfile`，整个文件家族共享这一个关闭入口，没有为异步文件单设的关闭函数。

## 示例

### 第一个完整程序：CRLF 行分帧与半包吸收

下面的程序来自 `examples/network/frame_line/main.c`：输入被刻意切成 `first\r` 与 `\nsecond\r\n` 两块到达（半包横跨分隔符），帧器逐条还原并演示 Reset 复用：

```embed path="examples/network/frame_line/main.c" title="examples/network/frame_line/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/frame_line/main.c -lws2_32 -liphlpapi
first
second
reset=ok
```

**刚才发生了什么。** ① 分隔符设为 `\r\n`（借用视图直接指向字面量）；第一块 `first\r` 喂进去得到 `MORE`——分隔符只到了一半，帧器把未决前缀记在增量游标里。② 第二块 `\nsecond\r\n` 追加到**同一个** `xnetbuf` 尾部（`MORE` 契约的正用），循环 `Next` 依次解出 `first` 与 `second` 两条完整帧。③ 每条帧走"Copy 到栈缓冲 → 打印 → Consume 精确消费"三步，`sizeof(sLine) - 1u` 给结尾零留位。④ 末尾 `xrtNetLineReset` 保留配置、丢弃进度——新会话复用帧器。这就是 Redis/SMTP 类文本协议的分帧底座：**粘包半包由解析器状态机吸收，业务代码只看到一条条完整消息**。

### 第二个完整程序：四字节长度前缀解帧

第二个程序来自 `examples/network/frame_length/main.c`，私有二进制协议的标准样本——`00 00 00 05` + `hello`：

```embed path="examples/network/frame_length/main.c" title="examples/network/frame_length/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/frame_length/main.c -lws2_32 -liphlpapi
hello
```

**刚才发生了什么。** 默认配置（`xrtNetLengthConfigInit`）就是四字节大端长度、去除长度字段、1 MiB 上限，直接命中本协议格式。`LengthNext` 一步解出帧描述，`FrameCopy` 把载荷复制进 5 字节缓冲（返回 5 恰好等于载荷长度，多一位给结尾零）。对照行分帧示例看返回模型：`LengthNext` 遇到"长度字段不完整"或"声明长度未到齐"同样返回 `MORE`，调用方的追加-重试循环与行分帧完全同构——**换一种帧格式，不改驱动节奏**。

### 第三个完整程序：完成端口上的文件读写与取消

第三个程序来自 `examples/network/file_tour/main.c`，把"提交-终态-取消"完整走一遍：绝对偏移写、读回验证、在途读取消后仍收唯一终态：

```embed path="examples/network/file_tour/main.c" title="examples/network/file_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/network/file_tour/main.c -lws2_32 -liphlpapi
file: open(async) + write completed at offset 0 ok
file: read-back verified + reopen append ok
file: cancel in-flight read -> terminal event ok
```

**刚才发生了什么。** ① 文件选项显式 `XFILE_READ|XFILE_WRITE|XFILE_CREATE|XFILE_TRUNCATE`——默认只读，新建文件必须声明，`xrtNetFileOpen` 在此之上自动附加异步标志。② 提交走**标准两跳**：主线程 `xrtNetEnginePost` 把 `exampleWriteTask` 投递到 0 号 Worker，任务函数**在 Worker 上**调用 `xrtNetFileWrite` 并登记 completion——这是"只能在所属 Worker 提交"契约的落地形态。终态回调 `exampleDone` 记录 `Result`/`Bytes`/`Id`，主线程自旋等 `bDone` 并核对三件事：结果 OK、字节数吻合、事件 `Id` 与提交返回值一致。③ 读回用同一姿势，`memcmp` 验证内容。④ 取消段先提交一个 4096 字节大读，自旋一小段确认未完成后 `xrtNetFileCancel`——终态**必达**且结果只可能是 `OK`（读恰好已完成）或 `CANCELLED`；注意 8 字节文件读 4096 字节会立即 EOF 完成，所以示例先等待再决定是否取消，这个"取消可能输给完成"的竞态处理正是真实代码的样子。⑤ 收尾 `xrtClose(File)` → 停 Engine → 删临时文件，顺序不可颠倒。

## 契约

- **帧器生命周期**：帧器是调用方栈上的值对象，无共享状态；不同实例任意线程并发、同一实例顺序调用均安全；配置（分隔符视图）必须存活到帧器弃用。
- **MORE 契约**：`MORE` 后只允许保留原输入前缀并在同一 `xnetbuf` 追加；消费/替换/换缓冲/重排前必须 `Reset`；`MORE` 不设错误。
- **帧借用输入**：`xnetframe` 的偏移相对当前输入头部；输入前缀变动后旧帧失效；`Consume` 验证范围后精确消费。
- **帧上限**：行载荷超 `MaxPayload` 报 `FRAME_LIMIT`；长度声明溢出/超限/调整非法报 `FRAME_LENGTH`；配置本身不可能合法报 `FRAME_CONFIG`。
- **文件归属**：`Read`/`Write`/`Cancel` 只在所属 Worker 线程有效（`XERR_STATE`）；同文件首次提交后固定同一 Worker；主线程经 `EnginePost` 转交。
- **文件终态**：操作返回非零 `Id`；completion 恰好一次终态（`Result`/`Bytes`/`Id`）；文件、缓冲、completion 保持到终态；`Cancel` 只请求，终态为 `OK` 或 `CANCELLED`。
- **文件能力**：后端无原生文件 I/O（SELECT 兜底）报 `XERR_UNSUPPORTED`，不退化阻塞；Windows IOCP、Linux io_uring。
- **关闭**：全部操作终结后 `xrtClose`；偏移 + 长度超出可表达范围报 `XNET_ERROR_PORT_SUBMIT`。

## 避坑

### 坑 1：MORE 之后动了输入前缀还继续用帧器

症状：偶发 `XNET_FRAME_ERROR` + `FRAME_STATE`，或解出的帧内容错乱——往往在"解到一半去处理别的字节"的代码里。

原因：`MORE` 后帧器的增量游标锚定在**原输入前缀**上。消费头部、替换缓冲、换一个 `xnetbuf`、甚至把前缀搬到别处，都会让锚点失效——契约规定此路之前必须 `Reset`。

```c bad
if ( xrtNetLineNext(&Framer, &Input, &Frame) ==
	XNET_FRAME_MORE ) {
	xrtNetBufConsume(&Input, 8);   /* 动了前缀：增量游标悬空 */
	xrtNetLineNext(&Framer, &Input, &Frame); /* FRAME_STATE */
}
```

```c good
if ( xrtNetLineNext(&Framer, &Input, &Frame) ==
	XNET_FRAME_MORE ) {
	/* 唯一合法动作：原前缀保留，尾部追加新数据 */
	xrtNetBufAppend(&Input, pChunk, iSize);
}
/* 确要丢弃当前输入（如协议重置）：先 Reset 再喂新流 */
(void)xrtNetLineReset(&Framer);
```

### 坑 2：在主线程直接提交文件操作

症状：提交返回 0，线程错误槽里是 `XERR_STATE`——文件打开得好好的，怎么提交都失败。

原因：`xrtNetFileRead`/`Write`/`Cancel` 是 Worker 专属接口，只能在所属 Worker 线程调用；主线程不是 Worker。这不是缺陷而是设计——完成端口操作必须在泵线程上提交，模块用错误码把边界讲清楚。

```c bad
/* 主线程：不在所属 Worker，XERR_STATE，返回 0 */
iId = xrtNetFileRead(pWorker, File, 0, Buffer,
	sizeof(Buffer), &Completion);
```

```c good
/* 标准两跳：Post 把任务投到 Worker，任务函数里提交 */
static void submitRead(xnetworker* pWorker, ptr pUserData) {
	exampletask* pTask = (exampletask*)pUserData;
	pTask->iId = xrtNetFileRead(pWorker, pTask->File,
		pTask->iOffset, (void*)pTask->pData, pTask->iSize,
		&pTask->pIo->Completion);
}
xrtNetEnginePost(pEngine, 0u, submitRead, (ptr)&Task);
```

### 坑 3：在 SELECT 兜底后端上跑文件 I/O

症状：同一份代码在 Windows（IOCP）正常，迁移到只有 SELECT 可用的嵌入式目标后，文件提交全部返回 0。

原因：SELECT 后端没有原生异步文件能力，模块按能力位**明确拒绝**（`XERR_UNSUPPORTED` + `XNET_ERROR_PORT_SUBMIT`）而不是悄悄退化成阻塞 Worker——否则一次磁盘读会卡住整个事件循环，这正是设计要消灭的故障模式。

```c bad
iId = xrtNetFileRead(pWorker, File, 0, Buffer,
	sizeof(Buffer), &Completion);
if ( iId == 0u ) {
	/* 假设是临时错误，死循环重试：永远 UNSUPPORTED */
}
```

```c good
iId = xrtNetFileRead(pWorker, File, 0, Buffer,
	sizeof(Buffer), &Completion);
if ( iId == 0u && xrtErrorIs(xrtGetError(),
	XERR_UNSUPPORTED) ) {
	/* 后端无原生文件能力：改用第 48 章的异步文件
	   模块或专用磁盘线程，而不是重试 */
}
```

## 练习

### 基础：给行分帧换分隔符

把行分帧示例改成 `IncludeDelimiter = true` 且分隔符仍为 `\r\n`：输出的两行应该带什么结尾？再换成单字节 `\n` 并把第二块输入拆成 `\nsec` 与 `ond\n` 两段，验证照样解出 `second`。验收标准：两种改动的输出与手推结果一致，程序返回 0。

### 进阶：带消息头的长度帧

设计协议"1 字节类型 + 2 字节小端长度 + 载荷"：配置 `LengthOffset = 1`、`LengthSize = 2`、`Order = XNET_FRAME_LITTLE_ENDIAN`、`Strip = 3`，构造两条粘在一起的报文一次性喂入，循环解出并按类型字节分别打印。提示：`FrameSize` 公式此时是 1 + 2 + Declared；两条帧连续 `Next`/`Consume` 即可。

### 挑战：极简静态文件服务器骨架

用第 67 章的事件面 TCP 服务 + 本章长度前缀帧器 + 异步文件，实现"客户端发 8 字节文件偏移请求 → 服务端读 4 KiB 块回传"的骨架：请求分帧用 `xrtNetLengthNext`（在 Read 回调里对 `Recv` 缓冲增量解析），磁盘读用 `xrtNetFileRead`（Worker 内提交），响应按"4 字节长度 + 数据"组帧经 `xrtNetStreamSend` 发送。验收标准：并发两客户端交替请求不串流；SELECT 后端启动时优雅报告能力缺失并退出；Engine 销毁无泄漏（第 6 章统计复核）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 三态 | `READY`（完整帧）/ `MORE`（等追加，不设错）/ `ERROR`（四类帧错，流不可信） |
| 行分帧 | 任意长度分隔符（借用）；MaxPayload 默认 8192、SIZE_MAX 无界；IncludeDelimiter 控制去留 |
| MORE 契约 | 只许保留前缀 + 同一 `xnetbuf` 尾部追加；动前缀前先 `Reset`（留配置弃进度） |
| 长度帧公式 | `FrameSize = LengthOffset + LengthSize + Declared + Adjustment`；Adjustment 可负 |
| 长度帧默认 | 4 字节大端、去长度字段、1 MiB 上限；宽度 1..8、大小端自选、Strip 定载荷起点 |
| 取载荷 | 小消息 `FrameCopy`（受容量截断）；大帧 `BufSpans`/`BufPeek` 零拷贝；`FrameConsume` 精确消费 |
| 异步文件 | `NetFileOpen` 自动 XFILE_ASYNC；绝对偏移读写；不建 Future、不占 TaskPool、零载荷复制 |
| Worker 归属 | Read/Write/Cancel 仅所属 Worker（跨线 XERR_STATE）；主线程经 `EnginePost` 两跳提交 |
| 终态 | 非零 Id ↔ 事件 Id 对应；completion 恰一次；Cancel 只请求，终态 OK 或 CANCELLED |
| 后端能力 | IOCP（Win）/io_uring（Linux）原生；SELECT 明确 `UNSUPPORTED`，不阻塞退化 |
| 关闭 | 全部终态后 `xrtClose(File)`；帧器/缓冲为栈值或 `BufClear`，无独立销毁 |
