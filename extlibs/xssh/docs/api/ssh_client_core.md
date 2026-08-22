# SSH 客户端动作核心

`ssh_client_core` 把客户端 identification、KEX、主机密钥信任和 USERAUTH 组合成唯一动作驱动。它不创建 Engine、TCP Stream、future、协程或 channel 表；空核心不分配输出，网络适配器只需提交 `Next` 返回的线路输出，并在读取事务提交前调用 `xrtSshClientCoreObserve`。

## 所有权

- `xsshclientcore` 拥有有界动态输出和最近一次认证方法副本。
- `Version`、`User`、回调上下文和凭据均由调用方持有，必须存活到 `xrtSshClientCoreClear`。
- `xsshclientnext.Data` 借用核心输出；再次调用 `xrtSshClientCoreNext` 前必须完成 `xrtSshSessionTcpWritePrepare`。
- Clear 会安全清零可能包含口令的整个输出容量。

## 主机信任

默认配置没有主机验证器，因此不会静默信任主机。验证器只能返回：

- `XSSH_CLIENT_HOST_ACCEPT`：接受当前已经完成密码学验签的 key blob。
- `XSSH_CLIENT_HOST_REJECT`：终止当前 KEX。
- `XSSH_CLIENT_HOST_DEFER`：保持 `VERIFY_HOST_KEY`，之后在同一会话执行流调用 `xrtSshClientCoreHostKeyAccept` 或 `xrtSshClientCoreHostKeyReject`。

## 认证

默认先发送 `none` 请求，以取得服务端方法列表。后续由 `xsshclientauthproc` 选择 password、publickey、keyboard-interactive 或外部方法。回调返回 `XSSH_ERROR_SPACE` 时，核心会在 `OutputLimit` 内扩容并重试；回调必须是可重试的。返回 `XSSH_NEED_MORE` 会产生 `XSSH_CLIENT_NEXT_AUTH`，供异步凭据源稍后继续。

`xrtSshClientPasswordAuth` 是普通 password 认证构建器，`AuthenticateData` 指向一个有效的 `xstrview` 口令。它会检查服务端方法列表，且不会接管口令。

## 动作循环

1. 调用 `xrtSshClientCoreNext`。
2. `IDENTIFICATION` 交给 `xrtSshSessionTcpIdentificationWritePrepare`。
3. `PAYLOAD` 交给 `xrtSshSessionTcpWritePrepare`。
4. `INPUT` 等待会话读取器；在 packet 提交前调用 `Observe`。
5. `HOST_KEY` 或 `AUTH` 等待外部决定。
6. `READY` 表示 `ssh-connection` 已经开放，可以进入 channel 层。

完整配置示例见 `examples/client_core/main.c`。客户端 Stream、future、同步等待和经典 exec/shell/forward 将建立在本核心之上，不复制 KEX 或认证逻辑。
