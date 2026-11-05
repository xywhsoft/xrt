# 扩展库集中压实台账

启动日期：2026-10-25。范围为 `extlibs/*/config/modules.json` 登记的全部 21 个产品。
工作分支：`codex/extlibs-hardening`。起点：`1f58f86109cb4fd2b910aa8f850d4a3e072d825f`。
该起点包含尚在草稿 PR #4 中的 CI 修复；本轮改动单独提交，不修改其工作分支。

## 完成条件

每个产品均需留下源码复核结论、实际执行的测试与交付验证，以及未覆盖的环境边界。
现有 CI 全绿、清单完整或存在某个测试文件，都不单独代表该产品已经完成本轮压实。

1. 复核公开接口、依赖边界与生成资产，核对模块化、声明头及实现单头的行为。
2. 检查不可信输入、长度与数值边界、分配失败、错误保真、失败原子性。
3. 对有状态产品检查所有权、借用寿命、取消、超时、回调重入和关闭收尾。
4. 对确认的缺陷先增加能复现的回归，再做最小修复；复用模块化与单头测试用例。
5. 执行产品完整离线套件、包消费与裁剪检查；按改动选择 sanitizer、fuzz、互操作、性能或体积验证。
6. 在适用平台运行本地和 GitHub 检查，记录平台限制；相关变更分批提交供审查。

不降低门禁或删除有效断言来换取通过。线上凭据只在用户明确要求在线验证时读取；
这轮默认使用离线夹具与本机对端。保留调用方工作区改动，不自动合并或发布。
缺乏 NAT、Android、特定内核或长稳环境的证据时，明确记录限制，不宣称已完成这些验证。

## 批次

| 批次 | 产品 | 检查重点 |
| --- | --- | --- |
| 1 | xllm、xllm-session、xwork | 流式组装、预算数值、消息所有权、持久化与恢复、工具失败/取消 |
| 2 | xjwt、xoauth2、xacme | 不可信认证输入、选钥/验签边界、错误保真、事务回滚 |
| 3 | xmail、xsmtp、xpop3、ximap | MIME 和协议增量解析、TLS 错误、背压、失败收尾 |
| 4 | xhttp、xhttp-client、xhttp-policy、xhttp-server | 编解码边界、缓存与重试、流式收发、请求/连接寿命 |
| 5 | xws、xws-client、xws-server、xws-group | 握手与帧边界、分片、广播所有权、异步终态 |
| 6 | xruntime、xssh、xrudp | 动态值生命周期、SSH 状态机与包边界、重传/拥塞/收尾 |

## 逐库状态

下表资产数量保留启动时的去重基线，后续新增用例见各批次；不包含依赖库的测试，也不表示本机已执行全部资产。
`tests` 包括专门的在线或平台测试，执行时需要依其声明与支持范围选择。

| 产品目录 | 实现模块 | 源文件 | tests | single_tests | examples | 本轮状态 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| xacme | 14 | 16 | 28 | 10 | 1 | HTTP/Issue/DNS/provider 预算及清理修复本地通过，RSA/协议与测试资产支持依赖复核继续 |
| xhttp | 115 | 124 | 236 | 115 | 82 | 正文流/前置条件字节快照及专项测试通过，其余源码复核继续 |
| xhttp-client | 18 | 18 | 97 | 32 | 12 | Future 失败发布和七处字节视图修复、四环境专项及双平台交付通过，其余源码复核继续 |
| xhttp-policy | 27 | 22 | 49 | 19 | 15 | 待复核 |
| xhttp-server | 25 | 31 | 90 | 32 | 25 | 关闭/Upgrade、字节视图及完整 GCC/Clang/TCC 通过，其余源码复核继续 |
| ximap | 11 | 9 | 22 | 15 | 5 | TLS 助手退休修复及受影响回归通过，完整协议/生命周期复核继续 |
| xjwt | 1 | 4 | 3 | 2 | 1 | 已完成本轮：源码、本地、双平台认证 CI 及交付验收 |
| xllm | 1 | 12 | 3 | 1 | 2 | 回放/UTF-8 修复及包/裁剪/profile 通过，源码复核继续 |
| xllm-session | 1 | 8 | 2 | 1 | 0 | 恢复/终值修复及包/裁剪/profile 通过，源码复核继续 |
| xmail | 19 | 19 | 27 | 18 | 14 | TLS 助手退休修复及受影响回归通过，完整协议/生命周期复核继续 |
| xoauth2 | 1 | 3 | 3 | 2 | 4 | 已完成本轮：源码、本地、双平台认证 CI、Hydra 及交付验收 |
| xpop3 | 6 | 4 | 10 | 6 | 4 | 三组会话/错误候选与生产依赖修复、本地完整选择/示例/独立 TLS/交付通过，待本批 CI |
| xrudp | 19 | 20 | 76 | 97 | 0 | 待复核 |
| xruntime | 36 | 23 | 87 | 42 | 30 | 待复核 |
| xsmtp | 6 | 4 | 8 | 6 | 4 | TLS 助手修复及 73/73 CI 通过，原 SIGSEGV 唯一根因未证实，协议复核继续 |
| xssh | 66 | 65 | 71 | 66 | 65 | 待复核 |
| xwork | 1 | 8 | 1 | 1 | 0 | 路径/只读隔离修复通过，工具状态机复核中 |
| xws | 10 | 5 | 22 | 6 | 7 | 待复核 |
| xws-client | 5 | 3 | 7 | 5 | 3 | 已完成本轮：源码/API/所有权、四环境完整测试、双平台交付及 73/73 CI |
| xws-group | 3 | 3 | 4 | 3 | 2 | 已完成本轮：源码/API/所有权、四环境完整测试、双平台交付及 73/73 CI |
| xws-server | 6 | 5 | 25 | 13 | 4 | 已完成本轮：五个 C/十四 API、四环境完整选择与示例、双平台交付/profile 及 73/73 CI |

## 执行记录

### 初始基线

- 21 个扩展分别执行 `audit_repository(..., release=True)`：全部通过结构门禁，未发现 developing 模块或 review 体系。
- `python tools/amalgamate.py --all --check`：全部生成单头与源码一致。
- `python -m unittest tools/test_extension_layout.py tools/test_extension_single.py tools/test_release_maturity.py tools/test_http_layout.py tools/test_websocket_layout.py`：26 项通过。
- 清单快照与本地日志位于工作树内未跟踪的 `out/extlibs-hardening/`。
- 起点对应的 GitHub CI 为 run `39835061015`，73/73 成功；这是上一轮的基线证据。

### 第一批：会话预算边界

确认并修复以下边界：NaN 阈值绕过校验；安全预留的百分比计算溢出；默认尾部预算先窄化再截断；
prompt/output 与下一轮增量回绕后错误降压；完整预算阈值在 double 舍入后超出 uint64 转换范围。
不改变正常配置与有效 usage 的行为，溢出的 usage 饱和到 `UINT64_MAX` 并保留溢出压力。

- 修复前：Windows/GCC 模块化和单头各有 7 个新增断言失败。
- 修复后：上述两种构建各 245 个断言通过，零失败；两者复用同一完整离线测试入口。
- 发布成熟度、API 文档覆盖、参考生成与单头生成检查通过。
- Linux/Clang ASan/UBSan/float-cast-overflow 初次运行发现独立的 routing-key 栈寿命缺陷，已由后续修复关闭。
- Linux/TCC 单头编译发现 `xllm_profile.c` 使用静态 const 变量初始化静态表，已由后续兼容修复关闭。

主要证据：`session-before.log`、`session-single-before.log`、`session-after.log`、
`session-single-after.log`、`session-sanitize-after.log`、`session-tcc-after.log`，均位于 `out/extlibs-hardening/`。

### 第一批续：摘要请求寿命与 TCC

- 摘要调用的 routing-key 字符数组原先在内部块结束后失效，但请求仍借用该数组；将其寿命延长到同步派发结束。
  既有摘要测试会在模型回调中读取该头，修复前 ASan 报 `stack-use-after-scope`，无需新建镜像测试。
- 随后的 LSan 检测到 95,296 字节的四个泄漏分配，来自两个测试夹具遗漏：一次已提交压缩事务未 Destroy，
  以及借用请求在重新 Init 前未 Unit。补齐释放，保留全部原有断言。
- `xllm` 内建 profile 表改用真正的 C 常量表达式；局部宏在表后取消定义，保持所有能力位和公开结构不变。
  NaN/Infinity 回归数组使用自动存储，以兼容 TCC 的浮点常量初始化限制。
- 最终 Linux/Clang `-O1 -g -fsanitize=address,undefined,float-cast-overflow`：会话完整离线套件 245 个断言通过；
  `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` 和 `UBSAN_OPTIONS=halt_on_error=1` 均开启，无 sanitizer 报错。
- Linux/TCC：`xllm-session` 及 `xllm` 完整离线单头套件分别通过。GCC/Windows 模块化会话套件通过。

后续优先复核：快照/日志是否完整保留多模态 parts 与 native 数据；损坏记录的类型与数值边界；
长度累加的极值拒绝与失败原子性。上述项目尚未判定为缺陷，不据候选线索修改运行行为。

证据：`session-sanitize-lifetime-fixed.log` 为夹具泄漏报告，`session-sanitize-final.log` 为最终干净结果；
`session-tcc-final.log`、`xllm-tcc-final.log`、`session-windows-final.log` 保留相应编译器的运行结果。

### 第二批：完整消息持久化

复核确认旧写入路径遗漏全部多模态 parts 和整条消息的 native 数据；旧比较器也没有检查这些字段。
扩展往返比较器后，快照与日志两条恢复路径均复现数据丢失。增加畸形消息用例后，修复前共 17 个断言失败。

- 快照及 JSONL 日志使用同一完整消息序列化：保留全部 part 类型、字段、顺序和 native 原文；附件以严格 Base64 保存。
- 新写入格式升级为 v3，避免旧读取器悄悄丢弃新字段；读取兼容 v1/v2/v3，允许旧日志恢复后继续追加 v3。
- 消息读取拒绝错误字段类型、嵌入 NUL 的 C 字符串、非法 Base64/native JSON、未知 part 类型及 text/parts 冲突。
- 真实往返覆盖文本、推理、图片/音频/文件字节、URL、detail/native-type 元数据、整消息 native 和工具结果关联 ID。
- 15 类畸形消息分别通过公开 Load 和 Recover 两条路径拒绝；另外验证 v1/v2 快照及日志兼容与新格式版本标记。
- GCC/Windows 模块化及单头、Linux/TCC 单头、Linux/Clang ASan/UBSan/float-cast-overflow 加泄漏检查：各 316 个断言通过。
- 公开文档纠正 Save/Load 的 JSON 快照与 JSONL 日志区别；发布结构、API 覆盖和生成检查通过。

证据：`persistence-before.log`、`persistence-validation-before.log`、`persistence-modular-final.log`、
`persistence-single-final.log`、`persistence-tcc-final.log`、`persistence-sanitize-final.log`。
本轮改动集中于草稿 PR #5，叠加在 CI 修复 PR #4 上。独立邮件工作流 `40202790485` 已成功。

后续源码线索仍需复现：无符号 64 位字段恢复、配置窄化及序列检查；日志写入失败时文件账本的事务性；
空会话的日志控制；助手响应的 native block 是否通过已有公开转换 API 完整转入会话。

### 第二批续：真实 native 子类型与日志失败原子性

- `xllmMessageFromResponse()` 会生成 `thinking_signature` NATIVE part，内容是 opaque 签名而非 JSON。
  本轮刚增加的严格解析原先误拒绝了这种合法回放数据；加入真实子类型后复现两条恢复路径失败，
  已按子类型保留原文，其他需要 JSON 的 native 数据继续严格校验。
- 用目录阻塞已启用的日志路径，确认 NoteFileRead/NoteFileModified 返回失败后仍污染可见账本，
  同一路径重试又被去重而不写入日志；重启无法恢复。新增回归在修复前有 4 个断言失败。
- 新路径在追加失败后回滚，成功后才推进 render generation；追加成功同时使 journal sequence 的统计缓存失效。
- 观察回调不能重入账本修改；NULL 会话的 EnableJournal 返回参数错误，DisableJournal 安全返回。
- GCC/Windows 模块化、Linux/TCC 和 Linux/Clang ASan/UBSan/float-cast-overflow 加泄漏检查：各 326 个断言通过。
  Windows/GCC 单头完成相同回归后追加确认，不使用只编译证据替代运行。

证据：`native-signature-tcc-before.log`、`ledger-atomicity-before.log`、`ledger-windows-final.log`、
`ledger-tcc-final.log`、`ledger-sanitize-final.log`、`ledger-windows-single-final.log`。
一次 Windows 编译因宿主资源瞬时不足报 `cc1.exe: out of memory`；随后完整重编译运行通过。

下一步保留重点：恢复数值类型与序列合法性；助手 signed-thinking 转换与文本回放；完整产品包消费、裁剪与 profile 验收。

### 第三批：助手回放、UTF-8 与故障诊断

- 新增回归在修复前复现：普通助手文本生成不完整 JSON；带签名推理转换后丢失回答；多段签名重用整段 reasoning；
  SessionAddAssistantResponse 没有转入签名。xllm 的 4 个、session 的 2 个新增断言失败。
- 响应转换保留顺序相邻的 REASONING/thinking_signature 对与 TEXT part；每个签名回放对应块的推理，
  保留旧式 signature-only part 对聚合 reasoning 的兼容路径。普通无签名响应继续使用文本快路径。
- 修复 Anthropic 助手文本缺少 JSON 闭括号；session 复用公开的 FromResponse 转换，保存/恢复后仍可回放签名与回答。
- 转换测试覆盖输入被覆盖后的独立所有权；按实测分配点逐一注入失败，确认释放所有半成品输出。
- 原 OOM 测试在每轮清空总失败标志，实际只检查最后一轮；修正后确认首个分配失败没有错误码。
  补齐 Completions builder 的 OOM 诊断，并将 96 个分配点的扫描扩展到 OpenAI-compatible、GLM、Responses、Anthropic 四种方言。
- 独立公开 API 探针复现三字节过长 UTF-8 被接受；新增完整回归后有 6 个失败。
  修复使用耗尽后计数器判断最短编码的错误；14 类非法编码在内容、推理与 part 三个入口被拒绝且不替换旧值，7 个有效边界继续接受。
- 最终 xllm 套件：Windows/GCC 单头、Linux/TCC、Linux/Clang ASan/UBSan/float-cast-overflow 加泄漏检查，各 654 个断言通过。
  会话套件：Windows/GCC 模块化与单头、Linux/TCC 和 Linux sanitizer 均按最终源码通过 332 个断言。
- 两库 API 覆盖、生成与发布结构检查通过；扩展工具基线 26 项通过。
- 首批提交 `505c418b` 的 CI `40202854136`：73/73 成功。`7ab05f42` 的 CI `40205705160` 随后成功，独立邮件工作流也成功。

证据：`assistant-replay-before.log`、`session-assistant-replay-before.log`、`oom-diagnostics-before.log`、
`utf8-boundary-before.log`、`assistant-utf8-oom-windows-final.log`、`assistant-utf8-oom-tcc-final.log`、
`assistant-utf8-oom-sanitize-final.log`、`assistant-utf8-session-windows-final.log`、`assistant-utf8-session-sanitize-final.log`。

下一批已确认的问题：公开 Save/Load 将 `UINT64_MAX` 的上下文窗口恢复为默认的 204800（`boundary-probes-before.log`）。
需连同 UInt/Int 类型、窄化、序列单调性与终值屏障一起修复；尚未完成此项，也未宣称两库已完成整轮交付验收。

### 第四批：恢复数值、日志顺序与计数器终值

- 修复前数值恢复新增用例有 45 个断言失败，日志元数据及计数器回归另有 15 个失败。
  JSON 的 UInt 与非负 Int 分别精确读取，`INT64_MAX`、`INT64_MAX+1`、`UINT64_MAX` 配置往返不再回退默认值。
- 全部 13 个 uint32 配置字段在窄化前检查范围；显式错误类型、负数、null、溢出和非法预算返回 PARSE，
  仅缺失的可选字段沿用旧格式默认值。摘要 usage 同样保留完整 unsigned 范围。
- 快照拒绝零值、重复、倒退或溢出的消息序列，以及越过 next_sequence 的水位；保留合法递增的序列间隔。
  最终 turn 可正常保存消息；消息、日志、压缩次数和摘要 generation 到达终值后拒绝继续推进，不回绕到零。
- 日志在快照去重前检查操作名与数值结构；拒绝 generation 窄化/倒退、非法 flags 及未知操作。
  旧格式缺少 generation 时从当前 epoch 递增，不再把宽 compaction_count 窄化赋值。
- 修复真实首次裁剪日志 `from_sequence=0` 被拒绝的问题；后续裁剪必须衔接当前水位。
  压缩准备/提交、自动压缩和溢出恢复统一拒绝观察回调重入；失败时不污染布尔输出。
- Windows/GCC 模块化与单头、Linux/TCC 单头、Linux/Clang ASan/UBSan/float-cast-overflow 加泄漏检查：
  每种构建均为 436 个断言通过。API 覆盖、生成一致性、发布成熟度通过；21 个结构工具测试通过。
- Linux/GCC 正向裁剪与六个缺依赖反向检查通过；现有静态包消费者可链接运行。
  包消费者仍需增强实际行为覆盖，再连同动态包、体积及性能 profile 完成交付验收。

证据：`numeric-recovery-before.log`、`numeric-journal-before.log`、`numeric-windows-modular-final.log`、
`numeric-windows-single-final.log`、`numeric-tcc-final.log`、`numeric-sanitize-final.log`、
`numeric-tooling-final.log`、`session-trim-linux-final.log`、`session-package-static-linux.log`。

`3d552904` 的独立邮件工作流 `40209247955` 成功；其 CI `40209250734` 发现 Windows 独立 ACME 服务夹具的
TCP/UDP 端口兼容性失败，另批修复。此记录不将该次 CI 记为全绿，全部 21 库任务继续保持未完成。

### CI 续修：独立 ACME 服务端口

- CI `40209250734` 的 Windows job `121216750509` 在客户端运行前失败：先选择 UDP 临时端口，
  随后检查同号 TCP 时触发 WinError 10013。单一传输可用不能证明另一传输可用。
- 端口选择现在同时保留所有候选绑定，DNS 必须同时绑定 TCP 与 UDP；三个 HTTP 端点也保持独立。
  仅对地址占用及端口权限冲突有界重选，64 次后携原始原因报错，其他 socket 故障立即传播。
- 四项回归分别在本机 Windows 和 Linux 通过：实际双传输互斥与释放、协议间权限/占用冲突、
  非重试错误保真及次数上限。用例纳入既有 ACME mock 测试入口，不降低独立服务验证要求。
- 本地固定版本 Pebble 准备曾因上游 Git 拉取 TLS 中断未完成；真实端到端结果仍以修复后的独立服务 CI 为准。
  该夹具修复不代表 xacme 产品源码复核完成。

证据：`acme-windows-ci-failure.log`、`pebble-ports-windows-final.log`、`pebble-ports-linux-final.log`、
`pebble-prepare-local.log`。

### 第五批：实际包消费与 profile

- 原包消费者只初始化配置并检查核心版本，不能说明关键产品行为能通过真实库边界。
  增强 xllm 消费者：转换带签名响应、复制请求、覆盖原始输入并释放中间消息，再构建和解析 Anthropic JSON，
  确认推理、签名、回答仍保留且没有借用已释放存储；整个过程不发网络请求。
- 增强 session 消费者：复制用户文本、构建借用视图、保存恢复 `UINT64_MAX` 窗口，
  释放恢复后的会话再检查独立请求仍有效；测试生成的快照在结束时清理。
- 两库分别在 Windows/GCC 与 Linux/GCC 的 static/shared 包上链接并运行上述消费者：8 组通过。
  两库 Linux/GCC 的正向裁剪与缺依赖拒绝检查通过。
- 两库各自的体积 profile 均完成 single/static/shared 三种实测；性能 profile 的 smoke 各完成一次。
  这些 profile 使用未固定的基线，性能 smoke 不作为性能回归比较或长稳证据。
- 构建、打包、API 工具回归共 68 项通过。增强消费者不改变公开 ABI。

证据：`llm-package-consumers-linux-final.log`、`package-xllm-static-windows.log`、
`package-xllm-shared-windows.log`、`package-xllm-session-static-windows.log`、
`package-xllm-session-shared-windows.log`、`xllm-trim-linux-final.log`、
`llm-size-final.log`、`xllm-size.json`、`session-size.json`、`llm-performance-smoke-final.log`、
`xllm-performance-smoke.json`、`session-performance-smoke.json`、`package-api-tooling-final.log`。

`8d70f28c` 的 CI `40213017331` 已确认 Windows/Linux 独立 ACME 服务两项均成功，
独立邮件 `40213012058` 成功。完整 CI 随后完成，73/73 成功。

### 第六批：xwork 物理路径与只读隔离

- 先执行完整现有离线套件：186 个断言均通过，但 LSan 报告 85 字节泄漏，来自测试夹具的 stop 输出和
  被覆盖前的中断运行摘要。补齐两处 Unit 后消毒器通过，没有删除原有断言。
- 新路径用例在修复前有 5 个断言失败：目录链接允许读取外部文件及创建外部后代，内部链接和工作区根别名
  没有物理归一化，POSIX `/` 被错误判为不包含其后代。另一个公开只读子代理回归复现 `.xcode` 别名绕过。
- 工作区根按物理路径固定；解析目标时解析最近的已存在祖先，拼接尚不存在的后缀后核对边界。
  拒绝外部目录/文件链接与悬空链接，保留工作区内部合法链接及新文件路径。
  Windows 普通 DOS/UNC 路径去除系统返回的扩展命名空间前缀，保留进程工作目录和相对路径的可用性。
- 只读子代理在调用宿主许可策略前检查相对于工作区的物理目标；隐藏在普通别名后的控制目录仍拒绝。
- 最终 Linux/GCC 模块化、Linux/TCC 单头、Clang ASan/UBSan/float-cast-overflow 加泄漏检查：各 207 个断言通过。
  Windows/GCC 单头为 191 个断言通过；本机未授予符号链接创建权限，相关用例明确跳过，未宣称覆盖 Windows junction。
- xwork API 覆盖、参考生成、单头生成及发布成熟度通过。文档明确路径解析后到实际操作之间要求命名空间稳定，
  本轮并非针对并发恶意替换链接的句柄相对文件系统沙箱；该环境仍是未覆盖边界。

证据：`xwork-sanitize-baseline.log`、`xwork-sanitize-fixture-fixed.log`、`xwork-path-before.log`、
`xwork-control-alias-before.log`、`xwork-path-tcc-final.log`、`xwork-path-modular-linux.log`、
`xwork-path-sanitize-final.log`、`xwork-path-single-windows.log`。

继续项：xwork 工具/进程与 MCP 状态机、分配失败和错误路径，清单内探索/Python 实现的可达性及边界，
产品包与 profile 验收；其余扩展库仍按逐库台账推进，当前不将任一产品标记为整轮完成。

### 第七批：JWT 关键头部

- 复核四个 JWT 实现分片后，用真实 HS256/RS256/ES256 签名构造 17 类关键头部反例；
  修复前 PEM/HMAC、公钥缓存及 JWKS 验签共 119 个新增断言失败。
- 所有验签入口共用关键头部预检：未知扩展、非法/空/重复 crit、缺少对应字段及非 true 的 b64 返回 MALFORMED。
  依据 RFC 7515 4.1.11 与 RFC 7797 7；支持 b64=true 及 crit=["b64"]，保留普通非关键扩展的忽略规则。
- 纯解码入口仍可检查原始头部，未将解码误作验签。错误返回前释放已经复制的 kid。
- Linux/GCC 模块化、Linux/TCC 单头、Windows/GCC 单头、Clang ASan/UBSan/float-cast-overflow 加泄漏检查：
  每种构建完整 JWT 套件均为 611 项通过。原有时间边界、签发所有权、逐分配点失败及密钥格式用例保留。
- xjwt API 覆盖、参考生成、单头一致性及发布成熟度检查通过。

证据：`jwt-sanitize-baseline.log`、`jwt-critical-before.log`、`jwt-critical-tcc-final.log`、
`jwt-critical-single-windows.log`、`jwt-critical-modular-linux.log`、`jwt-critical-sanitize-final.log`。

继续项：JWKS use/key_ops/alg 元数据的处理、签发 OOM 根因保真与交付验证；OAuth 授权端点已有 query 的拼接复核。

### 第八批：JWT 密钥用途、受众类型与签发 OOM

- 补强既有 HS/RS/ES 签发逐分配点扫描后，3 个新增断言证明 Core 内存错误被改写为 JWT 解析错误。
  五处包装保留 MEMORY 根因；签发失败仍不返回半成品，也不污染调用方 claims。
- JWKS 用途及算法新增回归先有 82 个断言失败。现在过滤不允许验签或字段畸形的 use/key_ops，
  只接受签名相关操作且必须包含 verify；显式 alg 需匹配密钥族并绑定后续验签。
  同一 RSA 密钥的 RS256 令牌不能越过 RS384 绑定，匹配的 RS384 签名继续通过；NULL JWKS 报 ARGUMENT。
- 受众数组新增回归先有 12 个断言失败：匹配项前后混入数字、null、对象、数组或布尔值会被忽略。
  配置 Audience 时检查全部元素为字符串，匹配成功也继续验证剩余元素；保留合法数组的任一匹配规则。
- 最终完整 JWT 测试：Linux/GCC 模块化、TCC 单头、Windows/GCC 单头、Clang ASan/UBSan/float-cast-overflow
  加泄漏检查，各 916 项通过。独立 OpenSSL RSA/EC PKCS#8 互操作通过。
- 最终源码的 gcov 精确计数：行 93.84%、分支结果 75.82%，超过现有 85%/67% 门禁，未修改阈值；
  报告同时绑定新增测试头的哈希。中间件示例及故障扫描在 sanitizer 下通过，覆盖 11 类身份输入及 51 个真实登录分配失败。
- Windows/Linux 的真实独立 JWT、OAuth2 及两库组合消费者，在 static/shared 两种形式共 12 组通过，核对单一 Core 错误上下文。
  JWT 正向/缺依赖裁剪通过，single/static/shared 体积 profile 及未固定基线的性能 smoke 通过。
- 源码复核包含四个实现分片：Base64url/段拆分、签发的 COW 所有权、claims 时间/字符串、三条验证入口、
  RSA/EC DER 与私钥清零、缓存及 JWKS 生命周期。公开便利字符串提取仍按文档截断；中间件已使用完整标识校验。
  浮点 NumericDate、二进制 HMAC 密钥、JWE、远端取钥和证书链语义不在当前接口支持范围，未宣称覆盖这些能力。

证据：`jwt-signing-oom-before.log`、`jwt-signing-oom-after.log`、`jwt-jwks-policy-before.log`、
`jwt-audience-types-before.log`、`jwt-policy-tcc-final.log`、`jwt-policy-single-windows.log`、
`jwt-policy-linux-final.log`、`jwt-interop-coverage-final.log`、`jwt-coverage-policy-final.json`、
`jwt-example-sanitize-final.log`、`auth-package-policy-windows-final.log`、`auth-delivery-profiles-linux-final.log`、
`auth-package-policy-linux-final.json`、`jwt-size-final.json`、`jwt-performance-smoke-final.json`。

`a18dc3b3` 的 CI `40214563156` 在 Windows 核心 TCP 重入测试出现回调契约断言失败，
正在单独复核，不将该次 CI 记为全绿；该问题不会被认证套件通过所替代。

### 第九批：OAuth 授权 URL 与失败事务

- 新增公开 BeginLogin 回归先有 13 个断言失败。端点已有 query 时原先再次加入问号，
  把 response_type 混入原参数值；fragment 内拼接的参数不能送到授权服务器。
- 按 RFC 6749 3.1 保留现有 query，正确处理普通、空 query 及末尾分隔符；原样保留百分号编码。
  拒绝带 fragment 的配置端点、空端点及空客户端标识。微信生成的 provider fragment 移至全部查询参数之后。
- 早期配置错误也走统一失败收尾，清除先前 state/verifier/challenge/nonce，不能错误复用旧事务。
  用例从真实成功的登录事务开始，修改配置后验证失败原子性。
- 完整 OAuth 套件：Linux/GCC 模块化、TCC 单头、Clang sanitizer 加泄漏检查各 279 项通过；
  Windows/GCC 单头 286 项通过（既有平台分支数量不同）。最终两个平台的 static/shared 独立与组合包消费者也通过。
- OAuth API 覆盖、参考生成、单头一致性及发布成熟度检查通过。

证据：`oauth-login-url-before.log`、`oauth-login-url-tcc-final.log`、`oauth-login-url-linux-final.log`、
`oauth-login-url-single-windows.log`、`auth-package-policy-windows-final.log`、`auth-delivery-profiles-linux-final.log`。

OAuth HTTP 生命周期、token 流程及发现/身份组合仍需后续复核，未将授权 URL 的回归作为整库验收替代。

### CI 续修：TCP 关闭状态与回调完成

- `a18dc3b3` 的 Windows job `121232795168` 实际失败点是核心 `test_net_tcp_server_reentrant` 的回调契约断言。
  该测试只等 CLOSED 状态就读取 Accept/Close 计数；公开头已明确状态先于 Close 通知发布，不能替代回调完成。
- 保留原来的唯一回调、状态、Listener 退出和聚合队列断言，改为等待真实 Accept/Close 完成后检查。
  在真实 Close 回调内加入测试门闩，确定性保留 CLOSED 已发布但回调未完成的窗口，再由主线程放行。
  未修改网络运行时，也没有用固定 sleep 或放宽断言规避时序。
- Windows/GCC 定向构建运行、Linux/Clang ASan/UBSan 加泄漏检查通过；两个平台各复跑 32 次通过，
  每次均执行上述确定性窗口及 16 连接队列背压验证。

证据：`a18-windows-ci-failure.log`、`tcp-server-callback-windows.log`、`tcp-server-callback-sanitize-linux.log`、
`tcp-server-callback-repeat-windows.log`、`tcp-server-callback-repeat-linux.log`。

### CI 续修：JWT 测试夹具的可空算法名

- `77b1d0d5` 的 Linux 认证 job `121242706124` 在 GCC `-O2 -Werror` 下拒绝
  critical header 夹具将可空的 `xjwtAlgName` 返回值直接交给 `%s`。
  先验证算法名，异常时保留失败断言并释放已建缓存密钥；正常路径复用已验证名称。
- Windows/GCC 与 Linux/GCC 的完整 JWT 入口均为 916 项通过；未关闭编译警告或降低断言。
- 前一批最终源码的 Windows/Linux 完整清单套件也已通过，包含模块化、两个单头入口、
  时间边界和中间件示例；当前 CI 失败不能由这些本地结果替代，仍需新提交验证。

证据：`77-auth-linux-ci-failure.log`、`jwt-ci-guard-gcc-final.log`、
`auth-config-alias-windows-final.log`（仅其中 JWT 段；后续 OAuth 段为中间测试结果）、
`jwt-full-windows-final.log`、`jwt-full-linux-final.log`。

### 第十批：OAuth 配置别名与端点所有权

- 新增公开 API 回归在原实现上有 23 项失败：传入 `&client.Config` 会先清空源配置；
  从 Microsoft 预设复制并修改配置后传回，会释放仍被新配置引用的三个动态端点。
- 先快照配置，再按新配置的全部九个字符串字段保留仍引用的旧分配，包含合法子串别名；
  其余旧端点立即释放。实现不分配额外内存，仍清空旧登录事务；外部字符串继续借用。
- 用 Core 分配预算检查逻辑存活数量，避开底层内存池高水位；九个字段轮流覆盖三个旧端点，
  检查立即释放无关端点及 ClientUnit 的唯一所有权收尾。夹具自身遵守带 Size 的统计接口契约。
- Linux/TCC 与 Clang ASan/UBSan 加泄漏检查的完整 OAuth 入口各 316 项通过；
  Windows/GCC 完整清单套件通过，模块化和单头各 323 项，另含 OIDC 组合、离线示例和最小入口。
- 独立 HTTP/TLS/未交付构造生命周期全部探针通过。gcov 联合覆盖为行 92.33%、
  分支结果 77.58%，保持 91%/75% 原门禁；JSON 记录精确计数和输入哈希。
- 三个实现分片已复核：PKCE/state 与 URL、token/刷新及回调错误、HTTP/TLS URL/解析/发送期限
  及 owned/borrowed engine 收尾。应用 OIDC 辅助策略与真实示例的发现/身份/刷新路径也已阅读。
  未读取线上凭据；真实 provider 注册、远端 CA 和长稳环境仍未验证。
- API 文档、参考、生成单头和发布成熟度门禁通过。剩余故障注入、网络 sanitizer、
  包/裁剪/profile 及当前提交 CI 继续执行，整库尚未标为完成。

证据：`oauth-config-alias-baseline-final.log`、`oauth-config-alias-tcc-final.log`、
`oauth-config-alias-sanitize-final.log`（完整入口段）、`oauth-full-alias-windows-final.log`、
`oauth-coverage-alias-final.log`、`oauth-coverage-alias-final.json`。

### 认证本地验收补全

- OAuth 最终源码的 Windows/Linux 完整清单套件均通过：分别 323/316 项完整测试，
  两平台均执行模块化与单头、OIDC 组合 15 项、三个离线示例及最小入口。
- Clang sanitizer 加泄漏检查执行 OAuth 完整入口、token/刷新/微信逐分配点故障扫描、
  TCP/TLS 各阶段故障、OIDC 组合及两个示例的分配失败和恢复测试，全部通过。
- 独立 HTTP/TLS 的全部 23 个分组在 Clang sanitizer 下通过，含 139 个真实分配失败位置、
  证书/身份/SNI、100/101 字段和 trailer、1 MiB 上限、raw NUL、截断、发送期限和构造退休。
- Windows/Linux 的 JWT/OAuth/组合 static/shared 共 12 个真实消费者全部通过；
  OAuth 裁剪正向及缺依赖拒绝、single/static/shared 体积测量和未固定基线的性能 smoke 通过。
  真实 OIDC 示例只构建运行无参数 usage，不读取在线凭据。
- `3da19a3f` 的 CI `40307069156`：两库扩展及元数据 job 已成功，双平台认证 job 仍在执行；
  独立邮件 run `40307060575` 已成功。上述结果尚不作为整库远端验收完成。

证据：`oauth-full-alias-windows-final.log`、`oauth-delivery-alias-linux-final.log`、
`oauth-config-alias-sanitize-final.log`、`oauth-tls-sanitize-alias-final.log`、
`auth-package-alias-windows-final.log`、`auth-package-alias-windows-final.json`、
`auth-package-alias-linux-final.json`、`oauth-size-alias-final.json`、
`oauth-performance-smoke-alias-final.json`、`acme-jose-sanitize-final.log`（开头 OIDC usage 段）。

### 第十一批：ACME JOSE 失败诊断

- JWS 签名将非法私钥的真实 crypto 错误覆盖为 MEMORY；真实回归使用有效公钥和零私有标量，
  与底层直接签名的 kind/domain/code/operation 对照，在修复前失败。
- 补强既有逐分配点扫描后，JWS 的首个 OOM 也因被改写为 JOSE 错误域而失败。
  两种 JWS 组装仅在底层未提供诊断时补 INTERNAL，保留实际 crypto/allocator 原因。
  同步纠正私有头中把扁平 JSON 误写为紧凑序列化的注释。
- Windows/Linux GCC 模块化、Linux TCC 单头、Clang ASan/UBSan 加泄漏检查均通过：
  RFC 固定密钥/JWK/指纹/签名/EAB 对照继续有效，JWK/JWS/EAB 的 1/14/13 个分配失败点全部通过。
- 生成单头、API 覆盖、参考和发布成熟度检查通过。core/JOSE 源码复核结束，
  CSR/PEM 的曲线识别、畸形 DER 与敏感临时缓冲等候选继续复核；其余 ACME 源码尚未完成。
- 既有 flow 控制/故障矩阵在 Clang sanitizer 加泄漏检查下全量通过；保留实际 JOSE/CSR、
  账户/签发/吊销/滚动/存储逻辑，HTTP 与存储失败点为受控边界，不能替代真实 CA 互操作。

证据：`acme-jose-diagnostic-before.log`、`acme-jose-oom-before.log`、
`acme-jose-tcc-final.log`、`acme-jose-modular-windows-final.log`、
`acme-jose-modular-linux-final.log`、`acme-jose-sanitize-final.log`、
`acme-flow-diagnostic-sanitize-final.log`。

### JWT / OAuth2 逐库验收核对

本轮 21 个产品中，xjwt、xoauth2 两个产品完成验收，其余 19 个继续。
验收绑定认证源码提交 `3da19a3fcf5806230cc74096d9c48015ee68a0a6`；随后 ACME 改动不修改该输入闭包。
这不表示整仓最新提交或全部扩展已验收。

| 完成条件 | xjwt 证据 | xoauth2 证据 |
| --- | --- | --- |
| 公开接口、依赖、生成资产 | 四分片及公开头复核；API/参考/生成/发布检查通过 | 三分片、公开头和四个示例复核；相同元数据检查通过 |
| 不可信输入、数值、OOM、错误与失败原子性 | 签名关键头、JWK 元数据、受众全部类型、时间极值及三算法逐分配点扫描 | URL/配置别名、事务焚毁、token 字段/NUL/整数、流程/HTTP 各阶段故障及示例分配失败 |
| 所有权与生命周期 | 签发 COW、缓存/JWKS/DER/私钥收尾和 sanitizer | copied/borrowed 配置、部分响应、owned/borrowed engine、取消/发送期限/退休和 sanitizer |
| 回归先复现，再修复 | 关键头 119、签发 OOM 3、JWK 82、受众 12 项失败记录 | 授权 URL 13、配置别名 23 项失败记录 |
| 完整离线与交付 | Windows/Linux 全套模块化/单头/示例；OpenSSL；包/裁剪/三形态体积/性能 smoke | Windows/Linux 全套、OIDC/示例；全组独立网络；包/裁剪/三形态体积/性能 smoke |
| 适用平台与远端 | 双平台 JWT/OAuth 认证及本库 Extension/Metadata job 成功 | 相同双平台 job、本库 Extension/Metadata 及真实 Hydra HTTPS job 成功 |

远端 run `40307069156` 中，Windows/Linux 认证 job 分别为 `121518695074`/`121518695099`，
真实 Hydra HTTPS job 为 `121518695080`，均为 success；本库扩展与元数据 job 也均成功。
此时该 run 的其他整仓任务仍在执行，未将它登记为 73/73 全绿。

已下载两个平台的 `auth-coverage-*` 产物，核对四份报告中的全部自有源码和测试输入哈希：
39 个去重输入与指定 Git 提交相符，当前工作树的相应文本内容也相符；只允许 Git checkout 的 LF/CRLF 差异，
零其他内容差异。JWT 两平台各 1189/1267 行、881/1162 分支结果；
OAuth Linux 为 1205/1303 行、859/1106 分支结果，Windows 为 1203/1303 行、859/1106 分支结果。
原 85%/67%、91%/75% 门禁均通过。

验收边界保留：JWT 的整数 NumericDate、文本 HMAC 密钥及当前支持算法；
OAuth 不声称完成真实商业 provider 注册/线上凭据/长期运行验证。
Hydra 为 CI 独立对端，不能替代这些环境；性能 smoke 没有固定基线，不作为吞吐回归结论。

证据：此前各批本地日志及交付报告；`ci-auth-3da/auth-coverage-*`；
`auth-ci-acceptance-input-audit.json`（含四份报告摘要、哈希和输入核对结果）。

### 第十二批：ACME EC 私钥导入边界

- 40 组格式夹具经 EC 专用和通用两个实际入口执行，在旧实现上出现 56 项失败：
  算法/曲线/版本被忽略，零私有标量或不匹配公钥被接受，重复/乱序/截断字段被跳过；
  已拒绝的输入还可能没有新诊断或修改原输出。夹具预置无关 MEMORY 错误，避免旧错误掩盖结果。
- 现在验证 PKCS#8 的 id-ecPublicKey/P-256、SEC1 版本与命名曲线，派生并核对所有公钥，
  消费完整 DER；RFC5958 版本与外层公钥对应，attributes 合法且按 DER 排序。
  合法省略公钥/内层曲线、带 attributes 及 RFC5958 公钥的输入均保留。
  未声明曲线的传统 SEC1 沿用既有 P-256 上下文，不把它误称为 RFC5915 严格生成格式。
- EC 读取先在局部结果验证，通用读取同样只在成功时发布，失败保持原输出。
  EC 专用入口的解码 DER 与局部密钥在收尾时清零；RSA 路径和写侧临时缓冲继续单独复核。
- Linux/TCC 生成单头、Windows/Linux GCC 模块化及 Clang ASan/UBSan 加泄漏检查通过。
  原 CSR/PEM/RSA 固定向量不变；94 个 CSR、46 个 PEM 写出、两个读取入口各 1 个真实分配失败点通过，
  新增读取 OOM 的输出不变断言。
- OpenSSL 3.5.5 独立生成的 P-256 PKCS#8、SEC1、RFC5958 输入，均经两入口导入并生成 CSR，
  OpenSSL 验签且核对 CSR 公钥通过；同工具生成的有效 secp256k1 密钥被两个入口拒绝。
  新源码下原 flow 控制/故障矩阵在 Clang sanitizer 通过。
- 完整 `measure_acme_coverage.py --compiler gcc --temp-root /tmp` 通过：全部模块测试、
  独立 HTTP/TLS/所有权、五家 provider、18 项 grant 对抗验证及 15 项模拟 CA 测试全部运行，无 skip。
  分母包含全部 16 个自有 C 文件，5766/6633 行（86.93%）、4735/6715 分支结果（70.51%），
  保持 70%/55% 原门禁；输入哈希在测量期间无变化。尚未据此将 ACME 整库标为完成。

证据：`acme-ec-import-before-final.log`、`acme-ec-import-tcc-final.log`、
`acme-ec-import-modular-windows-final.log`、`acme-ec-import-modular-linux-final.log`、
`acme-ec-import-sanitize-final.log`、`acme-ec-import-openssl-final.log`、
`acme-flow-ec-import-sanitize-final.log`、`acme-ec-import-coverage-final.log`、
`acme-ec-import-coverage-final.json`。

### 第十三批：ACME 解码私钥退休前擦除

- 新增探针在实际 `xrtPemDecodeNew` 分配与实际 `xrtFree` 之间观察原始字节，
  不替换解析、密码学或分配行为。原实现有五条路径在释放时仍包含私钥 DER：
  EC 拒绝后的通用回退、RSA PKCS#8、RSA PKCS#1、坏 PKCS#1 和不支持算法的 PKCS#8。
- 补齐通用读取的三个释放位置，覆盖成功、格式拒绝和算法拒绝。
  七条入口路径累计观察 10 份真实 DER，修复后全部在 Core 释放/隔离前为零；
  同时核对读取成功/失败结果及所有解码分配确实退休。
- 该探针登记为 `acme_csr` 的单头故障测试，进入产品默认清单套件；
  原 CSR 的外部 RSA 固定夹具提取为两测试共享头，保持字节及签名向量不变。
- Linux/TCC、Windows/Linux GCC 的清单定向单头、Clang ASan/UBSan 加泄漏检查通过；
  修复后的两平台完整 CSR 向量和 142 点分配失败回归继续通过。
  API/参考/单头/发布门禁通过。写侧含私钥缓冲的退休及 RSA DER 语义仍继续复核，
  不把解码擦除视为全部敏感内存审计完成。
- 上一批全量覆盖率报告绑定 `fbda2a18` 前后的 EC 导入实现；本批三个新增清零调用与新探针
  由上述定向验证及新提交 CI 验证，不将旧报告冒充为本批精确计数。

证据：`acme-key-wipe-before.log`、`acme-key-wipe-linux-final.log`、
`acme-key-wipe-windows-final.log`、`acme-key-wipe-sanitize-linux-final.log`、
`acme-key-wipe-modular-windows-final.log`。

远端补记：认证提交 `3da19a3f` 的 run `40307069156`，以及 JOSE 提交 `0dd55edd` 的
run `40309007965` 均最终 73/73 success；各自的独立邮件 run `40307060575`/`40308999365`
也成功。新 EC 导入/擦除提交尚需各自当前 CI，不由此前全绿替代。

### 第十四批：ACME HTTP Future 诊断、取消与失败收尾

- 实际 Future/Promise、Core 分配器和 HTTP 解析器配合受控传输边界，覆盖连接、发送、
  响应头/体及认证 EOF 的生产任务错误、直接失败、超时取消和错误包装分配失败。
  同一最终 83 个场景在旧实现出现 63 项失败，修复后全数通过；另有 3 项无效参数输出检查通过。
  每次失败后独立发起正常请求，核对上下文可继续使用，未自动重放失败请求。
- 通过 Future 取回不可变生产任务错误，保留 MEMORY/TIMEOUT；其余底层异常在 HTTP 层
  归入 I/O 并保留原因链。清理前持有原始错误，失败响应全零；超时先请求取消。
  发送阶段仍位于 UNCERTAIN 原因帧之下，结果未知位独立于错误分配存在。
- 真实 provider 回归发现两项中间实现问题：TLS EOF 的底层 PROTOCOL 不能阻断安全读取重试；
  包装本身 OOM 必须报告 MEMORY，停止内存失败重试。均在源码修正，原有断言未放宽；
  涉及五家 provider 的 19 项断流/重试/OOM 定向回归在 Clang sanitizer 下全部通过。
- 新增测试登记在 acme_http 的默认 single_tests，Windows/GCC、Linux/TCC、
  Linux/Clang ASan/UBSan/float-cast-overflow 加泄漏检查各 84 个分组通过。
  原因链、取消通知、阶段调用次数、部分响应分配退休、真实分配无泄漏均有断言。
  受控边界不能替代真实 TCP/TLS 和 engine 生命周期。完整独立 HTTP/TLS/所有权/UDP/生命周期
  及全部 721 项 provider 场景随后在 Clang sanitizer 加泄漏检查下通过；全量覆盖率进入模拟 CA 阶段。
- API/参考/生成/发布门禁通过。上一批 `9038dbf0` 对应 CI `40318715631` 最终 73/73 success，
  独立邮件 `40318707544` success；新 HTTP 改动需自己的远端结果。

证据：`acme-http-future-before-final.log/json`、`acme-http-arguments-before.log`、
`acme-http-future-tcc-final.log`、`acme-http-future-windows-final.log`、
`acme-http-future-sanitize-final.log`、`acme-http-provider-regressions-final.log`、
`acme-http-future-interop-sanitize-final.log`。

HTTP 批次全量补记：`measure_acme_coverage.py --compiler gcc --temp-root /tmp` 成功退出，
16 个自有 C 文件共 5789/6666 行（86.84%）、4753/6743 分支结果（70.49%），原 70%/55% 门禁不变。
全部模块、独立 HTTP/TLS/所有权/provider、18 项 grant 对抗及 15 项模拟 CA 测试通过，无 skip。
报告的 742 个去重输入哈希与 `71dd59c9` 全部相符（允许 Git checkout 换行差异），测量期间无输入变化。
该报告绑定 HTTP 提交；随后 Obtain 的 5 个已测输入发生变化，不能将旧计数冒充新 Obtain 覆盖率。
证据：`acme-http-future-coverage-final.log/json`、`acme-http-coverage-input-audit.json`。

远端 `71dd59c9` 的 run `40329603967` 在 macOS 既有 server ownership 取消竞争断言失败
（job `121595202596`）；尚未完成其他任务，不登记为全绿。独立邮件 `40329599491` 成功。
该断言与 Future 终态发布的同步关系继续定位，保留原始失败日志。

### 第十五批：ACME Obtain 账户持久化与失败原子性

- 账户读取的 MEMORY/I/O/PERMISSION/STATE 被当作不存在继续开户；导出私钥失败仍继续签发；
  保存失败的 MEMORY 被重写成 I/O，清理还可能覆盖原错误。新的 20 个场景在旧源码上全部失败。
- 只有明确 NOT_FOUND 才允许 bootstrap，并清除这个已处理状态；读取其他错误立即停止，
  导出/保存失败都不再调用后续阶段。保留真实子操作诊断；无诊断的失败补 INTERNAL。
  清理前保留主错误，失败产物统一 GrantUnit 且 renewed=false。
- 存储读取和新导出的账户私钥文本均在实际 Core free 前清零，失败证书私钥通过公共析构清零；
  探针观察释放前字节，并断言创建/导出/保存/签发次数、临时客户端一次退休、调用方借用配置不变、
  输出交付/回滚及真实分配余额。存储、客户端和清理为受控边界，不冒称真实 CA/文件系统端到端。
- Windows/Linux GCC 清单（原公共测试、单头入口及新增 20 场景）、Linux/TCC 和
  Clang ASan/UBSan/float-cast-overflow 加泄漏检查通过。实际 owned/borrowed 引擎及 Obtain Done
  生命周期、聚合 provider 保护和八个 UDP 生命周期场景，在独立 Clang sanitizer 入口继续通过。
- 新测试进入 acme_obtain 默认 single_tests；公开说明补全失败语义，API/参考/生成/发布检查通过。
  HTTP 上一批全量覆盖率不覆盖本批新控制流，后续远端需验证本批提交。

证据：`acme-obtain-account-probe.log`、`acme-obtain-fault-before.log/json`、
`acme-obtain-account-windows-final.log`、`acme-obtain-account-linux-final.log`、
`acme-obtain-account-tcc-final.log`、`acme-obtain-account-sanitize-final.log`、
`acme-obtain-account-ownership-sanitize-final.log`。

### 远端回归修复：server ownership 取消与终态发布

- `71dd59c9` 的 macOS job `121595202596` 在既有取消竞争断言处失败：
  `xrtFutureCancel` 返回 false 时仍观察到 PENDING。源码与公开契约表明这可以合法发生：
  PromiseClose 先请求一次性取消令牌并等待通知结束，再发布 CLOSED。
- 新增真实取消观察者门闩，将关闭线程停在已请求取消、尚未发布终态的窗口；
  在原断言上稳定复现同一行失败，无 sleep、伪造状态或修改运行时。
  重复取消现在核对实际令牌已经请求，或终态已经可见；强制场景进一步断言返回 false、
  Future 保持 PENDING、零等待超时，释放门闩后必须 CLOSED。原退休、图与精确分配平衡检查保留。
- Windows/GCC 与 Linux/Clang sanitizer 的模块化/单头均通过，Linux/TCC 的 select/epoll 通过。
  Windows 与 Linux sanitizer 各 32 次模块化/单头 select/native 实际运行通过，
  每平台包含 32 个强制发布窗口和 256 次原关闭/取消竞争，无泄漏或 sanitizer 报错。
  macOS 的最终确认交由新提交 CI；不将本机结果冒充该平台已通过。

证据：`ci-http-macos-121595202596.log`、`net-server-publication-before.log`、
`net-server-publication-windows-final.log`、`net-server-publication-sanitize-final.log`、
`net-server-publication-tcc-final.log`、`net-server-publication-repeat-windows-final.log`、
`net-server-publication-repeat-sanitize-final.log`。

远端补记：`1517d655` 的 run `40333596756` 中 macOS Apple Clang job `121609117936`
已成功，稳定复现的取消发布窗口已在该平台验证。独立邮件 `40333586361` 成功；
此时其他整仓任务仍在执行，不登记为 73/73 全绿。

### 第十六批：ACME 私钥写出不再产生动态中间副本

- 实际 Core 分配器、DER/PEM 与 ACME 写出源码的探针确认：五个正常夹具每次有
  36–60 次分配，累计 33 个 ACME 缓冲在实际 Unit 前仍含私钥（每例 4/4/10/5/10 个）。
  普通动态缓冲还会在扩容和 Core DER 临时组装时退休副本，仅擦除最终缓冲不能关闭这些路径。
- EC/RSA 共用有界反向 DER 组装，容量由固定 RSA 字段加头部余量决定；完成后继续用 Core DER
  验证与 PEM 编码。只有交付的 PEM 文本分配堆内存，整个局部 DER（含未使用区域和元数据）
  在成功及分配失败返回前均用真实 volatile 清零。未更改普通 Core buffer/DER 的公共契约。
- 新增默认 single_tests 探针：29 个正常/输出 OOM/恢复/DER 长度场景，实际分配计数排除
  全部堆中间副本，观察真实私有区域清零并核对输入不变、往返身份和精确分配余额。
  17 个长度越界、SIZE_MAX 和无效算法枚举场景在旧实现未按参数错误拒绝，修复后全部拒绝。
- 14 种合成 RSA 长度测试仅验证编码布局，不声称密钥数学有效。独立对照的 21 份输出
  与旧实现逐字节一致，包括高位补零、长度转换、全部字段容量、前导零及旧 n/e/d/不完整 CRT 回退。
  外部 RSA PKCS#8 固定向量在默认测试中精确匹配；OpenSSL 3.5.5 对四份真实 EC/RSA 产物检查成功。
- Windows/Linux GCC 的模块化 CSR 固定向量、单头导入/导出探针，以及 TCC、Clang
  ASan/UBSan/float-cast-overflow 加泄漏检查均通过。原 CSR 的 94 个分配失败位置、两个读取入口各
  1 个位置不变；写出从原 46 个分配点缩为唯一交付分配，该点失败保持 MEMORY 且局部 DER 清零。
  另有五个写出路径的同类输出 OOM 与恢复断言。flow 的 176 个分组在 Clang sanitizer 下通过。
- API/参考/生成/发布检查通过。此前 HTTP 全库覆盖率报告绑定 `71dd59c9`，不能冒充本批精确计数。
  RSA 导入结构/算法语义、flow/provider 及其余源码继续；不将该编码路径视为全库敏感内存审计完成。

证据：`acme-key-export-before-final.log`、`acme-key-export-tcc-final.log`、
`acme-key-export-windows-final.log`、`acme-key-export-linux-final.log`、
`acme-key-export-sanitize-final.log`、`acme-key-export-probe-windows-final.log`、
`acme-key-export-probe-sanitize-final.log`、`acme-key-export-interop-final.log/json`、
`acme-key-export-oom-windows-final.log`、`acme-key-export-oom-linux-final.log`、
`acme-key-export-oom-sanitize-final.log`、`acme-key-export-flow-sanitize-final.log`。

### 第十七批：ACME 只读 POST 重试保留内存失败

- 受控 HTTP 交付真实分配器产生的 MEMORY 原因及成功分配的 UNCERTAIN 上下文；
  故障注入在交付前已结束，模拟生产任务失败后消费者仍可正常分配的情况。
  原只读 POST-as-GET 仅检查结果未知码，单次 MEMORY 会被第二次成功吞掉；
  持续 MEMORY 仍进行三次 POST、两次 nonce 获取和退避。八项矩阵中这两项在旧源码失败。
- 现在 MEMORY 无论是否带 UNCERTAIN 都立即返回，同一不可变错误及原因链保留，
  不取下一 nonce、不签名重发或退避。普通 I/O/超时的结果未知仍可有限恢复，最多三次；
  无 UNCERTAIN 的普通失败仍立即返回。每项之后显式发起的新请求在原客户端恢复成功。
- 新矩阵并入既有默认 flow 单头故障测试，无新增测试资产或放宽旧断言。
  Windows/Linux GCC、Linux/TCC 全量及 Clang ASan/UBSan/float-cast-overflow 加泄漏检查均
  184 个分组通过（原 176 + 新 8），包含 nonce/JWS 重新生成次数、错误身份、输出、重试上限及实际分配清理。
  HTTP 为受控边界；真实 Future/传输错误的交付另由第十四批探针覆盖，不把本测试冒充真实 CA。
- API/参考/生成/发布检查通过。私钥写出提交 `74473142` 的独立邮件 `40338441424` 已成功，
  其 run `40338449568` 仍在执行。前一 `1517d655` run `40333596756` 当时 72 项成功、1 项执行中。
  两个 run 均不提前记为整轮成功，也不替代本批新提交的远端验证。

证据：`acme-flow-read-retry-proof-before.log`、`acme-flow-read-retry-before-final.log/json`、
`acme-flow-read-retry-windows-final.log`、`acme-flow-read-retry-linux-final.log`、
`acme-flow-read-retry-tcc-final.log`、`acme-flow-read-retry-tcc-all-final.log`、
`acme-flow-read-retry-sanitize-final.log`。

源码复核继续：flow 已读到初始化/账户注册段；EAB 临时 MAC、resolver 的 IPv6/端口拆分、
Retry-After 数值和总预算传播仍需分别验证。RSA 导入语义及六个 provider 源文件未结束验收。

### 第十八批：ACME resolver IPv6、端口与查询地址保真

- 原拆分按最后一个冒号和 atol 处理，`::1` 被改成地址 `:`、端口 1，
  部分裸 IPv6/带 scope 的地址也被破坏；非法端口的数字前缀则会实际发出查询。
  37 个端点向量中旧实现失败 29 项；独立 UDP 对端还复现 `:<port>junk` 得到有效 TXT，
  确认回归触及真实网络，不能只靠返回码判断。
- 复用 Core 的严格数字端点解析和地址格式化；裸 IPv4/IPv6 默认 53，显式 IPv6 端口用方括号，
  范围为 1–65535。错误输入或容量不足返回失败并清空输出，不接受端口前缀、主机名或截断地址。
  传播流程在创建 UDP 工作前跳过无效项；诊断分配失败保留 MEMORY 并停止后续 resolver。
- 原默认 flow 故障套件增加端点向量、精确/不足容量、参数、传给 DNS 的实际地址身份、
  无效项跳过和解析诊断 OOM。Windows/Linux GCC、TCC 及 Clang sanitizer 加泄漏检查各
  186 个分组通过。合法数字端点的转换还核对零新增 Core 分配。
- 既有独立 dns-lifecycle 分组新增真实传播入口，Windows GCC、Linux GCC 和 Clang sanitizer
  分别运行原八项 UDP 场景及四项端点场景：IPv4、非法端口前缀、IPv6 压缩和完整写法。
  无效端点不向对端发送数据；有效端点实际收到并解析 TXT，借用引擎保持运行，UDP 对象与分配归零。
  本机三组均无 IPv6 skip；其他环境仅在地址族/IPv6 回环不可用时明确跳过两项 IPv6 对端测试，
  不忽略权限、编译或协议失败。37 项解析向量不因网络环境跳过。
- 公共配置说明同步；API/参考/生成/发布门禁通过。先前 `1517d655` run `40333596756`
  最终 73/73 success，独立邮件 `40333586361` 成功；当前 resolver 变更仍需自己的 CI。

证据：`acme-flow-resolver-before.log`、`acme-flow-resolver-wire-before.log`、
`acme-flow-resolver-tcc-final.log`、`acme-flow-resolver-tcc-all-final.log`、
`acme-flow-resolver-windows-final.log`、`acme-flow-resolver-linux-final.log`、
`acme-flow-resolver-sanitize-final.log`、`acme-flow-resolver-wire-windows-final.log`、
`acme-flow-resolver-wire-sanitize-final.log`。

### 第十九批：ACME 产品聚合入口补齐组件探针

- 复核实际 CI 命令发现：先前新增的 HTTP Future、私钥导入/导出擦除及 Obtain 账户探针
  已在组件清单注册并完成本地验证，但 `xacme_tests` 没有收集依赖资产，远端产品入口会漏跑。
  先前“默认 single_tests”仅表示对应组件入口，不足以证明产品聚合 CI 已运行这些探针；
  既有绿色 CI 和历史覆盖报告不能被用作这四个新增探针的远端执行证据。
- 产品聚合节点通过既有 collect_dependency_assets 收集同产品闭包的 single_tests，
  同时纳入此前遗漏的产品单头消费测试；保持模块所有权、去重及平台过滤，不收集 Core 测试或例子。
  Windows 实际运行 10 个、Linux 实际运行 9 个单头测试，均通过，后者正确排除 Windows 专属入口。
- 构建器新增真实清单回归：Windows/Linux/macOS 的组件单头必须包含在产品入口中，
  且每个入口都能解析 owner 与模块选择。修复前在三个平台选择上均失败；修复后全部 42 个
  构建脚本测试通过。生成文件与发布成熟度检查通过，未修改 workflow 或放宽测试。
- 私钥导出提交 `74473142` 的 run `40338449568` 已最终 73/73 success；
  该结果仍受上述旧聚合清单边界限制。resolver run `40346675809` 尚未完成。

证据：`acme-aggregate-windows-final.log`、`acme-aggregate-linux-final.log`，
以及 tools/test_build.py 中真实产品清单的三平台回归。

### 第二十批：EAB 长 HMAC 密钥与临时副本清理

- 实际开户入口的旧实现固定 Mac[64]：1/11/32/63/64 字节密钥成功后未清零，
  65/80/128/129/512/513/4096 字节密钥则在签名前被拒绝。带/不带填充共 24 个
  正常向量全部复现失败。HMAC 分组长度不是密钥长度上限；Core 的真实 HMAC 已支持长密钥。
- 先用 Core 解码器验证输入并查询长度，常见长度保留局部 64 字节路径，长密钥只增加一个
  精确容量的临时分配；JWK/JWS/载荷构造共用清理出口，成功、OOM、格式失败都清零整个局部区，
  堆副本在真正 free 前清零。不修改调用方借用文本。长度查询增加一次输入校验扫描，
  不声称零成本；测得 32 字节路径的该构造窗口仍为 15 次 Core 分配，129 字节路径为 16 次。
- 已有默认 flow 故障入口新增 74 个场景：24 个长度/填充向量、31 个真实分配失败位置及恢复、
  无效输入、零字节内容、Core 原有部分填充兼容及未启用 EAB 的兼容控制。
  另有真实解码器诊断 OOM：注入在交付前结束，仍必须保留同一 MEMORY 原因，不能重写成参数错误。
  每个失败都阻止开户 POST，借用配置保持不变，实际分配余额归零。
- 探针只在真实 SecureZero/free 的有效存储期读取字节；跨返回仅保存整数地址和待清理状态，
  不读取已结束生命周期的栈对象。核对解码区全容量清零、局部区清零调用及未清零堆释放。
  全部 187 个 flow 分组分别通过 Windows/Linux GCC、Linux/TCC 与 Clang
  ASan/UBSan/float-cast-overflow 加泄漏检查。Windows/Linux 完整产品聚合测试也通过；
  最新生成的产品实现/声明单头及组件探针已再次运行。
- 独立 Python HMAC 与 cryptography P-256 对 Windows/TCC/sanitizer 各 24 份真实构造报文
  验证外层签名、相同账户 JWK、HS256/kid/url 和无 nonce 的 EAB 保护字段。
  独立 TLS 模拟 CA 通过原短密钥、65 字节无填充、4096 字节带填充三条完整流程，
  在 Windows GCC、Linux GCC 和 Linux Clang sanitizer 各执行一轮，包含签发、撤销、换钥、
  store 与 Obtain 证书/私钥配对检查。新增两种长密钥流程并入既有 mock 测试，不修改 workflow。
- 公开说明、生成文件、API/参考与发布门禁同步；未引入新的密钥最小长度策略。
  EAB 标识符的其他协议约束、RSA 导入、flow 后半段与 provider 源码仍需继续，xacme 尚未整库验收。
  只读重试提交 `bddc8353` 的 run `40341378249` 已 73/73 success；
  resolver run `40346675809` 尚在运行，两者仍使用第十九批之前的聚合边界。

证据：`acme-eab-before.log`、`acme-eab-windows-final.log`、
`acme-eab-flow-windows-final.log`、`acme-eab-flow-linux-final.log`、
`acme-eab-tcc-final.log`、`acme-eab-sanitize-final.log`、
`acme-eab-aggregate-windows-final.log`、`acme-eab-aggregate-linux-final.log`、
`acme-eab-single-windows-final.log`、`acme-eab-single-linux-final.log`、
`acme-eab-independent-final.log`、`acme-eab-mock-windows-final.log`、
`acme-eab-mock-linux-final.log`、`acme-eab-mock-sanitize-final.log`。

### 第二十一批：换钥 MEMORY 终止与持久化私钥清理

- 同一 flow 的换钥只读对账仍在带 UNCERTAIN 的 MEMORY 后继续查询；外层换钥写入失败
  也会自动对账。旧实现可以吞掉一次 MEMORY、切换内存钥并返回成功，持续 MEMORY 则查询三次。
  持久化导出的新私钥直接 free，成功及存储失败都有未清零退休。18 个新增场景在旧实现失败 8 项。
- 初始只读对账和写后自动对账均将 MEMORY 视为立即终止，保留同一不可变原因，不取下一 nonce、
  不额外查询或重复写入。普通 I/O/超时保留已有有限恢复，写后对账只在证实成功时切换账户钥。
  调用方显式使用保留的新钥仍可恢复，包括远端已经生效的 MEMORY 情形。
- 持久化临时私钥在真正 Core free 前清零，保存成功、I/O、MEMORY、唯一输出分配失败及恢复均覆盖。
  持久化失败仍保留已生效的新账户钥；重试只对账和重存，不再次换钥。探针读取有效存储期的真实字节，
  并核对原错误身份、nonce/查询/写入/导出/保存计数、借用 PEM 不变及实际分配归零。
- 新矩阵并入已有默认 flow 单头入口，完整 205 个分组在 Windows/Linux GCC、TCC 与
  Clang ASan/UBSan/float-cast-overflow 加泄漏检查均通过。Windows/Linux 产品聚合入口分别
  实际运行 38/37 个测试（二者均含 28 个模块化入口；单头为 10/9），离线套件通过。
- 独立 TLS 模拟 CA 的四项原换钥回归在 Windows GCC、Linux GCC、Linux Clang sanitizer
  分别通过：已执行但响应丢失、执行前断流、存储失败的部分成功、badNonce 外层重新签名。
  检查实际远端换钥次数、旧钥保留、显式对账、证书与持久化配对，不把受控 MEMORY 边界冒称真实网络 OOM。
  公开契约、生成实现/声明头、API/参考和发布检查通过。
- resolver 提交 `e3102af0` 的 run `40346675809` 已最终 73/73 success。
  EAB/聚合提交 `92571a88` 的 run `40352803330` 当时 66 项成功、7 项运行中，未发现失败；
  独立邮件 `40352793835` 成功。已下载并核对成功 Windows job `121677128146` 的实际日志：
  五个此前聚合遗漏的探针/消费入口均执行，且 EAB 的 74 场景通过；不再仅根据清单推断执行。

证据：`acme-rollover-before.log`、`acme-rollover-windows-final.log`、
`acme-rollover-linux-final.log`、`acme-rollover-tcc-final.log`、`acme-rollover-sanitize-final.log`、
`acme-rollover-aggregate-windows-final.log`、`acme-rollover-aggregate-linux-final.log`、
`acme-rollover-mock-windows-final.log`、`acme-rollover-mock-linux-final.log`、
`acme-rollover-mock-sanitize-final.log`、`ci-eab-aggregate-windows-121677128146.log`。

后续已复现但未在本批修复：

- 当前订单要求授权数量等于去重域名数量。独立派生的实际公开 flow 夹具中，零授权、双授权均在
  触及 DNS 前被拒绝，单授权控制成功；[RFC 8555 §7.1.3](https://datatracker.ietf.org/doc/html/rfc8555#section-7.1.3)
  明确允许服务端策略决定授权数量，不保证一一对应。后续须在现有容量边界内支持不同数量，
  不可用简单的一域一授权映射代替协议要求。
- 若新账户钥就是当前客户端已知的证书钥，换钥仍成功、执行对账并替换内存钥；
  [RFC 8555 §11.1](https://datatracker.ietf.org/doc/html/rfc8555#section-11.1) 要求拒绝已知用于其他用途的账户新钥。
  已有 Issue 侧的反向检查不覆盖换钥入口。证据 `acme-rollover-known-cert-key-before.log`。
- 总预算传播、Retry-After 数值、EAB 标识符、RSA 导入及其余 provider 复核尚未结束；
  Route53 C 主文件已读，签名/XML 依赖和异常原因链仍需结合真实失败验证。

授权数量证据：`acme-flow-authorizations-cardinality-before.log/json`。

### 第二十二批：账户换钥拒绝已知证书钥

- 上一批派生公开入口探针复现：当前证书钥被当作账户新钥接受，发起对账并替换账户钥。
  默认 flow 新增六项公钥身份与恢复回归，旧实现失败三项：PKCS#8、同钥 SEC1，以及诊断 OOM
  已经进入签名阶段。三个正常控制为不同 EC 钥、RSA 证书钥、未配置证书钥。
- 换钥读入新钥后、任何 JWS/网络对账前，按公钥身份拒绝与已配置 EC 证书钥相同的新钥。
  走原有统一清理擦除临时新钥，保留原账户钥与证书钥；参数诊断分配失败保留 MEMORY。
  不依据 PEM 文本比较，故不同私钥封装不能绕过。拒绝后同客户端可用独立新钥显式恢复。
- Windows/Linux GCC、TCC、Clang ASan/UBSan/float-cast-overflow 加泄漏检查的完整
  flow 套件各 211 个分组通过；每项同时检查签名/对账/写入计数、钥状态、诊断、恢复与真实分配余额。
  公开契约与生成头同步。产品聚合及独立 TLS 将随下一授权数量修复在最终源码上再验证，
  此处不把上一批 205 分组或旧远端结果当作本批完整产品验证。

证据：`acme-key-separation-before.log`、`acme-key-separation-windows-final.log`、
`acme-key-separation-linux-final.log`、`acme-key-separation-tcc-final.log`、
`acme-key-separation-sanitize-final.log`。

### 第二十三批：授权数量遵从服务端策略与完整 TXT 收尾

- 19 个新增公开 Issue 场景在旧数量检查上失败 16 项：零/双授权、16/17 边界、33/65 项及
  已有效授权均不能按服务端策略处理；对后段失败的检查还核对此前 TXT 的实际拥有权。
  单授权与缺失/错误类型列表的三个控制保持原行为。
- 删除授权数量等于订单域名数量的错误前提；常见 16 项以内沿用局部工作表，较大列表使用一次
  经 SIZE_MAX 乘法检查的分配。工作表直接持有全部已确认 TXT 到统一 Done，不再向另一个固定
  16 项表转移，也不因表满提前删除。CSR/SAN 仍由调用方的去重订单域名构造。
- 新矩阵确认 0/1/2/16/17/33/65 项、全有效状态、唯一工作表分配失败、最后一次查询 I/O/MEMORY、
  Add MEMORY、后段 URL 类型/NUL/空串、挑战触发及 finalize 失败。比较实际查询/触发/添加/删除
  次数、原错误身份、失败产物、同客户端恢复及真实分配余额；要求触发前所有 TXT 并存。
  17 项后的错误仍分别清理 16 或 17 个已拥有值。普通工作表路径不新增该分配，大列表仅一次。
- Windows/Linux 产品聚合入口通过，分别执行 38/37 个测试；两平台 GCC、Linux/TCC 和 Clang
  ASan/UBSan/float-cast-overflow 加泄漏检查的完整 flow 套件各 230 个分组通过，
  同时覆盖上一批账户/证书钥隔离的六项回归。生成实现/声明头、API/参考、feature 与发布门禁通过。
- 独立 TLS 模拟 CA 新增零/双授权策略，同时验证 CSR SAN、实际授权完成和无残留 TXT。
  双授权显式启用流程诊断，必须有覆盖授权数量的真实递归 DNS 查询，且不得出现传播预算耗尽；
  不是依靠咨询性超时放行。Windows GCC、Linux GCC、Linux Clang sanitizer 的整个 mock 套件
  各 18 项通过（含多个 subTest），覆盖原 EAB/换钥/备用链/异常/持久化场景。
  65 项及分配故障使用真实 Core/crypto 与受控 HTTP/provider 边界，不声称 65 项真实 UDP 已验证。
- 真实双授权最初在三组环境均触发 180 秒测试超时。追踪发现既有模拟 DNS 把每条 TXT 的
  RDLENGTH 固定写成零；单授权旧用例同样未通过递归传播，只因其为咨询性门而最终成功。
  新字节级单测在旧报文失败 0!=44；修正每条 RDATA 及 UTF-8 字节长度，验证不同长度、空值、
  多条记录、压缩属主、查询回放和无尾随字节。保留原超时失败日志，没有提高超时或放宽断言。
  历史 mock 的签名/权威 TXT/证书/存储验证仍有效，但不能据其旧成功声称递归传播已命中；
  历史覆盖率报告也不覆盖本批新控制流。独立 DNS 生命周期/对端测试使用各自报文，不据此否定其证据。
- EAB/聚合提交 `92571a88` 的 run `40352803330` 已最终 73/73 success；换钥提交 `38a9d1f3`
  的独立邮件 `40356901116` 成功，其 CI `40356913026` 尚未完成。
  其中 macOS ARM64 coroutine job `121691731347` 在执行任何步骤前取消，steps=[]、日志 404，
  单任务重跑请求被拒；保留未验证状态，新提交 CI 必须再确认该平台，不能记为代码测试失败或通过。

证据：`acme-authorization-policy-before.log`、`acme-authorization-policy-windows-probe.log`、
`acme-authorization-policy-aggregate-windows-final.log`、`acme-authorization-policy-aggregate-linux-final.log`、
`acme-authorization-policy-tcc-final.log`、`acme-authorization-policy-sanitize-final.log`、
`acme-authorization-policy-mock-windows-fixture-before.log`、
`acme-authorization-policy-mock-linux-fixture-before.log`、
`acme-authorization-policy-mock-sanitize-fixture-before.log`、`acme-mock-dns-rdata-before.log`、
`acme-authorization-policy-full-mock-windows-final.log`、
`acme-authorization-policy-full-mock-linux-final.log`、`acme-authorization-policy-full-mock-sanitize-final.log`。

账户/证书钥复用和授权数量两项已复现问题现已修复。xacme 的总预算、Retry-After、EAB 标识符、
RSA 导入及 provider/签名/XML 复核继续；大同名 RRset 的递归探测输出容量也须单独验证。

### 第二十四批：完整应答 TXT 匹配与结构边界

- 旧传播门和 Wait 只比较前四条文本结果；第五条会漏报，空分段及前置的合法长 TXT
  也会阻断匹配。真实 UDP 的十个旧实现对照场景失败七项，还包括 NUL 前缀误判、
  TC 截断和容量满后的畸形尾部被误报命中。三个控制保持正确。
- 新内部入口共用原 UDP 生命周期，逐段按字节比较全部 answer TXT，无固定记录窗口、
  无匹配临时分配。完整结构验证结束后才发布 found/计数。文本查询保留容量契约，
  拒绝输出不可表示的长度与内嵌 NUL；无关二进制或较长 TXT 不阻断精确匹配。
- 按 [RFC 1035 §3.3.14](https://datatracker.ietf.org/doc/html/rfc1035#section-3.3.14)
  接受空 character-string，拒绝不含分段的零 RDATA。检查全部 question/answer/authority/
  additional 边界、TXT 分段及尾随数据；压缩目标必须位于指针之前，检查链与展开长度，
  拒绝 TC 截断。authority/additional TXT 不算 answer 命中。修正原本自指的“合法”分段
  测试和两个 fuzz 种子，保留另一个真实 question 压缩控制。
- 15 项文本解析矩阵在旧实现失败 13 项；修复后与另 18 项精确匹配矩阵全部通过。
  覆盖第 5/65 条、空分段、二进制/NUL、256 字节前置值、空期望、参数/ID/负答复和
  后段失败。在真实分配故障阈值零下检查不分配、不发布失败命中/部分计数及容量哨兵。
- 实际 Core buffer/UDP/Future 的六个故障位置对 collect/contains 各执行控制/OOM，
  共 24 组，检查原 MEMORY 身份、终止重发、同引擎恢复及真实分配归零。
  Windows/Linux GCC、Linux/TCC 和 Clang ASan/UBSan/float-cast-overflow 加泄漏检查通过；
  产品聚合分别实际运行 38/37/37 个测试，完整 flow 各 230 个分组通过。
- 独立 UDP 增至 21 个生命周期/endpoint 场景，Windows GCC、Linux GCC 和 Linux Clang
  sanitizer 全部通过，两种 IPv6 表达实际执行。正答必须收到 Flow、Wait 两次实际查询，
  反例一次，畸形 resolver 零次；查询后 UDP 对象归零，借用引擎保持运行。
  完整模拟 CA 在三组环境各 18 项通过，授权策略扩为 0/2/5，核对每个授权实际递归命中、
  无残留 TXT、无咨询性传播超时。五条挑战和独立 UDP 夹具均不超过 512 字节。
- 匹配失败原子性加入 fuzz 契约；Clang ASan/UBSan libFuzzer 实际执行 200000 轮通过。
  生成一致性、feature、公共符号登记、参考生成与发布门禁通过；公开 API 数量不变。
  对照发现 Linux 生成头保留了一行上一批旧注释；同步公开源注释并重新验证生成消费，
  不将注释不同的旧生成输入冒称逐字节相同。
- 65 条仅为直接解析测试。传输仍无 EDNS/TCP 回退；截断恢复、属主/问题/CNAME 语义绑定、
  绝对预算与其他 ACME 源码尚待复核。本批不代表完整 DNS 或 xacme 整库验收。
- 上批 e84969f6 CI 40366086935 最终 71 success、1 failure、1 cancelled，独立邮件
  40366078120 成功。日志显示 macOS Apple Clang 的首个 TCP 拒绝对端连接断言失败；
  xhttp-server 的 test_http_server_lifecycle 运行约一小时后取消，仍继续追查，不能记为通过。
  更早换钥 CI 40356913026 最终 72 success、1 cancelled，仍无该次 macOS ARM64 通过证据。

证据：`acme-dns-parser-before.log`、`acme-dns-wire-before.log`、
`acme-dns-match-aggregate-{windows,linux,sanitize}-final.log`、
`acme-dns-match-{dns,flow}-tcc-final.log`、`acme-dns-match-wire-{windows,linux}-counted-final.log`、
`acme-dns-match-wire-sanitize-final.log`、`acme-dns-match-mock-{windows,linux,sanitize}-final.log`、
`acme-dns-match-libfuzzer-final.log`、`ci-e849-macos-failed.log`、`ci-e849-xhttp-server-cancelled.log`。

### 第二十五批：TCP 拒绝回归的握手/回调同步与 CI 卡死定位

- 上批 macOS job 121724053968 失败在 `TCP first rejected peer failed`。首个 Accept
  立即返回 false，关闭动作可早于阻塞 connect 的返回；旧日志没有系统错误码，不能断言
  这次 CI 唯一根因就是 reset。未据此修改 Socket/Listener 行为或放宽结果断言。
- 首个 Accept 使用真实原子门控，客户端 connect 返回后先确认回调已经进入并仍被阻塞，
  再放行拒绝并等待其返回。保留两次拒绝、无 Open/Close 泄漏、跨线程 SendRef 所有权、
  Listener Close 顺序和终态计数等全部原断言；连接失败补充 kind/模块码/系统码诊断。
- 当前最终源码在 Windows GCC select/IOCP 各 33 轮、Linux GCC/TCC/Clang ASan/UBSan
  加泄漏检查各 33 轮通过。macOS 实机仍须新 CI 验证，不能把本地通过当作 macOS 通过。
- 另一 cancelled job 121724053896 有真实执行记录：`test_http_server_lifecycle`
  运行约一小时后 job 被取消。Linux 原始源码第 33 轮自然复现卡死，并在终止本次测试进程前
  抓取 gdb 全线程栈：主线程位于 testHttpServerShutdownOrderCleanup，经 xrtHttpConnDestroy、
  __xrtHttpServerCloseConnections、xrtHttpServerAbort 进入；唯一 Worker 已回到 select。
  清理回调等待 ContinueCleanup，而主线程只能在 Abort 返回后设置它，构成确定的自等待。
- 源码证实 Abort 临时持有 Connection 引用，其最后释放可能转到调用线程；需继续结合
  Shutdown 的内部运行时/适配器清理契约处理此路径和仍持有公开 Connection 引用的情形。
  此处只记录真实卡死证据，未提高 timeout、跳过生命周期测试或提前声称整个 CI 已修好。

证据：`tcp-reject-gate-windows-build.log`、`tcp-reject-gate-windows-repeat-final.log`、
`tcp-reject-gate-{clang,tcc}-linux-final.log`、`tcp-reject-gate-gcc-linux-final-refresh.log`、
`http-server-lifecycle-repeat-before.log`、`ci-e849-xhttp-server-cancelled.log`。

### 第二十六批：HTTP 运行时适配器在 Shutdown 前退休

- CI 的一小时取消在 Linux 原始生命周期测试第 33 轮自然复现；gdb 栈证明 Abort 的临时
  Connection 引用成为最后拥有者，主线程在 Destroy 的适配器清理中等待自己的后续信号。
  进一步增加保留公开 Connection 引用的场景，稳定复现三个实际契约缺口：普通关闭不启动
  适配器清理，Upgrade 成功和中止都在内部上下文退休前发布 Shutdown。
- 在正常关闭及 Upgrade 运行时退出处、归还 RuntimeConnections 前清理剩余适配器。
  Upgrade 完成回调先返回；Close 或升级层已经 Take 的上下文保持其拥有权。
  helper 先清空 Data/Release 再调用，最终析构保留失败/未启动实例的兜底。
  公开引用可以保留关闭/升级后的查询对象，不再把内部退休推迟到最后公开引用释放。
- 默认生命周期测试同时覆盖保留/不保留公开引用，两条路径均要求清理在所属 Worker 上执行，
  清理门阻塞期间 Server 保持 ABORTING，放行后唯一 Shutdown/清理完成。
  Upgrade 的 Reply/Raw/Response/IOCP 共用回归确认回调内上下文仍有效、Shutdown 前已退休，
  最后释放公开引用不重复清理；原有关闭、事件、统计和引擎退休断言全部保留。
- 最终 Windows/Linux GCC 的完整 HTTP Server 套件分别实际执行 115/109 个测试程序，
  各包含 30 个单头入口。生命周期、升级成功、升级中止三个重点回归在 Windows/Linux GCC、
  Linux Clang ASan/UBSan/float-cast-overflow 加泄漏检查、Linux/TCC 各通过 33 轮。
  此处 Clang/TCC 指三项重点回归，完整套件的剩余问题见下一批记录。
- 依赖该生命周期的 WebSocket Server 最终 GCC 套件在 Windows/Linux 分别运行 38/31 个
  测试程序，均含 13 个单头入口，覆盖 HTTP/TLS/Deflate、Future/回调及重连交接。
  HTTP Server 的完整 31 个自有源文件分别生成双平台 static/shared 产品包，四个实际
  消费者链接并运行通过；包消费验证接口/链接，不冒称额外的运行时回调验证。
- 同步公开契约和生成实现/声明头；公共符号/参考/发布检查通过。
  1197 个 Core、ACME、HTTP、WebSocket 相关源码/头文件在 Windows/WSL 快照中逐项核对，
  仅归一化 LF/CRLF，最终无差异。两个 Server 产品仍不满足整库源码/平台验收条件。
- 上批 e84969f6 的 macOS ARM64 coroutine job 121724053637 已确认 success，补充了较早
  cancelled run 所缺的后续平台证据；这不代表本批改动已在 macOS 通过，仍须新的 CI。

证据：`http-server-lifecycle-repeat-before.log`、`http-server-adapter-retained-before.log`、
`http-server-adapter-upgrade-before.log`、`http-server-adapter-upgrade-failure-before.log`、
`xhttp-server-synchronized-{windows,gcc-linux}-final.log`、`server-runtime-final-focused-linux.log`、
`xws-server-synchronized-{windows,gcc-linux}-final.log`、
`http-server-synchronized-package-{windows,linux}-{static,shared}-final.log`、
`server-runtime-final-input-audit.json`。

### 第二十七批：显式字节视图与未闭合的整套验证

- 全量 Clang 21 sanitizer 发现文件正文配置初始化、Server 地址输出、WebSocket 配置初始化
  的未对齐 memcpy 参数。最小 C 对照中，typed 参数报错、显式 void 字节视图通过。
  [Clang 的对齐规则说明](https://lists.llvm.org/pipermail/llvm-dev/2016-January/094012.html)
  明确区分隐式库函数转换与显式字节视图，未把此行为记成编译器误报。
- 在已检查范围并承诺快照/未对齐存储的入口，共 19 处生产拷贝显式使用字节视图：文件正文
  配置、Server 配置/事件/地址/统计、Connection 地址/统计，以及 WebSocket 配置/握手和
  两个内部配置快照 helper。保持公开类型、范围/重叠检查、输出与分配行为不变。
  握手夹具自身的未对齐快照读取也改为字节视图，保留全部哨兵和失败原子性断言。
- 最终专用 WebSocket 握手 Clang ASan/UBSan/float-cast-overflow 加泄漏检查通过；普通与
  失效范围、配置/握手未对齐输入输出的原有效回归继续执行。模块化/生成单头、双平台完整
  GCC 套件通过。上述验收只覆盖此次明确修改的入口，不表示所有产品的字节视图已审计。
- 完整 HTTP Server sanitizer 仍停在 Reply 压缩夹具的未对齐 typed memcpy；其余 Router、
  静态服务等快照入口/夹具需继续统一复核。完整 TCC 在文件正文 OOM 夹具的 GNU
  __ATOMIC_RELAXED 内建上编译失败；三项生命周期 TCC 回归通过不能替代这项缺失。
  WebSocket 的完整 sanitizer 在本轮发现握手链中的多个存量拷贝后尚未全量完成；最终握手
  已专门复验，不据其推断余下测试通过。保留所有失败日志，不禁用 alignment、不跳过默认
  回归、不删除有效断言、不提高超时换取成功。

证据：`http-config-memcpy-compiler-probe.log`、
`http-server-adapter-*-alignment-before.log`、`xhttp-server-byteview-sanitize-linux-final.log`、
`xhttp-server-byteview-tcc-linux-final.log`、`websocket-handshake-byteview-sanitize-final.log`。

### 第二十八批：关闭 Server 全套 sanitizer/TCC 缺口

- 以 b246cdda 为输入先运行清单选定的全部 Linux 模块化 Clang sanitizer 测试，
  HTTP Server 79 个中 13 个失败、WebSocket Server 18 个中 1 个失败，保留逐程序日志。
  未停止在首个错误；对照矩阵共 97 个程序，未排除默认资产。
- 按公开未对齐/字节快照契约逐点复核，新增 75 处生产拷贝显式字节视图：Server 两库
  67 处，依赖 xhttp 的正文流与前置条件 8 处。覆盖路由计数/匹配/方法、压缩输出、Mux、
  事件、TLS/ALPN、升级中止、SSE、静态路径/计划/响应及配置。仅在相应输入/输出参数
  显式使用 void/const void；保留本就对齐的私有对象及字符拷贝，不批量改写所有 memcpy。
  五个夹具的另 16 处未对齐快照改为字节视图，保留范围/重叠、哨兵、失败原子性和所有权断言。
- 第一轮修复后 WebSocket 18/18 通过；HTTP 还暴露共享正文流/前置条件快照及一个独立
  Future 发布假设。补齐依赖入口并按下一批修正测试同步后，最终 97/97 模块化矩阵通过。
  旧矩阵失败包括夹具本身的拷贝，未将每个测试失败冒称独立生产漏洞。
- 文件正文 OOM 夹具改用 Core 的无分配 xatomic32 API，移除专用 Windows/GNU 原子分支，
  使用公开静态初始化宏。有效原子地址/顺序路径已核对不分配；继续在真实 allocator 回调中
  执行全部原有计数、平衡、MEMORY 与 256 个 adopt 失败位置，不降低 OOM 强度。
- 最终完整套件在 Windows GCC 实际运行 HTTP Server 115、WebSocket Server 38 个程序；
  Linux GCC、Clang ASan/UBSan/float-cast-overflow 加泄漏检查、TCC 各执行 109/31 个。
  每组 HTTP 含 30 个、WebSocket 含 13 个单头入口；全部通过。本批关闭第二十七批所列
  完整 sanitizer/TCC 缺口，没有禁用 alignment、删除有效断言或跳过默认测试。
- 另对依赖 xhttp 的前置条件、正文流、OOM 与并发四个模块套件分别执行上述四组环境，
  每组实际运行六个入口，含两个单头，全部通过。这只覆盖本次触及的依赖模块，不代表
  拥有 124 个源文件的 xhttp 已整库验收。
- xhttp、xhttp-server、xws-server 在 Windows/Linux 各生成 static/shared 包，十二个
  实际消费者全部链接、运行通过；每个包只拥有对应库源码，并链接一份 Core。
  三产品生成实现/声明、feature、公开符号登记、参考生成和发布门禁全部通过。
  1197 个 Core/ACME/HTTP/WS 源码/头输入归一化 LF 后 Windows/WSL 无差异。
  仍须后续源码、示例、裁剪及平台验收，两个 Server 产品未记为整库完成。

证据：`server-byteview-matrix-{before,after,final}.json`、
`server-byteview-change-counts.json`、`server-byteview-complete-{windows,linux}.json`、
`{xhttp-server,xws-server}-byteview-complete-windows-gcc.log`、
`{xhttp-server,xws-server}-byteview-complete-linux-{gcc,clang,tcc}.log`、
`server-byteview-dependency-*.log`、`server-byteview-packages-{windows,linux}.json`、
`server-byteview-check-*.log`、`server-byteview-final-input-audit.json`。

### 第二十九批：独立关闭 Future 的真实发布门控

- 全量运行自然出现 `HTTP server close wait did not resolve after shutdown`；旧夹具在第一个
  预先注册的 Future 就绪后，对 Shutdown 内新注册的另一个 Future 做零等待并要求已完成。
  实际 producer 按链表逐一发布，两次独立发布之间存在合法窗口，Shutdown 已返回并不让
  它们变成一次原子广播。未修改生产 Future 实现或放宽其完成契约。
- 使用真实 Core FutureWatch 阻塞第一个通知；在 producer 尚未推进第二个等待时，检查
  第一项 RESOLVED、Server CLOSED、唯一 Shutdown 和第二项仍 PENDING。放行并汇合通知后，
  独立等待第二项完成，检查 Watch 唯一释放及 Drain 线程正常退出。Drain 允许在调用线程
  完成，故单独创建 producer 线程，使控制线程始终可以放行门。保留独立取消、回调内
  待定、迟注册立即完成、析构与 Engine 退休的原有检查，原截止时间不变。
- 派生旧假设对照只把门控期间第二项的预期恢复为旧零等待就绪，稳定在该断言失败；
  这是旧假设的受控对照，不是未修改旧 HEAD 的运行证据。
  最终夹具在 Windows GCC select/IOCP 各 33 轮，Linux GCC/Clang sanitizer/TCC 各 33 轮
  全部通过，并参加上一批完整默认套件。

证据：`xhttp-server-byteview-after-test_http_server_wait_future.log`、
`http-server-wait-old-assumption-control.log`、`http-server-wait-publication-repeat-*.log`、
`http-server-wait-publication-iocp-windows-repeat.log`。

本轮仍为 2/21 产品完成，原范围不变。继续追查 Windows SMTP 覆盖率失败，再恢复 ACME
绝对预算、Retry-After、EAB 标识符、RSA 导入与 provider 复核及其他产品的集中检查。
### 第三十批：Windows SMTP 覆盖率失败的可诊断性

- b246cdda 的 CI 40384540301 中，Windows xsmtp coverage job 121789214803 在
  `test_smtp_upload_tls_fault_runtime` 的 Open 断言失败。普通 xsmtp job、xhttp-server、
  macOS Apple Clang 和 ARM64 coroutine 已确认 success；记录时 CI 为 71 success、
  1 failure、1 in_progress，最后 Windows 主任务仍在继续，不能称整条流水线全绿。
  独立邮件 run 40384529641 success。失败日志只给出 Open 断言，没有 bdat/mode 或底层错误。
- 在故障夹具记录六个已完成的准备阶段：Accept、Socket 配置、TLS、banner、EHLO、能力回复。
  Open 失败时输出具体 DATA/BDAT 与故障模式、阶段、线程状态、不可变错误的 kind/code/
  system/domain/operation/message 以及原截止时间是否已过。仅失败路径输出，保持六组真实
  部分发送、错误保真、客户端销毁前异常关闭、原 15 秒总预算及 3 秒故障预算。
- 本地 Windows GCC 使用与 CI 相同的 O0/coverage/原子 profile 编译选项，在初步诊断夹具
  连续 17 次执行六类故障场景均通过，未复现 CI 的 Open 失败。最终完整六阶段夹具重新编译，
  清除本夹具因改源码留下的旧校验和 profile 后，六组再次通过，无 profile 冲突告警。
  本机 GCC 16.2 与 GitHub MSYS2 编译器并非同一环境，不能据此宣布修复远端故障。
- 派生对照仅把客户端 Open 的截止时间设为已过，保留服务端真实线程；断言稳定失败，
  诊断实际显示 stage=0、TIMEOUT、xrt.mail 和 dial 前超时消息，验证错误采集路径。
  该对照不等同于 CI 根因复现。当前只补充诊断，后续 CI 仍须追查，不增加超时或跳过夹具。

证据：`ci-b246-xsmtp-windows-failed.log`、`smtp-tls-fault-coverage-windows-probe.log`、
`smtp-tls-fault-coverage-windows-repeat.log`、
`smtp-tls-fault-coverage-windows-diagnostic-complete.log`、
`smtp-tls-fault-coverage-windows-diagnostic-clean-final.log`、`smtp-tls-open-diagnostic-control.log`。

### 第三十一批：TCC 并发存储回归兼容

- 本轮完整 TCC 首次执行 20 个模块化程序后，在 `test_store_concurrent.c` 编译停止，
  明确报 `stdatomic.h` 不存在；保留原日志，不把此前单独 TCC fault probe 的通过当作整套通过。
- 夹具改用已经包含在 acme_store 依赖闭包内的 Core xatomic32 和公开静态初始化宏。
  对应 C11 默认操作全部保留 XMEMORY_SEQ_CST，没有改为较弱内存序。
  双写线程、每线程 12 次轮换、至少 50 次读、配对一致性、全部写成功、零读失败与混配
  断言保持原样；原生测试线程创建/汇合逻辑不变，不引入新的产品运行时依赖。
- Linux/TCC 的独立 acme_store 并发回归已实际执行通过。HTTP 交换预算修复的完整 TCC
  套件继续复验，最终整套结果另行记录；这项只关闭已确认的夹具编译兼容缺口。

证据：`acme-http-budget-full-linux-tcc.log`、`acme-store-concurrent-tcc-final.log`。

### 第三十二批：HTTP 交换共用绝对预算与末次拨号拦截

- 旧实现对连接、全部发送、响应读取分别重新打点，安全重试又取得完整超时并无条件退避。
  新增 32 组 TCP/TLS 受控场景，使用实际 Core Future、字节拥有者、取消和分配器，只控制
  时钟与传输提交边界；当前回归对旧 HTTP 源码失败 26 组，六个正常/不限时/MEMORY 控制通过。
  初始化负超时的旧实现对照也在实际资源拒绝断言失败，未只依据源码猜测。
- 每次 Exchange/Once 仅建立一个单调截止时间，连接及等待、全部发送块、响应头/正文、
  关闭定界 TLS END 和全部安全重试共用。退避取剩余预算，耗尽停止提交下一次操作；
  迟创建的 Future 在等待前重新计算剩余时间。到期不交付成功或部分响应。
  已经发布的 MEMORY 原因及取消/Abort 后的原错误保留；只读重试耗尽时以 TIMEOUT 包裹
  首个根因，错误上下文 OOM 保留真实 MEMORY。显式后续操作能在同一对象独立恢复。
- 发送开始标记移至首个实际发送提交之前。连接取得后、发送前耗尽预算会中止已取得流，
  但不误报写入结果未知；已经开始的 POST 仍保持未知结果、不自动重放。覆盖三块发送、
  头已解析后正文超时、末端 MEMORY 和耗尽诊断分配 OOM，清理均要求真实分配归零。
  HTTP Init 的 0 仍选 30 秒默认、-1 不限时；小于 -1 在构造传输资源前拒绝。
- 复核 Core 发现 Dial.Timeout 的 0 表示不限时。新增在配置初始化期间耗尽预算的两项
  回归，初版修复仍实际提交一次 Dial 而失败；最终 TCP/TLS 两条路径在调用 Core 前检查
  剩余量为零并停止，避免把到期转换为不限时。保留初版失败日志，不将其计入最终通过。
- 独立真实对端增加六个场景：TLS 握手与响应累计、TCP 头/正文、只读丢响应重试各有
  正常与耗尽控制。旧实现两项失败：TLS 已超总预算仍正常完成、只读重试发送第二次请求；
  其余四项控制通过。最终三组环境的六项全通过，POST 失败在客户端析构前确认关闭，
  TLS 必须观察到 Abort；只读到期只有一次真实请求，正常重试恰好两次。
- 完整产品套件最终 Windows GCC 运行 38 个程序（10 单头），Linux GCC/Clang sanitizer/
  TCC 各 37 个（9 单头）。默认 TCC 在存储夹具之外又暴露旧 max_align_t 的默认模式
  编译缺口；改为不被解引用的自然对齐 union token，不加语言模式特例或跳过单头。
  最后这项只改夹具存储类型，四组环境均另行重跑完整该夹具，116 个分组全部通过：
  原有 83 个传输组、新增 32 个预算组、一个参数组（含两种负超时 Init 子用例）。
  TCC 另完整重跑全部 37 个入口；GCC/Clang 的其他已通过入口没有改动。
- Windows/Linux GCC、Linux Clang ASan/UBSan 的完整独立 HTTP/TLS、DNS、provider 线缆与
  所有权脚本在末次零拨号拦截前全部通过，699 项输入快照专门绑定这一版。
  最终补丁另在这三组环境重跑预算/lifecycle/ownership 三组：六个预算场景、构造失败扫描、
  原错误保留、实际延后退休/借用引擎，以及单头 40 个精确错误检查均通过。
  未把末次补丁前的全量脚本冒称最终版全量重跑；最终生产路径还经完整产品套件验证。
- 最终 Clang 拥有权专项在该收尾路径自然失败，记录到 123 个存活分配/1184240 字节。
  该路径旧夹具要求解除 pin 后一次 5ms 清理即完成。用真实 Worker 阻塞门确认合法窗口：pin 已释放、
  Worker 仍被持有时，原预算 5ms 必须返回 false，队列仍有一个已擦除非 HTTP 数据的拥有者，
  原错误身份不变；放行 Worker 后才允许全部退休。派生旧立即就绪假设在该门控稳定失败，
  负对照自身仍完成清理，未将它冒称未修改旧 HEAD 的自然复现或已证明自然失败的唯一原因。
  四处最终清理改为重复原 5ms 调用并验证每次剩余拥有权/原错误，外层 5 秒守卫仅防测试
  卡死，未增加产品或每次调用预算。无调用前错误时，只清理本次短等待产生的 TIMEOUT；
  不吞 MEMORY/其他错误。保留分配失败零下的入队/轮询、并发摘链与重排、Obtain 收尾断言。
  完整拥有权程序在 Windows/Linux GCC、Linux Clang sanitizer 各通过，并各重复 33 轮
  12 个真实持有场景（每组 396 场景），全部分配归零；Clang 原失败命令也重新通过。
- 完整模拟 CA 脚本在 Windows/Linux GCC、Linux Clang sanitizer 各 18 项全部通过，
  包含脚本级检查及真实 TLS/JWS/EAB/TXT/grant/store 流程，未将每项都计为独立 CA 会话。
  四个实际 Windows/Linux static/shared 消费者通过，每包 16 个自有源文件且链接一份 Core。
  生成实现/声明、feature、公开符号/参考和发布门禁通过；1198 项最终相关源码/头/驱动
  输入归一化 LF 后 Windows/WSL 无差异。现有历史覆盖率报告未重新绑定到本批。
- a83aa622 的新 CI 已确认 Windows xsmtp coverage、两个 Server 扩展通过，独立邮件
  40393562237 success；PR CI 40393570759 已全部 73/73 success。
  旧 b246cdda CI 最终 72 success/1 failure，唯一失败为 SMTP Open。新一轮未复现，
  诊断补丁不等同根因修复；继续保留该限制及日志，不提高其 15 秒/3 秒预算。
- 本批只关闭单次 HTTP 交换预算。Issue 的总预算还未传到各 HTTP/DNS/provider 阶段，
  传播与外层退避需继续处理；当前 Issue 设置 bIssueDeadline 后无复位，小于 -1 可生成
  NAN，均为下一批待复现问题。同步宿主回调不能用抢占线程方式强行中止，provider Add
  未知结果的记录拥有权及 Remove/引擎退休的独立清理预算仍须保留。Retry-After、EAB
  标识符、RSA 导入、provider/签名/XML 和 DNS 语义绑定等工作继续；仍为 2/21 整库完成。

证据：`acme-http-budget-windows-before.json`、`acme-http-budget-before-provenance.json`、
`acme-http-budget-legacy-arguments-before.log`、`acme-http-budget-zero-dial-before.json`、
`acme-http-budget-wire-windows-before.{json,log}`、`acme-http-budget-full-final-{windows,linux}.json`、
`acme-http-budget-fixture-final-*.log`、`acme-http-budget-full-final-linux-tcc-portable.log`、
`acme-http-budget-mock-final-*.log`、`acme-http-budget-interop-*-full.log`、
`acme-http-budget-interop-initial-inputs.json`、`acme-http-budget-interop-final-*.log`、
`acme-http-budget-packages-final-*.json`、`acme-http-budget-final-check-*.log`、
`acme-http-budget-final-input-audit.json`、`acme-retirement-final-*.json`、
`acme-retirement-old-ready-assumption.log`、`acme-retirement-repeat-*.log`。


### 第 33 批：ACME 覆盖率夹具的有符号时间契约

- 8a23f336 的 PR CI 40405542024 两项 ACME coverage 任务失败，任务分别为
  Linux 121862231060、Windows 121862231101。全部独立 HTTP/TLS/DNS/provider 测试先已通过，
  随后严格覆盖率合并在 ownership_dns_obtain 的 xacme_http.c 画像处拒绝；未降低门槛或跳过画像。
- 退休夹具 rollback_deadline 原参数为 uint64，而 Core __xrtWaitAfter 接收 int64。
  用 8a23f336 的实际 HTTP 源码和同一 GCC 16.2 对照模块/嵌入编译，复现 OnceV 中同一个
  条件表达式的基本块由第 994 行分到 997 行，导致行集合与 CFG 绑定不一致。只把夹具
  声明改为 int64 后两个画像完全一致。新 HTTP 调用链自身也保持相同签名；无需修改合并器。
- 增加真实 Core 时间算术回归：无退休注入及两种退休故障下，-1 始终无限，-2 始终
  为 NAN/ARGUMENT；旧无符号声明在退休注入时会把负数改成 20ms。正数短等待不变。
  Windows GCC 完整拥有权程序通过，并额外重复三轮 12 个真实 worker 持有场景，
  所有资源最终归零。其余平台和新 CI 继续验证；本批仅修改夹具，不包含下一批 Issue 产品修复。

证据：`ci-8a23-acme-coverage-{linux,windows}.log`、`acme-issue-cfg-before.log`、
`acme-issue-cfg-signed-before-source.log`、`acme-retirement-signed-windows.json`、
`acme-retirement-signed-windows-gcc.log`、`acme-retirement-signed-repeat-windows-gcc.log`。


### 第 34 批：Issue 的 HTTP 截止时间、外层退避和预算状态收尾

- Issue 原先只在少数轮询入口检查截止时间，nonce、订单、挑战、finalize 和证书 HTTP
  请求仍可各自取得完整请求预算，外层退避也重新睡满；结束后 bIssueDeadline 未复位，
  可干扰同实例的后续操作。现在内部 HTTP 上可附加本次同步操作的绝对截止时间，
  Exchange/OnceV 都取该时间和请求自身截止时间的较早者；不改公开 ABI，不修改请求配置值。
  Issue 的 POST-as-GET 安全重试、状态轮询和 DNS Add 外层重试按剩余预算退避，耗尽后
  不发起下一阶段。badNonce 的再次请求同样经过统一 HTTP 截止时间。
- 0 和既有 -1 均保持不限时；小于 -1 在创建传输/账户之前报告 ARGUMENT，直接内部 Issue
  也检查。8a23f336 源码对 -2/INT64_MIN 均实际建立客户端并发出三次受控 HTTP 请求；
  最终回归要求这两种输入在请求数不增加时拒绝。实际计时使用 Core 单调截止时间。
- 先登记已获确认的 TXT 记录，再检查预算或进入传播；即使 Add 刚成功就超时，仍保有
  统一收尾所需的完整记录对。签发结果在清理前确定；随后清除客户端/HTTP 预算状态，
  Remove 的三次尽力重试及引擎退休保持独立预算。迟到的成功证书不作为部分 grant 交付，
  失败经统一 grant 析构清除私钥。预算超时保留失败原因链，实际 MEMORY 保留原对象。
- 新 Issue 入口还须隔离调用前的旧错误。新增真实 allocator MEMORY 后再让首次 HTTP 前
  的受控时钟耗尽预算，补清理前候选代码复现“无新 IO 却返回旧 MEMORY”；最后仅在有效
  签发开始时清除旧错误，当前阶段真实 MEMORY 仍保真。该反例是本批未提交候选的复现，
  与 8a23f336 的旧版本对照分别留证，未混为同一基线。
- 完整 flow 故障夹具增加 18 组预算场景及两种非法配置：含无限时控制、nonce/签名/订单/
  证书阶段、轮询/只读与 Add 退避、Add 成功即耗尽、传播回调耗尽、清理独立性、迟到
  MEMORY、超时上下文 OOM、非法域名及旧错误隔离；每组检查同实例分别请求的 read/Issue
  恢复、产物失败原子性和最终零分配。JSON/JWS/CSR/X509/allocator/grant/拥有权走真实源码，
  时间与 HTTP/provider 同步边界受控，不将这组宣称为真实网络证明。
  相同最终夹具配 8a23f336 源码：18 组中 16 组失败，0/-1 两组控制通过；内部 HTTP
  新增 30 组操作预算场景，旧源码 18 组失败、12 组控制通过。原 83 组传输、32 组交换
  预算及参数组保留，HTTP 故障程序现每环境共 146 组通过。
- 独立真实 TCP/TLS 对端增加六组外层预算控制/超时，总计 12 组：HTTP 自身留 30 秒，
  外层仅 1000/2500ms，跨握手、响应头/体及只读重试验证耗尽后的请求数、超时返回和
  析构前 Abort/关闭。Windows/Linux GCC、Linux Clang sanitizer 均通过；HTTP lifecycle、
  拥有权程序及 40 项公开聚合 provider guard 一并通过。六处内部 DnsAdd 夹具调用显式
  传 NULL，保持其原有无 Issue 预算的合同；受影响的 46 组 provider 真实协议/拥有权场景
  在这三环境通过，不把它们算作 provider 内层预算接通。
- 首轮 Windows 组合验证中的拥有权程序触发原驱动 90 秒总界限，之前的直接程序通过，
  旧日志未保留阶段输出，唯一根因尚未证实。现在夹具 stdout 不缓冲，驱动在总超时异常
  时保留 stdout/stderr；不增加 90 秒界限。单独拥有权复验及原组合命令均重新通过；
  保留首次失败，后续继续跟踪，诊断补充不冒充产品根因修复。
- 首次完整套件 Windows GCC 38 程序、Linux GCC/Clang ASan+UBSan+float-cast-overflow+leaks/
  TCC 各 37 程序通过。最终补入口旧错误清理后，四环境追加全部 28 个模块程序和完整
  flow 故障程序，18 组新增场景各通过；未重复与这处变化无关的其他单头程序。最终模拟
  CA 脚本在 Windows/Linux GCC、Linux Clang sanitizer 各 18 项全部通过，含脚本检查和
  真实 TLS/JWS/EAB/TXT/grant/store 流程，不把每项都计成独立 CA 会话。
  其中一次 Linux 复验误选历史 out/gcc/native-linux 二进制，产生五项失败；本轮构建日志
  指定的实际产物为 out/gcc/native，修正选择后两编译器均通过，保留误选日志与两份二进制
  哈希/mtime，不归咎于产品新回归。
- 最终四个 Windows/Linux static/shared 消费者通过，每包 16 个自有源文件、链接一份
  Core。生成实现/声明、feature、公开符号/参考和发布门禁通过；1211 项最终相关源码、
  头、manifest 与驱动输入归一化 LF 后 Windows/WSL 一致。
- 第 33 批 0ef84948 已在两平台恢复 ACME coverage：Windows 5885/6737 行（87.35%）、
  4903/6949 分支结果（70.56%）；Linux 5874/6740 行（87.15%）、4911/6957（70.59%），
  均超过原 70%/55% 门槛。已下载原始 artifact 并核对每平台全部 16 个自有 C 文件哈希
  与 0ef84948 Git blob 一致；严格合并器未改。这是 0ef84948 的证据，不能套用于本批
  新 Issue 控制流；本批仍需自己的 CI 和覆盖率。
- 本批关闭 Issue 的 ACME HTTP 预算与外层退避/状态生命周期；内层 UDP 接收、传播睡眠
  及内置 provider HTTP/重签退避仍需贯穿截止时间，宿主同步回调亦不能抢占终止。
  Retry-After、EAB 标识符、RSA 导入、provider/签名/XML、DNS 问题/属主/CNAME 绑定和
  EDNS/TCP 截断恢复继续。仍仅 xjwt/xoauth2 为整库有界首轮完成，合计 2/21。

证据：`acme-issue-budget-windows-before.json`、`acme-issue-budget-before-provenance.json`、
`acme-issue-budget-factory-before.json`、`acme-issue-budget-stale-before.log`、
`acme-issue-budget-stale-before-provenance.json`、`acme-issue-budget-full-{windows,linux}.json`、
`acme-issue-budget-postclear-{windows,linux}.json`、`acme-issue-budget-final-{windows,linux}.json`、
`acme-issue-budget-wire-windows-gcc.log`（首次总超时）、`acme-issue-budget-owner-windows-repro.log`、
`acme-issue-budget-wire-final-*.log`、`acme-issue-budget-provider-{windows,linux}.json`、
`acme-issue-budget-mock-{windows,linux}.json`、`acme-issue-budget-mock-stale-selection.json`、
`acme-issue-budget-mock-linux-gcc-stale-binary.log`、`acme-issue-budget-packages-final-*.json`、
`acme-issue-budget-checks.json`、`acme-issue-budget-final-input-audit.json`、
`ci-0ef84948-acme-coverage-provenance.json` 及对应原始 zip/json/log。


### 第 35 批：DNS 查询、重发与传播等待共享截止时间

- 以 3595c0d5 的实际 DNS 源码替换当前独立 UDP 夹具中的该实现：原有 Wait
  API 的 1000ms 超时仍接受 1.218 秒后的匹配响应；静默对端下发送两次查询，
  约 4 秒才结束。2500ms 控制组正常通过。新增 Until 名称仅用测试兼容转发器
  满足组合夹具的其他调用，本组旧版本对照实际调用未改变签名的旧 Wait，
  不把这些转发器冒称为旧版已支持的截止时间 API。
- 内部 QueryUntil / ContainsUntil 显式接收 Core Timer 的绝对截止时间，预算覆盖
  随机数、组包、打开 UDP、首次发送、两轮接收和重发；每轮接收仍保留原 2 秒上限，
  同时取外层剩余时间。普通 Query/Contains 继续以 INFINITY 委托，未加入可串扰的
  DNS 可变预算状态。超时后的成功或解析结果不交付部分输出，失败统一关闭/Abort，
  释放 packet/buffer/调用方引用；运行中的借用引擎不被关闭。
- 真实接收失败的首个原因保留到截止时间包装；实际 producer MEMORY 或超时上下文
  自身 OOM 保持真实 MEMORY。入口旧错误清除，清理诊断不能覆盖主错误。普通随机数/
  地址/发送错误仍使用既有稳定 DNS 诊断，不将本批宣称为所有底层错误均保留 cause。
  Wait 的查询、重发和退避共用一次预算：0 立即超时，-1 不限时，其他负数非法。
- 传播探测取 Issue / 自身传播预算中较早者，跨 resolver 传入同一截止时间，
  超限不再咨询后续 resolver，2 秒睡眠按剩余量截断。传播自身超时保持咨询性，
  Issue 超时和实际 MEMORY 仍可成为签发主错误；已确认的 TXT 先登记清理拥有权。
  配置注释明确同步宿主回调不能抢占，Remove 与引擎退休使用独立清理预算，
  因而函数返回时间可能晚于工作预算。内置 provider HTTP/重签退避尚未贯通。
- DNS 故障程序新增 60 组：实际 Core engine/UDP/packet/Future/allocator 运行，
  仅在提交/完成边界收费时钟，覆盖无穷/非法/立即预算、随机数与打开/发送耗尽、
  迟到响应/解析、错误 ID、重发、首因链、真实 MEMORY、包装 OOM、旧错误、
  退避截断、清理和同引擎恢复。flow 另增 7 组，核对父预算与传播自身预算、
  多 resolver、实际 MEMORY 对象和 TXT Remove；完整 Issue 预算矩阵共 25 组。
- 独立真实 UDP 对端另加 9 组 Query/Contains/Wait 的迟到控制、迟到超时和静默超时：
  以实际查询数量核对无额外重发，在 stdin 析构屏障前确认借用引擎仍运行且
  LiveObjects=0；原 DNS 生命周期与 13 组 resolver/完整应答场景保留。
  Windows/Linux GCC、Linux Clang ASan+UBSan+leaks 均通过，原拥有权/公开单头 guard
  一并通过；未增加已有 90 秒总界限。
- 完整 ACME Windows GCC 38 程序、Linux GCC/Clang ASan+UBSan+
  float-cast-overflow+leaks/TCC 各 37 程序通过；每环境均含上述 60/25 组。
  模拟 CA 脚本 Windows/Linux GCC、Linux Clang sanitizer 各 18 项通过；
  Windows/Linux static/shared 四个消费者通过。Windows shared 首次编译在
  Core value.c 报 cc1 无法分配 65536 字节；保留原日志，在可用内存恢复后
  单独原参数重跑通过，不改代码/预算/门禁。生成实现/声明、features、
  API 文档/参考与 release maturity 检查通过；1211 项最终输入归一化 LF
  后 Windows/WSL 一致。
- 3595c0d5 PR CI 40416266467 的实际终态为 72 成功 / 1 失败，唯一失败是
  Linux GCC 的 xsmtp test_smtp_upload_tls_fault_runtime 收到 SIGSEGV；两项 ACME
  coverage 成功，push mail 40416259726 成功。原 GCC 在 GDB 下 33 轮和 Clang
  ASan+UBSan 下 20 轮均通过，不能视为根因修复。进一步可控调度已复现邮件测试
  TLS Upgrade 助手在发布失败 Future 后，Worker 仍读取借用栈任务而调用线程已返回；
  xmail/xsmtp/xpop3/ximap 对应助手段落完全一致。该夹具缺陷待下一批修复，
  与上述 CI 崩溃的唯一因果关系尚未证实，不以它宣称报错已解决。
- 本批关闭库内 UDP 与传播等待预算。下一步继续内置 provider 的操作锁内 HTTP/
  重签退避、宿主同步边界、Retry-After、EAB/RSA/provider/签名/XML、DNS 问题/
  属主/CNAME 绑定及 EDNS/TCP 恢复。其余扩展仍按整库标准推进，只有 xjwt/xoauth2
  完成有界首轮验收，合计仍为 2/21，未合并或发布。

证据：acme-dns-budget-before.json、acme-dns-budget-before-build.log、
acme-dns-budget-full-{windows,linux}.json、acme-dns-budget-wire-windows-gcc.log、
acme-dns-budget-final-{windows,linux}.json 及对应日志、
acme-dns-budget-mock-{windows,linux}.json、acme-dns-budget-packages-final-*.json、
acme-dns-budget-package-final-windows-shared.log、
acme-dns-budget-package-windows-shared-oom-retry.{json,log}、
acme-dns-budget-checks.json、acme-dns-budget-final-input-audit.json、
ci-3595-xsmtp-extension-failed.log、smtp-3595-{gcc-gdb,clang-asan}.json 及原日志、
smtp-upgrade-stack-before{,-provenance}.json、smtp-upgrade-stack-before.log。


### 第 36 批：邮件 TLS 测试助手退休栈任务后返回

- xmail/xsmtp/xpop3/ximap 的 TLS Upgrade 助手把 Upgrade 和 Stream 栈对象借给
  Worker。在 Promise Resolve/Reject/Close 发布终态后，Worker 仍读取 Upgrade.Promise
  并销毁 Promise；旧等待线程却可能已经走失败返回，或因超时返回。终态可见并不证明
  producer 任务已经退休，不能据此释放借用栈帧。四份相关助手段落的原始内容相同。
- 用真实 Core TCP listener/connect/accept、Engine Post、Promise/Future 和 TLS
  constructor 创建缺少 context/identity 的实际拒绝：发布拒绝后门控 Worker，
  确认等待线程已经观察到 Future 终态，再检查调用线程仍不得返回。最终同一回归配
  655bdbae 旧助手源码明确失败，打印 Worker 仍借用栈任务；未伪造 Future 状态或
  以进程析构掩盖安全性。旧源码对照只调整 include 路径，Core/夹具仍为真实实现。
- 四份助手增加独立 release/acquire 完成标志。Worker 销毁 Promise、结束所有任务
  字段访问后发布完成；等待线程在任何返回分支前确认完成，再检查 Future 和 TLS。
  原截止时间内未退休则直接使测试失败，不能携未退休栈任务返回；未延长原预算。
  放行门控后核对调用线程成功退休、引擎销毁、零存活分配及零 invalid/double free。
- 共用门控回归分别挂入四库的测试 manifest。原 7 个实际使用该助手的 TLS 故障/
  升级/压缩程序与 4 个新门控程序，在 Windows GCC、Linux GCC、Clang ASan+UBSan+
  float-cast-overflow+leaks、TCC 均通过，每环境 11 程序。原始构建输出存在合并行，
  初版计数器以行首匹配只报 3 个 PASS；逐程序运行记录及全文标记复核均为 4 个
  门控程序通过，修正后的统计独立存于 final-audit，不隐去原始日志。
- SMTP 上传测试 stdout 不缓冲，每个 DATA/BDAT ×取消/超时/断开场景打印 begin/
  opened/retired 阶段，以保留下一次异常前最后阶段；断言、传输负载、15 秒场景预算、
  3 秒写超时和清理预算均未变。该批修复已确认的测试夹具竞态，3595c0d5 的
  SIGSEGV 尚未证明唯一根因，后续仍需实际 CI，不能仅凭本地通过宣称 CI 报错已解决。
- 四库的生成/feature/API 文档/参考/release maturity 共 20 项检查通过；914 项
  Core 和邮件库源码、头、manifest、共享夹具及驱动输入归一化 LF 后 Windows/WSL
  一致。上述为受影响路径验证，不冒称本地重新运行四库全部协议/单头/裁剪验收。
- 已下载 3595c0d5 的原始 ACME coverage artifact，两平台全部 16 个自有 C 文件
  哈希与对应 Git blob 一致。Windows 5908/6776 行（87.19%）、4923/6997 分支
  结果（70.36%）；Linux 5899/6779 行（87.02%）、4932/7005（70.41%），均超过
  未变的 70%/55% 门槛。这只证明 3595c0d5 的 Issue 变化，不能套用于 655bdbae
  的新 DNS 控制流，本批新 head 仍须独立远端验证。
- 下一步继续实际 CI 及 ACME 内置 provider 的操作锁/HTTP/重签退避预算。整库
  有界首轮完成仍为 xjwt/xoauth2，共 2/21，其余 19 个产品继续，未合并或发布。

证据：smtp-upgrade-stack-before{,-provenance}.json、mail-tls-upgrade-final-before.json、
mail-tls-upgrade-final-before{,-build}.log、mail-tls-upgrade-selected.json、
mail-tls-upgrade-{windows,linux}.json、mail-tls-upgrade-*-{gcc,clang,tcc}.log、
mail-tls-upgrade-final-audit.json、mail-tls-upgrade-checks.json、
mail-tls-upgrade-input-audit.json、ci-3595c0d5-acme-coverage-provenance.json
及对应原始 coverage zip/json。


### 第 37 批：内置 provider 的操作锁、HTTP 与重签退避预算

- 真实签名 TLS 对端先复现：给 Flow 的 Ali Add 配 1000ms Issue 截止时间，
  首个只读响应延迟 1.2 秒后仍继续发创建请求并交付记录；2500ms 控制组通过，
  测试记录最终完整清除。随后用最终 42 组夹具配 1c6f4cee 的全部自有 C/公开/
  私有头原始 Git blob：16 个超时场景失败，26 个控制通过；只重定向 include，
  未替换 signer、transport、allocator 或生产实现。原始快照、逐项哈希及二进制留存。
- 增加内部 AddUntil 分发，依据原始内置 Add/Remove 回调地址配对识别实例，
  不依据可自定义的 sId 强转上下文。自定义回调保留宿主同步契约。五家内置实现
  在自己的操作锁内保存/缩短/恢复 HTTP 上层截止时间，等待锁也计入有限预算，
  获取锁前不读写另一调用拥有的 HTTP 预算字段。普通公开 Add 委托无限上层预算，
  原有单次 HTTP 上限仍在。参数/已释放/清理中状态检查和 Unit 串行契约保留。
- 每次提供商请求在签名/提交前检查预算，HTTP 拨号/全量 IO/安全重试继承同一截止时间；
  Ali 重签、Tencent 只读 RPC、AWS RMW 冲突及 Huawei 删除确认的睡眠共用剩余量。
  Remove 的独立清理不继承本次 Add 预算。AWS 在新请求签名/预算检查前重置旧的
  bWriteUncertain，避免无新 IO 的失败沿用上一请求状态；写入结果未知仍不重放。
- 已确认创建的成功必须先交付句柄并登记 Flow 清理拥有权，即使后续本地完成工作
  跨过预算，不能先改成 false 丢失清理入口。新增迟到 ACK 场景在实际 transport
  成功返回后延迟本地完成，核对 Add 确认/Issue 超时、Scope 恢复和实际 Remove。
- 独立协议对端新增 42 组：五家各有迟到响应、丢失只读响应/重签退避、锁竞争、
  迟到 ACK 的控制/超时；AWS 另有两次真实冲突后的控制/超时。stdin 屏障前核对
  实际请求/已写入记录数；锁超时零请求，普通读超时只允许首个查询，预算耗尽不再创建。
  AWS 有限预算在两次已证实拒绝后停止 RMW，后续同实例恢复/删除保留外部 TXT 值。
  每例放行后完成独立清理、同实例无上层预算恢复及最终零 Core 资源。
- 新注册 provider budget 故障单头程序覆盖 id 碰撞/毒化上下文、回调配对、已释放
  状态、非法/已耗尽预算、旧 MEMORY 隔离、真实错误对象/allocator/包装 OOM、
  嵌套预算恢复、截断退避和非法锁。时间/睡眠边界受控，真实签名、错误、分配器与
  互斥锁执行实际源码；不把该程序冒充真实网络证据。
- 首轮候选另复现 Ali/Tencent 重签期间首因丢失：第二次请求清除旧错误后才发现
  超时，最终链中不含首个真实 IO 对象。两组失败、两组实际 MEMORY 控制通过，
  单独保留候选源码快照。现在每次调用保留首个可重试错误引用至统一 Done；
  超时保留该因果链，后续实际 MEMORY 优先，成功/失败均释放引用。最终四组通过，
  不宣称所有跨 RPC 的普通最终失败均已改为首因策略。
- 完整套件首轮的 28 个模块程序每环境通过，但 flow 单头夹具只嵌入一家 provider，
  与实际启用五家的选择不一致，导致四个私有符号未定义；原失败保留。补齐实际选择
  并让分发入口使用同一受控 Issue 时钟，25 组 Issue 预算回归通过。最后在首因修复
  后重跑完整套件：Windows GCC 39 程序，Linux GCC / Clang ASan+UBSan+
  float-cast-overflow+leaks / TCC 各 38 程序全部通过，含 146 组 HTTP、60 组 DNS、
  25 组 Issue 及新的 provider budget/首因单头程序。
- 最终 Linux GCC 完整独立 provider 协议集合 763 组通过；Windows GCC 与 Linux
  Clang ASan+UBSan+leaks 的新增 42 组分别通过。此前拥有权/构造与 provider body
  程序三环境通过；首因修复后刷新三环境各 18 项模拟 CA 及 Windows/Linux
  static/shared 四个消费者。生成实现/声明、features、API 文档/参考及 release
  maturity 检查通过；1213 项最终输入归一化 LF 后 Windows/WSL 一致。
- 最小选择检查：六个 DNS 根与纯客户端 acme_flow 根实际编译链接/运行，测试所需
  memory_debug 显式作为支持模块；两平台 GCC 模块裁剪通过。第一次直接选择 dns_ali
  缺少其测试使用的 memory_debug，第一次选 acme_flow 的 test_flow 又依赖 Obtain
  聚合能力；均保留失败，改用显式支持依赖及纯 test_client 核对最小生产闭包。
  这些结果不表示所有聚合集成测试可单独放在最小模块运行；测试资产支持依赖/
  归属的进一步整理保留为后续工作，不往生产模块塞入测试用依赖来换取通过。
- 已推送 1c6f4cee 的 PR CI 41321375367 最终 73/73 成功，独立 mail 41321370433 成功；
  两项重复工作流已确认取消。该 head 的两平台 ACME 原始 coverage artifact 已下载，
  各 16 个自有 C 文件哈希与 Git blob 一致：Windows 5942/6820 行（87.13%）、
  4954/7061 分支结果（70.16%）；Linux 5935/6823 行（86.99%）、4966/7069
  （70.25%），原 70%/55% 门槛不变。这证明前两批 DNS/邮件 head；
  本批新 provider 控制流仍需要自己的 CI/coverage，不能套用这些比例。
- 宿主同步回调不可抢占，包装后的回调仍须遵守宿主预算协作；Remove/退休可晚于
  工作预算返回。下一步继续实际 CI、Retry-After、EAB/RSA/provider/XML、
  DNS 问题/属主/CNAME 绑定与 EDNS/TCP 恢复及其他产品源码验收。
  整库有界首轮完成仍只有 xjwt/xoauth2，2/21，未合并或发布。

证据：acme-provider-parent-before.json、acme-provider-budget-before-final42{,-provenance}.json、
acme-provider-budget-baseline-final-42/、acme-provider-retry-cause-before{,-provenance}.json
及对应源码快照/构建日志；acme-provider-budget-full-final-{windows,linux}.json、
acme-provider-budget-wire-postcause-{windows,linux}.json、acme-provider-budget-postcause-*.json、
acme-provider-budget-package-postcause-*.log、acme-provider-budget-singles-final-*.json、
acme-provider-budget-minimal-final-linux.json（含被保留的选择失败）、
acme-provider-budget-minimal-complete-linux.json、acme-provider-budget-trim-windows-gcc.json、
acme-provider-budget-checks-final.json、acme-provider-budget-final-input-audit.json、
acme-provider-budget-final-validation.json；ci-1c6f4cee-{runs,jobs}.json、
ci-1c6f4cee-acme-coverage-provenance.json 及对应原始 coverage zip/json。


### 第三十八批：Remove 装饰保持内置 Add 预算与诊断屏障

- 以 eccd5638 的 16 个生产 C 文件哈希核对原实现，加入只包装 Remove、保持原生 Add
  与上下文的真实 TLS 场景。五家 provider 的 1 秒超时组都复现约 1.21 秒后仍创建记录，
  五个 2.5 秒控制组通过；每例都完成清理及同实例恢复，未留下测试记录。
- 内置 Add 本身决定其私有上下文类型契约，Remove 可独立包装。五个私有 Until 入口现只
  按原生 Add 地址匹配，保留自定义 Add 不解引用上下文的规则；不按 sId 猜类型，
  不改变公开结构、普通 Add、Remove、已确认晚成功及独立清理的契约。
- 新增十个网络场景及五个匹配/已释放上下文断言。provider budget 单头在 Windows GCC、
  Linux GCC、Clang ASan/UBSan/float-cast-overflow+leaks、TCC 通过。Windows 初次
  编译报 cc1 内存分配失败，原日志保留，随后同命令完整编译运行通过。
- 最终 Linux GCC 773 个真实 provider 协议场景全通过；Windows GCC 与 Linux Clang
  ASan/UBSan+leaks 各 52 个预算场景通过。全部保持返回屏障前的请求/创建数、屏障后
  清理、恢复及零 Core 资源断言；没有提高超时或减少协议场景。
- 另用原实现二进制、XACME_DEBUG=1 复现 AWS retry 正控制被诊断行误判为返回屏障。
  Python 观察器现在跳过并保留诊断行，等待实际 BUDGET_RETURNED 标记；三个网络环境
  各六个开启诊断的控制组通过，包括五家 decorated timeout 和 AWS retry control。
- 五项生成/features/API 文档/参考/release 检查通过；1213 项最终 ACME 构建输入归一化
  后 Windows/WSL 相同。本批刷新受影响预算单头与真实网络测试；上一批完整套件、
  包消费与裁剪证据仍按上一批源码范围记录，不冒称本批重跑了全部资产。
- eccd5638 的 PR CI 41330775664 目前 70 项成功、3 项运行中；独立 mail 成功，
  重复 push CI 和 PR mail 均已确认取消。其最终 CI/coverage 与本批新提交继续分开核对。
  全部产品首轮验收仍 2/21，ACME 的协议/RSA/DNS 语义复核及其他产品继续。

证据：acme-decorated-before{,-provenance}.json、acme-budget-debug-before.json、
acme-decorated-unit-{windows,linux}.json、acme-decorated-unit-windows-initial-oom.json、
acme-decorated-wire-final-{windows,linux}.json、acme-decorated-debug-*.log、
acme-decorated-checks.json、acme-decorated-final-input-audit.json 及对应逐组日志；
ci-eccd5638-{run,jobs,artifacts}.json。


### 第三十九批：连接组源码复核、字节视图与本地交付验收

- 逐段复核 xws-group 的三个自有 C 文件、两个内部头及 38 项公开接口：唯一成员/容量/
  Seal、锁外释放与快照扩容、成员引用回滚、广播共享负载、部分接纳、Future 监听引用、
  聚合取消和最终退休。基础层不引入 Future；快照独立于原组，取消聚合等待不代表成员
  全部已终止。完整测试包括八线程各 1000 轮成员/快照并发、128 次双成员广播、
  真实 TCP/Worker 队列背压、取消、OOM、未对齐范围/哨兵及失败所有权。
- 原实现 Linux Clang 21 ASan/UBSan/float-cast-overflow 在现有未对齐结果用例报错。
  保留 halt-on-error 失败；另一次仅用于观察全部诊断的执行确认 Result 写入和 Ref
  读取两处报告，未当作通过证据。三个自有 C 文件及共享夹具的旧哈希与 eccd5638
  Git blob 一致，原二进制和日志保留。实际修复为两个 Result 发布点及一个 Ref
  快照入口的显式 void/const void 字节视图，保持原有范围/重叠/所有权规则。
- 新增部分接纳时 REJECTED 槽位的未对齐结果、哨兵、连接与错误保真检查；保留原有
  RESOLVED/Ref 未对齐用例。包消费者新增真实公开 Future 广播调用：空组接管 Ref
  恰好 Release 一次，独立完成 Future 在 Operation/Group 销毁后仍有效。
- 修复主题文档遗留的 Core 模块宏及 xrt.h 入口说明，改为本库宏与 xws-group.h；
  Limit 固定值和加锁瞬时查询区分，删除重复 WaitFor 名称。
- 最终 Windows GCC、Linux GCC / Clang ASan+UBSan+float-cast-overflow+leaks / TCC
  各执行全部 4 个模块测试、3 个单头程序及 2 个可运行示例，9/9 均通过。两平台
  static/shared 四个消费者与模块依赖裁剪均通过。五项生成/features/API/release 门禁
  通过。原 Windows 包编译 OOM、随后一次无 CPU/子进展的 GCC 编译停滞均保留；
  仅终止已核对属于本任务的停滞编译器（约 234 秒），最终完整复跑通过。
- 两平台各执行基础组/广播两个体积 profile 的 single/static/shared 六种产物，禁用
  HTTP/Client/Server 符号检查通过；空组 snapshot 性能 smoke 通过，不据此声称吞吐
  或性能回归结论。WSL 首次体积命令无终态摘要且宿主返回 1，空日志保留；观察到
  WSL 系统重启记录，但未宣称唯一因果，最终重新执行两个 profile 均通过。
- 广范围比对发现一个未选择的 Core ownership 测试快照不同；保留初始报告，不覆盖
  无关源文件。该程序及包含它的 Core 单头不在组资产/包含闭包中；排除这一项后，
  覆盖注册输入、Core/xws/group C/头及嵌套测试的 2223 项超集归一化一致。
- eccd5638 的两平台 ACME coverage 已下载并各核对 16 个生产源文件 Git blob：Windows
  6044/6918 行（87.37%）、5063/7173 分支结果（70.58%）；Linux 6035/6921 行
  （87.20%）、5073/7181（70.64%）。70%/55% 原门槛不变，范围仅为该旧提交。
- 本产品尚需本批提交的 GitHub CI；Windows MSVC/clang-cl/x86 未本地执行，
  Android/BSD/macOS、TSan 和长稳不在本次证据内。现有并发与 sanitizer 结果不替代这些
  环境。暂不计入完成，整库首轮验收维持 2/21，待 CI 后更新本产品有界验收状态。

证据：xws-group-alignment-before-provenance.json、xws-group-alignment-before-all.log、
xws-group-alignment-before-client（WSL）、xws-group-accept-*-before.{json,log}、
xws-group-accept-windows-stalled.json 及对应日志；xws-group-accept-{windows,linux}.json、
xws-group-selected-assets.json、xws-group-checks.json、xws-group-size-{windows,linux}.json、
xws-group-performance-{windows,linux}.json、xws-group-profile-*.json、
xws-group-broad-input-audit.json、xws-group-final-input-audit.json、xws-group-final-validation.json；
ci-eccd5638-acme-{windows,linux}-coverage{,-provenance}.json 及原始 zip。


### 第三十九批验收补记：连接组本轮完成

- 精确 head dd5a57c7 的 PR CI 41333917222 已完成，73/73 个 job 成功；独立 mail
  41333912214 成功，两个重复工作流及已被替代的 74e183e6 CI 均确认取消。
- 本地复核、四环境各 9 个完整程序、双平台 static/shared、裁剪、六种体积产物及
  元数据证据见第三十九批；本次另核对自有实现/头、生成头及共享 group 回归夹具
  的 Git blob，当前内容与该成功 head 一致。未把后续 xws-client 工作套入本次证据。
- xws-group 计入有界首轮完成，累计 xjwt / xoauth2 / xws-group，3/21。平台与
  性能限制保持第三十九批的记录；整轮工作仍继续，未合并或发布。
- 上一 ACME head eccd5638 的 PR CI 41330775664 同样最终 73/73 成功，
  其两平台 coverage 的源码绑定与比例见第三十九批；不等于 ACME 整库验收完成。

证据：xws-group-first-round-acceptance.json、xws-group-final-validation.json、
ci-dd5a57c7-{run,jobs}.json、ci-dd5a57c7-run-disposition.json、ci-eccd5638-{run,jobs}.json。


### 第四十批：客户端 Future 失败发布与借用快照

- 复核 xws-client 三个自有 C 文件、内部头及 17 个公开接口：URL 映射、源请求克隆/
  管理字段替换、严格 101/Accept/子协议/Deflate 检查、传输与早到帧交接、
  Future 装配/取消/成功值、结果引用及原子 Take。HTTP HeadersSet 的同名字段折叠
  已核对，未因重复头候选线索修改正常行为；结果 Take 与借用不可并发的既有契约保留。
- 原 Future 监听安装失败后先发布 BridgeFail，完成回调可立即释放上下文及其 Call；
  工厂随后再次读取 Context->Call 并取消。真实 Core bridge/Future/error/allocator，
  配合仅控制生产者边界的确定性回归，在原 dd5a57c7 源码上复现 URL/Request 两例
  失效 Call 访问，四个正常取消/延后退休控制通过，最终 Core 资源均平衡。
- 现在发布失败前将已有 Call 调用方引用转移到局部，并清空 Context 的所有权槽。
  发布后只使用该局部引用，取消后归还；完成回调可独立退休 Context。没有增加正常路径
  分配或引用操作，也没有以提高超时、禁止取消或延长借用寿命替代修复。
- 最终受控六组全通过。另增 URL/Request 两组真实 HTTP Call、两 Worker、Loopback
  TCP 故障补测：底层工作接纳后，在当前线程的真实监听分配点注入 OOM，返回 NULL
  时保留原 MEMORY 对象，独立 Drain 并回收全部网络和 Core 资源。四环境各八组及
  Linux GCC 最小 future_setup 闭包的八组均通过；受控边界与真实网络证据分开记录。
- 原实现完整 Clang sanitizer 在现有未对齐配置用例失败；新增真实 TCP 事件表用例又
  在私有事件快照处复现。自有生产 C/头的十项旧输入哈希与 dd5a57c7 一致，旧源码/
  二进制/日志保留。四个公开快照边界显式使用 void/const void 字节视图，另修正夹具
  自身的未对齐配置读取，不禁用 alignment。请求/握手的范围、重叠和失败不改输出断言保留。
- 事件表用例先传入带哨兵的未对齐存储；返回后清空原表，再放行本地对端的 101 和
  同块首帧，验证复制的回调仍完成消息和关闭。Future 模块化/单头复用该夹具；原先
  只被包含而未在聚合套件独立执行的 callback 入口现也登记运行。
- Windows GCC 全套 17 个程序（9 模块/5 单头/3 示例），Linux GCC / Clang
  ASan+UBSan+float-cast-overflow+leaks / TCC 各 14 个（7/4/3）通过。
  真实 Call 补测随后刷新单独的 setup 程序，生产输入未再变更；不把最初六组的日志
  冒称已经包含后加的两组。Windows/Linux static/shared 四个消费者及模块依赖裁剪通过。
- 五项生成/features/API/release 门禁和八项 extension layout/single 工具测试通过；
  1207 项 Core/HTTP/WebSocket 构建输入及测试支持超集归一化后 Windows/WSL 相同。
  最小 client/future/deflate 根另核对默认功能关闭的实际编译/运行；Linux 三根通过。
  Linux 的两个体积 profile 各 single/static/shared 及 request 性能 smoke 通过，
  Windows 的这部分测量尚在执行，不在本次提交中标记为完成；smoke 不作为吞吐回归结论。
- 本库还需本批 GitHub CI。未本地认证 MSVC/clang-cl/x86、Android/BSD/macOS、
  TSan/长稳或外部真实代理/证书服务。xhttp-client 中类似的后置 BridgeFail 使用路径
  属于后续独立复核范围，本批没有将其源代码纳入同一缺陷修复结论。
  本库暂不计入完成，整轮保持 xjwt/xoauth2/xws-group，3/21，未合并或发布。

证据：xws-client-before-provenance.json、xws-client-before-inputs/、
xws-client-before-linux.json、xws-client-setup-before-{linux,case-audit}.json、
xws-client-setup-before-client（WSL）、xws-client-events-before-linux.json 及对应日志；
xws-client-accept-{windows,linux}.json、xws-client-setup-final-{windows,linux}.json、
xws-client-selected-assets-final.json、xws-client-checks.json、xws-client-extra-linux.json、
xws-client-{size,performance}-linux.json、xws-client-final-input-audit.json、
xws-client-final-validation.json。暂停中断的构建日志另存 *-paused-20261105.log。


### 第四十批测量补记

- Windows 的 client/future/deflate 三个最小根实际编译运行通过；两个体积 profile 的
  single/static/shared 六种产物和 request construction smoke 均已完成并通过。
  Linux 对应部分同样完成。smoke 仅执行 50 次请求构造，不作为吞吐/回归结论。
- 最终证据绑定 b8df81f5 的 30 项自有源码/头/测试/配置输入，详见
  xws-client-final-validation.json、xws-client-extra-{windows,linux}.json、
  xws-client-{size,performance}-{windows,linux}.json。该提交的 CI 出现下述独立 Core 夹具失败，
  不能将此前本地通过或独立 mail 成功冒称本批完整远端 CI 成功。

### 第四十一批：Dial Future OOM 的 Listener 启动隔离

- b8df81f5 的 PR CI 41980100890 中，macOS Apple Clang job 127087975024 在 Core
  test_net_tcp_dial_future_oom 失败，发生在 WebSocket 扩展步骤之前；原日志保留。
  gh 的 run 日志命令拒绝未结束 run，job 日志又含终端控制符；最终使用 job API 下载原始
  日志，并剥离控制符供分析，原始文件保留，不改变 CI 门禁。
- 原断言只打印“终态保留 Dial Engine 资源”，本地第 21 次复现后补充计数，实际是
  LiveObjects=0 / timers=0 / commands=0，含 7 个 pending Future。加入 Listener 错误
  记录后第 39 次再次复现，Listener=CLOSED，错误类别 MEMORY。并非该本地失败
  多保留了一个 Dial；所有网络/并发生产实现的 Git 输入与 b8df81f5 一致。
- Listener 同步绑定地址但异步向 Worker 投递启动，原夹具等待的是另一 Resolver 线程，
  可能在接受观察尚未启动时开启全局 backing allocator 故障，从而关闭作为恢复对端的
  Listener。现先在同一 Worker、同一 FIFO 投递启动检查，确认 OPEN 且接受已挂起后
  再注入原 OOM；原 4096 个上限、全局 backing 拒绝、MEMORY、Future 取消/终态、
  LiveObjects==1 / timers==0 以及恢复连接断言全部保留。另检查 Listener 仍健康，
  失败时输出准确资源/Listener 快照。不用 sleep、提高超时或容许零/额外占用换取通过。
- 最终夹具仅去掉该启动等待的对照，第 3 次重新出现 CLOSED/MEMORY/LiveObjects=0；
  对照源码和二进制留在忽略证据目录。带等待的 Windows GCC select 500 次、IOCP
  200 次，Linux GCC all 1000 次、Clang ASan/UBSan/float-cast-overflow+leaks 100 次、
  TCC 100 次，合计 1900 次重复全通过。
- 相关完整 Dial Future 套件额外执行 Windows GCC 6 程序，Linux GCC / Clang sanitizer /
  TCC 各 4 程序，全部通过；Linux io_uring 包含入口仅编译通过，未在受限内核运行。
  Core 单头生成检查通过。本批只修改共享测试夹具，生产实现及公开 API 不变。
- macOS 原日志未记录计数，因此上述 Listener 路径是本地确认的同断言失败原因，
  不宣称已经证明远端原失败的唯一原因；新提交仍须重新运行 macOS CI 验证。
  下一步继续 HTTP/WebSocket Future 对应路径及其余产品验收，整轮保持 3/21。

证据：ci-b8df81f5-job-127087975024-{raw,clean}.log、dial-oom-ci-before-provenance.json、
dial-oom-ci-before-linux-gcc-repeat.json、dial-oom-listener-before-linux-gcc-repeat.json、
dial-oom-*-before-fixture.c、dial-oom-ready-without-fence.{c,json} 及对应构建/重复日志；
dial-oom-ready-{windows,linux}.json、dial-oom-ready-full-{windows,linux}.json、
dial-oom-ready-linux-uring-compile.log、dial-oom-ready-final-validation.json。


### 第四十二批：xws-client 首轮验收

- 4b68f2c7 的 PR CI 41983687946 已完成，严格核对同一 head 的 73 个 job 全部成功，
  包括 macOS Apple Clang、Windows、扩展元数据和 xws-client 扩展作业。
  独立邮件 run 41983681051 同一 head 成功。
- 与第四十批本地验收的 b8df81f5 比对，30 项本库源码、头、测试和配置 Git blob
  在上述 CI head 与当前工作树中均未改变；Core OOM 夹具修复也通过了本次 macOS 作业。
  结合既有四环境完整测试、八组 setup 回归、三最小根、包/裁剪和体积/performance smoke
  证据，将 xws-client 计入本轮完成。smoke 仍不作为性能回归结论。
- 本轮累计 xjwt、xoauth2、xws-group、xws-client，4/21。当前工作树的后续 HTTP
  Future/字节视图修复属于下一批，不能由这次 CI 或本库的完成状态代为认证。
  未自动合并或发布；未覆盖平台和长稳边界延续第四十批记录。

证据：ci-4b68f2c7-run-final.json、ci-4b68f2c7-mail-final.json、
xws-client-first-round-acceptance.json、xws-client-final-validation.json。


### 第四十三批：HTTP Future 失败发布与字节视图

- 对允许提交线程和完成 Worker 并发的 xrtHttpClientDoAsync，先受控复现装配失败发布后
  完成回调释放上下文，而提交方继续从上下文读取 Call。真实 Core bridge/Future/error/
  allocator 不替换，只控制 producer 和完成时序；空/default options 两组提前完成失败，
  普通取消和延后完成四组对照通过，Windows/Linux 结果相同，原输入和二进制保留。
- Watch 失败时先把已有 Call 调用方引用移至局部并清空上下文槽，再发布 BridgeFail，
  后续只取消和释放局部 Call。正常路径不增加引用或分配。最终八组回归另含两个真实
  Call、双 Worker、loopback 的 Watch OOM 清理，核对原 MEMORY 错误对象和资源归零。
- WebSocket 服务端的同形候选撤回：xrtHttpConnRequest/响应提交检查要求连接所属
  Request Worker，真实 Upgrade 完成也在该 Worker 排队；受控跨线程提前完成不能证明
  该实现的真实可达缺陷。server_future.c 恢复原实现，只保留真实 Worker 上提交 Upgrade
  后注入 Watch OOM 的回归，验证原错误身份、Server drain 和 Engine/allocator 收尾。
- Clang sanitizer 随后在原有 HTTP Future 用例的 options init 中中止。保留严格失败日志，
  另用 recover 诊断遍历得到七处生产复制和两处测试夹具复制的未对齐访问；诊断运行即使
  返回 0 也不计为通过。五处 runtime 配置/options/CallInfo 与两处 Future options/ResultInfo
  均改用显式 void/const void 字节视图，保留范围验证和现有行为，不改变 ABI。
- Call 契约新增真正未对齐的终态 Info 输出，检查前后哨兵和 CANCELLED 元数据；修正
  夹具自身两处类型化 memcpy。两个新增单头入口分别复用完整 Call 契约和完整真实网络
  Future 用例，既有参数错误单头 smoke 保留。服务端结果注释纠正拆分遗留的客户端
  101 Response 所有权描述，同步生成实现/声明单头。测试依赖放在 feature=null 模块，
  不进入生产 ALL 根。
- 最终 Windows GCC 58 个程序通过：最小 setup 1、Call 契约模块/单头各 1、HTTP
  Future/lifecycle/HTTPS/coroutine 相关 12、完整选择的 xws_server_tests 43。
  Linux GCC / Clang ASan+UBSan+float-cast-overflow+leaks / TCC 各 50 个：1+1+1+11+36，
  合计 208 次程序运行。sanitizer 最终均恢复 halt_on_error=1；不把诊断 recover 当作门禁。
  这不是 xhttp-client 全产品 18 个源码和所有可选模块的完整验收。
- 两平台 http_client_future / websocket_server_future 根各 static/shared 消费者及裁剪
  均通过，合计八个消费者和四个裁剪检查；整体服务端其他根的交付/profile 仍待后续。
  十项生成/features/API/release 检查和八项 layout/single 工具测试通过。
  1457 项构建与 Core/HTTP/WebSocket 支持超集有一处既存无关 Core 测试差异；该项不在
  本批 source/test/single/example 集合且无 include 引用，两份都未覆盖。保留宽报告后，
  本批范围的 1456 项归一化输入 Windows/WSL 相同。
- 本次另阅读服务端五个 C、三个内部头和十四个公开函数，保留附加 Header/最小压缩根
  输入契约候选待验证，不作为已确认缺陷；未以源码阅读或本地套件通过代替全库验收。
  新改动仍须本批 CI。累计保持 4/21，未本地认证额外平台、TSan、长稳或外部服务。

证据：future-setup-before-{provenance,windows,linux}.json、future-setup-before-inputs/、
future-setup-server-candidate-disposition.json、future-setup-server-final-{windows,linux}.json；
http-byteviews-before-{provenance,diagnostics}.json 及原日志/二进制；
http-future-final-{windows,linux}.json、http-future-final-selected-assets.json、
http-future-final-input-audit.json、http-future-final-tooling.json、future-setup-checks.json、
http-future-final-validation.json、xws-server-source-review-20261105.json。


### 第四十三批远端补记

- 70365de1 的 PR CI 41992076397 已完成，同一 head 的 73 个 job 全部成功；
  独立邮件 41992068227 同一 head 成功，重复 push CI 已终态取消。
- 第四十三批八项 HTTP 自有变更输入仍与当前工作树相同；本次 CI 对应不可变的旧 head，
  不代替后续服务端握手策略和 benchmark 辅助层改动的验证。
  xhttp-client 全库源码与可选模块复核仍未完成，整轮保持 4/21。

证据：ci-70365de1-run-final.json、ci-70365de1-mail.json、http-future-ci-70365de1.json。


### 第四十四批：服务端握手策略与回环基准退休

- 完成 xws-server 五个自有 C、三个内部头及十四个公开函数的有界首轮复核，检查握手、
  配置/路由快照、Upgrade 转移、同步失败、Future 结果和 Take/取消/Release。上一批
  两个输入契约候选均经对照确认并修复；不重新引入已撤回的同 Worker 并发候选。
- UpgradeAccept 原来只禁止三个具体 Sec-WebSocket 响应字段，与公开声明的全部
  Sec-WebSocket-* 保留空间不符。原实现 Windows/Linux GCC 模块/单头在八项旧控制
  后接受 Sec-WebSocket-Key；现按大小写不敏感的完整前缀拒绝十一项保留字段。
  同一真实 HTTP 连接最终仍可完成合法 101、消息和关闭；Sec-WebSocket 与
  X-Sec-WebSocket-Experimental 两个近似普通字段在对端收到并核对原值。
- 未选 deflate 的最小根原来会把有效压缩快照静默改成无压缩 Reply，UpgradeAccept
  也漏掉禁用压缩配置的快照检查；完整根另漏掉 RequireDeflate 与无压缩快照的冲突。
  分别补齐能力和策略预检，保留错误类别/扩展码与同步失败不回调的契约。
  同一有效 permessage-deflate 快照复用于模块/单头；最小模块和单头旧实现各两项
  失败、两项控制通过，完整根新增强制策略一项失败、四项控制通过。夹具在两次基线
  之间补充了强制策略，保留各次原源码/二进制，不声称它们是同一夹具版本。
- 新压缩回归检查输入不变、零完成回调与普通无压缩 Reply。仅以真实私有连接存储
  的零值作为必须在 Worker 操作前拒绝的输入，不把它当作可提交 producer。
  引用快照改用 memcpy 保留填充字节；公开说明和实现/声明单头同步更新，ABI 不变。
- 专用回环 profile 暴露基准自身两项问题：Linux 在定义 _GNU_SOURCE 前引入系统头，
  以及两平台在关闭回调刚发布、最后引用尚未退休时立即断言 LiveObjects==0。
  原诊断记录 live=2 / wait_errors=0，后续 Destroy 成功；这不是生产泄漏的证明。
  现先引入 bench_common.h，并在原 5000ms 期限内用 TryStop 等待运行期引用退休、
  加入 Workers，随后保留 LiveObjects==0 / WaitErrors==0 和 Destroy 的严格检查。
  不消耗 Engine 的 TryStop 使最终快照有效；无法退休/销毁时基准进程退出，避免借用
  栈/载荷仍被使用时返回或释放。等待不计入测量，不增加 sleep 或放宽超时。
- 为旧基准单独重编译前源码、使用相同真实依赖和 Worker，Windows/Linux 各第一次
  重复即退出 2，live>0 / wait_errors=0。最终 GCC 两平台各 100 次、Linux Clang
  sanitizer 和 TCC 各 20 次，共 240 次额外重复全部通过。初次 performance-smoke
  的失败条目及日志保留，未改写成通过；这些 smoke 不证明吞吐或性能回归结果。
- 额外 TCC 构建发现公共基准辅助层依赖不支持的 GCC atomic 内建。仅为 TinyCC
  POSIX x64 LP64 增加与运行期相同的 locked cmpxchgq 原语，不引入库依赖；其他
  编译器和 Windows 分支保留。新增测试验证有符号/高 64 位值、四线程 10 万次更新、
  唯一获胜者和 5000 次非原子载荷发布。新增 50 次真实回环 smoke；两项均接入
  feature=null 测试根和完整服务端套件，测试/客户端依赖不进入生产 ALL 根。
- 本地完整选择为 Windows GCC 47、Linux GCC / Clang ASan+UBSan+
  float-cast-overflow+leaks / TCC 各 40 个程序：先执行完整 45/38 套件，再在相同
  完整 source/features/links 闭包独立运行新增两项，已逐项证明闭包不变；不声称
  47/40 来自一次整套调用。快照引用改进后另刷新四环境最小/完整/单头 12 程序；
  两项基准辅助在最小测试根/完整根四环境共 16 程序。含五种最小根的本批普通程序
  总运行 211 次，全部通过；240 次重复另计。
- 两平台全产品 static/shared 共四个消费者通过，实际检查 101 Reply 及同一 Core
  错误通道；共享包各核对十四个公开 API 导出。upgrade/router/full 三种体积 profile
  的 single/static/shared 共十八个产物通过，并额外对全部产物核对裁剪禁用符号。
  五项元数据门禁和八项 layout/single 工具测试通过。宽输入超集 1538 项保留一处
  既存无关 Core 测试差异，该测试未选中也未被审计支持头引用；未覆盖任一份文件。
  本批范围 1537 项归一化 Windows/WSL 输入一致，最终包/profile 在新清单后刷新。
- 本地未覆盖 MSVC/clang-cl/x86、Android/BSD/macOS、TSan、长稳和外部服务/代理；
  新 TCC 辅助分支仅验证 POSIX x64 LP64。当前改动仍待本批不可变 head 的远端 CI；
  70365de1 的 73/73 仅认证第四十三批，不能代替本批。累计保持 4/21，不合并或发布。

证据：xws-server-contract-before-{provenance,windows,linux}.json、
xws-server-contract-before-{windows,linux}-{supplement,required}.json 及各版本源码/日志；
xws-server-final-source-review.json、xws-server-round-{windows,linux}.json、
xws-server-snapshot-final-{windows,linux}.json、xws-server-bench-registration.json、
xws-server-bench-support-{windows,linux}.json、xws-server-bench-negative-{windows,linux}.json、
xws-server-bench-tcc-before.{json,log}、xws-server-bench-final-{windows,linux}-summary.json；
xws-server-final-package-{windows,linux}.json、xws-server-final-exports-{windows,linux}.json、
xws-server-size-{windows,linux}.json、xws-server-final-symbol-profiles-{windows,linux}.json、
xws-server-round-{checks,tooling,input-audit}.json、xws-server-final-validation.json。


### 第四十五批：UIDL 长度、基准跨平台与远端诊断

- d94c95be 的 PR CI 42008137820 在 macOS Apple Clang job 127180008239 的
  Build WebSocket extension runtime gates 编译失败。原始日志确认新接入的真实
  loopback smoke 引用了公共基准头；CPU 绑定分支把 cpu_set_t、CPU_SET 和
  sched_setaffinity 错用于所有 POSIX 平台，不是运行期服务端断言失败。
  本库扩展/元数据 job 已成功，不能据此忽略平台失败或计入验收。
- 公共基准辅助现只在 Linux 引入 sched.h/使用 Linux 绑定接口，Windows 分支保留；
  其他平台未设置 XBENCH_PIN_CPU 时安静运行，显式要求绑定时打印不支持并返回，
  不输出绑定成功标记。Linux 在 CPU_SET 前拒绝 CPU_SETSIZE 外的索引。
  保留原回环和资源退休门禁，不删除 macOS smoke，也不修改 workflow。
- 受控非 Linux POSIX 依赖配置在 Linux 主机上去除 __linux__ 并拒绝 Linux sched.h，
  GCC/Clang/TCC 的旧头均编译失败，新头均编译通过；unset/有效数字/无效文本九组
  检查通过。这不是 Darwin SDK 或 macOS 运行证明，仍须真实新 CI。
  Windows GCC、Linux GCC/Clang/TCC 另以各自真实平台跑十六组参数/合法 CPU 绑定
  检查；四环境原子/loopback 回归共八程序全通过，Clang 保持严格 sanitizer。
- 核对 RFC 1939 第 7 节 UIDL 规范：唯一 ID 限制 1..70 个 0x21..0x7e 字节。
  原解析器只检查字符、未检查长度；Windows/Linux GCC 模块和单头各接受 71、510
  字节 ID，空/1/69/70 及全部 256 字节范围控制通过，原文件、二进制和日志保留。
  现于写入结果前拒绝超过 70 字节的 ID，协议错误码不变；失败保持调用方输出。
  单头入口复用完整协议回归并保留原依赖裁剪、STAT/命令断言，公共说明和两种生成头同步。
- UIDL 最小根模块/单头四环境共八程序、每程序七组回归通过。完整 xpop3_tests
  四环境各 17 程序通过（11 模块、6 单头，所选集合没有 examples）；另有双平台
  五种最小根各六程序，总计 88 次普通程序运行。全产品 static/shared 四个消费者、
  两个裁剪检查、两种 profile 的 single/static/shared 共十二个体积产物通过，
  Windows/Linux profile 输入指纹相同；有限性能 smoke 不作为性能回归证明。
  五项元数据和八项 layout/single 工具检查通过。
- 回查输入审计脚本时发现，第四十四批七个绝对清单路径使 native/path 拼接仍读取
  Windows 原文件。保留旧报告后改为全仓库相对路径，实际分别读取 Windows/WSL，
  原范围 1537 项仍全部相同。扩展至 xpop3/xmail 源、头、清单、完整选择和工具导入后，
  最终 1691 项亦一致。原来排除的既存无关 Core 测试继续保持两份不动。
- d94c95be 的 Windows ACME job 127180008243 另在独立 HTTP/TLS 夹具失败：
  DNS TLS and copied response fields 连接 localhost 在 3000ms 预算内超时，随后
  IPv4/IPv6 与其余场景通过。该日志只有包装后的 connect 错误，不能判定唯一原因。
  增加失败时的耗时/预算及最多十六层 domain/operation/kind/code/system 错误链，
  不修改生产实现、原预算、成功断言和清理错误身份检查。Windows/Linux 各二十次
  真实 DNS/SNI/复制响应字段的独立 TLS 重复全部通过，故未声称本地重现了远端故障。
  两个故意连接失败的对照验证新增诊断；仍待下一 head 的 CI 及必要的进一步定位。
- xpop3 四个自有 C 已阅读，但环境旧错误与消息预算/回调失败、单消息响应序号关联、
  三处嵌套 deadline 检查仍只有源码候选，须真实夹具验证后决定。不能以本批 UIDL
  修复或套件通过宣布本库验收；四个示例也未由本批完整选择重新认证。
  xws-server 同样等待修复后的新 CI；d94c95be 的独立邮件成功、两组重复 run 终态取消。
  累计保持 4/21，长期任务继续，不合并或发布。

规范：https://www.rfc-editor.org/rfc/rfc1939.html#page-12 。
证据：ci-d94c95be-job-127180008239-{raw,clean}.log、
ci-d94c95be-job-127180008243-{raw,clean}.log、bench-affinity-before-provenance.json、
bench-affinity-{before,after}-posix.json、bench-affinity-{windows,linux}-platform.json、
bench-affinity-server-{windows,linux}.json、ci-portability-final-validation.json；
xpop3-protocol-before-{provenance,windows,linux}.json、xpop3-protocol-before-inputs/、
xpop3-protocol-before-fixtures/、xpop3-protocol-after-{windows,linux}.json、
xpop3-round-{windows,linux,checks,input-audit,selected-assets}.json、
xpop3-{size,performance}-{windows,linux}.json、xpop3-protocol-final-validation.json、
xpop3-source-review-in-progress.json；xws-server-round-input-audit-canonical.json、
acme-identity-{windows,linux}-probe.json、acme-identity-{windows,linux}-diagnostic-control.json。


### 第四十六批：xws-server 首轮验收

- 4e768fc7 的 PR CI 42012180391 已完成，同一 head 的 73 个 job 全部成功，包含
  实际 macOS WebSocket runtime gates；独立邮件 42012168899 同一 head 成功。
  先前 Windows ACME localhost TLS 超时本次未重现，不能据此证明原失败的唯一原因。
- 从不可变 CI head 逐项读取 Git blob，核对 1537 项构建/支持输入及 76 项本库自有、
  交付和基准输入与当前工作树相同，再分别读取 WSL 中独立物理副本核对 LF 归一化
  SHA256。服务端五个自有 C、三个内部头、十四个公开函数的复核输入亦保持一致。
- 结合第四十四/四十五批四环境完整选择、四个示例、真实错误/OOM/退休回归、双平台
  消费/裁剪/符号、十八个体积产物和有限回环证据，将 xws-server 计入有界首轮完成。
  并未将 smoke 当作吞吐或性能回归结论；未覆盖的长稳、TSan、外部服务等边界保留。
- 累计 xjwt、xoauth2、xws-group、xws-client、xws-server，5/21。当前 xpop3 改动
  不由旧 head 的成功结果代为认证；不合并或发布。

证据：ci-4e768fc7-run-final.json、ci-4e768fc7-mail-final.json、
xws-server-ci-4e768fc7-input-bindings.json、xws-server-first-round-acceptance.json。


### 第四十七批：POP3 会话、消息错误归属和生产依赖

- 用公开 Open/Login 取得的真实 Client 和真实回环对端验证上一批三个候选。旧实现
  Windows/Linux GCC 模块/单头各七项失败、四项正常对照通过；不是零填充对象或
  替换 transport 的 producer。原源码、夹具和四个失败二进制/日志均保留。
- AuthLine、Line、Close 的 deadline 检查原来嵌套在无效对象分支，合法会话的无效
  超时可触碰传输、错误归类为超时或使会话 FAILED，且无法完成健康 NOOP 恢复。
  现先拒绝无效 deadline，再检查对象/状态；回归通过实际后续 NOOP 核对会话仍可用。
  Windows 模块化旧 AuthLine 的失败发生在恢复/对端 transcript 阶段，日志没有证实
  该次失败也进入 FAILED，因此不把所有平台/入口的旧失败路径都描述成同一状态变化。
- 已成功输出的片段可留下旧线程错误，原消息预算超限和后续无错误 sink 拒绝会错误
  沿用它。预算算术溢出与硬上限分开处理，超限明确设置 RANGE/LIMIT；每次 sink 调用
  临时取走之前的错误，仅失败时传播本次错误或生成 IO/CALLBACK。成功没有新错误时
  恢复之前错误，成功回调自己设置的新错误仍保留。特定 sink MEMORY 错误身份、
  失败不发布长度、关闭部分响应以及正常完整读取/复用的控制全部保留。
  源码层每片段增加一次 TakeError，冷路径处理之前错误的所有权；不声称零开销。
- 单项 LIST/UIDL 原来接受请求 1 却回复 2 的成功行。现先解析到局部结果，核对序号
  再发布；错配返回协议错误、保留调用方输出和完整 LastReply，并使会话失败/中止。
  保留既有解析器对输出/输入重叠的拒绝语义；正常匹配仍能继续 NOOP。公开声明与
  指南补充这些状态/错误规则，同步生成实现和声明单头，公开 ABI 不变。
- 共用十一场景回归注册在 feature=null 的独立契约根，模块化和新增单头入口复用
  同一真实网络夹具；最终最小根八程序全部通过。每次场景后退休真实 peer/Client，
  最终 Engine TryStop/join 后检查 LiveObjects==0、WaitErrors==0 和 allocator 归零。
- 生产 xpop3 根原来显式依赖回环测试和范例，带入 RSA 身份/私钥签名与 String。
  现只依赖五个自有功能根，保留原协议 smoke 与产品单头，完整回归留在 xpop3_tests。
  两平台生产源集合均从 159 减至 152；已有完整测试的 source/features/links 及十九
  程序集合逐项证明不变，未删除有效测试。产品单头新增相应裁剪断言并在四环境刷新。
  TLS_SERVER 同时来自 Core tls_stream 的既有 client/server 依赖，本批没有移除它，
  也不以放宽断言或更改 Core 模块设计来声称已移除。
- 完整选择 Windows GCC / Linux GCC / Clang ASan+UBSan+float-cast-overflow+leaks /
  TCC 各十九程序通过；双平台另跑四个显式范例根，均通过，无账号或外部 POP3 服务。
  五最小根各六程序、最终契约根八程序及产品根裁剪后的四个单头刷新，总计 108 次
  普通程序运行。不是一次在生产根清理后重新执行十九程序；使用不变闭包证明和受影响
  产品入口刷新补齐证据。已有 TLS/STLS、OOM、取消和退休断言保留。
- 双平台 static/shared 四个消费者新增借用 Reply、owned 命令跨库释放和同一 Core
  错误通道检查；各核对全部四十七个公开导出。两 profile 的三种产物共十二个体积
  产物通过，新增可复用符号禁止规则并额外核对全部产物。生产依赖清理后重新刷新
  包、裁剪、体积和有限协议 smoke；不据协议 smoke 声称流式吞吐没有回退。
- 双平台另运行 GCC 独立 Python TLS 对端，每平台十二个 POP3 TLS/STLS 场景，共
  二十四场景，包括 DNS/SNI、IPv4/IPv6、身份拒绝、正文截断及缺少 close_notify；
  工具整体成功，保留每次真实客户端执行与二进制指纹，不把预期失败场景算成成功取信。
- 五项元数据门禁与八项 layout/single 工具测试通过；最终 1693 项构建/支持输入
  Windows/WSL 归一化一致。体积工具绑定原始字节，故两平台原始指纹不同；逐项核对
  它的 707 项物理输入，只有 features.h 行尾不同，LF 归一化后相同。各报告保留实际
  原始指纹，未借用另一平台指纹重新标注旧产物。既存无关 Core 测试差异继续保留两份。
- 本地四个自有 C 和四十七 API 的有界复核候选已处理，仍须本批不可变提交的远端 CI
  才能计入完成。未本地认证 POP3 macOS/BSD/Android/MSVC/clang-cl/x86、TSan、长稳或
  外部服务；整轮保持 5/21，不自动合并或发布。

证据：xpop3-contract-before-{provenance,windows,linux}.json、
xpop3-contract-before-{inputs,fixtures}/ 及原二进制/日志；
xpop3-contract-{final,round}-{windows,linux}.json、xpop3-boundary-{windows,linux}.json、
xpop3-production-{root-before,boundary-proof}.json、
xpop3-contract-final-{package,exports,symbols}-{windows,linux}.json、
xpop3-contract-{size,performance}-{windows,linux}.json、
xpop3-independent-tls-{win32,linux}-execution.json、xpop3-size-raw-input-differences.json、
xpop3-contract-round-{checks,input-audit,selected-assets}.json、
xpop3-session-final-{source-review,validation}.json。
