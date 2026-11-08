# xcron public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xcron/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `78` 个函数、`200` 个常量或宏、
`90` 个公共类型。

## `extlibs/xcron/include/xcron/calendar.h`

[查看带契约注释的公共头](../../include/xcron/calendar.h)

### 函数 (8)

- `xcronCalendarCursorInit`
- `xcronCalendarCursorInitCivil`
- `xcronCalendarCursorStep`
- `xcronCalendarCursorStepCivil`
- `xcronCalendarMatch`
- `xcronCalendarMatchCivil`
- `xcronCalendarNext`
- `xcronCalendarNextCivil`

### 常量与宏 (16)

- `XCRON_CALENDAR_ARGUMENT`
- `XCRON_CALENDAR_CURSOR_BYTES`
- `XCRON_CALENDAR_DATE`
- `XCRON_CALENDAR_ERROR`
- `XCRON_CALENDAR_EXPRESSION`
- `XCRON_CALENDAR_FOUND`
- `XCRON_CALENDAR_MAX_MONTHS`
- `XCRON_CALENDAR_NONE`
- `XCRON_CALENDAR_OK`
- `XCRON_CALENDAR_PENDING`
- `XCRON_CALENDAR_RANGE`
- `XCRON_CALENDAR_STATE`
- `XCRON_CALENDAR_WINDOW`
- `XCRON_FEATURE_XCRON_CALENDAR`
- `XCRON_FEATURE_XCRON_EXPRESSION`
- `XCRON_SEARCH_EXCLUSIVE`

### 类型 (6)

- `xcroncalendarcursor`
- `xcroncalendarerror`
- `xcroncalendarissue`
- `xcroncalendarstatus`
- `xcronexpression`
- `xcronfield`

## `extlibs/xcron/include/xcron/delivery.h`

[查看带契约注释的公共头](../../include/xcron/delivery.h)

### 函数 (3)

- `xcronDeliveryCursorInit`
- `xcronDeliveryCursorStep`
- `xcronDeliveryResolve`

### 常量与宏 (21)

- `XCRON_DELIVERY_AMBIGUOUS`
- `XCRON_DELIVERY_ARGUMENT`
- `XCRON_DELIVERY_CAPACITY`
- `XCRON_DELIVERY_CURSOR_BYTES`
- `XCRON_DELIVERY_ERROR`
- `XCRON_DELIVERY_FOUND`
- `XCRON_DELIVERY_LIMIT`
- `XCRON_DELIVERY_MISSING`
- `XCRON_DELIVERY_NONE`
- `XCRON_DELIVERY_OK`
- `XCRON_DELIVERY_PENDING`
- `XCRON_DELIVERY_STATE`
- `XCRON_DELIVERY_ZONE`
- `XCRON_FEATURE_XCRON_DELIVERY`
- `XCRON_FEATURE_XCRON_TIMEZONE`
- `XCRON_FOLD_ALL`
- `XCRON_FOLD_FIRST`
- `XCRON_FOLD_LAST`
- `XCRON_FOLD_REJECT`
- `XCRON_MISSING_REJECT`
- `XCRON_MISSING_SKIP`

### 类型 (11)

- `xcrondeliverybudget`
- `xcrondeliverycursor`
- `xcrondeliverycursorissue`
- `xcrondeliveryerror`
- `xcrondeliveryissue`
- `xcrondeliverypolicy`
- `xcrondeliverystatus`
- `xcronfoldpolicy`
- `xcronmissingpolicy`
- `xcronzone`
- `xcronzoneerror`

## `extlibs/xcron/include/xcron/executor.h`

[查看带契约注释的公共头](../../include/xcron/executor.h)

### 函数 (9)

- `xcronExecutionCancelled`
- `xcronExecutorCancel`
- `xcronExecutorCancelJob`
- `xcronExecutorClose`
- `xcronExecutorCreate`
- `xcronExecutorDestroy`
- `xcronExecutorDrain`
- `xcronExecutorInspect`
- `xcronExecutorSubmit`

### 常量与宏 (15)

- `XCRON_EXECUTOR_ARGUMENT`
- `XCRON_EXECUTOR_BUSY`
- `XCRON_EXECUTOR_CLOSED`
- `XCRON_EXECUTOR_DUPLICATE`
- `XCRON_EXECUTOR_LIMIT`
- `XCRON_EXECUTOR_MEMORY`
- `XCRON_EXECUTOR_OK`
- `XCRON_EXECUTOR_OWNER_CONFLICT`
- `XCRON_EXECUTOR_REENTRY`
- `XCRON_EXECUTOR_REJECTED`
- `XCRON_EXECUTOR_RESULT`
- `XCRON_EXECUTOR_STALE`
- `XCRON_EXECUTOR_THREAD`
- `XCRON_FEATURE_XCRON_EXECUTOR`
- `XCRON_FEATURE_XCRON_SCHEDULER`

### 类型 (13)

- `xcroncompletion`
- `xcronexecuteproc`
- `xcronexecution`
- `xcronexecutor`
- `xcronexecutorconfig`
- `xcronexecutorerror`
- `xcronexecutorissue`
- `xcronexecutornotify`
- `xcronexecutorstats`
- `xcronjobid`
- `xcronrun`
- `xcronrunid`
- `xcronrunresult`

## `extlibs/xcron/include/xcron/export.h`

[查看带契约注释的公共头](../../include/xcron/export.h)

### 常量与宏 (2)

- `XCRON_BUILD_SHARED`
- `XCRON_USE_SHARED`

## `extlibs/xcron/include/xcron/expression.h`

[查看带契约注释的公共头](../../include/xcron/expression.h)

### 函数 (1)

- `xcronExpressionCompile`

### 常量与宏 (20)

- `XCRON_DAY_MONTH_STAR`
- `XCRON_DAY_WEEK_STAR`
- `XCRON_EXPRESSION_ARGUMENT`
- `XCRON_EXPRESSION_LIMIT`
- `XCRON_EXPRESSION_MAX_BYTES`
- `XCRON_EXPRESSION_OK`
- `XCRON_EXPRESSION_OVERFLOW`
- `XCRON_EXPRESSION_RANGE`
- `XCRON_EXPRESSION_SYNTAX`
- `XCRON_EXPRESSION_UNSUPPORTED`
- `XCRON_FIELD_COUNT`
- `XCRON_FIELD_DAY`
- `XCRON_FIELD_HOUR`
- `XCRON_FIELD_MINUTE`
- `XCRON_FIELD_MONTH`
- `XCRON_FIELD_NONE`
- `XCRON_FIELD_SECOND`
- `XCRON_FIELD_WEEKDAY`
- `XCRON_SECONDS6`
- `XCRON_UNIX5`

### 类型 (3)

- `xcrondialect`
- `xcronexpressionerror`
- `xcronexpressionissue`

## `extlibs/xcron/include/xcron/occurrence.h`

[查看带契约注释的公共头](../../include/xcron/occurrence.h)

### 函数 (3)

- `xcronOccurrenceCursorInit`
- `xcronOccurrenceCursorStep`
- `xcronOccurrenceNext`

### 常量与宏 (16)

- `XCRON_FEATURE_XCRON_OCCURRENCE`
- `XCRON_FEATURE_XCRON_ZONED`
- `XCRON_OCCURRENCE_AMBIGUOUS`
- `XCRON_OCCURRENCE_ARGUMENT`
- `XCRON_OCCURRENCE_CURSOR_BYTES`
- `XCRON_OCCURRENCE_ERROR`
- `XCRON_OCCURRENCE_EXPRESSION`
- `XCRON_OCCURRENCE_FOUND`
- `XCRON_OCCURRENCE_MISSING`
- `XCRON_OCCURRENCE_NONE`
- `XCRON_OCCURRENCE_OK`
- `XCRON_OCCURRENCE_PENDING`
- `XCRON_OCCURRENCE_RANGE`
- `XCRON_OCCURRENCE_STATE`
- `XCRON_OCCURRENCE_WINDOW`
- `XCRON_OCCURRENCE_ZONE`

### 类型 (6)

- `xcronoccurrence`
- `xcronoccurrencebudget`
- `xcronoccurrencecursor`
- `xcronoccurrenceerror`
- `xcronoccurrenceissue`
- `xcronoccurrencestatus`

## `extlibs/xcron/include/xcron/scheduler.h`

[查看带契约注释的公共头](../../include/xcron/scheduler.h)

### 函数 (12)

- `xcronSchedulerAdd`
- `xcronSchedulerCancel`
- `xcronSchedulerClose`
- `xcronSchedulerComplete`
- `xcronSchedulerCreate`
- `xcronSchedulerDestroy`
- `xcronSchedulerInspect`
- `xcronSchedulerJob`
- `xcronSchedulerPoll`
- `xcronSchedulerPollBudgeted`
- `xcronSchedulerReap`
- `xcronSchedulerWake`

### 常量与宏 (24)

- `XCRON_JOB_CANCELLED`
- `XCRON_JOB_EXHAUSTED`
- `XCRON_JOB_FAULTED`
- `XCRON_JOB_SCHEDULED`
- `XCRON_JOB_SEARCHING`
- `XCRON_MISFIRE_CATCH_UP`
- `XCRON_MISFIRE_COALESCE`
- `XCRON_MISFIRE_SKIP`
- `XCRON_OVERLAP_ALLOW`
- `XCRON_OVERLAP_SKIP`
- `XCRON_OVERLAP_WAIT`
- `XCRON_RUN_CANCELLED`
- `XCRON_RUN_COALESCED`
- `XCRON_RUN_FAILED`
- `XCRON_RUN_SUCCEEDED`
- `XCRON_SCHEDULER_ARGUMENT`
- `XCRON_SCHEDULER_BUSY`
- `XCRON_SCHEDULER_CALCULATION`
- `XCRON_SCHEDULER_CLOCK`
- `XCRON_SCHEDULER_CLOSED`
- `XCRON_SCHEDULER_LIMIT`
- `XCRON_SCHEDULER_MEMORY`
- `XCRON_SCHEDULER_OK`
- `XCRON_SCHEDULER_STALE`

### 类型 (17)

- `xcronclock`
- `xcronjobconfig`
- `xcronjobinfo`
- `xcronjobstate`
- `xcronmisfire`
- `xcronoverlap`
- `xcronpoll`
- `xcronpollbudgeted`
- `xcronretired`
- `xcronscheduler`
- `xcronschedulerconfig`
- `xcronschedulererror`
- `xcronschedulerissue`
- `xcronschedulerstats`
- `xcronwake`
- `xcronzonedbudget`
- `xcronzonedissue`

## `extlibs/xcron/include/xcron/scheduler_selected.h`

[查看带契约注释的公共头](../../include/xcron/scheduler_selected.h)

### 函数 (5)

- `xcronSchedulerAddSelected`
- `xcronSchedulerCreateSelected`
- `xcronSchedulerJobSelected`
- `xcronSchedulerPollSelected`
- `xcronSchedulerReapSelected`

### 常量与宏 (1)

- `XCRON_FEATURE_XCRON_SCHEDULER_SELECTED`

### 类型 (4)

- `xcronselectedjobinfo`
- `xcronselectedpoll`
- `xcronselectedretired`
- `xcronselectedrun`

## `extlibs/xcron/include/xcron/service.h`

[查看带契约注释的公共头](../../include/xcron/service.h)

### 函数 (14)

- `xcronServiceAddAsync`
- `xcronServiceCancelAsync`
- `xcronServiceClose`
- `xcronServiceCollect`
- `xcronServiceCreate`
- `xcronServiceDestroy`
- `xcronServiceDrain`
- `xcronServiceFailure`
- `xcronServiceInfoAsync`
- `xcronServiceInspect`
- `xcronServiceReapAsync`
- `xcronServiceReapStopped`
- `xcronServiceWaitFor`
- `xcronServiceWake`

### 常量与宏 (27)

- `XCRON_COMPLETION_CANCELLED`
- `XCRON_COMPLETION_EXPIRED`
- `XCRON_COMPLETION_INTERNAL`
- `XCRON_COMPLETION_NORMAL`
- `XCRON_COMPLETION_REJECTED`
- `XCRON_FEATURE_XCRON_SERVICE`
- `XCRON_REQUEST_ADD`
- `XCRON_REQUEST_CANCEL`
- `XCRON_REQUEST_INFO`
- `XCRON_REQUEST_REAP`
- `XCRON_SERVICE_ARGUMENT`
- `XCRON_SERVICE_BUSY`
- `XCRON_SERVICE_CLOCK`
- `XCRON_SERVICE_CLOSED`
- `XCRON_SERVICE_EXECUTOR`
- `XCRON_SERVICE_FAILED`
- `XCRON_SERVICE_KERNEL`
- `XCRON_SERVICE_LIMIT`
- `XCRON_SERVICE_MEMORY`
- `XCRON_SERVICE_OK`
- `XCRON_SERVICE_RUNNING`
- `XCRON_SERVICE_STALE`
- `XCRON_SERVICE_STARTING`
- `XCRON_SERVICE_STOPPED`
- `XCRON_SERVICE_STOPPING`
- `XCRON_SERVICE_SYSTEM`
- `XCRON_SERVICE_THREAD`

### 类型 (12)

- `xcronclockproc`
- `xcronreply`
- `xcronrequestid`
- `xcronrequestkind`
- `xcronservice`
- `xcronservicecompletion`
- `xcronservicecompletioncode`
- `xcronserviceconfig`
- `xcronserviceerror`
- `xcronserviceissue`
- `xcronservicestate`
- `xcronservicestats`

## `extlibs/xcron/include/xcron/timezone.h`

[查看带契约注释的公共头](../../include/xcron/timezone.h)

### 函数 (8)

- `xcronZoneAt`
- `xcronZoneCreatePosix`
- `xcronZoneCreateTzif`
- `xcronZoneNextTransition`
- `xcronZoneRelease`
- `xcronZoneResolve`
- `xcronZoneRetain`
- `xcronZoneSplit`

### 常量与宏 (21)

- `XCRON_ZONE_ARGUMENT`
- `XCRON_ZONE_CAPACITY`
- `XCRON_ZONE_DATE`
- `XCRON_ZONE_ERROR`
- `XCRON_ZONE_FOUND`
- `XCRON_ZONE_LIMIT`
- `XCRON_ZONE_MEMORY`
- `XCRON_ZONE_NAME_MAX`
- `XCRON_ZONE_NONE`
- `XCRON_ZONE_OFFSET_UNKNOWN`
- `XCRON_ZONE_OK`
- `XCRON_ZONE_POSIX`
- `XCRON_ZONE_RULES_REQUIRED`
- `XCRON_ZONE_SCALE`
- `XCRON_ZONE_SYNTAX`
- `XCRON_ZONE_TEXT_MAX`
- `XCRON_ZONE_TRANSITIONS_MAX`
- `XCRON_ZONE_TZIF`
- `XCRON_ZONE_TZIF3`
- `XCRON_ZONE_TZIF_MAX`
- `XCRON_ZONE_UNKNOWN`

### 类型 (5)

- `xcronzonegrammar`
- `xcronzoneissue`
- `xcronzonestate`
- `xcronzonestatus`
- `xcronzonetransition`

## `extlibs/xcron/include/xcron/tzdb.h`

[查看带契约注释的公共头](../../include/xcron/tzdb.h)

### 函数 (10)

- `xcronTzdbAcquire`
- `xcronTzdbClose`
- `xcronTzdbCreate`
- `xcronTzdbDestroy`
- `xcronTzdbFind`
- `xcronTzdbInspect`
- `xcronTzdbLoad`
- `xcronTzdbSnapshotInfo`
- `xcronTzdbSnapshotRelease`
- `xcronTzdbSnapshotRetain`

### 常量与宏 (24)

- `XCRON_FEATURE_XCRON_TZDB`
- `XCRON_TZDB_ANY_REVISION`
- `XCRON_TZDB_ARGUMENT`
- `XCRON_TZDB_BUSY`
- `XCRON_TZDB_CLOSED`
- `XCRON_TZDB_DUPLICATE`
- `XCRON_TZDB_EMPTY`
- `XCRON_TZDB_ERROR`
- `XCRON_TZDB_FOUND`
- `XCRON_TZDB_IO`
- `XCRON_TZDB_LIMIT`
- `XCRON_TZDB_LOADS_MAX`
- `XCRON_TZDB_MEMORY`
- `XCRON_TZDB_NAME`
- `XCRON_TZDB_NAMES_MAX`
- `XCRON_TZDB_NAME_MAX`
- `XCRON_TZDB_NONE`
- `XCRON_TZDB_NO_INDEX`
- `XCRON_TZDB_OK`
- `XCRON_TZDB_STALE`
- `XCRON_TZDB_SYSTEM`
- `XCRON_TZDB_VERSION`
- `XCRON_TZDB_VERSION_MAX`
- `XCRON_TZDB_ZONE`

### 类型 (9)

- `xcrontzdb`
- `xcrontzdbconfig`
- `xcrontzdbentry`
- `xcrontzdberror`
- `xcrontzdbissue`
- `xcrontzdbsnapshot`
- `xcrontzdbsnapshotinfo`
- `xcrontzdbstats`
- `xcrontzdbstatus`

## `extlibs/xcron/include/xcron/tzdb_file.h`

[查看带契约注释的公共头](../../include/xcron/tzdb_file.h)

### 函数 (1)

- `xcronTzdbLoadRoot`

### 常量与宏 (1)

- `XCRON_FEATURE_XCRON_TZDB_FILE`

## `extlibs/xcron/include/xcron/zoned.h`

[查看带契约注释的公共头](../../include/xcron/zoned.h)

### 函数 (4)

- `xcronZonedCursorInit`
- `xcronZonedCursorStep`
- `xcronZonedMatch`
- `xcronZonedNext`

### 常量与宏 (12)

- `XCRON_ZONED_ARGUMENT`
- `XCRON_ZONED_CURSOR_BYTES`
- `XCRON_ZONED_ERROR`
- `XCRON_ZONED_EXPRESSION`
- `XCRON_ZONED_FOUND`
- `XCRON_ZONED_NONE`
- `XCRON_ZONED_OK`
- `XCRON_ZONED_PENDING`
- `XCRON_ZONED_RANGE`
- `XCRON_ZONED_STATE`
- `XCRON_ZONED_WINDOW`
- `XCRON_ZONED_ZONE`

### 类型 (4)

- `xcronzonedcursor`
- `xcronzonedcursorissue`
- `xcronzonederror`
- `xcronzonedstatus`
