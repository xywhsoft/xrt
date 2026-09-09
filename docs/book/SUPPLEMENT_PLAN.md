# SUPPLEMENT_PLAN — 缺失模块教学补充方案（三语言）

> 事实依据：以 `config/modules.json` 的 90 个公共头为全集，对照 143 章 `api:` 认领
> 与符号级全书检索（2026-09 核查）。本方案为执行计划，批准后按 P27/I29 两阶段推进。
> 纪律继承：书管线走 BOOK_SPEC + check_book + material 素材包；翻译管线走
> I18N_SPEC + check_i18n；双仓库提交。

## 0. 缺口清单（复查结论回顾）

| # | 模块（头） | 内容 | 现状 | 归宿决策 |
| --- | --- | --- | --- | --- |
| 1 | atomic / spin / wait | 原子原语、自旋锁、等待原语 | **第 9 章整章缺失**（order.json 有条目、卷一导言已预告、ch08/138/139 三处"第 9 章"悬空引用、zh 站挂旧编号产物页）；wait 已被 ch52–56 实教 | **补写 ch09**（唯一免费章号槽位） |
| 2 | pattern | 结构化模式匹配（Builder 多模式编译、`{name}` 捕获、预算三态） | 符号级零命中；start.html 模块地图与 api 卡已承诺 | **并入 ch30（regex）** |
| 3 | html | HTML 实体转义（TEXT/ATTRIBUTE 两种上下文，依赖 UNICODE） | 符号级零命中 | **并入 ch35（文本管线）** |
| 4 | future_bridge | Future/Promise 与任务系统的完成桥接 | 符号级零命中 | **并入 ch57（future）** |
| 5 | task_net | 网络任务组（xrtTaskNet*，任务组上网络引擎） | 符号级零命中 | **并入 ch60（调度实践）** |
| 6 | http1_net / http1_tls | HTTP/1.1 解析器的网络/TLS 流绑定（各 2 函数薄壳） | 符号级零命中 | **并入 ch89（http1）** |

**总原则**：
1. **章号与 URL 零变动**——不重排、不改 slug；ch09 用 order.json 现成槽位，
   其余六个模块以"宿主章新增 h3 小节"落地（BOOK_SPEC 允许 h2 八节内 h3 自由）。
2. **每处并入必须双向衔接**——并入点有前置铺垫（宿主章正文自然引出）、
   有后续回指（依赖该能力的后续章加一句"见第 N 章"）、卷导读与速查表同步。
3. **三语言一体推进**——zh 为事实源（P27），en/ru 走修订同步协议（I29），
   门禁与站点同步全部机器化。

## 1. ch09《原子操作、自旋锁与等待》补写方案（最大件）

- **定位**：卷一收官章（ch01 卷导言第 25 行已预告"原子操作（收官）：原子原语、
  自旋与等待的公共底座"）。type: concept，八节骨架全。
- **api 字段**：`atomic, spin, wait`。
- **素材包**：`python tools/gen_web_book.py material atomic`；
  头文件 atomic.h（xatomic32/64/ptr、内存序、CAS 循环、栅栏与 Pause）、
  spin.h（xrtSpin*）、wait.h（xbasic deadline / xwaitresult 三态）；
  现成示例 `examples/core/atomic`、`examples/core/atomic_tour`、
  `examples/concurrency/spin`、`examples/concurrency/deadline`（程序起步 2 个达标）。
- **与后续章的分工（防重复的衔接设计）**：
  - ch09 讲**原语层**：一条 CAS 循环、内存序的"为什么"、Pause、自旋锁的
    正确形态、xwaitresult 三态与 deadline 计算——止于单机单线程可见的小件；
  - ch53（取消体系）/ch56（channel）讲**系统化使用**——ch09 导读明确预告
    "等待原语的体系化应用在第 53 章"；核对 ch52–56 现文，如已自称基础归第 9 章
    则引用即刻生效；
  - ch138/139（configd）的"原子替换全局指针（第 9 章原子操作）"引用即刻生效；
    ch08 的"第 9 章原子操作的头文件就是首批消费者之一"（特性宏内联分派）生效。
- **避坑候选**（按 BOOK_SPEC 症状/原因/对照片格式）：无内存序的双检查 CAS；
  自旋锁内做长活；把 xwaitresult 当布尔用。
- **站点效应**：zh 旧产物页 ch09-atomic.html（自称"第 6 章"）被正式构建覆盖；
  zh 目录页该行从缺失态转正常态；ch144"一百四十四个章节"表述变为完全属实。

## 2. 六个模块的并入方案（宿主章 + 插入位置 + 衔接）

### 2.1 pattern → ch30《正则引擎》
- 插入：概念节新增 h3"当正则太重：结构化模式匹配（pattern）"
  （现有 h3"与字符串族、通配的分工"正前方，承接其"通配"话题）；
  示例节新增 h3 完整程序（embed `examples/text/pattern/main.c` 与
  `pattern_tour/main.c` 二选一进正文、另一个进练习）；速查表加一行；
  api 字段改为 `regex, pattern`。
- 衔接：ch30 概念节点明分工——regex 是"一个文本对一个模式"，pattern 是
  "一个文本对**多模式集**"（路由表/日志分类形态，Builder 预算防不可信模式）；
  ch104（路由）与 ch137（logstat 级别匹配）各加一句回指（"模式集形态见第 30 章"）。

### 2.2 html → ch35《文本管线》
- 插入：概念节新增 h3"管线里的一环：HTML 实体转义（html）"——模块无独立示例，
  以内联 c 块演示 xrtHtmlEscape* 两上下文（TEXT / 引号属性），点明依赖
  UNICODE（回指 ch27 charset）；速查表加一行；api 字段加 `html`。
- 衔接：ch35"环节间的数据契约"节顺势收编（转义是产出侧环节）；速查与
  契约节注明"输出侧编码环节"。

### 2.3 future_bridge → ch57《Future 与 Promise》
- 插入：概念节在"等待族与 Watch"之后新增 h3"桥接：把异步完成接回 Future
  （future_bridge）"；示例节新增 h3（embed `examples/concurrency/bridge_tour/main.c`）；
  速查加行；api 字段改 `future, cancel, future_bridge`。
- 衔接：ch58（executor）"Future 从哪来"处加回指；ch60 调度实践使用处回指。

### 2.4 task_net → ch60《调度实践》
- 插入：契约节"从骨架到网络引擎：卷七预告"处展开为新 h3"网络任务组
  （task_net）：卷六到卷七的桥"（embed `examples/network/task/main.c`）；
  api 字段加 `task_net`。
- 衔接：该 h3 本身就是卷六→卷七的门桥——ch63（事件端口）/ch66（TCP）开卷
  处各加一句"任务组视角见第 60 章"。

### 2.5 http1_net / http1_tls → ch89《HTTP/1.1》
- 插入：概念节"解析：从字节到结构"之后新增 h3"解析器的传输绑定
  （http1_net / http1_tls）"——4 个函数的薄壳定位讲清"为什么绑定层这么薄"
  （缓冲语义归 ch64、TLS 流归 ch86）；示例节新增 h3
  （embed `examples/http1/parse_buffer/main.c`；http1_tls 无示例，契约行说明
  与 net 版仅流来源不同）；api 字段改 `http1, http, http1_net, http1_tls`。
- 衔接：ch103（xhttp 服务端）与 ch107（流式服务）处各加回指。

### 2.6 并入的公共纪律
- 宿主章正文（导读/引入）各加一句并入预告，避免"突兀出现的小节"；
- 每个并入小节按 BOOK_SPEC 的节内规范（概念有图示或对照、示例完整可运行、
  契约与 docs/api 契约卡同源）；
- 速查表逐宿主加行；check_book 全绿为准。

## 3. 三语言执行计划

### P27（书管线，zh 事实源）
| 批 | 内容 | 验收 |
| --- | --- | --- |
| A | ch09 全章（素材包→八节→check_book） | check_book 单章绿 + build 覆盖旧产物页 |
| B | ch30 + ch35 并节（卷四） | 宿主章 check_book 绿 |
| C | ch57 + ch60 并节（卷六） | 同上 |
| D | ch89 并节 + 全局衔接核查（3 处悬空引用、4 处回指、卷一导言核对、ch144 计数核对）+ sync-index | check_book 全量绿 + 符号复查全命中 |

### I29（翻译管线，en/ru）
- 前置：glossary 先登记新术语（pattern/模式匹配、spin lock/自旋锁、
  future bridge、HTML escape 等，en/ru 译法一次定准）。
- en 批 1：ch09 en 全新翻译（金标准体例）；
- en 批 2：六宿主章**增量同步**（zh 加节 → en 同章加对应节，G4 的 h3 数量
  同步增加；走 SPEC 6.5 修订同步协议，PROGRESS 登记待同步→补译闭环）；
- ru 批 1/2：同序执行；
- 收尾：en/ru book index 插入 ch09 行 + 状态行 143→144 基数更新、
  PHASES/PROGRESS 的 143 表述全量更新、`gen_sitemap.py` 重跑（+2 URL）、
  hreflang 复验、check_i18n en/ru 全绿、三语言链接 0 断链、双仓库提交。
- 与 ch141 裁决**解耦**：互不阻塞，ch141 双语仍按 I27 呈报等待裁决。

## 4. 全局验收清单（P27+I29 完成时）

1. 符号级复查脚本重跑：7 个模块导出符号在 docs/book 命中 > 0；
2. "第 9 章 / Chapter 9 / глава 9"引用全部有实指（ru 侧审计并补齐章号引用）；
3. 90 个公共头全部被某章 api 字段认领（复跑 api 认领比对，目标：未认领数 0；
   tls_*/wait/websocket_upgrade/http_trailer/http_expect 等"实教未认领"项
   在 P27-D 一并补认领，属纯 frontmatter 修订）；
4. check_book 全绿（zh）+ check_i18n en/ru 全绿 + 三语言 0 断链
   + sitemap/hreflang 重生成校验；
5. 计数一致性：143→144 的全部口径（i18n PHASES/PROGRESS、en/ru 目录状态行、
   门面页章数表述）逐处更新，grep 复查无残留。

## 5. 风险与规避

| 风险 | 规避 |
| --- | --- |
| 章号重排引发 URL/交叉引用雪崩 | 方案天然规避：零重排，ch09 用现成槽位 |
| 并节打破 en/ru 的 G4（h3 数量） | I29 增量同步以"同章加节"为单位，h3 计数同步增加后再跑门禁 |
| ch09 与 ch52–56 的 wait 讲重复 | 分工线写死：ch09=原语，ch53+=体系；双向引用锚定 |
| http1_tls 无示例 | 定位讲清薄壳 + 契约行；不硬造示例（与库现状一致） |
| 翻译基数口径漏改 | 收尾用 grep 143/144 全量清点（含 PROGRESS/PHASES/门面/目录状态行） |
