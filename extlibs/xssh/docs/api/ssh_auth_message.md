# SSH Auth Message API

`ssh_auth_message` 实现 RFC 4252 公共 USERAUTH 消息，不创建网络连接、不保存密码，也不绑定
客户端或服务端状态机。它依赖 `ssh_wire` 与 XRT `unicode`，所有视图均借用完整 payload。

## 通用请求

`xrtSshAuthRequestSize`、`xrtSshAuthRequestWrite` 和 `xrtSshAuthRequestRead` 处理用户名、服务名、
方法名和原始方法字段。
未知认证方法不会被拒绝，调用方可以在不修改 xssh 的情况下实现扩展方法。用户名必须是规范
UTF-8；服务和方法必须是单个 SSH 名称。`xrtSshAuthNoneWrite/Read` 是标准
`ssh-connection` none 探测的直接路径。

## 公共响应

`xrtSshAuthFailureWrite/Read` 保留 name-list 和 partial-success；
`xrtSshAuthSuccessWrite/Read` 严格拒绝尾随字段；`xrtSshAuthBannerWrite/Read` 校验消息 UTF-8
与 ASCII language tag；language tag 统一复用 wire 层的 `xrtSshLanguageValid`。所有写入
先预留整条消息并拒绝输入输出重叠，失败不改变 writer。

示例见 `examples/auth_message/main.c`。
