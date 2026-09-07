---
num: 15
slug: buffer
title: 动态缓冲区 buffer
volume: 卷三 容器与数据结构
type: practice
lead: 字节世界的 xarray——分段追加、稀疏定点写入、无复制移交，网络与协议的数据积木。
api: buffer
---

## 导读

`xbuffer` 与上一章的 `xarray` 是同门师兄弟：`xarray` 装"元素"，`xbuffer` 装**字节**。它是网络收包、协议编码、文件读写的标准积木——数据从四面八方到来（不定长、不定时序），`Append` 追加到尾、`Write` 定点写（越过末尾自动补零）、`Take` 把拼好的内容**零复制**取走。本章把这三个动作讲透，并区分"逻辑长度"与"物理容量"这对容易混淆的概念。

## 引入

设想一个 TLS 记录解码器：数据经 TCP 分段到达，一条记录可能拆在三个包里；头部声明"记录体长 512 字节"，你要在头到达时就知道"还差多少"，在体凑齐时一次性取走处理。用定长缓冲，你得猜最大记录长；用裸 malloc 拼，每次扩容都是一次分配与拷贝；用 `xbuffer`，`Append` 累积分段、`Size` 对照头部长度、`Take` 取走完整记录——缓冲归零继续攒下一条。这个"攒—凑—取"的循环是协议处理的通用形态，`xbuffer` 就是为它设计的。

还有一个容易低估的收益：缓冲的归零复用让分配次数与"连接数 × 高峰消息数"脱钩，变成与"并发连接数"同阶——一个连接一个缓冲、一条消息一次 Take，全程零新建。第 6 章统计里 pooled 与 backing 的比值，跑起来就能看到这条曲线的形状。

定点写入 `Write` 解决另一个协议痛点：**定长布局**——它让"先写已知字段、后补依赖前文的字段"这种两阶段编码成为自然写法。"第 5 字节是标志位、第 10 到 14 字节是长度字段"——头到达时体长未知，先写标志位、后补长度，中间的空洞自动补零。用连续数组要自己算偏移、自己扩容、自己填零；`Write` 一步完成。

## 概念

### 三个核心动作（先背下来）

| 动作 | 语义 | 典型场景 |
| --- | --- | --- |
| `Append` | 追加到末尾，自动扩容 | 收包累积、流式读取 |
| `Write` | 定点写入，可越过末尾，空洞补零 | 定长协议头、带偏移的编码 |
| `Take` | 拼接为连续块并**移交所有权**，缓冲归零复用 | 攒齐一条记录取走处理 |

`Size` 与 `Data` 是公开字段——逻辑长度与数据指针。读访问直接用字段，编辑走函数，与上一章的约定一致。与 `xarray` 的 `Count` 对比着记：数组的公开字段是元素数，缓冲的是字节数——一个是"多少条"，一个是"多少字节"，这对差异决定了它们的消费者不同：数组消费结构化记录，缓冲消费原始字节流。

### 逻辑长度与物理容量

```diagram flow
- 逻辑长度 Size：已写入的字节数，Append/Write 推进
- 物理容量：内部持有的存储量，只增不减（除非 Trim）
- Reserve：预扩张容量，避免逐段到达时的反复分配
- Trim：归还多余容量，长跑前收紧占用
```

`Reserve(4096)` 在知道总量级时一次到位——收包循环里这行代码能把分配次数从"每包一次"降到"全程一两次"。`Resize` 显式改变逻辑长度（增长补零、缩短截断）。这对概念在所有容器都存在，字节缓冲上看得最清楚。

### 分段存储与拼接时机

`xbuffer` 的内部存储可以分段——连续 Append 未必住在一整块内存里。对外契约却永远是"取走时给连续块"：`Take` 负责在移交前拼接。这个设计的取舍很清楚：**攒的阶段追求分配效率**（新段直接挂上，不搬旧数据），**取的阶段才为连续性付费**（一次拼接）。如果每次 Append 都保持全局连续，等于每来一段就全量搬一次——收包场景下这是灾难。反过来，如果你每次 Append 完就要读全部内容，分段的意义就没了——那说明你其实需要"每条消息一个缓冲"而不是"一个缓冲攒多条消息"。

与第 66 章 `xnetbuf` 的关系在此说清：`xnetbuf` 是网络层的接收缓冲（引用、归还、池化，卷七深入），`xbuffer` 是通用字节累积器。协议解码的常见流水线是 `xnetbuf` 收段 → 拷贝或引用进 `xbuffer` 攒齐 → `Take` 出连续块交给解析器。两者职责不同、可以配合，但不互相替代。

### Take 的所有权语义

`Take` 返回**拥有式**的连续块（`bytes` 指针 + 长度出参），原缓冲的逻辑状态归零、可以继续复用；返回的块由 `xrtFree` 释放。注意"拼接"的含义：如果内部是分段存储，`Take` 会做一次拼接再交出连续块；这保证你拿到的永远是连续内存——解码器不需要处理分段。这与第 66 章网络流的"接收缓冲直接移交"是同一设计思想在不同层的体现。

## 示例

### 完整程序：追加、稀疏写入与移交

来自仓库范例 `examples/containers/buffer/main.c`：

```embed path="examples/containers/buffer/main.c" title="examples/containers/buffer/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/containers/buffer/main.c -lws2_32 -liphlpapi
61 62 63 00 00 7a
```

**刚才发生了什么。** ① `XRT_BYTES_LITERAL("abc")` 构造字节视图（自动去结尾零，第 3 章的约定在字节世界的对应物），`Append` 后 `Size = 3`。② `Write(5, "z")` 在偏移 5 写入——越过了当时的末尾 3，中间的偏移 3、4 被自动补零：转储 `61 62 63 00 00 7a` 里那两个 `00` 就是空洞，`7a` 是 `'z'`。定长协议头"先写标志位、后补长度字段"就是这个姿势。③ `Take` 取走全部 6 字节——返回连续块的所有权，`iSize` 出参带回长度；随后缓冲归零，`Unit` 安全（不会双重释放）。④ 取走的块用 `xrtFree` 释放——拥有式约定与第 5 章完全一致。

### 完整程序：容量与编辑全览（buffer 的体检单）

来自 `examples/containers/buffer_tour/main.c`，覆盖 Reserve/Resize/Trim、编辑族与三种取走方式：

```embed path="examples/containers/buffer_tour/main.c" title="examples/containers/buffer_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/containers/buffer_tour/main.c -lws2_32 -liphlpapi
buffer: reserve/resize/trim size=2
buffer: edit add+insert+remove+append assign=9
buffer: take set-take=6 create-take=3 from=2
```

**刚才发生了什么。** ① 第一行验证容量三件套：Reserve 预扩张、Resize 显式定长、Trim 收紧后 `Size` 语义不变——注意"Trim 后 Size 不变"这个细节：容量收缩砍掉的是空闲空间，已写内容原样保留。② 第二行验证编辑族：加、插、删、追加、赋值（Assign 整体替换内容）按序执行后值正确。③ 第三行是三种取走姿势的对照：在已有缓冲上 `Take`、在新建缓冲上一次构造再取、从视图转换——三条路径殊途同归于连续拥有块。网络代码里 90% 的用法就是第一种：一个复用的缓冲 + 每条消息一次 Take。

### 把三个动作放进真实节奏里

单独看 Append/Write/Take 是三个函数，放进协议处理的节奏里它们是一个循环的三个拍子。以"每条消息 = 头 4 字节长度 + 体"为例：收到任意分段先 `Append` 进缓冲；用 `Size` 对照"已凑齐的头声明长度"判断一条消息是否完整；完整则 `Take` 取走交给解析器，缓冲归零继续攒下一条。三个拍子的循环里，缓冲是唯一的状态载体——不需要消息链表、不需要逐条分配、不需要拷贝两道。第 66 章你会看到这个循环在网络引擎的回调里以每秒百万次的节奏运转，形状与这里的玩具版一模一样。

## 契约

- **公开字段**：`Size`（逻辑长度）与 `Data`（数据指针）直接读；编辑走函数。
- **Write 补零**：越过末尾的空洞自动补零；逻辑长度跳到写入末尾。
- **Take 移交**：返回拥有式连续块（`xrtFree` 释放）；缓冲归零可复用；`Unit` 不会双重释放。
- **失败无副作用**：编辑失败时缓冲保持原状；配合 `Unit` 的清理永远安全。
- **容量纪律**：已知量级先 `Reserve`；长跑驻留前 `Trim`；Trim 只砍空闲空间，不动已写内容。

## 避坑

两个坑围绕同一件事：`Size` 与 `Data` 是"读取那一刻的快照"，任何编辑动作（Append/Write/Take）都可能让快照过期。理解了这一点，两个坑其实是同一个坑的两种表现。

### 坑 1：Take 之后继续读旧 Size

症状：取走后的处理逻辑读到零长度或错误长度，偶发空消息。

原因：`Take` 把缓冲归零复用——调用前的 `Size` 是旧值，调用后已经是 0；用旧变量当长度自然错。

```c bad
size_t iWant = tBuffer.Size;          /* 保存了旧长度 */
bytes pBlock = xrtBufferTake(&tBuffer, &iSize, NULL);
process(pBlock, iWant);               /* iWant 是 Take 前的长度？凑巧相同才对 */
```

```c good
bytes pBlock = xrtBufferTake(&tBuffer, &iSize, NULL);
if ( pBlock == NULL ) {
	return false;
}
process(pBlock, iSize);              /* 长度来自 Take 出参，永远是这次的真实长度 */
```

### 坑 2：把 Data 指针存到扩容之后

症状：与上一章坑 1 同族——悬空指针、随机内容；缓冲增长路径上高发。

原因：`Data` 指向内部存储，`Append`/`Write`/`Reserve` 都可能触发搬迁；指针和长度一样有时效。

```c bad
bytes pView = tBuffer.Data;            /* 记下指针 */
xrtBufferAppend(&tBuffer, pMore, 4096);/* 扩容搬迁 */
consume(pView, tBuffer.Size);          /* pView 悬空 */
```

```c good
xrtBufferAppend(&tBuffer, pMore, 4096);
consume(tBuffer.Data, tBuffer.Size);  /* 用时再取，指针与长度同源同时 */
```

## 练习

### 基础：转储复现

先合上书在纸上写出六个字节的预期值，再按主示例手工构造 `61 62 63 00 00 7a`：先 Append 三个字节，再在偏移 5 写一个字节，逐字节十六进制转储验证空洞补零。

### 进阶：TLV 解码器骨架（协议世界的第一个玩具）

用 `xbuffer` 实现最小 TLV（类型-长度-值）累积器：`feed(字节段)` 追加；`next()` 检查是否攒齐一条完整记录（头 4 字节声明长度），攒齐则 `Take` 出该条、缓冲余量前移继续。提示：Take 是全量取走——需要"取走前 N 字节"时先记住记录边界，用视图消费、再整体处理。

### 挑战：收包性能对比（用数字说话）

写一个模拟收包循环：随机 64～1500 字节的分段、共 10MB，分别用"无 Reserve"与"启动 Reserve(1MB)"两种姿势累积并每 64KB Take 一次。用第 6 章统计对比两版的系统分配次数。验收标准：Reserve 版分配次数显著更少；两版最终拼接结果逐字节一致。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 三动作 | `Append` 尾追加 / `Write` 定点补零 / `Take` 零复制移交 |
| 公开字段 | `Size` 逻辑长度、`Data` 数据指针；编辑后时效即失效 |
| 容量 | `Reserve` 预扩张、`Resize` 定长、`Trim` 收紧 |
| Take 语义 | 返回拥有块（`xrtFree`），缓冲归零复用 |
| 构造视图 | `XRT_BYTES_LITERAL` 字面量直接喂 Append/Write |
| 协议循环 | Append 攒 → Size 对照 → Take 取 → 缓冲复用 |
| 与数组分工 | 数组消费结构化记录（元素数），缓冲消费原始字节（长度） |
| 与 xnetbuf | xnetbuf 收段（网络层），xbuffer 攒齐（通用），流水线配合 |
| 字面量 | `XRT_BYTES_LITERAL` 自动去结尾零，Append/Write 直接吃；拼接常量段零中间分配 |

| 字面量 | `XRT_BYTES_LITERAL` 自动去结尾零，Append/Write 直接吃 |
