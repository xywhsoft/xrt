# Mail 核心

`XMAIL_MODULE_MAIL_CORE` 提供邮件扩展的稳定错误域、默认线路限制和 CRLF 规范化。
`xrtMailCrlfWrite` 先验证参数并计算完整结果，容量不足或输入输出重叠时不修改输出。
查询模式传入空输出、零容量和有效的长度指针；实际文本缓冲容量必须大于返回长度。

`xrtMailCrlf` 是常见路径的分配式入口，返回值由 `xrtFree` 释放。两个入口都使用明确
长度，可以保留正文中的零字节。

`XMAIL_BOUNDARY_MAX` 定义标准 70 字节上限，`xrtMailBoundaryValid` 是不依赖随机数、
字符串容器或 multipart 解析器的纯语法谓词。随机生成入口位于独立 `mail_id` 模块。

邮件扩展错误域为 `xrt.mail`。配置、线路、字段、编码、字符集、地址、MIME、协议和
资源限制各自具有稳定 `XMAIL_ERROR_*` 代码。底层 XRT Base64 或网络错误会原样传播，
便于调用方读取最具体的失败原因。
