# xwork API 文档索引

> API 文档按使用心智模型组织，而不是按 `xwork.h` 的声明顺序机械展开。

English version: [xwork API Index](README.en.md)

## 当前公共 API 范围

`xwork.h` 已经覆盖以下核心对象：

| 模块 | 主要对象 / 能力 |
| --- | --- |
| 基础运行时 | `xwork_runtime`、状态码、版本、profile、xllm bootstrap。 |
| 工作区 | `xwork_workspace`、workspace memory sync、工作区 root 与策略边界。 |
| 工具系统 | `xwork_tool_def`、工具注册、工具执行器、host service bridge。 |
| Run 生命周期 | `xwork_run`、run state、step、event、summary、同步/异步执行。 |
| 编排器 | xllm model turn、tool loop、流式事件、取消、审批暂停/恢复。 |
| 审批与策略 | `xwork_approval_request`、autonomy mode、filesystem/process/network policy。 |
| Artifact | `xwork_artifact`、artifact summary、patch/report/command/output typed metadata。 |
| 持久化 | `xwork_persistence_backend`、run snapshot、checkpoint、event/artifact 查询。 |
| Multi-Agent | `xwork_agent_pool`、`xwork_task_graph`、task dependency、handoff、graph recovery。 |
| Remote Worker | `xwork_control_plane`、worker、remote task、lease、output/blob chunk。 |
| Replay | `xwork_replay_engine`、replay entry、manifest、filesystem ref、divergence report。 |

## API 阅读路径

1. 先读 `xwork.h` 顶部的公共契约说明，确认对象所有权、借用关系和线程安全边界。
2. 再按当前任务选择模块文档。
3. 编写集成代码时优先使用 `*_init()` 初始化 option/result 结构体，并使用对应 `*_reset()` 释放深拷贝结果。
4. 需要跨进程或跨版本保存状态时，确认 `XWORK_PERSISTENCE_FORMAT_VERSION` 和 `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT`。

错误码以 `xwork.h` 顶部 public contract 和 [通用类型与约定](types.md) 为准。模块页只补充该模块最常见的错误来源，不重新定义错误码语义。

## API 页面

基础页面：

- [API 页面模板](API_PAGE_TEMPLATE.md)
- [通用类型与约定](types.md)
- [Runtime API](api-runtime.md)
- [Workspace API](api-workspace.md)
- [Tool API](api-tools.md)
- [Run API](api-run.md)
- [Orchestrator API](api-orchestrator.md)

后续页面：

- [Policy / Approval API](api-policy-approval.md)
- [Artifact API](api-artifacts.md)
- [Persistence API](api-persistence.md)
- [Host Tools API](api-host-tools.md)
- [Profiles API](api-profiles.md)
- [Multi-Agent API](api-multi-agent.md)
- [Remote Worker API](api-remote-worker.md)
- [Replay API](api-replay.md)
- [xllm Integration API](api-xllm-integration.md)
- [Local Host API](api-local-host.md)

## API 文档编写标准

API 文档必须按 `D:\git\xllm\docs\api` 的粒度编写。不能只做模块概览，也不能只列函数名。

每个模块页至少包含：

- 常量、宏、枚举和结构体说明。
- 按功能分组的 API 目录。
- 每个公开 `XWORK_API` 函数的独立小节。
- 函数原型、参数、返回值、资源归属、补充说明和范例代码。
- 常见错误、相关 API、相关教程和相关案例。

每个函数小节至少包含：

- **功能**：这个函数解决什么问题，什么时候使用。
- **函数原型**：从 `xwork.h` 复制的准确 C 原型。
- **参数**：逐个解释输入/输出方向、是否可为 `NULL`、生命周期、所有权、单位、范围和默认值。
- **返回值**：成功/失败语义、错误码、是否可能产生部分结果。
- **资源归属**：谁分配、谁释放、用哪个 `reset` / `destroy` 函数清理。
- **补充说明**：调用顺序、线程安全、恢复边界、profile/workspace/host/replay 差异和兼容性注意事项。
- **范例代码**：尽量给出可直接学习的小代码段；复杂流程可链接到 `case/`。

只有当一个 API 页覆盖了分配给该模块的所有公开 `XWORK_API` 函数，并且每个函数都有上述内容时，才可以在 API 参考重写 spec 中标记完成。
