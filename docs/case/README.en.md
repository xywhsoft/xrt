# XRT Case Studies

> Entry pages for complete chains, combined capability usage, and real engineering slices. `docs/guide/` focuses on teaching order; `docs/case/` focuses on how multiple modules solve one complete problem together.

[Back to Docs Hub](../README.en.md)

---

## Contents

- [Purpose of the Case Studies](#purpose-of-the-case-studies)
- [Current Status](#current-status)
- [Reading Layers](#reading-layers)
- [Mainline Must-Read Cases](#mainline-must-read-cases)
- [Advanced Combined Cases](#advanced-combined-cases)
- [Advanced Control-Plane Series](#advanced-control-plane-series)
- [Current Gaps](#current-gaps)
- [Relationship to API and Guide Docs](#relationship-to-api-and-guide-docs)

---

## Purpose of the Case Studies

The `case/` directory is intended for:

- showing how one complete problem is solved with XRT
- showing how multiple subsystems cooperate
- showing the full chain from the runtime up through networking, concurrency, templating, JSON, and AI-facing usage

Formal case pages should cover at least:

- scenario background
- goals and constraints
- module selection and why other options are not chosen
- key code structure
- run steps
- common failure points
- directions for extension


## Current Status

The `case/` tree now has many rebuilt pages, but the old index structure had drifted into a problem:

- the early pages were still building a broad teaching ladder
- the later pages gradually collapsed into one long local-console-service control-plane chain
- the result was more pages, but a weaker mainline

That long chain is still valuable, but it should no longer pretend to be the one default ladder every reader is expected to finish from top to bottom.

So the case entry is now explicitly split into 3 layers:

- `Mainline must-read cases`: the first route most readers should follow
- `Advanced combined cases`: domain-oriented extensions after the mainline
- `Advanced control-plane series`: heavier pages about recovery, consensus, snapshots, leases, and journals

If you want the staged teaching plan behind this regrouping, read together with:

- [Guide Rebuild Roadmap](../guide/ROADMAP.en.md)


## Reading Layers

### 1. Mainline Must-Read Cases

Use this layer when you want:

- the broadest module coverage first
- a stable route from small combined examples to one complete project
- the shortest path to understanding how XRT modules fit together

### 2. Advanced Combined Cases

Use this layer when you want:

- business-style extensions built on top of the mainline
- focused topics such as retry, archive, warming, storage tiers, or recovery pipelines
- more realistic operational slices without dropping into the heaviest control-plane topics

### 3. Advanced Control-Plane Series

Use this layer when you are explicitly working on:

- long-lived recovery state
- consensus and ownership transitions
- snapshot scheduling, lease coordination, and journal pipelines

These pages remain in the repo, but they are no longer treated as the endless continuation of the default case ladder.


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


## Advanced Combined Cases

Read these after the mainline, by topic.

### Functional Dashboard Extensions

1. [Upgrade the Local Console Service into a Subprocess Probe Dashboard](subprocess-probe-dashboard.en.md)
2. [Upgrade the Local Console Service into a Batch Task Dashboard](batch-task-dashboard.en.md)
3. [Upgrade the Local Console Service into a Cache Refresh Dashboard](cache-refresh-dashboard.en.md)
4. [A Multi-Tenant Index Registry with Dict, List, and AVLTree](tenant-index-registry.en.md)

### Storage, Warming, and Archive Topics

1. [Upgrade the Local Console Service into a Retry Backoff Dashboard](retry-backoff-dashboard.en.md)
2. [Upgrade the Local Console Service into an Archive Dashboard](archive-dashboard.en.md)
3. [Upgrade the Local Console Service into a Hot-Cold Tier Dashboard](hot-cold-tier-dashboard.en.md)
4. [Upgrade the Local Console Service into a Warm-Back Dashboard](warm-back-dashboard.en.md)
5. [Upgrade the Local Console Service into a Rolling Tier Archive Dashboard](rolling-tier-archive-dashboard.en.md)
6. [Upgrade the Local Console Service into an Auto-Warm Policy Dashboard](auto-warm-policy-dashboard.en.md)
7. [Upgrade the Local Console Service into a Warm-Archive Coordination Dashboard](warm-archive-coordination-dashboard.en.md)
8. [Upgrade the Local Console Service into an Auto-Warm + Rolling-Archive Integration Dashboard](auto-warm-rolling-archive-dashboard.en.md)
9. [Upgrade the Local Console Service into a Multi-Tier Storage Dashboard](multi-tier-storage-dashboard.en.md)
10. [Upgrade the Local Console Service into a Hot-Cold Multi-Level Aging Dashboard](hot-cold-multi-level-aging-dashboard.en.md)
11. [Upgrade the Local Console Service into a Recovery Priority Dashboard](recovery-priority-dashboard.en.md)

### Recovery, Quota, and Cross-Media Pipelines

1. [Upgrade the Local Console Service into a Cross-Media Orchestration Dashboard](cross-media-orchestration-dashboard.en.md)
2. [Upgrade the Local Console Service into a Heavy Recovery-Chain Dashboard](heavy-recovery-chain-dashboard.en.md)
3. [Upgrade the Local Console Service into a Recovery Quota Dashboard](recovery-quota-dashboard.md) (Chinese for now)
4. [Upgrade the Local Console Service into a Cross-Machine Recovery Dashboard](cross-machine-recovery-dashboard.md) (Chinese for now)
5. [Upgrade the Local Console Service into a Distributed Orchestration Dashboard](distributed-orchestration-dashboard.md) (Chinese for now)


## Advanced Control-Plane Series

These pages are kept as a dedicated advanced series rather than as the default next-stop chain.

### Consensus, Recovery, and Snapshot Baselines

1. [Upgrade the Local Console Service into a Persistent Coordination Dashboard](persistent-coordination-dashboard.md) (Chinese for now)
2. [Upgrade the Local Console Service into a Consensus Arbitration Dashboard](consensus-arbitration-dashboard.md) (Chinese for now)
3. [Upgrade the Local Console Service into a Consensus Log Replication Dashboard](consensus-log-replication-dashboard.md) (Chinese for now)
4. [Upgrade the Local Console Service into a State-Machine Commit Dashboard](state-machine-commit-dashboard.md) (Chinese for now)
5. [Upgrade the Local Console Service into a Log Replay Dashboard](log-replay-dashboard.md) (Chinese for now)
6. [Upgrade the Local Console Service into a Snapshot Compaction Dashboard](snapshot-compaction-dashboard.md) (Chinese for now)

### Takeover, Handoff, and Ownership

1. [Upgrade the Local Console Service into a Lease Failover Dashboard](lease-failover-dashboard.md) (Chinese for now)
2. [Upgrade the Local Console Service into a Leader Handoff Recovery Dashboard](leader-handoff-recovery-dashboard.md) (Chinese for now)
3. [Upgrade the Local Console Service into a Compaction Takeover Dashboard](compaction-takeover-dashboard.md) (Chinese for now)
4. [Upgrade the Local Console Service into an Ownership Arbitration Dashboard](ownership-arbitration-dashboard.md) (Chinese for now)
5. [Upgrade the Local Console Service into a Distributed Takeover Orchestration Dashboard](distributed-takeover-orchestration-dashboard.md) (Chinese for now)
6. [Upgrade the Local Console Service into a Consensus-Style Ownership Dashboard](consensus-style-ownership-dashboard.md) (Chinese for now)
7. [Upgrade the Local Console Service into a Consensus-Style Takeover Recovery Dashboard](consensus-takeover-recovery-dashboard.md) (Chinese for now)

### Snapshot Scheduling, Lease Coordination, and Journals

1. [Upgrade the Local Console Service into a Consensus Recovery Orchestration Dashboard](consensus-recovery-orchestration-dashboard.md) (Chinese for now)
2. [Upgrade the Local Console Service into a Consensus Snapshot Orchestration Dashboard](consensus-snapshot-orchestration-dashboard.md) (Chinese for now)
3. [Upgrade the Local Console Service into a Snapshot Takeover Dashboard](snapshot-takeover-dashboard.md) (Chinese for now)
4. [Upgrade the Local Console Service into a Snapshot Continuation Dashboard](snapshot-continuation-dashboard.md) (Chinese for now)
5. [Upgrade the Local Console Service into a Snapshot Persistence Dashboard](snapshot-persistence-dashboard.md) (Chinese for now)
6. [Upgrade the Local Console Service into a Snapshot Scheduler and Quota Dashboard](snapshot-scheduler-quota-dashboard.md) (Chinese for now)
7. [Upgrade the Local Console Service into a Snapshot Multi-Queue Scheduler Dashboard](snapshot-multi-queue-scheduler-dashboard.md) (Chinese for now)
8. [Upgrade the Local Console Service into a Snapshot Distributed Scheduler and Quota Dashboard](snapshot-distributed-scheduler-quota-dashboard.md) (Chinese for now)
9. [Upgrade the Local Console Service into a Snapshot Distributed Quota Arbitration Dashboard](snapshot-distributed-quota-arbitration-dashboard.md) (Chinese for now)
10. [Upgrade the Local Console Service into a Snapshot Cross-Cluster Scheduler Dashboard](snapshot-cross-cluster-scheduler-dashboard.md) (Chinese for now)
11. [Upgrade the Local Console Service into a Snapshot Lease Coordination Dashboard](snapshot-lease-coordination-dashboard.md) (Chinese for now)
12. [Upgrade the Local Console Service into a Lease Journal and Ownership Migration Dashboard](lease-journal-ownership-migration-dashboard.md) (Chinese for now)
13. [Upgrade the Local Console Service into a Lease Journal Compaction and Ownership Migration Replay Dashboard](lease-journal-compaction-migration-replay-dashboard.md) (Chinese for now)


## Current Gaps

The next real priorities are no longer “keep extending the same control-plane chain forever.” They are:

- add one overview page for the advanced control-plane series itself
- prioritize English sync for this advanced series
- rebalance the case library with more non-control-plane directions
  for example: pure tooling programs, pure file/text chains, and long-running non-HTTP service skeletons


## Relationship to API and Guide Docs

You can think about the three layers this way:

- `api/`: module contracts and boundaries
- `guide/`: from-zero learning route
- `case/`: how multiple modules cooperate to solve one complete problem

If you already roughly know each module API but still do not know how to compose them into a full solution, this is the right place to read next.
