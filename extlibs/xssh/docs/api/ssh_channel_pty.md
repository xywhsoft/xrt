# SSH Channel PTY API

`ssh_channel_pty` 独立提供 PTY request 与 RFC 4254 terminal modes，不让普通 exec、subsystem 或
转发闭包携带终端专用代码。

## Terminal Modes

`xrtSshTerminalModeWrite` 直接向调用方缓冲写入 opcode/value，`xrtSshTerminalModeEnd` 写入结束
标记。`xrtSshTerminalModesRead` 先完整验证 stream，再由 `xrtSshTerminalModesNext` 借用迭代；
数量没有固定上限。空 stream 作为“未指定模式”兼容接受，非空已知 opcode stream 必须以
`TTY_OP_END` 结束。

RFC 要求遇到 160–255 的未来 opcode 时停止解析，因此 reader 设置 `Unsupported=true`，保留已解析
的前序 mode，不误读未知格式。Writer 不生成这些 opcode，扩展实现仍可直接传入原始 mode stream。

## PTY Request

`xrtSshChannelPtyWrite/Read` 保留字符尺寸、像素尺寸、终端名称和原始 modes。Writer 直接写入最终
channel request payload，仅借用已经编码的 mode stream，不创建固定 mode 数组或临时报文。

示例见 `examples/channel_pty/main.c`。
