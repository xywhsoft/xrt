# 发布状态与平台证据

本文件只记录当前重构版本的发布证据边界。功能存在、交叉编译成功和目标平台真实
运行通过是三种不同状态；只有最后一种可以声明对应运行时路径已经验证。

## 自动运行矩阵

当前 CI 对以下环境执行真实程序：

| 环境 | 运行范围 | 证据边界 |
|---|---|---|
| Linux GitHub runner | 全部模块、单头、裁剪、协议互操作、fuzz、协程 sanitizer | runner 的实际宿主架构 |
| macOS GitHub runner | 全部模块、单头、裁剪、发布库消费者 | runner 的实际宿主架构 |
| Windows x64 / MinGW | 全部模块、单头、裁剪、Select/IOCP 协议与压力、发布库消费者 | GCC/MinGW x64 |
| Windows x86、x64 / MSVC、clang-cl | 全功能静态库、动态库及独立消费者 | 对应 Visual C++ 目标环境；必须以实际 CI 结果为证据 |
| Android x86-64 emulator | `core,net` 模块与单头测试 | API 30 模拟器，不代表 ARM 设备 |
| iOS simulator | `core,net` 模块与单头测试 | simulator 目标，不代表真实 iPhone 设备 |

GitHub 托管 runner 通过 seccomp 禁用 io_uring（`io_uring_setup` 返回 EPERM），因此 CI 中
所有 `*uring*` 运行时测试被显式排除，io_uring 门禁只保留编译与链接证据。io_uring 的真实
运行证据必须在支持 io_uring 的环境（自托管 runner 或本机，如 WSL2 Linux 6.18 已验证全套
xhttp_tests 含 uring 用例通过）执行并记录于本文件，不允许以 epoll/Select 替代冒充。

发布库门禁使用 `tools/package.py --verify` 从独立翻译单元真实链接并运行静态库和
动态库消费者。静态库还必须用 whole-archive 或全部对象直链验证所选模块的完整链接
闭包，不能让最小消费者隐藏未解析依赖。构建命令生成单测不能替代目标工具链实际执行；
MSVC 和 clang-cl 的矩阵已经进入 CI，但最终发布组合仍须在发布候选提交上实际跑绿。

截至 2026-08-23，本机与目标硬件发布证据为：

- 2026-08-23，龙芯 3A5000LL / LoongArch64 LP64D / Kylin V10 SP1 /
  Linux 5.4.18-142 / GCC 8.3.0 完成核心真机验证。757 个模块化测试与 351 个单头
  测试全部在目标机编译、链接；其中目标内核实际支持的 710 个模块化测试与 342 个
  单头测试全部运行通过。其余 47 个模块化测试和 9 个单头测试显式要求 io_uring，
  Linux 5.4 不满足 XRT 的 `NODROP + FAST_POLL + operation probe` 后端契约，因此只计
  编译证据，不允许静默回退冒充运行通过。epoll 与 Select 实际路径、358 个裁剪正向
  闭包和 624 个逐直接依赖负向探针均通过。
- 同一 3A5000LL 目标上，LoongArch64 协程 ABI 门禁在默认优化和 `-O2` 下分别连续完成
  4096 轮宿主/协程双向切换，验证 `fp/s0-s8`、LP64D `fs0-fs7` 与 16 字节栈对齐；
  协程、调度器、事件、Future bridge、任务、Channel、取消、OOM、深栈、保护页与清理
  路径同步通过。性能 smoke 确认 `asm-la64-lp64` 协程后端与 epoll 网络后端真实生效；
  单轮环境数据约为 5.04 M switches/s、198 ns/switch 和 437 MiB/s TCP 引用发送，不把
  smoke 数值冻结为跨机器性能基线。全功能静态库的 whole-archive 独立消费者和全功能
  共享库的独立消费者均在目标机运行通过；目标旧版 binutils 也实际覆盖了不依赖响应
  文件的分批归档路径。目标 GCC 不支持 LoongArch ASan，系统也未提供 `libubsan`，
  sanitizer 仍属于工具链未覆盖项，不以普通测试替代 sanitizer 证据。
- 2026-08-22，Windows 10.0.19045 x86-64 / GCC 16.1.0 从全量重建运行全部核心
  模块化测试、示例和 350 组单头裁剪组合，Select 与 IOCP 的 TCP、UDP、代理、TLS、
  HTTP/1.1、WebSocket、压力、取消、OOM、mutation 与协议 fuzz 路径均通过；全部核心
  裁剪闭包及逐直接依赖负向探针通过，静态库 whole-archive 消费者和 DLL 独立消费者
  均通过。
- 2026-08-22，WSL2 Ubuntu x86-64 / Linux 6.18.33.2 / GCC 15.2.0 运行同一源码
  快照的全部核心模块化测试、示例和 350 组单头裁剪组合，Select、epoll、io_uring 的
  实际运行路径通过；全部核心裁剪闭包、静态库 whole-archive 消费者和动态库独立消费者
  均通过。Clang 21.1.8 的协程 ASan/UBSan、核心内存与协程 TSan 通过，HTTP/1.1 与
  WebSocket 的 libFuzzer + ASan/UBSan 各完成 20000 轮。`/mnt/d` 的 DrvFs 不持久化
  POSIX mode，`file_root` 权限语义因此改在同一源码的 WSL 原生 ext4 快照验证，不以
  DrvFs 的 `fchmod` 行为替代 Linux 文件系统证据。
- 2026-08-18，GCC 16.1.0 x64 的全部核心模块化测试、单头测试、全功能静态库、
  动态库及各自独立消费者均已构建并运行通过；核心 466 个模块的裁剪依赖门禁同步
  通过。
- 2026-08-18，Windows x64 Fiber 通过 `test_coroutine_abi_x86_64` 专项门禁：宿主与
  协程分别使用独立哨兵，连续完成 4096 次双向切换并验证 `rbx/rbp/rdi/rsi/r12-r15`
  和 `xmm6-xmm15`；默认优化、`-O2` 与 `-O3` 均通过。
- 2026-08-20，WSL2 Ubuntu x86-64 的 SysV 协程路径连续完成 4096 次双向切换，验证
  `rbx/rbp/r12-r15` 与栈对齐；协程、调度器和事件同时通过 Clang ASan、UBSan 与 TSan。
  TSan 还覆盖核心堆、内存调试、内存统计及其多线程首次初始化和压力测试。
- `xruntime`、`xws`、`xregex` 与 `xmail` 的产品测试聚合、完整裁剪、静态库消费者和
  动态库消费者均已在同一环境运行通过；四个扩展的单头、特性宏、API 参考、文档覆盖
  和发布成熟度同步通过。
- `xhttp` 的 Linux io_uring 客户端、服务器、TLS、取消竞态、重入恢复和慢读写超时节点已在
  WSL2 真实内核运行通过；产品测试、裁剪、单头、消费者和文档门禁同步通过。
- HTTP 认证、HTTP/1、Route、Router、SSE 与 WebSocket 六个协议目标均从所属产品清单
  解析裁剪闭包，并在本机 WSL2 以 libFuzzer、ASan、UBSan 各执行 20000 轮；历史失败输入
  已固化为确定性回归语料。
- `xssh` 已在本机隔离 OpenSSH 上通过 exec、PTY、direct-tcpip 和 8 路并发慢读长输出；
  GCC/Clang 均验证了 32 KiB 接收窗口回补、exit-status、结束标记、关闭和资源归零。
- WebSocket 双向互操作门禁从 `extlibs/xws` 的实际产品输出运行；Python 客户端与 XRT
  服务端、XRT 客户端与 Python 服务端均通过，Select 与 IOCP 各完成 1000 次 HTTP
  Upgrade 重连。CI 不再依赖核心 `all` 目录中可能残留的扩展测试产物。
- 构建工具单元测试、669 份源资产重构审计记录、API 参考、示例索引及全部生成物
  一致性检查同步通过。
- 2026-08-20，最新 XRT/xruntime 生成头、声明头、符号表和能力矩阵已同步到 xlang
  demo6；XRT 2825 个公开 API、xruntime 3267 个公开 API（其中 442 个为扩展自有）
  的快照门禁通过。随后在 WSL2 Ubuntu x86-64 完成 `libxl.so`、`libxlc.so`、CLI、
  内存 VFS、ELF、C/XRT、UTF-8 路径、import/package、xregex 源码包和共享库宿主 API
  全回归；异步任务、HTTP/1.1 核心、WebSocket 核心及 WS/WSS 回环 Upgrade 链路同时
  通过解释运行与 AOT 二进制回归。
- 2026-08-21，xlang demo6 的正式 `xmail` 与 `xssh` 源码包完成收口。`xmail`
  覆盖 MIME、SMTP、POP3、IMAP 及三种邮件协议的本地回环 JIT/AOT；`xssh`
  覆盖语言对象接口、JIT/AOT 和 C smoke。随后 Windows 单命令全量门禁与 WSL2
  全量兼容性门禁均退出成功，生成 C、HTTP/1.1、WS/WSS、本地 TLS 回环和共享库
  宿主测试没有编译器或运行时告警。
- 2026-08-20，GCC 严格模式重新构建并运行核心与 xruntime 的全功能静态库、动态库
  和独立消费者；动态库同时通过 `--no-undefined`，确认内存首次初始化并发修复进入
  实际分发产物后没有引入依赖闭包、ABI 或符号可见性回归。
- 2026-08-12，TinyCC 0.9.27 x86、x64 的全功能静态库及独立消费者均已构建并运行通过。
- TinyCC 0.9.27 x86、x64 的全功能 DLL 已生成；TinyCC Windows 不生成供独立消费者
  使用的 import library，因此动态库只能标记为 package-only，不能视为 `--verify`
  通过。该限制同时记录于 [构建与发布](BUILD.md)。
- TinyCC x86 已通过全功能模块回归、示例和全部单头测试；线程、任务池、协程调度器
  与 HTTP 文件 OOM 生命周期边界另有 GCC/TinyCC 定向回归。

## 尚未取得运行证据

以下实现不能仅凭源码存在或交叉编译成功声明为稳定发布目标：

- POSIX ARM64/AAPCS64 协程上下文、深栈和 `q8-q15` 非易失寄存器。
- POSIX RISC-V 64 协程的 `s0-s11/sp/ra` 及对应浮点 ABI 保存集合。
- Windows ARM64 Fiber 路径的目标设备运行回归。
- Android ARM64、iOS 真机及其他当前只覆盖模拟器或交叉编译的体系。

这些目标的验收必须包含创建/恢复/让出、深栈、保护页、浮点或向量非易失寄存器、
调度器、取消、sleep、Future bridge 和清理栈。模拟另一架构的编译不能替代运行；
若使用模拟器，还必须把模拟器种类和未覆盖的内核/ABI 差异写入发布说明。

## io_uring 门禁

核心与扩展库各自的 `config/modules.json` 是模块状态的唯一事实来源。HTTP 客户端/服务器
专用 io_uring 测试已随高级 HTTP 层迁入 xhttp；XRT 不再为应用层对象重复维护一套后端
契约。

通用网络、TLS 与 xhttp 的 io_uring 节点均已在 WSL2 的真实 Linux 内核完成 GCC/Clang
运行验证，当前核心和 xhttp 清单没有 `developing` 节点。覆盖范围包括 TCP/UDP、Future、
TLS stream/dial、HTTP client/server、TLS server、取消竞态、回调重入恢复和慢读写超时。

连接池 io_uring 契约已经完成，但仍固定进入同一发布门禁，避免扩展库回归被核心测试
覆盖假象遮蔽。Zig 或其他工具链交叉编译只能证明声明和链接闭包，不能把待验证节点改为
`implemented`。验收必须在支持 io_uring 的真实 Linux 内核运行连接、收发、代理、TLS、
取消、deadline、关闭和资源归零测试。门禁命令见
[HTTP/WebSocket 核心发布门禁](HTTP_WEBSOCKET_RELEASE.md)。显式请求 io_uring 时不允许后端
降级；工作流存在本身不算运行证据，只有对应提交在真实 Linux runner 上完整通过后才能
改变节点状态。

## 扩展库收口门禁

当前扩展清单共有 `xruntime`、`xhttp`、`xws`、`xregex`、`xmail` 和 `xssh` 六个产品。
清单状态是功能收口事实，历史二进制或旧版测试结果不能替代当前实现证据。

- `xruntime`、`xhttp`、`xws`、`xregex`、`xmail` 和 `xssh` 当前清单均没有 `developing` 模块。
- `xssh`：`ssh_client_core`、Resolver/Happy Eyeballs Dial、统一 Future 等待、Ed25519 publickey provider 及 session/forward/PTY helper 已通过无网络状态机、模块化、单头、
  裁剪和 Windows x86-64 Select 真实 TCP 回归。真实链路覆盖握手、认证、全局成功/失败回执、
  exec、PTY shell、窗口调整、direct-tcpip、DATA、退出与关闭，同时覆盖 Ready/Close、global、
  channel open/request/EOF Future、16 个并发 close waiter、跨线程取消、地址复用隔离与 channel
  终态收口，并连续运行 100 轮、500 条连接；其中包含 Select 完整成功链和 Windows IOCP 硬背压成功链。
  当前回归又补齐 Future 五个逻辑分配点故障、32 字节发送硬上限、
  16 字节远端窗口分片、WRITE Future、五次窗口恢复、错误口令后重试、channel open/request 拒绝、
  服务端主动 forwarded-tcpip、坏主机密钥拒绝、未决 global Future 期间 RST 断线、stderr、exit-signal
  和内存调试活动块归零；
  forwarded-tcpip 修正并回归了 Packet 回调内 peer-open 决策上下文，主机密钥拒绝修正了协议错误在
  Close/未决 Future 中的保留，RST 断线证明了网络错误在 Close 与所有未决 Future 间的一致传播；
  Packet HOLD 经同 Worker 显式恢复后仍保持原子提交，静默服务端则证明 TCP 后 50 ms Ready 截止时间
  以统一的 `XSSH_ERROR_TIMEOUT / XERR_TIMEOUT` 终结 Error、Close 和未决 Future。IOCP 链再以 32 KiB
  合法 IGNORE 和 48 KiB 写预算确定性验证组合层 TCP `AGAIN`、完整待发包保留、内部优先 Drain 重试；
  Packet OOM RETRY 也已验证不会污染后续正常 Close 的 `TerminalError`。
  Windows GNU 静态库、DLL 和各自的独立消费者也已构建并运行通过。64 路 channel 双向窗口
  耗尽/恢复、`uint32` ID 回绕避让、65,536 轮 reply FIFO 环绕及连接级/通道级 token ring
  扩容已经通过。隔离 OpenSSH 门禁又以 GCC/Clang 完成 Ed25519、exec、PTY、direct-tcpip 和
  8 路并发慢读长输出；未配置目标时仍明确 `SKIP`，不能替代目标发布环境的实际互操作结果。

旧 `xsmtp`、`xpop3`、`ximap` 和 `xmail_mime` 已由 `xmail` 覆盖，不恢复为重复产品。
它们只作为历史行为与边界资产保存在归档区。`xruntime` 已完成 xlang 快照、符号表、
能力矩阵、语言内核运行和标准库回归；`xregex` 已建立正式源码包、对象化接口、精确裁剪
根和 Windows/Linux 包内运行门禁，两者均不再属于延期项。`xhttp`、`xws` 的非快速路径
按当前边界继续延期；`xmail`、`xssh` 的正式语言包、语言级对象抽象和底层 C API 出口
已经完成，并进入 Windows/WSL 的 JIT、AOT 与 C smoke 固定门禁。

## 已关闭的旧全局缺陷

旧版全局 TODO 中以下问题已经进入当前契约并有专项证据：

- 本地时间使用目标时刻的操作系统历史时区规则，并处理 DST gap/fold；见
  `docs/api/time.md` 和 `tests/time/test_time_local.c`。
- UTF-8 解码拒绝 overlong、代理区码点和超过 `U+10FFFF` 的标量；见
  `tests/charset/test_unicode.c` 与 `test_unicode_convert.c`。
- X.509 名称规范化表使用定宽位流保存 Unicode 标量、映射、区间与组合关系；生成器
  校验与名称边界测试共同保证压缩表达不改变协议语义。
- `xruntime` callable 采用线程安全引用生命周期；见
  `extlibs/xruntime/tests/runtime/test_runtime_call_threads.c`。
- `xid` ABI 明确为 24 字节，批量尺寸溢出、OOM 和并发生成均有独立测试；见
  `tests/id/test_xid.c`、`test_xid_oom.c` 和 `test_xid_threads.c`。
- 结构化错误、统一 `xdeadline`、取消令牌和类型 Move 生命周期已经是公开基础层，
  不保留旧版 `Ex`/兼容包装计划。
- 通用运行时模型已经迁入 `extlibs/xruntime`，其 `runtime` 性能 profile 与
  `runtime_core`、`runtime_full` 体积 profile 随扩展维护，不进入核心发布门禁。
- 正则表达式实现和 BBRE 已迁入 `extlibs/xregex`；完整 URL、Query 与表单编码已迁入
  `extlibs/xhttp`，核心只保留 HTTP 线路处理所需的 Host/authority 与 request-target。
- 协程上下文切换、生命周期、跨线程 post 与 timer churn 已迁移到当前单头契约并进入
  `coroutine` 性能 profile；TaskPool 与 Executor 在 `task` profile 中分别测量。
- 任务池型异步文件已建立有界并发定位读写基准；IOCP/io_uring 原生完成式文件仍按网络
  后端独立验收，不与便利 Future 层混用性能口径。
- `thread` 的回收统计测试已拆为 `thread_memory_stats_tests`，生产线程模块不再因测试
  暗依赖内存统计，最小裁剪闭包保持干净。
- ASN.1、PEM、密码、X.509 与 TLS 的 154 个当前 Windows 根节点已逐根完成模块化、OOM、
  负向、示例和单头文件回归；不是只由顶层 TLS 间接覆盖。
- 网络内部统计 helper 只在原子模块存在时声明；纯 `net` 地址层、`x509_identity` 与
  `tls_session` 不再因默认 FULL 统计暗依赖 `atomic`。
- 单头文件多所有者实现 guard 不再生成多余的一元 `+`；生成器测试固定了单条件与多条件
  的精确输出，当前生成物不存在行首 `+`。
- SHA-256、ChaCha20-Poly1305、X25519、真实 DER 证书解析和 RSA 验签已进入统一性能
  profile；本机 smoke 只用于确认入口有效，正式发布仍须按固定机器多样本重采样。

## 发布规则

发布候选必须从空输出目录执行当前 [构建与发布](BUILD.md) 和
[HTTP/WebSocket 门禁](HTTP_WEBSOCKET_RELEASE.md)。任何未取得运行证据的平台或后端
必须明确标为未验证，不能通过降低模块状态、跳过测试或引用旧 benchmark 宣称完成。
