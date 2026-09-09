# Local Host Tools

>Status: First draft in Chinese, awaiting review.

This tutorial shows how to enable xwork's local host service helper so that the built-in filesystem, process, terminal, vcs, and editor tools can be called by the orchestrator.

## Object relationship

```text
xwork_local_host_options
xwork_local_host
xwork_host_services
xwork_runtime_options::pHostServices
```

`xwork_runtime_options` copies `xwork_host_services` by value, but `xwork_local_host` is the actual owner of the callback user data and must live until the runtime no longer uses these services.

## Minimal configuration

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

## Common tool requests

Read file:

```json
{"path":"README.md","max_bytes":4096}
```

Execute command:

```json
{"command":"git status --short","cwd":"D:/git/project","timeout_ms":10000,"include_events":true}
```

Start the terminal:

```json
{"command":"powershell","session_name":"dev-shell","terminal_cols":120,"terminal_rows":30}
```

## Security recommendations

- Turn on `bEnforceFilesystemRoot`.
- Use approval or dry-run for write, move, delete, apply_patch.
- Configure allow/deny pattern on process.exec.
- Turn on `bDenyDestructiveCommands`.
- Limit stdin, env and output sizes.

## Next step

- [Host Tools API](../api/api-host-tools.md)
- [Local Host API](../api/api-local-host.md)
- [Tools, Approval, and Artifacts](tool-approval-artifact-intro.md)
