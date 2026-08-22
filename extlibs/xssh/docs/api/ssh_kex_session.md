# SSH KEX 会话

`ssh_kex_session` 把双方 KEXINIT、Curve25519、SHA-256 exchange hash、
Ed25519 主机签名验证、A-D 密钥派生与 NEWKEYS 方向切换组合成确定性状态机。
它不创建 socket、Engine、任务或等待对象，也不保存 KEXINIT 和 packet 大缓冲。

## Transcript

`xsshkextranscript` 的四段视图必须在本轮 KEX 完成前保持有效。高级驱动可直接借用
稳定缓冲；需要脱离网络输入生命周期时，先用 `xrtSshKexTranscriptMeasure` 和
`xrtSshKexTranscriptWrite` 精确复制到调用方工作区。

## 事务

ECDH_INIT、ECDH_REPLY 和 NEWKEYS 都先调用对应 `Prepare`。transport core 可靠提交
packet 后再调用 `xrtSshKexSessionWriteCommit`；发送取消则调用 `WriteAbort`。
接收方向先由 transport core 认证 packet，再调用 `ReadPrepare`，随后按相同顺序提交
core 和 session。`ReadAbort` 会终止会话，因为已认证输入不能安全回滚。

## 主机信任与签名

服务端通过 `xrtSshKexSessionExchangeHash` 把摘要交给本地私钥、HSM 或其他签名器，
再把 signature blob 交给 `xrtSshKexSessionEcdhReplyPrepare`。该函数会用公开主机密钥
复验签名，避免错误签名进入线路。

客户端对 ECDH_REPLY 完成密码学验签后，将主机公钥复制到调用方存储并产生
`XSSH_KEX_EVENT_VERIFY_HOST_KEY`。known_hosts、证书或应用策略接受后调用
`xrtSshKexSessionHostKeyAccept`，拒绝时关闭 transport 并调用 `xrtSshKexSessionFail`。

## 密钥切换

本端 NEWKEYS 可靠提交后调用 `xrtSshKexSessionActivateWrite`；对端 NEWKEYS 认证提交后
调用 `xrtSshKexSessionActivateRead`。两个方向独立切换，函数会按 client/server 角色
选择 C2S 或 S2C 材料，并在 core 接管后清除会话中的对应密钥副本。

## 随机便利层

确定性核心使用 `xrtSshKexSessionBeginWithPrivate` 显式注入临时私钥。生产默认入口
`xrtSshKexSessionBegin` 位于独立 `ssh_kex_session_random` 模块，只额外引入 XRT 系统
安全随机源；确定性测试、会话 PRNG 和专用密钥设备无需携带该依赖。
