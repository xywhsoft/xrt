# Multipart

`mail_multipart` 是严格 CRLF 的零分配 MIME multipart 流式视图。它复用
`xrtMailBoundaryValid` 和 `mail_header`，不建立树、不复制字段、不解码 part 正文。

`xmailmultipartcursor` 保存借用的 `Source`、`Boundary`、`Preamble`、`Epilogue`，
以及推进位置、part 数量和预算。`xrtMailMultipartCursorInit` 的零预算使用
`XMAIL_MULTIPART_PARTS_DEFAULT`，`SIZE_MAX` 表示调用方明确不限制。
`xrtMailMultipartNext` 返回 `xmailmultipartview`，其中 `Source`、`Headers`、`Body`
都借用原输入。游标只接受物理行首完整匹配的 boundary，拒绝裸换行、非法 part 字段、
缺失关闭分隔线和预算溢出。

构建侧不强迫调用方创建消息对象。`xmailmultipartmark` 的
`XMAIL_MULTIPART_FIRST`、`XMAIL_MULTIPART_NEXT`、`XMAIL_MULTIPART_CLOSE` 交给
`xrtMailMultipartMarkWrite` 后，分别生成可直接发送的第一条、后续和关闭分隔片段。
调用方可以把字段和正文直接写入网络流，避免总报文拼接。

```c
xmailmultipartcursor cursor;
xmailmultipartview part;

xrtMailMultipartCursorInit(&cursor, body, boundary, 0);
while ( xrtMailMultipartNext(&cursor, &part) == XMAIL_NEXT_ITEM ) {
	consume(part.Headers, part.Body);
}
```
