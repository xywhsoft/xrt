# Test Runner 改造 Spec

## 0. 当前状态

截至当前工作区，这次改造的主线已经完成：

- [test.c](/D:/git/xrt/test.c) 已经成为统一 runner。
- `test/` 目录下的测试已接入 runner 注册表，并支持 `SINGLE / PRESET / ALL`。
- `Thread` 重复入口已经收口到 [test_thread_core.h](/D:/git/xrt/test/test_thread_core.h)，旧 [test_thread.h](/D:/git/xrt/test/test_thread.h) 仅保留兼容壳。
- `FutureCore` 已不再反向依赖 `dev/test_future_core.c`，实现落在 [test_future_core_impl.h](/D:/git/xrt/test/test_future_core_impl.h)。
- `memtelemetry baseline` 已进入 `test/` 体系：[test_memtelemetry_baseline.h](/D:/git/xrt/test/test_memtelemetry_baseline.h)。
- `dev/test*.c` 独立入口已经清空。
- 默认脚本 [build_test.sh](/D:/git/xrt/build_test.sh) 与 [build_GCC_TEST_x64.bat](/D:/git/xrt/build_GCC_TEST_x64.bat) 已直接编译并调用 [test.c](/D:/git/xrt/test.c)。
- Windows 脚本不再切换到 `release/x64` 目录执行 runner，避免 TLS/HTTP/WS 测试因为工作目录变化而丢失相对路径夹具。
- `xnet_native_core` 专项脚本已在两端补齐：
	- [build_test_xnet_native_core.sh](/D:/git/xrt/build_test_xnet_native_core.sh)
	- [build_GCC_TEST_XNET_NATIVE_CORE_x64.bat](/D:/git/xrt/build_GCC_TEST_XNET_NATIVE_CORE_x64.bat)
	并且都支持“无参数直接跑默认轮次、需要时再覆盖参数”。
- 测试产物清理脚本已补齐：
	- [build_clean_test.sh](/D:/git/xrt/build_clean_test.sh)
	- [build_GCC_CLEAN_TEST_x64.bat](/D:/git/xrt/build_GCC_CLEAN_TEST_x64.bat)
	只清理 runner/专项脚本生成的可执行文件与派生输出，不删除跟踪夹具。

当前保留的策略：

- [build_test_xnet_native_core.sh](/D:/git/xrt/build_test_xnet_native_core.sh) 继续保留为专项 native backend 回归脚本。
- [build_GCC_TEST_XNET_NATIVE_CORE_x64.bat](/D:/git/xrt/build_GCC_TEST_XNET_NATIVE_CORE_x64.bat) 作为 Windows 对应专项脚本一并保留。
- 顶层默认脚本只跑 curated modern smoke，不把 `xnet_native_core` 混入默认批量回归。
- 阶段性 runner 产物目前主要通过 [.gitignore](/D:/git/xrt/.gitignore) 收口，物理删除可留到人工清理阶段。
- 如需物理清理，可直接运行 [build_clean_test.sh](/D:/git/xrt/build_clean_test.sh) 或 [build_GCC_CLEAN_TEST_x64.bat](/D:/git/xrt/build_GCC_CLEAN_TEST_x64.bat)。

## 1. 背景

改造开始前，测试体系存在以下问题：

- 根入口 [test.c](/D:/git/xrt/test.c) 依赖手工注释开关，无法作为稳定 runner 使用。
- `test/` 目录中已有 59 个测试头，但 [test.c](/D:/git/xrt/test.c) 只接入了其中一部分。
- 默认构建入口分叉：
	- [build_GCC_TEST_x64.bat](/D:/git/xrt/build_GCC_TEST_x64.bat) 与 [build_test.sh](/D:/git/xrt/build_test.sh) 默认走 modern 分支。
	- [build_GCC_TEST_LEGACY_x64.bat](/D:/git/xrt/build_GCC_TEST_LEGACY_x64.bat) 与 [build_test_legacy.sh](/D:/git/xrt/build_test_legacy.sh) 仍然编译 [test.c](/D:/git/xrt/test.c)。
- `dev/test*.c` 中存在大量独立入口，部分只是包装器，部分已经与 `test/` 中的测试出现同类重复。

本次改造目标是把 [test.c](/D:/git/xrt/test.c) 变成唯一手工回归入口，同时保留“直接运行 `test.exe`、不记命令行、控制台尽量干净”的使用习惯。


## 2. 目标

### 2.1 用户侧目标

- 继续支持直接运行 `test.exe` / `./xrt_test`。
- 不强依赖命令行参数。
- 日常快速验证时，只需要改 [test.c](/D:/git/xrt/test.c) 顶部一个配置区即可切换测试目标。
- 单测运行时默认只输出当前选中测试的内容，不额外展开无关模块。

### 2.2 工程侧目标

- [test.c](/D:/git/xrt/test.c) 成为统一 runner。
- `test/` 目录中的测试全部纳入 runner 注册表。
- 同类测试逐步合并，移除 `dev/test*.c` 中纯包装器式入口。
- 最终将默认测试构建脚本重新收敛到 [test.c](/D:/git/xrt/test.c)。

### 2.3 非目标

- `examples/`、`dev/bench/`、`fuzz`、`singlehead` 不并入 [test.c](/D:/git/xrt/test.c)。
- 本轮不追求把所有旧测试一次性重写为统一断言风格。


## 3. 设计原则

- 顶部配置优先，不以 CLI 作为主入口。
- 单次运行只选一个测试或一个预设组，避免控制台噪声。
- runner 自身只负责调度、初始化、结果汇总，不侵入具体测试逻辑。
- 允许测试接口暂时不统一，由 runner 通过适配器收口。
- 先接入，再去重；先把测试找得到、跑得通，再做结构归并。


## 4. Runner 方案

### 4.1 顶部配置区

在 [test.c](/D:/git/xrt/test.c) 顶部保留一个小而稳定的配置区，例如：

```c
#define XRT_TEST_RUN_MODE			XRT_TEST_MODE_SINGLE
#define XRT_TEST_SINGLE_ID			XRT_TEST_ID_MEMGLOBAL_CORE
#define XRT_TEST_PRESET_ID			XRT_TEST_PRESET_MEMORY_SMOKE
#define XRT_TEST_FAIL_FAST			1
#define XRT_TEST_RUNNER_QUIET		1
```

支持 3 种模式：

- `XRT_TEST_MODE_SINGLE`
- `XRT_TEST_MODE_PRESET`
- `XRT_TEST_MODE_ALL`

默认推荐使用 `SINGLE`。

### 4.2 测试注册表

在 `test/` 下新增统一注册表文件，例如：

- `test/test_runner_catalog.h`
- `test/test_runner_preset.h`

每个测试条目至少包含：

- `iTestId`
- `sTestName`
- `sGroupName`
- `iFlags`
- `procRun`

`iFlags` 用于描述运行条件，例如：

- `XRT_TEST_F_NEED_XRT`
- `XRT_TEST_F_NEED_WSA`
- `XRT_TEST_F_WINDOWS_ONLY`
- `XRT_TEST_F_LINUX_ONLY`
- `XRT_TEST_F_DEBUG_ONLY`
- `XRT_TEST_F_STRESS`

### 4.3 适配器层

由于现有测试接口不统一，runner 不直接暴露原始接口，而是为每个测试提供一个薄适配器，统一签名：

```c
typedef int (*xrt_test_proc)(xrtGlobalData* pCore);
```

适配器负责兼容以下现状：

- `void Test_xxx(xrtGlobalData* pCore)`
- `void Test_xxx(void)`
- `bool Test_xxx(void)`
- 需要临时启用 `xrtMemDebugEnable(TRUE)` 的测试
- 需要 `WSAStartup()` 的测试

### 4.4 输出策略

runner 只输出统一短格式：

- `[RUN ] name`
- `[PASS] name`
- `[FAIL] name`
- `[SKIP] name`

具体测试自己的 `printf` 保留。  
在 `SINGLE` 模式下，这已经足够保持控制台干净。


## 5. 第一阶段：先把 test 目录下的测试都接进来

### 5.1 目标

- [test.c](/D:/git/xrt/test.c) 能调度 `test/` 目录下全部 59 个测试头。
- 不再依赖散落在 `main()` 中的手工注释调用。
- 不改默认脚本指向，先把 runner 本体做稳定。

### 5.2 范围

本阶段只保证“`test/` 目录下的测试都能被 runner 识别和选择”。

需要补接入的测试头包括：

- `test_memdebug_core.h`
- `test_memglobal_core.h`
- `test_mempool_core.h`
- `test_memtelemetry.h`
- `test_temparena_core.h`
- `test_tls_boundary.h`
- `test_xurl.h`
- `test_xnet_http.h`
- `test_xnet_httpd.h`
- `test_xnet_ws.h`
- `test_xnet2_base.h`
- `test_xnet2_codec.h`
- `test_xnet2_dgram.h`
- `test_xnet2_engine.h`
- `test_xnet2_mem.h`
- `test_xnet2_port.h`
- `test_xnet2_stream.h`
- `test_xnet2_sync.h`
- `test_xnet2_tls.h`

### 5.3 阶段性妥协

以下结构性问题在第一阶段允许暂时保留，但必须登记为二期债务：

- 某些网络测试头仍保持旧式打印风格，而不是 runner 风格结果返回。
- 旧测试如果本身会 `exit()`，runner 先接受，二期再统一。

### 5.4 第一阶段验收

- 无参数运行 `test.exe` 时，可通过顶部配置稳定选中任意一个 `test/` 测试。
- 不需要再在 `main()` 里手工注释/取消注释测试调用。
- 顶部配置切换一个测试的成本固定为“改 1 处配置”。


## 6. 第二阶段：合并测试同类项，补齐缺少的测试边界

### 6.1 目标

- 收敛 `dev/test*.c` 中的重复入口。
- 把真正的测试实现移回 `test/` 体系。
- 补齐 currently missing 的边界测试。
- 默认测试构建脚本重新指向 [test.c](/D:/git/xrt/test.c)。

### 6.2 需要合并的同类项

#### A. 纯包装器型入口

这些文件只做 `xrtInit/WSAStartup/调用测试/退出`，本轮已删除并并入 runner：

- `dev/test_memglobal_core.c`
- `dev/test_mempool_core.c`
- `dev/test_memdebug_core.c`
- `dev/test_memtelemetry_core.c`
- `dev/test_temparena_core.c`
- `dev/test_tls_boundary_core.c`
- `dev/test_xurl_core.c`
- `dev/test_xhttp_only.c`
- `dev/test_xnet2_sync_core.c`
- `dev/test_coroutine_core.c`

#### B. 组合调度型入口

这些文件代表“一个预设测试组”，本轮已经完成收口：

- `dev/test_xnet2_stage.c`
- `dev/test_memtelemetry_baseline.c`

对应结果：

- `xnet2_stage` 已落为 runner preset。
- `memtelemetry_baseline` 已落为 runner 测试项。

#### C. 真正独立实现型入口

这些文件原本不是简单包装器，本轮已完成提炼并删除入口：

- `dev/test_thread_core.c`
- `dev/test_coroutine_min.c`
- `dev/test_xnet_windows_native_core.c`
- `dev/test_xnet_linux_native_core.c`
- `dev/test_xnet2_listener_accept_core.c`
- `dev/test_future_core.c`

### 6.3 当前剩余债务

- `xnet` 测试当前仍通过 [test_xnet_internal_env.h](/D:/git/xrt/test/test_xnet_internal_env.h) 接入内部实现环境；虽然命名已收口，但后续仍可继续评估是否要把这层环境进一步模块化。
- 默认脚本虽然已经直达 runner，但 `modern` / top-level 仍存在兼容别名关系，后续如果不需要兼容名可以进一步删减。
- `release/x64` 下当前累积的阶段性可执行文件和输出文本，现已可通过专门的 clean 脚本清理；是否自动执行由使用者自行决定。
- `xnet_native_core` 继续保留为专项脚本，不纳入默认 modern smoke。

### 6.4 边界补齐方向

二期补边界优先考虑：

- runner 对 `DEBUG_ONLY` / `WINDOWS_ONLY` / `LINUX_ONLY` 的跳过行为
- `memdebug` 与 `temparena` 的调试态入口
- `thread` 的 auto-attach、nested attach、cleanup、timeout 边界
- `xnet native` 的 Windows/Linux 原生后端边界
- `listener accept` 的 future / waitsource / coroutine 三条路径
- `coroutine_min` 作为轻量 smoke

### 6.5 第二阶段验收

- `dev/test*.c` 中不再保留纯包装器式入口。
- 默认测试构建脚本重新收敛到 [test.c](/D:/git/xrt/test.c)。
- `test.c` 可覆盖“单测、预设组、全量”三种回归方式。

当前验收结果：以上 3 条已经满足。


## 7. 预设组建议

建议 runner 内置以下预设组：

- `SMOKE_MEMORY`
- `SMOKE_RUNTIME`
- `SMOKE_THREAD`
- `SMOKE_NET`
- `XNET2_STAGE`
- `MEMTELEMETRY_BASELINE`
- `ALL_HEADERS`

其中默认建议：

- 日常快速验证：`SINGLE`
- 合并前自检：`SMOKE_*`
- 大回归：`ALL_HEADERS`


## 8. 执行顺序建议

建议按以下顺序落地：

1. 重写 [test.c](/D:/git/xrt/test.c) 顶部配置区与 runner 主循环。
2. 建立 `catalog + preset + adapter` 三层结构。
3. 先把 `test/` 下 59 个测试全部接入。
4. 保留现有 modern/legacy 脚本不动，先验证 runner 可用。
5. 开始处理 `dev/test*.c` 的纯包装器合并。
6. 处理 `FutureCore`、`Thread`、`xnet native`、`listener accept` 这些真正结构重复点。
7. 最后统一默认构建脚本。


## 9. 下一步建议

如果继续收尾，优先级建议如下：

1. 视需要继续收口 [test_xnet_internal_env.h](/D:/git/xrt/test/test_xnet_internal_env.h) 的职责边界。
2. 决定是否保留 `modern` 兼容脚本名；如果不需要，可以再删一层别名脚本。
