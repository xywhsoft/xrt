# Mail 地址

## 地址语法

`xrtMailAddressValid` 验证一个完整 addr-spec，并可返回借用的 local-part 与 domain。
默认 `XMAIL_ADDRESS_DEFAULT` 只接受 ASCII local-part 和 DNS 风格域名，也支持 quoted
local-part 与 domain-literal。只有显式设置 `XMAIL_ADDRESS_SMTPUTF8` 才接受国际化
local-part 或 UTF-8 域名；SMTP 客户端仍应根据服务器能力决定是否允许发送。

这层只负责报文语法，不把 SMTP 路径长度或 DNS 查询策略混入地址解析。域名解析、IDNA
映射和服务器声明的 envelope 限制属于更高层协议步骤。

## 零分配列表

`xrtMailAddressCursorInit` 初始化 `xmailaddresscursor`，`xrtMailAddressNext` 逐项返回
`xmailaddressview`。`xmailaddresskind` 的 `XMAIL_ADDRESS_MAILBOX`、
`XMAIL_ADDRESS_GROUP_BEGIN` 和 `XMAIL_ADDRESS_GROUP_END` 保留 RFC 5322 group 结构，
不会把组成员错误地展平成普通逗号列表。

mailbox 视图的 `Source`、`Name`、`Address`、`Local` 和 `Domain` 全部借用输入。`Name`
保留原始 quoted-string、注释或 encoded-word，调用方可按需交给邮件编码词层。游标支持
折叠空白、嵌套注释、转义引号、quoted local-part 与 domain-literal；注释深度由
`XMAIL_ADDRESS_COMMENT_DEPTH` 限制。解析失败不推进游标，也不修改输出视图。

## 常用构建

`xrtMailAddressWrite` 直接写出常见的 `display-name <addr-spec>`。安全 ASCII 显示名原样
输出，包含分隔符的显示名自动加引号并转义，非 ASCII 显示名通过
`xmailwordencoding` 选择 B 或 Q 编码。显示名为空时只输出规范 addr-spec。

`xrtMailAddress` 是由 `xrtFree` 释放的一行式入口。两个入口都先完整验证和计量，容量
不足、非法地址、控制字节或重叠失败不会发布部分 mailbox。地址错误使用
`XMAIL_ERROR_ADDRESS`，字符集错误使用 `XMAIL_ERROR_CHARSET`。

`xmailaddress` 是构建侧的借用 Name/Address 二元组。`xrtMailAddressListWrite` 对整个数组
先验证和精确计量，再以 `, ` 连接规范 mailbox；任一地址无效或容量不足都不会发布部分
列表。`xrtMailAddressList` 返回由 `xrtFree` 释放的 owned 文本。空数组是合法空列表，适合
可选 Cc/Bcc；需要至少一个收件人的协议操作应在自己的状态层检查数量。
