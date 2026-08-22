# ximap

`ximap` 是基于 `xrt` 的 IMAP 客户端扩展库。

当前已落地：

- IMAP 配置、结果与阶段类型
- IMAP/IMAPS/STARTTLS 安全模式常量
- tag 命令生成
- pipelined command begin/finish：按 tag 取回响应，并暂存乱序完成的 tagged response
- tagged 状态行解析：`OK` / `NO` / `BAD` / `BYE`
- untagged response 基础解析
- literal marker 解析：`{123}`
- literal 网络读取：按 `{n}` 长度读取并拼接 raw 响应
- 明文同步客户端：`xrtImapOpen` / `xrtImapClose`
- IMAPS 993 隐式 TLS 连接路径，复用 `xtlssession` 和现有命令/literal 解析
- IMAP STARTTLS：tagged `STARTTLS` 后复用 `xtlssession` 包装后续命令/literal 读写
- `LOGIN`
- `AUTHENTICATE XOAUTH2`
- `CAPABILITY`
- `STATUS` raw 响应与 `ximapmailboxstatus` 结构化 mailbox 状态解析
- `SELECT` / `EXAMINE` raw 响应与 `ximapmailboxstatus` 结构化 mailbox 状态解析
- `LIST` raw 响应与 `ximaplist` 结构化 mailbox 列表解析
- `SEARCH` / `UID SEARCH` raw 响应与 `ximapsearchids` 结构化 ID 列表解析
- `FETCH FLAGS` raw 响应与 `ximapflags` 结构化解析
- `FETCH BODYSTRUCTURE` raw 响应与 `ximapbodystructure` 递归 part tree 解析，含 multipart parameter/disposition/language/location、TEXT leaf line count、APPLICATION attachment leaf、通用 leaf body parameter/id/description/encoding/octet/md5/disposition/language/location，以及 `message/rfc822` envelope/raw bodystructure/lines 摘要
- `UID FETCH BODY[]` raw 响应
- `UID FETCH BODY.PEEK[]` raw 响应，支持不隐式置已读的读取路径
- `xrtImapUidFetchBodyLiteral`：从 `UID FETCH BODY[]` raw 响应中抽取纯 RFC822 literal 内容
- `xrtImapUidFetchBodySectionRaw`：抓取安全 section token，例如 `BODY[HEADER]`、`BODY[TEXT]`、`BODY[1]`、`BODY[1.MIME]`、`BODY[1.HEADER.FIELDS (...)]`
- `xrtImapUidFetchHeaderFieldsRaw` / `xrtImapUidFetchHeaderFields`：抓取 `BODY[HEADER.FIELDS (...)]` 指定 header 字段
- `xrtImapUidPeekBodyLiteral` / `xrtImapUidPeekBodySectionRaw` / `xrtImapUidPeekHeaderFields` / `xrtImapUidPeekMime`：使用 `BODY.PEEK[...]` 抓取正文、section、header 字段或 MIME 解析结果
- `xrtImapUidFetchMime`：从 `UID FETCH BODY[]` literal 中解析 MIME header/body 与递归 multipart tree
- `STORE +FLAGS/-FLAGS` 与 `UID STORE +FLAGS/-FLAGS` raw 响应和 `ximapflags` 结构化解析
- `EXPUNGE` 与 `UID EXPUNGE` raw 响应和 `ximapexpungeresult` 结构化序号列表解析
- `IDLE` 事件读取：`xrtImapIdleOnce`、`xrtImapIdleCollect`、可取消 callback 事件流 `xrtImapIdleWatch`、有限异步收集 `xrtImapIdleCollectFuture`、后台 watch future `xrtImapIdleWatchFuture` 与可重连 loop `xrtImapIdleLoop`
- `LOGOUT`
- future 异步 API：`xrtImapStatusFuture`、`xrtImapListFuture`、`xrtImapSearchFuture`、`xrtImapUidSearchFuture`、`xrtImapFetchFlagsFuture`、`xrtImapStoreFlagsFuture`、`xrtImapUidStoreFlagsFuture`、`xrtImapExpungeFuture`、`xrtImapUidExpungeFuture`、`xrtImapIdleCollectFuture`、`xrtImapIdleWatchFuture`、`xrtImapFetchBodyStructureFuture`、`xrtImapUidFetchBodyRawFuture`、`xrtImapUidFetchBodyLiteralFuture`、`xrtImapUidFetchMimeFuture`、`xrtImapUidPeekBodyRawFuture`、`xrtImapUidPeekBodyLiteralFuture`、`xrtImapUidPeekMimeFuture`、`xrtImapUidFetchBodySectionRawFuture`、`xrtImapUidPeekBodySectionRawFuture`、`xrtImapUidFetchHeaderFieldsFuture`、`xrtImapUidPeekHeaderFieldsFuture` 及对应 `AsyncWait` 封装
- 本地 mock server 构建测试
- WSL2 Dovecot 真实 IDLE 事件样本测试：`ximap_dovecot_idle_live_test.c`

后续按 spec 继续补：

- IMAP STARTTLS 真实服务商兼容性修复
- FETCH BODY[] 更多真实服务商 multipart 样本覆盖
- 更多真实服务商 BODYSTRUCTURE 样本覆盖
- 更多 BODY section 组合与真实服务商样本覆盖
- IDLE 更长时间 watch/loop 稳定性样本覆盖

说明：当前 future API 采用线程 future 包装完整 open / optional SELECT / UID FETCH 或 UID FETCH BODY.PEEK / LOGOUT 流程；IMAPS 与 STARTTLS 已接入 xrt TLS session。`ximapconfig.iTlsMaxVersion` 可限制 TLS 最高版本，`ximapconfig.sCaFile` 可显式指定 PEM CA bundle；QQ IMAPS 993 在 `tls1.2` 下已完成真实 SELECT/UID SEARCH/UID FETCH MIME 闭环，本机 WSL2 Dovecot 已完成 IMAP 143 STARTTLS 和真实 IDLE `EXISTS` 事件样本。

## 能力矩阵

| 能力 | 状态 |
| --- | --- |
| 明文 IMAP | 已支持 |
| IMAPS 993 | 已支持；QQ IMAPS 在限制 TLS 1.2 后已完成真实收信闭环 |
| STARTTLS 143 | 已接入升级路径；QQ IMAP 143 实测未完成 greeting/STARTTLS 流程；本机 WSL2 Dovecot 已完成 STARTTLS 收信闭环 |
| LOGIN | 已支持 |
| XOAUTH2 | 已支持 |
| CAPABILITY | 已支持 |
| pipelined command | `xrtImapCommandBegin` / `xrtImapCommandFinish` 已支持；乱序 tagged response 会进入 pending 缓冲并可按 tag 取回 |
| STATUS | raw 响应与结构化 mailbox 状态已支持 |
| SELECT / EXAMINE | raw 响应与结构化 mailbox 状态已支持 |
| LIST | raw 响应与结构化 mailbox 列表已支持 |
| SEARCH / UID SEARCH | raw 响应与结构化 ID 列表已支持 |
| FETCH FLAGS | raw 响应与结构化 flags 解析已支持 |
| FETCH BODY[] / BODY.PEEK[] | raw 响应、literal 读取、纯 literal 抽取、安全 section raw 抓取、HEADER.FIELDS 字段抓取、复杂 section 字段列表抓取与不置已读 PEEK 路径已支持 |
| UID FETCH MIME | 基础 header/body 与 multipart tree 解析已支持 |
| FETCH BODYSTRUCTURE | raw 响应、递归 part tree、multipart parameter/disposition/language/location、TEXT 与 APPLICATION leaf、message/rfc822 envelope/body/lines 摘要已支持 |
| STORE flags | sequence 与 UID raw 响应、结构化 flags 解析已支持 |
| EXPUNGE | sequence 与 UID raw 响应、结构化序号列表已支持 |
| IDLE | one-shot、批量同步事件读取、可取消 callback 事件流、有限异步收集、后台 watch future、可重连 loop、全局停止 callback、trace callback 与周期心跳 callback 已支持；本机 WSL2 Dovecot 已完成真实 `EXISTS` 事件样本 |
| LOGOUT | 已支持 |
| future 异步 API | STATUS、LIST、SEARCH / UID SEARCH、FETCH FLAGS、STORE / UID STORE flags、EXPUNGE / UID EXPUNGE、IDLE collect/watch、BODYSTRUCTURE、UID FETCH raw / literal / MIME、UID PEEK raw / literal / MIME、BODY section raw 与 HEADER.FIELDS raw / literal 已支持 |

## 收信语义

- `xrtImapUidFetchBodyRaw` / `xrtImapUidFetchMime` 使用 `UID FETCH BODY[]` 读取邮件内容，不会删除服务端邮件。
- IMAP 邮件打开后通常只是增加 `\Seen` 已读标记；已读邮件仍可通过 `UID SEARCH`、`UID FETCH` 或指定 UID 再次读取。
- 如需预览或同步时尽量避免隐式置已读，使用 `xrtImapUidPeekBodyRaw`、`xrtImapUidPeekHeaderFields` 或 `xrtImapUidPeekMime`。
- 只有显式 `STORE +FLAGS (\\Deleted)` 并执行 `xrtImapExpungeRaw` / `xrtImapUidExpungeRaw` 时，邮件才会按 IMAP 语义提交删除。

## 编译

```bat
build_test.bat
```

可选 WSL2 Dovecot IDLE live test：

```bash
cd /mnt/d/git/xrt
mkdir -p extlibs/ximap/bin
gcc -std=c99 -Wall -Wextra extlibs/ximap/ximap_dovecot_idle_live_test.c \
  -I extlibs/ximap -I extlibs/xmail_mime -I singlehead -pthread \
  -o extlibs/ximap/bin/ximap_dovecot_idle_live_test
XIMAP_LIVE_PASSWORD=replace-with-local-password extlibs/ximap/bin/ximap_dovecot_idle_live_test
```

## 最小同步示例

```c
ximapconfig cfg;
ximapresult ret;
ximapclient* client = NULL;
xmailparsedmessage msg;

xrtImapConfigInit(&cfg);
cfg.sHost = "imap.example.com";
cfg.iPort = 143;
cfg.iSecureMode = XIMAP_SECURE_NONE;
cfg.sUser = "user@example.com";
cfg.sPass = "app-password";

xrtImapResultInit(&ret);
memset(&msg, 0, sizeof(msg));
if ( xrtImapOpen(&cfg, &client, &ret) ) {
	if ( xrtImapSelect(client, "INBOX", NULL, &ret)
		&& xrtImapUidFetchMime(client, "101", &msg, &ret) ) {
		printf("subject=%s\n", xmailMimeParsedGetHeader(&msg, "Subject"));
		xmailMimeParsedMessageFree(&msg);
	}
	xrtImapLogout(client, &ret);
}
```

## 最小异步示例

```c
ximapasyncopts opts;
ximapresult ret;
xmailparsedmessage msg;

xrtImapAsyncOptsInit(&opts);
opts.sMailbox = "INBOX";
opts.sDebugName = "imap_fetch";

memset(&msg, 0, sizeof(msg));
{
	ximapmailboxstatus status;
	if ( xrtImapStatusAsyncWait(&cfg, "INBOX", &opts, &status, &ret) ) {
		printf("messages=%u unseen=%u\n", (unsigned)status.iExists, (unsigned)status.iUnseen);
	}
}
{
	ximaplist boxes;
	memset(&boxes, 0, sizeof(boxes));
	if ( xrtImapListAsyncWait(&cfg, "", "*", &opts, &boxes, &ret) ) {
		xrtImapListFree(&boxes);
	}
}
if ( xrtImapUidFetchMimeAsyncWait(&cfg, "101", &opts, &msg, &ret) ) {
	/* use msg */
	xmailMimeParsedMessageFree(&msg);
}
{
	ximapsearchids ids;
	memset(&ids, 0, sizeof(ids));
	if ( xrtImapUidSearchAsyncWait(&cfg, "UNSEEN", &opts, &ids, &ret) ) {
		xrtImapSearchIdsFree(&ids);
	}
}
if ( xrtImapUidPeekMimeAsyncWait(&cfg, "101", &opts, &msg, &ret) ) {
	/* use msg without implicit Seen marking */
	xmailMimeParsedMessageFree(&msg);
}
{
	const char* fields[] = { "Subject", "From", "Date" };
	str header = NULL;
	if ( xrtImapUidPeekHeaderFieldsAsyncWait(&cfg, "101", fields, 3, &opts, &header, &ret) ) {
		/* use selected headers */
		xrtFree(header);
	}
}
{
	str* events = NULL;
	size_t count = 0;
	if ( xrtImapIdleCollectAsyncWait(&cfg, 2, &opts, &events, &count, &ret) ) {
		/* use finite IDLE events */
		xrtImapIdleEventsFree(events, count);
	}
	if ( xrtImapIdleWatchAsyncWait(&cfg, 0, my_idle_callback, user_data, &opts, &count, &ret) ) {
		/* callback returned false or max events reached */
	}
	{
		ximapidleloopopts loop_opts;
		ximapidleloopsummary summary;
		xrtImapIdleLoopOptsInit(&loop_opts);
		loop_opts.sMailbox = "INBOX";
		loop_opts.iMaxEventsPerCycle = 1;
		loop_opts.pShouldStop = my_should_stop;
		loop_opts.pStopUser = stop_context;
		loop_opts.pTrace = my_idle_loop_trace;
		loop_opts.pTraceUser = trace_context;
		loop_opts.iHeartbeatEveryCycles = 1;
		loop_opts.pHeartbeat = my_idle_loop_heartbeat;
		loop_opts.pHeartbeatUser = heartbeat_context;
		if ( xrtImapIdleLoop(&cfg, &loop_opts, my_idle_callback, user_data, &summary, &ret) ) {
			/* reconnecting IDLE loop */
		}
	}
}
```

## 安全建议

- 不要把真实 IMAP 密码、应用专用密码或 OAuth2 token 提交到仓库。
- IMAPS/STARTTLS 默认开启证书校验；测试自签名服务时可显式关闭 `bVerifyPeer`。
- `STORE +FLAGS/-FLAGS` 会修改服务端邮件状态；批处理建议优先使用 `UID STORE`、限定 mailbox/UID 范围，并加入重试策略。
- `EXPUNGE` 会提交当前 mailbox 中标记为 `\\Deleted` 的邮件删除；支持 UIDPLUS 的服务建议优先使用 `UID EXPUNGE` 限定 UID 范围。
- `xrtImapIdleLoop` 提供基础重连、backoff、全局停止 callback、trace callback 与按周期触发的业务心跳 callback。
- 收到的 MIME 附件文件名不可信，上层保存前必须规范化路径和限制大小。
