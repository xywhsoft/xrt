---
num: 118
slug: xmail-smtp
title: SMTP：发送邮件
volume: 卷十一 其他扩展库
type: practice
lead: 响应解析与能力协商、命令注入防御、dot transparency 的流式 DATA、CHUNKING 快速路径与 STARTTLS——发件的完整线路。
api: xmail-smtp, xmail-smtp_client, xmail-mail
---

## 导读

数据层（第 115 章）定义了邮件是什么；SMTP 定义**怎么发**。xmail 的 SMTP 两层：**协议原语**（`smtp`：响应行解析、EHLO 能力合并、命令构建——不依赖网络，官方客户端与自定义状态机同一实现）；**客户端**（`smtp_client`：同步会话状态机——`READY → MAIL → RECIPIENT → DATA → READY` 的 envelope 事务；不建隐藏 Engine/DNS 线程/固定缓冲，不负责构造 MIME）。四条主线：**注入防御**（命令层拒绝 CR/LF 与控制字符——SMTP 是头注入重灾区）；**流式 DATA**（dot transparency 的增量状态机——跨片段保留行首状态、不建整报文副本）；**CHUNKING**（BDAT 按字节声明直发——不扫描不转义）；**TLS 两形态**（隐式 TLS 与 STARTTLS 升级——升级后能力快照不沿用）。

## 引入

SMTP 是 1982 年的对话协议：客户端发命令（`EHLO`/`MAIL FROM`/`RCPT TO`/`DATA`）、服务端回三位码响应（`250 OK`/`550 拒绝`）。表面的简单之下是三个现代工程问题。**其一：注入**——早期实现的命令拼接把用户输入直接拼进命令行，含 CRLF 的输入能"夹带"额外命令（发件人伪造的经典路径）；`xrtSmtpCommandWrite` 在入口拒绝 CR/LF 与控制字符、512 字节上限。**其二：dot transparency**——DATA 模式里正文行的行首 `.` 必须转义（`.`→`..`）、结束靠单独的 `.` 行；流式发送（不攒整报文）要求转义状态**跨片段**保持——上片段结尾是行首、下片段开头是 `.`，跨界的点必须转义。**其三：二进制**——dot transparency 与 CRLF 要求让 DATA 天然文本；BDAT（CHUNKING 扩展）按声明字节数直发原始块，二进制附件不经转义。

## 概念

### 协议原语：响应、能力与命令

```diagram flow
- 响应：MailLineRead（取无 CRLF 行）→ SmtpReplyLineParse（三位码+多行分隔符）
  → xsmtpreplyparser（状态码一致性/行数限/唯一终止行）→ SmtpReplyRead（增量）
- 能力：SmtpCapabilityParse（借用名称与参数）→ CapabilityAdd（合并常用扩展+
  AUTH 三形态+64 位 SIZE；未知扩展可解析不占位）
- 命令：SmtpCommandWrite（Verb [Args]\r\n——拒控制字符/CR/LF 注入/512B 超限）
```

**路径验证**：`xrtSmtpPathValid` 验证 reverse/forward-path 内容（不含尖括号/空白/控制分隔符）——只验尖括号内部、不构造命令（职责切分：验内容归协议层、组命令归命令层）。

### 客户端：会话状态与所有权

`xrtSmtpClientOpen`：验证 `220` banner → 发 EHLO →（仅当服务器明确 `500/502/504` 才按配置回退 HELO——EHLO 失败的其他码不回退）。状态机 `READY → MAIL → RECIPIENT → READY`（一笔 envelope）；CHUNKING 在首块后进 `CHUNK`、LAST 成功回 READY。**所有权**：配置只在 Open 期间借用；Client 持传输与最后响应、**借用** Engine/Resolver/TLS Context/Verifier（销毁不动共享对象）。**线程**：全部阻塞操作接受绝对 `xdeadline`+可选 `xcancel`；**不能从所属 Engine 的 Worker 回调调用**（第 97 章同款死锁防线）；单 Client 不支持并发命令。**三收尾**：`Quit`（协议告别）/`Close`（跳过 QUIT 等传输关闭）/`Abort`（任意非空状态立即异常中止、重复成功、FAILED 保留最后响应供诊断）。

### DATA 快速路径：流式 dot transparency

`DataBegin/DataWrite/DataEnd` 接受任意分块：内部增量状态机**跨片段保留 CRLF 与行首状态**、直接发送 dot-transparent 片段——**不创建整报文副本**（大附件流式的发送侧）。`xrtSmtpClientData` 连续输入便利入口：先完整验证 CRLF 再委托增量状态机。**调用方仍负责** MIME 字段、传输编码与服务器 SIZE 限制——客户端管线路不管内容（内容归第 115 章模块）。

### CHUNKING：BDAT 快速路径

服务器声明 CHUNKING 后：`BdatBegin/BdatWrite/BdatEnd` 按**声明字节数**直发原始片段——**不扫描内容、不做 dot transparency、不追加 CRLF**、零整块副本。每块同步读 `250`；`4xx/5xx` 后禁止继续发块（必须 RSET/关闭/中止）。**声明长度与实际不一致**：End 拒绝读响应、调用方可补足字节；无法补足必须中止。BINARYMIME 的组合形态：能力位只声明支持，消息类型经 `Mail(..., "BODY=BINARYMIME", ...)` 的通用参数传递——**不为它复制 envelope API**（任意 MAIL 参数组合能力保留）。

### TLS：隐式与升级两形态

基础客户端明文；`smtp_client_tls` 层加两形态：`XMAIL_SECURITY_TLS`（隐式——banner 前完成握手，465 端口形态）；`XMAIL_SECURITY_STARTTLS`（升级——EHLO 声明 STARTTLS、验证 `220` 切换、原流上接管 TLS、**握手后重发 EHLO**）。两条硬规则：**升级不沿用升级前的能力快照**（中间人可能剥能力——重协商才是真相）；**必须提供验证器**——不静默跳过证书验证（第 84 章"不验证不是选项"的邮件版）。

## 示例

### 第一个完整程序：能力解析与命令构建

下面的程序来自 `examples/smtp/protocol`——协议原语的离线闭环：

```embed path="extlibs/xmail/examples/smtp/protocol/main.c" title="extlibs/xmail/examples/smtp/protocol/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xmail/single -include xmail.h impl.c extlibs/xmail/examples/smtp/protocol/main.c -lws2_32 -liphlpapi
（输出能力合并与 EHLO 命令构建的自检结果）
```

**刚才发生了什么。** ① `SmtpCapabilityParse("SIZE 10485760")` 解析单条能力（名称+参数借用视图）→ `CapabilityAdd` 并入内置位——**SIZE 的 64 位上限**（10 MiB）被记录：后续 DATA 前可对照（超限别白发）。② `SmtpCommandWrite("EHLO", "client.example", ...)` 产 `EHLO client.example\r\n`——参数过注入检查（控制字符/CR/LF 拒绝）、512 字节上限。③ **离线可测**是这个示例的要点：协议层不碰网络——响应/能力/命令三族原语都能单测（官方客户端就是这些原语的组装，行为一致无第二实现）。

### 第二个完整程序：完整发送会话

第二个程序来自 `examples/smtp/client`——真实会话的同步闭环：

```embed path="extlibs/xmail/examples/smtp/client/main.c" title="extlibs/xmail/examples/smtp/client/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xmail/single -include xmail.h impl.c extlibs/xmail/examples/smtp/client/main.c -lws2_32 -liphlpapi
（对配置的 SMTP 服务器完成发送会话后正常退出）
```

**刚才发生了什么。** ① `xrtSmtpClientData` 发送完整报文（头+空行+正文）——便利入口先验 CRLF 再进增量状态机（dot transparency 自动）。② `xrtSmtpClientQuit` 协议告别——正常关闭路径；失败路径换 Abort（示例的 Cleanup 形态）。③ 会话全程受 `Deadline`+可选 Cancel 约束——阻塞操作的标准两参（第 81 章以来的通用形态）。④ 与 `submit` 示例对照：那个是**最高层**（`xrtSmtpSubmit`——发件人/收件人/主题/正文的 struct 一次提交，内部组装 MIME+走会话）；本章示例展示中间层（完整报文自备）——**层次越高代劳越多、越低自由越大**，第 119 章把层次图补全。

## 契约

- **协议/客户端分层**：原语不碰网络；客户端是原语的状态机组装——无第二实现。
- **注入防御**：命令层拒控制字符/CR/LF、512B 上限；路径验证只管尖括号内部。
- **响应纪律**：三位码一致、行数限、唯一终止行；增量经 `MailLineRead`→`ReplyRead`。
- **EHLO 回退**：仅明确 `500/502/504` 按配置回退 HELO；其他失败不猜。
- **所有权**：配置 Open 期借用；共享对象（Engine/Resolver/TLS/Verifier）借不销毁；单 Client 无并发命令。
- **阻塞纪律**：绝对 deadline+可选 cancel；禁止所属 Worker 回调内调用。
- **三收尾**：Quit 协议告别/Close 跳 QUIT 等关闭/Abort 立即中止（重复成功；FAILED 留响应）。
- **DATA 流式**：跨片段 dot transparency；零整报文副本；CRLF 归调用方。
- **BDAT**：声明字节直发；不扫描不转义不加 CRLF；每块读 250；长度不符可补足否则中止。
- **BINARYMIME**：能力位+通用 MAIL 参数——不复制 envelope API。
- **TLS 两形态**：隐式（banner 前）/STARTTLS（升级后重发 EHLO、不沿用旧快照）；验证器必须。

## 避坑

### 坑 1：把用户输入直接拼进命令

症状：收件人地址含 CRLF 的输入“成功”发出——实际夹带了额外命令（伪造/放大）。

原因：SMTP 注入的老家：命令是文本行协议，裸拼即注入。`CommandWrite`/路径验证是防线——绕过它们拼字符串就是绕过防线。

```c bad
snprintf(Cmd, "RCPT TO:<%s>\r\n", UserInput);   /* 注入直通 */
```

```c good
if ( !xrtSmtpPathValid(view(UserInput), false) ) {
	return reject("invalid address");   /* 验内容 */
}
xrtSmtpCommandWrite(XRT_STR_LITERAL("RCPT TO"),
	AngleForm, Out, sizeof(Out), &n);   /* 走注入检查的入口 */
```

### 坑 2：STARTTLS 后沿用旧能力快照

症状：升级前缓存的 "支持 SIZE" 在升级后决策——被中间人剥掉能力的连接上发出超限邮件被拒（好情况）或错误启用 CHUNKING（坏情况）。

原因：明文阶段的 EHLO 响应**不可信**（中间人可改）；升级后必须重发 EHLO 取真快照——客户端契约自动做，自己缓存能力就是绕过它。

```c bad
caps_before_tls = parse_ehlo();   /* 明文时缓存 */
starttls();
	use_caps(caps_before_tls);  /* 不可信快照 */
```

```c good
starttls();
caps = client_renegotiated_ehlo();   /* 升级后客户端已重发——用新的 */
```

### 坑 3：BDAT 声明长度与实际不符还硬 End

症状：服务器挂等剩余字节或报协议错——声明 1024 实发 1000 就调 End。

原因：BDAT 的契约是"声明=承诺"；End 检查不符时拒读响应并给补足机会——补不了就中止（连接的块计数已错，无法继续）。

```c bad
BdatBegin(1024);
BdatWrite(实际 1000 字节);
BdatEnd();   /* 少 24 字节硬结：服务器挂起 */
```

```c good
BdatBegin(1024);
sent = BdatWrite(Buf, 1000);
if ( sent < 1024 ) {
	if ( !BdatWrite(Buf + 1000, 24) ) {  /* 补足 */
		xrtSmtpClientAbort(pClient);      /* 补不了：中止 */
	}
}
BdatEnd();
```

## 练习

### 基础：响应解析矩阵

构造单行/多行/不一致码/无终止行的四组响应字节流，走 LineRead→ReplyRead 验证。验收标准：合法组解析正确；非法组结构化错误可区分。

### 进阶：注入防御实验

对 `CommandWrite` 与 `PathValid` 投喂含 CRLF/控制字符/超长的输入——验证全部拒绝。验收标准：五类恶意输入（CRLF/裸 CR/控制字节/512B 超/尖括号）各自被正确入口拒绝。

### 挑战：流式大附件发送

DATA 路径：10 MB 附件分 8 KiB 块流式发送（dot transparency 跨界）——对照 BDAT 路径同附件。统计内存峰值与线上字节（DATA 因转义略多）。验收标准：两路收件一致；DATA 路径峰值恒定；跨界点（块边界恰在行首/点前）测试向量通过。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 分层 | 协议原语（离线可测）→客户端（状态机组装）——无第二实现 |
| 注入防御 | 命令拒 CR/LF/控制/512B；路径验尖括号内部 |
| 会话状态 | READY→MAIL→RECIPIENT→DATA→READY；EHLO 仅明确拒才回退 |
| 所有权 | 共享对象借不销毁；单 Client 无并发；Worker 回调禁调 |
| 阻塞形态 | 绝对 deadline+可选 cancel |
| 三收尾 | Quit/Close/Abort（立即、重复成功、FAILED 留诊断） |
| DATA | 跨片段 dot transparency；零整报文副本；CRLF 归调用方 |
| BDAT | 声明字节直发不转义；每块读 250；不符可补足否则中止 |
| BINARYMIME | 能力位+通用 MAIL 参数——不复制 envelope API |
| TLS | 隐式/STARTTLS 两形态；升级后重 EHLO；验证器必须 |
