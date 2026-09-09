# Workspace API

> 状态：中文逐函数参考，待人工审阅。

`xwork_workspace` 表示 Agent 可以操作的工作区。它通常对应一个项目根目录，并可选接入 `xllm_memory` 做 workspace memory sync。

## 模块定位

Workspace 负责把项目根目录、workspace id、memory 和路径策略接入 xwork。它不直接执行文件系统读写；真实读写由 host tool、host service 和本地/远程 worker 执行。

## 本页覆盖声明

| 类别 | 声明 |
| --- | --- |
| 不透明对象 | `xwork_workspace` |
| 结构体 | `xwork_workspace_options`, `xwork_workspace_memory_sync_summary`, `xwork_workspace_memory_file_sync_summary` |
| 函数 | `xwork_workspace_options_init`, `xwork_workspace_memory_sync_summary_init`, `xwork_workspace_memory_file_sync_summary_init`, `xwork_runtime_add_workspace`, `xwork_runtime_find_workspace`, `xwork_workspace_destroy`, `xwork_workspace_get_id`, `xwork_workspace_get_root_path`, `xwork_workspace_is_memory_enabled`, `xwork_workspace_get_memory`, `xwork_workspace_sync_memory`, `xwork_workspace_sync_memory_file` |

## 结构体字段

### xwork_workspace_options

| 字段 | 说明 |
| --- | --- |
| `sWorkspaceId` | 工作区 id。必填，非空字符串。`xwork_runtime_add_workspace` 会复制该字符串。 |
| `sRootPath` | 工作区根目录。必填，非空字符串。`xwork_runtime_add_workspace` 会复制该字符串。 |
| `bEnableMemory` | 是否启用 workspace memory。默认 `false`。 |
| `pMemory` | 借用的 `xllm_memory *`。启用 memory sync 时必须非 `NULL`，并且生命周期必须长于 workspace。 |
| `sMemorySyncAllowedExtensions` | 可选 include 列表，例如 `.c,.h,.md`。字符串会被复制。 |
| `sMemorySyncIgnoredDirectories` | 可选目录名排除列表，例如 `.git,build`。字符串会被复制。 |
| `sMemorySyncIgnoredExtensions` | 可选扩展名排除列表。字符串会被复制。 |
| `sMemorySyncIgnoredPathPatterns` | 可选相对路径片段/模式排除列表。字符串会被复制。 |
| `sMemorySyncIgnoredFiles` | 可选 ignore 文件名列表，转交给 xllm workspace sync。字符串会被复制。 |
| `iMemorySyncMaxFileBytes` | 单文件最大 ingest 字节数。`0` 表示不设置 xwork 侧上限。 |

memory sync policy 字符串使用 xllm 的分隔约定：逗号、分号、竖线或空白字符都可作为分隔符。

### xwork_workspace_memory_sync_summary

| 字段 | 说明 |
| --- | --- |
| `iVisitedFileCount` | 扫描访问的文件数。 |
| `iIngestedFileCount` | 实际 ingest 的文件数。 |
| `iCreatedRecordCount` | 新建 memory record 数。 |
| `iUpdatedRecordCount` | 更新 memory record 数。 |
| `iSkippedFileCount` | 被策略或 unchanged 检查跳过的文件数。 |
| `iFailedFileCount` | ingest 失败的文件数。 |
| `iExaminedRecordCount` | cleanup 阶段检查的历史 record 数。 |
| `iRemovedRecordCount` | cleanup 阶段移除的历史 record 数。 |

### xwork_workspace_memory_file_sync_summary

| 字段 | 说明 |
| --- | --- |
| `iChangeCount` | xllm 返回的 change 总数。 |
| `iCreatedCount` | 创建的 memory record 数。 |
| `iUpdatedCount` | 更新的 memory record 数。 |
| `iRemovedCount` | 移除的 memory record 数。 |
| `iSkippedCount` | 跳过的文件变更数。 |
| `iFailedCount` | 失败的文件变更数。 |

## 所有权规则

- workspace 对象挂载到 runtime 链表中，通常由 `xwork_runtime_destroy` 统一释放。
- `xwork_runtime_add_workspace` 成功后，`*ppWorkspace` 是借用指针；不要在 runtime 释放后继续使用。
- `xwork_workspace_destroy` 可以提前销毁 workspace，并会从所属 runtime 中摘除它。
- `sWorkspaceId`、`sRootPath` 和 memory sync policy 字符串会被复制。
- `pMemory` 是借用指针，xwork 不销毁它。
- getter 返回的字符串和 `xllm_memory *` 都是借用指针。

## 常见调用顺序

```text
xwork_runtime_options_init
xwork_runtime_create
xwork_workspace_options_init
xwork_runtime_add_workspace
xwork_runtime_find_workspace / xwork_workspace_get_*
xwork_workspace_sync_memory / xwork_workspace_sync_memory_file
xwork_runtime_destroy
```

---

### xwork_workspace_options_init

初始化 `xwork_workspace_options`。

**功能：**

你可以在创建 workspace 前调用该函数，把所有字段置为稳定默认值，再按需填写 workspace id、根目录和 memory sync 策略。

**函数原型：**

```c
XWORK_API void xwork_workspace_options_init(xwork_workspace_options *pOptions);
```

**参数：**

- `pOptions`：输出参数。可为 `NULL`；如果为 `NULL`，函数不执行任何操作。非 `NULL` 时会被整体清零。

**返回值：**

无。

**资源归属：**

- 函数不分配堆内存。
- 调用者拥有 `pOptions` 结构体存储。
- 清零后所有字符串字段和 `pMemory` 均为 `NULL`。

**补充说明：**

- `sWorkspaceId` 和 `sRootPath` 在调用 `xwork_runtime_add_workspace` 前必须填写为非空字符串。
- 若 `bEnableMemory` 为 `true`，通常还需要设置 `pMemory`。

**范例代码：**

```c
#include "xwork.h"

int main(void) {
    xwork_workspace_options options;

    xwork_workspace_options_init(&options);
    options.sWorkspaceId = "main";
    options.sRootPath = ".";
    return 0;
}
```

**相关 API：**

- `xwork_runtime_add_workspace`
- `xwork_workspace_destroy`

---

### xwork_workspace_memory_sync_summary_init

初始化 workspace 级 memory sync 统计结构。

**功能：**

你可以在手动复用 `xwork_workspace_memory_sync_summary` 前调用该函数，清空上一次同步的统计值。

**函数原型：**

```c
XWORK_API void xwork_workspace_memory_sync_summary_init(
    xwork_workspace_memory_sync_summary *pSummary
);
```

**参数：**

- `pSummary`：输出参数。可为 `NULL`；如果为 `NULL`，函数不执行任何操作。非 `NULL` 时会被整体清零。

**返回值：**

无。

**资源归属：**

该结构只包含计数字段，不拥有堆内存，不需要 reset/destroy。

**补充说明：**

- `xwork_workspace_sync_memory` 在 `pSummary` 非 `NULL` 时会自动先清零该结构。
- 该函数主要用于调用方自己维护 summary 生命周期的场景。

**范例代码：**

```c
#include "xwork.h"

int main(void) {
    xwork_workspace_memory_sync_summary summary;
    xwork_workspace_memory_sync_summary_init(&summary);
    return summary.iFailedFileCount == 0 ? 0 : 1;
}
```

**相关 API：**

- `xwork_workspace_sync_memory`
- `xwork_workspace_memory_file_sync_summary_init`

---

### xwork_workspace_memory_file_sync_summary_init

初始化单文件 memory sync 统计结构。

**功能：**

你可以在手动复用 `xwork_workspace_memory_file_sync_summary` 前调用该函数，清空上一次文件同步的统计值。

**函数原型：**

```c
XWORK_API void xwork_workspace_memory_file_sync_summary_init(
    xwork_workspace_memory_file_sync_summary *pSummary
);
```

**参数：**

- `pSummary`：输出参数。可为 `NULL`；如果为 `NULL`，函数不执行任何操作。非 `NULL` 时会被整体清零。

**返回值：**

无。

**资源归属：**

该结构只包含计数字段，不拥有堆内存，不需要 reset/destroy。

**补充说明：**

- `xwork_workspace_sync_memory_file` 在 `pSummary` 非 `NULL` 时会自动先清零该结构。
- `iChangeCount` 是 xllm 返回的 change 数，其他字段是按 change kind 聚合后的计数。

**范例代码：**

```c
#include "xwork.h"

int main(void) {
    xwork_workspace_memory_file_sync_summary summary;
    xwork_workspace_memory_file_sync_summary_init(&summary);
    return summary.iChangeCount == 0 ? 0 : 1;
}
```

**相关 API：**

- `xwork_workspace_sync_memory_file`
- `xwork_workspace_memory_sync_summary_init`

---

### xwork_runtime_add_workspace

向 runtime 注册 workspace。

**功能：**

你可以用该函数把项目根目录加入 xwork runtime，使后续 run、tool、orchestrator、memory sync 和恢复逻辑能够通过 workspace id 引用该工作区。

**函数原型：**

```c
XWORK_API xwork_status xwork_runtime_add_workspace(
    xwork_runtime *pRuntime,
    const xwork_workspace_options *pOptions,
    xwork_workspace **ppWorkspace
);
```

**参数：**

- `pRuntime`：输入/输出参数。必须非 `NULL`。workspace 会挂载到该 runtime。
- `pOptions`：输入参数。必须非 `NULL`，并且 `sWorkspaceId`、`sRootPath` 必须为非空字符串。
- `ppWorkspace`：输出参数。必须非 `NULL`。成功时接收 workspace 借用指针；失败时置为 `NULL`。

**返回值：**

- `XWORK_OK`：workspace 注册成功。
- `XWORK_ERROR_INVALID_ARGUMENT`：runtime、options、输出指针、workspace id 或 root path 无效。
- `XWORK_ERROR_ALREADY_EXISTS`：同一 runtime 中已存在相同 workspace id。
- `XWORK_ERROR_NO_MEMORY`：对象或字符串复制分配失败。

**资源归属：**

- 成功后 workspace 由 runtime 拥有。
- `*ppWorkspace` 是借用指针。
- 函数复制 `sWorkspaceId`、`sRootPath` 和 memory sync policy 字符串。
- 函数借用 `pMemory`，不会销毁它。

**补充说明：**

- workspace id 在同一 runtime 内必须唯一。
- 显式调用 `xwork_workspace_destroy` 会把 workspace 从 runtime 中移除；否则 `xwork_runtime_destroy` 会统一释放。
- 若启用 memory sync，调用方必须保证 `pMemory` 至少活到 workspace 被销毁之后。

**范例代码：**

```c
#include "xwork.h"

int main(void) {
    xwork_runtime *runtime = NULL;
    xwork_runtime_options runtime_options;
    xwork_workspace_options workspace_options;
    xwork_workspace *workspace = NULL;

    xwork_runtime_options_init(&runtime_options);
    if (xwork_runtime_create(&runtime_options, &runtime) != XWORK_OK) {
        return 1;
    }

    xwork_workspace_options_init(&workspace_options);
    workspace_options.sWorkspaceId = "main";
    workspace_options.sRootPath = ".";

    if (xwork_runtime_add_workspace(runtime, &workspace_options, &workspace) != XWORK_OK) {
        xwork_runtime_destroy(runtime);
        return 1;
    }

    xwork_runtime_destroy(runtime);
    return 0;
}
```

**相关 API：**

- `xwork_workspace_options_init`
- `xwork_runtime_find_workspace`
- `xwork_workspace_destroy`

---

### xwork_runtime_find_workspace

按 workspace id 查找 runtime 中已注册的 workspace。

**功能：**

你可以用该函数在运行时把外部配置、run snapshot 或 UI 选择的 workspace id 解析为 live workspace 对象。

**函数原型：**

```c
XWORK_API xwork_workspace *xwork_runtime_find_workspace(
    const xwork_runtime *pRuntime,
    const char *sWorkspaceId
);
```

**参数：**

- `pRuntime`：输入参数。可为 `NULL`；为 `NULL` 时返回 `NULL`。
- `sWorkspaceId`：输入参数。可为 `NULL` 或空字符串；无效时返回 `NULL`。

**返回值：**

- 找到时返回 workspace 借用指针。
- 未找到或参数无效时返回 `NULL`。

**资源归属：**

返回值由 runtime 拥有，调用者不能释放。返回指针在 workspace 或 runtime 被销毁后失效。

**补充说明：**

- 匹配规则是 workspace id 字符串精确匹配。
- 该函数不创建 workspace，也不加载持久化状态。

**范例代码：**

```c
#include "xwork.h"

int main(void) {
    xwork_runtime *runtime = NULL;
    xwork_workspace *workspace = NULL;
    xwork_runtime_options options;

    xwork_runtime_options_init(&options);
    if (xwork_runtime_create(&options, &runtime) != XWORK_OK) {
        return 1;
    }

    workspace = xwork_runtime_find_workspace(runtime, "main");
    xwork_runtime_destroy(runtime);
    return workspace == NULL ? 0 : 1;
}
```

**相关 API：**

- `xwork_runtime_add_workspace`
- `xwork_workspace_get_id`

---

### xwork_workspace_destroy

销毁 workspace。

**功能：**

你可以用该函数提前移除并释放一个 workspace，而不是等待 `xwork_runtime_destroy` 统一释放。

**函数原型：**

```c
XWORK_API void xwork_workspace_destroy(xwork_workspace *pWorkspace);
```

**参数：**

- `pWorkspace`：输入/销毁参数。可为 `NULL`；为 `NULL` 时函数不执行任何操作。

**返回值：**

无。

**资源归属：**

- 函数释放 workspace 自身和内部复制的字符串。
- 函数不会释放借用的 `xllm_memory *`。
- 如果 workspace 仍挂在 runtime 上，函数会先从 runtime 链表中摘除它。

**补充说明：**

- 不要对同一 workspace 重复销毁。
- 销毁后，之前通过 getter 或 find 获得的指针全部失效。

**范例代码：**

```c
#include "xwork.h"

void close_workspace_early(xwork_workspace *workspace) {
    xwork_workspace_destroy(workspace);
}
```

**相关 API：**

- `xwork_runtime_add_workspace`
- `xwork_runtime_destroy`

---

### xwork_workspace_get_id

获取 workspace id。

**功能：**

你可以用该函数在日志、UI、run summary、tool context 或恢复流程中读取 workspace 的稳定 id。

**函数原型：**

```c
XWORK_API const char *xwork_workspace_get_id(const xwork_workspace *pWorkspace);
```

**参数：**

- `pWorkspace`：输入参数。可为 `NULL`；为 `NULL` 时返回 `NULL`。

**返回值：**

- 返回 workspace id 的借用字符串。
- 如果 `pWorkspace` 为 `NULL`，返回 `NULL`。

**资源归属：**

返回值由 workspace 拥有，调用者不能释放。workspace 销毁后返回指针失效。

**补充说明：**

- workspace id 来自 `xwork_workspace_options::sWorkspaceId` 的复制值。
- 不要修改返回字符串。

**范例代码：**

```c
#include "xwork.h"
#include <stdio.h>

void print_workspace_id(const xwork_workspace *workspace) {
    const char *id = xwork_workspace_get_id(workspace);
    printf("workspace: %s\n", id ? id : "(null)");
}
```

**相关 API：**

- `xwork_runtime_find_workspace`
- `xwork_workspace_get_root_path`

---

### xwork_workspace_get_root_path

获取 workspace 根目录。

**功能：**

你可以用该函数把 workspace 映射回宿主文件系统路径，用于日志、UI 展示、host tool 请求或路径策略判断。

**函数原型：**

```c
XWORK_API const char *xwork_workspace_get_root_path(const xwork_workspace *pWorkspace);
```

**参数：**

- `pWorkspace`：输入参数。可为 `NULL`；为 `NULL` 时返回 `NULL`。

**返回值：**

- 返回 root path 的借用字符串。
- 如果 `pWorkspace` 为 `NULL`，返回 `NULL`。

**资源归属：**

返回值由 workspace 拥有，调用者不能释放。workspace 销毁后返回指针失效。

**补充说明：**

- xwork 不在该 getter 中规范化路径。
- 路径是否存在、是否可访问、是否允许写入，需要由 host service/policy 层判断。

**范例代码：**

```c
#include "xwork.h"
#include <stdio.h>

void print_workspace_root(const xwork_workspace *workspace) {
    const char *root = xwork_workspace_get_root_path(workspace);
    printf("root: %s\n", root ? root : "(null)");
}
```

**相关 API：**

- `xwork_workspace_get_id`
- `xwork_workspace_sync_memory`

---

### xwork_workspace_is_memory_enabled

查询 workspace 是否启用 memory。

**功能：**

你可以在执行 sync、构建 model context 或展示 workspace 状态前，用该函数判断该 workspace 是否声明启用 memory。

**函数原型：**

```c
XWORK_API bool xwork_workspace_is_memory_enabled(const xwork_workspace *pWorkspace);
```

**参数：**

- `pWorkspace`：输入参数。可为 `NULL`；为 `NULL` 时返回 `false`。

**返回值：**

- 返回 `true` 表示 workspace 配置中启用了 memory。
- 返回 `false` 表示未启用 memory 或参数为 `NULL`。

**资源归属：**

该函数不返回指针，不转移资源所有权。

**补充说明：**

- 返回 `true` 不代表 `pMemory` 一定可用；调用 `xwork_workspace_sync_memory` 时仍会校验 `pMemory`。
- 如果只需要取得 memory 指针，可直接调用 `xwork_workspace_get_memory` 并检查返回值。

**范例代码：**

```c
#include "xwork.h"

int has_workspace_memory(const xwork_workspace *workspace) {
    return xwork_workspace_is_memory_enabled(workspace) ? 1 : 0;
}
```

**相关 API：**

- `xwork_workspace_get_memory`
- `xwork_workspace_sync_memory`

---

### xwork_workspace_get_memory

获取 workspace 绑定的 `xllm_memory`。

**功能：**

你可以用该函数把 workspace memory 暴露给上层 orchestrator、诊断工具或宿主 UI，以便检查或追加上下文。

**函数原型：**

```c
XWORK_API xllm_memory *xwork_workspace_get_memory(const xwork_workspace *pWorkspace);
```

**参数：**

- `pWorkspace`：输入参数。可为 `NULL`；为 `NULL` 时返回 `NULL`。

**返回值：**

- 返回借用的 `xllm_memory *`。
- 如果 workspace 没有关联 memory 或参数为 `NULL`，返回 `NULL`。

**资源归属：**

返回值由调用方原始 owner 拥有，xwork 只借用。调用者不能通过 workspace getter 取得所有权。

**补充说明：**

- `pMemory` 来自 `xwork_workspace_options::pMemory`。
- xwork 不负责销毁该 memory；它必须比 workspace 活得更久。

**范例代码：**

```c
#include "xwork.h"

int workspace_has_memory_object(const xwork_workspace *workspace) {
    return xwork_workspace_get_memory(workspace) != NULL ? 1 : 0;
}
```

**相关 API：**

- `xwork_workspace_is_memory_enabled`
- `xwork_workspace_sync_memory`

---

### xwork_workspace_sync_memory

扫描 workspace 根目录并同步到 `xllm_memory`。

**功能：**

你可以在 run 开始前或用户显式刷新时调用该函数，把工作区文本文件 ingest 到绑定的 `xllm_memory`，供模型上下文检索使用。

**函数原型：**

```c
XWORK_API xwork_status xwork_workspace_sync_memory(
    xwork_workspace *pWorkspace,
    xwork_workspace_memory_sync_summary *pSummary
);
```

**参数：**

- `pWorkspace`：输入/输出参数。必须非 `NULL`，并且 workspace 必须启用 memory、绑定 `pMemory`、具有非空 root path。
- `pSummary`：输出参数。可为 `NULL`。非 `NULL` 时函数会先清零，再写入同步统计。

**返回值：**

- `XWORK_OK`：同步完成。
- `XWORK_ERROR_INVALID_ARGUMENT`：workspace 无效、未启用 memory、未绑定 memory 或 root path 无效。
- `XWORK_ERROR_EXTERNAL_FAILURE`：底层 `xllm_memory_sync_workspace` 失败。

**资源归属：**

- 函数不转移 workspace 或 memory 所有权。
- `pSummary` 由调用者拥有，函数只写入计数字段。
- 底层 xllm 临时结果会在函数返回前释放。

**补充说明：**

- 当前实现递归扫描 workspace root，默认跳过 hidden 文件，加载 `.gitignore`，跳过 unchanged 文件，并使用 `workspace://` 作为 source URI 前缀。
- policy 字符串和 `iMemorySyncMaxFileBytes` 来自 workspace 创建时复制的 options。
- 该函数会修改 `xllm_memory`，不要和同一个 memory 的其他写入操作并发执行，除非宿主明确提供串行化或底层对象支持并发。

**范例代码：**

```c
#include "xwork.h"
#include <stdio.h>

int sync_workspace_memory(xwork_workspace *workspace) {
    xwork_workspace_memory_sync_summary summary;
    xwork_status status;

    status = xwork_workspace_sync_memory(workspace, &summary);
    if (status != XWORK_OK) {
        return 1;
    }

    printf("ingested: %zu\n", summary.iIngestedFileCount);
    return 0;
}
```

**相关 API：**

- `xwork_workspace_sync_memory_file`
- `xwork_workspace_get_memory`
- `xwork_workspace_memory_sync_summary_init`

---

### xwork_workspace_sync_memory_file

同步单个文件变更到 `xllm_memory`。

**功能：**

你可以在文件保存、编辑器 buffer 写入或文件 watcher 捕获变更后调用该函数，只刷新一个路径对应的 memory record，而不重新扫描整个 workspace。

**函数原型：**

```c
XWORK_API xwork_status xwork_workspace_sync_memory_file(
    xwork_workspace *pWorkspace,
    const char *sPath,
    xwork_workspace_memory_file_sync_summary *pSummary
);
```

**参数：**

- `pWorkspace`：输入/输出参数。必须非 `NULL`，并且 workspace 必须启用 memory、绑定 `pMemory`、具有非空 root path。
- `sPath`：输入参数。必须为非空字符串。通常是 workspace root 下的文件路径。
- `pSummary`：输出参数。可为 `NULL`。非 `NULL` 时函数会先清零，再写入变更统计。

**返回值：**

- `XWORK_OK`：文件同步完成。
- `XWORK_ERROR_INVALID_ARGUMENT`：workspace、memory、root path 或 `sPath` 无效。
- `XWORK_ERROR_EXTERNAL_FAILURE`：底层 `xllm_memory_sync_file` 失败。

**资源归属：**

- 函数不转移 workspace、memory 或路径字符串所有权。
- `pSummary` 由调用者拥有，函数只写入计数字段。
- 底层 xllm change set 会在函数返回前释放。

**补充说明：**

- 当前实现使用 workspace 默认策略，跳过 hidden/unchanged 文件，并使用 `workspace://` 作为 source URI 前缀。
- `sPath` 是否存在、是否在 root 内、是否被策略允许，由 xllm sync 和宿主策略共同决定；调用前建议在产品层做 workspace boundary 校验。
- 该函数会修改 `xllm_memory`，应与其他 memory 写操作串行化。

**范例代码：**

```c
#include "xwork.h"
#include <stdio.h>

int sync_one_file(xwork_workspace *workspace, const char *path) {
    xwork_workspace_memory_file_sync_summary summary;
    xwork_status status;

    status = xwork_workspace_sync_memory_file(workspace, path, &summary);
    if (status != XWORK_OK) {
        return 1;
    }

    printf("changes: %zu\n", summary.iChangeCount);
    return 0;
}
```

**相关 API：**

- `xwork_workspace_sync_memory`
- `xwork_workspace_get_root_path`
- `xwork_workspace_memory_file_sync_summary_init`

## 错误处理

Workspace API 常见错误包括：

- `XWORK_ERROR_INVALID_ARGUMENT`：runtime、workspace、options、id、root path、memory 或 path 参数无效。
- `XWORK_ERROR_ALREADY_EXISTS`：同一 runtime 中重复注册 workspace id。
- `XWORK_ERROR_NO_MEMORY`：创建 workspace 或复制字符串失败。
- `XWORK_ERROR_EXTERNAL_FAILURE`：xllm memory sync 返回失败。

## 恢复边界

持久化 run snapshot 保存 workspace id 引用，不保存 live `xllm_memory *`。恢复 run 前，应先重新创建 runtime、注册兼容 workspace，并确保需要的 memory 对象已经由宿主创建或恢复。

## 线程边界

workspace 不是并发 mutation 容器。添加、销毁、memory sync、host tool 对同一工作区的写操作应由调用方或产品层策略串行化。只读 getter 可以在对象生命周期有效且没有并发销毁时调用。

## 相关文档

- [Runtime API](api-runtime.md)
- [xllm 集成 API](api-xllm-integration.md)
- [workspace memory 教程](../guide/workspace-memory-intro.md)
