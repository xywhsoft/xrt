# XRT 下一阶段蓝图

状态: Active  
语言: 中文  
最后更新: 2026-03-16

## 1. 目标定位

XRT 的长期目标是：

**互联网 + AI 时代的 C 语言基础设施库**

这个目标意味着，XRT 不是单纯的工具函数集合，也不是单纯的网络库，而是面向以下场景的基础运行时与通用基础设施：

- 通用应用程序开发
- 高并发服务开发
- 网络客户端与服务端开发
- AI Agent 与 LLM 应用开发
- 数据交换、流式处理与结构化信息处理

当前已有能力已经覆盖：

- 基础运行时
- 内存管理
- 常用数据结构
- 线程与协程
- 网络传输与网络上层协议
- `xvalue/json` 数据处理
- 模板与正则

下一阶段最重要的，不是继续零散增加功能点，而是补齐几条真正决定 XRT 上限的“基础主干”。


## 2. 当前能力图

### 2.1 已形成的基础层

- 基础功能: `base / os / string / charset / time / file / xid`
- 内存能力: `mempool / fixed-size mempool / memory trace / debug`
- 数据结构: `array / list / dict / avltree / stack`
- 并发基础: `thread / coroutine`
- 网络传输: `tcp / udp / pool / xnet-v2`
- 网络上层: `http / websocket / tls / httpd`
- 数据表达: `xvalue / json`
- 文本层: `template / regex`

### 2.2 当前缺少的主干

从“现代互联网 + AI 基础设施”的标准看，当前最明显的缺口主要集中在：

- 统一执行上下文模型
- 统一异步结果模型
- 统一互联网工具层
- 统一 AI 抽象层
- 统一日志、配置、进程与调试基础设施


## 3. 下一阶段最值得补的主干

## 3.1 统一执行上下文模型

建议引入一套正式的执行上下文抽象，例如：

- `xctx`
- `xcancel`
- `xdeadline`

它们应当统一承载：

- 取消请求
- deadline / timeout
- 上下文级附加数据
- 与线程、协程、future、网络等待的一致语义

这个能力非常关键，因为它会成为以下模块的共同基础：

- 协程等待
- 网络等待
- future/task
- HTTP 客户端
- LLM 请求
- Agent 工具调用

如果没有这层抽象，后续每个模块都会各自维护一套 timeout/cancel 语义，系统会逐渐失去一致性。


## 3.2 future / task / promise 正式化

当前 `future` 已经开始进入系统核心，但还更偏内部桥接与等待源。

建议将其提升为正式一等公民，并打通以下能力：

- 线程任务结果
- 协程等待结果
- 网络异步结果
- 定时器结果
- engine post 结果
- 组合等待能力

建议未来至少具备：

- `future create / resolve / reject / destroy`
- `wait / timeout / deadline`
- `wait source bridge`
- `result + status + error`
- 单等待者与多等待者策略明确


## 3.3 统一错误模型

当前已有线程级错误槽，但未来如果要支撑大型系统，建议建立三层错误语义：

- 快速路径: 简单返回值，例如 `bool/int/status`
- 线程级最近错误: `xrtGetError()`
- 结构化错误对象: 错误码、模块、系统错误、附加上下文

建议未来支持：

- 模块化错误码
- 系统错误透传
- 错误来源定位
- 结构化日志绑定

这会显著提高调试质量，也利于 HTTP、TLS、Agent、子进程等复杂系统组合。


## 3.4 统一资源生命周期模型

这项工作不会直接产生新功能，但会显著影响整个库的长期可维护性。

建议尽快冻结并统一以下语义：

- `Init / Unit`
- `Create / Destroy`
- `Open / Close`
- `Borrowed / Owned`
- `Local / Shared`
- `Sync / Async`

尤其是以下模块必须逐步统一：

- 并发与协程
- 内存池
- 网络对象
- `xvalue`
- wait source / future

如果这套模型不尽早冻结，后续越往上做 API 越容易失控。


## 4. 互联网时代非常实用的能力层

## 4.1 日志系统

建议增加正式日志模块，而不是继续依赖零散输出。

推荐最小能力：

- 级别: `trace/debug/info/warn/error/fatal`
- 模块标签
- 时间戳
- 线程 ID / 协程 ID
- sink: console / file / callback
- structured fields
- 滚动文件

这对服务程序、网络库、AI Agent、基准测试都非常重要。


## 4.2 配置系统

建议补齐以下配置基础设施：

- 命令行参数解析
- 环境变量读取
- 配置文件加载
- 多来源覆盖规则

建议优先考虑：

- `env`
- `ini`
- `json`
- 后续再考虑 `toml`


## 4.3 URL / Query / Header / Cookie / Form 工具层

当前已有 HTTP，但互联网应用中还有一层非常高频的辅助能力尚未独立出来。

建议补：

- URL parse / build
- query encode / decode
- header helper
- cookie parse / build
- `application/x-www-form-urlencoded`
- `multipart/form-data`

这会极大改善 HTTP 客户端、服务端、LLM API 调用与调试体验。

### 4.3.1 目标

这一层不应继续散落在 `xhttp / xhttpd / xws / xcodec_http1` 内部作为私有 helper，而应提升为正式公开工具层：

- 对外暴露统一的 `xrt*` 前缀函数
- 与网络传输层解耦，不依赖 `xnetstream / xnetconn / tls`
- 默认支持零拷贝解析视图
- 同时提供便捷的 copy / build / encode / decode 工具
- 成为 `xhttp / xhttpd / xws / LLM API / 调试工具` 的共同基础

### 4.3.2 当前代码中已经存在、但应收敛为公共能力的逻辑

当前主线中已经有几段成熟但分散的私有实现，应统一抽离：

- `xhttp` 客户端固定 URL 装配：目前已统一走 `xrtUrlParseFixedTo(...)`
- `xws` 客户端固定 URL 装配：目前已统一走 `xrtUrlParseFixedTo(...)`，并复用公开的 host/target helper
- `xhttpd` 请求目标拆分：目前已在 request materialization 中拆 `target/path/query`
- `xcodec_http1` header lookup / token helper：目前已经有大小写无关 header 查询和 token 匹配逻辑

这些能力目前的问题不是“没有实现”，而是：

- 分散重复
- 类型不统一
- 命名不是正式 API
- 只能给当前模块自己用
- URL 解析能力在 `xhttp` 和 `xws` 中各自维护，后续必然漂移

下一阶段应把这些逻辑统一收敛成一套公共 API，并让上层模块改为调用公共函数。

### 4.3.3 模块拆分建议

建议拆成两层公开头文件：

- `lib/xurl.h`
  - 负责 URL / URI / authority / query-string 级别的通用解析与构建
- `lib/xhttp_util.h`
  - 负责 header / cookie / form / multipart 等 HTTP 相关工具能力

这样拆分的原因是：

- URL 工具本身是通用互联网工具，不应只服务 HTTP
- Cookie / header / form 更明显属于 HTTP 语义
- `xws / xhttp / xhttpd` 都能直接复用 `xurl`
- 未来如果做 `proxy / sse / llm client / oauth`，也可以直接复用这层

### 4.3.4 公共数据模型建议

建议优先使用“view-first”模型，即默认输出切片视图，不强制复制：

- `xrtstrview`
  - `const char* sPtr`
  - `size_t iLen`
- `xrturlview`
  - `xrtstrview tScheme`
  - `xrtstrview tUserInfo`
  - `xrtstrview tHost`
  - `uint16 iPort`
  - `bool bHasPort`
  - `xrtstrview tAuthority`
  - `xrtstrview tPath`
  - `xrtstrview tQuery`
  - `xrtstrview tFragment`
  - `xrtstrview tTarget`
- `xrtheaderpair`
  - `xrtstrview tName`
  - `xrtstrview tValue`
- `xrtquerypair`
  - `xrtstrview tKey`
  - `xrtstrview tValue`
  - `bool bHasValue`
- `xrtcookiepair`
  - `xrtstrview tName`
  - `xrtstrview tValue`
- `xrtformfield`
  - `xrtstrview tName`
  - `xrtstrview tValue`
- `xrtmultipartpart`
  - `xrtstrview tName`
  - `xrtstrview tFileName`
  - `xrtstrview tContentType`
  - `xrtstrview tBody`

设计原则：

- parse 默认只做“定位”，不复制
- decode / normalize / build 再单独做
- 这样既适合高性能，也适合调试工具层

### 4.3.5 URL 层公开 API 建议

建议第一批正式公开这些函数，并统一使用 `xrt` 前缀：

- `xrtUrlParse(...)`
  - 解析完整 URL，提取 `scheme/host/port/path/query/fragment`
- `xrtUrlParseTarget(...)`
  - 解析 HTTP target，例如 `/v1/chat?stream=1`
- `xrtUrlParseAuthority(...)`
  - 解析 `host[:port]`，支持 IPv6 bracket 语义
- `xrtUrlBuild(...)`
  - 根据结构化字段构建 URL
- `xrtUrlBuildTarget(...)`
  - 根据 `path/query` 构建 target
- `xrtUrlDefaultPort(...)`
  - 根据 scheme 返回默认端口
- `xrtUrlMakeHostHeader(...)`
  - 生成适用于 HTTP/WS 的 `Host` header 值
- `xrtUrlIsDefaultPort(...)`
  - 判断是否是 scheme 的默认端口

必须覆盖的能力：

- `http / https / ws / wss`
- `host[:port]`
- IPv6 host，如 `[::1]:8080`
- path/query/fragment
- 相对 target
- 缺省 path 自动归一到 `/`

这层完成后，应替换：

- `xrtUrlParseFixedTo(...)`
- `xrtUrlViewCopy* / xrtUrlMakeHostHeader*`
- `xhttpd` 内部对 `target/path/query` 的手工拆分

### 4.3.6 Query 层公开 API 建议

建议提供两类能力：解析和编码。

解析：

- `xrtQueryParse(...)`
- `xrtQueryNext(...)`
- `xrtQueryCount(...)`
- `xrtQueryFind(...)`

编码 / 解码：

- `xrtQueryEncode(...)`
- `xrtQueryDecode(...)`
- `xrtPercentEncode(...)`
- `xrtPercentDecode(...)`

语义约束建议：

- 默认按 `application/x-www-form-urlencoded` 规则支持 `+` 为空格
- 同时提供“纯 percent-decode”模式
- 空 key / 空 value / 重复 key 都要保留
- `a=1&a=2` 必须能正确表示为两个 pair，而不是覆盖

### 4.3.7 Header 层公开 API 建议

header 工具层不应试图替代 `xcodec_http1` 整体 parser，而应暴露“高频通用工具”：

- `xrtHttpHeaderFind(...)`
- `xrtHttpHeaderFindNoCase(...)`
- `xrtHttpHeaderContainsToken(...)`
- `xrtHttpHeaderSplitLine(...)`
- `xrtHttpHeaderAppend(...)`
- `xrtHttpHeaderSet(...)`
- `xrtHttpHeaderRemove(...)`

适用范围：

- `xhttpresponse / xhttprequest`
- `xhttpdrequest / xhttpdresponse`
- `xcodechttp1msg`
- 独立 header 列表

这一层的目标是把当前：

- `xrtCodecHttp1GetHeader(...)`
- `__xhttpRequestHeaderValue(...)`
- `xrtHttpResponseHeader(...)`
- `xrtHttpdRequestHeader(...)`
- `xrtHttpdResponseHeader(...)`

逐步统一到更少的一套公共 helper 上。

### 4.3.8 Cookie 层公开 API 建议

Cookie 建议拆成 request-cookie 和 set-cookie 两条路径，不要混成一个 parser：

- `xrtCookieParse(...)`
  - 解析 `Cookie:` 请求头中的 `k=v; k2=v2`
- `xrtCookieNext(...)`
- `xrtCookieFind(...)`
- `xrtCookieBuild(...)`
  - 构建 request cookie header value

对于响应头：

- `xrtSetCookieParse(...)`
  - 解析单条 `Set-Cookie`
- `xrtSetCookieBuild(...)`
  - 构建单条 `Set-Cookie`

`Set-Cookie` 结构建议至少覆盖：

- `name`
- `value`
- `Path`
- `Domain`
- `Max-Age`
- `Expires`
- `Secure`
- `HttpOnly`
- `SameSite`

### 4.3.9 Form 层公开 API 建议

第一阶段建议只正式完成这两条：

- `application/x-www-form-urlencoded`
- `multipart/form-data`

第一条优先级更高：

- `xrtFormUrlEncodedParse(...)`
- `xrtFormUrlEncodedBuild(...)`
- `xrtFormUrlEncodedNext(...)`

第二条建议分阶段：

第一阶段：

- `xrtMultipartParse(...)`
- `xrtMultipartNextPart(...)`
- 能提取 part header、name、filename、content-type、body view

第二阶段：

- streaming multipart parser
- 大文件上传场景的边界扫描优化
- 与 `xnetchain` / `xcodec_http1` 更深度集成

### 4.3.10 与现有 HTTP 栈的协同方式

建议协同路线如下：

- `xcodec_http1`
  - 继续负责 HTTP/1.1 message delimiting 和 start-line/header parser
- `xurl`
  - 负责 URL / target / authority / query 的独立公共工具
- `xhttp_util`
  - 负责 header / cookie / form / multipart 工具
- `xhttp / xhttpd / xws`
  - 不再维护自己的私有 URL/header/query 解析逻辑，统一调用工具层

这样做的好处是：

- `xhttp`、`xhttpd`、`xws` 会共享同一套协议细节实现
- 减少重复代码和语义漂移
- 以后要暴露给用户时，不需要把整个 HTTP 对象体系一起带出来

### 4.3.11 公开 API 风格约束

这一层应全部使用正式公开命名，不再沿用内部私有风格：

- 公开函数全部使用 `xrt*`
- 内部 helper 继续使用 `__x*`
- 不再新增 `xhttp2 / xws2 / Ex / Internal` 风格的历史命名

并且建议遵守：

- parse 函数默认不分配内存
- copy/build 函数明确由调用者提供 buffer 或返回新分配结果
- 统一返回 `bool / size_t / status`，不要混出多套错误语义

### 4.3.12 分阶段实施建议

建议按下面顺序做，而不是一次把所有工具层一起推出来：

第一阶段：

- `xurl.h`
- `xrtUrlParse / xrtUrlParseTarget / xrtUrlMakeHostHeader`
- `xrtQueryParse / xrtQueryDecode / xrtPercentDecode`
- `xrtHttpHeaderFind / xrtHttpHeaderContainsToken`

第二阶段：

- `xrtCookieParse / xrtSetCookieParse`
- `xrtFormUrlEncodedParse / Build`
- `xhttp / xhttpd / xws` 全量切到新公共函数

第三阶段：

- `multipart/form-data`
- 更完整的 build helper
- 流式 parser 和更强的调试工具支持

### 4.3.13 验收标准

这一层上线前，至少要满足：

- `xhttp` 与 `xws` 的私有 URL 解析逻辑被删掉或降级为 wrapper
- `xhttpd` 的 target/path/query 拆分改用公共工具函数
- URL 工具正确覆盖 IPv4 / IPv6 / 缺省端口 / fragment / 相对 target
- query / cookie / form 对重复 key、空值、percent-encoding 都有明确行为
- header 工具能替代现有大部分重复 header 查找代码
- 所有公开函数都以 `xrt` 为前缀
- 有独立 focused test，不依赖整条网络链路才能验证

### 4.3.14 安全与性能实现标准

### Status (2026-03-17)

- [x] Added explicit public HTTP util limit helpers for query / cookie / form / header / token / param / multipart validation
- [x] Added `xrthttputillimits` default profile and `xrtMultipartStreamConfigApplyLimits(...)`
- [x] Added tracked malicious-input corpus and standalone corpus runners for the HTTP util fuzz harness
- [x] Added a sanitizer-backed Unix/Linux corpus helper plus a dedicated libFuzzer build helper for the HTTP util fuzz harness
- [x] Added GitHub Actions CI coverage for tracked corpus, ASan/UBSan corpus, libFuzzer build smoke, focused URL/HTTP util tests, and the modern integration flow

这一层不能按“普通工具函数”标准实现，而应按“对外服务级协议工具层”实现。

也就是说：

- 不能只追求功能可用
- 不能只追求 API 漂亮
- 必须同时满足高性能、严格语义和可审计性

这一层的实现标准应明确高于当前散落在 `xhttp / xhttpd / xws` 内部的私有 helper。

#### 必须遵守的安全要求

- 不允许 silent truncation
  - parser 不应像当前部分私有 helper 那样把结果复制到固定数组后悄悄截断
  - 应优先返回 `view`，copy helper 则显式返回失败或所需长度
- 不允许依赖 `atoi/strlen` 这类不带边界的弱语义解析
  - 端口、长度、十六进制、边界扫描必须使用长度受控的解析器
- 所有公共 parse 函数都应优先接受 `(ptr + len)` 或 `view`
  - 不应把 NUL 结尾字符串作为唯一输入模型
- 必须明确定义并拒绝非法输入
  - 非法 scheme
  - 非法 authority
  - 非法 port
  - 非法 percent-escape
  - 非法 header line
  - 非法 cookie pair
  - 非法 form field
- 必须为 DoS 风险设置明确上限
  - key/value 长度
  - pair 数量
  - header 数量
  - multipart header 数量
  - boundary 长度
  - form 总字段数
- HTTP 服务相关 helper 必须遵循“保守接受、严格拒绝”的 server-side 策略
  - 尤其是请求目标、header token、cookie、form、multipart boundary 解析

#### 必须遵守的性能要求

- parse 默认零拷贝
- 不在热路径里做隐式堆分配
- 不做不必要的小块 `malloc/free`
- 不用树结构或高开销容器做高频 token 查找
- query/header/cookie 的迭代器设计必须支持单次线性扫描
- URL parse 必须是单趟或近似单趟扫描，不做多次重复 `strlen/strchr`

### 4.3.15 现有实现的重构约束

当前主线里已有若干私有实现，但它们不能原样抬升为公开 API。

原因包括：

- `xhttp` 私有 URL parser 与 `xws` 私有 URL parser 行为不一致
- 现有 helper 大多基于固定大小字符数组，存在截断语义
- 当前 `xhttpd` 对 `target/path/query` 的拆分偏向“够用”，不是正式工具层的最终形态
- 某些数值解析和 host/port 解析还不够严格

因此真正开工时必须遵守：

- 以“更强的那套行为”为基准统一
- 但不是再维护一套 `xws` 私有 URL 解析器
- 而是吸收它对 `ws/wss/IPv6/default-port/host-header` 的正确行为后，重构成新的正式公共实现
- `xhttp/xws` 的 URL 私有拆装逻辑、`xhttpd` 的手工 component 复制逻辑，最终都应降为对公开 `xrt*` helper 的薄调用或直接删除

换句话说：

- 不能为了尽快公开 API，而把当前私有 helper 原封不动搬出来
- 必须先完成安全性和语义升级，再公开

### 4.3.16 参考基线

这一层实现时，建议参考长期大规模生产使用、长期接受公开安全审视的成熟实现思路，而不是凭感觉设计。

重点应参考：

- HTTP/1 parser 的状态机边界处理思路
- URL parser 对 scheme/authority/IPv6/default-port 的严格语义
- header/token/cookie/query 的长度受控扫描方式
- multipart/form-data 的 boundary 与 header 安全处理方式

参考这些成熟实现的目的不是“照搬 API”，而是：

- 拿它们的拒绝策略当底线
- 拿它们的边界处理方式当安全基线
- 拿它们的单趟扫描和零拷贝思想当性能基线

XRT 的最终实现应在风格和 API 上保持自己的体系，但安全性和边界处理不能弱于成熟实现。

### 4.3.17 建议增加的专项验证

这一层除了普通 unit test，还应增加专项验证：

- URL 语义测试
  - IPv4 / IPv6 / default-port / empty-path / query / fragment / invalid authority
- percent-encoding 测试
  - 非法 `%` 序列
  - UTF-8 边界
  - `+` 与空格语义
- header/cookie/query 模糊测试
  - 空 key
  - 重复 key
  - 超长 token
  - 非法分隔符
- multipart 恶意输入测试
  - 长 boundary
  - boundary 前后缀碰撞
  - header 爆炸
  - 大量空 part
- 与 `xhttp / xhttpd / xws` 的集成回归
  - 保证上层切换到公共工具后行为不退化

如果没有这组专项验证，这层就不应视为达到“对外服务级”的完成标准。


## 4.4 通用编码工具层

建议补齐这些高频基础件：

- `base64`
- `base64url`
- `hex`
- `percent-encode`
- `html escape/unescape`
- `csv`
- `sse parser`

其中 `SSE parser` 对 AI 应用价值很高，优先级应明显高于 `csv`。


## 4.5 子进程 / 工具执行层

AI Agent 场景极度依赖子进程能力。

建议增加：

- 启动子进程
- 管道读取 stdout/stderr
- 提供 stdin
- timeout
- exit code
- working dir
- env override

这是 Agent 与工具编排能力的关键基础件之一。


## 4.6 动态库 / 插件加载

建议提供最小跨平台动态库包装：

- load library
- get symbol
- unload

这项能力未来可用于：

- 动态扩展
- 可插拔模块
- 工具与 provider 接入


## 5. AI 时代特别重要的能力层

## 5.1 流式输出基础设施

LLM API 广泛依赖流式响应，因此建议把这部分提升到核心优先级。

建议能力包括：

- stream body reader
- SSE parser
- chunked body helper
- incremental message merge
- partial JSON / partial text 处理

这会直接影响：

- 聊天补全流式输出
- 工具调用流式结果
- 长响应可视化与中断


## 5.2 结构化输出与校验

`xvalue + json` 已经提供了不错的数据表达能力，下一步建议补齐：

- schema-like validation
- required / optional / default
- path-based error report
- typed binding helper

这样会显著改善：

- 配置校验
- HTTP body 校验
- LLM structured output
- tool call arguments 校验


## 5.3 AI 消息模型

建议构建“模型无关”的消息与工具调用抽象，而不是过早绑定某个厂商 SDK。

建议抽象：

- message list
- role
- content part
- tool call
- tool result
- delta merge
- usage / token stats

这样以后接 OpenAI、Anthropic、Gemini、私有模型都会更顺。


## 5.4 向量与基础相似度工具

如果希望 XRT 更贴近 AI 基础设施，可以加入轻量数值能力：

- float vector
- cosine similarity
- dot product
- L2 distance
- top-k helper

这里不建议一开始就做重量级向量数据库，但基础向量运算非常有价值。


## 5.5 文本切分与清洗

建议补一层轻量文本处理：

- sentence split
- paragraph split
- chunk by size
- overlap chunk
- whitespace normalize
- markdown/plaintext extract helper

这对 RAG、prompt 构造、LLM 前处理都很实用。


## 5.6 工具调用协议层

建议构建与模型无关的 tool-call 协议封装：

- tool descriptor
- argument schema
- dispatch helper
- result envelope
- error envelope

这会让 Agent 系统不必在每个项目里重新造一套工具调用框架。


## 6. 网络层还能继续增强的能力

## 6.1 客户端基础设施

建议逐步补齐：

- connection pool
- keepalive pool
- DNS cache
- retry policy
- backoff
- circuit breaker
- rate limiter
- proxy support

这些能力对互联网客户端、LLM API client 都非常重要。


## 6.2 SSE client

这项能力值得单独列出，因为它对 AI 的重要性极高。

建议具备：

- 连接 SSE endpoint
- event parse
- message assemble
- reconnect policy
- callback / coroutine wait / future bridge


## 6.3 WebSocket client 健壮化

当前已有 websocket，但未来建议继续增强：

- 自动 ping/pong
- reconnect policy
- close reason
- backpressure contract
- coroutine/future waiting


## 6.4 HTTP/2 与后续协议

当前命名和实现应坚持“没有历史包袱”的原则：

- HTTP/1.1 就明确叫 HTTP/1.1
- HTTP/2 只有在真实实现后再引入
- QUIC / HTTP/3 可放更后阶段


## 7. 工程质量与可观测性层

## 7.1 trace / metrics / profiling hooks

建议增加统一的轻量观测能力：

- span begin / end
- counter
- gauge
- histogram
- callback sink

重点覆盖：

- thread
- coroutine
- xnet
- http
- tls


## 7.2 fuzzing 友好入口

### Status (2026-03-17)

- [x] Added public HTTP util fuzz harness: `dev/fuzz_http_util_core.c`
- [x] Harness covers URL, query, header, token-list, cookie, Set-Cookie, media-type, content-disposition, ext-value, multipart, and streaming multipart public APIs
- [x] Added dedicated standalone build helpers for the HTTP util fuzz harness
- [x] Added tracked corpus runners on top of the HTTP util fuzz harness build helpers
- [x] Added sanitizer-backed corpus and dedicated libFuzzer build helpers on top of the existing harness and build helpers
- [x] Added GitHub Actions CI coverage on top of the existing harness and build helpers

建议逐步为以下模块准备独立 fuzz harness：

- json
- http parser
- websocket frame
- url parse
- template
- regex


## 7.3 sanitizer / 测试矩阵

建议未来稳定建设：

- ASan
- UBSan
- TSan
- Windows / Linux
- GCC / Clang / TCC

这对基础设施级项目非常关键。


## 7.4 benchmark policy

当前已经开始形成 bench 体系，建议继续固定并制度化：

- baseline
- hardware note
- repeat count
- throughput
- latency
- p50 / p95
- regression threshold


## 8. 建议的优先级排序

## 8.1 P0: 最优先

- 统一执行上下文: `context / cancel / deadline`
- `future / task` 正式化
- 统一错误模型
- 统一资源生命周期模型

这些能力会影响后面几乎所有模块。


## 8.2 P1: 高优先级

- logging
- config + env + cli
- URL/query/header/cookie/form
- SSE / 流式 HTTP body
- subprocess
- JSON validation / structured output

这批能力会显著提升 XRT 的可用性和现代感。


## 8.3 P2: AI 方向强化

- AI message model
- tool-call 协议层
- 轻量向量与相似度工具
- 文本切分与清洗

这批能力能让 XRT 更像“AI 时代基础设施库”，但不必先于 P0/P1。


## 8.4 P3: 后续扩展

- HTTP/2
- QUIC / HTTP/3
- 更完整的插件体系
- 更大规模的 observability 方案


## 9. 明确不建议过早扩张的范围

为了避免 XRT 失控，以下方向不建议过早投入过深：

- 重量级数据库
- ORM
- 完整向量数据库
- 厂商绑定的 LLM SDK 大合集
- 浏览器级 JS 引擎集成

这些方向会迅速稀释 XRT 作为“基础设施库”的边界。


## 10. 一句话结论

XRT 下一阶段最需要补的，不是再增加零散功能，而是补齐 3 条主干：

- 统一执行上下文主干
- 统一互联网工具层主干
- 统一 AI 抽象层主干

只要这三条线补起来，XRT 就会从“功能很多的 C 库”真正升级为：

**互联网 + AI 时代的 C 语言基础设施库**
