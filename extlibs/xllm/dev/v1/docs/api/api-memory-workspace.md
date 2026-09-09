# xllm Memory Workspace API

> 状态：中文初稿已生成，待审阅。  
> 头文件：`xllm-memory.h`

本页讲解 xllm memory 的工作区同步接口。和一次性 `ingest_directory` 不同，workspace API 更关注“一个目录会持续变化”的场景：文件新增、更新、删除后，memory 中的 record 应该跟着同步。

如果你只是第一次导入目录，可以先看 [Memory Ingest API](api-memory-ingest.md)。如果你要接文件监听器或事件队列，请继续看 [Memory Watcher API](api-memory-watcher.md)。

## 什么时候使用 workspace API

| 场景 | 推荐 API |
| --- | --- |
| 第一次导入一个源码仓库 | `xllm_memory_ingest_workspace` |
| 重新扫描工作区，并删除 memory 中已经不存在的文件记录 | `xllm_memory_sync_workspace` |
| 只同步一个文件 | `xllm_memory_sync_file` |
| 一次同步多个已知文件 | `xllm_memory_sync_files` |
| 根据文件事件同步 created/updated/deleted/renamed | `xllm_memory_sync_file_events` |
| 查看当前 workspace 记录概况 | `xllm_memory_get_workspace_status` |
| 检查 memory 存储是否健康 | `xllm_memory_check_health` |

## 类型

### xllm_memory_sync_file_options

**功能**：同步单个文件时使用的配置。

```c
typedef struct {
    xllm_memory_scope eScope;
    const char *sPath;
    const char *sRootPath;
    const char *sRecordId;
    const char *sRecordIdPrefix;
    const char *sTitle;
    const char *sSourceUri;
    bool bReplaceExisting;
    bool bUseWorkspaceDefaults;
    bool bSkipHidden;
    bool bSkipUnchanged;
    const char *sAllowedExtensions;
    const char *sIgnoredDirectories;
    const char *sIgnoredExtensions;
    const char *sIgnoredPathPatterns;
    const char *sSourceUriPrefix;
    uint64 uMaxFileBytes;
    uint32 uChunkChars;
    uint32 uChunkOverlapChars;
    xvalue tMetadata;
    xvalue tVendorExtra;
} xllm_memory_sync_file_options;
```

| 字段 | 含义 |
| --- | --- |
| `eScope` | 同步到哪个范围，默认 `KNOWLEDGE`。 |
| `sPath` | 文件路径，必填。 |
| `sRootPath` | 工作区根路径，用于计算相对路径和 source URI。 |
| `sRecordId` | 指定 record id。 |
| `sRecordIdPrefix` | 自动生成 record id 时使用的前缀。 |
| `sTitle` | record 标题。 |
| `sSourceUri` | 指定来源 URI。 |
| `bReplaceExisting` | 已存在时是否替换，默认 `true`。 |
| `bUseWorkspaceDefaults` | 是否启用 workspace 默认过滤规则。 |
| `bSkipHidden` | 是否跳过隐藏文件，默认 `true`。 |
| `bSkipUnchanged` | 是否跳过未变化文件，默认 `true`。 |
| `sAllowedExtensions` | 允许的扩展名集合。 |
| `sIgnoredDirectories` | 忽略目录集合。 |
| `sIgnoredExtensions` | 忽略扩展名集合。 |
| `sIgnoredPathPatterns` | 忽略路径模式集合。 |
| `sSourceUriPrefix` | 自动生成 source URI 的前缀。 |
| `uMaxFileBytes` | 文件大小上限。 |
| `uChunkChars` / `uChunkOverlapChars` | 本次同步的 chunk 设置。 |
| `tMetadata` | 写入 record 的 metadata。 |

### xllm_memory_sync_files_options

**功能**：同步多个文件。

```c
typedef struct {
    const xllm_memory_sync_file_options *pItems;
    size_t iItemCount;
    bool bContinueOnError;
    xvalue tVendorExtra;
} xllm_memory_sync_files_options;
```

**默认值**：`bContinueOnError = true`，表示某个文件失败时继续处理后续文件。

### xllm_memory_file_event_kind

**功能**：描述一个文件变化事件。

```c
typedef enum {
    XLLM_MEMORY_FILE_EVENT_CREATED = 1,
    XLLM_MEMORY_FILE_EVENT_UPDATED,
    XLLM_MEMORY_FILE_EVENT_DELETED,
    XLLM_MEMORY_FILE_EVENT_RENAMED
} xllm_memory_file_event_kind;
```

### xllm_memory_file_event

**功能**：同步文件事件时使用的输入项。

```c
typedef struct {
    xllm_memory_file_event_kind eKind;
    const char *sPath;
    const char *sPreviousPath;
    xvalue tVendorExtra;
} xllm_memory_file_event;
```

| 字段 | 含义 |
| --- | --- |
| `eKind` | 事件类型。 |
| `sPath` | 当前路径。 |
| `sPreviousPath` | rename 前的路径，仅 rename 常用。 |
| `tVendorExtra` | 扩展字段。 |

### xllm_memory_sync_file_events_options

**功能**：把一批文件事件应用到 memory。

```c
typedef struct {
    xllm_memory_sync_file_options tBaseOptions;
    const xllm_memory_file_event *pItems;
    size_t iItemCount;
    bool bContinueOnError;
    xvalue tVendorExtra;
} xllm_memory_sync_file_events_options;
```

`tBaseOptions` 提供根路径、过滤规则、record id 前缀等公共配置；每个事件只需要给出事件类型和路径。

### xllm_memory_change_kind

**功能**：描述同步后 memory 发生了什么变化。

```c
typedef enum {
    XLLM_MEMORY_CHANGE_CREATED = 1,
    XLLM_MEMORY_CHANGE_UPDATED,
    XLLM_MEMORY_CHANGE_REMOVED,
    XLLM_MEMORY_CHANGE_SKIPPED,
    XLLM_MEMORY_CHANGE_FAILED
} xllm_memory_change_kind;
```

### xllm_memory_change_info

**功能**：描述一个同步变化。

```c
typedef struct {
    xllm_memory_change_kind eKind;
    xllm_memory_record_info tRecord;
    xllm_memory_skipped_file_info tSkipped;
    xllm_memory_failed_file_info tFailed;
} xllm_memory_change_info;
```

**补充说明**：根据 `eKind` 读取对应字段。`CREATED/UPDATED/REMOVED` 主要看 `tRecord`，`SKIPPED` 看 `tSkipped`，`FAILED` 看 `tFailed`。

### xllm_memory_change_set

**功能**：保存一次同步产生的变化集合。

```c
typedef struct {
    xllm_memory_change_info *pChanges;
    size_t iChangeCount;
} xllm_memory_change_set;
```

**释放规则**：使用后调用 `xllm_memory_change_set_reset`。

### xllm_memory_sync_workspace_result

**功能**：保存全工作区同步结果。

```c
typedef struct {
    xllm_memory_ingest_directory_result tIngest;
    uint32 uExaminedRecordCount;
    uint32 uRemovedRecordCount;
    xllm_memory_record_info *pRemovedRecords;
    size_t iRemovedDetailCount;
} xllm_memory_sync_workspace_result;
```

| 字段 | 含义 |
| --- | --- |
| `tIngest` | 重新扫描并入库的统计。 |
| `uExaminedRecordCount` | 检查了多少已有 record。 |
| `uRemovedRecordCount` | 删除了多少已经不在工作区中的 record。 |
| `pRemovedRecords` | 被删除记录的详情。 |
| `iRemovedDetailCount` | 删除详情数量。 |

### xllm_memory_workspace_status_options

**功能**：查询工作区状态时的过滤条件。

```c
typedef struct {
    xllm_memory_scope eScope;
    const char *sRootPath;
    const char *sSourceUriPrefix;
    xvalue tVendorExtra;
} xllm_memory_workspace_status_options;
```

**默认值**：scope 为 `KNOWLEDGE`。

### xllm_memory_workspace_status

**功能**：保存 workspace 统计结果。

```c
typedef struct {
    xllm_memory_scope eScope;
    const char *sRootPath;
    const char *sSourceUriPrefix;
    bool bRootPathFilterSet;
    bool bSourceUriPrefixFilterSet;
    size_t iRecordCount;
    size_t iChunkCount;
    size_t iMissingPathRecordCount;
    size_t iSensitiveRecordCount;
    size_t iUntrustedRecordCount;
    uint64 uTotalFileBytes;
    int64 iOldestMtimeUnix;
    int64 iNewestMtimeUnix;
    xvalue tVendorExtra;
} xllm_memory_workspace_status;
```

### xllm_memory_health_check_options

**功能**：配置健康检查。

```c
typedef struct {
    xllm_memory_scope eScope;
    bool bCheckSqlite;
    bool bCheckSparsePostings;
    xvalue tVendorExtra;
} xllm_memory_health_check_options;
```

**默认值**：scope 为 `ANY`，检查 SQLite 和 sparse postings。

### xllm_memory_health_check

**功能**：保存健康检查结果。

```c
typedef struct {
    xllm_memory_scope eScope;
    bool bOk;
    bool bSqliteOpen;
    bool bSchemaOk;
    bool bProfileOk;
    bool bSparsePostingsOk;
    uint32 uSqliteSchemaVersion;
    const char *sMemoryProfileId;
    const char *sStoredMemoryProfileId;
    size_t iRecordCount;
    size_t iChunkCount;
    size_t iSqliteRecordCount;
    size_t iSqliteChunkCount;
    size_t iOrphanChunkCount;
    size_t iSparseChunkCount;
    size_t iSparseMissingChunkCount;
    size_t iSparseOrphanPostingCount;
    xvalue tVendorExtra;
} xllm_memory_health_check;
```

## API：工作区入库和同步

### xllm_memory_sync_workspace_result_init

**功能**：初始化 workspace 同步结果对象。

**原型**：

```c
XLLM_API void xllm_memory_sync_workspace_result_init(
    xllm_memory_sync_workspace_result *pResult
);
```

**返回值**：无。

### xllm_memory_sync_workspace_result_reset

**功能**：释放 workspace 同步结果中的详情数组。

**原型**：

```c
XLLM_API void xllm_memory_sync_workspace_result_reset(
    xllm_memory_sync_workspace_result *pResult
);
```

### xllm_memory_sync_workspace

**功能**：扫描工作区，入库新增或变化文件，并移除已经不存在的文件记录。

**原型**：

```c
XLLM_API int xllm_memory_sync_workspace(
    xllm_memory *pMemory,
    const xllm_memory_ingest_workspace_options *pOptions,
    xllm_memory_sync_workspace_result *pResult,
    xllm_error *pError
);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pMemory` | memory 对象。 |
| `pOptions` | 工作区入库配置，和 `xllm_memory_ingest_workspace` 共用。 |
| `pResult` | 输出同步统计，可为 `NULL`。 |
| `pError` | 错误对象，可为 `NULL`。 |

**返回值**：成功返回 `XRT_NET_OK`，失败返回 `XRT_NET_ERROR`。

**补充说明**：这是最适合“定期全量同步工作区”的接口。它不仅会处理新增和更新，也会检查 memory 中已有记录对应的文件是否仍存在。

### xllm_memory_sync_file_options_init

**功能**：初始化单文件同步 options。

**原型**：

```c
XLLM_API void xllm_memory_sync_file_options_init(
    xllm_memory_sync_file_options *pOptions
);
```

**默认值**：scope 为 `KNOWLEDGE`，替换已有记录，跳过隐藏文件，跳过未变化文件。

### xllm_memory_sync_file

**功能**：同步单个文件。

**原型**：

```c
XLLM_API int xllm_memory_sync_file(
    xllm_memory *pMemory,
    const xllm_memory_sync_file_options *pOptions,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pMemory` | memory 对象。 |
| `pOptions` | 单文件同步配置，`sPath` 必填。 |
| `pResult` | 输出变化集合，可为 `NULL`。 |
| `pError` | 错误对象，可为 `NULL`。 |

**返回值**：成功返回 `XRT_NET_OK`，失败返回 `XRT_NET_ERROR`。

### xllm_memory_sync_files_options_init

**功能**：初始化多文件同步 options。

**原型**：

```c
XLLM_API void xllm_memory_sync_files_options_init(
    xllm_memory_sync_files_options *pOptions
);
```

**默认值**：`bContinueOnError = true`。

### xllm_memory_sync_files

**功能**：同步一组文件。

**原型**：

```c
XLLM_API int xllm_memory_sync_files(
    xllm_memory *pMemory,
    const xllm_memory_sync_files_options *pOptions,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);
```

**补充说明**：适合你已经从外部系统拿到“这几个文件变了”的情况。相比重新扫描整个工作区，它更轻量。

### xllm_memory_sync_file_event_init

**功能**：初始化文件事件。

**原型**：

```c
XLLM_API void xllm_memory_sync_file_event_init(
    xllm_memory_file_event *pEvent
);
```

**默认值**：事件类型为 `XLLM_MEMORY_FILE_EVENT_UPDATED`。

### xllm_memory_sync_file_events_options_init

**功能**：初始化文件事件同步 options。

**原型**：

```c
XLLM_API void xllm_memory_sync_file_events_options_init(
    xllm_memory_sync_file_events_options *pOptions
);
```

**默认值**：初始化 `tBaseOptions`，并设置 `bContinueOnError = true`。

### xllm_memory_sync_file_events

**功能**：把一批 created/updated/deleted/renamed 文件事件应用到 memory。

**原型**：

```c
XLLM_API int xllm_memory_sync_file_events(
    xllm_memory *pMemory,
    const xllm_memory_sync_file_events_options *pOptions,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);
```

**补充说明**：deleted 事件通常会移除对应 record；renamed 事件通常会先处理旧路径，再处理新路径。具体变化请看返回的 `xllm_memory_change_set`。

### xllm_memory_change_set_init

**功能**：初始化变化集合。

**原型**：

```c
XLLM_API void xllm_memory_change_set_init(
    xllm_memory_change_set *pResult
);
```

### xllm_memory_change_set_reset

**功能**：释放变化集合。

**原型**：

```c
XLLM_API void xllm_memory_change_set_reset(
    xllm_memory_change_set *pResult
);
```

### xllm_memory_make_change_set_from_ingest

**功能**：把 `xllm_memory_ingest_directory_result` 转换成统一的 `xllm_memory_change_set`，便于 UI 或日志用同一种结构展示 created、updated、skipped、failed。

**原型**：

```c
XLLM_API int xllm_memory_make_change_set_from_ingest(
    const xllm_memory_ingest_directory_result *pIngest,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pIngest` | 输入目录或工作区 ingest 结果。 |
| `pResult` | 输出变化集合，调用前用 `xllm_memory_change_set_init` 初始化，使用后调用 `xllm_memory_change_set_reset`。 |
| `pError` | 失败详情，可为 `NULL`。 |

**返回值**：成功返回 `XRT_NET_OK`，失败返回 `XRT_NET_ERROR`。

### xllm_memory_make_change_set_from_sync

**功能**：把 `xllm_memory_sync_workspace_result` 转换成统一的 `xllm_memory_change_set`，包括本次 ingest 变化和 workspace sync 删除的记录。

**原型**：

```c
XLLM_API int xllm_memory_make_change_set_from_sync(
    const xllm_memory_sync_workspace_result *pSync,
    xllm_memory_change_set *pResult,
    xllm_error *pError
);
```

**补充说明**：这个函数适合工作区全量同步后向 UI 展示“新增、更新、删除、跳过、失败”的统一列表。

## API：状态与健康检查

### xllm_memory_workspace_status_options_init

**功能**：初始化 workspace status options。

**原型**：

```c
XLLM_API void xllm_memory_workspace_status_options_init(
    xllm_memory_workspace_status_options *pOptions
);
```

**默认值**：scope 为 `KNOWLEDGE`。

### xllm_memory_workspace_status_init

**功能**：初始化 workspace status 结果。

**原型**：

```c
XLLM_API void xllm_memory_workspace_status_init(
    xllm_memory_workspace_status *pStatus
);
```

### xllm_memory_get_workspace_status

**功能**：统计 workspace 相关记录的数量、大小、mtime 范围和风险标记数量。

**原型**：

```c
XLLM_API int xllm_memory_get_workspace_status(
    const xllm_memory *pMemory,
    const xllm_memory_workspace_status_options *pOptions,
    xllm_memory_workspace_status *pStatus,
    xllm_error *pError
);
```

**返回值**：成功返回 `XRT_NET_OK`，失败返回 `XRT_NET_ERROR`。

**补充说明**：这个接口不扫描磁盘，它统计的是 memory 中已有的 workspace 记录。

### xllm_memory_health_check_options_init

**功能**：初始化健康检查 options。

**原型**：

```c
XLLM_API void xllm_memory_health_check_options_init(
    xllm_memory_health_check_options *pOptions
);
```

**默认值**：scope 为 `ANY`，检查 SQLite 和 sparse postings。

### xllm_memory_health_check_init

**功能**：初始化健康检查结果。

**原型**：

```c
XLLM_API void xllm_memory_health_check_init(
    xllm_memory_health_check *pHealth
);
```

### xllm_memory_check_health

**功能**：检查 memory store 的基本一致性。

**原型**：

```c
XLLM_API int xllm_memory_check_health(
    const xllm_memory *pMemory,
    const xllm_memory_health_check_options *pOptions,
    xllm_memory_health_check *pHealth,
    xllm_error *pError
);
```

**返回值**：成功返回 `XRT_NET_OK`，失败返回 `XRT_NET_ERROR`。健康状态请看 `pHealth->bOk`。

**补充说明**：健康检查能帮助你确认 SQLite 是否打开、schema 是否正常、profile 是否匹配、record/chunk 数是否一致，以及 sparse postings 是否有缺失或孤儿项。

## 范例：同步整个工作区

```c
xllm_memory_ingest_workspace_options options;
xllm_memory_sync_workspace_result result;

xllm_memory_ingest_workspace_options_init(&options);
xllm_memory_sync_workspace_result_init(&result);

options.sPath = "D:/git/xllm";
options.sRecordIdPrefix = "workspace:xllm:";
options.sAllowedExtensions = ".h;.c;.md;.txt";

if (xllm_memory_sync_workspace(memory, &options, &result, NULL) == XRT_NET_OK) {
    printf("ingested=%u removed=%u failed=%u\n",
        result.tIngest.uIngestedFileCount,
        result.uRemovedRecordCount,
        result.tIngest.uFailedFileCount);
}

xllm_memory_sync_workspace_result_reset(&result);
```

## 范例：根据文件事件同步

```c
xllm_memory_file_event events[2];
xllm_memory_sync_file_events_options options;
xllm_memory_change_set changes;

xllm_memory_sync_file_event_init(&events[0]);
events[0].eKind = XLLM_MEMORY_FILE_EVENT_UPDATED;
events[0].sPath = "docs/api/api-memory.md";

xllm_memory_sync_file_event_init(&events[1]);
events[1].eKind = XLLM_MEMORY_FILE_EVENT_DELETED;
events[1].sPath = "docs/old.md";

xllm_memory_sync_file_events_options_init(&options);
xllm_memory_change_set_init(&changes);

options.tBaseOptions.sRootPath = "D:/git/xllm";
options.tBaseOptions.sRecordIdPrefix = "workspace:xllm:";
options.pItems = events;
options.iItemCount = 2;

xllm_memory_sync_file_events(memory, &options, &changes, NULL);

for (size_t i = 0; i < changes.iChangeCount; ++i) {
    printf("change kind=%d\n", (int)changes.pChanges[i].eKind);
}

xllm_memory_change_set_reset(&changes);
```

## 常见错误

### 用 ingest_workspace 期待删除旧记录

`xllm_memory_ingest_workspace` 负责导入和更新文件，不负责清理 memory 中已经不存在的文件记录。需要清理旧记录时，使用 `xllm_memory_sync_workspace`。

### 没有设置 root path

同步单个文件或文件事件时，建议设置 `sRootPath`。这样 xllm 可以稳定地生成相对路径、source URI 和 record id。

### 忘记释放 change set

`xllm_memory_change_set` 里包含记录、跳过原因、失败原因等详情。使用后调用 `xllm_memory_change_set_reset`。

## 相关文档

- [Memory Ingest API](api-memory-ingest.md)
- [Memory Watcher API](api-memory-watcher.md)
- [Memory Search API](api-memory-search.md)
- [返回 API 索引](README.md)
