# XRT

XRT 是一套可独立集成的 C 运行时与基础库，也可以作为编程语言运行时的底座。
XRT 2 已完成不兼容重构并进入发布稳定阶段；XLang 的标准库、运行时绑定和仓库同步
作为独立产品任务维护，不改变 XRT 自身的边界。

旧版源码保存在 `dev/ver1`，仅作为已经闭合的只读审计基线，不参与构建、打包或公共
契约。当前实现从核心、内存和文本能力开始，按明确依赖逐层建立并由模块、裁剪、单头、
平台和发布消费者门禁共同约束。

## 当前结构

```text
include/        模块化公共头文件
src/            按体系组织的实现
tests/          独立模块测试和单头文件测试
config/         模块、依赖和生成清单
tools/          构建、审计和单头文件工具
single/         生成的单头文件发布物
docs/           架构、契约和模块文档
extlibs/        基于 XRT 的扩展库与迁移资产
dev/ver1/       旧版只读参考实现
dev/refactor/   逐文件迁移台账
dev/bench/      发布性能与体积基准
```

## 最小验证

```text
build.bat --compiler gcc --suite core
build.bat --compiler tcc --suite core
build.bat --compiler tcc --arch x86 --suite all
```

Linux 和 macOS 可以使用：

```sh
./build.sh --compiler gcc --suite core
```

构建器只运行指定模块及其单头文件测试。完整回归会在独立发布门禁中执行，不作为每次局部修改的默认动作。

当前实现已覆盖基础、内存、容器、文本、密码、文件、并发和网络等体系；精确模块、
依赖与状态以 `config/modules.json` 为准。`--suite all` 验证全部已实现模块，
`--arch` 可选 `native`、`x86` 或 `x64`，输出按编译器、架构和模块隔离。

完整特性组合可以只验证一个模块化测试，失败后也可以从该测试继续后续序列：

```text
python tools/build.py --compiler gcc --suite all --test test_x509_store_file_oom
python tools/build.py --compiler gcc --suite all --start-test test_x509_store_file_oom
```

构建器按源码、头文件、编译器、架构和裁剪宏计算整套对象闭包指纹。闭包未变化时复用对象，`--rebuild` 可以忽略缓存强制重编。

可重复使用 `--cflag` 和 `--ldflag` 注入工具链参数；以 `-` 开头的值使用 `=` 传入。例如：

```text
python tools/build.py --compiler clang --suite coroutine --no-single --cflag=-fsanitize=address,undefined --cflag=-fno-omit-frame-pointer
```

完整文档入口见 `docs/README.md`；HTTP/1.1 与 WebSocket 的发布矩阵见
`docs/HTTP_WEBSOCKET_RELEASE.md`，当前平台运行证据与未验证目标见
`docs/RELEASE_STATUS.md`，XRT 的产品边界和功能准入规则见 `docs/SCOPE.md`。

模块化静态库和动态库由同一清单生成，不再维护逐平台源码列表：

```text
python tools/package.py --compiler gcc --arch x64 --suite all --kind static
python tools/package.py --compiler gcc --arch x64 --suite all --kind shared
```

GNU、TCC、MSVC 和 clang-cl 的产物规则与使用宏见 `docs/BUILD.md`。

裁剪依赖有独立快速门禁。它先验证每个模块的完整宏闭包可编译，再逐个移除直接依赖并要求公共头明确拒绝：

```text
python tools/build.py --compiler gcc --suite all --trim-only
python tools/build.py --compiler tcc --arch x86 --suite all --trim-only
```

## 单头文件

执行以下命令重新生成 `single/xrt.h`：

```text
python tools/amalgamate.py
python tools/amalgamate.py --check
```

第一条命令更新生成物；第二条命令只读检查 `xrt/features.h` 与 `single/xrt.h` 是否和模块清单、源码一致，适合 CI 与发布门禁。

在 POSIX 平台生成实现时，应在其他系统头之前定义 `XRT_IMPLEMENTATION` 并包含 `single/xrt.h`。该顺序允许实现单元取得所需的 GNU/POSIX.1-2008 声明和 64 位文件接口；仅包含声明不会改变系统特性宏。完整发布回归还会通过 `single_all_tests` 验证 `XRT_MODULE_ALL` 的编译和链接。

使用方式保持经典模式。XRT 2.0 使用正向模块选择，调用方只声明需要的根模块，依赖由
生成的 `xrt/features.h` 自动展开：

```c
#define XRT_MODULE_STRING
#define XRT_IMPLEMENTATION
#include "xrt.h"
```

编译器、调试器等需要完整运行库的宿主可以定义 `XRT_MODULE_ALL`。普通应用应选择实际
使用的 `XRT_MODULE_*`；完整规则见 `docs/FEATURE_SELECTION.md`。

公共 API 以 `include/`、生成的 `single/` 和对应 API 文档为唯一契约；旧版名称和开发期
临时接口不进入发布面。

HTTP 和 WebSocket 的核心只保留可组合的线协议与高性能路径。客户端、服务器、路由、
缓存、认证和其他框架型资产分别保存在 `extlibs/xhttp` 与 `extlibs/xws`，不参与 XRT
核心构建。
