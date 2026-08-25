# SSH Forward Message API

`ssh_forward_message` 实现 RFC 4254 的 `tcpip-forward`、`cancel-tcpip-forward`、动态端口成功
响应以及 `direct-tcpip`、`forwarded-tcpip` channel open。它只处理协议 payload，不创建本地
listener、不解析 DNS，也不决定允许转发的目标范围。

地址和来源字段按 SSH string 原样借用，不强迫它们先转换为 socket address。RFC 4254 在线路上
使用 `uint32` 承载端口，但本模块在语义编解码边界统一限制为 `0..65535`：非法写入不推进 writer，
非法读取返回 `XSSH_ERROR_PROTOCOL` 且不发布输出。`tcpip-forward` 的端口零仍用于请求动态分配；
目标访问控制继续由服务策略层决定。

所有 writer 直接复用 global/channel 公共前缀构建器，在最终 payload 中一次完成，不创建临时
Fields 缓冲。Reader 先由通用 envelope 解析，再严格读取类型专用字段并拒绝尾随内容。

真正的转发服务应在上层组合 XRT listener/stream、channel window、取消和背压；本模块不隐藏
线程、连接数或固定转发队列。示例见 `examples/forward_message/main.c`。
