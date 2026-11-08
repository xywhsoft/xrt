# mdo 分支归并记录（2026-11-05）

本次按可观察行为归并三个历史分支。主线的模块化布局与 XRT 核心继续作为基线；
不把旧分支的整棵源码树或旧版核心覆盖回主线，也不使用空合并伪造功能覆盖。

## 历史分支与处置

| 分支 | 保留的历史提交 | 归并结果 |
| --- | --- | --- |
| `codex/mdo-tool-image-text` | `54676d1d5623e536829fb7370f51455923abd215` | 工具结果进入结构化图像消息时保留文字/path 部分；独立修复与回归夹具先行提交。 |
| `codex/mdo-request-tokens` | `0dc70638b0faa00b89a6cdb4e73eac35ad2213e3` | 迁入三库的完整产品行为，包含此分支继承的会话、调度、工具与恢复工作，并吸收当前 mdo SDK 的后续修复。 |
| `mdo-root-rename` | `6040abda647bd947bd561cd185f37a3751f122f4` | 归档旧 API 实现。产品 SDK 的 `xsRootRenameNoReplace` 已提供 rooted 文件/目录/最终链接的原子不覆盖移动，并禁止中间链接穿越；不可变结果使用 rooted hard-link 独占发布。无需恢复旧的 XRT 公共接口。 |

三个 tip 在删除分支前保留为 `archive/mdo/*-20261105` 标签。已提前清理的
`mdo-search-results`、`mdo-conversation-resume`、`mdo-event-sequence`、
`mdo-refactor-xrt`、`mdo-session-restore` 和 `schedule-run-now` 的历史均由
request-tokens tip 可达。保留历史不意味着这些提交成为主线的 Git 祖先；
这里的覆盖标准是功能与测试。

## 归并的功能边界

- xllm：三种 provider 协议、完整的 reasoning/signature/text 重放、媒体与受限结果、
  客户端配置/钩子快照、取消、绝对 deadline、重试预算及失败诊断。
- xllm-session：schema 4 快照与日志、旧版迁移、结构化文字/图像/raw 消息、
  上下文快照、服务端 usage 前缀锚点、压缩事务和失败回滚。
- xwork：ABI 9 / event schema 3、显式 runtime/agent/run 生命周期、不可变工具目录、
  retained callback owner、effects/权限资源、并行批次、任务与进程所有权、MCP、
  子 Agent、调度/run-now、事件序号、恢复 receipt、request token 和结果文件。
- 已完成的可恢复只读调用可使用验证过的 receipt；不确定的有副作用调用需要显式
  恢复决定。图像与文字共同保留，重复工具 ID 不得绕过账本一致性。
- 工具使用规范名称，旧名称在边界映射。`status` 只观察，不消耗主机输出游标或释放
  任务；等待、读取和释放分别由对应主机 API 控制。

功能源为 mdo 原 `deps.lock` 锁定的 XS `216f5445163ca30c815ad7d5d2da3b71cb2f6401`
及 `xwork-mcp.patch`（SHA-256
`76b8ef683e418c271c4eb22e7eda7dd2add5c085fa6d4a7bac53f7eea36b56c9`）的有效 SDK。
迁入前已验证原锁定的生产源码摘要、ABI 与 schema，避免丢掉历史分支之后的产品修复。

## 源码与兼容性

三库保持独立 `.c` 编译、公开依赖声明、feature guard 和生成的单头文件。
新增内部时间转换头把 Unix/monotonic 微秒记录映射为 XRT 的相对毫秒等待，
不改变核心等待接口。原主线 `xllmRequestSetTimeout` 的相对毫秒语义保留，
与绝对微秒 deadline 共存，预算在提交时确定并涵盖重试与退避。

xllm 升为 3.2.0，公开 request 记录新增相对超时字段；所有 native/SDK 消费者必须
成套重编。session schema 4 与 xwork ABI 9/event schema 3 保持当前产品约定。
具体说明见 `extlibs/xllm/docs/MIGRATION-3.2.md`。

`tools/export_agent_libraries.py` 从已提交的规范源码机械导出 XS/TCC 的扁平 SDK：
仅调整头文件包装、DLL 注解和本地 include 路径。宿主自己的 XRT/HTTP bridge 保留，
不会导入第二份核心实现。导出器的 `--check`、XS 校验器和 mdo 生产树摘要共同验证
实际来源。产品 native 核心继续使用原锁定核心及其已有兼容修复。

## 验证与范围

- Windows GCC：三库 41 个扩展翻译单元独立编译；标准三库门禁，包含原主线测试与
  迁入的单头/OOM/provider/TLS/事务/恢复夹具，全部通过。
- Linux GCC：相同的标准三库门禁通过。修复测试夹具在 LP64 上的 task ID 指针类型
  假设，使用 `uint64_t` 与 `memcpy`，生产任务记录无变更。
- Android NDK 30 / API 26 / ARM64：三库模块化测试程序严格编译和链接通过；
  此项是交叉编译验证，不代表设备运行或 APK 发布。
- 三库独立 feature 裁剪拒绝测试、API 文档覆盖和单头生成一致性通过。
- Windows 三库组合的 static/shared 发布包及真实消费者链接验证通过。
- MCP HTTP：12 类错误与恢复、拒绝不安全 replay，以及现代路由/旧 SSE 恢复/DELETE
  的本地服务器测试通过。
- mdo native host：媒体、不可覆盖发布、外部只读 Home、结果文件、schedule executor、
  MCP reliability 和真实客户端搜索重启七组产品测试通过。
  重启夹具显式配置个人本地模型；旧宿主也存在空模型目录夹具失败，
  不通过修改产品默认模型或账号边界绕过检查。规范搜索工具名与历史别名均严格计数。

最终 SDK 提交、bundle 摘要和 mdo 依赖树摘要记录在 mdo 的 `deps.lock` 与配套
`docs/build-records/2026-11-05-xrt-agent-mainline.md`。只合入本地主线；
不推送远端、不替换日常安装、不发布版本。
