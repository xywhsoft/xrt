# XRT

XRT 是面向 C 程序的可组合运行时基础库。它提供字符串与结构化模式匹配、正则、容器、任务与并发、文件与进程、网络、TLS、JSON/XSON、模板、日志等一致的基础能力；应用可以只启用需要的模块。

## 快速开始

嵌入项目时，可以使用模块化源码与头文件，也可以使用 `single/xrt.h` 单头发布包。单头模式先选择模块，再定义实现：

```c
#define XRT_MODULE_STRING
#define XRT_MODULE_JSON
#define XRT_IMPLEMENTATION
#include "xrt.h"
```

不要同时在多个翻译单元定义 `XRT_IMPLEMENTATION`。模块化接入、编译器要求和第三方依赖见[构建与发布](docs/BUILD.md)。

## 文档入口

- [使用文档](docs/README.md)：按任务查找指南、设计说明和 API 参考。
- [特性选择](docs/FEATURE_SELECTION.md)：选择最小模块集和单头构建开关。
- [示例](docs/EXAMPLES.md)：从可编译示例开始集成。
- [发布支持](docs/RELEASE_STATUS.md)：平台、验证边界和发布前检查。

## 仓库结构

| 路径 | 内容 |
|---|---|
| `include/` | 公开 C API 头文件 |
| `src/` | 模块化实现 |
| `single/` | 生成的单头发布包 |
| `examples/` | 最小可运行示例 |
| `tests/` | 回归与兼容性测试 |
| `docs/` | 用户文档、设计说明和 API 参考 |
| `config/`、`tools/` | 构建、模块选择和发布校验工具 |

`extlibs/` 中的项目是可独立发布的扩展库，不是核心库 API 的前置依赖；其边界和许可证见[范围说明](docs/SCOPE.md)与[第三方组件](docs/THIRD_PARTY.md)。

## 参与构建

Windows 可运行 `build.bat`，POSIX shell 可运行 `./build.sh`。完整的构建、测试、单头生成与发布步骤见[构建与发布](docs/BUILD.md)。
