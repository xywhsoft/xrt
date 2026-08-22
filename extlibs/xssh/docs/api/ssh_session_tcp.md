# SSH TCP 会话驱动

## 定位

`ssh_session_tcp` 把 `ssh_transport_tcp` 与 `ssh_session_core` 组合成一条不会错序的 TCP 会话事务链。
它仍不是 SSH client/server：对象不创建或持有 `xnetstream`、engine、worker、等待、deadline、取消、
任务、凭据、主机信任数据库、channel 表或 channel 数据队列。

对象没有固定 packet、banner、channel 或事件数组。线路输出继续使用 Worker 的 `xnetbufpool` 动态链；
输入直接借用 Stream 回调提供的 `xnetbuf`。对象新增的读取状态只有 packet 元数据和外部明文工作区视图，
不会让每连接空闲内存随最大报文尺寸增长。

确定性基础闭包接受调用方 padding 和 Curve25519 私钥。系统安全随机 padding 与临时密钥位于独立的
`ssh_session_tcp_random`，测试向量、HSM 和可控随机源无需携带系统随机模块。

## 所有权

`xsshsessiontcp` 持有：

- 一个 `xsshtransporttcp`；
- 一个 `xsshsessioncore`；
- 当前未决读事务的借用 view。

调用方持有：

- `xnetstream` 与其 Worker；
- 单调时钟、deadline、取消与等待模型；
- 凭据、认证后端和主机信任策略；
- channel 查找表、reply FIFO 与 channel I/O；
- 解密工作区和客户端主机公钥存储。

通过 `xrtSshSessionTcpTransport` 和 `xrtSshSessionTcpCore` 可以继续访问全部低层状态。组合层不以统一
事件对象遮蔽 KEX、认证、connection 或 packet codec 能力。`xrtSshSessionTcpAction` 只是对同一核心
动作推导的零状态便利入口，后续网络驱动不需要再次编写阶段判断。

## Identification

本端版本调用 `xrtSshSessionTcpIdentificationWritePrepare`，随后调用
`xrtSshSessionTcpWriteSubmit`。网络队列返回 `XNET_RESULT_AGAIN` 时，版本 transcript 与动态输出都保持
未决，可直接重试；放弃时调用 `xrtSshSessionTcpWriteAbort`。

对端版本在 Stream 读回调中调用 `xrtSshSessionTcpIdentificationReadPrepare`。输入不足返回
`XSSH_NEED_MORE` 且不消费数据。成功后调用 `xrtSshSessionTcpReadCommit`，transport 与 transcript 才会
共同发布。

## Packet 写入

消息模块仍直接构建最终 payload，不产生中间响应对象：

1. 调用 `xrtSshSessionTcpWritePrepareWithPadding`；
2. 调用 `xrtSshSessionTcpWriteSubmit`；
3. `AGAIN` 时保留 payload 和所有外部候选，等待 writable 后重试；
4. 放弃时调用 `xrtSshSessionTcpWriteAbort`。

Prepare 内部严格执行 session prepare、transport prepare、payload bind。Submit 只有在 Stream 接管动态链
后才依次提交 transport 与 session。KEXINIT、NEWKEYS、认证和 channel 状态不会早于真实网络队列推进。

随机闭包把同一入口简化为 `xrtSshSessionTcpWritePrepare`，其余事务完全相同。

## Packet 读取

先调用 `xrtSshSessionTcpReadInspect` 获取线路尺寸和明文容量，再调用
`xrtSshSessionTcpReadPrepare`。`xsshsessiontcppacket` 同时返回：

- `Transport`：sequence、packet length、padding 和原始 payload view；
- `Session`：transport 控制、KEX、认证、connection 或未知扩展的轻量解析。

成功结果借用到 `xrtSshSessionTcpReadCommit` 或 `xrtSshSessionTcpReadAbort`。客户端 ECDH_REPLY 的主机
公钥存储不足时返回 `XSSH_ERROR_SPACE`，transport 不消费输入；保持同一个 `pInput`、`pPlain` 和容量，
换用更大的主机密钥存储即可重试上层解析。

认证后的协议错误会关闭 transport 并终止 session。该层不会静默跳过错误 packet，也不会在失败后让
上层状态继续运行。

## KEX 与认证

双方 KEXINIT 提交后，确定性路径调用 `xrtSshSessionTcpKexBeginWithPrivate`，随机路径调用
`xrtSshSessionTcpKexBegin`。KEX 方法 payload 仍通过 `xrtSshSessionTcpCore` 取得公开 KEX 会话构建；
主机信任也必须由调用方显式确认。

首轮 KEX 完成后调用 `xrtSshSessionTcpAuthBegin`。认证成功会沿用 session core 的契约自动开放
connection 层。rekey 使用同一对象，认证与 channel 状态不丢失。

## 裁剪

确定性 TCP 会话闭包：

```c
#define XSSH_MODULE_SSH_SESSION_TCP
#define XSSH_IMPLEMENTATION
#include <xssh.h>
```

生产环境常用的安全随机闭包：

```c
#define XSSH_MODULE_SSH_SESSION_TCP_RANDOM
#define XSSH_IMPLEMENTATION
#include <xssh.h>
```

两个闭包都不引入 task、future 或 coroutine。后续同步、future 和协程 client/server 驱动都应复用
这里已经闭合的网络接管事务，而不是再次组合协议状态机。
