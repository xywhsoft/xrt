# XTP 实施与验收

P1～P7 段落记录各阶段的历史状态和测量。当前实现、用户取消源码硬上限后的体积以及重建结果，以本文收尾记录、[最新性能](performance.md) 和 [部署资格](qualification.md) 为准。

P1 Codec 已实现并通过局部验证：独立固定字节样本、非字典序 table、array/table 下标一致性、二进制空值、五种消息、2000 组确定性随机有效输入、畸形字段、失败输出不变、批量末项失败原子性、别名、256 项及资源边界。

2026-11-16 验证：Windows GCC -O2 模块化和生成单头测试、真实 Codec 示例、独立静态包消费、严格 C99、C++11 头与 C 对象链接消费；WSL Ubuntu Clang ASan/UBSan 单元测试与 50000 轮 libFuzzer，以及 Linux TCC 0.9.27。Codec 对象的未定义符号仅为 memcmp/memcpy，没有分配器调用。Fuzzer 的外部 symbolizer 不可用，未出现 sanitizer 错误；有限轮次和本地 WSL 不替代全部输入及独立 Linux 实机验收。32 位和大端平台尚未执行。

测试命令以 README 为准。模糊入口为 tests/fuzz_codec.c；使用 Clang 的 `-fsanitize=fuzzer,address,undefined` 单独构建，不作为普通 main 测试链接。

P2 Reader 已实现：STREAM/RECORD、全拆分点和逐字节输入、粘包、准确 Used、显式暂停/恢复、接收 EOF、记录截断与异常丢弃、完整帧借用、前缀先校验再组装 Body、有界复制/丢弃/校验、绝对接收期限和自定义分配器。无网络状态或自动回执。

P2 验证：Windows GCC -O2 模块化/生成单头、真实 7 字节分片后端示例、独立静态包消费；WSL Clang ASan/UBSan 模块化、Linux TCC 0.9.27、50000 轮 Reader libFuzzer。覆盖 500KB 完整零复制输入与分片缓冲峰值、逐分配点 OOM、错误前缀不分配 Body、记录恢复、错误次数阈值、反向时钟、期限及预算延后校验时不误判已收齐帧。模糊入口 tests/fuzz_reader.c，有限轮次不代表穷尽验证。

P3 Session 已实现：双向独立 ID、单条/原子批量复制提交、统一消息/完成/故障回调、同步或异步 Reply、有限 pending 哈希表、逐 lane 输出链表、ACK/响应预留、短写 Ticket、两类输出边界、借用回调、绝对期限、有界关闭、绑定代与令牌代、窗口内 PUSH 去重和已退休 ID 拒绝。默认另保留 16 个控制发送槽，避免普通发送挤满所有槽位。

P3 验证：Windows GCC -O2 模块化/生成单头、双端例程、独立静态/动态包消费与 C++11 消费；WSL Clang ASan/UBSan、TCC 0.9.27 模块化/单头及 50000 轮状态机 libFuzzer。覆盖四种形态、32 请求逆序回应、错误类型/未知/迟到结果、回执、STREAM 短写、RECORD 整条受理、PAUSE/恢复、ACK 配额不足时不调用业务、响应预留后 Reply 无分配、逐创建/绑定/批量分配点失败、旧绑定/请求/输出令牌、单头下 ID/槽代耗尽、回调重入、分配器重入拒绝、回调内 Reply/Close、分轮恰好一次完成及 STATE_LOST 的 UNKNOWN 认知。tests/fuzz_session.c 驱动输入、提交、输出、异步应答、时钟、重绑和关闭；不替代真实承载故障测试。

P1～P3 基线的规范 C 源码及全部自有公共/内部头按 UTF-8/LF 计约 81 KiB（含 features，排除生成单头、依赖、测试、示例、文档）。已略超 80 KiB 设计预算，Session 的配额、令牌、预留和生命周期检查占据主要增量；后续可选模块须继续记录体积，不以移走协议状态或删除校验掩盖增长。该阶段尚未取得性能结论；P7 的测量另见下文。

P4 已实现通用有界排空、已有 TCP、TLS stream 和 xws connection 的写入适配，以及 WS Binary 分段接收桥接。没有连接工厂、线程、事件表接管或 HTTP Upgrade 工厂；外层连接生命周期由宿主管理。TLS 策略只允许 1.3、X25519、ChaCha20/AES128-GCM 和 Ed25519/P-256 签名，写入时核对实际版本/套件/ALPN。

P4 验证：Windows GCC -O2 模块化和单头真实回环，TCP/TLS1.3 ChaCha+AES/WS 双向三类消息、TLS 身份拒绝、TLS1.2 拒绝、短写、预算、RecordEnd、Text 拒绝及物理退休；WSL Clang ASan/UBSan 真实回环通过。Linux readiness 的同步 Read 重入曾导致示例复制前缀顺序被颠倒，已用宿主 Reading/Flushing 标志修复并在 sanitizer 下复验；XTP 线路解析没有改成 magic 扫描恢复。三个网络例程和单头运行相同断言；测试身份仅为固定 fixture pin，不替代生产 PKI 配置。


P4 自有 C/头文件合计（UTF-8/LF）：94918 字节（92.69 KiB），依赖和示例另计。

P5 已实现 xrudp 原生 BYTES/不可靠 DATAGRAM 写入适配，动态发送上限校验及单条超限通知的 LOCAL_ERROR/NOT_SENT 完成；拒绝超限通知不影响可靠 lane。

P5 验证：Windows GCC 模块化和单头、WSL Clang ASan/UBSan 的真实 UDP + Noise IK/静态公钥 pin/地址 cookie。双向命令、60Hz 提交快照和独立低优先级资源流在 0/1/5/10% 丢包、20/80/200ms RTT 的12种组合完成；另测1200到512的PMTU变化、较大通知本地拒绝和小快照/可靠资源继续到达。资源使用4KiB应用块、30s操作及15s接收期限，快照有50ms发送排队期限和应用Tick去旧，不把这些配置当作默认或互联网时延承诺。PMTU用例使用注入时钟，其运行时长不作延迟数据。

P5 自有 C/头文件合计（UTF-8/LF）：99384 字节（97.05 KiB），依赖和示例另计。

P6 Delivery 已实现：可选编译、默认关闭，认证证明续接、规范帧保留、逐轮 Ticket、BUSY 退避、异步请求令牌、结果缓存/RESULT_ACK、不可淘汰的紧凑完成回执及统一内存记账。Windows GCC 模块/单头与例程通过；Linux Clang ASan/UBSan 模块/单头通过；libFuzzer+ASan/UBSan 状态机 186832 轮/36 秒无失败（随机探索，不是穷举证明）。测试覆盖丢 ACK/响应/结果确认、无健康链路重传、状态/身份丢失、异步续接、BUSY 上限、原截止时间、窗口前移、半包完成仍保留物理配额以及 Create/SubmitBatch/续接逐分配点 OOM 回滚。

P6 结束时全部自有 include/src C/h（包含可选 native/Delivery，LF 规范计数）为 116790 字节，超出当时的初始设计预算。该历史测量不代表收尾后的源码；用户后续已取消硬上限。禁用模块可裁减编译路径，但不把可选源码排除出总量统计。

P7 已完成本地交付形态验收：Windows GCC 模块/单头和 TCP/TLS/WS 例程、Linux Clang ASan/UBSan、C99 模块/单头编译、C++11 静态/动态消费；纯 XTP 和包含 TCP/TLS/WS/xrudp 的独立静态/动态包分别核验。测试入口、运行命令及容量边界见 [性能与体积](performance.md)。888 个本地样本、180 个真实回环样本及三次重复的标准 profile 均记录源码和原始报告哈希。固定到达率记录所有客户端窗口拒绝，P50/P95/P99/P99.9/最大延迟与生成器延迟分开；不把低负载或已成功请求冒充过载结果。

XS 在 D:/GIT/xserver/lib/xtp 逐文件固定 XTP3，替换原头、实现、构建宏、符号和旧测试；不保留旧线格式。42 个直接导出与三扩展闭包 329 个导出通过 Windows/Linux 外层及嵌套 TCC，覆盖四种形态、TABLE 的 ParamAt、回调返回结构体、32 字节 token 按值、RPC/PUSH/NOTIFY 及旧头/符号不可见。脚本卸载前仍须停止任务、Close/Drive 并 Destroy，不内置隐式热重载所有权。

验收中发现并修正：TLS 公共头重复 typedef 使严格 C99 失败，已统一共享前置声明并复验；ABI 文本扫描把单行结构体后的条件误判进结构体，已修正并完整通过三项 ABI 与七项声明检查；统计探针额外持有 stream 引用曾阻碍退休，已改为开始关闭前的借用快照，Linux ASan/UBSan 的固定到达率与大 Body 压力通过。Windows/WSL 包工具共用 native 输出目录，因此本轮串行重建核验并分别保留本机包副本；跨平台不能混用同名静态库。

P7 当时未取得 WSS/HTTP Upgrade、xrudp 饱和矩阵、多连接、32位/大端及长期部署资格。其本地可执行部分在下列收尾继续完成；移动网络/NAT、多日长稳与内核唤醒/全部复制的动态归因仍须真实部署验收。

收尾 R1：完成记录改为固定滚动窗口，正回执仅用于已消费结果，旧结果以 RESULT_ACK+STATE_LOST 明确退休；活跃请求独立保留并钳制恢复下界。默认 1024 个记录下，同一逻辑保留期连续 12000 次可恢复 RPC 全部完成；5000 次实测提交零拒绝，去除原先 1024/60s 的容量上限。GCC 模块/单头、Clang ASan/UBSan、回执丢失、乱序活跃请求、晚到未消费结果、黄金字节与 OOM 测试通过。旧体积/吞吐记录属于先前实现，新的完整性能矩阵在后续验收重建。源码预算按用户确认不设硬上限，优先简洁、安全和性能。

收尾 R2：Output/Drain 默认立即聚合同一可靠 lane 的排队帧，STREAM 支持跨帧短写，RECORD 整批受理；不可靠数据报保持单消息。新增聚合票据失效、回调复用槽/关闭、恢复后结果抢先到达、OOM 退回单帧与预算计数测试。Windows GCC 模块/单头及原生回环、Linux Clang ASan/UBSan 通过；Delivery 单头 ASan/fuzzer 完成 99972 次运行。聚合暂存复制和惰性内存明确计费，后续性能记录列出计数。

收尾 R3：真实 TLS1.3 + HTTP Upgrade + xtp.v3 WSS 双向三类消息、错误 pin、两端旧子协议拒绝在 Windows 模块/单头及 Linux ASan/UBSan 通过；新增可运行 WSS 例程。PowerPC64 大端交叉编译/QEMU 的模块/单头全套通过；Windows x86 模块 -O2、模块/单头 -O0 全套通过，特定优化下的测试编译告警如实列于 qualification.md。Windows 全套 native/xrudp 测试、独立静态/动态包校验及公共 ABI 3 项/声明 7 项通过。

收尾 R4：新增真实 UDP/Noise 三模型饱和探针、同引擎多连接/多 worker 探针及持续连接 soak 工具，保留每连接尾延迟、NOTIFY 实际接收数、所有完成、聚合复制和内存。Linux ASan/UBSan 8 连接/4 worker 的持续 TLS 测试运行 301 秒、250 轮、8000000 次 RPC 后正常回收；Windows 相同形态 60 秒完成 43872000 次 RPC，OS 内存采样独立保留。它们是有界压力检查，不冒充移动公网或多日长稳。完整矩阵的收据见 performance.md。
