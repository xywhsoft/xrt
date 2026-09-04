# 构建与发布

## 成熟度门禁

仓库结构检查由 `tools/check_release_maturity.py` 统一执行：

```text
python tools/check_release_maturity.py
python tools/check_release_maturity.py --release
```

普通模式检查模块状态、登记资产、生产代码清单、模块测试、单头测试、文档、性能配置和体积配置。测试与文档按依赖闭包计算，允许聚合节点验证底层实现，但不允许任何生产代码脱离清单。

`--release` 是正式发布门禁；除上述检查外，它还会拒绝 `developing` 模块和 `review` 体系。只有目标平台的真实运行证据齐全后，严格模式才应通过。

XRT 使用模块清单驱动测试、裁剪、单头生成和库产物，避免在不同平台脚本中重复维护
源码列表、依赖、链接库和测试名称。
仓库工具支持 Python 3.8 及以上版本，便于在长期支持发行版上直接生成单头和执行门禁。

## 局部验证

`tools/build.py` 编译并运行指定根模块的模块化测试、示例和单头测试：

```text
python tools/build.py --compiler gcc --suite core,string
python tools/build.py --compiler tcc --arch x86 --suite all
python tools/build.py --compiler clang --suite net --no-single
```

`--suite` 接受一个模块、逗号分隔的多个根模块或 `all`。构建器从
`config/modules.json` 计算完整依赖闭包，不维护第二份源码清单。`--test`、
`--start-test`、`--single-test` 和 `--start-single-test` 可以缩小或续接回归范围；
`--exclude-test` 与 `--exclude-single-test` 可重复使用 glob，透明排除目标机缺失的运行时
能力测试；排除项只能用于兼容性分层，不能作为对应能力的发布证据。`--no-examples` 可在
完整测试门禁中跳过示例，`--trim-only` 只执行裁剪依赖门禁。

交叉编译使用 `--target-platform` 覆盖宿主平台的依赖和测试资产选择，使用 `--runner`
把生成的测试交给外部执行器。runner 命令会收到本地可执行文件路径作为最后一个参数；
不指定这两个参数时，构建器保持原有的本机编译与执行行为。Android AArch64 的自包含
协程测试可直接使用 NDK Clang 和仓库提供的 adb runner：

```text
python tools/build.py \
  --compiler "$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang" \
  --target-platform android \
  --suite coroutine,coroutine_scheduler,coroutine_event,future_coroutine,task_coroutine,task_group_coroutine,channel_coroutine \
  --no-single \
  --runner python3 tools/run_adb.py
```

完整 Android 实机回归应同时覆盖模块化实现、单头实现和裁剪依赖闭包：

```text
export ANDROID_SERIAL=<设备序列号>
export XRT_ADB=<adb 路径>

python tools/build.py \
  --compiler "$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang" \
  --target-platform android --suite all --no-single --no-examples \
  --runner python3 tools/run_adb.py

python tools/build.py \
  --compiler "$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang" \
  --target-platform android --suite all --start-single-test test_single \
  --no-examples --runner python3 tools/run_adb.py --jobs 4

python tools/build.py \
  --compiler "$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang" \
  --target-platform android --suite all --trim-only --no-examples
```

AArch64 协程发布门禁还必须分别使用 `-mbranch-protection=standard`、
`-fsanitize=shadow-call-stack` 以及两者组合重建并实机执行
`test_coroutine_abi_aarch64`。该测试连续切换 4096 次，检查 AAPCS64 非易失整数寄存器、
SIMD 寄存器、栈对齐和宿主/协程 ShadowCallStack 隔离。

`tools/run_adb.py` 依次检查设备、上传测试、执行并传播远端退出码，默认在结束后删除远端
可执行文件。多设备环境使用 `ANDROID_SERIAL` 或 runner 的 `--serial`；adb 不在 PATH 时
使用 `XRT_ADB` 或 `--adb`。Android 明确采用 `epoll + select` 网络后端，不会因为同时
定义 `__linux__` 而误启用尚未建立 Android 契约的 io_uring 路径。
Android shell 的 SELinux 策略可能拒绝创建硬链接或 FIFO；测试必须验证库返回结构化
`XERR_PERMISSION`，不能把平台能力限制伪装成功，也不能因此跳过符号链接和普通文件语义。

扩展库的产品级测试聚合可声明 `collect_dependency_assets`，取值只能是 `tests`、
`single_tests` 和 `examples`。构建器只从该聚合的依赖闭包中收集相同模块宏前缀的资产，
不会把核心 XRT 或另一个扩展库的测试误纳入；未声明的资产类型仍只使用聚合节点自身
登记的路径。正式扩展清单还必须提供与 `product` 同名的根模块，并覆盖产品内全部公开
功能模块。

## 单头文件

```text
python tools/amalgamate.py
python tools/amalgamate.py --check
```

生成结果为 `single/xrt.h`。调用方定义需要的 `XRT_MODULE_*` 根宏；实现单元再定义
`XRT_IMPLEMENTATION`。完整宿主可以选择 `XRT_MODULE_ALL`；完整生产构建可同时定义
`XRT_EXCLUDE_MEMORY_DEBUG` 以裁掉内存调试核心和报告层。普通应用应只选择实际使用
的模块。生成器按清单和真实本地 include 图稳定排序、按来源文件去重，并拒绝任何会被
展开规则移除却没有登记进清单的本地头；生成物不含时间戳，相同输入必须逐字节一致。
`--check` 不写文件，并先验证特性头，避免两份同时过期的生成物彼此掩盖。

## 生成文档

示例索引和大型 API 家族的公共符号参考都由清单与源码生成：

```text
python tools/generate_example_index.py
python tools/generate_api_reference.py
```

发布和 CI 使用 `--check`，确保新增、删除或改名公共符号后文档不会静默过期。

## 静态库与动态库

`tools/package.py` 只生成库，不以打包成功代替测试：

```text
python tools/package.py --compiler gcc --arch x64 --suite all --kind static
python tools/package.py --compiler gcc --arch x64 --suite all --kind shared
python tools/package.py --compiler tcc --arch x86 --suite core,string --kind static
```

发布门禁应追加 `--verify`。它会从独立翻译单元包含公共头、链接刚生成的库并运行
`xrtVersion`，从而同时检查归档格式、导入库、动态符号和运行时装载路径。静态库还会
用 whole-archive 强制解析全部归档成员；TCC 缺少可移植 whole-archive 参数时改为直链
全部对象，因此最小消费者不能隐藏未解析依赖：

```text
python tools/package.py --compiler gcc --suite all --kind static --verify
python tools/package.py --compiler gcc --suite all --kind shared --verify
```

GNU、Clang 和 TCC 使用 GNU 风格对象与归档器；`--archiver` 可以显式指定 `ar` 或
`llvm-ar`。Windows 的 GCC/Clang 动态库同时生成 `libxrt.dll.a`，TCC 只承诺生成
DLL，因此其 Windows shared 模式不能使用 `--verify`；需要可链接发布物时应选择
静态库或带导入库的工具链。Linux 和 macOS 分别生成 `.so` 与 `.dylib`。
GNU 静态归档按有界命令长度分批追加对象，再统一生成索引，不要求归档器支持
`@response-file`，因此可以兼容旧版 binutils。

MSVC 或 clang-cl 必须从已经初始化目标架构的 Developer Command Prompt 运行：

```text
python tools/package.py --compiler cl --arch x64 --suite all --kind static
python tools/package.py --compiler cl --arch x64 --suite all --kind shared
```

MSVC 使用 `lib.exe`，clang-cl 可以使用 `llvm-lib`；也可通过 `--archiver` 明确选择。
若开发环境暴露 `VSCMD_ARG_TGT_ARCH`，构建器会拒绝与 `--arch` 不一致的目标。

发布前可先预览并清理工作树中的可再生输出：

```text
python tools/clean.py
python tools/clean.py --apply
```

默认模式只处理构建目录、工具缓存和已知测试输出。`--history` 会额外处理维护目录中的
可再生产物；它不会删除受版本控制保护的源码、测试、文档或脚本。

库按工具链、架构、模块组合和种类隔离到 `release/`。模块化库只包含 `--suite`
闭包；使用方包含头文件时必须选择同一组或更窄的根模块，不能调用未编入库的符号。
Windows 动态库使用方还应定义 `XRT_USE_SHARED`，构建器本身会为动态库对象定义
`XRT_BUILD_SHARED`。静态库不需要这两个宏。

## 性能报告

`tools/measure_performance.py` 按 `config/performance_profiles.json` 编译当前裁剪单头，
串行执行预热与正式样本，并输出包含环境身份、原始样本、中位数和噪声统计的 JSON。
日常和 CI 只运行小规模 smoke：

```text
python tools/measure_performance.py --compiler gcc --arch x64 --smoke
```

发布候选在固定机器上与同环境基线比较：

```text
python tools/measure_performance.py --compiler E:/software/w64devkit/bin/gcc.exe --arch x64 --baseline dev/bench/performance/PERFORMANCE_BASELINE_WINDOWS_GCC16_X64.json --check
```

工具会拒绝 CPU、亲和范围、电源计划、编译器、参数、样本策略或 runner 指纹不同的
比较。共享 CI 主机不执行绝对性能比较，只负责严格编译、真实 smoke 和工具边界测试。

## 体积报告

`tools/measure_size.py` 对代表性裁剪组合生成可重复的发布体积报告。默认测量配置中的
全部 profile 和单头、静态库、动态库三类产物：

```text
python tools/measure_size.py --compiler gcc --arch x64
python tools/measure_size.py --compiler gcc --arch x64 --kind single
python tools/measure_size.py --compiler gcc --arch x64 --profiles network,http_websocket
python tools/measure_size.py --compiler gcc --arch x64 --profiles all
```

`--profiles *` 表示全部 profile；`--profiles all` 只表示完整库，二者没有重叠含义。
`--kind` 可以重复指定。单头实现以 `-O2 -Werror` 编译，静态库和动态库复用
`tools/package.py` 的发布闭包，因此体积门禁也会验证真实发布构建。

正式基线比较必须显式提供同环境报告：

```text
python tools/measure_size.py --compiler E:/software/w64devkit/bin/gcc.exe --arch x64 --baseline dev/bench/size/SIZE_BASELINE_WINDOWS_GCC16_X64.json --check
```

报告只允许比较相同的平台、机器架构、编译器家族和版本、`size` 工具版本、目标架构、
优化级别、strip 状态及 profile 模块闭包。路径不参与比较。默认增长阈值登记在
`config/size_profiles.json`；超过阈值必须定位原因，不能用不同工具链或直接覆盖基线
消除失败。

## 发布顺序

一个发布候选至少按以下顺序验证：

1. 核心 XRT 对目标编译器运行 `tools/build.py --suite all`；扩展库运行其产品测试聚合，避免跨产品重复回归。
2. 运行 `--trim-only` 验证全部直接依赖守卫。
3. 运行 `tools/amalgamate.py` 并完成 `single_all_tests`。
4. 用 `tools/package.py --verify` 生成并验证需要发布的静态库和动态库。
5. 运行 `tools/measure_size.py --check` 比较同工具链发布体积。
6. 运行 `tools/measure_performance.py --check` 比较固定机器性能基线。
7. 执行协议、跨平台、sanitizer 和 fuzz 门禁。

新增模块应通过清单登记依赖、测试和发布条件，使构建与发布工具取得同一依赖闭包。
