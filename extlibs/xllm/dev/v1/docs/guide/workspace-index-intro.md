# 工作区索引入门

工作区索引用来把一个目录里的源码、Markdown、文本等文件写入 `xllm_memory`，让模型能够按问题检索项目知识。

[返回教程入口](README.md) | [Memory RAG 入门](memory-rag-intro.md) | [Workspace API](../api/api-memory-workspace.md)

## 你会学到什么

本文会带你理解：

- `ingest_workspace` 和 `sync_workspace` 的区别。
- 怎样设置扩展名、忽略目录、`.gitignore` 和最大文件大小。
- 怎样用稳定的 record ID 和 source URI 表示文件来源。
- 怎样查询索引状态和健康检查。
- 什么时候使用 watcher 做增量更新。

## 什么时候索引工作区

当你的应用需要回答“这个项目里某个模块怎么用”、“某个接口在哪里定义”、“文档里有没有约定”这类问题时，应先把工作区写入 memory，再做 RAG 检索。

工作区索引适合：

- C/C++ 头文件和示例代码。
- Markdown 文档。
- 配置文件和小型文本文件。
- 经过过滤后的业务知识文件。

工作区索引不适合：

- 大型二进制文件。
- 构建产物、缓存、依赖目录。
- 密钥、证书、数据库 dump。
- 需要实时逐字读取的超大日志。

## Ingest 和 Sync 的区别

| API | 用法 |
| --- | --- |
| `xllm_memory_ingest_workspace` | 扫描目录，把符合规则的文件写入 memory。适合首次索引或简单重建。 |
| `xllm_memory_sync_workspace` | 扫描目录并和已有记录对比，新增、更新、删除索引。适合长期维护。 |
| `xllm_memory_sync_file` | 单个文件变化时更新该文件记录。 |
| `xllm_memory_sync_file_events` | 已经收集到一批文件事件时，批量同步。 |

初学时先用 `xllm_memory_ingest_workspace`。当你开始做 IDE、后台服务或持续索引时，再改为 `sync_workspace` 或 watcher。

## 最小工作区索引

下面是最小流程。完整 smoke 可参考 `examples/smoke_memory_ingest_workspace.c`。

```c
xllm_memory_ingest_workspace_options tWorkspace;
xllm_memory_ingest_directory_result tResult;
xllm_error tError;

xllm_error_init(&tError);
xllm_memory_ingest_workspace_options_init(&tWorkspace);
xllm_memory_ingest_directory_result_init(&tResult);

tWorkspace.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
tWorkspace.sPath = "D:\\git\\xllm";
tWorkspace.sRecordIdPrefix = "workspace";
tWorkspace.sSourceUriPrefix = "workspace://";
tWorkspace.sAllowedExtensions = ".c;.h;.md;.txt";
tWorkspace.uMaxFileBytes = 256u * 1024u;

if ( xllm_memory_ingest_workspace(
        pMemory,
        &tWorkspace,
        &tResult,
        &tError
    ) != XRT_NET_OK ) {
    fprintf(stderr, "workspace ingest failed: %s\n",
            tError.sMessage ? tError.sMessage : "(null)");
}

printf("visited=%u ingested=%u skipped=%u failed=%u\n",
       tResult.uVisitedFileCount,
       tResult.uIngestedFileCount,
       tResult.uSkippedFileCount,
       tResult.uFailedFileCount);

xllm_memory_ingest_directory_result_reset(&tResult);
```

字段含义：

| 字段 | 建议 |
| --- | --- |
| `eScope` | 工作区知识通常使用 `XLLM_MEMORY_SCOPE_KNOWLEDGE` |
| `sPath` | 要索引的根目录 |
| `sRecordIdPrefix` | 生成记录 ID 的前缀，建议稳定 |
| `sSourceUriPrefix` | 生成来源 URI 的前缀，例如 `workspace://` |
| `sAllowedExtensions` | 分号分隔的扩展名列表 |
| `uMaxFileBytes` | 单文件最大字节数，避免大文件进入索引 |

## 忽略规则

工作区索引必须先过滤，再写入。常用过滤字段：

```c
tWorkspace.bRecursive = true;
tWorkspace.bSkipHidden = true;
tWorkspace.bLoadGitIgnore = true;
tWorkspace.sIgnoredDirectories = ".git;build;out;node_modules;.venv";
tWorkspace.sIgnoredExtensions = ".exe;.dll;.obj;.pdb;.zip;.png;.jpg";
tWorkspace.sIgnoredPathPatterns = "*secret*;*.key;*.pem";
tWorkspace.sIgnoreFiles = ".gitignore;.xllmignore";
```

建议规则：

- 默认递归扫描，但排除构建目录和依赖目录。
- 开启 `.gitignore`，让索引规则和项目已有规则保持一致。
- 用 `sIgnoredPathPatterns` 跳过密钥、证书、敏感配置。
- 给 `uMaxFileBytes` 设置明确上限，避免日志和生成文件占满 memory。

## Record ID 和 Source URI

工作区文件会变。你需要稳定地识别“同一个文件的新版本”。因此：

- `sRecordIdPrefix` 用来生成稳定 record ID。
- `sSourceUriPrefix` 用来生成可读、可追踪的来源 URI。

例如 `sRecordIdPrefix = "workspace"`、`sSourceUriPrefix = "workspace://"` 时，记录可能带有：

```text
record id:  workspace:src/main.c
source uri: workspace://src/main.c
```

不要每次索引都使用随机 record ID。否则同一个文件的旧版本会留在 memory 里，搜索时可能命中旧内容。

## 同步已有工作区

当你希望删除已经不存在的文件记录，并更新变更文件时，使用 `xllm_memory_sync_workspace`：

```c
xllm_memory_sync_workspace_result tSync;

xllm_memory_sync_workspace_result_init(&tSync);

if ( xllm_memory_sync_workspace(
        pMemory,
        &tWorkspace,
        &tSync,
        &tError
    ) == XRT_NET_OK ) {
    printf("examined=%u removed=%u\n",
           tSync.uExaminedRecordCount,
           tSync.uRemovedRecordCount);
}

xllm_memory_sync_workspace_result_reset(&tSync);
```

`sync_workspace` 更适合长期运行的应用。它会把“当前文件系统状态”和“已有索引状态”对齐。

## 查询状态和健康检查

完成索引后，可以用状态和健康检查确认 memory 是否可用：

```c
xllm_memory_workspace_status_options tStatusOptions;
xllm_memory_workspace_status tStatus;
xllm_memory_health_check_options tHealthOptions;
xllm_memory_health_check tHealth;

xllm_memory_workspace_status_options_init(&tStatusOptions);
xllm_memory_workspace_status_init(&tStatus);
xllm_memory_health_check_options_init(&tHealthOptions);
xllm_memory_health_check_init(&tHealth);

tStatusOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
tStatusOptions.sSourceUriPrefix = "workspace://";

xllm_memory_get_workspace_status(pMemory, &tStatusOptions, &tStatus, &tError);
xllm_memory_check_health(pMemory, &tHealthOptions, &tHealth, &tError);
```

状态查询用于回答“索引里有多少工作区记录”；健康检查用于回答“memory backend 是否能正常工作”。

## 文件事件和 Watcher

如果你的应用能收到文件变化事件，可以先把事件放入队列，再批量同步：

```c
xllm_memory_file_event_queue *pQueue = NULL;
xllm_memory_change_set tChanges;
xllm_memory_file_event_queue_drain_options tDrain;

xllm_memory_file_event_queue_create(&pQueue);
xllm_memory_file_event_queue_push_updated(pQueue, "D:\\git\\xllm\\xllm.h", &tError);

xllm_memory_change_set_init(&tChanges);
xllm_memory_file_event_queue_drain_options_init(&tDrain);
tDrain.tBaseOptions.eScope = XLLM_MEMORY_SCOPE_KNOWLEDGE;
tDrain.tBaseOptions.sRootPath = "D:\\git\\xllm";
tDrain.tBaseOptions.sSourceUriPrefix = "workspace://";

xllm_memory_file_event_queue_drain(
    pMemory,
    pQueue,
    &tDrain,
    &tChanges,
    &tError
);
```

当事件来源很多、变化频繁时，再使用 watcher bridge、pump 或 worker。它们负责合并事件、debounce 和批量 flush。

## 常见错误

不要把整个仓库无过滤写入 memory。`build/`、`.git/`、依赖目录、二进制文件会制造大量噪声。

不要关闭稳定 record ID。工作区索引最重要的是“文件变了就更新同一条记录”，不是不断追加新记录。

不要忽略 skipped 和 failed 计数。`uSkippedFileCount` 高可能是正常过滤；`uFailedFileCount` 高通常表示权限、编码、文件大小或路径问题。

不要把工作区索引当作代码解析器。Memory RAG 能提供相关片段，但符号跳转、AST 分析、精确引用仍应交给专门的语言服务。

## 下一步

- 想把索引结果用于问答，读 [Memory RAG 入门](memory-rag-intro.md)。
- 想做持续同步，读 [Memory Watcher API](../api/api-memory-watcher.md)。
- 想看完整工作区 RAG 场景，后续读 [工作区 RAG 案例](../case/workspace-rag.md)。
