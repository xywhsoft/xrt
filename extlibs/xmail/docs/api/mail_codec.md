# Mail 编码

## Quoted-Printable

`xrtMailQpWrite` 和 `xrtMailQp` 处理任意长度字节。`XMAIL_QP_BINARY` 会把 CR/LF 当普通
二进制字节编码，`XMAIL_QP_TEXT` 会把 LF、CRLF 和独立 CR 统一成 CRLF。行宽为零时
使用 RFC 2045 的 76 字节默认值；可选行宽范围是 4 到 76。

编码器在每次软换行前预留 `=`，不会生成超过配置上限的物理行；行尾空格和制表符
始终编码。`xrtMailQpDecodeWrite` 严格拒绝残缺或非十六进制转义，并支持输入输出同址
收缩。只有显式设置 `XMAIL_QP_RELAXED_SOFT_BREAK` 才接受 `=\n`。

## Base64

`xrtMailBase64Write` 直接按最多 57 个输入字节一组调用 XRT Base64 核心，不建立完整
临时编码副本。非空结果的每一行都以 CRLF 结束。自定义行宽必须是 4 到 76 之间四的
倍数。

`xrtMailBase64DecodeWrite` 复用 XRT 严格解码器，只额外允许 MIME 空白。它拒绝非规范
填充和非零尾位，并支持输入输出同址。`xrtMailBase64` 与 `xrtMailBase64Decode` 是由
`xrtFree` 释放的一行式入口。
