# SSH 连接级会话核心

## 定位

`ssh_session_core` 把 `ssh_kex_exchange`、`ssh_auth_session` 和
`ssh_connection_session` 组合成一条连接级协议生命周期。它不是网络客户端，也不拥有 socket、
TCP stream、等待、deadline、凭据、主机信任数据库、channel 表或数据队列。

对象只固定保存密码状态和短事务快照。identification 与每代 KEXINIT transcript 使用调用方选择的
`xnetbufpool` 动态分块；空闲对象没有固定收发数据块。历史 runtime 中每连接 32 KiB 线路数组、
每 channel 16 KiB 数据数组、固定 channel/event 表均不进入该闭包。

系统安全随机临时密钥入口位于独立的 `ssh_session_core_random`。确定性核心可以用于协议向量、
可控随机源和 HSM 驱动，不会因此携带操作系统随机模块。

## 生命周期

1. 初始化 `xsshtransportcore` 与 `xsshsessioncore`。
2. 对每个 identification 先调用 `xrtSshSessionCoreVersionPrepare`，transport 提交后调用
   `xrtSshSessionCoreVersionCommit`。
3. KEXINIT 与普通 payload 都使用写事务；双方 KEXINIT 就绪后调用
   `xrtSshSessionCoreKexBeginWithPrivate` 或随机便利入口。
4. 通过 `xrtSshSessionCoreKex` 取得底层 KEX 会话，构建 ECDH、验证主机信任并发送 NEWKEYS。
   会话核心在 NEWKEYS commit 后自动激活对应 cipher 方向并收口 transcript 代际。
5. 首轮 KEX 完成后调用 `xrtSshSessionCoreAuthBegin`。认证方法仍通过公开认证模块构建最终 payload。
6. server USERAUTH_SUCCESS 在双方可靠提交后，connection 会话自动开放，不存在认证与 channel
   状态之间的空窗。
7. rekey 会优先路由到同一个 KEX 交换对象；认证或 connection 状态保持不变。

`xrtSshSessionCorePhase` 返回 identification、初始 KEX、认证、connection、rekey、closing 或失败。
`xrtSshSessionCoreAction` 进一步把三个子状态统一为驱动的下一项常见动作，覆盖双方 identification、
KEXINIT、ECDH、主机信任、NEWKEYS、USERAUTH 和 connection。它只读推导状态，不创建事件队列；
`WRITE_PENDING` 与 `READ_PENDING` 明确要求当前事务先提交或中止。本端 identification 和 KEXINIT
优先返回写动作，因此双方都采用默认驱动顺序时不会互相等待。

KEX、认证和 connection 三个访问器仍保留全部底层能力。统一动作适合 callback、future、同步和协程
驱动共享控制流，算法扩展、异步 HSM 或自定义认证后端仍可直接进入子对象，不会被封死在便利接口中。

## 写事务

写路径的稳定顺序是：

1. 用消息模块直接构建最终 SSH payload。KEX 方法先调用对应 KEX 会话 Prepare。
2. 调用 `xrtSshSessionCoreWritePrepare`，完成阶段、角色、预算和外部 channel/FIFO 候选验证。
3. 调用 transport core 或 TCP adapter 的写 Prepare。
4. 调用 `xrtSshSessionCoreWriteBind`，确认 transport 当前未决消息、payload 地址和长度属于同一事务。
5. transport 可靠接管线路输出并提交。
6. 调用 `xrtSshSessionCoreWriteCommit`。

网络队列返回 AGAIN 时，transport 和 session 两边都保持未决，可直接重试提交。放弃时先调用
`xrtSshSessionCoreWriteAbort` 回滚 KEX/auth/connection 候选，再中止 transport 并丢弃其输出。
不得在 Prepare 与 Bind 之间替换或修改 payload。

## 读事务

读路径先由 transport 认证完整 packet，再调用 `xrtSshSessionCoreReadPrepare`。返回的
`xsshsessionpacket` 区分：

- disconnect、ignore、unimplemented、debug、EXT_INFO 和 NEWCOMPRESS；
- KEXINIT 与 KEX 方法；
- USERAUTH；
- RFC 4254 connection；
- 未知扩展。

已知报文提供轻量借用视图，未知消息保留完整 payload。调用方处理完成后先提交 transport，再调用
`xrtSshSessionCoreReadCommit`。认证后的输入不能回滚；拒绝时调用 `xrtSshSessionCoreReadAbort` 并
关闭 transport。

客户端读取 ECDH_REPLY 时可传入主机公钥存储。空间不足返回 `XSSH_ERROR_SPACE` 并给出精确尺寸，
transport 读事务保持未提交，可扩容后重试。主机信任仍通过 KEX 会话访问器显式确认。

## 扩展与所有权

`XSSH_SESSION_PACKET_EXTENSION` 是未知消息的逃生口。会话核心不注册私有扩展表，也不自动生成
UNIMPLEMENTED；应用可以检查原始消息号和 payload 后提交、回复或关闭。

connection resolver、global/per-channel reply FIFO、channel core 和 channel I/O 都由调用方持有。
因此数组、哈希表、slot map、slab、零复制代理和动态缓冲策略可以共用同一会话核心。

`xrtSshSessionCoreClear` 释放动态 transcript 并清除秘密，但不处理 transport 和外部对象。
`xrtSshSessionCoreFail` 只终止协议子状态，socket 的 shutdown/abort、任务取消和等待唤醒仍属于驱动层。

## 裁剪

```c
#define XSSH_MODULE_SSH_SESSION_CORE
#define XSSH_IMPLEMENTATION
#include <xssh.h>
```

生产环境需要系统安全随机临时密钥时选择：

```c
#define XSSH_MODULE_SSH_SESSION_CORE_RANDOM
#define XSSH_IMPLEMENTATION
#include <xssh.h>
```

两个闭包都不引入 TCP、task 或 coroutine。TCP 同步、future 和协程驱动将在会话核心之上共享同一
Prepare/Bind/Commit 契约。
