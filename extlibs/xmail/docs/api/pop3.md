# POP3 协议 API

`pop3` 公开不依赖网络的状态、列表、能力和命令原语。官方客户端与自定义状态机共享
这些入口，原始 `RETR`/`TOP` 数据仍可直接交给 `mail_message` 或调用方自己的解析器。

## 响应

`xrtPop3ReplyParse` 区分 `+OK` 和 `-ERR`，并保留借用状态文本。`xrtPop3StatParse` 使用
64 位数量与字节数；`xrtPop3ListParse` 和 `xrtPop3UidlParse` 处理多行响应项，UIDL
不会被复制或截断。

多行终止和去点转义直接使用 `xrtMailLineRead`、`xrtMailDotLine` 或
`xrtMailDotDecodeWrite`，不再维护 POP3 专用副本。

## 能力与命令

`xrtPop3CapabilityParse` 对未知扩展同样返回名称和参数；`xrtPop3Capability` 只为常用
标准能力返回稳定标记。`xrtPop3CommandWrite` 是安全的通用兜底入口，输出容量包含末尾
零字节并拒绝控制字符、CR/LF 注入和超过 512 字节的命令。

## 示例

见 `examples/pop3/protocol/main.c`。
