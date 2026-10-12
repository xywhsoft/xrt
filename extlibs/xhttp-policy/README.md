# xhttp-policy — HTTP 策略

CookieJar、缓存存储和判断、Digest 会话/nonce/replay、重试与重定向决策。无拨号、监听或计时器。

XRT core HTTP 负责报文边界、线路收发与协议限制。xhttp 在其上提供完整请求/响应对象和字段编解码；未知字段、重复字段及顺序保留在通用 Header 容器中。现有专用字段解析器按模块选择。

## 最小选择

优先选择需要的模块，`XHTTP_POLICY_MODULE_ALL` 仅用于确实需要全部本库功能的宿主或回归测试。

```c
#define XHTTP_POLICY_MODULE_HTTP_RETRY_POLICY
#include <xhttp-policy.h>
```

网络客户端与服务器在自己的库中；xhttp 内的 `http_client_request`、`http_client_response`、`http_server_request`、`http_server_reply` 和 Exchange 是无 IO 的消息/编解码对象。名字中的 client/server 不代表拨号或监听。

## 单头与独立链接

在一个实现翻译单元中先确定闭包，核心实现提供一次，再按依赖顺序实现各扩展：

```c
#define XHTTP_POLICY_MODULE_HTTP_RETRY_POLICY
#include <xhttp-policy/features.h>
#include <xhttp/features.h>
#define XRT_IMPLEMENTATION
#include "single/xrt.h"
#define XHTTP_IMPLEMENTATION
#include "single/extlibs/xhttp.h"
#define XHTTP_POLICY_IMPLEMENTATION
#include "single/extlibs/xhttp-policy.h"
```

使用发布目录的声明头时，宿主需要先提供匹配的核心模块配置；源码仓库中的 features 负责推导闭包。其他翻译单元使用相同选择并包含声明头。扩展单头只展开本库实现，不嵌入核心或兄弟库。所有库可独立编译为翻译单元，再链接一份核心。

```sh
python tools/build.py --manifest extlibs/xhttp-policy/config/modules.json --suite xhttp_policy_tests --jobs 1
python tools/package.py --manifest extlibs/xhttp-policy/config/modules.json --suite xhttp_policy --kind static --verify
python tools/package.py --manifest extlibs/xhttp-policy/config/modules.json --suite xhttp_policy --kind shared --verify
python tools/check_release_maturity.py --release --manifest extlibs/xhttp-policy/config/modules.json
```

每个归档/DLL 只拥有本库源码。输出目录 `packages.json` 列出依赖库的链接顺序、系统库和共享库宏。静态库按目标库→依赖库→核心顺序链接；Windows DLL 使用各库自己的 `XHTTP_POLICY_BUILD_SHARED` / `XHTTP_POLICY_USE_SHARED`，核心使用 `XRT_BUILD_SHARED` / `XRT_USE_SHARED`。

## 所有权与迁移

请求、响应、Body、Header 与借用视图沿用各公共头中的所有权约定。网络 Client/Server 持有自身运行状态；调用方负责 Engine 的生命周期。Upgrade 成功后按既有 Take/Detach 契约转移传输和缓冲字节，扩展不会重置或抢占 Engine。策略对象由创建它的调用方持有并在用户结束后释放。

保留现有 C API 名称和 `XHTTP_FEATURE_*` 特性名。迁出的选择宏改用 `XHTTP_POLICY_MODULE_*`；替换旧的 xhttp 高层聚合入口，加入对应 manifest 与公共头。旧源码不能继续用 `XHTTP_MODULE_HTTP_CLIENT` / `XHTTP_MODULE_HTTP_SERVER` 选择已迁出的运行时。

完整符号见 [公共符号参考](docs/api/reference.md)，整体职责与迁移示例见 [拆分说明](../xhttp/docs/design/http-layers.md)。保留已有功能，不增加 CORS 框架、插件注册体系或新的协议产品。
