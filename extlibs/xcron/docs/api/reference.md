# xcron public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xcron/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `13` 个函数、`57` 个常量或宏、
`15` 个公共类型。

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

### 类型 (7)

- `xcronzone`
- `xcronzoneerror`
- `xcronzonegrammar`
- `xcronzoneissue`
- `xcronzonestate`
- `xcronzonestatus`
- `xcronzonetransition`
