# SSH Channel Request API

`ssh_channel_request` 在通用 `xsshchannelrequest` envelope 之上提供常用 session request 的直接
构建与严格解析。它不保存进程、终端或环境状态，也不隐藏 request reply 队列。

## Session 与环境

Shell 没有专用字段；exec command、subsystem 名称、env 名称和值按 RFC 的 SSH string 原样处理，
不擅自限定 UTF-8，因此底层调用方不会丢失二进制能力。调用方仍可直接使用
`xrtSshChannelRequestWrite` 构建未知 request。

## 通知

Xon-xoff、window-change、signal、exit-status 和 exit-signal 按协议固定 `want-reply=false`，读取
时也严格检查。Break request 保留调用方选择的回复位。Signal 名称必须是不带 `SIG` 前缀的单个
ASCII SSH name；exit-signal 的错误描述必须是 UTF-8，language tag 复用 wire 层校验。

所有便利写入直接在最终 writer 中构建公共前缀和专用字段，不创建临时报文或二次拼接。解析器只
借用通用 request 的 `Fields`，严格拒绝截断和尾随字段，失败不修改输出。

PTY 与 terminal modes 单独裁剪，避免不需要终端支持的 exec、subsystem 和转发程序携带该代码。
示例见 `examples/channel_request/main.c`。
