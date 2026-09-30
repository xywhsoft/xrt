# XRT VFS 阶段门审计

审计日期：2026-09-21
审计代码基线：`36955ba10b59cdae8c32554b6a560c3238f49a9b`
开发分支：`codex/mdo-refactor-xrt`

## 结论

XRT VFS 的库内实现门禁通过。`xfile` backend、VFS namespace、memory/disk/pack provider、单头发布形态、公开 ABI、文档和示例已形成闭环，可以进入宿主集成。

XRT-0 仍保留一项宿主验收：必须在 xs/mdo 的真实打包 webview + VFS + TCC + reload controller 路径执行少量、带生命周期诊断的启动回归。该条件无法由 xrt 单仓测试替代，因此它是进入 XS-101 前的集成门，不影响判定 VFS 库实现完成，也不能在集成验证前标为完全关闭。

按项目 2026-09-21 的执行约束，后续不运行压力测试或高负载测试。门禁采用低负载功能回归、确定性交错、有限语料、OOM/故障注入、sanitizer 冒烟、静态检查和干净重编译。表中较高负载结果均为约束生效前已经取得的历史证据，不要求重跑。

## 实现与证据

| 范围 | 代码提交 | 验证结论 |
| --- | --- | --- |
| VFS 设计合同 | `d4ffb8a9` | 路径、lookup、mount snapshot、generation、provider ABI、错误与所有权语义冻结 |
| Future 发布与 Watch 合同 | `e94a5d9b` | TCP/TLS Dial Future 的模块化、IOCP 与单头回归通过；API 文档明确调用期间保活责任 |
| xllm operation Future 生命周期 | `63ba83d6` | operation/watch 使用双引用 node 和 mutex 串行发布、摘除与 transport 退场；Windows 和 Linux sanitizer 回归通过 |
| native xfile backend | `087a645c` | 原生文件行为保持，backend close-once、OOM、capability 和非原生拒绝路径通过 |
| namespace/provider 核心 | `029248ba`、`0a7b7a00` | 不可变 snapshot、MISS/ERROR、重入、大小写策略和 generation 生命周期通过 |
| 原生能力与根内解析 | `5c0bd333`、`a38e949b`、`0233e4db` | native open、根句柄枚举、逐 segment 无链接解析通过 Windows/Linux 回归 |
| memory provider | `83217171` | copy/owned buffer、seal、目录派生、大小写索引和 blob 生命周期通过 |
| disk provider | `2c3e9914` | 锚定根、授权冻结、链接/特殊文件拒绝、目录改名后访问和平台回归通过 |
| pack 合同与实现 | `604f7544`、`48ac36df` | 新格式与严格 XSVPACK v1、STORE/LZMA1、缓存预算、single loader、稳定失败和统计通过 |
| pack 健壮性 | `d64cb745`、`93410a17`、`d3dc726e` | 畸形输入、OOM、有限语料、合法短读、提前 EOF、结构化 I/O 错误、Source close-once 和组合 suite 隔离通过 |
| 单头宏卫生 | `a769dadf` | xs 首次集成发现并修复 LZMA SDK 内部宏污染后续 logger/regex 标识符；`XRT_MODULE_ALL` 与排除 memory debug 的完整单头均以 warning-as-error 编译、链接并运行 |
| LZMA 符号隔离 | `36955ba1` | 对象级审计确认 decoder 只导出 `__xrtLzma*`；xs 默认宿主同时链接自身 LZMA decoder 成功 |
| 示例与文档 | `2fc7988d` | overlay 示例运行通过；VFS 公开函数、常量、类型文档完整 |

## 当前门禁复核

当前基线执行并通过：

- Windows GCC warning-as-error 的组合功能门：`file_backend_tests,vfs,vfs_memory,vfs_disk,vfs_pack_tests,vfs_pack_source_tests`，模块化和单头程序均运行；
- Windows GCC warning-as-error 的 `single_all_tests`，覆盖 `XRT_MODULE_ALL` 完整单头以及排除 memory debug 的变体；
- LZMA decoder 对象的导出符号审计，以及 xserver `32e7a23` 默认宿主与自身 LZMA decoder 的共存链接；
- `vfs_pack_oom_tests,vfs_pack_concurrency_tests` 的模块化与单头编译门，使用 `--no-run`，只验证可选 fixture 隔离，不执行高负载用例；
- `tools/amalgamate.py --check`；
- VFS API 文档覆盖检查：31 个函数、39 个常量、24 个类型，无缺项；
- 公开 ABI、release maturity 和 `git diff --check`；
- Linux Clang ASan/UBSan 的 pack source 模块化与单头低负载回归。

历史上已取得、后续不重跑的补充证据：memory/disk 的多线程和 TSan 回归、pack 的 100 线程首开、1000 轮确定性 fuzz 与 10000 轮 libFuzzer。它们只说明当时代码基线的覆盖，不改变当前低负载策略。

## ABI 与兼容性结论

- `xfile`、`xdir`、`xvfs` 和 provider 对外对象保持不透明；provider v1 回调表以 `Size + Version` 演进；
- `OpenNative` 是兼容旧 v1 大小的尾扩展，旧 provider 不需要重编译即可维持既有能力；
- 模块化头、`single/xrt.h` 与 `single/xrt_decl.h` 已同步；
- 原生 `xrtFileOpen`/`xrtDirOpen` 语义不被 VFS 接管；
- pack provider Create 成功接管 Source，失败保留调用方所有权，打开文件独立持有解压 blob；
- VFS lookup 热路径没有加入全局日志或计数器，诊断通过 MountId 与 provider 自有统计完成。

## 进入宿主集成的条件

宿主集成按以下顺序关闭最后风险：

1. 在隔离 xserver 工作树同步本门禁基线，并锁定 xrt 版本；
2. 用 xs 现有打包路径做一次带符号、带 Future/watch 生命周期日志的原故障路径启动；
3. 若启动成功，再做少量相同路径回归并确认没有 `FutureWaiterDetach` UAF、悬挂 reload generation 或资源泄漏；
4. 将结果记录到 mdo 进度账本，关闭 XRT-0 后再开始 XS 双 VFS 主体迁移。

若原打包复现必须依赖尚未迁移的 xs VFS/TCC 接口，则该验证与 XS-101 最小接入合并，但必须先于 XS-102 之后的功能扩张完成。
