# XTP 实施与验收

P1 Codec 已实现并通过局部验证：独立固定字节样本、非字典序 table、array/table 下标一致性、二进制空值、五种消息、2000 组确定性随机有效输入、畸形字段、失败输出不变、批量末项失败原子性、别名、256 项及资源边界。

2026-11-16 验证：Windows GCC -O2 模块化和生成单头测试、真实 Codec 示例、独立静态包消费、严格 C99、C++11 头与 C 对象链接消费；WSL Ubuntu Clang ASan/UBSan 单元测试与 50000 轮 libFuzzer，以及 Linux TCC 0.9.27。Codec 对象的未定义符号仅为 memcmp/memcpy，没有分配器调用。Fuzzer 的外部 symbolizer 不可用，未出现 sanitizer 错误；有限轮次和本地 WSL 不替代全部输入及独立 Linux 实机验收。32 位和大端平台尚未执行。

测试命令以 README 为准。模糊入口为 tests/fuzz_codec.c；使用 Clang 的 `-fsanitize=fuzzer,address,undefined` 单独构建，不作为普通 main 测试链接。

P2 增量输入、P3 会话与回调、P4 TCP/TLS/WS、P5 xrudp、P6 内存投递和 P7 XS 迁移/完整性能及生产验收尚未实现。不得把 Codec 验证当成网络或投递验收。
