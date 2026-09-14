---
num: 95
slug: ws-frame
title: WebSocket 帧：头、掩码与分片重组
volume: 卷九 Web 协议核心
type: practice
lead: 帧头封包与三态解析、按段推进的负载掩码、控制帧穿插的分片重组与增量 UTF-8 校验——RFC 6455 的字节层。
api: websocket, http
---

## 导读

升级完成（第 94 章）后，连接上跑的就是 WebSocket 帧——本章讲字节层的一切。**帧头**：`xrtWsFrameWrite` 把帧描述写成线路字节、`xrtWsFrameParse` 三态解析回头（与第 72 章分帧同构的增量模型）；**掩码**：客户端到服务端的帧必须掩码，`xrtWsMask` 按"负载下标 mod 4"循环异或、**可按段推进**（网络分片不用等整帧）；**分片重组**：一条消息可拆多帧（FIN 标志终结），控制帧允许穿插——`xrtWsMessageInit/FrameBegin/Payload/FrameEnd` 状态机带着**增量 UTF-8 校验**（一个汉字被帧边界劈开也能判合法性）。掩码样例用的是 RFC 6455 §5.7 的规范示例——又是标准向量锚定。

## 引入

为什么 WebSocket 要掩码？历史答案：防缓存投毒——当年有中间设备会把 TCP 流当 HTTP 缓存，攻击者构造"看似 HTTP 响应头"的负载数据骗缓存。掩码让客户端发出的数据不可预测（每帧随机 4 字节密钥），中间设备无法把帧负载误认为协议文本。规范据此规定：**客户端→服务端必须掩码，服务端→客户端必须不掩码**——方向不对称是协议的硬规则，`xwsframeconfig` 的 `Mask` 模式（REQUIRED/FORBIDDEN）在解析时强制执行。

为什么消息要分片？一条 100 MB 的消息一次发出，接收方必须先见帧头（总长未知直到 FIN）才能决定处理方式；分片让发送方流式产出（不知道总量）、接收方流式消费（不攒完整消息）——与 HTTP chunked（第 91 章）同动机。分片协议：首帧带 opcode（TEXT/BINARY）、后续帧 CONTINUATION、末帧 FIN=1；**控制帧（Ping/Pong/Close）允许且只允许穿插在分片之间**——长消息传输中心跳不被阻塞。

## 概念

### 帧结构与头解析

一帧 = 帧头 + 负载。帧头两字节起步：FIN + RSV(3) + Opcode（4 位）+ MASK + 长度（7 位；126→后接 16 位、127→后接 64 位扩展长度）；MASK=1 时再接 4 字节掩码。`xwsframe` 结构描述一帧：`Flags`（`XWS_FRAME_FIN`/`XWS_FRAME_MASKED`/RSV 位——RSV 非零意味着扩展协商了，第 97 章 deflate）、`Opcode`（TEXT/BINARY/CONTINUATION/PING/PONG/CLOSE）、`PayloadSize`、`Mask`。

- `xrtWsFrameWrite(帧, 配置, 输出, 容量, &头长)`：只写**帧头**（负载随后自便）——大负载配 Vec 发送（第 68 章）。
- `xrtWsFrameParse(输入, &解析, 配置, NULL)`：三态 `XWS_FRAME_READY/MORE/ERROR`——头不足等更多（增量），配置的 Mask 模式强制方向规则（客户端方向的解析器拒绝未掩码帧）。

### 掩码：可按段推进的循环异或

`xrtWsMask(数据, 长度, 掩码, 起始下标)`：`data[i] ^= mask[(起始+i) % 4]`——**原地**变换，掩码/解掩码同函数（异或自逆）。第 4 个参数是妙处：数据分段到达时，前段用 0 起、后段用"前段长度"起——**偏移接续，无需等整帧**。`XWS_MASK_SIZE` 恒 4。

### 分片重组与控制帧旁路

```diagram state
消息开始 -> 消息中: 首帧 FrameBegin（opcode 定类型）
消息中 -> 消息中: Payload × N + FrameEnd（该帧完）
消息中 -> 消息中: 控制帧穿插 → 被状态机旁路（不当消息数据）
消息中 -> 消息完成: FIN 帧的 FrameEnd（校验全过）
```

`xrtWsMessageInit(状态, 可选配置)` 建状态；每个帧走 `FrameBegin(帧头) → Payload(负载) → FrameEnd` 三步。状态机做四件事：**分片衔接**（CONTINUATION 的 opcode 合法性）；**控制帧旁路**（穿插的 Ping/Pong/Close 被忽略——调用方在消息状态机之外处理）；**增量 UTF-8 校验**（TEXT 消息的合法性跨帧判定——一个多字节字符被帧边界劈开时，前段挂在状态里等后段拼齐再判）；**完成回调**（消息完整时的 Info：类型/总长）。消息配置可设最大消息长度——防御恶意无限分片。

### 与第 72 章分帧、第 91 章 HTTP 分帧的三级对照

第 72 章是通用长度前缀/行分帧（自定义协议）；第 91 章是 HTTP 正文分帧（三种定界+走私防御）；本章是 WebSocket 分帧（掩码+方向规则+控制帧穿插+分片重组）。三级共享同一套设计词汇：三态返回、失败不推进、增量吸收粘包半包——**学过前两级，本章的新东西只剩协议特有规则**。

## 示例

### 第一个完整程序：帧头往返与分段解掩码

下面的程序来自 `examples/websocket/frame/main.c`——服务端方向的帧头解析与 RFC 掩码样例：

```embed path="examples/websocket/frame/main.c" title="examples/websocket/frame/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/websocket/frame/main.c -lws2_32 -liphlpapi
opcode=1 payload=5 text=Hello
```

**刚才发生了什么。** ① 构造一帧：FIN+MASKED、TEXT、5 字节负载、掩码 `37 FA 21 3D`（RFC 6455 §5.7 规范样例）；`Config.Mask = XWS_MASK_REQUIRED` 是服务端方向（收到必须掩码）。② `FrameWrite` 只产 7 字节帧头；`xrtWsMask` 原地掩码 5 字节负载。③ `FrameParse` 三态解析回头（READY）——`Parsed.Opcode=1`（TEXT）、负载长 5、掩码还原。④ **分段解掩码**：前 2 字节用起始 0、后 3 字节用起始 2——偏移接续解出 `Hello`。这就是"网络分片到达时无需等整帧"的证明：掩码流的位置由累计偏移决定，与分块边界无关。

### 第二个完整程序：汉字劈开与控制帧穿插

第二个程序来自 `examples/websocket/message/main.c`——分片重组的边界案例：

```embed path="examples/websocket/message/main.c" title="examples/websocket/message/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/websocket/message/main.c -lws2_32 -liphlpapi
message complete
```

**刚才发生了什么。** ① 切分点刻意刁钻：首帧 `{A, 0xE4, 0xB8}`、末帧 `{0xAD, B}`——汉字"中"的 UTF-8 三字节 `E4 B8 AD` 被帧边界劈成两半。② 首帧 opcode=TEXT（消息开始）、末帧 opcode=CONTINUATION + FIN（消息终结）——两次 FrameBegin/Payload/FrameEnd 循环。③ 状态机的**增量 UTF-8 校验**在此显形：首帧结束时 0xE4 0xB8 是不完整序列——不报错、挂状态等下段；末帧拼上 0xAD 后完整合法，消息放行（拼接出 `A中B`）。逐帧独立校验的实现会在这里误报——这就是"跨帧状态"存在的意义。④ 控制帧穿插的旁路语义在测试向量（tests）里覆盖——消息状态机遇到穿插的 Ping 帧直接忽略、不影响 TEXT 消息的重组。

## 契约

- **方向规则**：客户端→服务端必须掩码、服务端→客户端必须不掩码；`Mask` 配置（REQUIRED/FORBIDDEN）在解析时强制——违反方向的帧直接拒绝。
- **帧头三态**：READY/MORE/ERROR；MORE 不消费不报错；失败不推进（同第 72/91 章）。
- **Write 只产头**：负载由调用方发送（掩码自理或按段推进）；容量原子。
- **掩码推进**：`xrtWsMask` 原地异或、起始下标接续——分块边界无关；XWS_MASK_SIZE=4。
- **分片协议**：首帧定 opcode、后续 CONTINUATION、末帧 FIN；控制帧只可穿插分片之间（不可嵌在消息中间当数据）。
- **控制帧旁路**：消息状态机忽略穿插的控制帧——调用方在消息流之外处理 Ping/Pong/Close。
- **增量 UTF-8**：TEXT 消息合法性跨帧判定——多字节序列跨帧边界时挂状态等待，不误报。
- **长度防御**：消息配置限最大消息长；帧级 PayloadSize 超上限拒绝（配置项）。
- **RSV 位**：非零 RSV 意味着扩展（第 97 章 permessage-deflate）——未协商扩展时收到非零 RSV 是协议错误。
- **裁剪**：`WEBSOCKET_FRAME`/`WEBSOCKET_MESSAGE` 帧与消息层独立于 Stream 层（第 96 章）——不接传输也能用。

## 避坑

### 坑 1：掩码每段都从 0 起

症状：分片到达时解出的数据"部分对部分乱"——前段正确、后段错乱，且错乱模式随分块边界变化。

原因：掩码循环以**整帧负载的绝对下标**为相位。第二段从 0 重新起等于相位错位——异或结果系统性错误。

```c bad
recv(seg1); xrtWsMask(seg1, n1, Mask, 0);
recv(seg2); xrtWsMask(seg2, n2, Mask, 0);   /* 相位重置：错乱 */
```

```c good
size_t iOff = 0;
recv(seg1); xrtWsMask(seg1, n1, Mask, iOff); iOff += n1;
recv(seg2); xrtWsMask(seg2, n2, Mask, iOff); /* 偏移接续 */
```

### 坑 2：控制帧穿插时把消息状态机喂乱了

症状：长分片消息传输中来一个 Ping，接收侧消息重组报"意外的 opcode"——消息断了。

原因：控制帧允许穿插是协议特性（心跳不能被大消息阻塞）。消息状态机设计上旁路它们——但**调用方要正确分诊**：opcode 是控制帧就别喂给消息状态机（或喂了就信它旁路——两选一，别混着来）。

```c bad
while ( parse_frame(&Frame) == READY ) {
	message_frame_begin(&State, &Frame);  /* Ping 也喂：状态错乱 */
	...
}
```

```c good
while ( parse_frame(&Frame) == READY ) {
	if ( is_control(Frame.Opcode) ) {
		handle_control(&Frame);   /* Ping/Pong/Close 单独路径 */
		continue;
	}
	message_frame_begin(&State, &Frame);   /* 数据帧才进重组 */
	...
}
```

### 坑 3：逐帧独立做 UTF-8 校验

症状：合法消息被误报非法——恰好有汉字/emoji 跨过帧边界；或更糟，为"修这个问题"干脆关掉校验（放行非法文本）。

原因：UTF-8 是多字节序列，合法性判定天然跨帧。逐帧校验把不完整序列当非法；关闭校验则接受非法文本（协议要求 TEXT 必须合法 UTF-8）。

```c bad
on_frame_payload(seg) {
	if ( !utf8_valid(seg) ) {   /* 帧边界劈开汉字：误报 */
		close_protocol_error();
	}
}
```

```c good
/* 消息状态机内建增量校验：跨帧拼接后整体合法才放行 */
xrtWsMessagePayload(&State, Seg, NULL);   /* 校验由状态机管 */
/* 完成时若曾非法，FrameEnd/Init 路径已按协议报错 */
```

## 练习

### 基础：四方向掩码矩阵

用 RFC 样例掩码对 `Hello` 做四种切分（1+4、2+3、3+2、4+1）分段掩码再分段解掩码，验证全部还原。验收标准：四种切分输出一致；起始偏移参数与累计长度对应。

### 进阶：大消息流式发送器

实现 `send_text_streamed(write_fn, 数据, 总长, 块大小)`：首帧 TEXT（无 FIN）+ N 个 CONTINUATION 帧 + 末帧 FIN——每帧 FrameWrite 头 + write_fn 负载，模拟流式产出。验收标准：接收侧用消息状态机重组出完整原文（含恰好跨帧的多字节字符）；块大小 1 与 4096 两档结果一致。

### 挑战：裸帧级回显服务器核心

不借助第 96 章 Stream 层，在第 67 章 TCP 事件面上手搭帧循环：接收→FrameParse 三态→掩码解（按段推进）→控制帧分诊（Ping 自动 Pong）→数据帧进消息重组→完整消息 FrameWrite 回写（服务端方向不掩码）+ 掩码规则强制（收到未掩码帧拒绝）。验收标准：与浏览器/`wscat` 对打成功收发文本；穿插 Ping 不影响大消息；未掩码帧被拒并回 Close 1002。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 帧头 | FIN+RSV+Opcode+MASK+长度（7/16/64 位扩展）；Write 只产头、容量原子 |
| 三态解析 | READY/MORE/ERROR；MORE 等数据不消费；方向规则由 Mask 配置强制 |
| 掩码 | 客户端→服务端必须、反向必须不；循环异或 mod 4；原地自逆 |
| 分段掩码 | 起始下标=累计负载偏移——分块边界无关；每段从 0 起是经典错 |
| 分片协议 | 首帧定 opcode → CONTINUATION×N → FIN 终结；流式收发两利 |
| 控制帧穿插 | Ping/Pong/Close 可穿插分片之间；消息状态机旁路、调用方分诊 |
| 增量 UTF-8 | TEXT 合法性跨帧判定；序列跨帧挂状态——逐帧校验误报、关闭校验失守 |
| 长度防御 | 消息级最大长 + 帧级上限；防无限分片攻击 |
| RSV 位 | 非零=扩展已协商（第 97 章 deflate）；未协商收到即协议错误 |
| 三级对照 | 71 通用 → 90 HTTP 定界 → 94 WS 掩码分片：同一套增量词汇 |
