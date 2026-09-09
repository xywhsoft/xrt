# Local Host API

> 状态：中文初稿，待审阅。

Local Host API 是 xwork 提供的本地 host service helper，用于把 filesystem、process、terminal、vcs 和 editor-buffer 能力挂到 runtime 的 `xwork_host_services`。

## 相关声明

- `xwork_local_host_options`
- `xwork_local_host`
- `xwork_local_host_options_init()`
- `xwork_local_host_init()`
- `xwork_local_host_configure_services()`
- `xwork_local_host_reset()`
- `xwork_host_services`
- `xwork_host_services_init()`

## 模块定位

local host 是本地进程内 helper。它适合 examples、smoke、AI IDE/claw 本地执行基线。生产产品仍可以实现自己的 host services，只要遵守相同 JSON contract、policy 和 artifact 语义。

## 配置字段

| 字段 | 说明 |
| --- | --- |
| `sDefaultWorkingDirectory` | 默认工作目录。 |
| `bEnforceFilesystemRoot` | 是否强制 path 留在 root 下。 |
| `psFilesystemAllowPathPrefixes` | 文件路径 allow prefix。 |
| `psFilesystemDenyPathPrefixes` | 文件路径 deny prefix。 |
| `psCommandAllowPatterns` | 命令 allow pattern。 |
| `psCommandDenyPatterns` | 命令 deny pattern。 |
| `bDenyDestructiveCommands` | spawn 前拒绝破坏性命令。 |
| `iMaxReadBytes` | 文件读取上限。 |
| `iMaxProcessInputBytes` | stdin_text 上限。 |
| `iMaxProcessEnvEntries` | env 条目上限。 |
| `iMaxProcessOutputBytes` | process output 上限。 |

能力开关包括 filesystem read/write、process exec、VCS status/diff/log/branch 和 editor buffers。

## 最小配置

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
tOptions.bEnableVcsStatus = true;

status = xwork_local_host_configure_services(&tHost, &tOptions, &tServices);
```

`xwork_runtime_options::pHostServices` 会按值复制 `tServices`，但 `tHost` 是 callback user data 的实际拥有者，必须活到 runtime 不再调用 host service。

## 逐函数说明

### xwork_host_service_init

初始化单个 host service。

**功能：**

将 `xwork_host_service` 清零，准备填充 operation callback、user data 和能力描述。

**函数原型：**

```c
XWORK_API void xwork_host_service_init(xwork_host_service *pService);
```

**参数：**

- `pService`：要初始化的 service；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源；callback 和 user data 均由调用方管理。

**补充说明：**

生产宿主可以不用 local host，直接填充自己的 `xwork_host_service`。

**范例代码：**

```c
xwork_host_service service;
xwork_host_service_init(&service);
```

**相关 API：**

- `xwork_host_services_init`

---

### xwork_host_services_init

初始化 host services 集合。

**功能：**

将 `xwork_host_services` 中所有 host service 槽位初始化为空。

**函数原型：**

```c
XWORK_API void xwork_host_services_init(xwork_host_services *pServices);
```

**参数：**

- `pServices`：要初始化的 services；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源；services 结构体由调用方拥有。

**补充说明：**

传给 runtime options 后，runtime 按值复制 services；callback user data 的生命周期仍由调用方保证。

**范例代码：**

```c
xwork_host_services services;
xwork_host_services_init(&services);
```

**相关 API：**

- `xwork_local_host_configure_services`

---

### xwork_local_host_options_init

初始化 local host options。

**功能：**

设置本地 host service helper 的默认安全和能力配置。

**函数原型：**

```c
XWORK_API void xwork_local_host_options_init(xwork_local_host_options *pOptions);
```

**参数：**

- `pOptions`：要初始化的 options；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

调用 `xwork_local_host_configure_services` 前应显式启用需要暴露的 filesystem、process、terminal、vcs 或 editor-buffer 能力。

**范例代码：**

```c
xwork_local_host_options opts;
xwork_local_host_options_init(&opts);
opts.bEnableProcessExec = true;
```

**相关 API：**

- `xwork_local_host_configure_services`

---

### xwork_local_host_init

初始化 local host helper。

**功能：**

准备一个调用方持有的 `xwork_local_host`，用于承载 host service callback user data。

**函数原型：**

```c
XWORK_API void xwork_local_host_init(xwork_local_host *pHost);
```

**参数：**

- `pHost`：要初始化的 host；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配长期资源；host 结构体由调用方拥有。

**补充说明：**

配置到 runtime 后，`pHost` 必须活到 runtime 不再调用 host service。

**范例代码：**

```c
xwork_local_host host;
xwork_local_host_init(&host);
```

**相关 API：**

- `xwork_local_host_reset`

---

### xwork_local_host_reset

释放 local host helper。

**功能：**

释放 local host 内部复制的路径、allow/deny 列表、terminal/session 状态等资源。

**函数原型：**

```c
XWORK_API void xwork_local_host_reset(xwork_local_host *pHost);
```

**参数：**

- `pHost`：要释放的 host；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放内部资源，不释放结构体本身。

**补充说明：**

不要在 runtime 仍可能调用该 host service 时 reset。

**范例代码：**

```c
xwork_local_host_reset(&host);
```

**相关 API：**

- `xwork_local_host_init`

---

### xwork_local_host_configure_services

配置 local host services。

**功能：**

根据 local host options 初始化 host helper，并把 filesystem/process/terminal/vcs/editor-buffer callback 挂到 `xwork_host_services`。

**函数原型：**

```c
XWORK_API xwork_status xwork_local_host_configure_services(
    xwork_local_host *pHost,
    const xwork_local_host_options *pOptions,
    xwork_host_services *pServices
);
```

**参数：**

- `pHost`：调用方拥有的 local host helper。
- `pOptions`：配置参数。
- `pServices`：输出 services；调用前应 init。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

`pHost` 保存配置副本并作为 callback user data；`pServices` 不拥有 `pHost`。

**补充说明：**

local `process.exec` 走 xrt subprocess。terminal session 是 live local resource，不会被 snapshot 自动恢复。

**范例代码：**

```c
xwork_local_host_configure_services(&host, &opts, &services);
```

**相关 API：**

- `xwork_runtime_create`

---

### xwork_host_invoke_context_should_cancel

检查 host service 调用是否应取消。

**功能：**

让长时间运行的 host service 在阶段边界检查 cancel token 或 runtime 中断状态。

**函数原型：**

```c
XWORK_API bool xwork_host_invoke_context_should_cancel(
    const xwork_host_invoke_context *pContext,
    const char *sPhase
);
```

**参数：**

- `pContext`：host invoke context；可为 `NULL`。
- `sPhase`：可选阶段名，用于诊断。

**返回值：**

应取消时返回 `true`；否则返回 `false`。

**资源归属：**

不分配资源；不接管 `sPhase`。

**补充说明：**

host service 应在 spawn 前、I/O 循环、等待子进程、上传 chunk 等阶段主动检查。

**范例代码：**

```c
if (xwork_host_invoke_context_should_cancel(ctx, "before-spawn")) {
    return XWORK_ERROR_CANCELLED;
}
```

**相关 API：**

- `xwork_tool_exec_context_should_cancel`

---

## process.exec

local `process.exec` 走 xrt subprocess 路径，不依赖 shell `popen`。它支持：

- cwd override
- stdin_text
- env list
- timeout_ms
- timeout_stop：interrupt / terminate / kill / kill_tree
- allow_nonzero_exit
- merge_stderr
- include_events
- max_output_bytes
- terminal mode

结果包含 stdout/stderr、exit code、截断标记、I/O 统计、timeout/cancel/stop reason 和可选 ordered events。

## interactive terminal

local host 支持：

- `process.start_terminal`
- `process.list_terminals`
- `process.terminal_read`
- `process.terminal_write`
- `process.terminal_resize`
- `process.terminal_stop`

terminal session 是 live local resource，不会跨进程 restart 自动恢复。持久化 artifact 只保存 transcript/state/inventory 记录。

## 安全建议

- 默认开启 `bEnforceFilesystemRoot`。
- 对写入、delete、move、apply_patch 使用 dry-run 或审批。
- 对 process.exec 配置 allow/deny pattern。
- 对 destructive command 开启 `bDenyDestructiveCommands`。
- 网络能力默认 deny-by-default。

## 相关文档

- [Host Tools API](api-host-tools.md)
- [Policy / Approval API](api-policy-approval.md)
- [claw 自主 Agent 范例](../case/claw-autonomous-agent.md)
- [内部 host tool contract](../../dev/docs/HOST_TOOL_CONTRACTS.md)
