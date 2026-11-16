# XTP 实施与验收

P1 Codec 已实现并通过局部验证：独立固定字节样本、非字典序 table、array/table 下标一致性、二进制空值、五种消息、2000 组确定性随机有效输入、畸形字段、失败输出不变、批量末项失败原子性、别名、256 项及资源边界。

2026-11-16 验证：Windows GCC -O2 模块化和生成单头测试、真实 Codec 示例、独立静态包消费、严格 C99、C++11 头与 C 对象链接消费；WSL Ubuntu Clang ASan/UBSan 单元测试与 50000 轮 libFuzzer，以及 Linux TCC 0.9.27。Codec 对象的未定义符号仅为 memcmp/memcpy，没有分配器调用。Fuzzer 的外部 symbolizer 不可用，未出现 sanitizer 错误；有限轮次和本地 WSL 不替代全部输入及独立 Linux 实机验收。32 位和大端平台尚未执行。

测试命令以 README 为准。模糊入口为 tests/fuzz_codec.c；使用 Clang 的 `-fsanitize=fuzzer,address,undefined` 单独构建，不作为普通 main 测试链接。

P2 Reader 已实现：STREAM/RECORD、全拆分点和逐字节输入、粘包、准确 Used、显式暂停/恢复、接收 EOF、记录截断与异常丢弃、完整帧借用、前缀先校验再组装 Body、有界复制/丢弃/校验、绝对接收期限和自定义分配器。无网络状态或自动回执。

P2 验证：Windows GCC -O2 模块化/生成单头、真实 7 字节分片后端示例、独立静态包消费；WSL Clang ASan/UBSan 模块化、Linux TCC 0.9.27、50000 轮 Reader libFuzzer。覆盖 500KB 完整零复制输入与分片缓冲峰值、逐分配点 OOM、错误前缀不分配 Body、记录恢复、错误次数阈值、反向时钟、期限及预算延后校验时不误判已收齐帧。模糊入口 tests/fuzz_reader.c，有限轮次不代表穷尽验证。

P3 会话与回调、P4 TCP/TLS/WS、P5 xrudp、P6 内存投递和 P7 XS 迁移/完整性能及生产验收尚未实现。不得把 Codec/Reader 验证当成网络或投递验收。
