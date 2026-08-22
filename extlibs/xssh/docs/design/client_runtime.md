# xssh 客户端运行时设计

## 范围

经典 `exec`、交互 `shell`、subsystem 和 TCP forwarding 共用同一条客户端链路：

```text
xnetstream -> xsshsessionstream -> xssh client driver -> xsshchannels -> application
```

客户端运行时只组合已经公开的协议和网络层，不复制 KEX、认证、channel 报文、TCP 等待或
缓冲实现。它不创建隐藏 Engine，不用固定 channel/event 数组，也不把同步、Future 和协程写成
三套状态机。

## 分层

1. `ssh_client_core`：已经实现的无网络客户端动作核心，唯一处理 identification、KEX、主机信任、
   USERAUTH、动态敏感输出和认证方法副本。
2. `ssh_channels`：动态 channel 所有权、稳定本地 id、resolver、数据缓冲和回复 FIFO。
3. `ssh_client`：在一个已连接 `xnetstream` 上组合 `ssh_client_core`，并驱动
   connection packet，只提供 callback 与非阻塞操作。
4. `ssh_client_dial`：已实现的可裁剪薄适配，直接组合 XRT Resolver、Happy Eyeballs、deadline、
   cancel、统计和结构化错误；核心客户端仍可接管代理、Unix socket 或应用自行建立的 Stream。
5. `ssh_client_future`：已把 Ready、channel open/request、read/write/drain/close 的单次等待桥接为
   Future；不拥有第二套协议状态，取消只移除本次 waiter。
6. 同步与协程不形成 SSH 重复模块：线程使用 `xrtFutureWaitUntilCancel`，协程使用
   `xrtFutureAwait` 等待同一个 Future，成功、失败、取消和关闭语义完全相同。
7. `ssh_forward`：组合 XRT listener/stream 与 direct/forwarded-tcpip channel，负责双向背压、
   半关闭和连接级预算，不使用旧版 `select` 轮询泵。

`exec`、`shell` 和 subsystem 是 `ssh_client` 的短场景函数，不单独形成网络运行时。PTY、env、窗口
变化、signal 和 break 继续复用现有协议写出函数；高级入口不能封死自定义 channel type 或 request。

## 客户端所有权

客户端对象由调用方持有，借用 Engine、Resolver、TLS/Proxy 建链结果和策略回调；接管一个
Stream 引用、`xsshsessionstream`、`xsshchannels`、少量可增长控制报文 scratch，以及认证方法列表
副本。密码和私钥默认只借用到认证终态，私钥签名可以由应用或 HSM 回调完成，客户端不长期复制
秘密。

空客户端不预分配 channel、DATA、stderr 或回复 token。控制报文 scratch 从零增长并复用；
channel 数据继续使用 `xnetbuf` 的 Copy/Borrow/Take/Ref/Buffer 五种路径。全部数量和字节预算都来自
公开配置，达到上限返回 `XSSH_ERROR_SPACE` 或 XRT `AGAIN`，不建立隐藏无界队列。

对象只能由 Stream 所属 Worker 推进。非 Worker 调用通过 XRT Engine Post/Future 桥接；公开低层
访问器仍允许高级用户在同一 Worker 直接取得 session、transport、KEX、auth、connection 和 channel。

## 动作驱动

`xsshsessionstream.Action` 只报告下一动作。客户端驱动按下列唯一映射准备输出或策略决定：

| Action | 客户端行为 |
| --- | --- |
| `WRITE_IDENTIFICATION` | 写稳定、可配置的 SSH-2.0 版本串 |
| `WRITE_KEXINIT` | 按首次或 rekey 配置构建安全随机 KEXINIT |
| `BEGIN_KEX` | 使用系统 CSPRNG 建立 Curve25519 临时密钥 |
| `WRITE_ECDH_INIT` | 直接从 KEX session 构建方法报文 |
| `VERIFY_HOST_KEY` | 先完成密码学验签，再调用主机信任策略；默认不静默信任 |
| `WRITE_NEWKEYS` | 构建 NEWKEYS，密钥激活仍由 session commit 原子完成 |
| `BEGIN_AUTH` | 启动统一认证 guard |
| `WRITE_SERVICE_REQUEST` | 请求 `ssh-userauth` |
| `WRITE_AUTH_REQUEST` | 调用可扩展认证 provider；密码只是内置常见 provider |
| `CONNECTION` | 提交上次 packet 的应用结果，再调度一个控制报文或 DATA 分片 |

认证 provider 接收 server 方法清单、SessionId、用户名和调用方 writer，可以实现 none、password、
publickey、keyboard-interactive、agent、硬件签名或私有扩展。客户端只约束总时间、尝试、轮次、消息和
字节预算，不把认证顺序写死。

内置 `xrtSshClientEd25519Auth` 组合公开 publickey 签名原文、OpenSSH Ed25519 身份和客户端动态
输出，直接生成带签名请求，不增加 probe 往返或额外堆分配。agent、HSM 和其他算法继续使用同一个
provider 契约，不要求进入客户端核心。

## Packet 提交边界

Stream 的 `Packet` 回调发生在 transport/session read commit 之前。客户端必须遵守两阶段顺序：

1. 在 `Packet` 中检查借用视图，并对 DATA/stderr 调用 `xrtSshChannelIoReceivePrepare`；复制必须在
   packet 仍有效时完成。
2. 回调返回 ACCEPT 后，由 Stream 提交 transport 和 connection/channel 状态。
3. 随后的 `Action(CONNECTION)` 调用 `ReceiveCommit`，再向应用发布 Data、Request、Open、EOF、
   Close 或 reply 事件。

提交后事件只保存稳定 channel 指针、事件枚举、失败 reason 和回复 token，不保存 packet 中的借用
字符串。普通应用使用 `Channel` 事件推进 open/request/EOF/close 状态；需要检查未知 request type 或
扩展字段的应用仍在 `Packet` 中读取，并在返回 ACCEPT/HOLD 前复制真正需要跨提交保留的数据。

任何 Prepare 失败都返回 HOLD/RETRY 或拒绝当前 packet，不能先发布应用事件再发现协议提交失败。
Borrow 类型未知扩展如果应用要求零复制，必须在 Packet 回调内同步消费并明确 ACCEPT；不能把借用
指针保存到 commit 之后。

## 经典场景

`OpenSession` 创建动态 channel 并发送 `session` open。confirmation 可靠提交后，应用可选择：

- `Exec(command)`：发送 want-reply exec；成功后流式读取 stdout/stderr，并保存 exit-status 或
  exit-signal，直到双向 close。
- `Shell(pty)`：可先发送零个或多个 env，再发送可选 PTY 和 shell；窗口变化、输入、EOF 与关闭都
  走同一 channel。
- `Subsystem(name)`：与 exec 使用同一请求/回复和数据链路。
- `OpenDirect(target, origin)`：建立 direct-tcpip；本地 Stream 和 SSH channel 分别遵守自己的
  高低水位，任一方向 EOF 只关闭对应发送半边。

短 helper 只组合必要报文，不构建字典、响应对象或事件数组。每个 helper 都必须保留等价底层路径：
调用方可以自行写 payload，再交给公开 session Prepare/Commit。

## Future、同步与协程

每个可等待操作进入按需链表节点，结果保留成功、远端拒绝、结构化 `xerror`、协作取消和对象关闭
终态。timeout/cancel 只取消本次等待；是否中止 channel 或整个连接由操作契约明确决定。线程与协程
直接使用 XRT 的通用 Future 等待，不增加一组只改名的 SSH wrapper，因此三种入口的成功、超时、
取消、背压和关闭语义完全一致。

channel waiter 以指针和本地 ID 双重关联。这样 close 回调可以移除旧 channel 并立即创建新对象，
即使内存地址被复用，提交后的旧通知也不能误完成新 waiter。读取等待在无剩余数据的 EOF/close 上
关闭，写入与 request waiter 在 channel close 上关闭，不依赖整个 SSH 连接最终退出。

## 验证门禁

当前 Windows x86-64 `select` 真实 TCP 回归通过公开 `xrtSshClientDial` 建链，并同时验证 callback、
Future、16 个并发 close waiter、跨线程 Future 取消、水平条件立即完成、channel 地址复用和 EOF
终态。链路已经覆盖动态 Ed25519 主机密钥、Curve25519 KEX、
`none` 方法探测、错误 password 拒绝后重试、remote forward 成功与 cancel 失败、服务端主动
forwarded-tcpip、exec、PTY、window-change、shell、direct-tcpip、双向 DATA、stderr、exit-status、
exit-signal、EOF 和双向关闭。客户端在 Packet 回调内解析并接受 forwarded-tcpip，确认提交后再发布
`Incoming OPENED`，随后验证 DATA、EOF、CLOSE、Future 终态和双方 channel 延迟回收；该路径同时
回归 Packet 回调与 HOLD 共用的 peer-open 决策契约。
channel request 与 channel open 还分别验证远端拒绝、结构化失败 Future、后续操作恢复及失败 channel
延迟回收。测试还在独立连接拒绝正确但不受信任的主机密钥，验证认证错误同时到达 Error callback、
Close 和未决 Ready Future；另一条连接在 remote-forward Future 未决时由服务端用零秒 linger 注入
RST，验证网络错误只通过 Close 终态传播，未决 Future 以同一个 `xrt.net` I/O 错误失败，且不会误报
SSH 协议 Error callback。测试复用公开 server SessionStream 与 Channels，使用 17/19 字节读分片，
另有静默服务端在 TCP 建连后不发送 identification，验证 50 ms Ready 截止时间以
`XSSH_ERROR_TIMEOUT / XERR_TIMEOUT` 唯一终结 Error、Close 和未决 Ready Future。成功链路中的
forwarded-tcpip 先由 Packet 回调暂存接受决定并返回 HOLD，再在同 Worker 的下一轮显式 Accept，
证明输入提交、channel confirmation 和 `Incoming OPENED` 的顺序不被异步策略破坏。三条 Select
故障链、Select 完整成功链和 Windows IOCP 硬背压成功链已完成 100 轮、500 条连接的连续运行。
IOCP 链使用 32 KiB 合法 IGNORE 和 48 KiB TCP 写预算，确定性证明 `AGAIN` 时完整 packet 留在
transport，底层 Drain 先提交内部保留包、确认实际排空后才通知应用，随后仍完成全部工作流。该回归
还通过 XRT 内存调试器逐点击穿
Future 管理器、waiter、Promise、cancel 和 watch 五个逻辑分配点；direct-tcpip 使用 32 字节发送
硬上限、16 字节 packet/窗口返还传输 96 字节数据，验证 `XSSH_ERROR_SPACE`、WRITE Future、五次
`WINDOW_ADJUST` 恢复与六次分片提交。全部 Engine、Resolver、Stream、Dial、channel 和 Future
清理后，内存调试的活动块、活动字节及越界/重复释放计数均归零。以下仍是产品发布前需要继续完成的门禁：

- 确定性双端 mock：custom auth、stderr、exit-signal、open/request 拒绝和对端主动
  forwarded-tcpip 已经覆盖。
- 故障注入：Future 动态分配、channel 发送窗口、坏 host key 和未决全局请求期间 RST 断线的唯一终态
  已经覆盖；Packet HOLD/显式恢复、TCP 后 Ready 超时、客户端组合层 TCP `AGAIN` 与内部 Packet OOM
  RETRY 已经覆盖，重试型 OOM 不会保留为 `TerminalError`。认证失败后的合法重试已经覆盖。
  64 路 channel 的发送窗口耗尽、反向 `WINDOW_ADJUST` 恢复、接收预算耗尽、消费和窗口返还已覆盖；
  `uint32` ID 回绕避让、65,536 轮 reply FIFO 环绕及连接级/通道级 token ring 扩容也已覆盖。
- OpenSSH 互操作：`ssh_client_openssh_tests` 已提供显式环境门禁，覆盖 password/Ed25519 publickey、
  exec、PTY shell 和 direct-tcpip；没有 `sshd` 的环境明确 SKIP，不伪造通过。Windows/Linux 的
  实际运行结果仍是把产品状态改为 `implemented` 的必要证据。
- 长稳态：单连接真实 TCP 慢写窗口恢复和关闭后动态块归零已经覆盖，64 路无网络双向窗口压力也已
  覆盖；仍需真实网络多 channel 长跑和慢读证据。
- 裁剪：core client 不携带 dial/Future/sync/forward；exec 不携带 PTY；direct-tcpip 不携带 shell。

旧版 `xsshConnect`、`xsshPoll` 和固定事件队列只作为行为资产，不恢复其隐藏 Engine、轮询循环、固定
数组或兼容命名。新 API 冻结前必须先通过上述 mock 与真实 OpenSSH 两类门禁。
