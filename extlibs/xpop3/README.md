# xpop3

`xpop3` 是基于 `xrt` 的 POP3 收信扩展库。

当前处于官方扩展骨架阶段，已落地：

- POP3 配置、结果与状态类型
- POP3/POP3S/STARTTLS 安全模式常量
- POP3 阶段常量
- `+OK` / `-ERR` 基础响应解析
- `CAPA` 能力解析
- `STAT` 响应解析
- `LIST` / `UIDL` 多行结果解析
- `AUTH PLAIN` SASL 登录模式
- POP3 多行响应 dot-stuffing 解码
- 明文 POP3 同步客户端 API：`open/stat/capa/list/uidl/retr/top/dele/noop/rset/quit`
- POP3S 995 隐式 TLS 连接路径，使用阻塞 socket + `xtlssession`
- POP3 STARTTLS：`STLS` 后复用 `xtlssession` 包装后续 socket 读写
- 本地构建测试
- 本地 mock server 自动测试，覆盖 `USER/PASS/AUTH PLAIN/CAPA/STAT/LIST/UIDL/RETR/TOP/DELE/NOOP/RSET/QUIT`
- `xrtPop3RetrMime` 将 `RETR` 原始邮件交给 `xmail_mime` 基础 parser 解析
- `xrtPop3TopRaw` / `xrtPop3TopMime` 支持邮件头部和预览正文抓取
- `xrtPop3ListSummaries` 组合 `LIST` / `UIDL` / `TOP` 返回邮件摘要列表
- future 异步 API：`xrtPop3FetchRawFuture`、`xrtPop3FetchMimeFuture`、`xrtPop3TopRawFuture`、`xrtPop3TopMimeFuture`、`xrtPop3ListSummariesFuture` 及对应 `AsyncWait` 封装

后续按 spec 继续补：

- POP3 STARTTLS 真实服务商兼容性修复

说明：当前同步客户端使用阻塞 socket 路径，POP3S 使用 `xtlssession` 隐式 TLS，STARTTLS 使用 socket + `xtlssession` 升级路径，future API 采用线程 future 包装完整 open / RETR、TOP 或摘要列表 / optional DELE / QUIT 流程。`xpop3config.iTlsMaxVersion` 可限制 TLS 最高版本，`xpop3config.sCaFile` 可显式指定 PEM CA bundle；QQ POP3S 995 在 `tls1.2` 下已完成真实 LIST/RETR/MIME 解析闭环。

## 能力矩阵

| 能力 | 状态 |
| --- | --- |
| 明文 POP3 | 已支持 |
| POP3S 995 | 已支持；QQ POP3S 在限制 TLS 1.2 后已完成真实收信闭环 |
| STARTTLS 110 | 已接入升级路径；QQ POP3 110 实测不支持 `STLS` |
| USER/PASS | 已支持 |
| AUTH PLAIN | 已支持 |
| CAPA | 已支持 |
| STAT | 已支持 |
| LIST | 已支持 |
| UIDL | 已支持 |
| RETR raw | 已支持 |
| RETR MIME 基础解析 | 已支持 |
| TOP raw / MIME | 已支持 |
| 邮件摘要列表 | 已支持 |
| dot-stuffing 解码 | 已支持 |
| DELE | 已支持 |
| NOOP | 已支持 |
| RSET | 已支持 |
| QUIT | 已支持 |
| 同步 API | 明文、POP3S、STARTTLS 路径已接入 |
| future 异步 API | Fetch raw / MIME、TOP raw / MIME 与摘要列表已支持 |
| 本地 mock server 自动测试 | 已支持 |
| 真实服务商手工验证 | QQ POP3S 995 已通过；STARTTLS 仍需其他服务商覆盖 |

## 收信语义

- `xrtPop3RetrRaw` / `xrtPop3RetrMime` 只执行 `RETR`，不会删除服务端邮件；同一封邮件通常可以再次读取。
- 只有显式调用 `xrtPop3Delete`，或 future 选项 `bDeleteAfterRetr = TRUE` 时才会发送 `DELE`。
- POP3 的 `DELE` 通常在 `QUIT` 后提交删除；如果在 `QUIT` 前 `RSET`，服务端应取消本次会话的删除标记。

## 编译

```bat
build_test.bat
```

## 最小同步示例

```c
xpop3config cfg;
xpop3result ret;
xpop3client* client = NULL;
xmailparsedmessage msg;

xrtPop3ConfigInit(&cfg);
cfg.sHost = "pop3.example.com";
cfg.iPort = 110;
cfg.iSecureMode = XPOP3_SECURE_NONE;
cfg.sUser = "user@example.com";
cfg.sPass = "app-password";

xrtPop3ResultInit(&ret);
memset(&msg, 0, sizeof(msg));
if ( xrtPop3Open(&cfg, &client, &ret) ) {
	if ( xrtPop3RetrMime(client, 1, &msg, &ret) ) {
		printf("subject=%s\n", xmailMimeParsedGetHeader(&msg, "Subject"));
		xmailMimeParsedMessageFree(&msg);
	}
	if ( xrtPop3TopMime(client, 1, 0, &msg, &ret) ) {
		printf("preview-subject=%s\n", xmailMimeParsedGetHeader(&msg, "Subject"));
		xmailMimeParsedMessageFree(&msg);
	}
	xrtPop3Quit(client, &ret);
}
```

## 最小异步示例

```c
xpop3asyncopts opts;
xpop3result ret;
xmailparsedmessage msg;

xrtPop3AsyncOptsInit(&opts);
opts.sDebugName = "pop3_fetch";

memset(&msg, 0, sizeof(msg));
if ( xrtPop3FetchMimeAsyncWait(&cfg, 1, &opts, &msg, &ret) ) {
	/* use msg */
	xmailMimeParsedMessageFree(&msg);
}
if ( xrtPop3TopMimeAsyncWait(&cfg, 1, 0, &opts, &msg, &ret) ) {
	/* use preview headers */
	xmailMimeParsedMessageFree(&msg);
}
{
	xpop3summary* items = NULL;
	size_t count = 0;
	if ( xrtPop3ListSummariesAsyncWait(&cfg, &opts, &items, &count, &ret) ) {
		/* use items[0..count) */
		xrtPop3SummariesFree(items);
	}
}
```

## 安全建议

- 不要把真实 POP3 密码或应用专用密码提交到仓库。
- POP3S/STARTTLS 默认开启证书校验；测试自签名服务时可显式关闭 `bVerifyPeer`。
- `DELE` 会删除服务端邮件；批处理前建议先用 `UIDL` 建立幂等记录。
- 收到的 MIME 附件文件名不可信，上层保存前必须规范化路径和限制大小。
