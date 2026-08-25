# xmail xlang API contract

本文档定义 xrt 邮件 extlibs 接入 xlang 时的脚本层 API 合同。xlang 层采用 dict API，只做脚本友好包装，不实现 SMTP、POP3、IMAP 或 MIME 协议细节。

## 1. 总原则

- xlang 包名：`x.mail.mime`、`x.mail.smtp`、`x.mail.pop3`、`x.mail.imap`。
- API 主形态：dict / array / string / bytes / bool / number。
- JSON 只作为可选边界转换，不作为主调用形态。
- xlang 层不暴露 C 指针、socket、TLS session、future 内部句柄或结构体完整内存布局。
- 密码、OAuth2 token、AUTH payload 默认不进入返回值、trace 或错误文本。
- 除明确声明的 mutating API 外，读取邮件不隐式删除服务端邮件。

## 1.1 第一阶段边界

第一阶段 xlang 接入只封装 xrt extlibs 已实现的客户端能力。

明确不做：

- 不在 xlang 中重新实现 SMTP、POP3、IMAP、MIME、TLS、AUTH、BODYSTRUCTURE 或 multipart parser。
- 不绑定 xserver/xadmin 的配置系统、任务队列、模板系统、日志持久化或重试策略。
- 不实现 SMTP/POP3/IMAP 邮件服务端。
- 不强制支持 DKIM/DMARC/SPF 生成或校验；这些能力保留为后续安全扩展。
- 不内置 OAuth 浏览器授权流程；OAuth2 token 由调用方传入。

这些边界保证 xlang 包是轻量脚本包装层，协议行为仍由 xrt extlibs 负责。

## 2. 通用配置 dict

SMTP、POP3、IMAP 共用以下配置字段。未列出的 C 内部字段不进入 xlang API。

```text
{
  "host": string,
  "port": number,
  "secure": "auto" | "none" | "ssl" | "starttls",
  "timeout_ms": number,
  "verify_peer": bool,
  "ca_file": string,
  "tls_max_version": "auto" | "tls1.2" | "tls1.3" | number,
  "user": string,
  "password": string,
  "oauth2_token": string
}
```

字段规则：

- `password` 和 `oauth2_token` 只作为输入。
- `secure` 由字符串映射到 extlibs 的安全模式常量。
- `timeout_ms` 缺省时使用 extlibs 默认值。
- `verify_peer` 缺省为 true；测试环境可显式传 false。
- `ca_file` 可选；传入 PEM CA bundle 路径时交给 xrt TLS 作为信任源，不传时使用 xrt TLS 默认 CA 发现策略。
- `tls_max_version` 缺省为 extlibs 默认 TLS 协商；服务商需要固定版本时可传 `tls1.2`、`tls1.3` 或底层版本号。

## 3. 通用返回 dict

所有网络协议 API 返回 dict，不直接返回 C result 结构体。

```text
{
  "ok": bool,
  "error": string,
  "stage": string,
  "status": string,
  "server_code": number,
  "capabilities": [string],
  "last_reply": string,
  "used_tls": bool,
  "used_starttls": bool,
  "data": any
}
```

映射规则：

- `ok` 来自 extlibs result success。
- `stage` 使用稳定字符串，例如 `connect`、`tls`、`auth`、`select`、`fetch`、`idle`。
- `status` 使用协议状态字符串，例如 SMTP code family、IMAP `ok/no/bad/bye`、POP3 `ok/err`。
- `last_reply` 允许保留服务端最后回复摘要，但必须避免输出认证 payload。
- `data` 承载业务结果，例如 MIME message、UID 列表、mailbox 状态或发送摘要。

## 4. MIME dict

### 4.1 地址

```text
{
  "email": string,
  "name": string
}
```

### 4.2 附件和 inline 资源

```text
{
  "filename": string,
  "content_type": string,
  "content": bytes | string,
  "content_id": string,
  "inline": bool
}
```

### 4.3 邮件 message

```text
{
  "from": address,
  "to": [address],
  "cc": [address],
  "bcc": [address],
  "reply_to": [address],
  "subject": string,
  "text": string,
  "html": string,
  "headers": [{"name": string, "value": string}],
  "attachments": [attachment]
}
```

`bcc` 只参与 SMTP envelope，不写入 MIME header。

### 4.4 解析结果

```text
{
  "headers": [{"name": string, "value": string}],
  "subject": string,
  "from": [address],
  "to": [address],
  "cc": [address],
  "date": string,
  "message_id": string,
  "text": string,
  "html": string,
  "parts": [part],
  "attachments": [attachment_summary]
}
```

`part` 只暴露 MIME 语义字段：`content_type`、`content_disposition`、`filename`、`content_id`、`encoding`、`body`、`children`。不暴露 parser buffer、boundary scanner 状态或 C 指针。

## 5. SMTP dict API

### 5.1 `smtp.capability(config)`

返回服务端 EHLO capability：

```text
{
  "capabilities": [string],
  "capability_mask": number,
  "size_limit": number
}
```

该形态内部使用 `xrtSmtpCapability`，只执行连接、banner、EHLO、可选 STARTTLS 后 EHLO、QUIT，不进入 AUTH、MAIL FROM、RCPT 或 DATA。

### 5.2 `smtp.send(request)`

```text
{
  "config": config,
  "message": message,
  "dsn": {
    "ret": "FULL" | "HDRS",
    "envid": string,
    "notify": "NEVER" | "SUCCESS,FAILURE,DELAY",
    "orcpt": string
  },
  "require_smtputf8": bool,
  "max_size": number
}
```

返回：

```text
{
  "ok": bool,
  "stage": string,
  "server_code": number,
  "last_reply": string,
  "data": {
    "accepted": [string],
    "rejected": [{"email": string, "reply": string}]
  }
}
```

不暴露项：SMTP socket、TLS session、AUTH 负载、EHLO raw buffer、内部 `xsmtpmessage` 完整字段。

## 6. POP3 dict API

建议第一阶段提供无状态快捷 API 和有状态 client 包装两种形式。

### 6.1 `pop3.capability(config)`

返回服务端 POP3 capability：

```text
{
  "capabilities": [string],
  "capability_mask": number
}
```

该形态内部使用 `xrtPop3Capa`，用于脚本侧判断 `STLS`、`UIDL`、`TOP`、`USER`、`SASL` 等能力。

### 6.2 `pop3.status(config)`

返回 POP3 `STAT` 摘要：

```text
{
  "count": number,
  "messages": number,
  "size": number
}
```

该形态内部使用 `xrtPop3Stat`，只执行连接、认证、STAT、QUIT，不下载邮件内容。

### 6.3 `pop3.list(config)`

返回消息摘要：

```text
{
  "messages": [{
    "number": number,
    "uid": string,
    "size": number,
    "subject": string,
    "from": string,
    "date": string
  }]
}
```

### 6.4 `pop3.fetch(config, request)`

```text
{
  "number": number,
  "uid": string,
  "parse_mime": bool,
  "delete_after_retr": bool
}
```

返回 raw 或 MIME dict。`delete_after_retr` 缺省为 false。

### 6.5 `pop3.delete_message(config, request)`

```text
{
  "number": number
}
```

显式发送 POP3 `DELE`，成功后发送 `QUIT` 提交删除，返回：

```text
{
  "number": number,
  "deleted": bool
}
```

该 API 命名为 `delete_message`，避免使用 xlang 保留字 `delete`。

不暴露项：POP3 socket、TLS session、多行读取状态、dot-stuffing 解码临时 buffer、内部 `xpop3client`。

## 7. IMAP dict API

### 7.1 `imap.capability(request)`

```text
{
  "config": config
}
```

返回：

```text
{
  "capabilities": [string],
  "capability_mask": number
}
```

该形态内部使用 `xrtImapCapability`，用于脚本侧在调用 `idle_probe`、`idle`、`watch` 或 STARTTLS 配置前判断服务端能力。

### 7.2 `imap.status(config, request)`

```text
{
  "mailbox": "INBOX"
}
```

返回：

```text
{
  "mailbox": string,
  "messages": number,
  "exists": number,
  "recent": number,
  "unseen": number,
  "uid_validity": number,
  "uid_next": number,
  "read_only": bool,
  "read_write": bool
}
```

该形态内部使用 `xrtImapStatus`，不进入 SELECT 状态，适合同步前查询邮箱摘要。

### 7.3 `imap.list(config, request)`

```text
{
  "reference": "",
  "pattern": "*"
}
```

返回：

```text
{
  "mailboxes": [{
    "mailbox": string,
    "delimiter": string,
    "flags_raw": string,
    "no_select": bool,
    "has_no_children": bool,
    "has_children": bool,
    "marked": bool,
    "unmarked": bool
  }],
  "count": number
}
```

该形态内部使用 `xrtImapList`，用于脚本侧发现邮箱目录。

### 7.4 `imap.search(config, request)`

```text
{
  "mailbox": "INBOX",
  "uid": true,
  "criteria": "UNSEEN"
}
```

返回：

```text
{
  "ids": [number]
}
```

### 7.5 `imap.fetch(config, request)`

```text
{
  "mailbox": "INBOX",
  "uid": number,
  "peek": true,
  "section": "BODY[]",
  "headers": ["Subject", "From"],
  "parse_mime": true
}
```

当 `headers` 非空时，native JSON ABI 使用 UID `BODY.PEEK[HEADER.FIELDS (...)]` 或 `BODY[HEADER.FIELDS (...)]` 抓取指定头字段，并返回：

```text
{
  "raw": string,
  "headers_raw": string,
  "headers": [{"name": string, "value": string}],
  "requested_headers": [string],
  "headers_only": true
}
```

未指定 `headers` 时返回 raw、literal、section raw 或 MIME dict。

### 7.6 `imap.bodystructure(config, request)`

```text
{
  "mailbox": "INBOX",
  "sequence": number | string
}
```

返回：

```text
{
  "bodystructure": {
    "multipart": bool,
    "part_count": number,
    "type": string,
    "subtype": string,
    "params": [{"name": string, "value": string}],
    "disposition": string,
    "disposition_params": [{"name": string, "value": string}],
    "octets": number,
    "lines": number,
    "children": [bodystructure],
    "message_body": bodystructure | null
  },
  "mailbox": string,
  "sequence": string
}
```

该形态内部使用 `xrtImapFetchBodyStructure`，用于在不下载正文的情况下判断 multipart 结构、附件候选、编码、大小和 `message/rfc822` 嵌套结构。第一阶段保持 sequence 形态，与底层同步 API 对齐。

### 7.7 `imap.fetch_flags(config, request)`

```text
{
  "mailbox": "INBOX",
  "sequence": number | string
}
```

返回：

```text
{
  "flags": {
    "seen": bool,
    "answered": bool,
    "flagged": bool,
    "deleted": bool,
    "draft": bool,
    "recent": bool,
    "raw": string
  },
  "mailbox": string,
  "sequence": string
}
```

该形态内部使用 `xrtImapFetchFlags`。由于当前底层同步 API 的 fetch flags 入口是 sequence 形态，xlang 第一阶段不伪造 UID fetch flags；需要 UID 语义时先 `search(uid=true)` 再按同步状态选择 sequence 或使用后续底层能力扩展。

### 7.8 `imap.store_flags(config, request)`

```text
{
  "mailbox": "INBOX",
  "uid": number | string,
  "sequence": number | string,
  "operation": "+FLAGS" | "-FLAGS" | "FLAGS" | "+FLAGS.SILENT" | "-FLAGS.SILENT" | "FLAGS.SILENT",
  "flags": "(\\Seen)"
}
```

`uid` 优先于 `sequence`。返回结构包含解析后的 `flags`、`uid_mode`、`operation` 和 `requested_flags`。该形态内部按输入调用 `xrtImapUidStoreFlagsParsed` 或 `xrtImapStoreFlagsParsed`。

### 7.9 `imap.expunge(config, request)`

```text
{
  "mailbox": "INBOX",
  "uid_set": "101:*"
}
```

`uid_set` 为空时执行普通 `EXPUNGE`；非空时执行 `UID EXPUNGE uid_set`。返回：

```text
{
  "sequences": [number],
  "count": number,
  "mailbox": string,
  "uid_set": string,
  "uid_mode": bool
}
```

该形态内部使用 `xrtImapExpunge` 或 `xrtImapUidExpunge`，只返回服务端报告的 expunged message sequence numbers。删除语义仍遵循 IMAP：通常先用 `store_flags(..., flags:"(\\Deleted)")` 标记，再调用 `expunge` 真正清除。

### 7.10 `imap.idle_probe(request)`

第一阶段提供短探针形态用于验证服务端是否接受 `IDLE`，不会等待新邮件事件，也不暴露长连接内部 client：

```text
{
  "config": config,
  "request": {
    "mailbox": "INBOX"
  }
}
```

返回：

```text
{
  "idle_supported": bool,
  "mailbox": string
}
```

该形态内部使用 `xrtImapIdleProbe`，进入 `IDLE`、确认 continuation 后立即发送 `DONE` 并读取 tagged OK。

### 7.11 `imap.idle(request)`

第一阶段 xlang native JSON ABI 提供有限事件收集形态，不在脚本层暴露长连接内部 client：

```text
{
  "config": config,
  "request": {
    "mailbox": "INBOX",
    "max_events": number,
    "max_events_per_cycle": number
  }
}
```

返回：

```text
{
  "events": [string],
  "count": number,
  "mailbox": string
}
```

`max_events_per_cycle` 作为 `max_events` 的兼容别名。该形态内部使用 `xrtImapIdleCollect`，收集到指定数量事件后发送 `DONE` 并退出当前 IDLE 会话。

### 7.12 `imap.watch(request)`

第一阶段 watch 形态同样不暴露长连接内部 client，也不直接跨 native JSON ABI 传脚本 callback。它内部使用 `xrtImapIdleLoop`，按配置完成有限 watch loop，并返回事件与 summary：

```text
{
  "config": config,
  "request": {
    "mailbox": "INBOX",
    "max_events": number,
    "max_events_per_cycle": number,
    "max_cycles": number,
    "reconnect_delay_ms": number,
    "max_reconnect_delay_ms": number,
    "heartbeat_every_cycles": number
  }
}
```

返回：

```text
{
  "events": [string],
  "count": number,
  "cycles": number,
  "heartbeats": number,
  "cancelled": bool,
  "stopped": bool,
  "heartbeat_stopped": bool,
  "last_cycle_failed": bool,
  "mailbox": string
}
```

该形态适合 xlang 先用轮询/批量事件方式接入 IMAP IDLE。第一阶段按已确认的 dict API 主形态返回批量事件，不跨 native JSON ABI 传脚本 callback。后续如果 xlang native ABI 支持稳定 callback，可在不改变 `watch` dict 主形态的前提下扩展：

- `on_event(event_dict) -> bool`
- `should_stop() -> bool`
- `on_trace(trace_dict)`
- `on_heartbeat(summary_dict) -> bool`

不暴露项：IMAP tag counter、literal parser internal buffer、TLS session、socket、raw `ximapclient` 指针、future result ownership。

## 8. 禁止直接暴露的 C 内部字段

xlang 层不得直接暴露以下内容：

- 任意 `xsocket`、`xtlssession*`、`xfuture*`、`xthread`。
- `ximapclient*`、`xpop3client*`、SMTP 内部连接对象。
- parser 临时 buffer、boundary scanner 游标、literal 读取偏移。
- AUTH PLAIN/LOGIN/XOAUTH2 原始 payload。
- 结构体中的保留字段、生命周期归属字段、allocator 相关字段。
- 可导致误用的协议内部状态，例如 IMAP tag counter、POP3 multi-line state、SMTP command staging buffer。

## 9. 字段演进规则

- 新增字段必须向后兼容。
- 删除或重命名字段需要先保留 alias 一个发布周期。
- 协议能力用字符串数组表达，不直接暴露 bit mask。
- 错误阶段用稳定字符串表达，不直接暴露 C enum 数值。
- 原始协议回复只作为调试摘要，不能作为业务解析接口。
