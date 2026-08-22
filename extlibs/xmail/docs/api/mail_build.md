# 流式邮件构建

`mail_build` 把已经稳定的字段、编码词、地址和 multipart 原语组合为同步 sink 写入器。
它不依赖网络，不持有正文，不隐式生成 Date、Message-ID、boundary 或传输编码，因此
文件、固定缓冲、TCP、TLS 和测试 sink 可以复用同一条路径。

`xmailwriteproc` 必须在返回前消费整个借用片段。`xmailbuilder` 从
`XMAIL_BUILDER_HEADERS` 开始；`xrtMailBuilderHeadersEnd` 写出空行并进入正文阶段，
`xrtMailBuilderFinish` 最终进入 `XMAIL_BUILDER_CLOSED`。回调失败后状态固定为
`XMAIL_BUILDER_FAILED`，避免调用方误把不完整报文继续发送。

字段阶段有三种层次：

- `xrtMailBuilderHeader` 验证并折叠普通字段。
- `xrtMailBuilderWordHeader` 为 Subject 等非结构化 UTF-8 值生成编码词。
- `xrtMailBuilderAddressHeader` 格式化 mailbox 数组。
- `xrtMailBuilderHeaderBlock` 验证后零复制提交调用方已经构建好的一个或多个字段。

正文阶段的 `xrtMailBuilderBody` 直接借用并提交字节，不复制、不解释内容。
`xrtMailBuilderMultipart` 复用 `xrtMailMultipartMarkWrite` 输出 FIRST、NEXT 或 CLOSE
分隔片段，并根据最后两个输出字节避免重复 CRLF。`xrtMailBuilderPartBegin` 写出 FIRST
或 NEXT 后重新进入字段阶段，适合逐层构建嵌套 MIME entity。Builder 不猜测 multipart
是否完整，底层用户始终保留完整协议控制权。

```c
xmailbuilder builder;

xrtMailBuilderInit(&builder, write_proc, user_data);
xrtMailBuilderHeader(
	&builder,
	XRT_STR_LITERAL("Content-Type"),
	XRT_STR_LITERAL("text/plain; charset=UTF-8"),
	0
);
xrtMailBuilderHeadersEnd(&builder);
xrtMailBuilderBody(&builder, body, body_size);
xrtMailBuilderFinish(&builder);
```
