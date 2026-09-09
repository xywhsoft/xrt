# xllm Memory Watcher API

> 状态：中文初稿已生成，待审阅。  
> 头文件：`xllm-memory.h`

本页讲解如何把外部文件监听事件接入 xllm memory。xllm 不替你监听操作系统文件变化；它提供的是一组“接收文件事件、合并事件、按批同步到 memory”的 API。

## 四层模型

| 层 | 对象 | 作用 |
| --- | --- | --- |
| queue | `xllm_memory_file_event_queue` | 只收集文件事件，需要你主动 drain。 |
| bridge | `xllm_memory_watcher_bridge` | 持有 queue 和 memory，flush 时同步。 |
| pump | `xllm_memory_watcher_pump` | push 时可按阈值自动 flush。 |
| worker | `xllm_memory_watcher_worker` | 在 pump 之上增加 debounce、poll、run loop。 |

学习时建议按这个顺序理解：先会用 queue，再用 bridge，最后在持续同步场景使用 worker。

## 类型

### xllm_memory_file_event_queue

**功能**：文件事件队列的不透明句柄。

```c
typedef struct xllm_memory_file_event_queue xllm_memory_file_event_queue;
```

### xllm_memory_file_event_queue_drain_options

**功能**：把队列中的事件同步到 memory 时使用的配置。

```c
typedef struct {
    xllm_memory_sync_file_options tBaseOptions;
    size_t iMaxItems;
    bool bContinueOnError;
    xvalue tVendorExtra;
} xllm_memory_file_event_queue_drain_options;
```

| 字段 | 含义 |
| --- | --- |
| `tBaseOptions` | 文件同步公共配置，例如 root path、record id 前缀、过滤规则。 |
| `iMaxItems` | 本次最多处理多少事件；`0` 表示不额外限制。 |
| `bContinueOnError` | 单个事件失败后是否继续。 |

### xllm_memory_watcher_bridge_options

**功能**：创建 watcher bridge。

```c
typedef struct {
    xllm_memory_sync_file_options tBaseOptions;
    size_t iDefaultMaxItems;
    bool bContinueOnError;
    xvalue tVendorExtra;
} xllm_memory_watcher_bridge_options;
```

### xllm_memory_watcher_pump_options

**功能**：创建 watcher pump。

```c
typedef struct {
    xllm_memory_watcher_bridge_options tBridgeOptions;
    size_t iAutoFlushThreshold;
    xvalue tVendorExtra;
} xllm_memory_watcher_pump_options;
```

`iAutoFlushThreshold` 大于 `0` 时，push 后待处理事件达到阈值会自动 flush。

### xllm_memory_watcher_worker_options

**功能**：创建 watcher worker。

```c
typedef struct {
    xllm_memory_watcher_pump_options tPumpOptions;
    uint32 uDebounceMs;
    size_t iDefaultMaxItems;
    xvalue tVendorExtra;
} xllm_memory_watcher_worker_options;
```

`uDebounceMs` 用来避免文件连续写入时反复同步同一路径。

### xllm_memory_watcher_worker_run_options

**功能**：控制 `run_ready` 一次最多处理多少批。

```c
typedef struct {
    size_t iMaxBatches;
    size_t iMaxItemsPerBatch;
    xvalue tVendorExtra;
} xllm_memory_watcher_worker_run_options;
```

### xllm_memory_watcher_worker_loop_options

**功能**：控制 `run_loop` 等待和超时行为。

```c
typedef struct {
    xllm_memory_watcher_worker_run_options tRunOptions;
    uint32 uSleepMs;
    uint32 uMaxWaitMs;
    bool bForceFlushOnTimeout;
    xvalue tVendorExtra;
} xllm_memory_watcher_worker_loop_options;
```

### xllm_memory_watcher_worker_state

**功能**：查看 worker 当前状态。

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

## API：file event queue

### xllm_memory_file_event_queue_create

**功能**：创建文件事件队列。

**原型**：

```c
XLLM_API int xllm_memory_file_event_queue_create(
    xllm_memory_file_event_queue **ppQueue
);
```

**返回值**：成功返回 `XRT_NET_OK`，失败返回 `XRT_NET_ERROR`。

### xllm_memory_file_event_queue_destroy

**功能**：销毁队列。

**原型**：

```c
XLLM_API void xllm_memory_file_event_queue_destroy(
    xllm_memory_file_event_queue *pQueue
);
```

### xllm_memory_file_event_queue_clear

**功能**：清空待处理事件。

```c
XLLM_API void xllm_memory_file_event_queue_clear(
    xllm_memory_file_event_queue *pQueue
);
```

### xllm_memory_file_event_queue_count

**功能**：返回待处理事件数。

```c
XLLM_API size_t xllm_memory_file_event_queue_count(
    const xllm_memory_file_event_queue *pQueue
);
```

### xllm_memory_file_event_queue_compact

**功能**：合并队列中可合并的事件，减少重复同步。

```c
XLLM_API int xllm_memory_file_event_queue_compact(
    xllm_memory_file_event_queue *pQueue,
    xllm_error *pError
);
```

**补充说明**：同一路径连续 updated、多次 rename 等事件可能可以压缩。接入文件监听器时，建议 drain 前 compact。

### xllm_memory_file_event_queue_push

**功能**：向队列推入一个事件。

```c
XLLM_API int xllm_memory_file_event_queue_push(
    xllm_memory_file_event_queue *pQueue,
    const xllm_memory_file_event *pEvent,
    xllm_error *pError
);
```

### push_created / push_updated / push_deleted / push_renamed

**功能**：便捷推入某类事件。

```c
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

### xllm_memory_file_event_queue_drain_options_init

**功能**：初始化 drain options。

```c
XLLM_API void xllm_memory_file_event_queue_drain_options_init(
    xllm_memory_file_event_queue_drain_options *pOptions
);
```

**默认值**：初始化 `tBaseOptions`，并设置 `bContinueOnError = true`。

### xllm_memory_file_event_queue_drain

**功能**：处理队列中的事件，并把变化同步到 memory。

```c
XLLM_API int xllm_memory_file_event_queue_drain(
    xllm_memory *pMemory,
    xllm_memory_file_event_queue *pQueue,
    const xllm_memory_file_event_queue_drain_options *pOptions,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);
```

**补充说明**：`pResult` 使用后调用 `xllm_memory_change_set_reset`。

## API：watcher bridge

### xllm_memory_watcher_bridge_options_init

**功能**：初始化 bridge options。

```c
XLLM_API void xllm_memory_watcher_bridge_options_init(
    xllm_memory_watcher_bridge_options *pOptions
);
```

### xllm_memory_watcher_bridge_create / destroy

**功能**：创建或销毁 bridge。

```c
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

### bridge clear / pending_count / compact_pending

**功能**：管理 bridge 内部待处理事件。

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

### bridge push / flush

**功能**：向 bridge 推入事件，或同步待处理事件。

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

另有四个便捷函数，参数形式和 queue 版本一致，只是第一个参数换成 `xllm_memory_watcher_bridge *`：

- `xllm_memory_watcher_bridge_push_created`
- `xllm_memory_watcher_bridge_push_updated`
- `xllm_memory_watcher_bridge_push_deleted`
- `xllm_memory_watcher_bridge_push_renamed`

## API：watcher pump

### xllm_memory_watcher_pump_options_init

**功能**：初始化 pump options。

```c
XLLM_API void xllm_memory_watcher_pump_options_init(
    xllm_memory_watcher_pump_options *pOptions
);
```

### pump create / destroy / clear / pending_count

```c
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

### pump push / flush

**功能**：推入事件，并可在达到阈值时自动 flush。

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

另有以下便捷函数：

- `xllm_memory_watcher_pump_push_created`
- `xllm_memory_watcher_pump_push_updated`
- `xllm_memory_watcher_pump_push_deleted`
- `xllm_memory_watcher_pump_push_renamed`
- `xllm_memory_watcher_pump_compact_pending`
- `xllm_memory_watcher_pump_flush_max`

`xllm_memory_watcher_pump_compact_pending` 用于在 flush 前合并待处理事件；`xllm_memory_watcher_pump_flush_max` 用于一次最多 flush `iMaxItems` 个事件。

## API：watcher worker

### xllm_memory_watcher_worker_options_init

```c
XLLM_API void xllm_memory_watcher_worker_options_init(
    xllm_memory_watcher_worker_options *pOptions
);
```

### worker create / destroy / clear / state

```c
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

`xllm_memory_watcher_worker_pending_count`、`xllm_memory_watcher_worker_clear`、`xllm_memory_watcher_worker_compact_pending` 分别用于查看、清空和压缩待处理事件。`xllm_memory_watcher_worker_state_init` 用于初始化 `xllm_memory_watcher_worker_state`，`xllm_memory_watcher_worker_get_state` 会填充 worker 当前是否 ready、是否被 debounce 阻塞、剩余等待时间等字段。

### worker push / poll / flush

**功能**：推入事件、尝试按 debounce 规则处理、或强制 flush。

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

另有以下便捷函数：

- `xllm_memory_watcher_worker_push_created`
- `xllm_memory_watcher_worker_push_updated`
- `xllm_memory_watcher_worker_push_deleted`
- `xllm_memory_watcher_worker_push_renamed`
- `xllm_memory_watcher_worker_poll_max`
- `xllm_memory_watcher_worker_flush_max`

`xllm_memory_watcher_worker_poll_max` 和 `xllm_memory_watcher_worker_flush_max` 用于限制单次处理事件数量，适合 UI 线程或后台任务需要分批处理的场景。

### xllm_memory_watcher_worker_run_ready

**功能**：处理当前已经 ready 的事件批次，并在每批完成后调用回调。

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

调用前使用 `xllm_memory_watcher_worker_run_options_init` 初始化 `xllm_memory_watcher_worker_run_options`，使用 `xllm_memory_watcher_worker_run_result_init` 初始化 `xllm_memory_watcher_worker_run_result`。

### xllm_memory_watcher_worker_run_loop

**功能**：在有 pending 事件时循环等待 debounce 到期并处理批次。

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

调用前使用 `xllm_memory_watcher_worker_loop_options_init` 初始化 `xllm_memory_watcher_worker_loop_options`，使用 `xllm_memory_watcher_worker_loop_result_init` 初始化 `xllm_memory_watcher_worker_loop_result`。

## 范例：用 worker 同步文件更新

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

## 常见错误

### 以为 watcher 会自动监听磁盘

xllm 的 watcher API 接收事件，但不直接注册 OS 文件监听。你需要用自己的监听器把事件 push 进 queue、bridge、pump 或 worker。

### 不处理 debounce

编辑器保存文件时可能连续触发多个事件。worker 的 debounce 可以减少重复入库；如果你用 queue 或 bridge，需要自己决定何时 compact 和 flush。

### 忘记 reset change set

所有 flush、poll、drain 返回的 `xllm_memory_change_set` 使用后都要调用 `xllm_memory_change_set_reset`。

## 相关文档

- [Memory Workspace API](api-memory-workspace.md)
- [Memory Ingest API](api-memory-ingest.md)
- [返回 API 索引](README.md)
