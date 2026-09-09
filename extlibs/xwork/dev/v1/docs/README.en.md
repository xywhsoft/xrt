#xwork Document Center

> Formal documentation entry for xwork users. Development plans, history and internal design information are retained in `dev/`.

## Quick entry

- [API Reference Index](api/README.md)
- [English documentation center](README.en.md)
- [Tutorial Index](guide/README.md)
- [Examples Index](case/README.md)
- [Architecture](ARCHITECTURE.md)
- [Best Practices](BEST_PRACTICES.md)
- [FAQ](FAQ.md)
- [Migration Guide](MIGRATION.md)
- [Integration and Packaging](INTEGRATION.md)
- [Documentation Review Checklist](DOCS_REVIEW_CHECKLIST.md)
- [Repository Examples](../examples/README.md)
- [Root README](../README.md)

## Read by target

| Objectives | Suggested Reading |
| --- | --- |
| First time understanding xwork | [First xwork Program](guide/first-xwork-program.md) |
| Do AI IDE integration | [xllm Orchestration and Tool Loop](guide/xllm-orchestrator-intro.md), [AI IDE Agent Example](case/ai-ide-agent.md) |
| Make claw / Autonomous Agent | [Tools, Approval, and Artifacts](guide/tool-approval-artifact-intro.md), [claw Autonomous Agent Example](case/claw-autonomous-agent.md) |
| Do long Agent scheduling | [Multi-Agent Task Graph](guide/multi-agent-intro.md), [Multi-Agent claw Example](case/multi-agent-claw.md) |
| Do remote execution | [Remote Worker and Control Plane](guide/remote-worker-intro.md), [Remote Worker Example](case/remote-worker-agent.md) |
| Make recoverable and replay | [Persistence, Checkpoints, and Replay](guide/persistence-replay-intro.md), [Replay Example](case/replay-agent-run.md) |
| Check public interface | [API Reference Index](api/README.md), [`xwork.h`](../xwork.h) |
| Understand architectural trade-offs | [Architecture](ARCHITECTURE.md), [FAQ](FAQ.md) |
| Access to the build system | [Integration and Packaging](INTEGRATION.md) |

## Document partition

| Partition | Content |
| --- | --- |
| `docs/api/` | Modular description of the public API, organized by usage model. |
| `docs/guide/` | Tutorials and concept guides explaining how to integrate xwork into products. |
| |
| `docs/*.md` | Architecture, best practices, FAQs, migration and integration instructions. |
| `dev/` | Design documentation, development spec, history tracking and internal contracts. |

## Documentation maturity

| Status | Description |
| --- | --- |
| Chinese first draft | Generated according to the current API and examples, waiting for manual review. |
| Awaiting review | The content structure is stable but requires manual confirmation of wording, examples, and boundaries. |
| Stable | Can be used as the main source for translating English documents and publishing documents. |

The current official document defaults to:

- [`xwork.h`](../xwork.h) in the current warehouse.
- `XWORK_PERSISTENCE_FORMAT_VERSION` Current value.
- `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT` Current value.
- Current runnable examples in `examples/` and `tests/`.

When a document problem is discovered, priority is given to determining whether it belongs to:

- API name or field mismatch.
- The link or path is broken.
- The code snippet does not reflect the current API.
- Official documentation conflicts with `dev/` internal contract facts.

## Writing convention

- Chinese documents will be published first, and English documents will be generated after the Chinese main draft has been reviewed.
- API documentation focuses on object ownership, lifecycle, error returns, thread boundaries, and recovery boundaries.
- The tutorial document gives priority to explaining the minimum calling sequence, and then explains the extensibility points.
- The sample document must correspond to the executable code under `examples/`.
- Development status, plans and unstabilized internal details are not put into the official documentation center and remain in `dev/`.
- When adding a new public API, example, persistence format, remote protocol or built-in host tool, the corresponding official documents must be updated simultaneously.
