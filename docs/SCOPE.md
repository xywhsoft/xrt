# XRT 产品边界与功能准入

## 当前任务边界

XRT 2.0 当前只完成 XRT 自身：公共 C API、实现、模块依赖、裁剪、单头文件、构建、
测试、文档、体积、性能和跨平台运行证据共同构成验收范围。

XLang 的标准库、编译器宿主、运行时绑定、符号注册、生成代码和单头同步由 XLang 仓库
独立维护，不进入当前仓库的发布构建。XRT 核心、`xruntime`、`xregex`、`xmail` 与
`xssh` 的 XLang 集成已经完成并由适配方仓库验证；`xhttp`、`xws` 的非快速路径语言包装
按当前边界继续延期。XRT 只记录集成状态，不保存外部仓库路径、同步器或语言专用实现。

## 产品原则

XRT 不是通用功能的合集。一个能力只有同时满足以下条件，才适合进入发布面：

1. 能明确归入一个 XRT 体系，并完成该体系从底层原语到常见路径的必要层次。
2. 脱离任何具体外部项目后，仍能用独立的 C 使用场景解释其公共价值。
3. 依赖方向由浅入深，不制造反向依赖、循环依赖或为了表面解耦增加的函数表。
4. 与现有能力组合后消除真实死角，而不是重复已有解析器、状态机或所有权实现。
5. 能独立裁剪、测试、说明所有权和错误语义；默认未选择时不增加代码和运行时状态。
6. 维护、体积和平台成本与使用价值相称，并有旧资产、测试或基准证明实现质量。
7. 常见路径简单直接，特殊路径保留低层扩展能力，不把用户锁死在单一对象模型中。

仅有历史实现、名称常见、某个外部项目可能使用，或者代码已经写完，都不是保留理由。

## 决策状态

- `retained`：体系属于 XRT，继续按完整契约和发布门禁压实。
- `review`：当前实现暂时保留，但尚未证明应进入最终 XRT 发布面。
- `internal`：只服务其他体系的实现，不形成独立公共产品能力。
- `deferred`：外部集成尚未纳入当前范围，不实现、不验证、不统计。
- `integrated`：外部适配方已经完成接入并维护独立验证，仍不进入 XRT 产品闭包。

复审结果只能是：归入已有体系、形成有充分理由的新体系、移到独立扩展项目，或退役。
不能建立永久 `misc`、`extra` 或 `compat` 目录容纳无法解释的功能。

## 当前体系

下表由 `config/modules.json` 中的 `scope.systems` 约束，源码根目录不得重复归属或遗漏：

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

`config/modules.json` 的 466 个节点主要表达裁剪、测试和平台证据粒度，其中 463 个已经
实现、3 个仍等待 Linux io_uring 运行证据；这不代表 XRT 有 466 个同级产品模块。架构
讨论以本表的体系为单位，裁剪验证才以细粒度节点为单位。

## 已确认保留

### XID

XID 是 XRT 与 XLang 的标准分布式 ID。它可与 UUID、GUID 并存，以稳定的跨平台格式提供
更高时间精度和更强随机性，不把平台原生 ID 的差异暴露给调用者。该能力归入标识体系，
保持独立裁剪，不因其他模块未使用而退役。

### XSON

XSON 是 JSON 的兼容超集和 `xvalue` 的无损持久化层，属于数据体系。实现必须与 JSON
共享扫描、字符串、数字、DOM、写入和文件适配，只保留 XSON 独有的语法和类型映射，
不得形成一套重复的 JSON 基础设施。

### Template

Template 是独立于 HTTP 的通用数据呈现基础设施，服务配置、代码、提示词和文本生成等
场景。它依赖 `xvalue` 和输出原语，但 HTTP 不得反向依赖 Template。模板体系应保持完整、
可裁剪，并复用现有文本、文件和数据能力。

### 浏览器 CORS 客户端

浏览器 Fetch safelist、客户端预检判定和预检缓存不属于 XRT 的 HTTP 底座，已迁入
`dev/archive/http_cors_client_20260812/`。XRT 继续保留 CORS 字段解析、服务端策略判断和
直接字段写入能力；未来独立扩展可在这些低层原语上实现浏览器客户端策略。

## 当前收敛结果

HTTP 核心只保留借用式 HTTP/1.1 解析、正文边界、调用方输出、Upgrade 和流式内容解码；
客户端、服务器、路由、拥有型报文及高级语义由活动扩展 `extlibs/xhttp` 承接，重构前快照
才保存在其 `archive/`。

WebSocket 核心只保留帧、消息、Close、握手字段、扩展协商和流式 permessage-deflate；
连接对象、Writer、Future/协程桥接、TLS 适配、连接级压缩、连接组和客户端/服务器由活动
扩展 `extlibs/xws` 承接。两个扩展均有独立清单、生成、构建和发布门禁，不进入 XRT 核心
闭包；各自 `archive/` 仍只保存不参与构建的历史快照。

后续精简仍必须逐模块给出依赖闭包、发布体积、重复实现、公共使用场景、替代路径和旧资产
证据，不得仅凭文件数量或当前没有内部调用者删除能力。

### 通用运行时模型

类型描述、类型转换、对象生命周期、对象图、动态调用和类型化容器已经拆分为
`extlibs/xruntime`。XRT 核心只保留 `xvalue`、基础容器、线程、Future 与任务等可独立成立的
底座；需要反射宿主、插件对象模型或拥有型 typed 容器时显式选择 `xruntime`。

扩展继续直接复用 XRT 容器和并发原语，不复制底层实现。模块清单、测试、示例、文档、
体积 profile 与性能 profile 均保存在 `extlibs/xruntime`，不再进入 XRT 核心发布体积。

### 正则表达式

正则编译、匹配器、捕获、替换、拆分和多模式集合已经拆分为 `extlibs/xregex`。XRT 核心
继续提供分配器、字符串、Unicode 与结构化错误底座；只有明确需要模式匹配的应用才选择
`xregex`，BBRE 实现、测试、示例和基准不再进入核心清单与通用单头文件。

## 外部集成规则

XRT 当前仓库不保存外部语言仓库的绝对路径、API 差异扫描器、同步脚本或语言专用发布
命令。适配方仓库维护绑定生成和同步门禁，并只依赖一个已经发布的 XRT 契约。若适配
暴露 XRT 本身缺少通用原语，应先按本文件的准入规则评估该原语，而不是直接把语言专用
实现迁入 XRT。

根 `config/modules.json` 的 `external_integrations` 只描述 XRT 核心产品与外部产品的
关系；每个 `extlibs/*/config/modules.json` 中的同名字段只描述该扩展自身的关系。两者
作用域独立，同名集成项不要求状态相同，也不得用扩展状态反向覆盖根产品状态。

`extlibs/` 是基于 XRT 公共 API 的活动扩展产品区，不属于 XRT 核心闭包，也不与主库强制
共同发布。当前 `xruntime`、`xhttp`、`xws`、`xregex`、`xmail` 和 `xssh` 分别维护独立模块
清单、裁剪单头、测试、文档、体积、性能和发布包；邮件扩展已经统一承接历史 IMAP、
Mail/MIME、POP3 与 SMTP 资产。扩展不得读取 XRT 私有头或复制基础实现，历史快照只允许
保存在各扩展的 `archive/` 或 `dev/archive/`，并与正式构建隔离。
