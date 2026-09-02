# XRT 产品边界

XRT 是独立发布的 C 运行时基础库。核心发布面由公开头文件、模块清单、测试、单头包和本目录的使用说明共同定义；应用不应依赖未公开的内部头文件、构建缓存或维护目录。

## 核心库

核心库包含可独立使用的基础设施：内存、文本与正则、容器与数据、并发、文件与进程、密码与 TLS、网络与协议原语、标识符和模板。它们共享一致的模块选择、错误、所有权和测试要求。

HTTP 与 WebSocket 核心提供协议解析、帧处理和传输辅助，不提供应用级客户端、服务端、路由、连接组或业务对象模型。需要这些能力时，选择相应的扩展库。

## 扩展库

`extlibs/` 中的 xruntime、xhttp、xws、xmail 和 xssh 是独立产品：它们可以复用 XRT 公开 API，但不进入核心库的依赖闭包、单头包或发布承诺。每个扩展维护自己的版本、依赖、文档和兼容性说明。

## 不在核心范围内

核心库不提供语言绑定、应用框架、ORM、浏览器策略模拟、容器隔离或跨主机调度。需要特定业务协议、宿主对象模型或语言集成时，应在应用或独立扩展中实现，而不是依赖 XRT 内部实现细节。

## 模块体系

下表是当前核心模块的产品分组；`config/modules.json` 是模块选择、依赖和平台条件的机器可读来源。

| 体系 | 状态 | 源码根目录 | 复审模块 |
|---|---|---|---|
| `foundation` | `retained` | `core, memory, text, math, hash, codec, compress, containers` | - |
| `data` | `retained` | `data, value` | - |
| `concurrency` | `retained` | `concurrency` | - |
| `system_io` | `retained` | `fs, io, system, process, logging` | - |
| `security_transport` | `retained` | `asn1, crypto, x509, tls` | - |
| `network` | `retained` | `network` | - |
| `web_protocols` | `retained` | `http, websocket` | - |
| `identifier` | `retained` | `id` | - |
| `template` | `retained` | `template` | - |
| `implementation_support` | `internal` | `internal, third_party` | - |

`retained` 表示该体系属于公开产品能力；`internal` 表示它只为公开模块提供实现支持，不能被应用直接依赖。模块是否可用仍取决于所选特性、目标平台和构建配置，具体选择方式见[特性选择](FEATURE_SELECTION.md)。

## 集成原则

应用应只包含所需的公开头文件，并使用匹配的模块化库或单头模块集。扩展库不得读取 XRT 私有头文件或复制核心实现；发现通用能力缺口时，应先以独立、可裁剪、可测试的核心 API 评估其价值。
