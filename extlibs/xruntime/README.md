# xruntime

`xruntime` 是建立在 XRT 之上的运行时模型扩展库，包含类型描述、类型转换、
运行时对象、调用模型、对象图以及拥有型 typed 容器。

这些能力不再进入 XRT 核心清单、核心 umbrella 或 `single/xrt.h`。`xvalue`、
基础容器、线程、Future 和任务仍由 XRT 提供，`xruntime` 只在应用明确选择时
增加运行时抽象。

## 使用

模块化源码构建在包含头文件前选择所需模块，或选择完整扩展：

```c
#define XRUNTIME_MODULE_ALL
#include <xruntime.h>
```

独立单头已经包含所需 XRT 实现和全部 xruntime 源码，功能仍按模块宏裁剪：

```c
#define XRUNTIME_MODULE_RUNTIME_TYPE
#define XRUNTIME_IMPLEMENTATION
#include "xruntime.h"
```

生成文件位于 `single/xruntime.h`，纯声明版本位于
`single/xruntime_decl.h`。`XRUNTIME_IMPLEMENTATION` 只能在一个翻译单元定义。

扩展直接复用 XRT 容器的内部快速路径，不复制底层实现。正式的静态库和动态库产物
都包含当前 xruntime 所需的裁剪后 XRT 闭包；应用只链接 `libxruntime` 或
`xruntime.dll`，不能再同时链接另一份包含相同 XRT 闭包的库。

## 发布门禁

模块化回归使用独立测试根，测试专用的 `memory_debug` 不进入生产 `xruntime` 根：

```text
python tools/build.py --manifest extlibs/xruntime/config/modules.json --suite xruntime_tests --no-single
```

单头回归、生成一致性和发布包验证：

```text
python tools/build.py --manifest extlibs/xruntime/config/modules.json --suite xruntime --start-single-test test_single_runtime_call --jobs 8
python tools/amalgamate.py --manifest extlibs/xruntime/config/modules.json --check
python tools/check_release_maturity.py --manifest extlibs/xruntime/config/modules.json --release
python tools/package.py --manifest extlibs/xruntime/config/modules.json --suite xruntime --kind static --verify
python tools/package.py --manifest extlibs/xruntime/config/modules.json --suite xruntime --kind shared --verify
```

性能门禁分别覆盖类型/对象/调用、拥有式容器/三种并发类型队列、对象图回收；体积
门禁分别记录核心模型和完整运行时闭包：

```text
python tools/measure_performance.py --config extlibs/xruntime/config/performance_profiles.json --manifest extlibs/xruntime/config/modules.json --profiles "*" --smoke
python tools/measure_size.py --config extlibs/xruntime/config/size_profiles.json --profiles runtime_core,runtime_full --kind single --kind static
```

公开声明位于 `include/`，实现位于 `src/`，原有模块化测试和示例分别位于
`tests/` 与 `examples/`。完整符号索引见
[公共符号参考](docs/api/reference.md)，对象与所有权模型见
[运行时对象模型](docs/design/runtime_object_model.md)。
