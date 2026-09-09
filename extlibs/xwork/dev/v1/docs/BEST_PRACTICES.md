# xwork 最佳实践

> 状态：中文初稿，待审阅。

本文给出 xwork 集成时的默认建议，重点覆盖安全、artifact、恢复和 profile 选择。

## Host Tool Policy 默认收紧

- 默认开启 filesystem root enforcement。
- 文件写入、move、delete、apply_patch 默认走审批或 dry-run。
- process.exec 配置 allow/deny pattern。
- destructive command 在 spawn 前拦截。
- network 默认 deny-by-default，只对明确 host allowlist 放行。
- remote worker 不能绕过 workspace path、network 和 capability policy。

## 文件与 Patch

- 优先使用 `filesystem.apply_patch` 的 dry-run 路径生成可审阅 artifact。
- UI 应展示 patch artifact 的 file/hunk/add/delete 统计。
- 大文件读取使用 `offset_bytes` 和 `max_bytes`。
- 写入前明确 `mode=overwrite/append/create`，不要依赖隐式默认。
- workspace 外路径应被 host policy 拒绝，而不是由工具执行器自行判断。

## Process 与 Terminal

- 对 `process.exec` 设置 `timeout_ms`。
- 对长任务设置 `timeout_stop`，明确 interrupt/terminate/kill/kill_tree 策略。
- 限制 `stdin_text`、env 条目数量和 output bytes。
- 需要交互时使用 terminal session 工具，不要把交互式命令塞进一次性 `process.exec`。
- 持久化 terminal artifact 只是审计记录，不能当作 live session handle。

## Artifact 设计

- 列表页使用 artifact summary，不默认加载完整内容。
- 对文件内容使用 output class `file-content`。
- 对文件变更使用 output class `file-change` 或 patch artifact。
- 对终端状态使用 `xwork.terminal_state.v1`。
- 对诊断使用 `xwork.diagnostics.v1`。
- 对 plan/progress/final 使用 `xwork.report.v1`。

## 恢复与幂等性

- 恢复前重新注册 runtime、workspace、tool、host services 和 profile。
- pending tool 恢复后可能重新执行，工具应尽量幂等。
- 对不可幂等副作用，使用 approval、checkpoint、replay 或调用方锁保护。
- READY/RUNNING/BLOCKED 的 multi-agent task 恢复为 PENDING 是正常行为。
- remote worker 的 in-flight assignment 恢复为 orphaned 后，应由产品决定 retry/cancel/manual inspect。

## Replay 组合建议

- CI 回归使用 strict replay。
- 安全审计使用 audit replay，收集更多 divergence。
- 高风险副作用在 record mode 下可启用 side-effect blocking。
- JSON payload 优先使用 normalized JSON hash，避免无意义空白导致误报。

## Profile 选择

- AI IDE 默认从 `xcode` profile 开始，再覆盖产品 UI 审批策略。
- CLI autonomous agent 默认从 `xclaw` profile 开始，再收紧网络和命令 policy。
- 不要把 profile 当作最终安全边界；profile 只是默认配置集合。

## 相关文档

- [Policy / Approval API](api/api-policy-approval.md)
- [Host Tools API](api/api-host-tools.md)
- [Artifact API](api/api-artifacts.md)
- [Replay API](api/api-replay.md)
