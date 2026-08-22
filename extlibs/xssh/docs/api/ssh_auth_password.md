# SSH Password Auth API

`ssh_auth_password` 在公共 USERAUTH 消息层上实现 RFC 4252 password 方法。它不保存凭据、
不记录凭据，也不选择认证策略；输入与解析结果均为调用方拥有的借用视图。

`xrtSshAuthPasswordWrite` 构建普通密码请求；`xrtSshAuthPasswordChangeWrite` 构建包含旧密码和
新密码的请求。`xrtSshAuthPasswordRead` 严格区分两种形式并校验密码 UTF-8。

`xrtSshAuthPasswordPromptWrite/Read` 实现服务端消息号 60 的密码更改提示。所有构建器先检查
完整输出容量和输入输出重叠，失败不会发布部分报文。上层状态机必须确保 password 方法只在
已经提供机密性与完整性保护的 transport 上使用，并负责及时清理承载明文凭据的缓冲。

示例见 `examples/auth_password/main.c`。
