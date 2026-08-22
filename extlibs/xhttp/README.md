# xhttp

`xhttp` 是建立在 XRT HTTP/1 快速路径、网络与 TLS 底座之上的高级协议扩展库。
它提供完整 URL、Query、表单、Multipart、Cookie、缓存、认证、流式 Body、SSE、
客户端、服务器、代理、连接池、路由、中间件、Mux、静态文件与响应压缩能力。

XRT 核心只保留 Host/authority、四种 request-target、HTTP/1 报文边界、
Content-Encoding、TCP/TLS 和 WebSocket Upgrade 所需能力。直接读取报文并写出
预构建响应的高性能路径不依赖 xhttp；应用需要结构化对象和经典场景封装时再选择
xhttp，底层能力不会被高级抽象遮蔽。

## 使用

选择完整扩展：

```c
#define XHTTP_MODULE_ALL
#include <xhttp.h>
```

单头模式在一个翻译单元中额外定义实现宏：

```c
#define XHTTP_MODULE_HTTP_CLIENT
#define XHTTP_IMPLEMENTATION
#include "xhttp.h"
```

`single/xhttp.h` 包含所需 XRT 实现，功能仍按模块宏裁剪；
`single/xhttp_decl.h` 只提供声明。`XHTTP_IMPLEMENTATION` 只能定义一次。

## 发布门禁

产品闭包和示例使用 `xhttp`，OOM、线程、平台后端与组合回归使用独立的
`xhttp_tests`。测试专用依赖不会进入发布库：

```text
python tools/build.py --manifest extlibs/xhttp/config/modules.json --suite xhttp --no-single
python tools/build.py --manifest extlibs/xhttp/config/modules.json --suite xhttp_tests --no-single
python tools/build.py --manifest extlibs/xhttp/config/modules.json --suite xhttp_tests --start-single-test test_single_xhttp --jobs 8
python tools/amalgamate.py --manifest extlibs/xhttp/config/modules.json --check
python tools/generate_api_reference.py --manifest extlibs/xhttp/config/modules.json --check
python tools/check_api_docs.py --manifest extlibs/xhttp/config/modules.json
```

公开声明位于 `include/`，实现位于 `src/`，测试和示例分别位于 `tests/` 与
`examples/`。完整符号索引见 [公共符号参考](docs/api/reference.md)。

`archive/` 不参与构建，只保存重构前的历史快照。CORS 浏览器策略明确不属于
xhttp 活动能力，仍只保留在归档区。
