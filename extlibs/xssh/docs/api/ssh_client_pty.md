# SSH 客户端 PTY

`ssh_client_pty` 是交互终端专用可裁剪层。普通 exec、subsystem 和 forwarding 不依赖 PTY。

- 使用 `xrtSshTerminalModeWrite` 和 `xrtSshTerminalModeEnd` 在调用方缓冲中构建无固定数量上限的 mode 流。
- `xrtSshClientSessionPty` 发送终端名称、字符尺寸、像素尺寸和 mode 流，并可按 channel FIFO 等待回复。
- `xrtSshClientSessionResize` 发送不要求回复的 `window-change`。

输入视图只借用到函数返回。请求必须在客户端 Stream 所属 worker 中执行，线路写事务、TCP 背压和 reply token 提交仍由 `ssh_client` 统一处理。
