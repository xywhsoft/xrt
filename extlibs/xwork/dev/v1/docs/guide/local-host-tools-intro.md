# 本地 Host Tools

> 状态：中文初稿，待审阅。

本教程说明如何启用 xwork 的本地 host service helper，让内置 filesystem、process、terminal、vcs 和 editor 工具可被 orchestrator 调用。

## 对象关系

```text
xwork_local_host_options
xwork_local_host
xwork_host_services
xwork_runtime_options::pHostServices
```

`xwork_runtime_options` 会按值复制 `xwork_host_services`，但 `xwork_local_host` 是 callback user data 的实际拥有者，必须活到 runtime 不再使用这些 services。

## 最小配置

```c
xwork_local_host_options tHostOptions;
xwork_local_host tHost;
xwork_host_services tServices;
xwork_runtime_options tRuntime;

xwork_local_host_options_init(&tHostOptions);
xwork_local_host_init(&tHost);
xwork_host_services_init(&tServices);
xwork_runtime_options_init(&tRuntime);

tHostOptions.sDefaultWorkingDirectory = "D:/git/project";
tHostOptions.bEnforceFilesystemRoot = true;
tHostOptions.bEnableFilesystemReadText = true;
tHostOptions.bEnableFilesystemWriteText = true;
tHostOptions.bEnableProcessExec = true;
tHostOptions.bEnableVcsStatus = true;
tHostOptions.bDenyDestructiveCommands = true;

if (xwork_local_host_configure_services(&tHost, &tHostOptions, &tServices) != XWORK_OK) {
    return 1;
}

tRuntime.pHostServices = &tServices;
```

## 常见工具请求

读取文件：

```json
{"path":"README.md","max_bytes":4096}
```

执行命令：

```json
{"command":"git status --short","cwd":"D:/git/project","timeout_ms":10000,"include_events":true}
```

启动终端：

```json
{"command":"powershell","session_name":"dev-shell","terminal_cols":120,"terminal_rows":30}
```

## 安全建议

- 开启 `bEnforceFilesystemRoot`。
- 对写入、move、delete、apply_patch 使用审批或 dry-run。
- 对 process.exec 配置 allow/deny pattern。
- 开启 `bDenyDestructiveCommands`。
- 限制 stdin、env 和 output 大小。

## 下一步

- [Host Tools API](../api/api-host-tools.md)
- [Local Host API](../api/api-local-host.md)
- [工具、审批与 artifact](tool-approval-artifact-intro.md)
