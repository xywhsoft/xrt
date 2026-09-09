# Replay API

Replay API 提供 deterministic replay cassette，用于记录、加载、重放和审计 agent run 中的模型、工具、host service、filesystem ref、checkpoint 和事件序列。

## 模块边界

- replay engine 不恢复 live terminal session、OS process handle 或外部网络连接。
- strict/audit replay 依赖已记录 cassette；未记录的外部副作用无法保证 exactly-once。
- runtime 可以借用 replay engine 集成 host service replay；replayed output 指针属于 runtime scratch buffer。
- replay engine 不提供内部同步；多线程 record/replay 同一 engine 时调用方必须串行化。

## 模式

| 模式 | 说明 |
| --- | --- |
| `XWORK_REPLAY_MODE_RECORD` | 记录 entry/event/ref。 |
| `XWORK_REPLAY_MODE_STRICT` | 按 cassette 对比，遇到 divergence 失败。 |
| `XWORK_REPLAY_MODE_AUDIT` | 记录 divergence，用于审计对比。 |

## 初始化与释放 API

### xwork_replay_options_init

初始化 replay engine 创建参数。

**功能：**

设置 replay options 默认值。

**函数原型：**

```c
XWORK_API void xwork_replay_options_init(xwork_replay_options *pOptions);
```

**参数：**

- `pOptions`：要初始化的 options；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

默认 mode 为 record，默认 readonly filesystem 为 true。

**范例代码：**

```c
xwork_replay_options opts;
xwork_replay_options_init(&opts);
opts.sReplayId = "replay-1";
```

**相关 API：**

- `xwork_replay_engine_create`

---

### xwork_replay_manifest_init

初始化 replay manifest。

**功能：**

准备 manifest 输出结构。

**函数原型：**

```c
XWORK_API void xwork_replay_manifest_init(xwork_replay_manifest *pManifest);
```

**参数：**

- `pManifest`：要初始化的 manifest；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

调用 `xwork_replay_engine_get_manifest` 前应初始化。

**范例代码：**

```c
xwork_replay_manifest manifest;
xwork_replay_manifest_init(&manifest);
```

**相关 API：**

- `xwork_replay_engine_get_manifest`

---

### xwork_replay_manifest_reset

释放 replay manifest。

**功能：**

释放 manifest 中的 id、source run id、时间和 hash algorithm 字符串。

**函数原型：**

```c
XWORK_API void xwork_replay_manifest_reset(xwork_replay_manifest *pManifest);
```

**参数：**

- `pManifest`：要释放的 manifest；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 manifest 内部 deep-copy 字符串。

**补充说明：**

调用后恢复为空状态。

**范例代码：**

```c
xwork_replay_manifest_reset(&manifest);
```

**相关 API：**

- `xwork_replay_manifest_init`

---

### xwork_replay_entry_options_init

初始化 replay entry options。

**功能：**

准备一条模型、工具、host service、process、terminal、artifact 或 checkpoint cassette entry。

**函数原型：**

```c
XWORK_API void xwork_replay_entry_options_init(xwork_replay_entry_options *pOptions);
```

**参数：**

- `pOptions`：要初始化的 options；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源；record/load API 会复制需要保留的字段。

**补充说明：**

默认 entry kind 为 model，默认 status 为 `XWORK_OK`。

**范例代码：**

```c
xwork_replay_entry_options entry;
xwork_replay_entry_options_init(&entry);
entry.sKey = "model:1";
```

**相关 API：**

- `xwork_replay_engine_record_entry`

---

### xwork_replay_entry_summary_init

初始化 replay entry summary。

**功能：**

准备 entry replay/list 输出结构。

**函数原型：**

```c
XWORK_API void xwork_replay_entry_summary_init(xwork_replay_entry_summary *pSummary);
```

**参数：**

- `pSummary`：要初始化的 summary；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

默认 kind 为 model，status 为 `XWORK_OK`。

**范例代码：**

```c
xwork_replay_entry_summary summary;
xwork_replay_entry_summary_init(&summary);
```

**相关 API：**

- `xwork_replay_engine_replay_entry`

---

### xwork_replay_entry_summary_reset

释放 replay entry summary。

**功能：**

释放 entry summary 中的 key、operation、payload 和 hash 字符串。

**函数原型：**

```c
XWORK_API void xwork_replay_entry_summary_reset(xwork_replay_entry_summary *pSummary);
```

**参数：**

- `pSummary`：要释放的 summary；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放内部 deep-copy 字符串。

**补充说明：**

适用于 replay/list API 填充的输出。

**范例代码：**

```c
xwork_replay_entry_summary_reset(&summary);
```

**相关 API：**

- `xwork_replay_entry_summary_init`

---

### xwork_replay_entry_summary_list_init

初始化 replay entry summary 列表。

**功能：**

准备空 entry summary list。

**函数原型：**

```c
XWORK_API void xwork_replay_entry_summary_list_init(xwork_replay_entry_summary_list *pList);
```

**参数：**

- `pList`：要初始化的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

调用 `xwork_replay_engine_list_entries` 前应初始化。

**范例代码：**

```c
xwork_replay_entry_summary_list list;
xwork_replay_entry_summary_list_init(&list);
```

**相关 API：**

- `xwork_replay_engine_list_entries`

---

### xwork_replay_entry_summary_list_reset

释放 replay entry summary 列表。

**功能：**

释放列表中所有 entry summary 和数组。

**函数原型：**

```c
XWORK_API void xwork_replay_entry_summary_list_reset(xwork_replay_entry_summary_list *pList);
```

**参数：**

- `pList`：要释放的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放列表拥有的内容。

**补充说明：**

释放后列表可复用。

**范例代码：**

```c
xwork_replay_entry_summary_list_reset(&list);
```

**相关 API：**

- `xwork_replay_entry_summary_reset`

---

### xwork_replay_filesystem_ref_options_init

初始化 filesystem ref options。

**功能：**

准备 filesystem snapshot/ref cassette 记录。

**函数原型：**

```c
XWORK_API void xwork_replay_filesystem_ref_options_init(
    xwork_replay_filesystem_ref_options *pOptions
);
```

**参数：**

- `pOptions`：要初始化的 options；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

typed filesystem ref 会映射为 replay entry 的稳定 operation id。

**范例代码：**

```c
xwork_replay_filesystem_ref_options ref;
xwork_replay_filesystem_ref_options_init(&ref);
ref.sRefId = "fs-1";
```

**相关 API：**

- `xwork_replay_engine_record_filesystem_ref`

---

### xwork_replay_filesystem_ref_summary_init

初始化 filesystem ref summary。

**功能：**

准备 filesystem ref replay/list 输出结构。

**函数原型：**

```c
XWORK_API void xwork_replay_filesystem_ref_summary_init(
    xwork_replay_filesystem_ref_summary *pSummary
);
```

**参数：**

- `pSummary`：要初始化的 summary；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

调用 replay/list filesystem refs 前应初始化。

**范例代码：**

```c
xwork_replay_filesystem_ref_summary summary;
xwork_replay_filesystem_ref_summary_init(&summary);
```

**相关 API：**

- `xwork_replay_engine_replay_filesystem_ref`

---

### xwork_replay_filesystem_ref_summary_reset

释放 filesystem ref summary。

**功能：**

释放 ref id、path、metadata JSON 和 content hash。

**函数原型：**

```c
XWORK_API void xwork_replay_filesystem_ref_summary_reset(
    xwork_replay_filesystem_ref_summary *pSummary
);
```

**参数：**

- `pSummary`：要释放的 summary；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放内部 deep-copy 字符串。

**补充说明：**

调用后恢复为空状态。

**范例代码：**

```c
xwork_replay_filesystem_ref_summary_reset(&summary);
```

**相关 API：**

- `xwork_replay_filesystem_ref_summary_init`

---

### xwork_replay_filesystem_ref_summary_list_init

初始化 filesystem ref summary 列表。

**功能：**

准备空 filesystem ref summary list。

**函数原型：**

```c
XWORK_API void xwork_replay_filesystem_ref_summary_list_init(
    xwork_replay_filesystem_ref_summary_list *pList
);
```

**参数：**

- `pList`：要初始化的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

调用 `xwork_replay_engine_list_filesystem_refs` 前应初始化。

**范例代码：**

```c
xwork_replay_filesystem_ref_summary_list list;
xwork_replay_filesystem_ref_summary_list_init(&list);
```

**相关 API：**

- `xwork_replay_engine_list_filesystem_refs`

---

### xwork_replay_filesystem_ref_summary_list_reset

释放 filesystem ref summary 列表。

**功能：**

释放列表中所有 filesystem ref summary 和数组。

**函数原型：**

```c
XWORK_API void xwork_replay_filesystem_ref_summary_list_reset(
    xwork_replay_filesystem_ref_summary_list *pList
);
```

**参数：**

- `pList`：要释放的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放列表拥有的内容。

**补充说明：**

释放后列表可复用。

**范例代码：**

```c
xwork_replay_filesystem_ref_summary_list_reset(&list);
```

**相关 API：**

- `xwork_replay_filesystem_ref_summary_reset`

---

### xwork_replay_event_options_init

初始化 replay event options。

**功能：**

准备一条 replay event 记录。

**函数原型：**

```c
XWORK_API void xwork_replay_event_options_init(xwork_replay_event_options *pOptions);
```

**参数：**

- `pOptions`：要初始化的 options；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源；record/load API 会复制需要保留的字段。

**补充说明：**

默认 event kind 为 generic，status 为 `XWORK_OK`。

**范例代码：**

```c
xwork_replay_event_options event;
xwork_replay_event_options_init(&event);
event.sKey = "event:1";
```

**相关 API：**

- `xwork_replay_engine_record_event`

---

### xwork_replay_event_options_from_model_event

从 model event 构造 replay event options。

**功能：**

将 `xwork_model_event` 映射为 replay event options，便于记录模型流事件。

**函数原型：**

```c
XWORK_API void xwork_replay_event_options_from_model_event(
    const xwork_model_event *pEvent,
    xwork_replay_event_options *pOptions
);
```

**参数：**

- `pEvent`：源 model event；可为 `NULL`。
- `pOptions`：输出 options；必须非 `NULL` 才会写入。

**返回值：**

无。

**资源归属：**

不分配资源；输出中的指针借用源 event。

**补充说明：**

调用者如果需要长期保存，应立即交给 record/load API 复制。

**范例代码：**

```c
xwork_replay_event_options opts;
xwork_replay_event_options_from_model_event(&modelEvent, &opts);
```

**相关 API：**

- `xwork_replay_engine_record_event`

---

### xwork_replay_event_summary_init

初始化 replay event summary。

**功能：**

准备 event replay/list 输出结构。

**函数原型：**

```c
XWORK_API void xwork_replay_event_summary_init(xwork_replay_event_summary *pSummary);
```

**参数：**

- `pSummary`：要初始化的 summary；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

默认 event kind 为 generic，status 为 `XWORK_OK`。

**范例代码：**

```c
xwork_replay_event_summary summary;
xwork_replay_event_summary_init(&summary);
```

**相关 API：**

- `xwork_replay_engine_replay_event`

---

### xwork_replay_event_summary_reset

释放 replay event summary。

**功能：**

释放 event key、name、payload hash 和 content hash。

**函数原型：**

```c
XWORK_API void xwork_replay_event_summary_reset(xwork_replay_event_summary *pSummary);
```

**参数：**

- `pSummary`：要释放的 summary；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放内部 deep-copy 字符串。

**补充说明：**

适用于 replay/list API 填充的输出。

**范例代码：**

```c
xwork_replay_event_summary_reset(&summary);
```

**相关 API：**

- `xwork_replay_event_summary_init`

---

### xwork_replay_event_summary_list_init

初始化 replay event summary 列表。

**功能：**

准备空 event summary list。

**函数原型：**

```c
XWORK_API void xwork_replay_event_summary_list_init(xwork_replay_event_summary_list *pList);
```

**参数：**

- `pList`：要初始化的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

调用 `xwork_replay_engine_list_events` 前应初始化。

**范例代码：**

```c
xwork_replay_event_summary_list list;
xwork_replay_event_summary_list_init(&list);
```

**相关 API：**

- `xwork_replay_engine_list_events`

---

### xwork_replay_event_summary_list_reset

释放 replay event summary 列表。

**功能：**

释放列表中所有 event summary 和数组。

**函数原型：**

```c
XWORK_API void xwork_replay_event_summary_list_reset(xwork_replay_event_summary_list *pList);
```

**参数：**

- `pList`：要释放的列表；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放列表拥有的内容。

**补充说明：**

释放后列表可复用。

**范例代码：**

```c
xwork_replay_event_summary_list_reset(&list);
```

**相关 API：**

- `xwork_replay_event_summary_reset`

---

### xwork_replay_divergence_init

初始化 replay divergence。

**功能：**

准备 divergence 输出结构。

**函数原型：**

```c
XWORK_API void xwork_replay_divergence_init(xwork_replay_divergence *pDivergence);
```

**参数：**

- `pDivergence`：要初始化的 divergence；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

用于保存 replay 对比中的第一处差异。

**范例代码：**

```c
xwork_replay_divergence divergence;
xwork_replay_divergence_init(&divergence);
```

**相关 API：**

- `xwork_replay_engine_get_first_divergence`

---

### xwork_replay_divergence_reset

释放 replay divergence。

**功能：**

释放 divergence 中的 expected/actual key、hash 和 message。

**函数原型：**

```c
XWORK_API void xwork_replay_divergence_reset(xwork_replay_divergence *pDivergence);
```

**参数：**

- `pDivergence`：要释放的 divergence；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放内部 deep-copy 字符串。

**补充说明：**

`xwork_replay_result_reset` 会重置内部 first divergence。

**范例代码：**

```c
xwork_replay_divergence_reset(&divergence);
```

**相关 API：**

- `xwork_replay_result_reset`

---

### xwork_replay_result_init

初始化 replay result。

**功能：**

准备 replay engine 统计结果输出结构。

**函数原型：**

```c
XWORK_API void xwork_replay_result_init(xwork_replay_result *pResult);
```

**参数：**

- `pResult`：要初始化的 result；可为 `NULL`。

**返回值：**

无。

**资源归属：**

不分配资源。

**补充说明：**

默认 status 为 `XWORK_OK`，并初始化 first divergence。

**范例代码：**

```c
xwork_replay_result result;
xwork_replay_result_init(&result);
```

**相关 API：**

- `xwork_replay_engine_get_result`

---

### xwork_replay_result_reset

释放 replay result。

**功能：**

释放 replay result 内部的 first divergence。

**函数原型：**

```c
XWORK_API void xwork_replay_result_reset(xwork_replay_result *pResult);
```

**参数：**

- `pResult`：要释放的 result；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放内部 deep-copy divergence 字段。

**补充说明：**

调用后 result 恢复为 init 状态。

**范例代码：**

```c
xwork_replay_result_reset(&result);
```

**相关 API：**

- `xwork_replay_result_init`

---

## Hash API

### xwork_replay_hash_text

计算文本 replay hash。

**功能：**

对普通文本生成 replay content hash。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_hash_text(
    const char *sText,
    char *sBuffer,
    size_t iBufferSize
);
```

**参数：**

- `sText`：输入文本；必须非 `NULL`。
- `sBuffer`：输出缓冲区；必须非 `NULL`。
- `iBufferSize`：输出缓冲区大小，单位字节。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

不分配资源；hash 写入调用方缓冲区。

**补充说明：**

缓冲区必须足够保存 hash 字符串和结尾 `\0`。

**范例代码：**

```c
char hash[128];
xwork_replay_hash_text("hello", hash, sizeof(hash));
```

**相关 API：**

- `xwork_replay_hash_json`

---

### xwork_replay_hash_json

计算规范化 JSON replay hash。

**功能：**

先规范化 JSON，再生成 replay hash。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_hash_json(
    const char *sJson,
    char *sBuffer,
    size_t iBufferSize
);
```

**参数：**

- `sJson`：输入 JSON 字符串；必须是有效 JSON。
- `sBuffer`：输出缓冲区；必须非 `NULL`。
- `iBufferSize`：输出缓冲区大小，单位字节。

**返回值：**

返回 `XWORK_OK`；无效 JSON 返回 `XWORK_ERROR_INVALID_ARGUMENT`。

**资源归属：**

不分配资源；hash 写入调用方缓冲区。

**补充说明：**

规范化会排序对象 key 并忽略无意义空白；entry JSON 字段优先使用该 hash。

**范例代码：**

```c
char hash[128];
xwork_replay_hash_json("{\"a\":1}", hash, sizeof(hash));
```

**相关 API：**

- `xwork_replay_hash_text`

---

## Engine API

### xwork_replay_engine_create

创建 replay engine。

**功能：**

创建 record/strict/audit replay engine。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_create(
    const xwork_replay_options *pOptions,
    xwork_replay_engine **ppEngine
);
```

**参数：**

- `pOptions`：创建参数；可为 `NULL` 使用默认值。
- `ppEngine`：输出 owned engine。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

成功后 engine 归调用者所有，用 `xwork_replay_engine_destroy` 释放。

**补充说明：**

engine 可借给 runtime，但 runtime 不接管其生命周期。

**范例代码：**

```c
xwork_replay_engine *engine = NULL;
xwork_replay_engine_create(&opts, &engine);
```

**相关 API：**

- `xwork_replay_engine_destroy`

---

### xwork_replay_engine_destroy

销毁 replay engine。

**功能：**

释放 engine、entry cassette、event cassette、filesystem refs 和 divergence 记录。

**函数原型：**

```c
XWORK_API void xwork_replay_engine_destroy(xwork_replay_engine *pEngine);
```

**参数：**

- `pEngine`：要销毁的 engine；可为 `NULL`。

**返回值：**

无。

**资源归属：**

释放 engine 拥有资源；不会释放借用该 engine 的 runtime。

**补充说明：**

确保 runtime 不再使用该 engine 后再销毁。

**范例代码：**

```c
xwork_replay_engine_destroy(engine);
```

**相关 API：**

- `xwork_replay_engine_create`

---

### xwork_replay_engine_get_mode

获取 replay engine 模式。

**功能：**

返回 engine 当前 record/strict/audit 模式。

**函数原型：**

```c
XWORK_API xwork_replay_mode xwork_replay_engine_get_mode(
    const xwork_replay_engine *pEngine
);
```

**参数：**

- `pEngine`：replay engine；可为 `NULL`。

**返回值：**

返回 mode；`pEngine` 为 `NULL` 时返回 record。

**资源归属：**

不分配资源。

**补充说明：**

用于 runtime 集成和诊断。

**范例代码：**

```c
xwork_replay_mode mode = xwork_replay_engine_get_mode(engine);
```

**相关 API：**

- `xwork_replay_engine_blocks_side_effects`

---

### xwork_replay_engine_blocks_side_effects

检查 replay 是否阻断副作用。

**功能：**

返回 engine 是否配置为阻断 side effect host service。

**函数原型：**

```c
XWORK_API bool xwork_replay_engine_blocks_side_effects(
    const xwork_replay_engine *pEngine
);
```

**参数：**

- `pEngine`：replay engine；可为 `NULL`。

**返回值：**

阻断副作用返回 `true`；否则返回 `false`。

**资源归属：**

不分配资源。

**补充说明：**

record mode + side-effect blocking 会在真实副作用执行前暂停。

**范例代码：**

```c
bool blocked = xwork_replay_engine_blocks_side_effects(engine);
```

**相关 API：**

- `xwork_replay_options_init`

---

### xwork_replay_engine_record_entry

记录 replay entry。

**功能：**

向 record-mode cassette 追加一条 entry。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_record_entry(
    xwork_replay_engine *pEngine,
    const xwork_replay_entry_options *pEntry
);
```

**参数：**

- `pEngine`：目标 engine。
- `pEntry`：entry 参数。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

engine 复制 entry 字段。

**补充说明：**

非 record mode 调用通常视为无效状态。

**范例代码：**

```c
xwork_replay_engine_record_entry(engine, &entry);
```

**相关 API：**

- `xwork_replay_engine_replay_entry`

---

### xwork_replay_engine_load_entry

加载 replay entry。

**功能：**

从持久化 cassette 预加载 entry，供 strict/audit replay 对比。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_load_entry(
    xwork_replay_engine *pEngine,
    const xwork_replay_entry_options *pEntry
);
```

**参数：**

- `pEngine`：目标 engine。
- `pEntry`：要加载的 entry。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

engine 复制 entry 字段。

**补充说明：**

load 不增加 recorded count，主要用于恢复 cassette。

**范例代码：**

```c
xwork_replay_engine_load_entry(engine, &entry);
```

**相关 API：**

- `xwork_replay_engine_replay_entry`

---

### xwork_replay_engine_replay_entry

重放并对比 replay entry。

**功能：**

把 expected entry 与 cassette 中下一条 entry 对比，并返回实际 entry summary。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_replay_entry(
    xwork_replay_engine *pEngine,
    const xwork_replay_entry_options *pExpected,
    xwork_replay_entry_summary *pActual
);
```

**参数：**

- `pEngine`：目标 engine。
- `pExpected`：本次期望 entry。
- `pActual`：可选输出实际 entry summary。

**返回值：**

匹配返回 `XWORK_OK`；不匹配按模式返回 divergence 相关错误。

**资源归属：**

`pActual` 如被填充，调用者用 `xwork_replay_entry_summary_reset` 释放。

**补充说明：**

strict mode 遇到 divergence 会失败；audit mode 会记录 divergence 继续审计。

**范例代码：**

```c
xwork_replay_entry_summary actual;
xwork_replay_entry_summary_init(&actual);
xwork_replay_engine_replay_entry(engine, &expected, &actual);
xwork_replay_entry_summary_reset(&actual);
```

**相关 API：**

- `xwork_replay_engine_get_first_divergence`

---

### xwork_replay_engine_record_filesystem_ref

记录 filesystem ref。

**功能：**

以 typed wrapper 记录 filesystem snapshot/ref entry。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_record_filesystem_ref(
    xwork_replay_engine *pEngine,
    const xwork_replay_filesystem_ref_options *pRef
);
```

**参数：**

- `pEngine`：目标 engine。
- `pRef`：filesystem ref 参数。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

engine 复制 ref 字段。

**补充说明：**

适合记录 workspace/file snapshot 引用，而不是文件内容本体。

**范例代码：**

```c
xwork_replay_engine_record_filesystem_ref(engine, &ref);
```

**相关 API：**

- `xwork_replay_engine_replay_filesystem_ref`

---

### xwork_replay_engine_load_filesystem_ref

加载 filesystem ref。

**功能：**

从持久化数据加载 typed filesystem ref cassette。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_load_filesystem_ref(
    xwork_replay_engine *pEngine,
    const xwork_replay_filesystem_ref_options *pRef
);
```

**参数：**

- `pEngine`：目标 engine。
- `pRef`：要加载的 ref。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

engine 复制 ref 字段。

**补充说明：**

用于 replay 前恢复 cassette。

**范例代码：**

```c
xwork_replay_engine_load_filesystem_ref(engine, &ref);
```

**相关 API：**

- `xwork_replay_engine_list_filesystem_refs`

---

### xwork_replay_engine_replay_filesystem_ref

重放 filesystem ref。

**功能：**

对比 expected filesystem ref 与 cassette 中的实际记录。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_replay_filesystem_ref(
    xwork_replay_engine *pEngine,
    const xwork_replay_filesystem_ref_options *pExpected,
    xwork_replay_filesystem_ref_summary *pActual
);
```

**参数：**

- `pEngine`：目标 engine。
- `pExpected`：期望 ref。
- `pActual`：可选输出实际 ref summary。

**返回值：**

返回 `XWORK_OK` 或 divergence/错误码。

**资源归属：**

`pActual` 如被填充，调用者 reset。

**补充说明：**

只对比 ref 元数据和 hash，不恢复 live filesystem state。

**范例代码：**

```c
xwork_replay_filesystem_ref_summary actual;
xwork_replay_filesystem_ref_summary_init(&actual);
xwork_replay_engine_replay_filesystem_ref(engine, &expected, &actual);
xwork_replay_filesystem_ref_summary_reset(&actual);
```

**相关 API：**

- `xwork_replay_engine_record_filesystem_ref`

---

### xwork_replay_engine_list_filesystem_refs

列出 filesystem refs。

**功能：**

获取 engine 中已记录或加载的 filesystem refs。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_list_filesystem_refs(
    const xwork_replay_engine *pEngine,
    xwork_replay_filesystem_ref_summary_list *pList
);
```

**参数：**

- `pEngine`：源 engine。
- `pList`：输出列表；调用前应 init。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

列表拥有 deep-copy 内容，用 reset 释放。

**补充说明：**

用于审计 replay cassette 中引用了哪些 filesystem snapshot。

**范例代码：**

```c
xwork_replay_filesystem_ref_summary_list list;
xwork_replay_filesystem_ref_summary_list_init(&list);
xwork_replay_engine_list_filesystem_refs(engine, &list);
xwork_replay_filesystem_ref_summary_list_reset(&list);
```

**相关 API：**

- `xwork_replay_filesystem_ref_summary_list_reset`

---

### xwork_replay_engine_record_event

记录 replay event。

**功能：**

向 event cassette 追加一条事件记录。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_record_event(
    xwork_replay_engine *pEngine,
    const xwork_replay_event_options *pEvent
);
```

**参数：**

- `pEngine`：目标 engine。
- `pEvent`：事件参数。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

engine 复制 event 字段。

**补充说明：**

用于模型流、run event、tool event、terminal interaction 等顺序审计。

**范例代码：**

```c
xwork_replay_engine_record_event(engine, &event);
```

**相关 API：**

- `xwork_replay_engine_replay_event`

---

### xwork_replay_engine_load_event

加载 replay event。

**功能：**

从持久化 cassette 加载事件记录。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_load_event(
    xwork_replay_engine *pEngine,
    const xwork_replay_event_options *pEvent
);
```

**参数：**

- `pEngine`：目标 engine。
- `pEvent`：要加载的事件。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

engine 复制 event 字段。

**补充说明：**

load 用于 replay 前恢复 event cassette。

**范例代码：**

```c
xwork_replay_engine_load_event(engine, &event);
```

**相关 API：**

- `xwork_replay_engine_replay_event`

---

### xwork_replay_engine_replay_event

重放并对比 replay event。

**功能：**

把 expected event 与 cassette 中下一条 event 对比。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_replay_event(
    xwork_replay_engine *pEngine,
    const xwork_replay_event_options *pExpected,
    xwork_replay_event_summary *pActual
);
```

**参数：**

- `pEngine`：目标 engine。
- `pExpected`：期望事件。
- `pActual`：可选输出实际事件摘要。

**返回值：**

返回 `XWORK_OK` 或 divergence/错误码。

**资源归属：**

`pActual` 如被填充，调用者 reset。

**补充说明：**

事件 replay 重点检查顺序、key、kind、hash 和 status。

**范例代码：**

```c
xwork_replay_event_summary actual;
xwork_replay_event_summary_init(&actual);
xwork_replay_engine_replay_event(engine, &expected, &actual);
xwork_replay_event_summary_reset(&actual);
```

**相关 API：**

- `xwork_replay_engine_record_event`

---

### xwork_replay_engine_seek_checkpoint

定位 checkpoint。

**功能：**

将 replay 游标推进到指定 checkpoint 相关位置。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_seek_checkpoint(
    xwork_replay_engine *pEngine,
    const char *sCheckpointId
);
```

**参数：**

- `pEngine`：目标 engine。
- `sCheckpointId`：checkpoint id。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

不分配资源。

**补充说明：**

用于恢复后从特定 checkpoint 继续 replay。

**范例代码：**

```c
xwork_replay_engine_seek_checkpoint(engine, "checkpoint-1");
```

**相关 API：**

- `xwork_checkpoint`

---

### xwork_replay_engine_emit_report_artifact

生成 replay 报告 artifact。

**功能：**

将 replay manifest、统计结果和 divergence 信息写入 artifact。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_emit_report_artifact(
    const xwork_replay_engine *pEngine,
    xwork_run *pRun,
    const char *sArtifactId,
    xwork_artifact *pArtifact
);
```

**参数：**

- `pEngine`：源 engine。
- `pRun`：接收 artifact 的 run。
- `sArtifactId`：artifact id。
- `pArtifact`：输出 artifact。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

artifact 内容由输出对象持有，调用者按 artifact API reset。

**补充说明：**

适合作为审计报告或 CI 产物。

**范例代码：**

```c
xwork_artifact artifact;
xwork_artifact_init(&artifact);
xwork_replay_engine_emit_report_artifact(engine, run, "replay-report", &artifact);
xwork_artifact_reset(&artifact);
```

**相关 API：**

- `xwork_run_emit_report_artifact`

---

### xwork_replay_engine_cancel

取消 replay engine。

**功能：**

标记 replay engine 已取消，并记录取消原因。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_cancel(
    xwork_replay_engine *pEngine,
    const char *sReason
);
```

**参数：**

- `pEngine`：目标 engine。
- `sReason`：可选取消原因。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

engine 复制原因字符串。

**补充说明：**

取消不会停止外部进程或 live terminal。

**范例代码：**

```c
xwork_replay_engine_cancel(engine, "user cancelled replay");
```

**相关 API：**

- `xwork_replay_engine_get_result`

---

### xwork_replay_engine_get_manifest

获取 replay manifest。

**功能：**

返回 replay id、manifest id、source run id、hash algorithm 和 entry count。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_get_manifest(
    const xwork_replay_engine *pEngine,
    xwork_replay_manifest *pManifest
);
```

**参数：**

- `pEngine`：源 engine。
- `pManifest`：输出 manifest；调用前应 init。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

manifest 拥有 deep-copy 内容，用 reset 释放。

**补充说明：**

函数会重置输出 manifest 的旧内容。

**范例代码：**

```c
xwork_replay_manifest manifest;
xwork_replay_manifest_init(&manifest);
xwork_replay_engine_get_manifest(engine, &manifest);
xwork_replay_manifest_reset(&manifest);
```

**相关 API：**

- `xwork_replay_manifest_reset`

---

### xwork_replay_engine_get_result

获取 replay 结果。

**功能：**

返回 recorded/replayed/divergence 计数和第一处 divergence。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_get_result(
    const xwork_replay_engine *pEngine,
    xwork_replay_result *pResult
);
```

**参数：**

- `pEngine`：源 engine。
- `pResult`：输出 result；调用前应 init。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

result 内部 divergence 拥有 deep-copy 内容，用 reset 释放。

**补充说明：**

用于 replay 完成后的 gate 判断和报告生成。

**范例代码：**

```c
xwork_replay_result result;
xwork_replay_result_init(&result);
xwork_replay_engine_get_result(engine, &result);
xwork_replay_result_reset(&result);
```

**相关 API：**

- `xwork_replay_engine_get_first_divergence`

---

### xwork_replay_engine_get_first_divergence

获取第一处 divergence。

**功能：**

返回 replay 过程中记录的第一处差异。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_get_first_divergence(
    const xwork_replay_engine *pEngine,
    xwork_replay_divergence *pDivergence
);
```

**参数：**

- `pEngine`：源 engine。
- `pDivergence`：输出 divergence；调用前应 init。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

divergence 拥有 deep-copy 内容，用 reset 释放。

**补充说明：**

无 divergence 时输出保持空语义。

**范例代码：**

```c
xwork_replay_divergence divergence;
xwork_replay_divergence_init(&divergence);
xwork_replay_engine_get_first_divergence(engine, &divergence);
xwork_replay_divergence_reset(&divergence);
```

**相关 API：**

- `xwork_replay_engine_get_result`

---

### xwork_replay_engine_list_entries

列出 replay entries。

**功能：**

获取 engine 中所有 entry summary。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_list_entries(
    const xwork_replay_engine *pEngine,
    xwork_replay_entry_summary_list *pList
);
```

**参数：**

- `pEngine`：源 engine。
- `pList`：输出列表；调用前应 init。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

列表拥有 deep-copy 内容，用 reset 释放。

**补充说明：**

用于调试 cassette 内容和持久化导出。

**范例代码：**

```c
xwork_replay_entry_summary_list list;
xwork_replay_entry_summary_list_init(&list);
xwork_replay_engine_list_entries(engine, &list);
xwork_replay_entry_summary_list_reset(&list);
```

**相关 API：**

- `xwork_replay_engine_record_entry`

---

### xwork_replay_engine_list_events

列出 replay events。

**功能：**

获取 engine 中所有 event summary。

**函数原型：**

```c
XWORK_API xwork_status xwork_replay_engine_list_events(
    const xwork_replay_engine *pEngine,
    xwork_replay_event_summary_list *pList
);
```

**参数：**

- `pEngine`：源 engine。
- `pList`：输出列表；调用前应 init。

**返回值：**

返回 `XWORK_OK` 或错误码。

**资源归属：**

列表拥有 deep-copy 内容，用 reset 释放。

**补充说明：**

用于事件序列审计和 replay 报告。

**范例代码：**

```c
xwork_replay_event_summary_list list;
xwork_replay_event_summary_list_init(&list);
xwork_replay_engine_list_events(engine, &list);
xwork_replay_event_summary_list_reset(&list);
```

**相关 API：**

- `xwork_replay_engine_record_event`

---

## 相关文档

- [Persistence API](api-persistence.md)
- [Replay Agent Run 范例](../case/replay-agent-run.md)
- [持久化、checkpoint 与 replay](../guide/persistence-replay-intro.md)
- [内部 replay contract](../../dev/docs/REPLAY.md)
