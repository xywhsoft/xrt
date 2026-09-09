# xwork API 参考文档重写规格

本文件用于跟踪 `docs/api` 从模块概览升级为逐函数 API 参考的工作。

当前状态：

- `xwork.h` 唯一 `XWORK_API` 函数：`367`
- 已有逐函数三级标题覆盖：`367`
- 缺失逐函数三级标题覆盖：`0`

## 完成标准

- [x] 每个公开函数都有独立三级标题。
- [x] 每个函数小节包含功能、函数原型、参数、返回值、资源归属、补充说明、范例代码、相关 API。
- [x] 增加 `tools/check_api_reference_coverage.ps1`，用于统计 `xwork.h` 与 API 三级标题覆盖差距。
- [x] 完成 runtime、workspace、tools、run、policy、artifact、persistence、multi-agent、remote-worker、replay、local-host 和 shared helper 页面覆盖。

## 模块状态

| 模块 | 页面 | 状态 |
| --- | --- | --- |
| Common Types | `docs/api/types.md` | [x] |
| Runtime core | `docs/api/api-runtime.md` | [x] |
| Profiles | `docs/api/api-profiles.md` | [x] |
| xllm integration | `docs/api/api-xllm-integration.md` | [x] |
| Workspace | `docs/api/api-workspace.md` | [x] |
| Tools | `docs/api/api-tools.md` / `docs/api/api-host-tools.md` | [x] |
| Run / Event / Async / Query | `docs/api/api-run.md` / `docs/api/api-orchestrator.md` | [x] |
| Policy / Approval | `docs/api/api-policy-approval.md` | [x] |
| Artifacts | `docs/api/api-artifacts.md` | [x] |
| Persistence / Checkpoint | `docs/api/api-persistence.md` | [x] |
| Multi-Agent | `docs/api/api-multi-agent.md` | [x] |
| Remote Worker | `docs/api/api-remote-worker.md` | [x] |
| Replay | `docs/api/api-replay.md` | [x] |
| Local Host | `docs/api/api-local-host.md` | [x] |
