# XTP 实施与验收

P1 Codec 已实现并通过局部验证：独立固定字节样本、非字典序 table、array/table 下标一致性、二进制空值、五种消息、2000 组确定性随机有效输入、畸形字段、失败输出不变、批量末项失败原子性、别名、256 项及资源边界。

2026-11-16 验证：Windows GCC -O2 模块化和生成单头测试、真实 Codec 示例、独立静态包消费、严格 C99、C++11 头与 C 对象链接消费；WSL Ubuntu Clang ASan/UBSan 单元测试与 50000 轮 libFuzzer，以及 Linux TCC 0.9.27。Codec 对象的未定义符号仅为 memcmp/memcpy，没有分配器调用。Fuzzer 的外部 symbolizer 不可用，未出现 sanitizer 错误；有限轮次和本地 WSL 不替代全部输入及独立 Linux 实机验收。32 位和大端平台尚未执行。

测试命令以 README 为准。模糊入口为 tests/fuzz_codec.c；使用 Clang 的 `-fsanitize=fuzzer,address,undefined` 单独构建，不作为普通 main 测试链接。

P2 Reader 已实现：STREAM/RECORD、全拆分点和逐字节输入、粘包、准确 Used、显式暂停/恢复、接收 EOF、记录截断与异常丢弃、完整帧借用、前缀先校验再组装 Body、有界复制/丢弃/校验、绝对接收期限和自定义分配器。无网络状态或自动回执。

P2 验证：Windows GCC -O2 模块化/生成单头、真实 7 字节分片后端示例、独立静态包消费；WSL Clang ASan/UBSan 模块化、Linux TCC 0.9.27、50000 轮 Reader libFuzzer。覆盖 500KB 完整零复制输入与分片缓冲峰值、逐分配点 OOM、错误前缀不分配 Body、记录恢复、错误次数阈值、反向时钟、期限及预算延后校验时不误判已收齐帧。模糊入口 tests/fuzz_reader.c，有限轮次不代表穷尽验证。

P3 Session 已实现：双向独立 ID、单条/原子批量复制提交、统一消息/完成/故障回调、同步或异步 Reply、有限 pending 哈希表、逐 lane 输出链表、ACK/响应预留、短写 Ticket、两类输出边界、借用回调、绝对期限、有界关闭、绑定代与令牌代、窗口内 PUSH 去重和已退休 ID 拒绝。默认另保留 16 个控制发送槽，避免普通发送挤满所有槽位。

P3 验证：Windows GCC -O2 模块化/生成单头、双端例程、独立静态/动态包消费与 C++11 消费；WSL Clang ASan/UBSan、TCC 0.9.27 模块化/单头及 50000 轮状态机 libFuzzer。覆盖四种形态、32 请求逆序回应、错误类型/未知/迟到结果、回执、STREAM 短写、RECORD 整条受理、PAUSE/恢复、ACK 配额不足时不调用业务、响应预留后 Reply 无分配、逐创建/绑定/批量分配点失败、旧绑定/请求/输出令牌、单头下 ID/槽代耗尽、回调重入、分配器重入拒绝、回调内 Reply/Close、分轮恰好一次完成及 STATE_LOST 的 UNKNOWN 认知。tests/fuzz_session.c 驱动输入、提交、输出、异步应答、时钟、重绑和关闭；不替代真实承载故障测试。

当前规范 C 源码及全部自有公共/内部头按 UTF-8/LF 计约 81 KiB（含 features，排除生成单头、依赖、测试、示例、文档）。已略超 80 KiB 设计预算，Session 的配额、令牌、预留和生命周期检查占据主要增量；后续可选模块须继续记录体积，不以移走协议状态或删除校验掩盖增长。尚未取得吞吐、延迟、分配或端到端性能结论。

P4 TCP/TLS/WS、P5 xrudp、P6 内存 Delivery 和 P7 XS 迁移/完整性能及生产验收尚未实现。当前 Session 不恢复网络中断、不在 BUSY 后自动重投、不缓存可恢复 RPC 结果；Replay 明确拒绝，RESULT_ACK 只由 Codec 支持线路校验。普通重复 RPC 不再次执行业务，活跃请求等待原响应，已结束请求报告 STATE_LOST；不能把此有限保护当成可恢复投递保证。专用安全承载和恢复能力仍需各自验收。
