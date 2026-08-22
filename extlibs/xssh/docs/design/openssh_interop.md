# xssh OpenSSH 互操作门禁

## 边界

`ssh_client_openssh_tests` 使用当前公开的 `xrtSshClientDial`、客户端事件、session/PTY/forward helper
和动态 channel I/O 连接真实 OpenSSH `sshd`。测试不启动服务、不探测外部目标；未设置
`XSSH_LIVE_HOST` 时只输出 `SKIP`，且 `SKIP` 不能作为互操作通过证据。

主机密钥不会被默认接受。测试必须设置固定的 OpenSSH `SHA256:` 指纹，或在隔离测试环境中显式
设置 `XSSH_LIVE_ACCEPT_NEW=1`。后者会把实际指纹输出到标准错误，便于下一轮改用固定值。

## 基础配置

| 环境变量 | 含义 |
|---|---|
| `XSSH_LIVE_HOST` | 必填；OpenSSH 主机名或地址 |
| `XSSH_LIVE_PORT` | 可选；默认 `22` |
| `XSSH_LIVE_USER` | 必填；认证用户名 |
| `XSSH_LIVE_PASSWORD` | 与 identity 二选一；密码认证 |
| `XSSH_LIVE_IDENTITY` | 与 password 二选一；未加密 OpenSSH Ed25519 私钥路径 |
| `XSSH_LIVE_HOST_FINGERPRINT` | 推荐；完整 `SHA256:` 主机密钥指纹 |
| `XSSH_LIVE_ACCEPT_NEW` | 仅隔离测试可用；显式接受并打印新指纹 |
| `XSSH_LIVE_TIMEOUT_MS` | 可选；全链路超时，默认 30000，最大 600000 |
| `XSSH_LIVE_STRESS_CHANNELS` | 可选；并发慢读 channel 数，默认 8，最大 64 |

基础场景总会执行一次 exec。`XSSH_LIVE_COMMAND` 默认为 `echo xssh-live-exec`，
`XSSH_LIVE_EXPECT` 默认为 `xssh-live-exec`。门禁要求 request success、期望输出、exit-status 0、
EOF/close 和全部网络对象清理同时成立。

## 可选场景

设置 `XSSH_LIVE_PTY_COMMAND` 后，测试继续在同一连接打开 PTY、调整窗口、请求 shell、发送命令和
`exit`。`XSSH_LIVE_PTY_EXPECT` 是输出标记，默认 `xssh-live-pty`。

设置 `XSSH_LIVE_FORWARD_HOST` 和 `XSSH_LIVE_FORWARD_PORT` 后，测试继续打开 direct-tcpip channel。
目标应是从 SSH 服务端可访问的回显服务。`XSSH_LIVE_FORWARD_SEND` 是发送内容，默认
`xssh-live-forward`；`XSSH_LIVE_FORWARD_EXPECT` 默认与发送内容相同。该场景验证 local-forward
实现所依赖的 SSH transport primitive，不在扩展内部隐藏创建本地 listener。

## 并发慢读门禁

基础 exec 和可选场景完成后，测试总会在同一连接上顺序发起 channel open，并让已经启动的远端命令
保持并发运行。每条 channel 输出超过 32 KiB，客户端使用 32 KiB 接收窗口、8 KiB 最大 packet、
16 KiB 回补阈值和 256 字节分块延迟消费，显式验证窗口耗尽后的恢复路径。

每条 channel 必须同时取得 request success、exit-status 0、超过 8 KiB 的已消费正文、唯一末尾标记
和 CLOSE；全部 channel 回收后测试才允许关闭 TCP。默认 8 路适合日常回归，发布长稳态可以通过
`XSSH_LIVE_STRESS_CHANNELS=64` 提升并发度。失败输出包含逐 channel 的回复、退出、字节、标记和
关闭证据，便于区分协议错误、窗口停滞与远端命令异常。

## 运行

```powershell
$env:XSSH_LIVE_HOST = "127.0.0.1"
$env:XSSH_LIVE_USER = "xssh-test"
$env:XSSH_LIVE_PASSWORD = "test-only-password"
$env:XSSH_LIVE_ACCEPT_NEW = "1"
python tools/build.py --manifest extlibs/xssh/config/modules.json --suite ssh_client_openssh_tests --no-single --cflag=-Werror
```

正式发布证据应分别保存 Windows 与 Linux、password 与 Ed25519 identity，以及启用 PTY、
direct-tcpip 和并发慢读的运行结果。凭据只通过运行环境提供，不写入仓库、测试日志或生成物。
