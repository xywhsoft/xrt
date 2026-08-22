# SSH Client Dial API

`ssh_client_dial` 是 `ssh_client` 与 XRT `net_tcp_dial` 之间的薄组合层。它复用 Resolver、DNS 缓存、
Happy Eyeballs、连接截止时间、取消、地址竞速统计和结构化错误，不创建隐藏 Engine，也不复制 TCP
连接状态机。

## 建立连接

先初始化 `xsshclient`、`xnetengine` 与 `xnetresolver`，再调用 `xrtSshClientDial`。传入的
`xnetdialconfig` 与 `xrtNetDial` 完全同义；空配置使用 XRT 默认值。

`xrtSshClientDial` 返回一个由调用方持有的 `xnetdial` 引用。取消、统计和销毁仍直接使用
`xrtNetDialCancel`、`xrtNetDialStats` 与 `xrtNetDialDestroy`，不提供重复的 SSH 包装函数。

## 两个完成点

完成回调沿用 `xnetdialproc`：它只表示 TCP 建链成功、失败、超时或取消。成功时回调接管
`xnetstream` 引用，且 SSH Stream 驱动已经在公开 `Open` 前安装。应用必须保留并最终释放这个
Stream 引用。

SSH identification、KEX、主机信任和认证全部完成后，才会发布 `xsshclientevents.Ready`。不得把
Dial 成功当作 SSH 会话可用。握手期间的协议错误和最终关闭继续通过 `xsshclientevents.Error` 与
`Close` 发布。

代理、TLS 隧道、Unix socket 或应用自建 Stream 不经过本模块；它们仍在 Stream 所属 Worker 调用
`xrtSshClientAttach`，因此 Dial helper 不会封死自定义传输路径。
