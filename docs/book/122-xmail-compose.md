---
num: 122
slug: xmail-compose
title: 组合收发：附件、编码与收发闭环
volume: 卷十一 其他扩展库
type: practice
lead: 一行 Compose 的结构化邮件、三段 multipart 自动选型、SMTP 提交闭环与 Bcc 边界——xmail 五章的收官合体。
api: xmail-mail_compose, xmail-smtp_submit, xmail-mail
---

## 导读

xmail 收官章。前四章给了数据层（116）与三个协议（117-119）；本章合体：**`mail_compose`**（高层组合——`xmailmessage`/`xmailattachment` 全部借用调用方数据，`xrtMailCompose` 一行产完整报文：**三层 multipart 自动选型**（mixed/alternative/related 按内容组合、boundary 彼此不同）、QP/Base64 自动编码、**Bcc 只给提交层不进报文**）；**`smtp_submit`**（最高层发送——struct 一次提交，envelope 从消息推导）；**地址语法层**（`mail_address`：addr-spec 验证、ASCII 默认与 SMTPUTF8 显式开）；**闭环**（Compose→Valid 静态校验→Submit 会话→附件流式编码全程零整报文副本）。层次总图：从 `xrtSmtpSubmit`（一行）到 `CommandWrite`（字节）每层自由下钻。

## 引入

"发一封带附件的 HTML 邮件"在手工 MIME 时代是二十行胶水：选 multipart 结构（mixed 套 related 套 alternative？）、生成不碰撞的 boundary、QP 编正文、Base64 编附件并折行、编码词处理中文 Subject、算 Content-Type……每一项第 116 章都给了原语——但常用组合就该每次手拼吗？`mail_compose` 的答案是**结构化输入+自动选型**：你声明"这是正文、这是 HTML 版、这是两张内联图、这是三个附件"——结构由内容**自动推导**（三层嵌套自动组、boundary 互异自动生成）、编码按类型自动（文本 QP/附件 Base64 分块）、你只在需要**确定性输出**（测试/归档）时注入 Date/Message-ID/boundary。

Bcc 的处理是安全设计的样本：**密送地址只进入 SMTP 提交层的 envelope 收件人**（RCPT TO 有他）、**不进 RFC 报文**（To/Cc 头没有他）——"密送"语义的协议级正确实现；漏了这层（把 Bcc 写进头）就是抄送，收件人互相可见的隐私事故。

## 概念

### Compose：描述、校验与两出口

```diagram flow
- 描述：xmailmessage（From/Reply-To/To/Cc/Bcc/UTF-8 Subject/Text/Html/
  内联资源/附件/自定义字段）——全部借用调用方数据
- 校验：xrtMailMessageValid——只查描述（不生成随机值不分配不调 sink）
  ——"第一次网络写入前发现全部静态输入错误"
- 出口一：xrtMailComposeWrite(描述, sink, ...)——直写流（附件分块编码进 sink）
- 出口二：xrtMailCompose(描述, &长度)——完整报文（xrtFree 释放）
```

**确定性输出的钥匙**：Date/Message-ID/三种 boundary 可由调用方提供——留空才用当前 UTC/安全随机/安全随机（Message-ID 域留空从 From 推导）。测试与归档场景注入固定值→输出逐字节可复现；生产留空→安全随机。**自定义字段不能覆盖 Compose 管理的结构字段**——结构字段（Content-Type/boundary/编码声明）的所有权归组合层，你加字段加不破结构。

### 三层 multipart 自动选型

| 内容组合 | 结构 |
| --- | --- |
| 有普通附件 | `multipart/mixed` |
| 纯文本+HTML 同存 | `multipart/alternative` |
| HTML 带内联资源 | `multipart/related` |
| 三者齐 | 三层嵌套——**boundary 必须彼此不同**（组合层保证） |

编码策略：文本 QP（第 116 章 MIME 行宽版）；附件**按固定小块编码 Base64**——"不创建随附件大小增长的编码副本"（10 MB 附件的编码峰值=小块不是 13 MB）。这是第 108 章流式正文哲学在编码层的落点。

### 地址语法与国际化边界

`xrtMailAddressValid` 验证 addr-spec 并借出 local-part/domain：**默认只接受 ASCII** local-part 与 DNS 风格域名（支持 quoted local-part 与 domain-literal）；**`XMAIL_ADDRESS_SMTPUTF8` 显式设置才接受**国际化 local-part/UTF-8 域名——且 SMTP 客户端仍按服务器能力决定发送（语法过了≠对端收）。**层界**：这层只管报文语法——路径长度、DNS、IDNA、envelope 限制归更高层（第 89 章分层的邮件版：语法层不掺杂传输策略）。

### Submit：最高层的发送闭环

`xrtSmtpSubmit(客户端, 消息, 截止, 取消)`：内部组装 MIME（经 Compose）→ envelope 推导（From→MAIL FROM；To+Cc+**Bcc**→RCPT TO）→ DATA/BDAT 发送——一行完成"结构化描述→线路"。与第 117 章三层对照：`CommandWrite`（字节）← `smtp_client`（会话）← `submit`（业务）——**每层往下钻一行、每层往上升一代劳**。

### 收发闭环全景

```diagram flow
- 发：描述→Valid（静态校验）→Compose（结构+编码）→Submit（envelope+会话+流式发送）
- 收：POP3/IMAP 流式取→MailMessageParse（视图）→multipart 游标→编码词解码→业务
- 存量工程：收下即转存（117 章管道）+ 发送即组装（本章）——两端都零整报文驻留
```

## 示例

### 第一个完整程序：一行 Compose

下面的程序来自 `examples/mail/compose`——UTF-8 主题文本邮件的最短路径：

```embed path="extlibs/xmail/examples/mail/compose/main.c" title="extlibs/xmail/examples/mail/compose/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xmail/single -include xmail.h impl.c extlibs/xmail/examples/mail/compose/main.c -lws2_32 -liphlpapi
（输出完整 MIME 报文——含编码词主题与 QP 正文）
```

**刚才发生了什么。** ① `MailMessageInit`+五字段赋值（发件人/收件人/Subject `示例邮件`/正文）——**全部借用**（字符串字面量直进结构，零复制零树）。② `xrtMailCompose` 一行产完整报文：中文 Subject 自动编码词（`=?utf-8?B?...?=`）、纯文本 QP 编码、Date/Message-ID 安全随机——**手工 MIME 二十行的全部决策在这里自动完成**。③ 输出 fwrite 后 `xrtFree`——本入口是"完整报文"形态（要流式改 `ComposeWrite`+sink：附件分块进 sink）。④ 用第 116 章的 `MailMessageParse` 解回这个输出——**发与收用的是同一套解析词汇**，闭环自洽。

### 第二个完整程序：Submit 提交

第二个程序来自 `examples/smtp/submit`——最高层的发送：

```embed path="extlibs/xsmtp/examples/submit/main.c" title="extlibs/xsmtp/examples/submit/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xmail/single -include xmail.h impl.c extlibs/xsmtp/examples/submit/main.c -lws2_32 -liphlpapi
（对配置的服务器完成一次提交后正常退出）
```

**刚才发生了什么。** ① `xmailmessage` 同款描述（From/To/Subject/Text）→ `xrtSmtpSubmit(pClient, &Message, iDeadline, NULL)` 一行——内部 Compose+envelope 推导+会话命令+流式发送。② 截止与取消贯穿（第 117 章阻塞形态不变——层次上升纪律不变）。③ 对照第 117 章 client 示例（自备完整报文）：那是"我控制每一字节"、这是"我描述业务意图"——**同一条会话、两种 altitude**。附件版本把 `Attachments` 数组填上即得 multipart/mixed——组合层自动升级结构。

## 契约

- **借用描述**：message/attachment 全借用——零隐藏连接/零字典/零递归 owned 树。
- **Valid 纯校验**：不生成随机值、不分配、不调 sink——首次网络写入前的静态错误全发现。
- **两出口**：ComposeWrite 直写 sink（附件分块编码）/Compose 完整报文（xrtFree）。
- **结构自动**：三层 mixed/alternative/related 按内容组合；boundary 彼此不同由层保证。
- **确定性键**：Date/Message-ID/boundary 可注入（留空安全随机）；Message-ID 域从 From 推导。
- **字段边界**：自定义字段不可覆盖结构字段——结构所有权归组合层。
- **编码策略**：文本 QP；附件固定小块 Base64——零随附件增长的编码副本。
- **Bcc 语义**：只进提交层 envelope、不进 RFC 报文——密送协议级正确。
- **地址默认 ASCII**：SMTPUTF8 显式开；语法过≠对端收（能力判断归协议层）。
- **Submit 层**：描述→组装→envelope→会话一行；截止/取消贯穿。

## 避坑

### 坑 1：把 Bcc 收件人写进自定义字段

症状："密送"变成了明送——所有收件人在头里互相可见，隐私事故。

原因：Bcc 的语义由**提交层消费**（envelope 收件人）实现——报文头没有它是特性。把 Bcc 地址当自定义字段加=手写抄送。

```c bad
message.Bcc = &Secret;
add_custom_field(&message, "Bcc", "secret@x.com");  /* 进报文：明送 */
```

```c good
message.Bcc = &Secret;   /* 只设结构字段 */
/* Submit 推导 envelope：RCPT TO 含 secret；报文头无 Bcc——真密送 */
```

### 坑 2：附件走 Compose（非 Write）还嫌内存

症状：50 MB 附件经 `Compose` 完整报文入口——内存里同时有原文+完整报文。

原因：两个出口的语义差：`Compose` 是"给我完整报文"（必然聚合）；流式场景用 `ComposeWrite`——附件按固定小块编码直进 sink（网络流/文件），峰值=小块。

```c bad
str raw = xrtMailCompose(&Msg, &n);   /* 50MB 聚合：峰值翻倍 */
send_all(raw);
```

```c good
/* ComposeWrite + 流式 sink：分块产出直发 */
xrtMailComposeWrite(&Msg, stream_sink, pCtx, ...);  /* 小块直达网络 */
```

### 坑 3：测试断言没注入确定性键

症状：两次 Compose 输出对比失败——Date/Message-ID/boundary 每次随机。

原因：留空=安全随机（生产的正确默认）；测试要确定性就**注入**固定值——这是设计好的钥匙不是缺陷。

```c bad
raw1 = xrtMailCompose(&Msg, NULL);
raw2 = xrtMailCompose(&Msg, NULL);
assert(equal(raw1, raw2));   /* 随机 Date/ID：永不相等 */
```

```c good
Msg.Date = FixedDate; Msg.MessageId = FixedId;
provide_boundaries(&Msg, "t1", "t2", "t3");
raw1 = Compose(&Msg); raw2 = Compose(&Msg);
assert(equal(raw1, raw2));   /* 确定性：逐字节一致 */
```

## 练习

### 基础：三层结构验证

构造三种描述（纯文本+HTML+附件；+内联图）——Compose 输出用第 116 章游标逐层拆解验证结构与 boundary 互异。验收标准：结构选型与表格一致；三层嵌套顺序正确。

### 进阶：Bcc 闭环实验

带 Bcc 的消息 Submit 到本地测试服务器（或第 104 章自建）——抓线路：RCPT TO 含密送、DATA 报文头无 Bcc。验收标准：两处证据齐；收件方转发的报文里也永不出现密送地址。

### 挑战：收发全链路工具

组合：ComposeWrite 流式发带 10 MB 附件（第 117 章 DATA 流式）→ POP3 流式收（第 118 章）→ 增量解析还原（第 116 章）→ 附件落盘校验哈希。验收标准：全链内存恒定（MB 级）；附件哈希一致；编码词主题往返无损。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 定位 | 常见场景便利层：结构化描述→自动 MIME——原语全在第 116 章可达 |
| 描述借用 | message/attachment 零复制；无隐藏连接/字典/owned 树 |
| Valid | 纯静态校验——首次网络写入前错误全发现 |
| 两出口 | ComposeWrite（流式 sink、分块编码）/Compose（完整报文） |
| 结构自动 | mixed/alternative/related 三层按内容；boundary 互异层保证 |
| 确定性键 | Date/Message-ID/boundary 可注入；留空安全随机 |
| 编码 | 文本 QP；附件小块 Base64——零随大小增长的副本 |
| Bcc | envelope 有、报文无——密送的协议级正确 |
| 地址 | 默认 ASCII；SMTPUTF8 显式；语法≠可送达 |
| Submit | 一行闭环；与 CommandWrite 之间的每层可下钻 |
