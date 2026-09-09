# Replay API

The Replay API provides deterministic replay cassettes for recording, loading, replaying, and auditing models, tools, host services, filesystem refs, checkpoints, and event sequences in agent runs.

## Module boundaries

- The replay engine does not restore live terminal sessions, OS process handles, or external network connections.
- strict/audit replay relies on documented cassette; undocumented external side effects cannot be guaranteed exactly-once.
- The runtime can use the replay engine to integrate host service replay; the replayed output pointer belongs to the runtime scratch buffer.
- The replay engine does not provide internal synchronization; the caller must serialize when multi-threaded record/replay the same engine.

## model

| Mode | Description |
| --- | --- |
| `XWORK_REPLAY_MODE_RECORD` | Record entry/event/ref. |
| `XWORK_REPLAY_MODE_STRICT` | Press cassette to compare and encounter divergence failure. |
| `XWORK_REPLAY_MODE_AUDIT` | Record divergence for audit comparison. |

## Initialization and release API

### xwork_replay_options_init

Initialize replay engine creation parameters.

**Function:**

Set replay options default values.

**Function prototype:**

```c
XWORK_API void xwork_replay_options_init(xwork_replay_options *pOptions);
```

**parameter:**

- `pOptions`: options to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The default mode is record, and the default readonly filesystem is true.

**Example code:**

```c
xwork_replay_options opts;
xwork_replay_options_init(&opts);
opts.sReplayId = "replay-1";
```

**Related API:**

- `xwork_replay_engine_create`

---

### xwork_replay_manifest_init

Initialize replay manifest.

**Function:**

Prepare the manifest output structure.

**Function prototype:**

```c
XWORK_API void xwork_replay_manifest_init(xwork_replay_manifest *pManifest);
```

**parameter:**

- `pManifest`: Manifest to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

It should be initialized before calling `xwork_replay_engine_get_manifest`.

**Example code:**

```c
xwork_replay_manifest manifest;
xwork_replay_manifest_init(&manifest);
```

**Related API:**

- `xwork_replay_engine_get_manifest`

---

### xwork_replay_manifest_reset

Release the replay manifest.

**Function:**

Release the id, source run id, time and hash algorithm strings in the manifest.

**Function prototype:**

```c
XWORK_API void xwork_replay_manifest_reset(xwork_replay_manifest *pManifest);
```

**parameter:**

- `pManifest`: Manifest to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Release the deep-copy string inside the manifest.

**Additional Note:**

Returns to empty state after calling.

**Example code:**

```c
xwork_replay_manifest_reset(&manifest);
```

**Related API:**

- `xwork_replay_manifest_init`

---

### xwork_replay_entry_options_init

Initialize replay entry options.

**Function:**

Prepare a model, tool, host service, process, terminal, artifact or checkpoint cassette entry.

**Function prototype:**

```c
XWORK_API void xwork_replay_entry_options_init(xwork_replay_entry_options *pOptions);
```

**parameter:**

- `pOptions`: options to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated; the record/load API copies fields that need to be retained.

**Additional Note:**

The default entry kind is model, and the default status is `XWORK_OK`.

**Example code:**

```c
xwork_replay_entry_options entry;
xwork_replay_entry_options_init(&entry);
entry.sKey = "model:1";
```

**Related API:**

- `xwork_replay_engine_record_entry`

---

### xwork_replay_entry_summary_init

Initialize replay entry summary.

**Function:**

Prepare entry replay/list output structure.

**Function prototype:**

```c
XWORK_API void xwork_replay_entry_summary_init(xwork_replay_entry_summary *pSummary);
```

**parameter:**

- `pSummary`: summary to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The default kind is model and status is `XWORK_OK`.

**Example code:**

```c
xwork_replay_entry_summary summary;
xwork_replay_entry_summary_init(&summary);
```

**Related API:**

- `xwork_replay_engine_replay_entry`

---

### xwork_replay_entry_summary_reset

Release replay entry summary.

**Function:**

Release the key, operation, payload and hash strings in the entry summary.

**Function prototype:**

```c
XWORK_API void xwork_replay_entry_summary_reset(xwork_replay_entry_summary *pSummary);
```

**parameter:**

- `pSummary`: summary to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Free the internal deep-copy string.

**Additional Note:**

Applies to replay/list API populated output.

**Example code:**

```c
xwork_replay_entry_summary_reset(&summary);
```

**Related API:**

- `xwork_replay_entry_summary_init`

---

### xwork_replay_entry_summary_list_init

Initialize the replay entry summary list.

**Function:**

Prepare an empty entry summary list.

**Function prototype:**

```c
XWORK_API void xwork_replay_entry_summary_list_init(xwork_replay_entry_summary_list *pList);
```

**parameter:**

- `pList`: List to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

It should be initialized before calling `xwork_replay_engine_list_entries`.

**Example code:**

```c
xwork_replay_entry_summary_list list;
xwork_replay_entry_summary_list_init(&list);
```

**Related API:**

- `xwork_replay_engine_list_entries`

---

### xwork_replay_entry_summary_list_reset

Release the replay entry summary list.

**Function:**

Free all entry summaries and arrays in the list.

**Function prototype:**

```c
XWORK_API void xwork_replay_entry_summary_list_reset(xwork_replay_entry_summary_list *pList);
```

**parameter:**

- `pList`: List to free; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the contents owned by the list.

**Additional Note:**

The list can be reused after it is released.

**Example code:**

```c
xwork_replay_entry_summary_list_reset(&list);
```

**Related API:**

- `xwork_replay_entry_summary_reset`

---

### xwork_replay_filesystem_ref_options_init

Initialize filesystem ref options.

**Function:**

Prepare filesystem snapshot/ref cassette records.

**Function prototype:**

```c
XWORK_API void xwork_replay_filesystem_ref_options_init(
    xwork_replay_filesystem_ref_options *pOptions
);
```

**parameter:**

- `pOptions`: options to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The typed filesystem ref will be mapped to the stable operation id of the replay entry.

**Example code:**

```c
xwork_replay_filesystem_ref_options ref;
xwork_replay_filesystem_ref_options_init(&ref);
ref.sRefId = "fs-1";
```

**Related API:**

- `xwork_replay_engine_record_filesystem_ref`

---

### xwork_replay_filesystem_ref_summary_init

Initialize filesystem ref summary.

**Function:**

Prepare filesystem ref replay/list output structure.

**Function prototype:**

```c
XWORK_API void xwork_replay_filesystem_ref_summary_init(
    xwork_replay_filesystem_ref_summary *pSummary
);
```

**parameter:**

- `pSummary`: summary to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

Filesystem refs should be initialized before calling replay/list.

**Example code:**

```c
xwork_replay_filesystem_ref_summary summary;
xwork_replay_filesystem_ref_summary_init(&summary);
```

**Related API:**

- `xwork_replay_engine_replay_filesystem_ref`

---

### xwork_replay_filesystem_ref_summary_reset

Release filesystem ref summary.

**Function:**

Release ref id, path, metadata JSON and content hash.

**Function prototype:**

```c
XWORK_API void xwork_replay_filesystem_ref_summary_reset(
    xwork_replay_filesystem_ref_summary *pSummary
);
```

**parameter:**

- `pSummary`: summary to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Free the internal deep-copy string.

**Additional Note:**

Returns to empty state after calling.

**Example code:**

```c
xwork_replay_filesystem_ref_summary_reset(&summary);
```

**Related API:**

- `xwork_replay_filesystem_ref_summary_init`

---

### xwork_replay_filesystem_ref_summary_list_init

Initialize the filesystem ref summary list.

**Function:**

Prepare an empty filesystem ref summary list.

**Function prototype:**

```c
XWORK_API void xwork_replay_filesystem_ref_summary_list_init(
    xwork_replay_filesystem_ref_summary_list *pList
);
```

**parameter:**

- `pList`: List to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

It should be initialized before calling `xwork_replay_engine_list_filesystem_refs`.

**Example code:**

```c
xwork_replay_filesystem_ref_summary_list list;
xwork_replay_filesystem_ref_summary_list_init(&list);
```

**Related API:**

- `xwork_replay_engine_list_filesystem_refs`

---

### xwork_replay_filesystem_ref_summary_list_reset

Free the filesystem ref summary list.

**Function:**

Free all filesystem ref summary and arrays in the list.

**Function prototype:**

```c
XWORK_API void xwork_replay_filesystem_ref_summary_list_reset(
    xwork_replay_filesystem_ref_summary_list *pList
);
```

**parameter:**

- `pList`: List to free; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the contents owned by the list.

**Additional Note:**

The list can be reused after it is released.

**Example code:**

```c
xwork_replay_filesystem_ref_summary_list_reset(&list);
```

**Related API:**

- `xwork_replay_filesystem_ref_summary_reset`

---

### xwork_replay_event_options_init

Initialize replay event options.

**Function:**

Prepare a replay event record.

**Function prototype:**

```c
XWORK_API void xwork_replay_event_options_init(xwork_replay_event_options *pOptions);
```

**parameter:**

- `pOptions`: options to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated; the record/load API copies fields that need to be retained.

**Additional Note:**

The default event kind is generic and status is `XWORK_OK`.

**Example code:**

```c
xwork_replay_event_options event;
xwork_replay_event_options_init(&event);
event.sKey = "event:1";
```

**Related API:**

- `xwork_replay_engine_record_event`

---

### xwork_replay_event_options_from_model_event

Construct replay event options from model event.

**Function:**

Map `xwork_model_event` to replay event options to facilitate recording model flow events.

**Function prototype:**

```c
XWORK_API void xwork_replay_event_options_from_model_event(
    const xwork_model_event *pEvent,
    xwork_replay_event_options *pOptions
);
```

**parameter:**

- `pEvent`: source model event; can be `NULL`.
- `pOptions`: Output options; must not be `NULL` to write.

**Return value:**

none.

**Resource ownership:**

No resources are allocated; pointers in the output borrow from the source event.

**Additional Note:**

If the caller needs to save it for a long time, it should immediately hand it over to the record/load API for copying.

**Example code:**

```c
xwork_replay_event_options opts;
xwork_replay_event_options_from_model_event(&modelEvent, &opts);
```

**Related API:**

- `xwork_replay_engine_record_event`

---

### xwork_replay_event_summary_init

Initialize replay event summary.

**Function:**

Prepare event replay/list output structure.

**Function prototype:**

```c
XWORK_API void xwork_replay_event_summary_init(xwork_replay_event_summary *pSummary);
```

**parameter:**

- `pSummary`: summary to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The default event kind is generic and status is `XWORK_OK`.

**Example code:**

```c
xwork_replay_event_summary summary;
xwork_replay_event_summary_init(&summary);
```

**Related API:**

- `xwork_replay_engine_replay_event`

---

### xwork_replay_event_summary_reset

Release replay event summary.

**Function:**

Release the event key, name, payload hash, and content hash.

**Function prototype:**

```c
XWORK_API void xwork_replay_event_summary_reset(xwork_replay_event_summary *pSummary);
```

**parameter:**

- `pSummary`: summary to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Free the internal deep-copy string.

**Additional Note:**

Applies to replay/list API populated output.

**Example code:**

```c
xwork_replay_event_summary_reset(&summary);
```

**Related API:**

- `xwork_replay_event_summary_init`

---

### xwork_replay_event_summary_list_init

Initialize the replay event summary list.

**Function:**

Prepare an empty event summary list.

**Function prototype:**

```c
XWORK_API void xwork_replay_event_summary_list_init(xwork_replay_event_summary_list *pList);
```

**parameter:**

- `pList`: List to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

It should be initialized before calling `xwork_replay_engine_list_events`.

**Example code:**

```c
xwork_replay_event_summary_list list;
xwork_replay_event_summary_list_init(&list);
```

**Related API:**

- `xwork_replay_engine_list_events`

---

### xwork_replay_event_summary_list_reset

Release the replay event summary list.

**Function:**

Free all event summaries and arrays in the list.

**Function prototype:**

```c
XWORK_API void xwork_replay_event_summary_list_reset(xwork_replay_event_summary_list *pList);
```

**parameter:**

- `pList`: List to free; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the contents owned by the list.

**Additional Note:**

The list can be reused after it is released.

**Example code:**

```c
xwork_replay_event_summary_list_reset(&list);
```

**Related API:**

- `xwork_replay_event_summary_reset`

---

### xwork_replay_divergence_init

Initialize replay divergence.

**Function:**

Prepare the divergence output structure.

**Function prototype:**

```c
XWORK_API void xwork_replay_divergence_init(xwork_replay_divergence *pDivergence);
```

**parameter:**

- `pDivergence`: divergence to initialize; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

Used to save the first difference in the replay comparison.

**Example code:**

```c
xwork_replay_divergence divergence;
xwork_replay_divergence_init(&divergence);
```

**Related API:**

- `xwork_replay_engine_get_first_divergence`

---

### xwork_replay_divergence_reset

Release replay divergence.

**Function:**

Release expected/actual key, hash and message in divergence.

**Function prototype:**

```c
XWORK_API void xwork_replay_divergence_reset(xwork_replay_divergence *pDivergence);
```

**parameter:**

- `pDivergence`: divergence to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Free the internal deep-copy string.

**Additional Note:**

`xwork_replay_result_reset` resets the internal first divergence.

**Example code:**

```c
xwork_replay_divergence_reset(&divergence);
```

**Related API:**

- `xwork_replay_result_reset`

---

### xwork_replay_result_init

Initialize replay result.

**Function:**

Prepare the replay engine statistical result output structure.

**Function prototype:**

```c
XWORK_API void xwork_replay_result_init(xwork_replay_result *pResult);
```

**parameter:**

- `pResult`: result to be initialized; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

The default status is `XWORK_OK`, and first divergence is initialized.

**Example code:**

```c
xwork_replay_result result;
xwork_replay_result_init(&result);
```

**Related API:**

- `xwork_replay_engine_get_result`

---

### xwork_replay_result_reset

Release replay result.

**Function:**

Release the first divergence inside the replay result.

**Function prototype:**

```c
XWORK_API void xwork_replay_result_reset(xwork_replay_result *pResult);
```

**parameter:**

- `pResult`: result to be released; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Release the internal deep-copy divergence field.

**Additional Note:**

After the call, result returns to the init state.

**Example code:**

```c
xwork_replay_result_reset(&result);
```

**Related API:**

- `xwork_replay_result_init`

---

## Hash API

### xwork_replay_hash_text

Compute text replay hash.

**Function:**

Generate replay content hash for normal text.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_hash_text(
    const char *sText,
    char *sBuffer,
    size_t iBufferSize
);
```

**parameter:**

- `sText`: Enter text; must not be `NULL`.
- `sBuffer`: output buffer; must not be `NULL`.
- `iBufferSize`: output buffer size in bytes.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

No resources are allocated; the hash is written to the caller buffer.

**Additional Note:**

The buffer must be large enough to hold the hash string and the trailing `\0`.

**Example code:**

```c
char hash[128];
xwork_replay_hash_text("hello", hash, sizeof(hash));
```

**Related API:**

- `xwork_replay_hash_json`

---

### xwork_replay_hash_json

Computes the normalized JSON replay hash.

**Function:**

First normalize JSON and then generate replay hash.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_hash_json(
    const char *sJson,
    char *sBuffer,
    size_t iBufferSize
);
```

**parameter:**

- `sJson`: Input JSON string; must be valid JSON.
- `sBuffer`: output buffer; must not be `NULL`.
- `iBufferSize`: output buffer size in bytes.

**Return value:**

Returns `XWORK_OK`; invalid JSON returns `XWORK_ERROR_INVALID_ARGUMENT`.

**Resource ownership:**

No resources are allocated; the hash is written to the caller buffer.

**Additional Note:**

Normalization sorts object keys and ignores meaningless whitespace; entry JSON fields take precedence over this hash.

**Example code:**

```c
char hash[128];
xwork_replay_hash_json("{\"a\":1}", hash, sizeof(hash));
```

**Related API:**

- `xwork_replay_hash_text`

---

## Engine API

### xwork_replay_engine_create

Create replay engine.

**Function:**

Create record/strict/audit replay engine.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_create(
    const xwork_replay_options *pOptions,
    xwork_replay_engine **ppEngine
);
```

**parameter:**

- `pOptions`: Create parameters; default values ​​can be used for `NULL`.
- `ppEngine`: Output owned engine.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

After success, the engine is owned by the caller and released with `xwork_replay_engine_destroy`.

**Additional Note:**

The engine can be lent to the runtime, but the runtime does not take over its life cycle.

**Example code:**

```c
xwork_replay_engine *engine = NULL;
xwork_replay_engine_create(&opts, &engine);
```

**Related API:**

- `xwork_replay_engine_destroy`

---

### xwork_replay_engine_destroy

Destroy the replay engine.

**Function:**

Release engine, entry cassette, event cassette, filesystem refs, and divergence records.

**Function prototype:**

```c
XWORK_API void xwork_replay_engine_destroy(xwork_replay_engine *pEngine);
```

**parameter:**

- `pEngine`: The engine to be destroyed; can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases resources owned by an engine; runtimes borrowing from that engine are not released.

**Additional Note:**

Make sure the engine is no longer used by the runtime before destroying it.

**Example code:**

```c
xwork_replay_engine_destroy(engine);
```

**Related API:**

- `xwork_replay_engine_create`

---

### xwork_replay_engine_get_mode

Get replay engine mode.

**Function:**

Returns the engine's current record/strict/audit mode.

**Function prototype:**

```c
XWORK_API xwork_replay_mode xwork_replay_engine_get_mode(
    const xwork_replay_engine *pEngine
);
```

**parameter:**

- `pEngine`: replay engine; can be `NULL`.

**Return value:**

Return mode; record is returned when `pEngine` is `NULL`.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

For runtime integration and diagnostics.

**Example code:**

```c
xwork_replay_mode mode = xwork_replay_engine_get_mode(engine);
```

**Related API:**

- `xwork_replay_engine_blocks_side_effects`

---

### xwork_replay_engine_blocks_side_effects

Check if replay blocks side effects.

**Function:**

Returns whether the engine is configured to block side effect host service.

**Function prototype:**

```c
XWORK_API bool xwork_replay_engine_blocks_side_effects(
    const xwork_replay_engine *pEngine
);
```

**parameter:**

- `pEngine`: replay engine; can be `NULL`.

**Return value:**

Block side effects return `true`; otherwise return `false`.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

record mode + side-effect blocking will pause before the actual side effects are executed.

**Example code:**

```c
bool blocked = xwork_replay_engine_blocks_side_effects(engine);
```

**Related API:**

- `xwork_replay_options_init`

---

### xwork_replay_engine_record_entry

Record replay entry.

**Function:**

Append an entry to the record-mode cassette.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_record_entry(
    xwork_replay_engine *pEngine,
    const xwork_replay_entry_options *pEntry
);
```

**parameter:**

- `pEngine`: target engine.
- `pEntry`: entry parameter.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

engine copies the entry field.

**Additional Note:**

Non-record mode calls are generally considered invalid.

**Example code:**

```c
xwork_replay_engine_record_entry(engine, &entry);
```

**Related API:**

- `xwork_replay_engine_replay_entry`

---

### xwork_replay_engine_load_entry

Load replay entry.

**Function:**

Preload entries from persistent cassette for strict/audit replay comparison.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_load_entry(
    xwork_replay_engine *pEngine,
    const xwork_replay_entry_options *pEntry
);
```

**parameter:**

- `pEngine`: target engine.
- `pEntry`: entry to load.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

engine copies the entry field.

**Additional Note:**

load does not increase the recorded count and is mainly used to restore cassette.

**Example code:**

```c
xwork_replay_engine_load_entry(engine, &entry);
```

**Related API:**

- `xwork_replay_engine_replay_entry`

---

### xwork_replay_engine_replay_entry

Replay and compare replay entries.

**Function:**

Compare the expected entry with the next entry in the cassette and return the actual entry summary.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_replay_entry(
    xwork_replay_engine *pEngine,
    const xwork_replay_entry_options *pExpected,
    xwork_replay_entry_summary *pActual
);
```

**parameter:**

- `pEngine`: target engine.
- `pExpected`: This entry is expected.
- `pActual`: Optionally output the actual entry summary.

**Return value:**

A match returns `XWORK_OK`; a mismatch returns a divergence-related error by pattern.

**Resource ownership:**

If `pActual` is filled, the caller releases it with `xwork_replay_entry_summary_reset`.

**Additional Note:**

Strict mode will fail when encountering divergence; audit mode will record divergence and continue auditing.

**Example code:**

```c
xwork_replay_entry_summary actual;
xwork_replay_entry_summary_init(&actual);
xwork_replay_engine_replay_entry(engine, &expected, &actual);
xwork_replay_entry_summary_reset(&actual);
```

**Related API:**

- `xwork_replay_engine_get_first_divergence`

---

### xwork_replay_engine_record_filesystem_ref

Log filesystem ref.

**Function:**

Record filesystem snapshot/ref entry with typed wrapper.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_record_filesystem_ref(
    xwork_replay_engine *pEngine,
    const xwork_replay_filesystem_ref_options *pRef
);
```

**parameter:**

- `pEngine`: target engine.
- `pRef`: filesystem ref parameter.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

engine copies the ref field.

**Additional Note:**

Suitable for recording workspace/file snapshot references rather than file content entities.

**Example code:**

```c
xwork_replay_engine_record_filesystem_ref(engine, &ref);
```

**Related API:**

- `xwork_replay_engine_replay_filesystem_ref`

---

### xwork_replay_engine_load_filesystem_ref

Load filesystem ref.

**Function:**

Load typed filesystem ref cassette from persistent data.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_load_filesystem_ref(
    xwork_replay_engine *pEngine,
    const xwork_replay_filesystem_ref_options *pRef
);
```

**parameter:**

- `pEngine`: target engine.
- `pRef`: ref to load.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

engine copies the ref field.

**Additional Note:**

Used to restore cassette before replaying.

**Example code:**

```c
xwork_replay_engine_load_filesystem_ref(engine, &ref);
```

**Related API:**

- `xwork_replay_engine_list_filesystem_refs`

---

### xwork_replay_engine_replay_filesystem_ref

Replay filesystem ref.

**Function:**

Compare expected filesystem ref with actual records in cassette.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_replay_filesystem_ref(
    xwork_replay_engine *pEngine,
    const xwork_replay_filesystem_ref_options *pExpected,
    xwork_replay_filesystem_ref_summary *pActual
);
```

**parameter:**

- `pEngine`: target engine.
- `pExpected`: Expected ref.
- `pActual`: Optionally output the actual ref summary.

**Return value:**

Returns `XWORK_OK` or divergence/error code.

**Resource ownership:**

`pActual` If filled, the caller resets.

**Additional Note:**

Only compares ref metadata and hash, and does not restore live filesystem state.

**Example code:**

```c
xwork_replay_filesystem_ref_summary actual;
xwork_replay_filesystem_ref_summary_init(&actual);
xwork_replay_engine_replay_filesystem_ref(engine, &expected, &actual);
xwork_replay_filesystem_ref_summary_reset(&actual);
```

**Related API:**

- `xwork_replay_engine_record_filesystem_ref`

---

### xwork_replay_engine_list_filesystem_refs

List filesystem refs.

**Function:**

Get the recorded or loaded filesystem refs in engine.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_list_filesystem_refs(
    const xwork_replay_engine *pEngine,
    xwork_replay_filesystem_ref_summary_list *pList
);
```

**parameter:**

- `pEngine`: source engine.
- `pList`: Output list; should be init before calling.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The list has deep-copy contents and is released with reset.

**Additional Note:**

Used to audit which filesystem snapshots are referenced in the replay cassette.

**Example code:**

```c
xwork_replay_filesystem_ref_summary_list list;
xwork_replay_filesystem_ref_summary_list_init(&list);
xwork_replay_engine_list_filesystem_refs(engine, &list);
xwork_replay_filesystem_ref_summary_list_reset(&list);
```

**Related API:**

- `xwork_replay_filesystem_ref_summary_list_reset`

---

### xwork_replay_engine_record_event

Record the replay event.

**Function:**

Append an event record to the event cassette.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_record_event(
    xwork_replay_engine *pEngine,
    const xwork_replay_event_options *pEvent
);
```

**parameter:**

- `pEngine`: target engine.
- `pEvent`: event parameters.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

engine copies the event field.

**Additional Note:**

Used for sequential auditing of model flow, run event, tool event, terminal interaction, etc.

**Example code:**

```c
xwork_replay_engine_record_event(engine, &event);
```

**Related API:**

- `xwork_replay_engine_replay_event`

---

### xwork_replay_engine_load_event

Load replay event.

**Function:**

Load event records from a persistent cassette.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_load_event(
    xwork_replay_engine *pEngine,
    const xwork_replay_event_options *pEvent
);
```

**parameter:**

- `pEngine`: target engine.
- `pEvent`: Event to load.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

engine copies the event field.

**Additional Note:**

load is used to restore event cassette before replay.

**Example code:**

```c
xwork_replay_engine_load_event(engine, &event);
```

**Related API:**

- `xwork_replay_engine_replay_event`

---

### xwork_replay_engine_replay_event

Replay and compare replay events.

**Function:**

Compare the expected event with the next event in the cassette.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_replay_event(
    xwork_replay_engine *pEngine,
    const xwork_replay_event_options *pExpected,
    xwork_replay_event_summary *pActual
);
```

**parameter:**

- `pEngine`: target engine.
- `pExpected`: expected event.
- `pActual`: Optional output of actual event summary.

**Return value:**

Returns `XWORK_OK` or divergence/error code.

**Resource ownership:**

`pActual` If filled, the caller resets.

**Additional Note:**

Event replay focuses on checking sequence, key, kind, hash and status.

**Example code:**

```c
xwork_replay_event_summary actual;
xwork_replay_event_summary_init(&actual);
xwork_replay_engine_replay_event(engine, &expected, &actual);
xwork_replay_event_summary_reset(&actual);
```

**Related API:**

- `xwork_replay_engine_record_event`

---

### xwork_replay_engine_seek_checkpoint

Locate checkpoint.

**Function:**

Advance the replay cursor to the specified checkpoint relative position.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_seek_checkpoint(
    xwork_replay_engine *pEngine,
    const char *sCheckpointId
);
```

**parameter:**

- `pEngine`: target engine.
- `sCheckpointId`: checkpoint id.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

No resources are allocated.

**Additional Note:**

Used to continue replay from a specific checkpoint after recovery.

**Example code:**

```c
xwork_replay_engine_seek_checkpoint(engine, "checkpoint-1");
```

**Related API:**

- `xwork_checkpoint`

---

### xwork_replay_engine_emit_report_artifact

Generate replay report artifact.

**Function:**

Write the replay manifest, statistical results and divergence information to the artifact.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_emit_report_artifact(
    const xwork_replay_engine *pEngine,
    xwork_run *pRun,
    const char *sArtifactId,
    xwork_artifact *pArtifact
);
```

**parameter:**

- `pEngine`: source engine.
- `pRun`: Receive artifact's run.
- `sArtifactId`: artifact id.
- `pArtifact`: Output artifact.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The artifact content is held by the output object and the caller presses the artifact API reset.

**Additional Note:**

Suitable as audit report or CI product.

**Example code:**

```c
xwork_artifact artifact;
xwork_artifact_init(&artifact);
xwork_replay_engine_emit_report_artifact(engine, run, "replay-report", &artifact);
xwork_artifact_reset(&artifact);
```

**Related API:**

- `xwork_run_emit_report_artifact`

---

### xwork_replay_engine_cancel

Cancel replay engine.

**Function:**

Mark the replay engine as canceled and log the reason for cancellation.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_cancel(
    xwork_replay_engine *pEngine,
    const char *sReason
);
```

**parameter:**

- `pEngine`: target engine.
- `sReason`: Optional cancellation reason.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

engine copies the reason string.

**Additional Note:**

Cancellation does not stop external processes or the live terminal.

**Example code:**

```c
xwork_replay_engine_cancel(engine, "user cancelled replay");
```

**Related API:**

- `xwork_replay_engine_get_result`

---

### xwork_replay_engine_get_manifest

Get replay manifest.

**Function:**

Returns replay id, manifest id, source run id, hash algorithm and entry count.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_get_manifest(
    const xwork_replay_engine *pEngine,
    xwork_replay_manifest *pManifest
);
```

**parameter:**

- `pEngine`: source engine.
- `pManifest`: Output manifest; should be init before calling.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The manifest has deep-copy content and is released with reset.

**Additional Note:**

The function resets the old contents of the output manifest.

**Example code:**

```c
xwork_replay_manifest manifest;
xwork_replay_manifest_init(&manifest);
xwork_replay_engine_get_manifest(engine, &manifest);
xwork_replay_manifest_reset(&manifest);
```

**Related API:**

- `xwork_replay_manifest_reset`

---

### xwork_replay_engine_get_result

Get replay results.

**Function:**

Returns the recorded/replayed/divergence count and the first divergence.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_get_result(
    const xwork_replay_engine *pEngine,
    xwork_replay_result *pResult
);
```

**parameter:**

- `pEngine`: source engine.
- `pResult`: Output result; should be init before calling.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The internal divergence of result has deep-copy content and is released with reset.

**Additional Note:**

Used for gate judgment and report generation after replay is completed.

**Example code:**

```c
xwork_replay_result result;
xwork_replay_result_init(&result);
xwork_replay_engine_get_result(engine, &result);
xwork_replay_result_reset(&result);
```

**Related API:**

- `xwork_replay_engine_get_first_divergence`

---

### xwork_replay_engine_get_first_divergence

Get your first divergence.

**Function:**

Returns the first difference recorded during replay.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_get_first_divergence(
    const xwork_replay_engine *pEngine,
    xwork_replay_divergence *pDivergence
);
```

**parameter:**

- `pEngine`: source engine.
- `pDivergence`: Output divergence; should be init before calling.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

divergence has deep-copy content and is released with reset.

**Additional Note:**

Without divergence the output maintains empty semantics.

**Example code:**

```c
xwork_replay_divergence divergence;
xwork_replay_divergence_init(&divergence);
xwork_replay_engine_get_first_divergence(engine, &divergence);
xwork_replay_divergence_reset(&divergence);
```

**Related API:**

- `xwork_replay_engine_get_result`

---

### xwork_replay_engine_list_entries

List replay entries.

**Function:**

Get all entry summary in engine.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_list_entries(
    const xwork_replay_engine *pEngine,
    xwork_replay_entry_summary_list *pList
);
```

**parameter:**

- `pEngine`: source engine.
- `pList`: Output list; should be init before calling.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The list has deep-copy contents and is released with reset.

**Additional Note:**

Used for debugging cassette content and persistent exports.

**Example code:**

```c
xwork_replay_entry_summary_list list;
xwork_replay_entry_summary_list_init(&list);
xwork_replay_engine_list_entries(engine, &list);
xwork_replay_entry_summary_list_reset(&list);
```

**Related API:**

- `xwork_replay_engine_record_entry`

---

### xwork_replay_engine_list_events

List replay events.

**Function:**

Get all event summaries in engine.

**Function prototype:**

```c
XWORK_API xwork_status xwork_replay_engine_list_events(
    const xwork_replay_engine *pEngine,
    xwork_replay_event_summary_list *pList
);
```

**parameter:**

- `pEngine`: source engine.
- `pList`: Output list; should be init before calling.

**Return value:**

Returns `XWORK_OK` or error code.

**Resource ownership:**

The list has deep-copy contents and is released with reset.

**Additional Note:**

Used for event sequence auditing and replay reporting.

**Example code:**

```c
xwork_replay_event_summary_list list;
xwork_replay_event_summary_list_init(&list);
xwork_replay_engine_list_events(engine, &list);
xwork_replay_event_summary_list_reset(&list);
```

**Related API:**

- `xwork_replay_engine_record_event`

---

## Related documents

- [Persistence API](api-persistence.md)
- [Replay Agent Run Example](../case/replay-agent-run.md)
- [Persistence, Checkpoints, and Replay](../guide/persistence-replay-intro.md)
- [Internal replay contract](../../dev/docs/REPLAY.md)
