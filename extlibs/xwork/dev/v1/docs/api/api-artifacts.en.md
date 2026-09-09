# Artifact API

> Status: Chinese function-by-function reference, waiting for manual review.

The Artifact API is responsible for saving the file contents, patches, command output, terminal status, diagnosis and reports of Agent running into queryable, persistent and auditable products.

## Module positioning

Artifact is xwork's durable output model. It is not a UI display object, nor a distributed blob store; it provides stable metadata, summary, storage ref and optional content text, allowing the product layer to display or synchronize itself.

## This page covers the statement

| Category | Statement |
| --- | --- |
| Structure | `xwork_artifact_options`, `xwork_patch_artifact_options`, `xwork_report_artifact_options`,
| Function | `xwork_artifact_options_init`, `xwork_patch_artifact_options_init`, `xwork_report_artifact_options_init`, `xwork_artifact_summary_reset`, `xwork_artifact_summary_list_init`, `xwork_artifact_summary_list_reset`, `xwork_artifact_summary_query_init` |

## Artifact Kind

| Type | Description |
| --- | --- |
| `XWORK_ARTIFACT_PATCH` | patch text, apply result and file summary. |
| `XWORK_ARTIFACT_REPORT` | Structured reports such as plan, review, diagnostics, final. |
| `XWORK_ARTIFACT_COMMAND` | Command text, output, exit code and stdout/stderr statistics. |
| `XWORK_ARTIFACT_OUTPUT` | General output such as file content, JSON, terminal status, terminal inventory, etc. |

## Schema constants

- `XWORK_REPORT_SCHEMA_V1`
- `XWORK_DIAGNOSTICS_SCHEMA_V1`
- `XWORK_PATCH_APPLY_RESULT_SCHEMA_V1`
- `XWORK_PATCH_FILE_SUMMARY_SCHEMA_V1`
- `XWORK_TERMINAL_STATE_SCHEMA_V1`
- `XWORK_TERMINAL_INVENTORY_SCHEMA_V1`

## Ownership Rules

- The strings and content pointers in options are borrowed; copied to run/artifact storage when emitted.
- `xwork_artifact` and `xwork_artifact_summary` have deep-copy fields after being populated by query/emit output and must be reset after use.
- `xwork_artifact_summary_list` has the `pItems` array and internal summary field, which must be reset after use.
- The external blob or file pointed to by `sStorageRef` is the responsibility of the host system; xwork does not turn it into an owned blob.

---

### xwork_artifact_options_init

Initialize common artifact options.

**Function:**

Used to prepare generic artifact metadata and content fields before calling `xwork_run_emit_artifact` directly.

**Function prototype:**

```c
XWORK_API void xwork_artifact_options_init(xwork_artifact_options *pOptions);
```

**parameter:**

- `pOptions`: Output parameter. Can be `NULL`; clear and set default kind when not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. The options fields are all provided by the caller.

**Additional Note:**

- Default `eKind` is `XWORK_ARTIFACT_OUTPUT`.
- When using this general options directly, the caller should explicitly fill in fields such as kind, name, mime/content/storage, etc.

**Example code:**

```c
xwork_artifact_options options;
xwork_artifact_options_init(&options);
options.sName = "output.txt";
options.sContentText = "hello";
```

**Related API:**

- `xwork_run_emit_artifact`
- `xwork_artifact_init`

---

### xwork_patch_artifact_options_init

Initialize patch artifact options.

**Function:**

Used to prepare patch text, target ref, apply result and file summary JSON before issuing patch artifact.

**Function prototype:**

```c
XWORK_API void xwork_patch_artifact_options_init(xwork_patch_artifact_options *pOptions);
```

**parameter:**

- `pOptions`: Output parameter. Can be `NULL`; cleared if not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. All string fields are borrowed by the caller.

**Additional Note:**

- `xwork_run_emit_patch_artifact` will set kind to `XWORK_ARTIFACT_PATCH` and mime type to diff type.
- `sApplyResultJson` It is recommended to use `XWORK_PATCH_APPLY_RESULT_SCHEMA_V1`.
- `sFileSummaryJson` It is recommended to use `XWORK_PATCH_FILE_SUMMARY_SCHEMA_V1`.

**Example code:**

```c
xwork_patch_artifact_options options;
xwork_patch_artifact_options_init(&options);
options.sPatchText = "--- a/file\n+++ b/file\n";
```

**Related API:**

- `xwork_run_emit_patch_artifact`

---

### xwork_report_artifact_options_init

Initialize report artifact options.

**Function:**

Used to prepare report fields before issuing reports, diagnostics, plans, reviews or final output.

**Function prototype:**

```c
XWORK_API void xwork_report_artifact_options_init(xwork_report_artifact_options *pOptions);
```

**parameter:**

- `pOptions`: Output parameter. Can be `NULL`; clear if not `NULL` and set default MIME.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. All string fields are borrowed by the caller.

**Additional Note:**

- Default `sMimeType` is `text/markdown`.
- `xwork_run_emit_report_artifact` will set kind to `XWORK_ARTIFACT_REPORT`.

**Example code:**

```c
xwork_report_artifact_options options;
xwork_report_artifact_options_init(&options);
options.eReportClass = XWORK_ARTIFACT_REPORT_FINAL;
options.sReportText = "# Result\n";
```

**Related API:**

- `xwork_run_emit_report_artifact`
- `XWORK_REPORT_SCHEMA_V1`

---

### xwork_output_artifact_options_init

Initialize output artifact options.

**Function:**

Used to emit plain text, JSON, file contents, terminal status, or other general output.

**Function prototype:**

```c
XWORK_API void xwork_output_artifact_options_init(xwork_output_artifact_options *pOptions);
```

**parameter:**

- `pOptions`: Output parameter. Can be `NULL`; clear if not `NULL` and set default MIME.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. All string fields are borrowed by the caller.

**Additional Note:**

- Default `sMimeType` is `text/plain`.
- JSON output recommends setting `sMimeType = "application/json"` and `eOutputClass = XWORK_ARTIFACT_OUTPUT_JSON` explicitly.

**Example code:**

```c
xwork_output_artifact_options options;
xwork_output_artifact_options_init(&options);
options.sOutputText = "done";
```

**Related API:**

- `xwork_run_emit_output_artifact`

---

### xwork_command_artifact_options_init

Initialize command artifact options.

**Function:**

Used to log command text, output, exit code, and stdout/stderr statistics.

**Function prototype:**

```c
XWORK_API void xwork_command_artifact_options_init(xwork_command_artifact_options *pOptions);
```

**parameter:**

- `pOptions`: Output parameter. Can be `NULL`; clear if not `NULL` and set default MIME.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. All string fields are borrowed by the caller.

**Additional Note:**

- Default `sMimeType` is `text/plain`.
- If exit code is set, `bHasExitCode = true` should also be set.

**Example code:**

```c
xwork_command_artifact_options options;
xwork_command_artifact_options_init(&options);
options.sCommandText = "git status";
options.bHasExitCode = true;
options.iExitCode = 0;
```

**Related API:**

- `xwork_run_emit_command_artifact`

---

### xwork_artifact_init

Initialize the artifact.

**Function:**

Prepare to receive artifacts returned by run emit, run get, or persistence load.

**Function prototype:**

```c
XWORK_API void xwork_artifact_init(xwork_artifact *pArtifact);
```

**parameter:**

- `pArtifact`: Output parameter. Can be `NULL`; clear and set default kind when not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. The populated artifact has a deep-copy field and must be reset.

**Additional Note:**

- Default `eKind` is `XWORK_ARTIFACT_OUTPUT`.

**Example code:**

```c
xwork_artifact artifact;
xwork_artifact_init(&artifact);
xwork_artifact_reset(&artifact);
```

**Related API:**

- `xwork_artifact_reset`
- `xwork_run_get_artifact`

---

### xwork_artifact_reset

Release and reset the artifact.

**Function:**

Release the id, run id, name, mime, storage ref, summary, content, and typed metadata fields in the artifact.

**Function prototype:**

```c
XWORK_API void xwork_artifact_reset(xwork_artifact *pArtifact);
```

**parameter:**

- `pArtifact`: input/output parameters. Can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the copy of the string owned by the artifact.

**Additional Note:**

- After reset, the artifact returns to the init state.

**Example code:**

```c
xwork_artifact_reset(&artifact);
```

**Related API:**

- `xwork_artifact_init`

---

### xwork_artifact_summary_init

Initialize artifact summary.

**Function:**

Prepare to receive artifact summary query results.

**Function prototype:**

```c
XWORK_API void xwork_artifact_summary_init(xwork_artifact_summary *pSummary);
```

**parameter:**

- `pSummary`: Output parameter. Can be `NULL`; clear and set default kind when not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. The populated summary has deep-copy fields and must be reset.

**Additional Note:**

- summary does not contain the complete content text, but only retains queryable metadata and statistics.

**Example code:**

```c
xwork_artifact_summary summary;
xwork_artifact_summary_init(&summary);
xwork_artifact_summary_reset(&summary);
```

**Related API:**

- `xwork_artifact_summary_reset`
- `xwork_runtime_query_persisted_artifact_summaries`

---

### xwork_artifact_summary_reset

Release and reset the artifact summary.

**Function:**

Release the id, name, mime, storage ref, summary, role, report subject and patch JSON fields in summary.

**Function prototype:**

```c
XWORK_API void xwork_artifact_summary_reset(xwork_artifact_summary *pSummary);
```

**parameter:**

- `pSummary`: input/output parameters. Can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the copy of the string owned by summary.

**Additional Note:**

- Summary returns to init state after reset.

**Example code:**

```c
xwork_artifact_summary_reset(&summary);
```

**Related API:**

- `xwork_artifact_summary_init`

---

### xwork_artifact_summary_list_init

Initialize the artifact summary list.

**Function:**

Prepare to receive artifact summary query list.

**Function prototype:**

```c
XWORK_API void xwork_artifact_summary_list_init(xwork_artifact_summary_list *pList);
```

**parameter:**

- `pList`: Output parameter. Can be `NULL`; cleared if not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. After the query is populated the list has the `pItems` array.

**Additional Note:**

- Call `xwork_artifact_summary_list_reset` after use.

**Example code:**

```c
xwork_artifact_summary_list list;
xwork_artifact_summary_list_init(&list);
xwork_artifact_summary_list_reset(&list);
```

**Related API:**

- `xwork_artifact_summary_list_reset`

---

### xwork_artifact_summary_list_reset

Free and reset the artifact summary list.

**Function:**

Free the list array and the deep-copy fields owned by each summary.

**Function prototype:**

```c
XWORK_API void xwork_artifact_summary_list_reset(xwork_artifact_summary_list *pList);
```

**parameter:**

- `pList`: input/output parameters. Can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases `pItems` and its element contents.

**Additional Note:**

- List variables can be reused after reset.

**Example code:**

```c
xwork_artifact_summary_list_reset(&list);
```

**Related API:**

- `xwork_artifact_summary_list_init`

---

### xwork_artifact_summary_query_init

Initialize artifact summary query conditions.

**Function:**

Used to filter artifact summary by kind, output class, role, report class, name, MIME, storage ref, exit code, and sequence scope.

**Function prototype:**

```c
XWORK_API void xwork_artifact_summary_query_init(xwork_artifact_summary_query *pQuery);
```

**parameter:**

- `pQuery`: Output parameter. Can be `NULL`; cleared if not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. Query string fields are borrowed and provided by the caller.

**Additional Note:**

- An empty query means no filtering.
- `iLimit` can be used for paging; return to list can set `bHasMore` and `iNextAfterSequence`.

**Example code:**

```c
xwork_artifact_summary_query query;
xwork_artifact_summary_query_init(&query);
query.bHasOutputClass = true;
query.eOutputClass = XWORK_ARTIFACT_OUTPUT_TERMINAL_STATE;
query.iLimit = 50u;
```

**Related API:**

- `xwork_runtime_query_persisted_artifact_summaries`

## Emit Artifact

The emitting function is defined in [Run API](api-run.md) and includes:

- `xwork_run_emit_artifact`
- `xwork_run_emit_patch_artifact`
- `xwork_run_emit_report_artifact`
- `xwork_run_emit_output_artifact`
- `xwork_run_emit_command_artifact`

## Restore boundaries

Artifact metadata and content text can be restored persistently. The external blob or file pointed to by `sStorageRef` is the responsibility of the host system; xwork's built-in file backend does not manage the distributed blob store.

## Thread boundaries

The artifact init/reset/query structure does not access global state. The run emit/get/query operation reads and writes the run or persistence backend, and mutations in the same run should be serialized by the caller.

## Related documents

- [Run API](api-run.md)
- [Persistence API](api-persistence.md)
- [Tools, Approval, and Artifacts](../guide/tool-approval-artifact-intro.md)
