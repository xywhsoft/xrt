# MIME 树

`mail_tree` 是建立在 `mail_message`、`mail_multipart` 和 `mail_param` 公开原语上的可选拥有型
解析层。低层模块继续提供零分配借用视图；只有需要在输入缓冲释放后保留完整 MIME 结构时，
才需要启用本模块。

`xrtMailTreeParse` 复制原始消息并递归建立 `xmailpart` 树。每个节点公开原始 `Message`、
解析后的 `ContentType` 和 `Disposition`、UTF-8 解码文件名、去掉尖括号的 `ContentId`、multipart
的 `Preamble`/`Epilogue`、传输编码和解码后的 `Data`。multipart 子项连续存放在 `Children`
中；`message/rfc822` 使用一个 `Embedded` 子节点表达，不另造一套消息模型。所有视图和节点
由同一个 `xmailtree` 持有，统一调用 `xrtMailTreeFree` 释放。

默认严格拒绝未知 Content-Transfer-Encoding。启用
`XMAIL_TREE_ALLOW_UNKNOWN_TRANSFER` 后，节点保留原始正文、`Transfer` 为
`XMAIL_TRANSFER_UNKNOWN`，且 `Decoded` 为 false。`XMAIL_TREE_RELAXED_QP` 仅放宽历史邮件中
Quoted-Printable 软换行的兼容处理，不放宽字段和 multipart 结构检查。

MIME entity 中的 `Content-Type`、`Content-Transfer-Encoding`、`Content-ID` 等
singleton 字段重复时，`xrtMailTreeParse` 会拒绝整棵树。这是有意的严格契约：RFC
只允许这些字段至多出现一次，也没有为重复值规定稳定的合并或优先级语义。需要检查或
容忍畸形邮件的调用方，应先使用 `mail_message` 的低层 Header API 按索引枚举全部重复值，
再自行决定取舍；`mail_tree` 不静默丢弃输入。

RFC 2231 文件名会按声明的 UTF-8、ASCII、Latin-1 或 Windows-1252 转换，
`FileNameUtf8` 为 true。未知字符集默认失败；显式启用
`XMAIL_TREE_ALLOW_UNKNOWN_CHARSET` 后保留原始解码字节并把 `FileNameUtf8` 设为 false，
调用方可以接入自己的大型字符集转换器。无论采用哪条路径，NUL 和其他控制字节都不会进入
文件名视图。

`xmailtreelimits` 控制整树最大深度、总 part 数、输入字节和传输解码分配字节，并复用
`mail_message` 的每实体字段字节与字段数量限制。传入 NULL 或把字段设为零都会使用默认值；
`SIZE_MAX` 可用于明确取消除递归深度外的对应预算。深度始终不得超过
`XMAIL_TREE_DEPTH_MAX`，避免 C 调用栈被不可信嵌套耗尽。

`xrtMailTreeLimitsInit` 建立默认预算，`xrtMailTreeLimitsValid` 可以在读取文件或发送网络命令
之前独立验证调用方配置。`xrtMailTreeParse` 与外部桥接层复用同一规范化和校验实现。

```c
xmailtree tree;

if ( xrtMailTreeParse(source, NULL, &tree) ) {
	const xmailpart* root = tree.Root;

	/* root->Data 是叶子解码正文，root->Children 是嵌套 part。 */
	xrtMailTreeFree(&tree);
}
```
