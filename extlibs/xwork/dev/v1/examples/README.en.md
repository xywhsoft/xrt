# examples

This directory holds runnable `xwork` integration examples. Each example corresponds to the official parsing document under `docs/case/`.

## Build conventions

The command is executed from the repository root directory by default. The example uses `examples/xwork_example_runtime.c` as the example-specific aggregation unit, which enables the `xrt`, `xllm-session`, and `xllm-memory` implementations provided with the warehouse, and then includes `xwork.c`.

Under Windows, a link is usually required:

```powershell
-lws2_32 -liphlpapi
```

Under `-std=c11 -Wall -Wextra -pedantic`, `lib/xrt.h`, `lib/xllm-memory.h`, and `lib/sqlite/sqlite3.c` may generate existing pedantic/unused warnings; as long as the command exit code is 0, the example build passes.

## Minimal xwork program

[first_xwork_program.c](first_xwork_program.c) exhibits the minimal `runtime -> workspace -> run` lifecycle.

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\first_xwork_program.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\first_xwork_program.exe -lws2_32 -liphlpapi
examples\first_xwork_program.exe
```

Expected output: No standard output, exit code 0.

## AI IDE Agent

[ai_ide_agent.c](ai_ide_agent.c) shows the AI ​​IDE integration baseline under the `xcode` profile.

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\ai_ide_agent.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\ai_ide_agent.exe -lws2_32 -liphlpapi
examples\ai_ide_agent.exe
```

Coverage capabilities: filesystem reading, file-content artifact, dry-run patch artifact, mock xllm tool call, approval pause/resume, final report artifact.

Expected output: AI IDE agent completion summary, including run status and artifact count, with exit code 0.

## claw Autonomous Agent

[claw_autonomous_agent.c](claw_autonomous_agent.c) displays the autonomous Agent baseline under the `xclaw` profile.

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\claw_autonomous_agent.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\claw_autonomous_agent.exe -lws2_32 -liphlpapi
examples\claw_autonomous_agent.exe
```

Coverage capabilities: `process.exec`, command artifact, final report, file persistence, run recovery.

Expected output: claw autonomous agent completion summary, including status information of the original run and recovered run, with an exit code of 0.

## Multi-Agent claw

[multi_agent_claw.c](multi_agent_claw.c) shows the `xclaw` profile with the in-process multi-agent scheduler.

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\multi_agent_claw.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\multi_agent_claw.exe -lws2_32 -liphlpapi
examples\multi_agent_claw.exe
```

Coverage capabilities: agent pool, task graph, fan-out/fan-in, child run report, graph persistence, child run index query, graph recovery.

Expected output: multi-agent graph completion summary, including task number, child run index query results, and recovery results, with exit code 0.

## Remote Worker Agent

[remote_worker_agent.c](remote_worker_agent.c) shows the remote worker/control plane baseline.

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\remote_worker_agent.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\remote_worker_agent.exe -lws2_32 -liphlpapi
examples\remote_worker_agent.exe
```

Coverage capabilities: control plane, worker register/heartbeat, assignment, local worker `process.exec`, artifact/output chunk, snapshot recovery, orphaned assignment.

Expected output: remote worker/control plane completion summary, including task status, worker status, chunk/artifact or recovery information, exit code 0.

## Replay Agent Run

[replay_agent_run.c](replay_agent_run.c) shows the deterministic replay cassette baseline.

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\replay_agent_run.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\replay_agent_run.exe -lws2_32 -liphlpapi
examples\replay_agent_run.exe
```

Coverage capabilities: record entry, typed filesystem snapshot/ref, strict replay, checkpoint seek, audit divergence, divergence report artifact.

Expected output: replay completion summary, including strict replay, audit divergence, and report artifact information, with exit code 0.

## Example persistence directory

Some examples will generate the `.xwork_*_store` directory under `examples/`:

| Table of contents | Source examples | Purpose |
| --- | --- | --- |
| `.xwork_claw_store` | `claw_autonomous_agent.c` | Save run snapshot, event, artifact, and recovery data. |
| `.xwork_multi_agent_claw_store` | `multi_agent_claw.c` | Save the agent pool, task graph, child run, and run index. |
| `.xwork_remote_worker_store` | `remote_worker_agent.c` | Save control plane, worker, task, assignment, artifact/output chunk. |

These directories are artifacts of local sample runs and can be deleted when there is no need to retain audit data.
