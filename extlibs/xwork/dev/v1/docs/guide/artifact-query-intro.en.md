# Artifact Query

>Status: First draft in Chinese, awaiting review.

Artifact is xwork's core artifact model for product UI, auditing, and recovery. This tutorial shows how to use summary query to paginate, filter, and display artifacts.

## Applicable scenarios

- Display command output list.
- Display patch and file changes.
- Show terminal state/inventory.
- Show diagnostics and final report.
- Only the summary is loaded after recovery, and the complete artifact is loaded when the user expands it.

## Query terminal status artifact

```c
xwork_artifact_summary_query tQuery;
xwork_artifact_summary_list tList;

xwork_artifact_summary_query_init(&tQuery);
xwork_artifact_summary_list_init(&tList);

tQuery.bHasOutputClass = true;
tQuery.eOutputClass = XWORK_ARTIFACT_OUTPUT_TERMINAL_STATE;
tQuery.iLimit = 50;

status = xwork_runtime_query_persisted_artifact_summaries(
    pRuntime,
    "run-1",
    &tQuery,
    &tList
);

xwork_artifact_summary_list_reset(&tList);
```

## Pagination

Use `after_sequence + limit`:

```c
tQuery.bHasAfterSequence = true;
tQuery.iAfterSequence = lastSequence;
tQuery.iLimit = 100;
```

`bHasMore` and `iNextAfterSequence` in the returned list can be used as next page cursors.

## Commonly used filters

| Filter | Field |
| --- | --- |
| artifact kind | `bHasKind` / `eKind` |
| output class | `bHasOutputClass` / `eOutputClass` |
| report class | `bHasReportClass` / `eReportClass` |
| name exact match | `sArtifactName` |
| name prefix | `sNamePrefix` |
| MIME prefix | `sMimeTypePrefix` |
| storage ref prefix | `sStorageRefPrefix` |
| exit code |

## UI display suggestions

- Use summary for list pages, do not load the full content by default.
- command artifact displays exit code, stdout/stderr byte count, and truncation flag.
- patch artifact shows file/hunk/add/delete statistics.
- Report artifacts are displayed by report class partition.
- terminal artifact concatenates the same session by storage ref or output role.

## Next step

- [Artifact API](../api/api-artifacts.md)
- [Persistence API](../api/api-persistence.md)
- [AI IDE Agent Example](../case/ai-ide-agent.md)
