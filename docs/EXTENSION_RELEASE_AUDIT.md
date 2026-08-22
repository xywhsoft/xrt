# 扩展库最终发布审计

审计日期：2026-08-22

审计范围：`xruntime`、`xhttp`、`xws`、`xregex`、`xmail`、`xssh` 的公开 API、
文档、示例、模块清单、裁剪闭包、单头文件、发布库、互操作、性能入口和尺寸入口。
历史归档目录不参与产品构建，也不作为当前实现证据。

## 结论

六个扩展库的当前源码均通过 Windows x86-64 和 WSL2 Linux x86-64 的产品级回归，
并通过生成物一致性、API 文档完整性、模块成熟度、完整裁剪、单头、静态库、动态库
及独立消费者门禁。当前清单没有 `developing` 或 `review` 模块，公共头没有反向包含
`src/internal`，也没有保留兼容版、版本号或弃用后缀 API。

本轮结论是：六个产品的 API、文档、示例和可执行 release gate 已经收口，可以进入
发布候选阶段。正式发布前仍应在干净的候选提交和固定基准机上采集多样本性能/尺寸基线；
当前 profile 的策略是 `baseline-unpinned`，本轮 smoke 结果只能证明入口、指标解析和
裁剪测量有效，不能替代版本化回归阈值。

## 产品矩阵

| 产品 | 清单模块 | 已实现/测试/单头/文档 | 函数 | 常量 | 类型 | C 示例 | 性能 profile | 尺寸 profile |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `xruntime` | 47 | 36 / 36 / 36 / 36 | 442 | 125 | 68 | 30 | 3 | 2 |
| `xhttp` | 272 | 173 / 173 / 173 / 173 | 1036 | 901 | 442 | 133 | 2 | 4 |
| `xws` | 73 | 22 / 22 / 22 / 22 | 133 | 72 | 35 | 15 | 1 | 4 |
| `xregex` | 6 | 6 / 6 / 6 / 6 | 54 | 23 | 14 | 4 | 1 | 2 |
| `xmail` | 50 | 38 / 38 / 38 / 38 | 277 | 247 | 102 | 23 | 2 | 32 |
| `xssh` | 68 | 66 / 66 / 66 / 66 | 585 | 517 | 154 | 65 | 7 | 55 |

API 文档检查共覆盖 2527 个函数、1885 个常量和 815 个类型，缺失数为 0。
示例索引与 270 个扩展 C 示例保持一致。

## API 与生成物

每个产品均通过以下固定检查：

```text
generate_extension_features.py --check
amalgamate.py --check
generate_api_reference.py --check
check_api_docs.py
check_release_maturity.py --release
```

附加静态检查结果：

- 公共头只依赖公共入口，不包含产品 `src/internal`。
- 公共符号没有 `Legacy`、`Deprecated`、`Compat`、`V<n>` 或 `Ex<n>` 版本尾缀。
- 六份 `features.h` 与六份单头文件和模块清单一致。
- 全仓示例索引一致，669 份源资产重构审计记录与历史基线一致。
- 构建、打包、尺寸、性能、文档和清单工具的 94 项单元测试通过。

本轮修正了 `xhttp` 的直接裁剪依赖声明：流式客户端、客户端运行时和 Future 客户端
显式要求 `spin`，拥有型 HTTP body 显式要求 `atomic`。这避免最小组合依靠全功能构建
间接带入依赖。还修正了 API 文档工具测试在独立 unittest discovery 下的模块搜索路径。

## Windows 证据

环境：Windows 10 x86-64，GCC 16.1.0，`-Wall -Wextra -Werror`。

- 六个产品的模块化测试、示例和完整单头测试通过。
- 六个产品的全部裁剪闭包测试通过。
- 六个产品的静态库、动态库及各自独立消费者通过。
- `xws` 与 Python WebSocket 实现的客户端/服务器双向互操作通过。
- `xws` 在 Select 和 IOCP 后端分别完成 1000 次 Upgrade 重连。
- 16 个性能 profile 的 smoke 构建、运行和指标解析通过。
- 99 个单头尺寸 profile 以 `-O2 -Werror` 从头编译通过。

HTTP smoke 中，预构建原始响应为 18950.2 req/s，结构化响应为 15701.5 req/s；这证明
原始响应快速路径独立存在且没有被高级对象路径强制接管。URL 解析为 4038772 ops/s，
Query 扫描为 16694491 ops/s。以上数字只用于本轮路径核验，不作为稳定性能承诺。

尺寸门禁验证了分层裁剪的实际效果。例如 `xhttp` Query 层为 23276 字节 text，URL 层
为 51652 字节 text，完整扩展为 1971972 字节 text；`xssh` wire 层为 17848 字节 text，
完整 TCP session 路径约为 373004 字节 text。

## Linux 证据

环境：WSL2 Ubuntu x86-64，Linux 6.18.33.2，GCC 15.2.0，原生 ext4 源码快照，
`-Wall -Wextra -Werror`。

- 六个产品的模块化测试、示例、完整单头和裁剪闭包通过。
- 六个产品的静态库、共享库及各自独立消费者通过。
- `xws` 与 Python WebSocket 实现的客户端/服务器双向互操作通过。
- `xws` Select 后端完成 1000 次 Upgrade 重连。
- `xssh` 与仅监听 `127.0.0.1` 的临时 OpenSSH 服务端完成真实协议互操作。
- `xssh` 的密码和 Ed25519 公钥认证均覆盖 exec、PTY、direct-tcpip 和 8 路并发通道。

临时 OpenSSH、密钥、回环转发端点和 WSL 审计快照只用于本机门禁，不是外部服务测试。

## 发布判定

| 门禁 | 状态 | 判定 |
|---|---|---|
| API 命名与公共边界 | PASS | 无历史兼容 API 和内部头泄漏 |
| API 文档 | PASS | 函数、常量、类型缺失均为 0 |
| 示例 | PASS | 示例可构建运行，索引一致 |
| 模块成熟度 | PASS | 六产品均无 developing/review 节点 |
| 裁剪与单头 | PASS | Windows/WSL 完整闭包通过 |
| 静态/动态发布库 | PASS | 独立消费者真实链接并运行 |
| 协议互操作 | PASS | WebSocket/Python、SSH/OpenSSH 本地链路通过 |
| 性能与尺寸入口 | PASS | 16 个性能、99 个尺寸 profile 可执行 |
| 版本化回归阈值 | PENDING | 必须在干净候选提交和固定机器上冻结 |
| ARM64/RISC-V/LoongArch 真机 | DEFERRED | 按既定计划单独执行平台任务 |

`PENDING` 项不表示已发现功能错误，而是防止把单次 smoke 数据误写成长期性能契约。
正式候选发布时，应使用默认 warmup/repeat 配置重采样，将基线与源码指纹、编译器版本、
CPU、亲和性和电源策略一并纳入版本控制，然后启用 `--check` 回归比较。
