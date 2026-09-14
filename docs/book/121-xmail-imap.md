---
num: 121
slug: xmail-imap
title: IMAP：服务端邮箱
volume: 卷十一 其他扩展库
type: practice
lead: tagged 响应与 literal 流式读取、两级命令模型、SELECT 摘要与 IDLE 推送、COMPRESS 与流水线——服务器权威的收件协议。
api: xmail-imap, xmail-imap_client, xmail-imap_command
---

## 导读

IMAP 是 POP3（第 118 章）的"服务器权威"对偶：邮件留在服务端、文件夹/标记/搜索/FETCH 部分获取都在服务器上做——多设备同步的基石。协议复杂度也高一个量级：**tagged 响应模型**（每命令带唯一 tag，服务器响应按 tag 关联——支持流水线）；**literal 机制**（含任意字节的参数与响应走 `{N}` 长度声明——同步 literal 要等续行确认、`LITERAL+` 免同步）；**untagged 事件流**（EXISTS/RECENT/FETCH 更新随时插队）。xmail 的分层：协议原语（`imap`：响应/literal/capability/命令——不建邮箱对象不构造搜索条件，**未知扩展按原始文本可达**）→客户端（`imap_client`：两级命令模型——低层显式 tag 流水线 + 顺序便利层）→命令便利层（`imap_command`：SELECT/LIST/SEARCH/FETCH/IDLE 的安全构造）→ 专项层（auth/body/message/append/compress）。

## 引入

IMAP 的三个机制值得开场。**其一：literal 流式**。FETCH 一封 20 MB 邮件，响应里是 `{20971520}` 后跟原始字节——客户端**必须按明确长度分段读**：`ReadLiteral` 用调用方缓冲逐段取、**未读完前不能读后续响应**——"消息大小不会变成固定内存上限"是协议层的保证（与 POP3 逐行流式同族、但按长度而非按行）。**其二：IDLE 推送**。`IDLE` 命令让服务器在新邮件到达时主动推 untagged 事件——"长连接等新邮件"的服务器版（SSE 的邮件版）。**其三：流水线**。低层 `Send` 显式 tag——发多条再统一 `Receive` 按到达顺序收事件、按 tag 关联 completion——IMAP 的并发原语（连接复用的吞吐形态）。

注入防御同款在场：`CommandWrite` 拒 atom 错误/控制字符/线路注入/超限；`imap_command` 的邮箱名"按 IMAP string 规则校验和引用，**不能通过 CRLF 注入额外命令**"。

## 概念

### 响应模型与 literal

```diagram flow
- 响应三类：tagged（命令完成——按 tag 关联）/ untagged（* 开头——事件流插队）/ continuation（+ 开头——literal 续行）
- 状态五态：OK/NO/BAD/PREAUTH/BYE（稳定识别）
- literal 三形态：同步 {N}（等客户端续行确认）/ LITERAL+ {N+}（免同步直发）/ binary ~{N}
  ——协议层只解析长度与标记；数据由状态机按长度读取（零复制零隐式缓存）
```

**literal 预算双向**：发送侧（APPEND 大邮件——第 120 章组合）与接收侧（FETCH 大 literal）。接收侧的契约：`HasLiteral` 事件先查 `Event.Literal.Size`——超应用预算**立即 Abort**（不必消费正文——"先看菜单再决定吃不吃"的内存防御）；低层流式**无默认上限**（上限归应用）；`imap_message` 便利层自带 64 MiB 预算（两者不共享——各层各策）。

### 两级命令模型

- **低层**（`Send/SendParts/Write/Continue/Receive/ReadLiteral`）：显式 tag 流水线——多发命令、按序收事件、tag 关联；`SendParts` 参数片间插空格**直写线路**（不构造整条临时命令）。视图借线路缓冲——**下一次 Receive/Next/ReadLiteral 后失效**（第 118 章行视图的同族纪律，这里多了 literal 读取也是失效点）。
- **顺序层**（`Begin/BeginParts/Next`）：自动唯一 tag；`Next` 把 untagged（`XMAIL_NEXT_ITEM`）与目标 completion（`XMAIL_NEXT_END`）**分态产出**——循环写法统一；活动顺序命令未完不能开下一条。

### 命令便利层：SELECT 摘要与流式结果

`imap_command` 覆盖常用命令但**不建重量级对象**：`Select/Examine` 产零分配 `ximapmailboxinfo`（**Present 位区分"未返回"与"值为零"**——第 102 章存在位的邮箱版；ReadOnly 取最终 completion 的访问模式；`MailboxInfoUpdate` 合并后续 EXISTS/RECENT 事件到同一摘要）。**管理命令**（CREATE/DELETE/RENAME/SUBSCRIBE/CHECK/UNSELECT/CLOSE）完整消费同步完成；**流式命令**（LIST/STATUS/SEARCH/FETCH/STORE/COPY/MOVE/EXPUNGE）只安全构造并开始——结果经 `Next`+`ReadLiteral` 流式消费（便利层不截流）。**IDLE**：进入后 `Done` 结束——推送事件走同一 Receive 循环。

### TLS、压缩与失败语义

**TLS**：隐式/STARTTLS（tagged OK 完整消费后握手；升级后**重新 CAPABILITY**——不沿用明文快照；验证器必须）——与 SMTP/POP3 同族三连。**COMPRESS**（`imap_compress`）：认证后协商 DEFLATE 压缩线路——高延迟链路的带宽优化。**失败语义**三分：**线路 FAILED**（网络/取消/超时/协议错序/解析失败——"无法保证下一字节位于命令边界"，连接不可复用）；**命令 NO/BAD**（服务器对完整命令的否定——客户端**保留可恢复状态**、结构化错误返回——下一命令继续）；**Abort**（无等待中止、可重复、FAILED 与最近 completion 保留到 Destroy 供诊断）。

## 示例

### 第一个完整程序：协议原语

下面的程序来自 `examples/imap/protocol`——离线的响应与命令闭环：

```embed path="extlibs/xmail/examples/imap/protocol/main.c" title="extlibs/xmail/examples/imap/protocol/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xmail/single -include xmail.h impl.c extlibs/xmail/examples/imap/protocol/main.c -lws2_32 -liphlpapi
（输出 IMAP 响应/literal/命令原语的离线自检结果）
```

**刚才发生了什么。** ① 响应三类/状态五态/literal 三形态的解析验证全部无网络完成——与 SMTP/POP3 协议层同款"离线可测、无第二实现"。② literal 解析只产**长度与标记视图**——数据读取是客户端状态机的事（层间职责：解析层管语法、状态机管字节流）；这个切分让"literal 流式"成为可能：解析永远不碰数据本体。③ `QuoteWrite` 的转义（控制数据必须改走 literal）与 `CommandWrite` 的注入拒绝——发送侧的两道闸都在原语层可单测。

### 第二个完整程序：会话与压缩配置

第二个程序来自 `examples/imap/client`——真实会话与 COMPRESS 协商：

```embed path="extlibs/xmail/examples/imap/client/main.c" title="extlibs/xmail/examples/imap/client/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xmail/single -include xmail.h impl.c extlibs/xmail/examples/imap/client/main.c -lws2_32 -liphlpapi
（对配置的 IMAP 服务器完成会话与压缩协商后正常退出）
```

**刚才发生了什么。** ① `ImapCompressConfigInit/Valid`——压缩线路的配置校验（窗口/等级等）；认证后执行 COMPRESS 命令、其后线路字节即 DEFLATE 流。② 压缩的意义与代价：移动网络省 60-80% 线路字节；CPU 换带宽——配置层的 Valid 就是"代价声明"的入口。③ 会话骨架（Open→CAPABILITY→认证→SELECT→…→Logout）贯穿两级命令模型——本示例聚焦压缩插入点；`message`/`body` 示例覆盖 FETCH 流式与 MIME 边界的消费侧。

## 契约

- **原语无模型**：不建邮箱对象/不构造搜索/FETCH 模型——未知扩展按原始文本可达。
- **响应三类五态**：tagged/untagged/continuation；OK/NO/BAD/PREAUTH/BYE 稳定识别；全视图借用。
- **literal 契约**：解析只给长度与标记；数据分段读取；未读完不能读后续响应——消息大小≠内存上限。
- **literal 预算**：低层无默认上限（应用自查 Size、超限立即 Abort）；message 层 64 MiB 独立预算。
- **两级模型**：低层显式 tag 流水线（SendParts 直写线路）/顺序层自动 tag+分态 Next；顺序命令不并发。
- **视图失效点**：Receive/Next/ReadLiteral 任一发生即失效线路缓冲视图。
- **命令便利层**：SELECT 零分配摘要（Present 位/事件合并）；管理命令同步完成；流式命令不截流。
- **注入防御**：命令/邮箱名注入拒绝；控制数据走 literal。
- **TLS**：隐式/STARTTLS；升级后重 CAPABILITY；验证器必须。
- **压缩**：认证后 COMPRESS；配置先 Valid。
- **失败三分**：线路 FAILED 不可复用/命令 NO-BAD 可恢复/Abort 幂等留诊断。

## 避坑

### 坑 1：literal 事件先读后查大小

症状：`HasLiteral` 直接开始 ReadLiteral——下一封是 500 MB 邮件（历史归档），读到一半内存或预算崩。

原因：契约明示"先检查 `Event.Literal.Size`"——大小在事件里、读取在决定后。低层无默认上限＝上限是你的责任。

```c bad
if ( Event.HasLiteral ) {
	while ( ReadLiteral(...) ) { consume(); }   /* 未查大小：500MB 直灌 */
}
```

```c good
if ( Event.HasLiteral ) {
	if ( Event.Literal.Size > MyBudget ) {
		xrtImapClientAbort(pClient);   /* 超预算：立即中止不吃正文 */
		return;
	}
	while ( ReadLiteral(...) ) { consume(); }
}
```

### 坑 2：流水线响应按发送顺序等 completion

症状：死等命令 A 的 tagged 响应——服务器实际先回了 B 的（或先插了 untagged 事件）。

原因：IMAP 响应**按服务器到达顺序**回，不承诺与发送同序；untagged 事件随时插队——按 tag 关联、按到达消费，别按发送序等待。

```c bad
send(A); send(B);
wait_tagged(A);   /* 服务器可能先回 B：挂等 */
wait_tagged(B);
```

```c good
send(A); send(B);
while ( Receive(&Event) ) {
	if ( Event.kind == TAGGED ) { dispatch_by_tag(Event.Tag); }
	else { handle_untagged(Event); }   /* 插队事件即时处理 */
}
```

### 坑 3：视图跨过下一次 Receive

症状：保存的响应视图/fragment 读到错乱——线路缓冲已被下一次读取覆盖。

原因：失效点是**三个**（Receive/Next/ReadLiteral）——比 POP3 的"下次读取"更细：literal 读取也会动缓冲。要跨调用保存就复制。

```c bad
Subject = Event.ResponseView;   /* 存视图 */
Receive(&Event2);               /* 缓冲已换 */
use(Subject);                   /* 错乱 */
```

```c good
Subject = copy_view(Event.ResponseView);   /* 跨调用先复制 */
Receive(&Event2);
use(Subject);
```

## 练习

### 基础：响应与 literal 解析矩阵

构造 tagged/untagged/continuation 与同步/LITERAL+/binary literal 样本走原语。验收标准：五态识别正确；三形态 literal 长度与标记解析一致。

### 进阶：SELECT 与新邮件监视

SELECT 收摘要（Present 位验证）→ IDLE 进入→模拟 EXISTS 推送→`MailboxInfoUpdate` 合并→Done 退出。验收标准：摘要的 EXISTS/RECENT 随事件更新；IDLE 期间的 untagged 分态处理正确。

### 挑战：流水线 FETCH 抓取器

显式 tag 流水线并发 4 条 FETCH（不同邮件区间）——按到达消费、tag 关联、literal 分段写盘。验收标准：乱序响应正确关联；20 MB 邮件内存恒定；预算超限路径干净 Abort。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 模型 | 服务器权威：文件夹/标记/搜索/FETCH 部分获取都在服务端 |
| 响应 | tagged（tag 关联）/untagged（插队事件）/continuation（literal 续行） |
| literal | 同步 {N}/LITERAL+ {N+}/binary ~{N}；解析只给长度——数据分段读 |
| 预算纪律 | 事件先查 Size 再决定；低层无默认上限；message 层 64 MiB 独立 |
| 两级模型 | 低层流水线（显式 tag）/顺序层（自动 tag+分态 Next） |
| 视图失效 | Receive/Next/ReadLiteral 三失效点——跨调用必复制 |
| 命令层 | SELECT 零分配摘要（Present 位）；流式命令不截流 |
| IDLE | 服务器推送——Done 结束；事件走同一 Receive |
| TLS/压缩 | STARTTLS 后重 CAPABILITY；COMPRESS 配置先 Valid |
| 失败三分 | 线路 FAILED 不可复用/NO-BAD 可恢复继续/Abort 幂等 |
