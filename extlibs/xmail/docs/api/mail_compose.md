# 高层邮件组合

`mail_compose` 是建立在 `mail_build` 之上的常见场景便利层。`xmailmessage` 和
`xmailattachment` 全部借用调用方数据，不隐藏网络连接，也不要求先构建通用字典或
递归 owned 树。`xrtMailComposeWrite` 直接写 sink，`xrtMailCompose` 返回由
`xrtFree` 释放的完整报文。

`xrtMailMessageValid` 只执行完整描述校验，不生成随机 Date、Message-ID 或 boundary，
也不分配最终输出、调用 sink。协议提交层可用它保证第一次网络写入之前发现全部静态输入
错误。

消息支持 From、Reply-To、To、Cc、Bcc、UTF-8 Subject、纯文本、HTML、自定义字段、
内联资源和普通附件。Bcc 只保留给后续 SMTP 提交层取得 envelope recipient，不进入
RFC 报文。纯文本和 HTML 使用 Quoted-Printable；附件按固定小块编码 Base64，不创建
随附件大小增长的编码副本。

正文结构按内容自动选择：

- 普通附件使用 `multipart/mixed`。
- 同时存在纯文本和 HTML 时使用 `multipart/alternative`。
- HTML 内联资源使用 `multipart/related`。
- 三层可以嵌套，且 boundary 必须彼此不同。

Date、Message-ID 和三种 boundary 可由调用方提供，从而获得完全确定的输出；留空时
分别使用当前 UTC 时间、安全随机 Message-ID 和安全随机 boundary。Message-ID 域留空
时从 From 地址推导。自定义字段不能覆盖 Compose 管理的结构字段。

```c
xmailmessage message;
xmailaddress to;

xrtMailMessageInit(&message);
message.From = (xmailaddress){
	XRT_STR_LITERAL("Sender"),
	XRT_STR_LITERAL("sender@example.com")
};
to = (xmailaddress){
	XRT_STR_LITERAL("Receiver"),
	XRT_STR_LITERAL("receiver@example.net")
};
message.To = &to;
message.ToCount = 1;
message.Subject = XRT_STR_LITERAL("Hello");
message.Text = XRT_STR_LITERAL("body");

str raw = xrtMailCompose(&message, NULL);
```
