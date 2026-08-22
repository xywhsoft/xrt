# xssh

`xssh` 是建立在新版 XRT 公共能力之上的 SSH 扩展。当前已经收口无分配 wire、packet、KEX、
transport、双端 USERAUTH 编排、无固定 channel 数量的 connection 会话、连接级协议核心和客户端
动作核心。组合 Stream 客户端、Resolver/Happy Eyeballs Dial、session/exec/shell/subsystem、PTY 与
TCP forwarding API 已完成实现、裁剪验证和 Windows x86-64 Select 真实 TCP 回归；
Ready/Drain/Close、channel 与 request Future 已完成，线程和协程直接等待同一 XRT Future；
Future 逻辑分配故障、认证重试、channel open/request 拒绝、服务端主动 forwarded-tcpip、stderr、
exit-signal、坏主机密钥拒绝、未决请求期间 RST 断线、发送硬背压、小窗口恢复和全链路资源归零已经
进入同一真实 TCP 回归；组合客户端的 Packet HOLD/显式恢复与 TCP 建连后的 Ready 截止时间也已经
压实。Windows IOCP 组合链还以 32 KiB 合法报文和 48 KiB 硬预算确定性验证 TCP `AGAIN`、整包保留、
内部优先 Drain 重试和后续工作流；内部 Packet OOM RETRY 不会污染连接终态。64 路 channel 的
双向窗口耗尽/恢复、`uint32` ID 回绕避让、65,536 轮 reply FIFO 环绕及连接级/通道级 token ring
扩容也已通过确定性门禁。Linux GCC/Clang 已通过真实 OpenSSH 的 exec、PTY、direct-tcpip 和
8 路并发长输出慢读门禁；每路正文超过 32 KiB，并在 32 KiB 接收窗口、8 KiB packet、256 字节
分块消费下验证窗口回补、exit-status、结束标记、确定性关闭和全链路资源归零。

真实 OpenSSH 门禁已经提供可执行测试；未配置目标时明确 `SKIP`，password、Ed25519 identity、
PTY 和 direct-tcpip 的运行方法见 [OpenSSH 互操作门禁](docs/design/openssh_interop.md)。

## 当前能力

- 网络字节序 byte、boolean、uint32、uint64。
- uint32 长度前缀 string 与原始字节视图。
- 严格 name-list 校验、包含、重复检测和首选算法选择。
- 规范非负/有符号 mpint 读取与 magnitude 写入。
- 严格生成本端 SSH-2.0 identification，并支持前置行、SSH-2.0/SSH-1.99 对端增量解析。
- 调用方 padding 源驱动的 plain packet 计算、写入、增量读取和序列管理。
- 操作系统密码学安全随机源驱动的 plain packet 便利路径。
- `aes128-gcm@openssh.com` 与 `aes256-gcm@openssh.com` 的无堆分配 packet 编解码。
- AES-GCM 坏标签拒绝、nonce 防回绕和失败状态原子性。
- 可独立裁剪随机适配的 plain/AES-GCM 双向 packet codec、NEWKEYS 切换、strict-kex 序列重置与工作区探测。
- 无固定会话数组的 TCP transport 核心，以及可独立裁剪的系统安全随机 padding 便利层。
- client/server 共用的确定性 KEX 会话事务，以及可独立裁剪的安全随机临时密钥入口。
- 连接级稳定 identification、动态 KEXINIT transcript、提交序号关联与无悬空 rekey 晋升。
- 角色/阶段安全默认的 KEXINIT、RFC 8308/strict-kex 标记、严格解析、协商和 guessed packet 处理。
- 显式 cookie 与操作系统安全随机 cookie 两条 KEXINIT 构建路径。
- ECDH init/reply 无分配报文编解码。
- 流式 SHA-256 exchange hash 与 A-F 密钥扩展。
- Curve25519 公钥、低阶点拒绝、共享秘密与安全随机临时密钥对。
- 通用 host-key/signature blob 与严格 Ed25519 主机密钥验证。
- OpenSSH public-key/authorized_keys 行、quoted options 与可扩展算法文本解析。
- known_hosts marker、通配符、否定项、非默认端口和大小写无关明文匹配。
- 独立裁剪的 OpenSSH `|1|` hashed-host HMAC-SHA1 生成与常量时间匹配。
- 无分配 known_hosts 文本游标与 `MATCH / NEW / CHANGED / REVOKED / CA` 信任判定。
- 不绑定算法的 OpenSSH SHA-256 主机密钥指纹。
- 无固定缓冲的 openssh-key-v1 容器、可选 XRT PEM 适配和未加密 Ed25519 签名。
- RFC 4253 公共 transport 消息与 RFC 8308 EXT_INFO/NEWCOMPRESS。
- RFC 4253/4344 字节、时间、包与密码块 rekey 策略。
- 无分配 transport 状态、方向性 rekey、精确 KEX 方法额度、strict-kex 与 EXT_INFO 顺序约束。
- 无缓冲 transport core、发送背压事务、接收提交边界与 NEWKEYS cipher 原子激活。
- XRT TCP 动态缓冲适配、零复制发送接管、硬背压重试与单 packet 按需连续化。
- RFC 4252 可扩展 USERAUTH 请求、none 探测及公共响应消息。
- RFC 4252 普通密码、旧/新密码和密码更改提示。
- RFC 4252 publickey probe、PK_OK、签名请求和签名原文。
- RFC 4256 keyboard-interactive 请求、无上限 challenge/response 迭代。
- RFC 4252 hostbased 请求、签名原文和严格 DNS 主机名校验。
- 认证超时、尝试、交互轮次、消息和字节的无时钟资源 guard。
- 无网络、无固定 payload 缓冲的 client/server USERAUTH 事务编排。
- 保留未知字段的 RFC 4254 全局请求与成功/失败响应。
- RFC 4254 channel open、窗口调整、数据、生命周期与可扩展请求报文。
- 无固定接收缓冲的双向 channel 窗口、消费阈值与动态额度管理。
- Shell、exec、subsystem、env、window、signal、break 与 exit request 便利层。
- 独立裁剪、无固定 mode 数量上限的 PTY 与 terminal modes。
- 双向 EOF/CLOSE、在途数据和 channel slot 回收生命周期状态。
- 无数据缓冲的单 channel open、窗口、request 能力与关闭组合核心。
- 动态有界 channel DATA/stderr 缓冲、五种发送所有权与窗口提交/回滚。
- Remote forwarding 控制面与 direct/forwarded TCP/IP channel open。
- Global/per-channel 顺序回复关联与可迁移调用方 token FIFO。
- 无网络、无固定 channel 表的 RFC 4254 双端事务编排与调用方存储路由。
- 按需分配的动态 channel 集合、稳定本地编号、connection resolver 与可增长回复 FIFO。
- 无 socket、无固定收发缓冲的连接级 KEX/auth/connection 生命周期、严格写绑定与扩展消息路由。
- TCP transport 与连接级协议的一致 identification/packet 提交、分块读取和无复制上层路由。
- callback、future、同步和协程可共享的统一会话动作推导，不增加事件队列或隐藏底层状态。
- 明文零分配、加密包按需工作区和 ECDH_REPLY 主机公钥自动扩容的动态会话读取器。
- client/server 共用的 callback Stream 驱动、自动输出提交、HOLD、内存重试与 TCP 背压串行化。
- 不拥有网络对象的客户端动作核心、默认拒绝的主机信任、动态敏感输出、none 探测和可扩展认证器。
- 不创建隐藏 Engine 的组合 Stream 客户端、动态控制报文 scratch、硬上限全局回复 FIFO 与结构化错误。
- 直接复用 XRT Resolver、DNS 缓存、Happy Eyeballs、deadline、cancel 和统计的可裁剪客户端 Dial。
- 按需 waiter、精确 reply token、跨线程取消和 Close/Clear 收口的统一 Future 等待层。
- 零额外堆分配的 Ed25519 publickey 客户端 provider，直接发送签名请求且保留通用认证扩展点。
- 自定义 channel open、对端主动 channel 接受/拒绝、DATA flush、窗口调整、EOF 和 CLOSE 基础事务。
- 可独立裁剪的 session/env/exec/shell/subsystem/signal/break、PTY/resize 和 direct/forwarded-tcpip helper。
- 所有失败保持 reader、writer 和输出参数不变。

wire 与 plain packet 层不分配内存，只依赖 XRT `core`。安全 padding 与 AES-GCM 是独立裁剪层，
分别只增加 `random_secure` 与 `crypto_aes_gcm`。它们不携带网络、字符串容器或旧运行时，因此
可以直接用于协议解析器、模糊测试和资源受限构建。packet 不隐藏不安全的零 padding；真实
transport 必须提供安全随机或安全播种的会话 PRNG。

## 裁剪

```c
#define XSSH_MODULE_SSH_WIRE
#include <xssh.h>
```

选择当前全部已迁移能力：

```c
#define XSSH_MODULE_XSSH
#include <xssh.h>
```

单头实现使用 `XSSH_IMPLEMENTATION`。声明单头与实现单头由统一工具生成，不维护第二份源代码。
生成的 `XSSH_FEATURE_WIRE` 和 `XSSH_FEATURE_PACKET` 分别表示对应能力可用，
`XSSH_FEATURE_SSH` 表示当前 xssh 聚合闭包已经完整启用；功能代码应读取 feature 宏，
不应自行定义它们。

## 验证

```text
python tools/generate_extension_features.py extlibs/xssh/config/modules.json
python tools/amalgamate.py --manifest extlibs/xssh/config/modules.json
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_wire --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_wire --trim-only --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_packet_random --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_packet_aes_gcm --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_packet_codec --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_packet_codec_random --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_kexinit --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_kexinit_random --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_kex_ecdh --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_kex_sha256 --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_kex_curve25519_random --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_hostkey --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_hostkey_ed25519 --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_key_text --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_known_host --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_known_host_hash --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_known_host_db --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_fingerprint --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_private_key --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_private_key_pem --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_private_key_ed25519 --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_transport_message --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_transport_rekey --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_transport_state --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_transport_core --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_kex_session --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_kex_exchange --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_kex_exchange_random --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_auth_message --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_auth_password --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_auth_publickey --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_auth_keyboard --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_auth_hostbased --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_auth_guard --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_auth_session --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_connection_message --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_channel_message --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_channel_window --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_channel_request --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_channel_pty --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_channel_state --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_channel_core --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_channel_io --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_connection_session --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_channels --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_channels --trim-only --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_forward_message --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_reply_queue --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_session_core --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_session_core_random --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_session_tcp --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_session_reader --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_session_stream --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_session_tcp_random --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_client_core --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_client_core --trim-only --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_client_auth_ed25519 --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_client_auth_ed25519 --trim-only --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_client --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_client --trim-only --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_client_dial --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_client_dial --trim-only --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_client_future --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_client_future --trim-only --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_client_session --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_client_session --trim-only --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_client_forward --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_client_forward --trim-only --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_client_pty --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_client_pty --trim-only --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xssh/config/modules.json --suite ssh_client_runtime_tests --no-single --cflag=-Werror
python tools/check_api_docs.py --manifest extlibs/xssh/config/modules.json
python tools/check_release_maturity.py --release --manifest extlibs/xssh/config/modules.json
python tools/measure_performance.py --config extlibs/xssh/config/performance_profiles.json --manifest extlibs/xssh/config/modules.json --profiles ssh_wire --smoke
python tools/measure_size.py --config extlibs/xssh/config/size_profiles.json --manifest extlibs/xssh/config/modules.json --profiles ssh_wire
```

历史单头、测试和示例位于 `dev/archive/ssh_legacy_20260816/xssh`，不进入产品清单或发布包。
