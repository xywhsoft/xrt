# 功能选择与精细裁剪

## 公开契约

XRT 2.0 使用正向模块选择。用户只声明需要的根模块，XRT 根据
`config/modules.json` 自动展开完整依赖闭包：

```c
#define XRT_MODULE_STRING_SPLIT
#define XRT_IMPLEMENTATION
#include "xrt.h"
```

`XRT_MODULE_STRING_SPLIT` 会同时启用字符串基础能力和切分能力。用户不需要知道模块
当前依赖哪些内部特性，也不应复制依赖表。

模块选择宏必须在当前翻译单元第一次包含 `xrt.h` 或 `xrt/features.h` 前定义。一个实现
翻译单元只能定义一次 `XRT_IMPLEMENTATION`。模块化链接时，所有参与同一 XRT 实例的
翻译单元应使用一致的模块集合。

需要完整运行库的宿主、编译器或调试工具可以使用：

```c
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include "xrt.h"
```

内存调试会改变全局分配路径。需要完整生产功能但不需要内存调试时，可以只排除这一
侵入式诊断模块：

```c
#define XRT_MODULE_ALL
#define XRT_EXCLUDE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "xrt.h"
```

`XRT_EXCLUDE_MEMORY_DEBUG` 同时排除 `memory_debug` 和
`memory_debug_report`，但保留 `memory_stats`。它只抑制 `XRT_MODULE_ALL` 的隐式选择；
显式定义 `XRT_MODULE_MEMORY_DEBUG` 或 `XRT_MODULE_MEMORY_DEBUG_REPORT` 时，显式根模块
仍然优先。

普通应用不应默认选择全部模块。

没有定义任何 `XRT_MODULE_*` 时只提供无特性宏的 Core 契约，不会隐式启用完整库。
这条规则使最小集成保持可预测，也避免新增模块在未来版本中悄悄增加现有程序体积。
例如结构化错误对象和当前错误槽属于 Core，printf 错误消息构造则由
`XRT_MODULE_ERROR_FORMAT` 独立选择，不会把格式化运行时强制链接进最小动态库。

## 裁剪粒度

一个公开模块对应一小组具有共同状态、依赖和使用场景的能力，不按单个函数拆宏，也
不把整个体系压成一个总开关。例如 Queue 的基础、SPSC、MPSC、MPMC、等待、取消、
Select 和协程桥接分别选择；网络的 Engine、Select/epoll/kqueue/io_uring/IOCP、TCP、
UDP、同步、Future、DNS、代理和 TLS 也分别形成闭包。

只有无法独立成立、拆开后只会暴露内部状态的实现细节才跟随父模块。新增公共能力时
必须先判断它能否独立测试、独立裁剪和独立说明所有权；满足这些条件就应建立自己的
模块，而不是继续扩大已有总宏。

## 两层宏

- `XRT_MODULE_*` 是公开的根模块选择层，面向应用和其他 C 宿主。
- `XRT_FEATURE_*` 是实现与依赖保护层，面向 XRT 自身构建和精确故障测试。

公开模块宏由 `tools/generate_features.py` 生成到 `include/xrt/features.h`，单头生成器再把
同一内容合入 `single/xrt.h`。模块化头和单头文件因此使用同一份依赖关系。

直接定义 `XRT_FEATURE_*` 仍可用于 XRT 内部测试，但调用方必须自行提供全部依赖，缺少
直接依赖时公共头会以 `#error` 拒绝编译。这不是推荐的应用集成方式。

## 声明与实现保证

裁剪完成必须同时满足两件事：未选择模块的公共声明不可见，对应实现也不进入最终
产物。两种发布方式通过不同路径实现同一结果：

- 模块化构建和 `tools/package.py` 只编译所选根模块的依赖闭包源码。
- `single/xrt.h` 包含统一生成物，但每个声明和实现块由同一组
  `XRT_FEATURE_*` 保护，预处理后未选择实现不会产生代码。

测试、模糊入口和互操作驱动只能登记在 `test_sources`、`tests` 或 `single_tests`，
不能作为普通 `sources` 混入发布闭包。模块没有独立源文件不代表不能独立裁剪；相邻
小模块可以共享物理文件，但每段实现仍必须使用自己的特性保护，并接受单头门禁。

## 平台依赖

平台后端也由模块清单选择。例如 `XRT_MODULE_NET_ENGINE` 在 Windows 选择 IOCP 与
`select` fallback，在 Linux 选择 epoll 与 `select` fallback，在 macOS/BSD 选择 kqueue
与 `select` fallback。应用层只选择网络引擎，不直接复制平台判断。

明确需要额外后端时仍可叠加根模块，例如 Linux 发布构建可以同时声明
`XRT_MODULE_NET_ENGINE` 与 `XRT_MODULE_NET_PORT_URING`。

## 生成与验证

修改模块名称、特性宏或依赖后执行：

```text
python tools/generate_features.py
python tools/amalgamate.py
```

只检查生成物是否最新：

```text
python tools/generate_features.py --check
```

裁剪门禁会对每个有特性宏的模块执行两类检查：

1. 只定义对应 `XRT_MODULE_*`，完整闭包必须可编译。
2. 直接定义 `XRT_FEATURE_*` 闭包并逐个移除直接依赖，公共头必须拒绝编译。

```text
python tools/build.py --compiler gcc --suite all --trim-only
python tools/build.py --compiler tcc --arch x86 --suite all --trim-only
```

`tests/single/test_single_module_selection.c` 还会实际运行一个只选择字符串切分模块的
单头程序，防止生成头只在声明层通过而遗漏实现。

旧版 `XRT_NO_*` 和 `XRT_MINIMAL` 负向宏不再保留。负向模式会让新增功能默认进入旧
构建，并要求在多个位置维护父子关闭关系；正向根选择使新增模块默认不增加调用方的
声明、实现或依赖。`XRT_EXCLUDE_MEMORY_DEBUG` 是只约束 `XRT_MODULE_ALL` 的窄例外，
不构成通用负向选择体系。旧 `XRT_NO_NETWORK`、`XRT_NO_NETTLS`、`XRT_NO_XHTTP` 等
名称不能与当前宏混用，也不提供兼容映射。
