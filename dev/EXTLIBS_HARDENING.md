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

下表数量是该产品清单内去重的资产数量，未包含依赖库的测试，也不表示本机已执行全部资产。
`tests` 包括专门的在线或平台测试，执行时需要依其声明与支持范围选择。

| 产品目录 | 实现模块 | 源文件 | tests | single_tests | examples | 本轮状态 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| xacme | 14 | 16 | 28 | 7 | 1 | core/JOSE、EC 导入和解码 DER 擦除修复通过，RSA/写侧临时数据及其余源码继续 |
| xhttp | 115 | 124 | 236 | 115 | 82 | 待复核 |
| xhttp-client | 18 | 18 | 97 | 32 | 12 | 待复核 |
| xhttp-policy | 27 | 22 | 49 | 19 | 15 | 待复核 |
| xhttp-server | 25 | 31 | 90 | 32 | 25 | 待复核 |
| ximap | 11 | 9 | 22 | 15 | 5 | 待复核 |
| xjwt | 1 | 4 | 3 | 2 | 1 | 已完成本轮：源码、本地、双平台认证 CI 及交付验收 |
| xllm | 1 | 12 | 3 | 1 | 2 | 回放/UTF-8 修复及包/裁剪/profile 通过，源码复核继续 |
| xllm-session | 1 | 8 | 2 | 1 | 0 | 恢复/终值修复及包/裁剪/profile 通过，源码复核继续 |
| xmail | 19 | 19 | 27 | 18 | 14 | 待复核 |
| xoauth2 | 1 | 3 | 3 | 2 | 4 | 已完成本轮：源码、本地、双平台认证 CI、Hydra 及交付验收 |
| xpop3 | 6 | 4 | 10 | 6 | 4 | 待复核 |
| xrudp | 19 | 20 | 76 | 97 | 0 | 待复核 |
| xruntime | 36 | 23 | 87 | 42 | 30 | 待复核 |
| xsmtp | 6 | 4 | 8 | 6 | 4 | 待复核 |
| xssh | 66 | 65 | 71 | 66 | 65 | 待复核 |
| xwork | 1 | 8 | 1 | 1 | 0 | 路径/只读隔离修复通过，工具状态机复核中 |
| xws | 10 | 5 | 22 | 6 | 7 | 待复核 |
| xws-client | 5 | 3 | 7 | 5 | 3 | 待复核 |
| xws-group | 3 | 3 | 4 | 3 | 2 | 待复核 |
| xws-server | 6 | 5 | 25 | 13 | 4 | 待复核 |

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
