# xcron public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xcron/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `35` 个函数、`105` 个常量或宏、
`45` 个公共类型。

## `extlibs/xcron/include/xcron/calendar.h`

[查看带契约注释的公共头](../../include/xcron/calendar.h)

### 函数 (4)

- `xcronCalendarMatch`
- `xcronCalendarMatchCivil`
- `xcronCalendarNext`
- `xcronCalendarNextCivil`

### 常量与宏 (13)

- `XCRON_CALENDAR_ARGUMENT`
- `XCRON_CALENDAR_DATE`
- `XCRON_CALENDAR_ERROR`
- `XCRON_CALENDAR_EXPRESSION`
- `XCRON_CALENDAR_FOUND`
- `XCRON_CALENDAR_MAX_MONTHS`
- `XCRON_CALENDAR_NONE`
- `XCRON_CALENDAR_OK`
- `XCRON_CALENDAR_RANGE`
- `XCRON_CALENDAR_WINDOW`
- `XCRON_FEATURE_XCRON_CALENDAR`
- `XCRON_FEATURE_XCRON_EXPRESSION`
- `XCRON_SEARCH_EXCLUSIVE`

### 类型 (5)

- `xcroncalendarerror`
- `xcroncalendarissue`
- `xcroncalendarstatus`
- `xcronexpression`
- `xcronfield`

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

## `extlibs/xcron/include/xcron/scheduler.h`

[查看带契约注释的公共头](../../include/xcron/scheduler.h)

### 函数 (11)

- `xcronSchedulerAdd`
- `xcronSchedulerCancel`
- `xcronSchedulerClose`
- `xcronSchedulerComplete`
- `xcronSchedulerCreate`
- `xcronSchedulerDestroy`
- `xcronSchedulerInspect`
- `xcronSchedulerJob`
- `xcronSchedulerPoll`
- `xcronSchedulerReap`
- `xcronSchedulerWake`

### 常量与宏 (24)

- `XCRON_FEATURE_XCRON_ZONED`
- `XCRON_JOB_CANCELLED`
- `XCRON_JOB_EXHAUSTED`
- `XCRON_JOB_FAULTED`
- `XCRON_JOB_SCHEDULED`
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

### 类型 (16)

- `xcronclock`
- `xcronjobconfig`
- `xcronjobinfo`
- `xcronjobstate`
- `xcronmisfire`
- `xcronoverlap`
- `xcronpoll`
- `xcronretired`
- `xcronscheduler`
- `xcronschedulerconfig`
- `xcronschedulererror`
- `xcronschedulerissue`
- `xcronschedulerstats`
- `xcronwake`
- `xcronzone`
- `xcronzonedissue`

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

### 常量与宏 (22)

- `XCRON_FEATURE_XCRON_TIMEZONE`
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

### 类型 (6)

- `xcronzoneerror`
- `xcronzonegrammar`
- `xcronzoneissue`
- `xcronzonestate`
- `xcronzonestatus`
- `xcronzonetransition`

## `extlibs/xcron/include/xcron/zoned.h`

[查看带契约注释的公共头](../../include/xcron/zoned.h)

### 函数 (2)

- `xcronZonedMatch`
- `xcronZonedNext`

### 常量与宏 (9)

- `XCRON_ZONED_ARGUMENT`
- `XCRON_ZONED_ERROR`
- `XCRON_ZONED_EXPRESSION`
- `XCRON_ZONED_FOUND`
- `XCRON_ZONED_NONE`
- `XCRON_ZONED_OK`
- `XCRON_ZONED_RANGE`
- `XCRON_ZONED_WINDOW`
- `XCRON_ZONED_ZONE`

### 类型 (2)

- `xcronzonederror`
- `xcronzonedstatus`
