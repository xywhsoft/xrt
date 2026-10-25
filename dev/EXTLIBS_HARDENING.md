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
| xacme | 14 | 16 | 28 | 6 | 1 | 待复核 |
| xhttp | 115 | 124 | 236 | 115 | 82 | 待复核 |
| xhttp-client | 18 | 18 | 97 | 32 | 12 | 待复核 |
| xhttp-policy | 27 | 22 | 49 | 19 | 15 | 待复核 |
| xhttp-server | 25 | 31 | 90 | 32 | 25 | 待复核 |
| ximap | 11 | 9 | 22 | 15 | 5 | 待复核 |
| xjwt | 1 | 4 | 3 | 2 | 1 | 待复核 |
| xllm | 1 | 12 | 3 | 1 | 2 | TCC 兼容修复完成，继续复核 |
| xllm-session | 1 | 8 | 2 | 1 | 0 | 预算/摘要寿命修复完成，持久化复核中 |
| xmail | 19 | 19 | 27 | 18 | 14 | 待复核 |
| xoauth2 | 1 | 3 | 3 | 2 | 4 | 待复核 |
| xpop3 | 6 | 4 | 10 | 6 | 4 | 待复核 |
| xrudp | 19 | 20 | 76 | 97 | 0 | 待复核 |
| xruntime | 36 | 23 | 87 | 42 | 30 | 待复核 |
| xsmtp | 6 | 4 | 8 | 6 | 4 | 待复核 |
| xssh | 66 | 65 | 71 | 66 | 65 | 待复核 |
| xwork | 1 | 8 | 1 | 1 | 0 | 待复核 |
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
