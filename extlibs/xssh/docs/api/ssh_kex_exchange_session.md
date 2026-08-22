# SSH KEX 交换上下文

`ssh_kex_exchange` 负责把连接级 identification、每代双方 KEXINIT 和 `ssh_kex_session` 组成一个
稳定生命周期。它解决驱动层最容易出错的三件事：不能在 transport 提交前发布报文、transcript
必须跨完整方法交换存活，以及 rekey 不能提前释放上一代仍被协商结果借用的内存。

## 对象边界

`xsshkexexchange` 拥有七条按需分配的 `xnetbuf` 链和一个 `xsshkexsession`：

- client/server identification 在整个 SSH 连接期间保留；
- 当前代 client/server KEXINIT 保留到下一代成功开始；
- 下一代两条 KEXINIT 与单条 staging 链只在交换期间占用内存；
- 空闲对象不分配 payload 块，也不包含历史实现的 2 KiB/4 KiB 固定数组；
- 对象不拥有 transport、TCP、Engine、任务、时钟、主机密钥或随机源。

当前代和下一代不能复用同一组链。`xsshkexsession` 的协商结果会借用当前 KEXINIT；收到 rekey 的
第一条 KEXINIT 后立刻清除旧链会产生悬空视图。实现只在新一代 `Begin` 成功、会话已经改借新视图
后晋升缓冲，因此失败、背压和中止都保留上一代完整状态。

## 提交顺序

Identification 的可靠边界：

1. transport 准备发送或解析出无换行版本串；
2. `xrtSshKexExchangeVersionPrepare` 复制并校验版本串；
3. transport/core 提交对应方向；
4. `xrtSshKexExchangeVersionCommit` 发布稳定副本。

KEXINIT 的可靠边界：

1. `xrtSshTransportCoreWritePrepareWithPadding` 或 `ReadPrepare` 建立 packet 事务；
2. `xrtSshKexExchangeKexInitPrepare` 校验并复制同一 payload；
3. 网络受理后提交 transport core；
4. `xrtSshKexExchangeKexInitCommit` 核对方向、代际和 packet ordinal 后发布副本。

在 transport 提交前可调用对应 `Abort`。如果误把 `ExchangeCommit` 放在 core 之前，函数返回
`XSSH_ERROR_STATE` 且保留 staging，调用方仍可按正确顺序完成提交。

## 开始与推进

`xrtSshKexExchangeReady` 同时检查四段 transcript 和 transport 状态。确定性测试、自定义密钥管理
或受控随机源使用 `xrtSshKexExchangeBeginWithPrivate`；生产便利路径使用独立裁剪的
`xrtSshKexExchangeBegin`。后者只增加 XRT 安全随机与 Curve25519 密钥对依赖，并在返回前清除
临时私钥和冗余公钥副本。

开始成功后，`xrtSshKexExchangeSession` 返回底层会话，ECDH、主机信任、NEWKEYS 和方向密钥激活
继续使用 `ssh_kex_session` 的事务 API。双向切换完成后调用 `xrtSshKexExchangeComplete`，对象便可
接受下一代 KEXINIT。`SessionId` 保存在同一个会话中，不会在 rekey 时改变。

`xrtSshKexExchangeTranscript` 在 READY 阶段借出下一代，在 METHOD/COMPLETE 阶段借出当前代。
视图只在交换对象未清理且未开始下一代成功晋升时有效。

## 裁剪

```c
#define XSSH_MODULE_SSH_KEX_EXCHANGE
#define XSSH_IMPLEMENTATION
#include <xssh.h>
```

确定性闭包只包含 KEX 会话与 XRT 动态网络缓冲，不带 TCP、任务、协程或安全随机。生产随机入口使用
`XSSH_MODULE_SSH_KEX_EXCHANGE_RANDOM`。
