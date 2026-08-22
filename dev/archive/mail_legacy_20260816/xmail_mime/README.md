# xmail_mime

`xmail_mime` 是 xrt 邮件协议扩展族的共享 MIME 层。

当前阶段先实现纯本地、无网络依赖的基础能力：

- CRLF 规范化
- header 注入检测
- UTF-8 文本校验，复用 `xrtIsUTF8`
- RFC 2047 encoded-word 编码
- RFC 2047 encoded-word 解码
- quoted-printable 编码
- quoted-printable 解码
- base64 76 字符换行包装
- header folding / unfolding
- header 行解析与自定义 header 构建优先复用 `xrtHttpHeader*`
- RFC 2822 Date 生成
- Message-ID 生成
- 邮箱地址和地址列表格式化
- 邮箱地址和地址列表解析
- Reply-To
- 自定义 header
- Bcc envelope 字段不写入 MIME header
- `text/plain` / `text/html` MIME part 构建
- `multipart/alternative` 构建
- `multipart/mixed` 构建
- `multipart/related` 构建
- 附件 MIME part 构建
- inline 资源 `Content-ID` 构建
- UTF-8 附件文件名 `filename*` 输出
- 通用 `xmailmimepart` 递归 part tree 构建：`xmailMimeBuildPart`
- raw message 基础解析：header/body 拆分、folded header 展开、header 查询
- 同名 header 多值查询：count / by-index / join
- `xmailmimepart` 解析树
- multipart part tree 递归解析
- 叶子 part `Content-Transfer-Encoding` 解码：quoted-printable / base64
- part 结构化字段解析：`Content-Disposition`、`filename/filename*`、`Content-ID`
- `message/rfc822` part 递归解析
- `Content-Type` 解析优先复用 `xrtHttpMediaType*`
- `Content-Disposition` 文件名解析优先复用 `xrtHttpContentDisposition*`
- multipart 边界扫描优先复用 `xrtMultipartNextN`，邮件专用场景自动回退

## 编译测试

```bat
build_test.bat
```

## 设计定位

`xmail_mime` 不负责 SMTP、POP3、IMAP 网络协议。它只负责邮件内容的构建、解析和编码处理。

后续 `xsmtp`、`xpop3`、`ximap` 都应复用这一层。

## 最小构建示例

```c
xmailmessage msg;
xmailaddr to;
str raw;

memset(&msg, 0, sizeof(msg));
memset(&to, 0, sizeof(to));

msg.tFrom.sEmail = "sender@example.com";
msg.tFrom.sName = "Sender";
to.sEmail = "receiver@example.com";
to.sName = "Receiver";
msg.arrTo = &to;
msg.iToCount = 1;
msg.sSubject = "Hello";
msg.sTextBody = "Plain text body";
msg.sHtmlBody = "<p>HTML body</p>";

raw = xmailMimeBuildMessage(&msg);
if ( raw != NULL ) {
	/* send or inspect raw */
	xmailMimeFree(raw);
}
```

## 最小解析示例

```c
xmailparsedmessage parsed;

memset(&parsed, 0, sizeof(parsed));
if ( xmailMimeParseMessage("Subject: hi\r\n\r\nbody\r\n", &parsed) ) {
	printf("subject=%s\n", xmailMimeParsedGetHeader(&parsed, "Subject"));
	xmailMimeParsedMessageFree(&parsed);
}
```

## 安全建议

- 构建 header 前会做 CR/LF 注入检查；调用方仍应避免把未清洗的用户输入直接作为自定义 header 名。
- 主题、地址显示名、文本正文、自定义 header 值和附件文件名会按 UTF-8 文本校验；非法 UTF-8 会拒绝构建。
- 解析 MIME 时不要默认信任 `filename` 或 `filename*`；保存附件前应去除路径分隔符、控制字符和保留文件名。
- `Content-Type` 和 `Content-Disposition` 只表示发送方声明，上层需要按业务策略限制可接受的类型和大小。
- `message/rfc822` 可递归包含邮件内容，上层处理时应设置最大深度、最大大小或超时策略。
- 这个库不做病毒扫描、DKIM/DMARC/SPF 校验或远程内容下载。

## 能力矩阵

| 能力 | 状态 |
| --- | --- |
| CRLF 规范化 | 已支持 |
| header 注入防护 | 已支持 |
| UTF-8 文本校验 | 复用 xrtIsUTF8 |
| RFC 2047 encoded-word 编码 | 已支持 |
| RFC 2047 encoded-word 解码 | 已支持 |
| quoted-printable 编码 | 已支持 |
| quoted-printable 解码 | 已支持 |
| base64 76 字符换行 | 已支持 |
| header folding / unfolding | 已支持 |
| xrt HTTP header 复用 | 已支持 |
| Date / Message-ID 生成 | 已支持 |
| 地址格式化 | 已支持 |
| 地址解析 | 已支持 |
| 同名 header 保留 | 已支持 |
| 同名 header 多值查询 API | 已支持 |
| text/plain | 已支持 |
| text/html | 已支持 |
| multipart/alternative | 已支持 |
| multipart/mixed | 已支持 |
| multipart/related | 已支持 |
| attachment | 已支持 |
| inline Content-ID | 已支持 |
| UTF-8 filename* | 已支持 |
| 通用递归 part tree 构建 | 已支持 |
| raw message header/body 解析 | 已支持 |
| 递归 multipart 解析 | 基础 part tree 已支持 |
| part transfer 解码 | quoted-printable / base64 已支持 |
| part 附件元数据解析 | Content-Disposition / filename / filename* / Content-ID 已支持 |
| xrt HTTP media type 复用 | 已支持 |
| xrt Content-Disposition 复用 | 已支持 |
| 通用 MIME part tree API | 构建与解析均已支持 |
| message/rfc822 | 已支持基础递归解析 |
