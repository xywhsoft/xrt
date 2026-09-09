# Host Tools API

> 状态：中文初稿，待审阅。

Host Tools API 说明 xwork 内置工具和本地 host service 的边界。模型看到的是 tool id，xwork 执行时通过 host service 调用宿主提供的文件、进程、终端、VCS 或编辑器能力。

## 相关声明

- `xwork_host_service_kind`
- `xwork_host_service`
- `xwork_host_services`
- `xwork_host_invoke_context`
- `xwork_local_host_options`
- `xwork_local_host`
- `xwork_host_service_init()`
- `xwork_host_services_init()`
- `xwork_local_host_options_init()`
- `xwork_local_host_init()`
- `xwork_local_host_configure_services()`
- `xwork_local_host_reset()`
- `xwork_runtime_invoke_host_service()`
- `xwork_runtime_invoke_host_service_ex()`

## 模块定位

Host service 是 xwork 和宿主系统之间的副作用边界。xwork 定义 contract、policy gate、事件和 artifact；宿主负责实际访问文件系统、启动进程、管理终端、连接远程系统或编辑器。

## Host service kind

| Kind | 说明 |
| --- | --- |
| `XWORK_HOST_FILESYSTEM` | 文件读写、列表、stat、glob、mkdir、move、delete、apply_patch。 |
| `XWORK_HOST_PROCESS` | `process.exec` 和交互式 terminal session。 |
| `XWORK_HOST_VCS` | status、diff、log、branch。 |
| `XWORK_HOST_NETWORK` | 外部网络访问边界。 |
| `XWORK_HOST_DIAGNOSTICS` | 从进程输出等来源生成诊断。 |
| `XWORK_HOST_EDITOR` | 编辑器 buffer 桥接。 |

## 本地 host helper

```c
xwork_local_host_options tOptions;
xwork_local_host tHost;
xwork_host_services tServices;

xwork_local_host_options_init(&tOptions);
xwork_local_host_init(&tHost);
xwork_host_services_init(&tServices);

tOptions.sDefaultWorkingDirectory = "D:/git/project";
tOptions.bEnforceFilesystemRoot = true;
tOptions.bEnableFilesystemReadText = true;
tOptions.bEnableFilesystemWriteText = true;
tOptions.bEnableProcessExec = true;
tOptions.bDenyDestructiveCommands = true;

status = xwork_local_host_configure_services(&tHost, &tOptions, &tServices);
```

把 `tServices` 放入 `xwork_runtime_options::pHostServices` 后，runtime 会按值复制 service 表；`tHost` 本身必须活到 runtime 不再调用这些 services。

## 通用 JSON 规则

- 路径默认是 workspace-relative，除非 host 配置另有说明。
- 开启 `bEnforceFilesystemRoot` 后，filesystem/editor path 必须留在 `sDefaultWorkingDirectory` 下。
- 失败响应包含 `ok:false`、`error_kind` 和可选 `error`。
- 有界输出可返回 `truncated:true` 或分页 metadata。
- runtime 带 `pReplayEngine` 时，host service 可进入 record/strict/audit replay 路径。
- orchestrator 可以从成功的内置工具结果自动合成 artifact。

## Filesystem 工具

内置 filesystem 工具：

- `filesystem.read_text`
- `filesystem.write_text`
- `filesystem.list`
- `filesystem.stat`
- `filesystem.glob`
- `filesystem.mkdir`
- `filesystem.move`
- `filesystem.delete`
- `filesystem.apply_patch`

`filesystem.apply_patch` 的 v1 内置实现是单文件精确文本替换，使用 `old_text` / `new_text` / `dry_run`。

## Process 与 terminal

`process.exec` 支持：

- `command`
- `cwd`
- `stdin_text`
- `timeout_ms`
- `timeout_stop`
- `env`
- `max_output_bytes`
- `merge_stderr`
- `include_events`
- `use_terminal`
- `terminal_cols` / `terminal_rows`

结果包含 stdout/stderr、exit code、截断标记、I/O byte count、timeout/cancel/stop reason，并可返回 xrt subprocess events。

交互式 terminal session 工具：

- `process.start_terminal`
- `process.terminal_read`
- `process.terminal_write`
- `process.terminal_resize`
- `process.terminal_stop`
- `process.list_terminals`

terminal state 使用 `xwork.terminal_state.v1`，inventory 使用 `xwork.terminal_inventory.v1`。

## VCS 和 editor

VCS 工具当前面向只读查询：

- `vcs.status`
- `vcs.diff`
- `vcs.log`
- `vcs.branch`

editor 工具用于产品侧 buffer bridge：

- `editor.open_buffer`
- `editor.apply_edit`

## 安全边界

- filesystem root enforcement 不应被产品层绕过。
- process allow/deny patterns 和 destructive command 拦截应在 spawn 前执行。
- network 默认建议 deny-by-default。
- remote worker 执行同样需要经过 policy、workspace root 和 capability allowlist。

## 相关文档

- [Tool API](api-tools.md)
- [Policy / Approval API](api-policy-approval.md)
- [工具、审批与 artifact](../guide/tool-approval-artifact-intro.md)
- [内部 host tool contract](../../dev/docs/HOST_TOOL_CONTRACTS.md)
- [内部 host tool examples](../../dev/docs/HOST_TOOL_EXAMPLES.md)
