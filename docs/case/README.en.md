# XRT Case Studies

> Entry pages for complete chains, combined capability usage, and real engineering slices. `docs/guide/` focuses on teaching order; `docs/case/` focuses on how multiple modules solve one complete problem together.

[Back to Docs Hub](../README.en.md)

---

## Contents

- [Purpose of the Case Studies](#purpose-of-the-case-studies)
- [Current Status](#current-status)
- [Target Case Ladder](#target-case-ladder)
- [How to Use the Existing Pages](#how-to-use-the-existing-pages)
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

The `case/` tree already contains a useful set of pages, but it is still under rebuild:

- older pages were closer to topic notes than full teaching cases
- reviewed formal case pages now exist for several core chains
- the case ladder is better than before, but not yet complete from simple to complex
- the current reviewed formal case ladder now has English coverage
- newly rebuilt pages may still land in Chinese first before later English sync

If you want the staged teaching order behind these cases, read together with:

- [Guide Rebuild Roadmap](../guide/ROADMAP.en.md)


## Target Case Ladder

The formal case ladder now targets this order:

1. config system: `file + path + value + json`
2. template rendering page: `value + template + file`
3. multitask worker: `thread + queue + future`
4. subprocess and async-file pipeline: `subprocess + file_async + future`
5. HTTP client call chain: `xurl + http util + xhttp + TLS + proxy`
6. HTTP service: `xhttpd + json + template + value`
7. WebSocket session service: `xws + coroutine + queue`
8. streaming LLM API: `xhttp/xws + future + coroutine + json`
9. a small full project that combines config, logging, tasks, networking, and templates

The point of this ladder is not only to show that something runs. It is to show:

- why the modules are split this way
- which layer should use threads and which layer should use coroutines
- which layer should converge into future / wait-source
- how basic modules grow into complete features


## How to Use the Existing Pages

### 1. English-Synced Case Pages

The following case pages already exist in English:

1. [Configuration System with `xvalue + json`](json-config-system.en.md)
2. [Rendering HTML with Template](template-render-html.en.md)
3. [Thread, Coroutine, and Future Coordination](thread-coroutine-future.en.md)
4. [Queue + Future Multi-Producer Worker](queue-worker-future.en.md)
5. [Subprocess + Async File Tooling Pipeline](subprocess-file-async-pipeline.en.md)
6. [XHTTP Client Chain with URL, Proxy, and TLS](xhttp-client-proxy-tls.en.md)
7. [XWS Session Skeleton with Queue and Coroutine](xws-session-queue-coroutine.en.md)
8. [xnet-v2 Stream Wait-Source Walkthrough](xnet-stream-wait-source.en.md)
9. [Minimal HTTP Service with XRT](minimal-http-service.en.md)
10. [Streaming LLM API with XRT](streaming-llm-api.en.md)
11. [Full HTTP + JSON + Template Service Chain](http-json-template-chain.en.md)
12. [Signed Rule Bundle Import with Charset, Regex, and Crypto](signed-rule-bundle.en.md)
13. [Session Registry with `mempool + avltree + list`](session-registry-pool-index.en.md)
14. [A Local Console Service that Combines Config, Logging, Tasks, Networking, and Templates](local-console-service.en.md)
15. [Upgrade the Local Console Service into a Subprocess Probe Dashboard](subprocess-probe-dashboard.en.md)
16. [Upgrade the Local Console Service into a Batch Task Dashboard](batch-task-dashboard.en.md)
17. [Upgrade the Local Console Service into a Cache Refresh Dashboard](cache-refresh-dashboard.en.md)
18. [A Multi-Tenant Index Registry with Dict, List, and AVLTree](tenant-index-registry.en.md)
19. [Upgrade the Local Console Service into a Retry Backoff Dashboard](retry-backoff-dashboard.en.md)
20. [Upgrade the Local Console Service into an Archive Dashboard](archive-dashboard.en.md)
21. [Upgrade the Local Console Service into a Hot-Cold Tier Dashboard](hot-cold-tier-dashboard.en.md)
22. [Upgrade the Local Console Service into a Warm-Back Dashboard](warm-back-dashboard.en.md)
23. [Upgrade the Local Console Service into a Rolling Tier Archive Dashboard](rolling-tier-archive-dashboard.en.md)
24. [Upgrade the Local Console Service into an Auto-Warm Policy Dashboard](auto-warm-policy-dashboard.en.md)
25. [Upgrade the Local Console Service into a Warm-Archive Coordination Dashboard](warm-archive-coordination-dashboard.en.md)
26. [Upgrade the Local Console Service into an Auto-Warm + Rolling-Archive Integration Dashboard](auto-warm-rolling-archive-dashboard.en.md)
27. [Upgrade the Local Console Service into a Multi-Tier Storage Dashboard](multi-tier-storage-dashboard.en.md)
28. [Upgrade the Local Console Service into a Hot-Cold Multi-Level Aging Dashboard](hot-cold-multi-level-aging-dashboard.en.md)
29. [Upgrade the Local Console Service into a Recovery Priority Dashboard](recovery-priority-dashboard.en.md)
30. [Upgrade the Local Console Service into a Cross-Media Orchestration Dashboard](cross-media-orchestration-dashboard.en.md)
31. [Upgrade the Local Console Service into a Heavy Recovery-Chain Dashboard](heavy-recovery-chain-dashboard.en.md)

### 2. Current English Coverage Notes

At this point, the main reviewed formal case ladder has English coverage for its core concurrency, tooling, HTTP-client, WebSocket-session, signed-rule-import, session-registry, subprocess-dashboard, batch-dashboard, cache-dashboard, tenant-index, retry-backoff, archive, hot-cold-tier, warm-back, rolling-tier archive, warm-archive coordination, auto-warm rolling archive, multi-tier storage, hot-cold multi-level aging, recovery-priority, cross-media orchestration, and heavy recovery-chain business pages.

So the remaining gaps are now mostly:

- the newly added [Upgrade the Local Console Service into a Persistent Coordination Dashboard](persistent-coordination-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Consensus Arbitration Dashboard](consensus-arbitration-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Consensus Log Replication Dashboard](consensus-log-replication-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a State-Machine Commit Dashboard](state-machine-commit-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Log Replay Dashboard](log-replay-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Snapshot Compaction Dashboard](snapshot-compaction-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Lease Failover Dashboard](lease-failover-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Leader Handoff Recovery Dashboard](leader-handoff-recovery-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Compaction Takeover Dashboard](compaction-takeover-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into an Ownership Arbitration Dashboard](ownership-arbitration-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Distributed Takeover Orchestration Dashboard](distributed-takeover-orchestration-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Consensus-Style Ownership Dashboard](consensus-style-ownership-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Consensus-Style Takeover Recovery Dashboard](consensus-takeover-recovery-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Consensus Recovery Orchestration Dashboard](consensus-recovery-orchestration-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Consensus Snapshot Orchestration Dashboard](consensus-snapshot-orchestration-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Snapshot Takeover Dashboard](snapshot-takeover-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Snapshot Continuation Dashboard](snapshot-continuation-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Snapshot Persistence Dashboard](snapshot-persistence-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Snapshot Scheduler and Quota Dashboard](snapshot-scheduler-quota-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Snapshot Multi-Queue Scheduler Dashboard](snapshot-multi-queue-scheduler-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Snapshot Distributed Scheduler and Quota Dashboard](snapshot-distributed-scheduler-quota-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Snapshot Distributed Quota Arbitration Dashboard](snapshot-distributed-quota-arbitration-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Snapshot Cross-Cluster Scheduler Dashboard](snapshot-cross-cluster-scheduler-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Snapshot Lease Coordination Dashboard](snapshot-lease-coordination-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Lease Journal and Ownership Migration Dashboard](lease-journal-ownership-migration-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Lease Journal Compaction and Ownership Migration Replay Dashboard](lease-journal-compaction-migration-replay-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Distributed Orchestration Dashboard](distributed-orchestration-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Cross-Machine Recovery Dashboard](cross-machine-recovery-dashboard.md) is still Chinese-first
- the newly added [Upgrade the Local Console Service into a Recovery Quota Dashboard](recovery-quota-dashboard.md) is still Chinese-first
- more business-style variants around lease control-plane orchestration

These pages are currently useful when you want:

- the latest reviewed combined examples
- an end-to-end chain that matches the rebuilt Chinese mainline
- a practical route from basic modules to full features

## Current Gaps

The most obvious remaining case gaps are:

- English sync for the persistent coordination page
- English sync for the consensus arbitration page
- English sync for the consensus log replication page
- English sync for the state-machine commit page
- English sync for the log replay page
- English sync for the snapshot compaction page
- English sync for the lease failover page
- English sync for the leader handoff recovery page
- English sync for the compaction takeover page
- English sync for the ownership arbitration page
- English sync for the distributed takeover orchestration page
- English sync for the consensus-style ownership page
- English sync for the consensus-style takeover recovery page
- English sync for the consensus recovery orchestration page
- English sync for the consensus snapshot orchestration page
- English sync for the snapshot takeover page
- English sync for the snapshot continuation page
- English sync for the snapshot persistence page
- English sync for the snapshot scheduler and quota page
- English sync for the snapshot multi-queue scheduler page
- English sync for the snapshot distributed scheduler and quota page
- English sync for the snapshot distributed quota arbitration page
- English sync for the snapshot cross-cluster scheduler page
- English sync for the snapshot lease coordination page
- English sync for the lease journal and ownership migration page
- English sync for the lease journal compaction and ownership migration replay page
- English sync for the distributed orchestration page
- English sync for the cross-machine recovery page
- English sync for the recovery quota page
- more business-style variants around lease control-plane orchestration


## Relationship to API and Guide Docs

You can think about the three layers this way:

- `api/`: module contracts and boundaries
- `guide/`: from-zero learning route
- `case/`: how multiple modules cooperate to solve one complete problem

If you already roughly know each module API but still do not know how to compose them into a full solution, this is the right place to read next.

