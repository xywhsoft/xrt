# xssh 完整实现 Spec

状态标记：

- `[ ]` 未开始
- `[~]` 进行中
- `[X]` 已完成

本文档是 `xrt` 官方扩展库 `xssh` 的主跟踪清单。`xssh` 不按 MVP 拆分目标；所有列入目标范围的能力都按完整 SSH-2 Client 栈设计，但实现时必须按依赖顺序逐项闭环、逐项测试。

## 0. 当前状态

- [X] 已确认 `xssh` 放在 `D:\Git\xrt\extlibs\xssh`，遵循 extlibs 独立目录与单头文件优先约定。
- [X] 已确认 XRT core 不因 SSH 兼容需求无边界膨胀；SSH 专属算法与格式优先留在 `xssh` 内部。
- [X] 已确认 XUI2 Terminal 可作为原生 SSH shell 示例前端，代码参考 `D:\git\xge\dev\xui2`。
- [X] 已生成本 SPEC，后续所有实现和测试进度以本文档为准。
- [~] 已创建基础 `xssh.h`、wire/packet/crypto/KEX/transport/auth/channel/known_hosts/mock transcript 测试程序、CLI 示例、XUI2 Terminal mock 示例和构建脚本；已接入真实 xnetstream TCP + SSH identification exchange + curve25519/Ed25519/AES-GCM KEX/NEWKEYS + password/signer-callback/OpenSSH-v1-Ed25519 publickey/keyboard-interactive USERAUTH + session/exec/subsystem/direct-tcpip/forwarded-tcpip 数据 transport，`xssh_forward` 单连接本地监听数据泵和 mock self-test 已接入，真实 OpenSSH interop、加密私钥/RSA/ECDSA/agent 和多连接转发管理待接入。

## 1. 产品边界

- [ ] 实现 SSH-2 client library，目标协议族以 RFC 4251 / 4252 / 4253 / 4254 及 OpenSSH 主流互操作为准。
- [~] 支持交互式 shell；已完成 mock runtime shell 数据路径、真实 PTY/shell request/response、加密本地 socket shell echo roundtrip，OpenSSH live shell 验证待补。
- [~] 支持 exec command；已完成真实 session channel exec request、stdout/stderr/exit-status/EOF/CLOSE 基础闭环，OpenSSH live interop 待验证。
- [~] 支持 subsystem channel，供后续 `xsftp` 或上层协议复用；已完成 subsystem request 消息层、真实 request/response 基础路径和本地加密 socket raw byte channel roundtrip，live interop 待验证。
- [~] 支持 PTY request、terminal modes、window-change；已完成 PTY/window-change 消息发送路径、固定容量 terminal modes 编码 helper、TTY_OP_END 结束符编码和消息测试，OpenSSH live shell 验证待补。
- [~] 支持 env request、signal、break、exit-status、exit-signal；已完成消息层编解码/解析、真实 env success/failure request/reply、signal/break 发送、exit-status 保存和 exit-signal runtime 保存，OpenSSH live 兼容验证待补。
- [X] 支持 channel window flow control；已完成接收窗口消耗、自动 WINDOW_ADJUST 补充、写入超过 remote window 时的固定容量队列和 WINDOW_ADJUST 后自动恢复发送。
- [~] 支持 local forwarding / direct-tcpip；已完成 direct-tcpip 消息层、mock runtime API、真实 CHANNEL_OPEN 控制面、CHANNEL_OPEN_FAILURE 拒绝路径、direct-tcpip 加密数据面 roundtrip、`xssh_forward` 单连接本地监听/accept/双向数据泵和 mock self-test，多连接管理和 OpenSSH live 验证待补。
- [~] 支持 remote forwarding / forwarded-tcpip；已完成 tcpip-forward/cancel 消息层、真实 tcpip-forward/cancel 控制面、forwarded-tcpip open 解析、runtime 接收分发和本地 socket 数据面 roundtrip，OpenSSH live remote forwarding 与多连接管理待接入。
- [X] 支持 keepalive、ignore、debug、disconnect；已完成 ignore/debug 收发路径、`keepalive@openssh.com` 请求、REQUEST_FAILURE 视为存活回复、disconnect 解析和主动 disconnect packet 发送。
- [ ] 支持 rekey，按字节数、包数、时间三种阈值触发。
- [X] 不在 `xssh` 内实现 SFTP 文件协议；只提供 subsystem byte channel。
- [ ] 不在 `xssh` 内实现 SSH server；如需要 server，另建 `xsshd` 扩展库。

## 2. 设计原则

- [ ] 代码以简单、直接、可审计为第一优先级，避免过早抽象和大而全的宏系统。
- [ ] 对外 API 使用稳定 C ABI 风格：显式 init/free、显式 result、显式 ownership。
- [ ] 内部模块分层清晰：wire、packet、transport、auth、connection、channel、adapter 不交叉访问。
- [ ] 所有协议状态机必须有明确枚举值，不使用隐式布尔组合表达复杂状态。
- [ ] 所有解析函数必须同时返回“已消费字节数”和错误阶段，方便定位半包、坏包和互操作问题。
- [ ] 内部辅助函数尽量 `static`，对外只暴露必要 API。
- [ ] 结构体字段命名保持 XRT/extlibs 风格，避免引入新的大写/驼峰混杂风格。
- [ ] 中文注释优先解释协议约束、状态迁移、安全边界和不直观的兼容逻辑。
- [ ] 不为显而易见的赋值写空注释；注释要说明“为什么这样做”，不是重复“做了什么”。
- [ ] 所有密钥、session key、password、passphrase、签名中间缓冲必须在释放前清零。
- [ ] 日志不得输出 password、private key、passphrase、session key、完整认证报文。

## 3. 目录结构

- [X] 创建 `extlibs/xssh/xssh.h`，作为主入口单头文件。
- [X] 创建 `extlibs/xssh/README.md`，说明功能、构建、最小示例和安全默认值。
- [X] 创建 `extlibs/xssh/xssh-spec.md`，作为主跟踪清单。
- [X] 创建 `extlibs/xssh/build_test.bat`。
- [X] 创建 `extlibs/xssh/xssh_wire_test.c`。
- [X] 创建 `extlibs/xssh/xssh_packet_test.c`。
- [X] 创建 `extlibs/xssh/xssh_crypto_test.c`。
- [X] 创建 `extlibs/xssh/xssh_kex_test.c`。
- [X] 创建 `extlibs/xssh/xssh_transport_test.c`。
- [X] 创建 `extlibs/xssh/xssh_auth_test.c`。
- [X] 创建 `extlibs/xssh/xssh_channel_test.c`。
- [X] 创建 `extlibs/xssh/xssh_known_hosts_test.c`。
- [X] 创建 `extlibs/xssh/xssh_mock_server_test.c`。
- [X] 创建 `extlibs/xssh/xssh_runtime_test.c`。
- [X] 创建 `extlibs/xssh/xssh_interop_test.c`。
- [X] 创建 `extlibs/xssh/examples/xssh_exec.c`。
- [X] 创建 `extlibs/xssh/examples/xssh_shell.c`。
- [X] 创建 `extlibs/xssh/examples/xssh_forward.c`。
- [X] 创建 `extlibs/xssh/examples/xssh_terminal_xui2/main.c`。
- [X] 创建 `extlibs/xssh/examples/xssh_terminal_xui2/build.bat`。

## 4. Public API

### 4.1 基础类型

- [X] 定义 `xssh_session_t`。
- [X] 定义 `xssh_channel_t`。
- [X] 定义 `xssh_known_hosts_t`。
- [X] 定义 `xssh_config_t`。
- [X] 定义 `xssh_result_t`。
- [X] 定义 `xssh_auth_t`。
- [X] 定义 `xssh_pty_t`。
- [X] 定义 `xssh_event_t`。
- [X] 定义 `xssh_forward_t`。
- [X] 定义错误码枚举 `XSSH_ERR_*`。
- [X] 定义阶段枚举 `XSSH_STAGE_*`，覆盖 connect、banner、kex、hostkey、newkeys、auth、channel、disconnect。

### 4.2 初始化与生命周期

- [X] 实现 `xsshConfigInit`。
- [X] 实现 `xsshResultInit`。
- [X] 实现 `xsshAuthInit`。
- [X] 实现 `xsshPtyInit`。
- [~] 实现 `xsshConnect`；已完成 mock transport 路径、真实 xnetstream TCP 连接、client/server identification exchange、curve25519/Ed25519/AES-GCM KEX/NEWKEYS、known_hosts policy、password USERAUTH、signer callback publickey USERAUTH、OpenSSH v1 未加密 Ed25519 私钥 USERAUTH、keyboard-interactive USERAUTH、result 填充与 connected/authenticated 事件，加密私钥/RSA/ECDSA/agent 待接入。
- [~] 实现 `xsshDisconnect`；已完成 mock runtime 断开事件、真实 SSH disconnect packet best-effort 发送和 xnetstream close，graceful close wait 细节待补。
- [X] 实现 `xsshFree`；已完成 session/auth 清零、mock 释放、真实 xnetstream/engine/cond/mutex 释放和 packet codec key 清零。
- [~] 实现 `xsshGetLastResult` 或等效结果查询；已完成 mock runtime result 查询和真实 connect/banner/KEX/NEWKEYS/password AUTH/session channel 阶段填充，后续转发和更多 channel 错误阶段待补全。
- [~] 实现连接超时、读写超时和握手总超时；已覆盖 xnetstream connect、banner recv、KEX packet recv 和 USERAUTH packet recv 超时，完整握手总超时待 channel 状态机接入。

### 4.3 认证 API

- [X] 实现 password auth；已完成 mock connect password success、真实加密 USERAUTH password success/failure 和事件/result 填充。
- [~] 实现 publickey auth；已完成 signer callback + public key blob 和 OpenSSH v1 未加密 Ed25519 私钥的 probe/PK_OK/signed request 认证循环、本地加密 socket 验签测试，加密私钥/RSA/ECDSA/agent 待接入。
- [X] 实现 keyboard-interactive auth；已完成真实加密 USERAUTH_INFO_REQUEST/USERAUTH_INFO_RESPONSE 循环、回调接口和本地 socket 双 prompt 成功认证。
- [X] 实现 `none` auth 探测，用于查询服务端允许的认证方式；已在无凭据时发送 none request，并在 USERAUTH_FAILURE 中填充 `xsshresult.sAuthMethods`。
- [X] 支持 signer callback，允许上层接入 ssh-agent、Pageant 或自管密钥；当前回调输入签名原文，返回裸签名，xssh 负责包装 SSH signature blob。
- [X] 支持私钥文件路径输入；已支持 OpenSSH v1 未加密 Ed25519，测试覆盖临时文件解析。
- [X] 支持内存私钥输入；已支持 OpenSSH v1 未加密 Ed25519，测试覆盖内存 PEM 和真实 USERAUTH。
- [ ] 支持 passphrase callback。

### 4.4 Channel API

- [X] 实现 `xsshOpenSessionChannel`；已完成 mock runtime session channel 和真实 CHANNEL_OPEN/confirmation/failure 循环。
- [~] 实现 `xsshChannelRequestPty`；已完成 mock runtime API、消息 helper、显式 terminal modes 设置/更新/清空、真实 request/response 路径和本地 socket shell echo roundtrip，OpenSSH live 验证待补。
- [~] 实现 `xsshChannelRequestShell`；已完成 mock runtime shell 数据路径、真实 request/response 路径和本地 socket shell echo roundtrip，OpenSSH live shell 验证待补。
- [X] 实现 `xsshChannelRequestExec`；已完成 mock runtime exec/stdout/stderr/exit-status 和真实 exec request/response 基础路径。
- [~] 实现 `xsshChannelRequestSubsystem`；已完成 request 消息 helper、mock runtime API、真实 request/response 路径和本地 socket raw byte channel roundtrip，subsystem live 验证待补。
- [~] 实现 `xsshChannelSetEnv`；已完成 env request 消息 helper、mock runtime API、本地加密 socket success/failure 路径和 `CHANNEL_FAILURE` result 校验，OpenSSH AcceptEnv 兼容测试待补。
- [X] 实现 `xsshChannelWrite`；已完成 mock runtime 写入/回显数据路径、真实 CHANNEL_DATA 发送、按 remote window/max packet 分包、窗口耗尽后的固定容量内部队列和 WINDOW_ADJUST 后自动 flush。
- [X] 实现 `xsshChannelRead`；已完成 mock runtime 与真实 stdout 缓冲读取。
- [X] 实现 `xsshChannelReadStderr` 或 extended-data 读取能力；已完成 mock runtime 与真实 stderr 缓冲读取。
- [X] 实现 `xsshChannelResizePty`；已完成 mock runtime API 和真实 window-change 发送路径。
- [X] 实现 `xsshChannelSendSignal`；已完成 signal request 消息 helper、mock runtime API 和真实发送路径。
- [X] 实现 `xsshChannelSendBreak`；已完成 break request 消息 helper、mock runtime API 和真实发送路径。
- [X] 实现 `xsshChannelSendEof`；已完成 mock runtime EOF event 和真实 CHANNEL_EOF 发送。
- [X] 实现 `xsshChannelClose`；已完成 mock runtime close event、真实 CHANNEL_CLOSE 发送/接收、远端 close 自动 best-effort 回包，以及本端/远端 close 双向确认后释放 channel slot。
- [X] 实现 exit-status 和 exit-signal 查询；已完成 request 消息解析 helper、mock/真实 exit-status 保存、exit-signal runtime 保存和 `xsshChannelGetExitSignal` 查询。

### 4.5 事件与非阻塞 API

- [~] 实现 `xsshPoll`；已完成 mock runtime no-op poll 和真实非阻塞 channel packet drain，阻塞/可取消 pump 封装待补。
- [X] 实现 `xsshNextEvent`；已完成 mock runtime event queue 和真实 channel/auth/disconnect event 注入。
- [X] 实现 channel data event；已完成消息层编解码、mock runtime event queue 和真实事件泵。
- [X] 实现 channel extended data event；已完成消息层编解码、mock runtime event queue 和真实事件泵。
- [X] 实现 channel eof/close event；已完成消息层编解码、mock runtime event queue 和真实事件泵。
- [X] 实现 auth success/failure event；已完成 mock authenticated event 和真实 password USERAUTH success/failure 事件。
- [~] 实现 disconnect/error event；已完成 mock disconnect event、真实 USERAUTH failure error event、channel DISCONNECT event 和 result disconnect reason 保存，底层 transport close reason 统一映射待补。
- [ ] 实现后台 pump 线程可选封装，服务 XUI Terminal 等 UI 场景。
- [ ] 实现可取消连接与可取消 channel 操作。

### 4.6 端口转发 API

- [~] 实现 direct-tcpip channel；已完成 direct-tcpip channel open 消息层、mock runtime API、真实 CHANNEL_OPEN 控制面、真实加密数据面 roundtrip、remote window 归零后的写入队列和自动 flush，OpenSSH live 验证待补。
- [~] 实现本地监听转发封装；已完成 `xssh_forward` 单连接本地监听、accept、direct-tcpip 打开、双向数据泵、mock self-test 和 live CLI 参数，多连接后台服务封装待补。
- [X] 实现 tcpip-forward request；已完成 global request 编解码、request-success port 解析、mock runtime API 和真实 transport 控制面。
- [X] 实现 forwarded-tcpip 接收；已完成 channel open 消息层、extra 解析、真实 transport dispatch、open confirmation、channel open event 和本地 socket 数据面 roundtrip。
- [X] 实现 cancel-tcpip-forward；已完成 global request 编解码、mock runtime API 和真实 transport 控制面。
- [X] 测试转发关闭、半关闭、远端拒绝、窗口耗尽；已覆盖 direct-tcpip 数据 roundtrip、local socket 半关闭触发 channel EOF、channel close、CHANNEL_OPEN_FAILURE 远端拒绝、remote window 归零和 WINDOW_ADJUST 后续写。

## 5. Wire 与 Packet 层

- [X] 实现 SSH `byte` / `boolean` / `uint32` / `uint64` 编解码。
- [X] 实现 SSH `string` 编解码，支持二进制内容。
- [X] 实现 SSH `name-list` 编解码；支持写入、基础合法性检查、精确包含判断、重复项检测和首个共同算法选择。
- [X] 实现 SSH `mpint` 编解码；已支持正数、零、最高位补零、负数 two's-complement 编码、signed canonical 解析与非规范符号扩展拒绝。
- [X] 实现 banner 读写，兼容服务端 banner 前的说明行。
- [X] 实现 packet payload、padding、packet_length、padding_length。
- [X] 实现 seqno 递增与 wrap 行为测试。
- [X] 实现未加密阶段 packet 读写。
- [~] 实现加密阶段 packet 读写；已完成 AES-GCM AEAD packet codec、plain/AES-GCM packet codec 切换、真实 KEX/NEWKEYS、password USERAUTH 与 session/exec channel pump 接入，CTR/HMAC 待补。
- [ ] 实现 MAC-then-encrypt 和 encrypt-then-mac 的差异路径。
- [~] 实现 AEAD packet 路径；已完成 AES-GCM `packet_length` AAD、密文、16 字节 tag、nonce counter 递增与解密缓冲 ownership，OpenSSH chacha AEAD 待补。
- [~] 实现坏包长度、坏 padding、坏 MAC、截断包防护；坏包长度、坏 padding、截断包、AES-GCM bad tag 已覆盖，ETM/HMAC bad MAC 待补。
- [X] 限制最大 packet 长度，默认拒绝异常大包。

## 6. Transport 与 KEX

- [X] 实现 client/server identification exchange。
- [X] 实现 KEXINIT 构建。
- [X] 实现 KEXINIT 解析。
- [~] 实现算法协商，按客户端偏好和服务端列表求交集；已完成 KEXINIT 协商 helper、AEAD cipher 隐式 MAC 处理、CTR/HMAC 选择、无共同算法拒绝和真实 KEX 接入，rekey/strict-kex 待补。
- [X] 实现 first_kex_packet_follows 处理；已完成字段构建/解析、错误猜测首包跳过判断、真实 KEX 状态机跳过和本地 socket runtime 覆盖。
- [~] 实现 exchange hash H 计算；已完成 curve25519/ecdh-style SHA256 hash 输入拼接、digest helper 和真实 KEX 接入，SHA384/P-384 待补。
- [X] 实现 session id 固定为首次 KEX 的 H。
- [~] 实现 NEWKEYS 前后密钥切换；已完成 packet codec 层 plain→AES-GCM 切换、seqno 连续性测试和真实 KEX/NEWKEYS 接入，rekey 待补。
- [ ] 实现 rekey 状态机。
- [X] 实现 disconnect reason 解析和发送。
- [X] 实现 debug / ignore message 处理。
- [ ] 实现 strict kex 兼容开关，若后续需要 OpenSSH strict-kex 扩展则补充。

## 7. 算法清单

### 7.1 KEX

- [~] 支持 `curve25519-sha256`；已完成 X25519 shared secret、ECDH_INIT/ECDH_REPLY payload、SHA256 exchange hash helper 和真实 KEX 接入，rekey 待补。
- [~] 支持 `curve25519-sha256@libssh.org`；与 `curve25519-sha256` 共用当前 helper，真实 KEX 可协商，rekey 待补。
- [ ] 支持 `ecdh-sha2-nistp256`。
- [ ] 支持 `ecdh-sha2-nistp384`。
- [ ] 支持 `diffie-hellman-group14-sha256`，兼容项，默认可启用。
- [ ] 支持 `diffie-hellman-group16-sha512`，兼容项，默认可启用。
- [ ] 不支持 `diffie-hellman-group1-sha1`，除非未来有明确旧设备需求。

### 7.2 Host Key

- [~] 支持 `ssh-ed25519`；已完成 host key blob 解析、signature blob 解析、真实 KEX exchange hash 验证、publickey auth 签名原文构建、Ed25519 signed request 验证、signer callback 和 OpenSSH v1 未加密私钥认证循环，OpenSSH live interop 待验证。
- [ ] 支持 `rsa-sha2-256`。
- [ ] 支持 `rsa-sha2-512`。
- [ ] 支持 `ecdsa-sha2-nistp256`。
- [ ] 支持 `ecdsa-sha2-nistp384`。
- [ ] `ssh-rsa` 默认关闭，仅兼容模式启用。

### 7.3 Cipher

- [X] 支持 `aes128-gcm@openssh.com`；已完成 XRT AES-128-GCM 基础向量、roundtrip、tag 篡改拒绝、SSH packet AEAD codec、packet codec 切换、真实 NEWKEYS、password USERAUTH、session/exec channel pump 和 runtime result cipher 校验。
- [X] 支持 `aes256-gcm@openssh.com`；已完成 XRT AES-256-GCM 基础向量、roundtrip、tag 篡改拒绝、SSH packet AEAD codec、真实 NEWKEYS、password USERAUTH 和 runtime result cipher 校验。
- [ ] 支持 `chacha20-poly1305@openssh.com`，内部实现，不进入 XRT core；已验证 RFC 8439 标准 AEAD 底层向量，OpenSSH 专用 packet 构造待实现。
- [ ] 支持 `aes128-ctr`。
- [ ] 支持 `aes256-ctr`。
- [ ] 不支持 CBC/3DES，除非未来有明确旧设备需求。

### 7.4 MAC

- [~] 支持 `hmac-sha2-256`；已完成 RFC 4231 HMAC-SHA256 向量测试，SSH packet MAC 覆盖范围待接入。
- [~] 支持 `hmac-sha2-512`；已完成 RFC 4231 HMAC-SHA512 向量测试，SSH packet MAC 覆盖范围待接入。
- [ ] 支持 `hmac-sha2-256-etm@openssh.com`。
- [ ] 支持 `hmac-sha2-512-etm@openssh.com`。
- [ ] `hmac-sha1` 默认关闭，仅兼容模式启用。

### 7.5 Compression

- [X] 支持 `none`；已覆盖 KEX 默认列表、显式协商和真实 runtime KEX。
- [ ] 暂不启用 `zlib` / `zlib@openssh.com`。
- [ ] 若未来启用压缩，必须先完成压缩炸弹限制、最大解压窗口和内存上限测试。

## 8. Key 与 Known Hosts

- [X] 解析 OpenSSH public key 行。
- [X] 解析 authorized_keys 常见格式，忽略前置 options；已覆盖普通 public key、无空格 options 前缀和带 quoted 空格的 `command=` 场景。
- [X] 解析 known_hosts 普通 host。
- [X] 解析 known_hosts `[host]:port`。
- [X] 解析 known_hosts hashed host；已支持 OpenSSH `|1|salt|hash` HMAC-SHA1 格式，覆盖默认端口和 `[host]:port`。
- [X] 支持 known_hosts 多 key 类型匹配；检查时先按 host 和算法筛选，只有同 host 且同算法的记录才参与 blob 比较，避免 RSA/ECDSA 等不同类型记录误触发 Ed25519 mismatch。
- [X] 支持 host key SHA256/base64 指纹显示；已输出 OpenSSH 风格 `SHA256:<base64-no-padding>` 并写入 `xsshresult.sHostKeyFingerprint`。
- [X] 支持 MD5 hex 指纹显示，仅用于兼容展示；MD5 实现限制在 `xssh` 内部，不进入 XRT core，也不参与安全决策。
- [X] 实现 strict host key policy；已完成 known_hosts text helper 层和真实 connect 流程 strict unknown 拒绝。
- [X] 实现 accept-new host key policy；已完成候选 known_hosts 记录生成、父目录创建、connect 流程追加、短时 `.lock` 独占写保护和锁内重读，避免并发首次接受同一 host key 时重复追加。
- [X] 实现 insecure-accept-any policy，必须显式配置并在 result 中标记；已完成 helper 层与 connect 流程显式放行，并通过 `xsshresult.bHostKeyInsecure` 标记风险。
- [X] 检测 host key mismatch，并返回专用错误阶段；已新增 `XSSH_ERR_HOSTKEY_MISMATCH` 并接入 connect result。
- [X] 支持 OpenSSH private key v1 未加密 Ed25519；已覆盖 PEM 解析、文件路径输入、内存输入和本地加密 socket publickey USERAUTH。
- [ ] 支持 OpenSSH private key v1 加密 Ed25519，bcrypt_pbkdf 放在 `xssh` 内部。
- [ ] 支持 RSA private key signing。
- [ ] 支持 ECDSA private key signing。
- [ ] 所有私钥解析错误必须区分格式错误、passphrase 错误、不支持算法。

## 9. UserAuth

- [X] 实现 service request `ssh-userauth`。
- [X] 实现 `none` request，读取 allowed methods；已完成 none request、failure allowed-methods 编解码、无凭据时的真实认证循环和 `xsshresult.sAuthMethods` 结构化输出。
- [X] 实现 password request。
- [X] 实现 publickey probe。
- [X] 实现 publickey signed request；已覆盖签名原文 `session_id || USERAUTH_REQUEST` 构建。
- [X] 实现 keyboard-interactive challenge/response；已覆盖单 prompt、多 prompt、prompt 数量上限和本地加密 socket USERAUTH_SUCCESS。
- [X] 处理 partial success。
- [X] 处理服务端 method list 更新；已完成 failure method-list 解析，并在真实 password/none 认证失败时写入 result。
- [X] 处理认证次数上限；已限制 USERAUTH 阶段服务端包数和 keyboard-interactive 轮数，避免认证阶段被无限拖住。
- [X] 处理 auth banner。
- [~] 测试密码错误、公钥错误、passphrase 错误、账号不存在、服务端断开；已覆盖真实 socket password failure。

## 10. Connection 与 Channel

- [X] 实现 global request 分发；已完成 tcpip-forward/cancel 编解码、success/failure 解析、真实 request/response 控制面，以及服务端主动 `GLOBAL_REQUEST` 的解析和 `REQUEST_FAILURE` 兼容回包。
- [X] 实现 session channel open；已完成消息层编解码和真实 runtime 状态机。
- [X] 实现 direct-tcpip channel open；已完成 channel open 消息层、extra 解析、mock runtime API 和真实 transport 控制面。
- [X] 实现 forwarded-tcpip channel 接收；已完成 runtime channel 分配、open confirmation、metadata 保存、事件分发和数据读取。
- [X] 实现 channel open confirmation/failure。
- [X] 实现 channel data。
- [X] 实现 channel extended data。
- [X] 实现 channel window adjust。
- [X] 实现 channel eof。
- [X] 实现 channel close。
- [X] 实现 request success/failure。
- [X] 实现 exit-status；已完成消息层解析和 mock/真实 runtime 状态保存。
- [X] 实现 exit-signal；已完成消息层解析、runtime 状态保存和对外查询。
- [X] 实现多 channel 并发读写；已覆盖同一真实加密 socket 连接上的两个 session channel、交错数据事件、独立 stdout 缓冲、exit-status 和 close 事件。
- [X] 实现 channel local window 自动补充。
- [X] 实现写入超过 remote window 时的排队与恢复；已完成固定容量内部写队列、WINDOW_ADJUST 后自动 flush 和 direct-tcpip 本地 socket 覆盖。

## 11. XRT 集成策略

- [~] 优先复用 XRT 内存、字符串、base64、hex、随机数、hash、HMAC、HKDF、AES-GCM、X25519、ECDH、Ed25519、RSA/ECDSA verify；已接入并测试 HMAC-SHA2、AES-GCM、X25519、Ed25519 verify 与 base64/key blob 路径。
- [~] 网络层优先使用当前主线 xnet-v2 / xnetstream 能力；真实主机 connect、identification exchange、KEX/NEWKEYS、password USERAUTH、session/exec/direct-tcpip channel 和转发控制面 packet I/O 已使用 xnetstream；`xssh_forward` 示例本地 listener 先使用轻量原生 socket，后续库级多连接转发管理再收敛到 xnet listener。
- [ ] 异步封装可复用 XRT future；交互式 UI 场景优先提供 poll/pump，不强迫使用阻塞 API。
- [ ] 不把 SSH 专属 wire/parser 放进 `xrt.h`。
- [ ] 不把 OpenSSH key、known_hosts、agent 协议放进 `xrt.h`。
- [ ] AES-CTR、HMAC-SHA1、bcrypt_pbkdf、OpenSSH chacha20-poly1305 先放 `xssh` 内部。
- [ ] 若实现中发现 XRT crypto/network/future bug，允许直接修复 XRT 仓库代码，并在本 SPEC “跨仓库修复记录”登记。

## 12. XGE / XUI2 Terminal 示例

- [X] 示例程序使用 `D:\git\xge\dev\xui2` 的 XGE + XUI Terminal 控件。
- [X] 示例不得调用 XUI2 现有 OpenSSH-backed SSH adapter；必须走原生 `xssh`。
- [X] `xssh` channel stdout/stderr data 写入 `xuiTerminalWrite`。
- [X] Terminal input callback 写入 `xsshChannelWrite`。
- [X] Terminal resize callback 调用 `xsshChannelResizePty`。
- [X] UI frame loop 每帧调用 `xsshPoll(session, 0)` 并 drain event queue。
- [X] 后台 pump 线程不得直接访问 XUI widget；当前示例不创建后台 pump，后续如加入 pump 只能投递线程安全事件。
- [X] 示例支持 `--mock`，无真实 SSH 服务也能跑渲染和输入路径。
- [X] 示例支持 `--host`、`--port`、`--user`、`--identity`、`--password-env`。
- [X] 示例支持 `--known-hosts PATH`，便于 live smoke 使用临时 known_hosts 文件。
- [X] 示例支持 `--accept-new`，默认仍为 strict host key。
- [X] 示例支持 `--frames N` 和 `--seconds N`，便于自动 smoke test。
- [X] XUI2 Terminal mock smoke：`.\bin\xssh_terminal_xui2.exe --mock --frames 5` 已通过，摘要 `connect=1 channel=1 layout=1 dynamic=1 input=15 resize=2 events=4 bytes=17`。
- [ ] 若发现 XUI Terminal 输入、resize、渲染或 session API bug，允许修复 XGE 仓库代码，并在本 SPEC “跨仓库修复记录”登记。

## 13. 测试总原则

- [ ] 每个协议层先有纯内存单测，再接 mock server，再接真实互操作。
- [ ] 所有 parser 必须覆盖成功、半包、截断、越界、非法长度、非法枚举、重复字段。
- [ ] 所有状态机必须覆盖正常路径、错误路径、远端主动断开、本地主动取消。
- [~] 所有网络测试必须有超时，禁止无限等待；本地 KEX transport 测试已设置 3000ms/5000ms 超时，后续真实 interop/forwarding 继续补齐。
- [~] 所有真实 SSH 互操作测试必须由环境变量显式启用，不能默认依赖外部服务；`xssh_interop_test` 已默认 skip，并支持显式 `XSSH_LIVE_PASSWORD` password auth 与 exec channel smoke；2026-06-11 对用户提供服务器重试仍在 banner 阶段被远端关闭（`stage=2 error=-11`），待服务端 SSH banner 可用后再验证。
- [~] 测试输出必须包含失败阶段、服务端 banner、协商算法、错误码；`xssh_interop_test` 和 CLI/XUI 示例已统一打印 stage/error/disconnect/banner/kex/hostkey/cipher/MAC/auth/fingerprint/text，纯单元测试失败仍保持短 case 名。
- [ ] 不在日志中输出敏感数据；测试夹具也要遵守。
- [X] 所有示例程序都要有 `--help`；`xssh_exec`、`xssh_shell`、`xssh_forward` 和 `xssh_terminal_xui2` 已完成。
- [~] 所有可自动运行的测试都必须能在 Windows PowerShell 下执行；当前 `build_test.bat`、本地 socket password/publickey/private-key/keyboard-interactive USERAUTH、first_kex_packet_follows 错误猜测跳过、auth packet flood 拒绝、AES-GCM bad tag/坏 packet_length/半包关闭/乱序 channel data 异常注入、keepalive/ignore/debug、session shell echo/exec/subsystem raw channel、forward control、direct-tcpip reject/data roundtrip、forwarded-tcpip data roundtrip、`xssh_forward --mock --self-test` 和 XUI2 Terminal mock smoke 已通过，真实 channel interop 待可用服务器验证。

## 14. Unit Test 细目

### 14.1 Wire Tests

- [X] `uint32` / `uint64` 大端编码测试。
- [X] `string` 二进制内容测试。
- [X] `string` 空值和超长拒绝测试；已覆盖空值写入、超出 SSH uint32 长度上限拒绝和 writer 原子性。
- [X] `name-list` 空列表、单项、多项、重复项测试。
- [X] `mpint` 零值测试。
- [X] `mpint` 正数最高位补零测试。
- [X] `mpint` 非规范编码拒绝测试。
- [X] `mpint` 负数 two's-complement 与非规范符号扩展拒绝测试。
- [X] banner CRLF / LF / 说明行兼容测试。

### 14.2 Packet Tests

- [X] 未加密 packet roundtrip。
- [X] padding 长度边界测试。
- [~] payload 长度边界测试；已覆盖写入容量不足和 max packet 拒绝，超大 payload 专门夹具待补。
- [X] seqno 递增测试。
- [X] bad packet length 拒绝。
- [X] bad padding length 拒绝。
- [~] bad MAC 拒绝；已覆盖 AES-GCM bad tag，ETM/HMAC bad MAC 待补。
- [X] 截断 packet 拒绝。
- [X] 多 packet 粘包读取。
- [X] 半包增量读取。
- [X] AES-GCM packet_length AAD / ciphertext / tag 固定向量。
- [X] AES-GCM 解密缓冲区 ownership 与容量不足拒绝。

### 14.3 Crypto Tests

- [X] AES-GCM encrypt/decrypt vector。
- [ ] AES-CTR encrypt/decrypt vector。
- [X] HMAC-SHA2 vector。
- [ ] ETM MAC 覆盖范围测试。
- [ ] OpenSSH chacha20-poly1305 vector。
- [X] RFC 8439 ChaCha20-Poly1305 AEAD vector；仅验证底层 XRT 适配，不等同 OpenSSH packet 算法。
- [X] X25519 shared secret vector。
- [ ] ECDH P-256 / P-384 shared secret vector。
- [X] Ed25519 verify vector；已覆盖 RFC 8032 空消息 seed/public/signature、公钥派生、签名输出、SSH signature blob 验证和篡改拒绝。
- [ ] RSA SHA2 verify vector。
- [ ] ECDSA verify vector。
- [X] key derivation A/B/C/D/E/F 测试。

### 14.4 KEX Tests

- [X] KEXINIT 构建列表顺序测试。
- [X] KEXINIT 解析测试。
- [X] 无共同算法失败测试。
- [X] KEXINIT 默认 AEAD 协商测试。
- [X] first_kex_packet_follows 错误猜测跳过测试；已覆盖 helper 判断和本地 socket runtime。
- [X] KEXINIT CTR/HMAC 协商测试。
- [X] curve25519-sha256 exchange hash 测试。
- [ ] ecdh-sha2-nistp256 exchange hash 测试。
- [ ] rsa-sha2 host key signature 验证测试。
- [X] ssh-ed25519 host key signature 验证测试。
- [X] NEWKEYS 前后 packet codec 切换测试。
- [ ] rekey 时旧 channel 不丢数据测试。

### 14.5 Known Hosts Tests

- [X] 普通 host 匹配；已覆盖 `example.com`。
- [X] `[host]:port` 匹配；已覆盖 `[example.org]:2222` 且默认端口不误匹配。
- [X] 多 host 同行匹配；已覆盖 `example.com,[example.org]:2222`。
- [X] hashed host 匹配；已覆盖 OpenSSH `|1|salt|hash` 默认端口和非默认端口。
- [X] unknown host strict 拒绝。
- [X] accept-new 写入候选记录；已覆盖嵌套父目录创建、文件追加和锁内重读去重。
- [X] host key mismatch 拒绝。
- [X] 同 host 多 key type 选择；已覆盖不同算法行的 header 解析、strict 下同 host 不同算法不算 mismatch、accept-new 可为同 host 追加当前算法记录，以及不同算法行后续 Ed25519 精确匹配。

### 14.6 Auth Tests

- [X] none auth allowed methods；已覆盖消息层、无凭据真实认证循环和 `xsshresult.sAuthMethods` 结构化输出。
- [X] password success；已覆盖纯内存 mock transcript 和真实本地 socket 加密 USERAUTH。
- [X] password failure；已覆盖真实本地 socket 加密 USERAUTH failure、result 与 error event。
- [X] publickey probe accepted；已覆盖 publickey probe request、PK_OK 消息层和真实 signer callback 认证循环。
- [X] publickey signed success；已覆盖 signed request 消息层、签名数据构建、Ed25519 签名验证和本地加密 socket USERAUTH_SUCCESS。
- [X] publickey failure；已覆盖本地 socket publickey probe 被拒绝、allowed methods 输出和 error event。
- [X] keyboard-interactive single prompt；已覆盖消息层 request/response。
- [X] keyboard-interactive multi prompt；已覆盖消息层和本地 socket 双 prompt 认证循环。
- [X] partial success。
- [X] auth banner。
- [X] auth too many attempts；已覆盖服务端 USERAUTH banner flood 被客户端上限拒绝，并验证错误信息不包含密码。

### 14.7 Channel Tests

- [X] session channel open success；已覆盖 open/confirmation 消息和真实 runtime 状态机。
- [X] session channel open failure。
- [~] shell request success；已覆盖 request 消息、真实 request/response 路径和本地 socket shell echo roundtrip，OpenSSH live shell 验证待补。
- [X] exec request success；已覆盖 request 消息、纯内存 mock transcript 和真实本地 socket runtime 状态机。
- [~] subsystem request success；已覆盖 request 消息、真实 request/response 路径和本地 socket raw byte channel roundtrip，subsystem live 验证待补。
- [~] PTY request success；已覆盖 request 消息、terminal modes opcode/uint32/TTY_OP_END 编码、真实 request/response 路径和本地 socket shell echo roundtrip，OpenSSH live shell 验证待补。
- [~] env request success/failure；已覆盖 env request 消息、本地加密 socket success/failure 路径和 `CHANNEL_FAILURE` 错误上报，OpenSSH AcceptEnv 兼容测试待补。
- [X] stdout data。
- [X] stderr extended data。
- [X] window adjust。
- [X] remote window 归零后的写入排队；已覆盖 remote window 小于输入长度时的固定容量排队、WINDOW_ADJUST 后自动 flush 和服务端按序收包。
- [X] channel 读后缓冲空间复用；已覆盖 stdout 缓冲读完后继续追加超过原剩余容量的回归测试，避免 shell/forwarding 长连接把固定缓冲耗尽。
- [X] EOF / close 顺序；已覆盖本端先 close 后远端确认、远端先 close 后本端 best-effort 回包和最终 close event。
- [X] signal / break request；已覆盖 request 消息、mock runtime 和真实发送路径。
- [X] exit-status；已覆盖 request 消息解析、纯内存 mock transcript 和真实 runtime 状态保存。
- [X] exit-signal；已覆盖 request 消息解析、本地 socket runtime 状态保存和 `xsshChannelGetExitSignal`。
- [X] direct-tcpip / forwarded-tcpip channel open extra 解析。
- [X] 多 channel 并发；已覆盖同一真实加密 socket 连接上的两个 session channel 并发 exec 数据分发。

## 15. Mock Server

- [~] 实现纯本地 mock SSH server，用于确定性测试；已完成纯内存 client/server transcript 和本地 socket KEX/password/server-global/env/exec channel/forward control/direct-tcpip/forwarded-tcpip data server，AES-GCM bad tag、坏 packet_length、半包关闭和乱序 channel data 异常注入已完成，ETM/HMAC bad MAC 注入待补。
- [~] mock server 支持固定 banner；transcript 已固定 client/server version，`xssh_runtime_test` 已新增本地 socket KEX/password/server-global/env/exec channel/forward control/direct-tcpip/forwarded-tcpip data server、first_kex_packet_follows、加密 packet 异常注入和乱序 channel 夹具，更多 ETM-HMAC 夹具待补。
- [~] mock server 支持可配置 KEXINIT；transcript 已覆盖固定 curve25519/ssh-ed25519/aes128-gcm 协商，本地 socket server 已覆盖 AES-128/256-GCM 和 first_kex_packet_follows 错误猜测，更多 KEX 算法族夹具待补。
- [~] mock server 支持 password auth；transcript 和本地 socket server 已覆盖 password success/failure，次数限制待补。
- [X] mock server 支持 publickey auth；本地 socket mock server 已覆盖 PK_OK、signed request 验签和 USERAUTH_SUCCESS。
- [X] mock server 支持 keyboard-interactive auth；本地 socket mock server 已覆盖 INFO_REQUEST、INFO_RESPONSE 校验和 USERAUTH_SUCCESS。
- [X] mock server 支持 shell echo；本地 socket mock server 已覆盖 PTY、shell request、客户端写入、服务端回显和 channel close。
- [X] mock server 支持 subsystem raw byte channel；本地 socket mock server 已覆盖 subsystem request、客户端写入、服务端数据回显和 channel close。
- [X] mock server 支持 exec 固定输出与 exit-status；transcript 和本地 socket server 已覆盖 exec success、stdout/stderr data、exit-status、EOF/CLOSE。
- [X] mock server 支持 stderr extended data；本地 socket exec server 已发送 extended-data 并由 runtime test 校验。
- [X] mock server 支持窗口耗尽与延迟 window adjust；direct-tcpip data 测试已覆盖 remote window 归零、内部写队列和 WINDOW_ADJUST 自动 flush。
- [X] mock server 支持主动 disconnect；本地 socket mock server 已覆盖认证后发送 DISCONNECT、客户端 poll 返回关闭、事件和 result reason。
- [X] mock server 支持 keepalive/ignore/debug；本地 socket mock server 已覆盖 IGNORE、DEBUG 和 `keepalive@openssh.com` REQUEST_FAILURE 存活回复。
- [~] mock server 支持坏 MAC、坏包长、半包、乱序 channel 消息注入；已完成 AES-GCM bad tag、非法 packet_length、半包关闭和乱序 channel data 注入，ETM/HMAC bad MAC 待补。

## 16. Interop Tests

- [~] OpenSSH server password auth；`xssh_interop_test` 已接入显式环境变量 smoke，当前真实服务器在 banner 阶段关闭连接（`stage=2 error=-11`），待 SSH banner 可用后验证 password auth。
- [ ] OpenSSH server Ed25519 publickey auth。
- [ ] OpenSSH server RSA SHA2 publickey auth。
- [ ] OpenSSH shell PTY。
- [ ] OpenSSH exec。
- [ ] OpenSSH subsystem raw。
- [ ] OpenSSH local forwarding。
- [ ] OpenSSH remote forwarding。
- [ ] OpenSSH rekey。
- [ ] Dropbear 基础连接。
- [ ] 旧设备兼容模式测试，单独开关，不进入默认测试。

环境变量约定：

- [X] `XSSH_LIVE_HOST`
- [X] `XSSH_LIVE_PORT`
- [X] `XSSH_LIVE_USER`
- [X] `XSSH_LIVE_PASSWORD`
- [X] `XSSH_LIVE_IDENTITY`
- [X] `XSSH_LIVE_KNOWN_HOSTS`
- [X] `XSSH_LIVE_ACCEPT_NEW`
- [X] `XSSH_LIVE_COMMAND`
- [ ] `XSSH_LIVE_ENABLE_LEGACY`

## 17. 示例用例

- [~] `xssh_exec.c`：连接、认证、执行命令、读取 stdout/stderr、读取 exit-status；mock runtime 和真实 session/exec API 已接入，OpenSSH live 验证待补。
- [~] `xssh_shell.c`：创建 PTY、打开 shell、stdin/stdout 交互；mock runtime、真实 PTY/shell API 和本地 socket shell echo 已接入，OpenSSH live 验证待补。
- [~] `xssh_forward.c`：本地端口转发到远端目标；已完成 mock CLI、mock self-test、本地监听/accept、direct-tcpip channel 打开和单连接双向数据泵，OpenSSH live 验证和多连接后台服务封装待补。
- [~] `xssh_terminal_xui2`：XGE + XUI Terminal 原生 SSH 终端；mock 渲染、输入、resize、event drain 已完成，真实 SSH shell transport 路径已接入，OpenSSH live 验证待补。
- [~] 示例覆盖 strict host key 默认行为；CLI 和 UI 文案已保持 strict 默认，connect 流程已接入 host key 验证，OpenSSH live 验证待补。
- [~] 示例覆盖 `--accept-new` 明确接受新 host key；参数已接入 config，connect 流程已覆盖 known_hosts 父目录创建和追加，OpenSSH live 验证待补。
- [X] 示例覆盖错误信息打印：`xssh_exec`、`xssh_shell`、`xssh_forward` 和 `xssh_terminal_xui2` 的 SSH connect/channel/pty/shell/exec/forward 失败都会输出阶段、错误码、服务端断开原因、banner、协商算法、认证方法和 host key 指纹。

## 18. 安全检查

- [X] 默认拒绝 unknown host，除非显式 `accept-new`；`xsshConfigInit` 默认 strict，known_hosts helper 和 connect 流程已覆盖。
- [X] 默认禁用 `ssh-rsa`；默认 host key 列表仅包含 Ed25519、RSA-SHA2 和 ECDSA。
- [X] 默认禁用 `hmac-sha1`；默认 MAC 列表仅包含 HMAC-SHA2 族。
- [X] 默认禁用 CBC/3DES；默认 cipher 列表不包含 CBC、3DES 或 arcfour。
- [X] 默认禁用 insecure accept any；`xsshConfigInit` 默认 `XSSH_HOSTKEY_STRICT`，必须显式设置 insecure policy。
- [X] 认证失败不回显敏感字段；password failure、publickey failure 和 auth packet flood 错误只输出 method/stage，不包含 password/private key/signature。
- [ ] 私钥和密码内存释放前清零。
- [X] packet 最大长度有硬上限；默认 `XSSH_PACKET_MAX_DEFAULT` 且 packet read 会拒绝超限长度。
- [X] channel window 有硬上限；配置归一化会将超大 window 钳制到 `XSSH_CHANNEL_WINDOW_MAX`，测试覆盖 mock channel open。
- [~] 端口转发目标有配置边界，不默认开放任意 remote bind；mock API 要求显式 bind/target，真实 remote bind 策略待接入。
- [X] mock/live 测试不把密码写入命令行参数，优先使用环境变量；live interop 使用 `XSSH_LIVE_PASSWORD`，CLI 示例使用 `--password-env`。

## 19. 跨仓库修复记录

本节用于记录开发 `xssh` 时发现并修复的 XRT / XGE 问题。修复原则：只修阻塞或明确相关的 bug，不做无关重构。

- [ ] XRT bug：暂无记录。
- [ ] XGE bug：暂无记录。

记录格式：

```text
- [ ] 仓库：D:\Git\xrt 或 D:\git\xge\dev\xui2
  问题：
  复现：
  修复文件：
  验证命令：
```

## 20. 完成定义

- [ ] `xssh.h` 对外 API 文档完整。
- [ ] 所有 public struct 都有 init 函数或明确零初始化规则。
- [ ] 所有 public 函数都有返回值语义说明。
- [ ] 所有内部协议复杂点都有中文注释。
- [ ] 所有单元测试通过。
- [~] mock server 测试通过；当前 `xssh_mock_server_test` 与 `xssh_runtime_test` 本地 mock/socket server 全部通过，完整 OpenSSH/live 互操作和 ETM/HMAC 夹具待补。
- [ ] OpenSSH live interop 在显式环境变量下通过。
- [X] XUI2 Terminal mock smoke 通过。
- [ ] XUI2 Terminal live shell 手工验证通过。
- [X] README 给出安全默认值和最小用法。
- [ ] 本 SPEC 中所有目标项完成或明确移出范围。
