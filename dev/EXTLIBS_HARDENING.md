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
| xllm | 1 | 12 | 3 | 1 | 2 | TCC 兼容复核中 |
| xllm-session | 1 | 8 | 2 | 1 | 0 | 预算修复完成，生命周期复核中 |
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
- Linux/Clang ASan/UBSan/float-cast-overflow 初次运行发现独立的 routing-key 栈寿命缺陷，待下一个修复提交关闭。
- Linux/TCC 单头编译发现 `xllm_profile.c` 使用静态 const 变量初始化静态表，待下一批兼容修复。

主要证据：`session-before.log`、`session-single-before.log`、`session-after.log`、
`session-single-after.log`、`session-sanitize-after.log`、`session-tcc-after.log`，均位于 `out/extlibs-hardening/`。
