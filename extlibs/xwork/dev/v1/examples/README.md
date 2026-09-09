# examples

本目录保存可运行的 `xwork` 集成范例。每个范例都对应 `docs/case/` 下的正式解析文档。

## 构建约定

命令默认从仓库根目录执行。示例使用 `examples/xwork_example_runtime.c` 作为示例专用聚合单元，它会启用随仓库提供的 `xrt`、`xllm-session` 和 `xllm-memory` 实现，然后包含 `xwork.c`。

Windows 下通常需要链接：

```powershell
-lws2_32 -liphlpapi
```

`-std=c11 -Wall -Wextra -pedantic` 下，`lib/xrt.h`、`lib/xllm-memory.h` 和 `lib/sqlite/sqlite3.c` 可能产生既有 pedantic/unused 警告；只要命令退出码为 0，示例构建即通过。

## 最小 xwork 程序

[first_xwork_program.c](first_xwork_program.c) 展示最小 `runtime -> workspace -> run` 生命周期。

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\first_xwork_program.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\first_xwork_program.exe -lws2_32 -liphlpapi
examples\first_xwork_program.exe
```

预期输出：无标准输出，退出码为 0。

## AI IDE Agent

[ai_ide_agent.c](ai_ide_agent.c) 展示 `xcode` profile 下的 AI IDE 集成基线。

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\ai_ide_agent.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\ai_ide_agent.exe -lws2_32 -liphlpapi
examples\ai_ide_agent.exe
```

覆盖能力：filesystem 读取、file-content artifact、dry-run patch artifact、mock xllm tool call、approval pause/resume、final report artifact。

预期输出：AI IDE agent 完成摘要，包含 run 状态和 artifact 数量，退出码为 0。

## claw Autonomous Agent

[claw_autonomous_agent.c](claw_autonomous_agent.c) 展示 `xclaw` profile 下的自主 Agent 基线。

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\claw_autonomous_agent.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\claw_autonomous_agent.exe -lws2_32 -liphlpapi
examples\claw_autonomous_agent.exe
```

覆盖能力：`process.exec`、command artifact、final report、file persistence、run recovery。

预期输出：claw autonomous agent 完成摘要，包含原 run 和 recovered run 的状态信息，退出码为 0。

## Multi-Agent claw

[multi_agent_claw.c](multi_agent_claw.c) 展示 `xclaw` profile 与 in-process multi-agent scheduler。

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\multi_agent_claw.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\multi_agent_claw.exe -lws2_32 -liphlpapi
examples\multi_agent_claw.exe
```

覆盖能力：agent pool、task graph、fan-out/fan-in、child run report、graph persistence、child run index query、graph recovery。

预期输出：multi-agent graph 完成摘要，包含 task 数量、child run index 查询结果和恢复结果，退出码为 0。

## Remote Worker Agent

[remote_worker_agent.c](remote_worker_agent.c) 展示 remote worker/control plane 基线。

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\remote_worker_agent.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\remote_worker_agent.exe -lws2_32 -liphlpapi
examples\remote_worker_agent.exe
```

覆盖能力：control plane、worker register/heartbeat、assignment、local worker `process.exec`、artifact/output chunk、snapshot recovery、orphaned assignment。

预期输出：remote worker/control plane 完成摘要，包含 task 状态、worker 状态、chunk/artifact 或恢复信息，退出码为 0。

## Replay Agent Run

[replay_agent_run.c](replay_agent_run.c) 展示 deterministic replay cassette 基线。

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\replay_agent_run.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\replay_agent_run.exe -lws2_32 -liphlpapi
examples\replay_agent_run.exe
```

覆盖能力：record entry、typed filesystem snapshot/ref、strict replay、checkpoint seek、audit divergence、divergence report artifact。

预期输出：replay 完成摘要，包含 strict replay、audit divergence 和 report artifact 信息，退出码为 0。

## 示例持久化目录

部分示例会在 `examples/` 下生成 `.xwork_*_store` 目录：

| 目录 | 来源示例 | 用途 |
| --- | --- | --- |
| `.xwork_claw_store` | `claw_autonomous_agent.c` | 保存 run snapshot、event、artifact 和恢复数据。 |
| `.xwork_multi_agent_claw_store` | `multi_agent_claw.c` | 保存 agent pool、task graph、child run 和 run index。 |
| `.xwork_remote_worker_store` | `remote_worker_agent.c` | 保存 control plane、worker、task、assignment、artifact/output chunk。 |

这些目录是本地示例运行产物，可在不需要保留审计数据时删除。
