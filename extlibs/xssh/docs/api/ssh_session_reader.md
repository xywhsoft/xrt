# SSH 动态会话读取器

## 定位

`ssh_session_reader` 是 `ssh_session_tcp` 之上的可选按需工作区。它解决高层 client/server 驱动最容易
重新引入固定数组的问题：packet 长度头尚未到齐时不分配，完整线长已知后才按本包实际明文尺寸申请
连续空间；客户端 ECDH_REPLY 的主机公钥先取得精确长度，再在同一未消费输入上扩容重试。

未启用入站密码的明文 packet 直接借用输入链，不申请零长度工作区。启用 AES-GCM 后才按探测出的
`PlainSize` 申请连续尾部；缓冲池可以按尺寸类返回更大的实际容量，因此这是精确的最小需求，不是
要求分配器产生大小完全相等的物理块。

读取器借用 `xsshsessiontcp` 和输入 `xnetbuf`，拥有一个临时明文链和一个稳定主机公钥链。它不持有
`xnetstream`、Engine、Worker、等待、任务、凭据、信任数据库、channel 表或事件队列。对象本身只有
缓冲链头和事务元数据，不包含 packet、banner 或 key 的固定容量数组。

## 初始化

`xrtSshSessionReaderInit` 要求读取器与 TCP 会话使用同一个 `xnetbufpool`。这使分配和回收都保持在会话
所属 Worker，并复用 XRT 的四级尺寸类。空池表示两者都直接使用全局分配器。

Reader 必须早于绑定会话清理。`xrtSshSessionReaderClear` 遇到未决 packet 会调用底层读中止，消费对应
线路前缀并把会话置为失败，防止动态工作区释放后留下悬空借用。

## 读取事务

在 Stream 的 `Read` 回调中调用 `xrtSshSessionReaderPrepare`：

1. 四字节长度头不足或完整线路尚未到齐时返回 `XSSH_NEED_MORE`，输入和 Reader 都保持空闲；
2. 完整 packet 到齐后，明文模式直接借用输入，密码模式按 `PlainSize` 最小需求申请连续尾部；
3. 普通 packet 直接返回轻量解析结果；
4. 客户端 ECDH_REPLY 首次返回的精确 host-key 尺寸由 Reader 内部处理，并在同一明文地址上自动重试；
5. 应用接受消息后调用 `xrtSshSessionReaderCommit`，拒绝则调用 `xrtSshSessionReaderAbort`。

Prepare 成功返回的 packet 与 `xrtSshSessionReaderHostKey` 都是借用视图。packet 只存活到 Commit/Abort；
已提交主机公钥保持到下一轮主机密钥替换或 Reader 清理，便于信任策略、指纹和日志异步分层处理。

所有有效 packet 的内存硬上限仍由 TCP transport 的 `MaxPacketSize` 统一控制，不维护第二套互相冲突的
限制。分配失败返回 `XSSH_ERROR_SPACE`，transport 输入尚未消费，调用方可以释放其他 Worker 缓存后重试，
也可以显式 Abort 关闭该会话。

同一 Reader 同时只允许一个成功 Prepare 的 packet 事务。提交或中止前再次 Prepare 返回
`XSSH_ERROR_STATE`；`xrtSshSessionReaderState` 对无效对象返回 `XSSH_SESSION_READER_INVALID`，不会与
等待 host-key 空间混淆。

`XSSH_ERROR_SPACE` 不等同于 host-key 空间不足。只有解析结果明确给出更大的 `HostKeySize` 时状态才是
`XSSH_SESSION_READER_HOST_KEY`；KEX transcript 等内部动态分配失败返回
`XSSH_SESSION_READER_RETRY`。两种状态都保留同一未消费输入和解密工作区，释放其他 Worker 缓存后可用
相同参数重试。主机公钥尺寸在重试间增长时，Reader 会放弃旧 staging 并按新最小尺寸重新申请。

## 裁剪

```c
#define XSSH_MODULE_SSH_SESSION_READER
#define XSSH_IMPLEMENTATION
#include <xssh.h>
```

该闭包只依赖确定性 `ssh_session_tcp`，不会带入安全随机、task、future 或 coroutine。生产驱动可再选择
`ssh_session_tcp_random`；两者共享同一个 TCP 会话对象，不需要第二份读取状态机。
