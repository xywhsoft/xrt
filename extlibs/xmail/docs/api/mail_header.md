# Mail 字段

`xrtMailHeaderCursorInit` 和 `xrtMailHeaderNext` 是零分配字段解析路径。游标接受包含或
省略末尾空行的明确长度字段块，在空行处停止，不读取正文。名称和值借用原报文；值
保留原始 `CRLF + WSP` 折叠，调用方可按需交给 `xrtMailHeaderUnfoldWrite`。

解析器严格要求 CRLF，拒绝裸 CR/LF、首行 continuation、非法字段名、控制字节以及
超过 998 字节的物理行。重复字段逐项返回，不进行错误的逗号合并；`Received`、
`Set-Cookie` 类似语义可由上层正确处理。

`xrtMailHeaderWrite` 写出 `Name: Value\r\n`。它会先展开输入中的合法折叠，再压缩普通
空白，并尽量在配置软宽度前的单词边界折叠；任何物理行仍受 998 字节硬限制。空值
写成 `Name:\r\n`。容量不足、非法字段或输入输出重叠均不会发布半个字段。
