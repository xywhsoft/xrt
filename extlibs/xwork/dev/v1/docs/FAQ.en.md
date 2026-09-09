# xwork FAQ

>Status: First draft in Chinese, awaiting review.

## What is the boundary between xwork and xllm?

`xllm` is responsible for the model side: provider, request/response, stream, session, memory and tool call protocols.

`xwork` is responsible for the workflow side: workspace, run, tool execution, approval, artifact, checkpoint, persistence, multi-agent, remote worker and replay.

## Is xwork a complete Agent product?

no. xwork is a common library and runtime infrastructure for Agent development. UI, CLI, cloud services, user accounts, deployment, product strategy, and branded experiences belong to upper-level products, such as AI IDE or claw.

## Why doesn't remote worker have built-in network services?

The production remote worker needs to handle socket lifecycle, authentication, signing, mTLS, tenant/project isolation, retries, throttling, blob streaming, and deployment topology. These strategies are strongly product dependent. xwork only defines control-plane objects and decoded-message transport boundaries to avoid binding the library to a certain cloud architecture.

## Why does recovery not restore the live process/terminal?

OS process handles, terminal sessions, thread stacks, and callback stacks cannot be reliably serialized. xwork persists snapshot, event, artifact and replay cassette. After recovery, the product should rediscover or restart the live resources.

## Can xwork directly replace the tool system in the product?

Shared semantic parts can be replaced incrementally: tool definition, approval, host service contract, artifact, and persistence. Product specific UI, permissions, remote network and editor bridge should remain at the product layer, accessed through xwork host services.

## Does xwork require built-in file persistence?

Not required. Built-in file backend suitable for local durable runs, examples and smoke. The product can implement `xwork_persistence_backend` to connect to its own database or object storage.

## Does xwork have a built-in planner?

There is no built-in complete autonomous planner. xwork provides planner boundary, plan report artifact, tool choice and task graph import capabilities. The real planning strategy is implemented by the product or model layer.

## Related documents

- [Architecture](ARCHITECTURE.md)
- [Best Practices](BEST_PRACTICES.md)
- [Migration Guide](MIGRATION.md)
