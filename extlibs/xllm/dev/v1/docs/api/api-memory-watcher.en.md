# xllm Memory Watcher API

> Header: `xllm-memory.h`

This page explains how to connect external file watcher events to xllm memory. xllm does not watch operating system file changes for you. It provides APIs to receive file events, merge events, and batch-sync them into memory.

## Four-Layer Model

| Layer | Object | Purpose |
| --- | --- | --- |
| queue | `xllm_memory_file_event_queue` | Collects file events only; you drain it manually. |
| bridge | `xllm_memory_watcher_bridge` | Holds queue and memory; syncs when flushed. |
| pump | `xllm_memory_watcher_pump` | Can auto-flush on push when threshold is reached. |
| worker | `xllm_memory_watcher_worker` | Adds debounce, poll, and run loop on top of pump. |

Learn in this order: use queue first, then bridge, then worker for continuous sync scenarios.

## Types

### xllm_memory_file_event_queue

**Purpose:** opaque handle for a file event queue.

```c
typedef struct xllm_memory_file_event_queue xllm_memory_file_event_queue;
```

### xllm_memory_file_event_queue_drain_options

**Purpose:** configuration used when syncing queued events into memory.

```c
typedef struct {
    xllm_memory_sync_file_options tBaseOptions;
    size_t iMaxItems;
    bool bContinueOnError;
    xvalue tVendorExtra;
} xllm_memory_file_event_queue_drain_options;
```

| Field | Meaning |
| --- | --- |
| `tBaseOptions` | Common file sync configuration, such as root path, record ID prefix, and filters. |
| `iMaxItems` | Maximum events to process in this drain. `0` means no extra limit. |
| `bContinueOnError` | Whether to continue after one event fails. |

### xllm_memory_watcher_bridge_options

```c
typedef struct {
    xllm_memory_sync_file_options tBaseOptions;
    size_t iDefaultMaxItems;
    bool bContinueOnError;
    xvalue tVendorExtra;
} xllm_memory_watcher_bridge_options;
```

### xllm_memory_watcher_pump_options

```c
typedef struct {
    xllm_memory_watcher_bridge_options tBridgeOptions;
    size_t iAutoFlushThreshold;
    xvalue tVendorExtra;
} xllm_memory_watcher_pump_options;
```

When `iAutoFlushThreshold > 0`, pending events reaching the threshold after push trigger auto-flush.

### xllm_memory_watcher_worker_options

```c
typedef struct {
    xllm_memory_watcher_pump_options tPumpOptions;
    uint32 uDebounceMs;
    size_t iDefaultMaxItems;
    xvalue tVendorExtra;
} xllm_memory_watcher_worker_options;
```

`uDebounceMs` avoids syncing the same path repeatedly while a file is being written multiple times.

### xllm_memory_watcher_worker_run_options

```c
typedef struct {
    size_t iMaxBatches;
    size_t iMaxItemsPerBatch;
    xvalue tVendorExtra;
} xllm_memory_watcher_worker_run_options;
```

Controls how many batches one `run_ready` call can process.

### xllm_memory_watcher_worker_loop_options

```c
typedef struct {
    xllm_memory_watcher_worker_run_options tRunOptions;
    uint32 uSleepMs;
    uint32 uMaxWaitMs;
    bool bForceFlushOnTimeout;
    xvalue tVendorExtra;
} xllm_memory_watcher_worker_loop_options;
```

Controls wait and timeout behavior for `run_loop`.

### xllm_memory_watcher_worker_state

```c
typedef struct {
    size_t iPendingCount;
    uint32 uDebounceMs;
    uint32 uElapsedSinceActivityMs;
    uint32 uWaitMsRemaining;
    bool bReady;
    bool bBlockedByDebounce;
    xvalue tVendorExtra;
} xllm_memory_watcher_worker_state;
```

Use it to inspect pending count, debounce state, and remaining wait time.

## API: File Event Queue

```c
XLLM_API int xllm_memory_file_event_queue_create(
    xllm_memory_file_event_queue **ppQueue
);

XLLM_API void xllm_memory_file_event_queue_destroy(
    xllm_memory_file_event_queue *pQueue
);

XLLM_API void xllm_memory_file_event_queue_clear(
    xllm_memory_file_event_queue *pQueue
);

XLLM_API size_t xllm_memory_file_event_queue_count(
    const xllm_memory_file_event_queue *pQueue
);
```

### xllm_memory_file_event_queue_compact

Merges events in the queue where possible to reduce duplicate sync work.

```c
XLLM_API int xllm_memory_file_event_queue_compact(
    xllm_memory_file_event_queue *pQueue,
    xllm_error *pError
);
```

Repeated updates on the same path or multiple rename events may be compacted. When connecting a file watcher, compact before drain.

### Push APIs

```c
XLLM_API int xllm_memory_file_event_queue_push(
    xllm_memory_file_event_queue *pQueue,
    const xllm_memory_file_event *pEvent,
    xllm_error *pError
);

XLLM_API int xllm_memory_file_event_queue_push_created(
    xllm_memory_file_event_queue *pQueue,
    const char *sPath,
    xllm_error *pError
);

XLLM_API int xllm_memory_file_event_queue_push_updated(
    xllm_memory_file_event_queue *pQueue,
    const char *sPath,
    xllm_error *pError
);

XLLM_API int xllm_memory_file_event_queue_push_deleted(
    xllm_memory_file_event_queue *pQueue,
    const char *sPath,
    xllm_error *pError
);

XLLM_API int xllm_memory_file_event_queue_push_renamed(
    xllm_memory_file_event_queue *pQueue,
    const char *sPreviousPath,
    const char *sPath,
    xllm_error *pError
);
```

### Drain APIs

```c
XLLM_API void xllm_memory_file_event_queue_drain_options_init(
    xllm_memory_file_event_queue_drain_options *pOptions
);

XLLM_API int xllm_memory_file_event_queue_drain(
    xllm_memory *pMemory,
    xllm_memory_file_event_queue *pQueue,
    const xllm_memory_file_event_queue_drain_options *pOptions,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);
```

Default drain options initialize `tBaseOptions` and set `bContinueOnError = true`. Reset `pResult` with `xllm_memory_change_set_reset` after use.

## API: Watcher Bridge

```c
XLLM_API void xllm_memory_watcher_bridge_options_init(
    xllm_memory_watcher_bridge_options *pOptions
);

XLLM_API int xllm_memory_watcher_bridge_create(
    xllm_memory *pMemory,
    const xllm_memory_watcher_bridge_options *pOptions,
    xllm_memory_watcher_bridge **ppBridge,
    xllm_error *pError
);

XLLM_API void xllm_memory_watcher_bridge_destroy(
    xllm_memory_watcher_bridge *pBridge
);
```

Bridge queue management:

```c
XLLM_API void xllm_memory_watcher_bridge_clear(
    xllm_memory_watcher_bridge *pBridge
);

XLLM_API size_t xllm_memory_watcher_bridge_pending_count(
    const xllm_memory_watcher_bridge *pBridge
);

XLLM_API int xllm_memory_watcher_bridge_compact_pending(
    xllm_memory_watcher_bridge *pBridge,
    xllm_error *pError
);
```

Push and flush:

```c
XLLM_API int xllm_memory_watcher_bridge_push(
    xllm_memory_watcher_bridge *pBridge,
    const xllm_memory_file_event *pEvent,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_bridge_flush(
    xllm_memory_watcher_bridge *pBridge,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_bridge_flush_max(
    xllm_memory_watcher_bridge *pBridge,
    size_t iMaxItems,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);
```

Convenience functions mirror the queue versions, with `xllm_memory_watcher_bridge *` as the first parameter:

- `xllm_memory_watcher_bridge_push_created`
- `xllm_memory_watcher_bridge_push_updated`
- `xllm_memory_watcher_bridge_push_deleted`
- `xllm_memory_watcher_bridge_push_renamed`

## API: Watcher Pump

```c
XLLM_API void xllm_memory_watcher_pump_options_init(
    xllm_memory_watcher_pump_options *pOptions
);

XLLM_API int xllm_memory_watcher_pump_create(
    xllm_memory *pMemory,
    const xllm_memory_watcher_pump_options *pOptions,
    xllm_memory_watcher_pump **ppPump,
    xllm_error *pError
);

XLLM_API void xllm_memory_watcher_pump_destroy(
    xllm_memory_watcher_pump *pPump
);

XLLM_API void xllm_memory_watcher_pump_clear(
    xllm_memory_watcher_pump *pPump
);

XLLM_API size_t xllm_memory_watcher_pump_pending_count(
    const xllm_memory_watcher_pump *pPump
);
```

Push and flush:

```c
XLLM_API int xllm_memory_watcher_pump_push(
    xllm_memory_watcher_pump *pPump,
    const xllm_memory_file_event *pEvent,
    bool *pbFlushed,
    xllm_memory_change_set *pAutoFlushResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_pump_flush(
    xllm_memory_watcher_pump *pPump,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);
```

Additional convenience APIs:

- `xllm_memory_watcher_pump_push_created`
- `xllm_memory_watcher_pump_push_updated`
- `xllm_memory_watcher_pump_push_deleted`
- `xllm_memory_watcher_pump_push_renamed`
- `xllm_memory_watcher_pump_compact_pending`
- `xllm_memory_watcher_pump_flush_max`

## API: Watcher Worker

Worker adds debounce and batch running on top of pump.

```c
XLLM_API void xllm_memory_watcher_worker_options_init(
    xllm_memory_watcher_worker_options *pOptions
);

XLLM_API int xllm_memory_watcher_worker_create(
    xllm_memory *pMemory,
    const xllm_memory_watcher_worker_options *pOptions,
    xllm_memory_watcher_worker **ppWorker,
    xllm_error *pError
);

XLLM_API void xllm_memory_watcher_worker_destroy(
    xllm_memory_watcher_worker *pWorker
);

XLLM_API int xllm_memory_watcher_worker_get_state(
    const xllm_memory_watcher_worker *pWorker,
    xllm_memory_watcher_worker_state *pState,
    xllm_error *pError
);
```

`xllm_memory_watcher_worker_pending_count`, `xllm_memory_watcher_worker_clear`, and `xllm_memory_watcher_worker_compact_pending` inspect, clear, and compact pending events. `xllm_memory_watcher_worker_state_init` initializes state, and `xllm_memory_watcher_worker_get_state` reports ready/debounce/wait status.

Push, poll, and flush:

```c
XLLM_API int xllm_memory_watcher_worker_push(
    xllm_memory_watcher_worker *pWorker,
    const xllm_memory_file_event *pEvent,
    bool *pbFlushed,
    xllm_memory_change_set *pPushResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_worker_poll(
    xllm_memory_watcher_worker *pWorker,
    bool *pbFlushed,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);

XLLM_API int xllm_memory_watcher_worker_flush(
    xllm_memory_watcher_worker *pWorker,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);
```

Additional convenience APIs:

- `xllm_memory_watcher_worker_push_created`
- `xllm_memory_watcher_worker_push_updated`
- `xllm_memory_watcher_worker_push_deleted`
- `xllm_memory_watcher_worker_push_renamed`
- `xllm_memory_watcher_worker_poll_max`
- `xllm_memory_watcher_worker_flush_max`

`poll_max` and `flush_max` limit the number of events processed in one call, useful for UI threads or background jobs that process in batches.

### xllm_memory_watcher_worker_run_ready

Processes currently ready batches and calls a callback after each batch.

```c
XLLM_API int xllm_memory_watcher_worker_run_ready(
    xllm_memory_watcher_worker *pWorker,
    const xllm_memory_watcher_worker_run_options *pOptions,
    xllm_memory_watcher_worker_batch_fn fnOnBatch,
    void *pBatchCtx,
    xllm_memory_watcher_worker_run_result *pResult,
    xllm_error *pError
);
```

Initialize run options with `xllm_memory_watcher_worker_run_options_init` and result with `xllm_memory_watcher_worker_run_result_init`.

### xllm_memory_watcher_worker_run_loop

Loops while there are pending events, waits for debounce expiry, and processes batches.

```c
XLLM_API int xllm_memory_watcher_worker_run_loop(
    xllm_memory_watcher_worker *pWorker,
    const xllm_memory_watcher_worker_loop_options *pOptions,
    xllm_memory_watcher_worker_batch_fn fnOnBatch,
    void *pBatchCtx,
    xllm_memory_watcher_worker_loop_result *pResult,
    xllm_error *pError
);
```

Initialize loop options with `xllm_memory_watcher_worker_loop_options_init` and result with `xllm_memory_watcher_worker_loop_result_init`.

## Example: Sync File Updates with Worker

```c
xllm_memory_watcher_worker_options options;
xllm_memory_watcher_worker *worker = NULL;
xllm_memory_change_set changes;
bool flushed = false;

xllm_memory_watcher_worker_options_init(&options);
options.tPumpOptions.tBridgeOptions.tBaseOptions.sRootPath = "D:/git/xllm";
options.tPumpOptions.tBridgeOptions.tBaseOptions.sRecordIdPrefix = "workspace:xllm:";
options.uDebounceMs = 250;

if (xllm_memory_watcher_worker_create(memory, &options, &worker, NULL) != XRT_NET_OK) {
    return;
}

xllm_memory_change_set_init(&changes);
xllm_memory_watcher_worker_push_updated(
    worker,
    "D:/git/xllm/docs/api/api-memory.md",
    &flushed,
    &changes,
    NULL);

if (!flushed) {
    xllm_memory_watcher_worker_flush(worker, &changes, NULL);
}

xllm_memory_change_set_reset(&changes);
xllm_memory_watcher_worker_destroy(worker);
```

## Common Mistakes

### Thinking watcher automatically watches disk

xllm watcher APIs receive events, but do not register OS file watchers directly. Use your own watcher and push events into queue, bridge, pump, or worker.

### Not handling debounce

Editors may trigger multiple events when saving a file. Worker debounce reduces duplicate ingest. If you use queue or bridge, decide when to compact and flush yourself.

### Forgetting to reset change set

All `xllm_memory_change_set` values returned by flush, poll, or drain must be reset with `xllm_memory_change_set_reset`.

## Related Documentation

- [Memory Workspace API](api-memory-workspace.en.md)
- [Memory Ingest API](api-memory-ingest.en.md)
- [Back to API Index](README.en.md)
