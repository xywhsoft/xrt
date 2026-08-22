# SSH Forward Message API

`ssh_forward_message` 实现 RFC 4254 的 `tcpip-forward`、`cancel-tcpip-forward`、动态端口成功
响应以及 `direct-tcpip`、`forwarded-tcpip` channel open。它只处理协议 payload，不创建本地
listener、不解析 DNS，也不决定允许转发的目标范围。

地址和来源字段按 SSH string 原样借用，不强迫它们先转换为 socket address；端口保持线路上的
完整 `uint32`，操作系统端口范围和访问控制由服务策略层检查。这样协议日志、扩展地址格式和拒绝
响应不会丢失原始值。

所有 writer 直接复用 global/channel 公共前缀构建器，在最终 payload 中一次完成，不创建临时
Fields 缓冲。Reader 先由通用 envelope 解析，再严格读取类型专用字段并拒绝尾随内容。

真正的转发服务应在上层组合 XRT listener/stream、channel window、取消和背压；本模块不隐藏
线程、连接数或固定转发队列。示例见 `examples/forward_message/main.c`。
