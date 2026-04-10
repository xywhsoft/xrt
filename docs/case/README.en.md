# XRT Case Studies

> End-to-end examples for readers who want to see how multiple XRT modules fit together into one complete program. `guide/` explains the learning path; `case/` shows the assembled result.

[Back to Docs Hub](../README.en.md)

---

## Mainline Must-Read Cases

For a first systematic pass through XRT cases, read in this order:

1. [Configuration System with `xvalue + json`](json-config-system.en.md)
2. [Rendering HTML with Template](template-render-html.en.md)
3. [Queue + Future Multi-Producer Worker](queue-worker-future.en.md)
4. [Subprocess + Async File Tooling Pipeline](subprocess-file-async-pipeline.en.md)
5. [XHTTP Client Chain with URL, Proxy, and TLS](xhttp-client-proxy-tls.en.md)
6. [XWS Session Skeleton with Queue and Coroutine](xws-session-queue-coroutine.en.md)
7. [Thread, Coroutine, and Future Coordination](thread-coroutine-future.en.md)
8. [Minimal HTTP Service with XRT](minimal-http-service.en.md)
9. [Streaming LLM API with XRT](streaming-llm-api.en.md)
10. [Full HTTP + JSON + Template Service Chain](http-json-template-chain.en.md)
11. [Signed Rule Bundle Import with Charset, Regex, and Crypto](signed-rule-bundle.en.md)
12. [Session Registry with `mempool + avltree + list`](session-registry-pool-index.en.md)
13. [A Local Console Service that Combines Config, Logging, Tasks, Networking, and Templates](local-console-service.en.md)


## Representative Extension Cases

After `local-console-service.en.md`, pick the one extension that matches your current problem. You do not need to read dozens of near-duplicate variants in sequence.

1. [Upgrade the Local Console Service into a Subprocess Probe Dashboard](subprocess-probe-dashboard.en.md)
	Use this when you need one formal background chain for external CLI execution, worker scheduling, futures, and page output.
2. [Upgrade the Local Console Service into a Hot-Cold Tier Dashboard](hot-cold-tier-dashboard.en.md)
	Use this when you need a clean split between hot state, cold state, page output, and file snapshots.
3. [Upgrade the Local Console Service into a Cross-Media Orchestration Dashboard](cross-media-orchestration-dashboard.en.md)
	Use this when recovery must switch between local files, bundles, and external tool chains in one formal flow.


## Reading Advice

- Follow the mainline until `local-console-service.en.md` to establish the full application skeleton.
- Then add only the extension case closest to your real problem.
- Use [API Index](../api/README.en.md) when you need exact function and type details.
- Use [Guide Entry](../guide/README.en.md) when you need the conceptual explanation behind a design choice.
