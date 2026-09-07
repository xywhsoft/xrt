---
num: 119
slug: xmail-pop3
title: POP3：收取邮件
volume: 卷十一 其他扩展库
type: practice
lead: +OK/-ERR 的简单世界、STAT/LIST/UIDL 的邮箱事实、流式 RETR 与 dot 去转义、SASL 认证与 STLS——下载式收件的标准协议。
api: xmail-pop3, xmail-pop3_client, xmail-mail
---

## 导读

POP3 是收件三协议（POP3/IMAP/第 118 章）里最简单的：**下载式**模型——连上、认证、列邮件、逐封取、（可选）删、告别。简单不等于没讲究：**多行响应的 dot transparency**（RETR/TOP 的正文行首点转义与单点终止——与 SMTP DATA 对称的反向操作）；**流式收取**（`Begin/Next` 逐行产出、**不分配整封邮件**——每行借用内部缓冲、只稳定到下一次读取：直写文件/增量 MIME/自有存储的三条出路）；**UIDL 的身份语义**（唯一 ID 支撑"只取新邮件"的客户端逻辑）；**SASL 认证族**（USER/PASS 明文的默认拒绝与 SASL PLAIN 加 OAuth 机制）；**STLS 升级**（CAPA 声明→+OK→原位升级→**重新 CAPA**）。xmail 的分层照旧：协议原语（离线）+客户端（同步状态机：AUTHORIZATION→TRANSACTION→MULTILINE→UPDATE）+可选 message 层（有界聚合）。

## 引入

POP3 与 IMAP 的模型差异决定选型：POP3 假设"取走即本地"——服务器是暂存投递点，客户端是权威存储；IMAP（第 118 章）假设"服务器是权威"——邮件留服务器、文件夹/标记/搜索都在服务端。POP3 的简单带来低资源占用与离线友好；IMAP 的丰富带来多设备同步。今天多数场景是"收件即转存"（拉到本地数据库）——POP3 的流式收取正合适。

`Begin/Next` 的**逐行借用**值得注意：RETR 一封 20 MB 的邮件，任意时刻内存里只有当前行——`Next` 产出的行视图**只稳定到下一次线路读取**（下一行到来即覆盖）。这比"整封缓冲"激进得多——它把"收邮件"变成了"读文件"的形态（第 40 章 IO 流的同构）。

## 概念

### 协议原语：响应、事实与命令

- **响应**：`xrtPop3ReplyParse` 区分 `+OK`/`-ERR` 并借状态文本；`StatParse`（**64 位**数量与字节——大邮箱不溢出）、`ListParse`/`UidlParse`（多行项；**UIDL 不复制不截断**）。
- **dot 家族复用**：多行终止与去点转义直接用 `mail_net` 的 `MailLineRead`/`MailDotLine`/`MailDotDecodeWrite`——**不维护 POP3 专用副本**（与 SMTP DATA 同一族原语的两个方向）。
- **能力**：`CapabilityParse` 未知扩展也返名称与参数；`Capability` 只为常用标准给稳定标记。
- **命令**：`CommandWrite` 通用兜底（容量含结尾零；拒控制字符/CR/LF 注入/512B——与 SMTP 命令层同款纪律）。

### 客户端：状态机与分层入口

```diagram state
AUTHORIZATION -> TRANSACTION: 认证成功（USER/PASS 或 SASL）
TRANSACTION -> MULTILINE: 多行命令（RETR/TOP/LIST/UIDL 全量）
MULTILINE -> TRANSACTION: 单点终止行（Next 自动恢复）
TRANSACTION -> UPDATE: Quit（提交 DELE 标记）
任意 -> FAILED: 线路错误（不可复用）
```

**分层入口四级**：`Send/Line`（最低层线路——SASL continuation 与未知扩展的口）→ `Receive`（+OK/-ERR 解析）→ `Command`（构建+状态响应）→ `Begin/Next`（多行逐行）。**标准命令全建在公开入口上**——STAT/LIST×2/UIDL×2/RETR/TOP/DELE/RSET/NOOP/QUIT 是组装不是私路：自定义扩展与官方命令同权。

### 流式收取与可选聚合

RETR/TOP 的 `Begin/Next`：每行去 dot transparency、**不分配整封邮件**、行视图借内部接收缓冲（稳定到下次读取）。三条出路：直写文件（第 44 章）、增量 MIME 解析（第 115 章模块逐步喂）、自有消息存储。可选 `pop3_message` 层：同一状态机上加**有界** `RetrWrite/TopWrite`（上限内聚合为 owned 字节）与 MIME 树入口——只要逐行路径就不带聚合闭包（裁剪自由）。

### 认证：明文默认拒绝与 SASL 族

`pop3_auth`：传统 USER/PASS + RFC 5034 SASL（PLAIN 与两种 OAuth 机制）。**凭据纪律**：配置只借用凭据；Base64 与明文临时缓冲**调用结束前清零**（第 76 章密码工程的协议层兑现）。**默认拒绝明文传输发凭据**——`AllowPlaintext` 只用于明确控制的兼容环境（与第 105 章 Basic 的 TLS 底线同款立场）。服务器拒凭据停留 AUTHORIZATION（可重试）；线路错误进 FAILED。

### TLS：隐式与 STLS

基础客户端 110 明文；`pop3_client_tls` 两形态：隐式 TLS（连接即握手）；STLS（CAPA 明确声明→发 STLS→**+OK 完整消费后**才握手→**握手后重新 CAPA**——升级前能力不泄漏到安全会话，与 SMTP STARTTLS 的重 EHLO 同理）。必须提供 Verifier。

## 示例

### 第一个完整程序：协议原语

下面的程序来自 `examples/pop3/protocol`——离线的响应与命令闭环：

```embed path="extlibs/xmail/examples/pop3/protocol/main.c" title="extlibs/xmail/examples/pop3/protocol/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xmail/single -include xmail.h impl.c extlibs/xmail/examples/pop3/protocol/main.c -lws2_32 -liphlpapi
（输出 POP3 响应解析与命令构建的自检结果）
```

**刚才发生了什么。** ① 响应/STAT/UIDL/命令四族原语在无网络环境完成解析与构建验证——与 SMTP/SSH 协议层同款"离线可测"纪律。② POP3 的响应比 SMTP 更简单（`+OK`/`-ERR` 两态——没有多行状态码的复杂度），复杂度全在**多行数据**的 dot 家族——那是 `mail_net` 共享原语，本示例之外由测试矩阵覆盖。③ 自定义状态机作者（写代理/测试服务器的人）直接消费这些原语——与官方客户端同一实现（无第二实现的家族传统）。

### 第二个完整程序：收取会话

第二个程序来自 `examples/pop3/client`——同步客户端的完整流程：

```embed path="extlibs/xmail/examples/pop3/client/main.c" title="extlibs/xmail/examples/pop3/client/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xmail/single -include xmail.h impl.c extlibs/xmail/examples/pop3/client/main.c -lws2_32 -liphlpapi
（对配置的 POP3 服务器完成收取会话后正常退出）
```

**刚才发生了什么。** ① Open（greeting 验证+CAPA 快照）→认证→STAT（邮箱事实：N 封 M 字节）→标准命令（示例按场景取 RETR/TOP/UIDL 组合）。② **收取的流式形态**：`Begin/Next` 循环逐行——每行立刻消费（打印/写盘/喂解析器），内存与邮件大小无关；`message` 示例对照有界聚合入口（`RetrWrite` 的上限形态）。③ 收尾三选（Quit 提交 UPDATE/Close/Abort）——DELE 的删除语义只在 Quit 的 UPDATE 阶段生效（中途 Abort = 放弃删除标记——"没告别就没删"的协议安全网）。

## 契约

- **协议/客户端分层**：原语不依赖网络；客户端是状态机组装；标准命令建在公开入口上。
- **64 位事实**：STAT 数量与字节 64 位；UIDL 不复制不截断。
- **dot 家族共享**：多行终止/去转义复用 mail_net 原语——无 POP3 专用副本。
- **注入纪律**：CommandWrite 拒控制/CR/LF/512B——与 SMTP 同款。
- **流式收取**：RETR/TOP 逐行借用（稳定到下次读取）；不分配整封邮件；三条出路（文件/增量 MIME/自有存储）。
- **可选聚合**：message 层有界 RetrWrite/TopWrite——逐线路径不携带聚合闭包。
- **凭据清零**：借用配置；临时缓冲调用结束前清零；默认拒绝明文发凭据。
- **认证语义**：拒凭据停留 AUTHORIZATION；线路错误 FAILED 不可复用。
- **TLS**：隐式/STLS；+OK 完整消费后才握手；升级后重新 CAPA；验证器必须。
- **UPDATE 语义**：DELE 标记在 Quit 的 UPDATE 生效——Abort 放弃删除。
- **所有权**：共享对象借不销毁；Worker 回调禁调；单 Client 无并发。

## 避坑

### 坑 1：把行视图存起来继续收

症状：收完后处理缓存行——内容已是下一行的字节。

原因：行的稳定性边界是"下一次线路读取"——POP3 客户端的最激进借用。要跨行保存必须复制。

```c bad
while ( Next(&Line) ) {
	rows[n++] = Line;   /* 存视图：下一次 Next 全部失效 */
}
process(rows);   /* 全是最后一行 */
```

```c good
while ( xrtPop3ClientNext(pClient, &Line) == OK ) {
	process_now(Line);          /* 即时消费 */
	/* 或复制：append_own_store(Line); */
}
```

### 坑 2：明文连接上发 USER/PASS

症状：`AllowPlaintext` 一开了之——内网抓包全是凭据。

原因：默认拒绝是保护不是障碍；兼容老服务器的正确路径是 STLS（CAPA 声明就升级），不是放弃加密。

```c bad
Config.AllowPlaintext = true;   /* 图省事：明文凭据 */
Auth(pClient, User, Pass);
```

```c good
if ( capa_has_stls() ) {
	Stls(pClient, ...);   /* 先升级 */
}
Auth(pClient, User, Pass);   /* 加密隧道内 */
```

### 坑 3：以为 DELE 立即删除

症状：DELE 后服务器崩了/客户端 Abort——重启后邮件还在，用户以为"删了又回来"。

原因：POP3 的删除是**两阶段**：DELE 只打标记、Quit 的 UPDATE 才提交——协议对"误删"的防御。要"必然删除"就完整 Quit；要"也许删除"（试探性处理）就 Abort 回滚。

```c bad
Dele(pClient, 1);
/* 崩溃/Abort：邮件未删——以为是 bug */
```

```c good
Dele(pClient, 1);          /* 打标记 */
...
xrtPop3ClientQuit(...);    /* UPDATE 提交——真删 */
/* 处理失败时 Abort：标记回滚——安全网 */
```

## 练习

### 基础：响应与事实解析

构造 `+OK`/`-ERR`/STAT/List/Uidl 多行样本走原语解析。验收标准：64 位数量正确；UIDL 全长保留；多行终止识别正确。

### 进阶：只取新邮件

UIDL 列表对照本地已见集合（第 18 章 Map）→ 只 RETR 新 UID → 处理后记录 → 可选 DELE+Quit。验收标准：重复运行零重复收取；UIDL 稳定性（同邮件同 ID）；中断后重跑不丢不重。

### 挑战：邮件转存管道

POP3 流式收取→逐行喂第 115 章消息视图/MIME 增量解析→提取头与正文→写入本地 mbox/JSONL 存储。20 MB 大邮件。验收标准：全程内存恒定（MB 级）；转存后经第 115 章解析还原完整；断点续收（UIDL 对账）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 模型 | 下载式：取走即本地；UIDL 是"只取新"的身份 |
| 状态机 | AUTHORIZATION→TRANSACTION→MULTILINE→UPDATE（Quit 提交） |
| 响应 | +OK/-ERR 两态；STAT 64 位；UIDL 不截断 |
| dot 家族 | 与 SMTP DATA 对称；复用 mail_net 原语无专用副本 |
| 四级入口 | Send/Line→Receive→Command→Begin/Next——标准命令同权 |
| 流式收取 | 逐行借用稳定到下次读取；零整封分配；文件/增量/存储三出路 |
| 可选聚合 | message 层有界 RetrWrite——逐线路径零闭包 |
| 凭据 | 默认拒明文；临时缓冲调用内清零；SASL 三机制 |
| TLS | 隐式/STLS；+OK 消费后才握手；重 CAPA；验证器必须 |
| 删除语义 | DELE 打标记、Quit UPDATE 提交——Abort 回滚 |
