# XRT 文档重建 Spec

> 目标：以当前源码主线为唯一事实来源，系统重建 `docs/` 文档体系，清除历史包袱，补齐新增能力，并把淘汰文档归档到 `dev/` 目录。

---

## 1. 目标

本次文档工作不是“修几个 README”，而是一次完整的文档主线收口。

必须同时完成 3 件事：

1. 对 `docs/` 中**仍然有效**的文档逐个与当前代码实现重新对齐。
2. 对 `docs/` 中**缺失**、但当前主线已经存在的模块与能力补写新文档。
3. 对 `docs/` 中**已经淘汰**、会误导使用者的旧文档迁移到 `dev/` 目录归档，不再作为正式文档入口。

本次重建后的文档体系，必须能够准确表达当前 XRT 的定位：

- 互联网 + AI 时代的 C 语言基础设施库
- 高性能
- 轻量化
- 跨平台
- 功能强大、完备且成体系
- 不依赖拼凑式代码组织


## 2. 范围

本次文档重建覆盖：

- `D:\git\xrt\docs\`
- 根目录：
	- `README.md`
	- `README.en.md`
- 归档目标目录：
	- `D:\git\xrt\dev\`

本次工作的唯一事实来源：

- `D:\git\xrt\xrt.h`
- `D:\git\xrt\xrt.c`
- `D:\git\xrt\lib\*.h`
- `D:\git\xrt\test\*.h`
- `D:\git\xrt\dev\*.c`
- 已冻结设计文档：
	- `XRT_RUNTIME_PHASE1_SPEC.md`
	- `XRT_RUNTIME_PHASE2_SPEC.md`
	- `XRT_COROUTINE_REFACTOR_SPEC.md`
	- `XRT_FUTURE_TASK_PROMISE_SPEC.md`
	- `XNET_V2_SPEC.md`
	- 以及相关 status / summary / production review 文档


## 3. 文档重建原则

### 3.1 单一主线原则

文档必须只反映当前正式主线。

不允许长期维持：

- 旧 API 文档与新 API 文档并列
- 旧实现与新实现同时作为“推荐用法”
- 过时命名与新命名共存

如果某能力已经收口为单一主线，则文档也必须只有单一主线。


### 3.2 源码优先原则

文档与代码冲突时，以当前源码主线为准。

不得因为历史文档存在，就反向保留历史 API 表面。


### 3.3 不保留无效文档壳层

对已经淘汰的文档，不做“轻微修补后继续挂在 docs/”。

处理原则：

- 仍有历史价值：迁移到 `dev/` 归档
- 无历史价值且完全误导：直接移除

本轮按用户要求，统一先归档到 `dev/`。


### 3.4 文档层级清晰

正式文档应分成 4 层：

1. 项目入口与定位
2. 文档索引与架构说明
3. API 参考
4. 迁移、FAQ、最佳实践、示例、性能

避免：

- 同一信息在多个文档里用不同事实重复叙述
- 索引页承载过多 API 细节
- API 文档承担架构说明职责


### 3.5 中英文一致

中文与英文文档必须保持结构和事实一致。

允许：

- 英文语气更适合英文技术写作

不允许：

- 中文已更新而英文缺失关键能力
- 中英文的 API 合同描述不一致


### 3.6 中文优先生成

本轮文档重建采用：

- 先完整生成和审阅中文版本
- 中文版本经确认后，再统一翻译为英文版本

执行规则：

- 当前阶段优先修改 `*.md` 中文文档
- 对应英文文档可以暂时保留旧版、留空或标记待同步，但不抢先生成
- 不做“中文改一半、英文同步改一半”的双线并行维护

原因：

- 避免在中文内容尚未审定前，英文版本反复返工
- 避免中英文两套文档在重建阶段同时漂移
- 先把中文主线收稳，再统一做英文同步，效率更高、错误更少


## 3.7 精修阶段原则

在入口页、API 页、guide、case 基本补齐之后，文档重建进入第二轮精修阶段。

这一阶段的重点不是继续堆新页面，而是继续清理这些“看起来还能用，但会误导当前主线”的旧语义：

- 旧隐藏返回槽语义，例如 `xCore.sRet / xCore.iRet / xCore.nRet`
- 过度暴露运行时内部缓存的表述，例如直接把 `xCore.LocalAddr` 写成推荐 public 用法
- 把源码树主线误讲成单头文件主线
- 把旧网络/TLS public surface 写成当前推荐入口
- 直接暴露已经不稳定或不再推荐的内部结构字段

精修阶段处理规则：

- 如果某个表述只是历史解释，可保留在迁移语境中
- 如果某个表述会误导“当前推荐写法”，必须改成当前正式主线
- 中英文文档要同步收口，避免中文已经精修而英文仍保留旧语义

当前这轮已经完成的精修重点：

- `api-path*` 已移除把 `xCore.iRet` 当成长度返回契约的说明与示例
- `api-string*` 的 `Find / Trim / Filter / Replace / Split / Hex / Base64` 关键段已改成当前正式签名与返回约定
- `api-hash*` 的文件校验示例已改成显式 `iRetSize` 风格
- `api-network*` 与 `api-xid*` 已收紧对 `xCore.LocalAddr` 的直接 public 用法说明
- `types.md` 已从旧整块 `xCore` 结构布局文档改写为当前“进程级 / 线程级运行时模型”


## 4. 文档分类动作

本次文档按 4 种动作处理：

### A. 保留并逐篇对齐

适用于：

- 文档主题仍然有效
- 但内容可能缺少后续新增能力
- 或示例、术语、局部片段仍有旧语义残留

要求：

- 逐篇检查，不允许因为“看起来大体没问题”就跳过
- 每篇都要和当前代码再次比对


### B. 新建

适用于：

- 当前主线已有正式模块/能力
- `docs/` 中没有对应文档


### C. 重写

适用于：

- 文档名或主题仍可保留
- 但内容主体已失效
- 修补成本高于重写


### D. 归档到 `dev/`

适用于：

- 当前已淘汰
- 保留在 `docs/` 会误导使用者
- 但仍有历史参考价值

归档规则：

- 从 `docs/` 移到 `dev/`
- 文件名加明确前缀或后缀，标记为历史归档
- 归档文档不再出现在正式索引页中


## 5. 当前已确认的处理清单

### 5.1 需要归档到 `dev/` 的文档

以下文档已确认属于旧主线，应从 `docs/` 退出正式体系：

- `api-nettcp.md`
- `api-nettcp.en.md`
- `api-netudp.md`
- `api-netudp.en.md`
- `api-nethttp.md`
- `api-nethttp.en.md`
- `api-netpoll.md`
- `api-netpoll.en.md`
- `api-nettls.md`
- `api-nettls.en.md`

说明：

- 它们描述的是旧网络/TLS 公共表面
- 当前网络主线已经切换到 `xnet-v2 + xtlssession`


### 5.2 需要重写的文档

以下文档主题仍然需要，但主体内容已明显过期：

- `docs/README.md`
- `docs/README.en.md`
- `ARCHITECTURE.md`
- `ARCHITECTURE.en.md`
- `裁剪宏定义说明.md`
- `api-network-tls.en.md`

说明：

- `docs/README*` 需要成为当前正式索引页
- `ARCHITECTURE*` 必须反映 runtime/thread/coroutine/future/xnet-v2/shared-mode 新架构
- `裁剪宏定义说明.md` 必须反映当前真实裁剪宏
- `api-network-tls.en.md` 主题可保留，但内容必须重建为 session 主线


### 5.3 需要新建的文档

当前已确认缺失：

- `api-xurl.md`
- `api-xurl.en.md`
- `api-http-util.md`
- `api-http-util.en.md`
- `api-xhttp.md`
- `api-xhttp.en.md`
- `api-xhttpd.md`
- `api-xhttpd.en.md`
- `api-xws.md`
- `api-xws.en.md`
- `api-xnet-v2.md`
- `api-xnet-v2.en.md`
- `api-network-tls.md`
- `api-future-task-promise.en.md`
- `api-regex.md`
- `api-regex.en.md`

后续还应继续补充：

- `guide/` 下的正式教学页
- `case/` 下的正式案例页

说明：

- `api-xnet-v2*` 可以是总览文档，不强制一开始拆成多个子文档
- 网络应用层先以用户可理解的模块入口为主，不必一开始写成纯内部组件清单


### 5.4 需要逐篇对齐和补强的现有文档

这些文档不能只做“快速修词”，必须逐篇重新对照当前实现：

#### 基础运行时

- `api-base.md`
- `api-base.en.md`
- `types.md`
- `types.en.md`
- `api-string.md`
- `api-string.en.md`
- `api-file.md`
- `api-file.en.md`
- `api-network.md`
- `api-network.en.md`
- `api-math.md`
- `api-math.en.md`

重点补强：

- global/thread runtime 分层
- `xrtGetError()` 取代 `xCore.LastError`
- 线程级临时内存语义
- 默认 RNG 线程化

#### 线程与协程

- `api-thread.md`
- `api-thread.en.md`
- `api-coroutine.md`
- `api-coroutine.en.md`

重点补强：

- attach/detach 模型
- cleanup 栈
- owner-thread / scheduler 约束
- event/deadline wait
- backend 分层与自检
- 与 future/wait-source 的关系

#### 内存管理与容器

- `api-bsmm.md`
- `api-bsmm.en.md`
- `api-memunit.md`
- `api-memunit.en.md`
- `api-mempool-fs.md`
- `api-mempool-fs.en.md`
- `api-mempool.md`
- `api-mempool.en.md`
- `api-array.md`
- `api-array.en.md`
- `api-ptrarray.md`
- `api-ptrarray.en.md`
- `api-avltree.md`
- `api-avltree.en.md`
- `api-dict.md`
- `api-dict.en.md`
- `api-list.md`
- `api-list.en.md`

重点补强：

- owner-thread 默认语义
- local/shared 模式边界
- shared root 合同
- `Lock/Unlock`
- 错线程访问诊断
- 内存调试与危险操作识别能力

#### 高层数据与异步

- `api-value.md`
- `api-value.en.md`
- `api-json.md`
- `api-json.en.md`
- `api-template.md`
- `api-template.en.md`
- `api-future-task-promise.md`

重点补强：

- shared-mode 后续补充能力
- future/task/promise 与协程、xnet 的交叉索引
- ownership 合同
- task group / nested scope / dynamic join

#### 辅助文档

- `MIGRATION.md`
- `MIGRATION.en.md`
- `BEST_PRACTICES.md`
- `BEST_PRACTICES.en.md`
- `FAQ.md`
- `FAQ.en.md`
- `EXAMPLES.md`
- `EXAMPLES.en.md`（若不存在则补建）
- `PERFORMANCE.md`

重点补强：

- 去除旧 `xCore.LastError`
- 去除旧网络/TLS 主线示例
- 增加当前推荐写法
- 增加已重构后的模块使用建议


## 6. 明确必须修正的已知问题

以下问题是本轮审计中已确认的确定错误：

- `api-list.md` / `api-list.en.md` 顶部链接错误，误指向 JSON 文档
- `api-mempool.md` 顶部英文链接拼写错误（`api-mempoll.en.md`）
- `docs/README.md` 中 `Value` 类型数量描述已落后
- 多处文档仍引用：
	- `xCore.LastError`
	- 旧 `xrtTempMemory` 环形槽位语义
	- 旧 `nettcp/netudp/nethttp/nettls` 公共表面


## 7. 重建后的正式文档结构目标

### 7.1 项目入口

- 根目录 `README.md`
- 根目录 `README.en.md`

### 7.2 文档索引

- `docs/README.md`
- `docs/README.en.md`

### 7.3 核心架构

- `ARCHITECTURE.md`
- `ARCHITECTURE.en.md`
- `types.md`
- `types.en.md`

### 7.4 API 文档

按当前主线整理，必须突出：

- runtime / thread / coroutine
- future / task / promise / wait-source
- xnet-v2
- TLS session
- URL / HTTP util / HTTP / HTTPD / WebSocket
- 内存与容器 shared-mode

### 7.5 辅助文档

- `MIGRATION*`
- `BEST_PRACTICES*`
- `FAQ*`
- `EXAMPLES*`
- `PERFORMANCE*`

### 7.6 历史归档

所有淘汰主线文档统一迁入 `dev/`，不再出现在正式索引里。


## 8. 执行顺序

按以下顺序推进，避免中途再次漂移：

### Phase A：清主线入口

1. 重写 `docs/README*`
2. 重写 `ARCHITECTURE*`
3. 重写 `裁剪宏定义说明.md`
4. 归档旧网络/TLS 文档

当前执行策略调整为：

- 先完成中文版本：
	- `docs/README.md`
	- `ARCHITECTURE.md`
	- `裁剪宏定义说明.md`
- 对应英文版本延后，待中文审阅通过后统一翻译

### Phase B：补主线缺失文档

1. `api-xurl*`
2. `api-http-util*`
3. `api-xnet-v2*`
4. `api-network-tls*`
5. `api-xhttp*`
6. `api-xhttpd*`
7. `api-xws*`
8. `api-future-task-promise.en.md`
9. `api-regex*`

### Phase C：逐篇对齐仍有效文档

先基础 runtime，再线程/协程，再内存/容器，再 value/json/template。

### Phase D：补辅助文档

1. `MIGRATION*`
2. `BEST_PRACTICES*`
3. `FAQ*`
4. `EXAMPLES*`
5. `PERFORMANCE.md`


## 9. 验收标准

本轮文档工作完成时，必须满足：

1. `docs/` 中不存在继续公开旧网络/TLS 主线的正式文档。
2. `docs/README*` 可以准确引导到当前正式主线。
3. 当前主线模块不存在“代码已稳定但 docs 中完全缺页”的情况。
4. 所有仍保留的旧文档内容，都已经按当前代码逐篇对齐。
5. 不再出现 `xCore.LastError` 等明显过期 runtime 用法。
6. 中英文文档结构与事实保持一致。
7. 淘汰文档已迁入 `dev/` 并明确标记历史归档身份。


## 10. 当前状态

当前阶段已不再是“尚未开始”，而是已经完成第一轮中文主线重建。

### 10.1 已完成

- 已建立 `docs/api/`、`docs/guide/`、`docs/case/` 二级结构
- 已重写中文：
	- `docs/README.md`
	- `ARCHITECTURE.md`
	- `裁剪宏定义说明.md`
- 已将旧网络 / TLS 文档从 `docs/` 迁出并归档到 `dev/`
- 已补齐当前主线缺失的中文 API 文档：
	- `api-xurl.md`
	- `api-http-util.md`
	- `api-xnet-v2.md`
	- `api-network-tls.md`
	- `api-xhttp.md`
	- `api-xhttpd.md`
	- `api-xws.md`
	- `api-regex.md`
- 已对齐并修正第一批中文旧文档：
	- `types.md`
	- `api-base.md`
	- `api-file.md`
	- `api-network.md`
	- `api-string.md`
	- `BEST_PRACTICES.md`
	- `FAQ.md`
	- `MIGRATION.md`
	- `PERFORMANCE.md`
- 已补强中文线程 / 协程 / Value 文档：
	- `api-thread.md`
	- `api-coroutine.md`
	- `api-value.md`
- 已完成网络主线中文 API 文档的第二轮补强：
	- `api-xnet-v2.md`
	- `api-network-tls.md`
	- `api-xhttp.md`
	- `api-xhttpd.md`
	- `api-xws.md`
- 已完成内存与容器中文 API 文档的第二轮补强：
	- `api-mempool.md`
	- `api-mempool-fs.md`
	- `api-array.md`
	- `api-ptrarray.md`
	- `api-avltree.md`
	- `api-dict.md`
	- `api-list.md`
- 已完成继续补强的中文页：
	- `api-json.md`
	- `api-template.md`
	- `api-future-task-promise.md`
	- `BEST_PRACTICES.md`
	- `FAQ.md`
	- `MIGRATION.md`
	- `PERFORMANCE.md`
	- `docs/README.md`
	- `裁剪宏定义说明.md`
	- `api-crypto.md`
	- `EXAMPLES.md`
- 已重写中文 `EXAMPLES.md`，移除旧单头文件示例主线写法
- 已更新 `docs/api/README.md`，建立当前 API 主线索引
- 已把 `guide/README.md`、`case/README.md` 从占位页补成正式入口页
- 已新增首批正式中文教学页：
	- `docs/guide/first-xrt-program.md`
	- `docs/guide/runtime-thread-attach.md`
	- `docs/guide/xvalue-json-intro.md`
	- `docs/guide/coroutine-future-task-intro.md`
	- `docs/guide/xnet-v2-tls-intro.md`
	- `docs/guide/xnet-stream-wait-source-intro.md`
	- `docs/guide/minimal-http-service-intro.md`
	- `docs/guide/streaming-llm-api-intro.md`
- 已新增首批正式中文案例页：
	- `docs/case/json-config-system.md`
	- `docs/case/thread-coroutine-future.md`
	- `docs/case/minimal-http-service.md`
	- `docs/case/streaming-llm-api.md`
	- `docs/case/template-render-html.md`
	- `docs/case/http-json-template-chain.md`
	- `docs/case/xnet-stream-wait-source.md`

### 10.2 当前已确认的中文基线

当前中文文档中，已确认不再保留以下明显旧残留：

- `xCore.LastError`
- 旧 public TLS 主线 `xtlsctx / xrtTls*` 的推荐写法
- 旧“32 槽位环形临时内存”表述
- 旧 `nettcp / netudp / nethttp / netpoll / nettls` 作为当前正式主线

### 10.3 英文同步状态

- 已开始同步英文入口页：
	- `docs/README.en.md`
	- `docs/api/README.en.md`
	- `docs/guide/README.en.md`
	- `docs/case/README.en.md`
- 已开始同步英文辅助入口页：
	- `FAQ.en.md`
	- `MIGRATION.en.md`
	- `PERFORMANCE.en.md`
	- `BEST_PRACTICES.en.md`
	- `EXAMPLES.en.md`
	- `FEATURE_TRIMMING_MACROS.en.md`
- 已开始同步英文 API 主线页：
	- `docs/api/api-xnet-v2.en.md`
	- `docs/api/api-network-tls.en.md`
	- `docs/api/api-xurl.en.md`
	- `docs/api/api-http-util.en.md`
	- `docs/api/api-xhttp.en.md`
	- `docs/api/api-xhttpd.en.md`
	- `docs/api/api-xws.en.md`
	- `docs/api/api-future-task-promise.en.md`
	- `docs/api/api-regex.en.md`
- 已完成第二轮英文对齐与补强：
	- `docs/api/api-json.en.md`
	- `docs/api/api-template.en.md`
	- `docs/api/api-base.en.md`
	- `docs/api/api-file.en.md`
	- `docs/api/api-network.en.md`
	- `docs/api/api-string.en.md`
	- `docs/api/api-thread.en.md`
	- `docs/api/api-coroutine.en.md`
	- `docs/api/api-value.en.md`
	- `docs/api/api-time.en.md`
	- `docs/api/types.en.md`
	- `docs/BEST_PRACTICES.en.md`
	- `docs/EXAMPLES.en.md`
	- `docs/ARCHITECTURE.en.md`
- 已完成一轮英文入口与交叉链接一致性收口：
	- `docs/README.en.md`
	- `docs/FAQ.en.md`
	- `docs/case/minimal-http-service.en.md`
	- `docs/case/xnet-stream-wait-source.en.md`
	- `docs/api/api-crypto.en.md`
- 已开始同步英文 guide / case 页面：
	- `docs/guide/first-xrt-program.en.md`
	- `docs/guide/runtime-thread-attach.en.md`
	- `docs/guide/xvalue-json-intro.en.md`
	- `docs/guide/coroutine-future-task-intro.en.md`
	- `docs/guide/xnet-v2-tls-intro.en.md`
	- `docs/guide/xnet-stream-wait-source-intro.en.md`
	- `docs/guide/minimal-http-service-intro.en.md`
	- `docs/guide/streaming-llm-api-intro.en.md`
	- `docs/case/json-config-system.en.md`
	- `docs/case/template-render-html.en.md`
	- `docs/case/thread-coroutine-future.en.md`
	- `docs/case/minimal-http-service.en.md`
	- `docs/case/streaming-llm-api.en.md`
	- `docs/case/xnet-stream-wait-source.en.md`
	- `docs/case/http-json-template-chain.en.md`
- 当前英文同步策略保持不变：
	- 先以中文主线为事实来源
	- 再分批同步英文版
- 当前英文页中，`xCore.LastError` 仅保留在 `MIGRATION.en.md` 的迁移说明语境中，不再作为推荐当前用法
- 当前已完成一轮英文交叉链接检查，已清掉入口级误跳中文页的断链
- 当前已额外清理一批英文页中的明显中文残留与占位内容：
	- `docs/ARCHITECTURE.en.md`
	- `docs/api/api-time.en.md`
	- `docs/api/api-string.en.md`
	- `docs/api/types.en.md`
- 已继续收口一批容易误导“当前主线仍以单头文件驱动”的页面：
	- `docs/裁剪宏定义说明.md`
	- `docs/api/api-crypto.md`
	- `docs/api/api-crypto.en.md`
	- `docs/EXAMPLES.md`
- 已继续收口一批主文档里对旧网络模块名的重复列举：
	- `docs/README.md`
	- `docs/README.en.md`
	- `docs/FAQ.md`
	- `docs/FAQ.en.md`
	- `docs/api/api-xnet-v2.md`
	- `docs/api/api-xnet-v2.en.md`
	- `docs/裁剪宏定义说明.md`
- 当前旧网络模块名主要只保留在：
	- 历史裁剪宏清单
	- `MIGRATION.md / MIGRATION.en.md` 的迁移清单语境

### 10.4 当前完成面

到当前阶段，本轮文档重建的主目标已经完成：

- 中文主线已完成系统性重建
- 英文主线已完成系统性同步
- `docs/` 目录结构已收敛为：
	- `api/`
	- `guide/`
	- `case/`
- 旧网络 / 旧 TLS 文档已退出正式文档体系并归档到 `dev/`
- 新 runtime / thread / coroutine / future-task-promise / xnet-v2 / TLS session / HTTP / HTTPD / WebSocket / URL / regex 主线都已有正式文档入口
- 中文与英文入口页、API 索引页、guide 与 case 入口页均已建立
- 当前 docs 中最容易误导当前主线的旧 runtime 语义已基本清空：
	- `xCore.LastError`
	- 旧 public TLS 主线 `xtlsctx / xrtTls*` 推荐写法
	- 旧“32 槽位环形临时内存”表述
	- `xCore.sNull / xCore.iRet / xCore.sRet / xCore.LocalAddr` 作为当前推荐 public 合同的写法

### 10.5 合理保留项

当前 docs 中仍保留的少量历史术语，只出现在合理语境中：

- `types.md`
- `types.en.md`

这些页面中保留 `xCore.sRet / xCore.iRet / xCore.nRet`，仅用于明确说明：

- 当前主线**不再推荐依赖**这些隐藏返回槽

它们不再属于旧用法残留。

### 10.6 后续可选扩展

本轮主任务完成后，后续文档工作应视为增量扩展，而不是继续主线重建：

1. 继续新增更多 `guide/` 教学页
2. 继续新增更多 `case/` 综合案例页
3. 在新模块稳定后补充对应 API 文档
4. 结合后续真实 benchmark / platform validation 继续补 `PERFORMANCE*`、`MIGRATION*`

### 10.7 当前结论

按本 spec 的目标定义，`docs/` 文档重建工作已经达到可完成状态。
