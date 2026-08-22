# SSH Keyboard-interactive Auth API

`ssh_auth_keyboard` 在公共 USERAUTH 消息层上实现 RFC 4256 keyboard-interactive 方法。模块只
处理请求、挑战和响应的协议格式，不保存凭据、不执行 PAM/OTP，也不拥有认证轮次状态。

`xrtSshAuthKeyboardWrite` 按规范建议写入空 language tag；需要保留协议原始能力时可使用
`xrtSshAuthKeyboardWriteLanguage`。submethods 是允许为空的严格 SSH name-list。

`xrtSshAuthKeyboardChallengeWrite/Read/Next` 和
`xrtSshAuthKeyboardResponseWrite/Read/Next` 使用“完整预验证、借用迭代”的接口。prompt 和
response 数量由 uint32 协议字段与调用方缓冲决定，没有库内固定上限，也不分配对象数组。
prompt 必须是非空 UTF-8，response 可以是空 UTF-8；所有写入先检查完整容量和输入输出重叠，
失败不会发布部分报文。

协议允许一次认证包含多轮挑战。后续认证状态机负责保证服务端同一时刻最多存在一个未完成
challenge，并限制总尝试数、轮次数、字节数和截止时间。示例见 `examples/auth_keyboard/main.c`。
