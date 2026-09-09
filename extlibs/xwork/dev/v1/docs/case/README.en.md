# xwork example index

The sample document explains the goals, processes, key APIs, and extensibility points of programs executable under `examples/`.

English version: [xwork Examples](README.en.md)

## Current example

| Example | Description |
| --- | --- |
| [First xwork Program](first-xwork-program.md) | Minimum runtime/workspace/run life cycle. |
| [AI IDE Agent](ai-ide-agent.md) | Use the `xcode` profile to run through file reading, dry-run patch, approval, and final reporting. |
| [claw Autonomous Agent](claw-autonomous-agent.md) | Use the `xclaw` profile to run through autonomous process.exec, artifacts, persistence and recovery. |
| [Multi-Agent claw](multi-agent-claw.md) | Use agent pool and task graph to run through planner/coder/tester/reviewer fan-out/fan-in. |
| [Remote Worker Agent](remote-worker-agent.md) | Use control plane and worker loop to run through remote task claim/execute/complete/recovery. |
| [Replay Agent Run](replay-agent-run.md) | Record and replay model calls, tools, checkpoints, and filesystem refs, then generate a divergence report. |

## How to choose an example

| You want to verify | Recommended examples |
| --- | --- |
| Minimum runtime/workspace/run lifecycle | [First xwork Program](first-xwork-program.md) |
| AI IDE approval and patch UI | [AI IDE Agent](ai-ide-agent.md) |
| Autonomous command execution, artifacts and recovery | [claw Autonomous Agent](claw-autonomous-agent.md) |
| Multi-role task splitting and fan-out/fan-in | [Multi-Agent claw](multi-agent-claw.md) |
| worker distribution, lease and remote results | [Remote Worker Agent](remote-worker-agent.md) |
| replay, strict/audit and divergence | [Replay Agent Run](replay-agent-run.md) |

## Run entry

See [examples/README.md](../../examples/README.md) for the build command.
