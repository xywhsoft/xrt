# xllm Memory API

> 状态：中文初稿已生成，待审阅。  
> 头文件：`xllm-memory.h`

本页讲解 memory 子系统的基础对象：如何创建一个记忆库、如何选择检索方案、如何配置 embedding、如何查看当前记忆库的基本状态。  
如果你只是想把文本或文件写入 memory，请先读完本页的创建部分，再继续阅读 [Memory Ingest API](api-memory-ingest.md)。

## 你将学到什么

- `xllm_memory` 是什么，以及它和 `xllm_runtime` 的关系。
- `memory` 与 `knowledge` 两种 scope 的区别。
- sparse、ONNX E5、自定义 embedder 三种方案如何选择。
- 如何初始化 options，创建、销毁 memory。
- 如何查询 memory 的 scheme、profile、记录数和 chunk 数。
- 如何使用内置 embedder 探测与创建接口。

## 核心概念

### memory store

`xllm_memory` 表示一个可检索的记忆库。它负责保存 record、chunk、元数据和可选的向量索引。你可以把它理解成 xllm 内置的轻量 RAG 存储层。

典型使用顺序是：

1. 创建 `xllm_runtime`。
2. 初始化并填写 `xllm_memory_options`。
3. 调用 `xllm_memory_create` 得到 `xllm_memory *`。
4. 使用 ingest API 写入内容。
5. 使用 search API 检索内容，或者把检索结果注入 request/turn。
6. 调用 `xllm_memory_destroy` 释放 memory。
7. 调用 `xllm_runtime_destroy` 释放 runtime。

### scope

`xllm_memory_scope` 用来区分记忆的用途。

| 枚举值 | 含义 | 适合放什么 |
| --- | --- | --- |
| `XLLM_MEMORY_SCOPE_ANY` | 查询时不限定范围 | 搜索、统计、删除时的通配范围 |
| `XLLM_MEMORY_SCOPE_MEMORY` | 对话记忆 | 用户偏好、任务、事实、会话摘要 |
| `XLLM_MEMORY_SCOPE_KNOWLEDGE` | 知识资料 | 文档、源码、工作区文件、知识库片段 |

初学时可以遵守一个简单规则：用户和对话相关的信息放进 `MEMORY`，外部资料和文件放进 `KNOWLEDGE`。

### record 和 chunk

record 是一次入库的逻辑对象，例如一段笔记、一个文件、一条偏好。chunk 是 xllm 为了检索把 record 切分出来的文本片段。

检索返回的是 chunk 命中，但命中里会带上所属 record 的 id、标题、来源 URI 和 metadata。这样你既能看到具体匹配文本，也能知道它来自哪里。

## 常量

### 编译期方案开关

**功能**：控制当前构建包含哪些 memory 检索方案。

```c
#define XLLM_MEMORY_SCHEME_MODE_ONNX_E5 1
#define XLLM_MEMORY_SCHEME_MODE_BUILTIN_SPARSE 2
#define XLLM_MEMORY_SCHEME_MODE_CUSTOM 3
#define XLLM_MEMORY_SCHEME_MODE_ALL 4

#ifndef XLLM_MEMORY_SCHEME_MODE
#define XLLM_MEMORY_SCHEME_MODE XLLM_MEMORY_SCHEME_MODE_ALL
#endif
```

**说明**：

- `XLLM_MEMORY_SCHEME_MODE_ALL` 是默认值，表示尽量编译全部可用方案。
- `BUILTIN_SPARSE` 不依赖外部 embedding 模型，适合快速入门和测试。
- `ONNX_E5` 依赖内置 ONNX E5 embedder 相关资源，适合需要语义检索的场景。
- `CUSTOM` 用于你自己提供 embedding 回调。

## 类型

### xllm_memory

**功能**：memory store 的不透明句柄。

```c
typedef struct xllm_memory xllm_memory;
```

你不能直接访问它的字段，只能通过 API 操作。这样的设计可以让 xllm 在内部调整存储结构，而不破坏你的代码。

### xllm_memory_scheme

**功能**：选择 memory 的检索和 embedding 方案。

```c
typedef enum {
    XLLM_MEMORY_SCHEME_AUTO = 0,
    XLLM_MEMORY_SCHEME_BUILTIN_SPARSE,
    XLLM_MEMORY_SCHEME_ONNX_E5,
    XLLM_MEMORY_SCHEME_CUSTOM
} xllm_memory_scheme;
```

| 值 | 含义 |
| --- | --- |
| `XLLM_MEMORY_SCHEME_AUTO` | 让 xllm 自动选择可用方案。 |
| `XLLM_MEMORY_SCHEME_BUILTIN_SPARSE` | 使用内置稀疏检索，不需要向量模型。 |
| `XLLM_MEMORY_SCHEME_ONNX_E5` | 使用内置 multilingual E5 small ONNX embedder。 |
| `XLLM_MEMORY_SCHEME_CUSTOM` | 使用你传入的自定义 embedder。 |

**学习建议**：第一次使用时选 `AUTO` 或不设置；当你想排除模型资源问题时，显式设置为 `BUILTIN_SPARSE`。

### xllm_memory_embed_task

**功能**：告诉 embedder 当前是在编码查询还是文档。

```c
typedef enum {
    XLLM_MEMORY_EMBED_QUERY = 1,
    XLLM_MEMORY_EMBED_DOCUMENT
} xllm_memory_embed_task;
```

一些 embedding 模型会对 query 和 document 使用不同前缀，例如 E5 系列常见的 `query:` 与 `passage:`。自定义 embedder 可以根据该值选择合适的编码方式。

### xllm_memory_embedding

**功能**：承载一次 embedding 结果。

```c
typedef struct {
    float *pfValues;
    uint32 uValueCount;
} xllm_memory_embedding;
```

| 字段 | 含义 |
| --- | --- |
| `pfValues` | 浮点向量数组。 |
| `uValueCount` | 向量维度。 |

**补充说明**：如果你提供自定义 embedder，`pfnResetEmbedding` 需要知道如何释放 `pfValues`。如果向量内存由你的 embedder 分配，也应该由你的 reset 回调释放。

### xllm_memory_embedder

**功能**：描述一个 embedding 提供者。

```c
typedef struct {
    xllm_memory_embed_text_fn pfnEmbedText;
    xllm_memory_embed_reset_fn pfnResetEmbedding;
    xllm_memory_embed_clone_fn pfnCloneCtx;
    xllm_memory_embed_dispose_fn pfnDisposeCtx;
    void *pCtx;
    xvalue tVendorExtra;
} xllm_memory_embedder;
```

| 字段 | 含义 |
| --- | --- |
| `pfnEmbedText` | 把文本转成向量的回调。 |
| `pfnResetEmbedding` | 释放或重置 `xllm_memory_embedding` 的回调。 |
| `pfnCloneCtx` | 克隆 embedder 上下文的回调，可为 `NULL`。 |
| `pfnDisposeCtx` | 销毁 embedder 上下文的回调，可为 `NULL`。 |
| `pCtx` | 你的 embedder 上下文，例如模型句柄。 |
| `tVendorExtra` | 扩展字段，保留给集成方。 |

**补充说明**：如果你选择 `XLLM_MEMORY_SCHEME_CUSTOM`，通常需要至少设置 `pfnEmbedText` 和 `pfnResetEmbedding`。

### xllm_memory_builtin_embedder_kind

**功能**：选择 xllm 提供的内置 embedder。

```c
typedef enum {
    XLLM_MEMORY_BUILTIN_EMBEDDER_NONE = 0,
    XLLM_MEMORY_BUILTIN_EMBEDDER_HASH,
    XLLM_MEMORY_BUILTIN_EMBEDDER_MULTILINGUAL_E5_SMALL_ONNX
} xllm_memory_builtin_embedder_kind;
```

| 值 | 含义 |
| --- | --- |
| `NONE` | 不使用内置 embedder。 |
| `HASH` | 使用哈希型内置 embedder，适合测试和无模型环境。 |
| `MULTILINGUAL_E5_SMALL_ONNX` | 使用 multilingual E5 small ONNX embedder。 |

### xllm_memory_builtin_embedder_options

**功能**：配置内置 embedder 的模型、tokenizer 和运行时资源。

```c
typedef struct {
    xllm_memory_builtin_embedder_kind eKind;
    const char *sRuntimeDllPath;
    const char *sModelPath;
    const char *sTokenizerPath;
    bool bAutoDiscoverAssets;
    uint32 uHashDimensions;
    xvalue tVendorExtra;
} xllm_memory_builtin_embedder_options;
```

| 字段 | 含义 |
| --- | --- |
| `eKind` | 内置 embedder 类型。 |
| `sRuntimeDllPath` | ONNX Runtime 动态库路径，可为 `NULL` 让 xllm 自动发现。 |
| `sModelPath` | embedding 模型路径。 |
| `sTokenizerPath` | tokenizer 路径。 |
| `bAutoDiscoverAssets` | 是否自动发现模型相关资源。 |
| `uHashDimensions` | `HASH` embedder 的向量维度。 |
| `tVendorExtra` | 扩展字段。 |

### xllm_memory_builtin_embedder_probe

**功能**：保存内置 embedder 探测结果。

```c
typedef struct {
    xllm_memory_builtin_embedder_kind eKind;
    bool bRuntimeFound;
    bool bModelFound;
    bool bTokenizerFound;
    bool bImplemented;
    bool bReady;
    char *sResolvedRuntimeDllPath;
    char *sResolvedModelPath;
    char *sResolvedTokenizerPath;
} xllm_memory_builtin_embedder_probe;
```

**释放规则**：调用 `xllm_memory_builtin_embedder_probe_reset` 释放探测结果中由 xllm 分配的字符串。

### xllm_memory_options

**功能**：创建 memory store 时的配置。

```c
typedef struct {
    const char *sNamespace;
    const char *sSqlitePath;
    const char *sSqliteVectorExtensionPath;
    bool bLoadSqliteVectorExtension;
    xllm_memory_embedder tEmbedder;
    xllm_memory_scheme eScheme;
    const char *sMemoryProfileId;
    bool bEnableHybridSearch;
    xllm_opt_float tLexicalWeight;
    xllm_opt_float tVectorWeight;
    uint32 uDefaultChunkChars;
    uint32 uDefaultChunkOverlapChars;
    uint32 uDefaultMaxHits;
    uint32 uSqliteBusyTimeoutMs;
    bool bDisableSqliteWal;
    xvalue tVendorExtra;
} xllm_memory_options;
```

| 字段 | 含义 |
| --- | --- |
| `sNamespace` | 命名空间，用于区分不同应用或租户。 |
| `sSqlitePath` | SQLite 文件路径。为 `NULL` 时使用内存或默认存储策略。 |
| `sSqliteVectorExtensionPath` | sqlite-vector 扩展路径。 |
| `bLoadSqliteVectorExtension` | 是否加载 SQLite 向量扩展。 |
| `tEmbedder` | 自定义或内置创建后的 embedder。 |
| `eScheme` | 检索方案。 |
| `sMemoryProfileId` | memory profile 标识，用于检测存储配置是否匹配。 |
| `bEnableHybridSearch` | 是否启用 lexical + vector 混合检索。 |
| `tLexicalWeight` | 词法检索权重。 |
| `tVectorWeight` | 向量检索权重。 |
| `uDefaultChunkChars` | 默认 chunk 字符数。 |
| `uDefaultChunkOverlapChars` | 默认 chunk 重叠字符数。 |
| `uDefaultMaxHits` | 默认检索命中数。 |
| `uSqliteBusyTimeoutMs` | SQLite busy timeout。 |
| `bDisableSqliteWal` | 是否禁用 WAL。 |
| `tVendorExtra` | 扩展字段。 |

**默认值**：`xllm_memory_options_init` 会设置 `AUTO` scheme、启用 hybrid search、词法和向量权重均为 `1.0`、chunk 大小 `800`、overlap `120`、默认命中数 `5`、SQLite busy timeout `5000ms`。

## API

### xllm_memory_embedder_init

**功能**：初始化 `xllm_memory_embedder`。

**原型**：

```c
XLLM_API void xllm_memory_embedder_init(xllm_memory_embedder *pEmbedder);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pEmbedder` | 要初始化的 embedder 结构体。可为 `NULL`。 |

**返回值**：无。

**补充说明**：这个函数会把结构体清零。你在栈上声明 `xllm_memory_embedder` 后，应先调用它，再填写回调字段。

**范例代码**：

```c
xllm_memory_embedder embedder;
xllm_memory_embedder_init(&embedder);
embedder.pfnEmbedText = my_embed_text;
embedder.pfnResetEmbedding = my_reset_embedding;
embedder.pCtx = my_model;
```

### xllm_memory_embedder_reset

**功能**：释放 embedder 持有的上下文并清空结构体。

**原型**：

```c
XLLM_API void xllm_memory_embedder_reset(xllm_memory_embedder *pEmbedder);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pEmbedder` | 要重置的 embedder。 |

**返回值**：无。

**补充说明**：如果 `pEmbedder->pfnDisposeCtx` 非空，reset 会通过该回调释放 `pCtx`。这对 `xllm_memory_make_builtin_embedder` 创建出来的 embedder 尤其重要。

### xllm_memory_builtin_embedder_options_init

**功能**：初始化内置 embedder 的 options。

**原型**：

```c
XLLM_API void xllm_memory_builtin_embedder_options_init(
    xllm_memory_builtin_embedder_options *pOptions
);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pOptions` | 要初始化的 options。 |

**返回值**：无。

**补充说明**：初始化后再设置 `eKind`、模型路径或自动发现选项。不要手动只清零然后跳过 init，因为默认值可能随版本演进。

### xllm_memory_builtin_embedder_probe_reset

**功能**：释放内置 embedder 探测结果。

**原型**：

```c
XLLM_API void xllm_memory_builtin_embedder_probe_reset(
    xllm_memory_builtin_embedder_probe *pProbe
);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pProbe` | 要释放的探测结果。 |

**返回值**：无。

**补充说明**：`xllm_memory_probe_builtin_embedder` 可能会在 probe 中写入解析后的路径字符串。使用完必须 reset。

### xllm_memory_probe_builtin_embedder

**功能**：检查内置 embedder 所需资源是否可用。

**原型**：

```c
XLLM_API int xllm_memory_probe_builtin_embedder(
    const xllm_memory_builtin_embedder_options *pOptions,
    xllm_memory_builtin_embedder_probe *pProbe,
    xllm_error *pError
);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pOptions` | 内置 embedder 配置。 |
| `pProbe` | 输出探测结果。 |
| `pError` | 错误对象，可为 `NULL`。 |

**返回值**：

| 返回值 | 含义 |
| --- | --- |
| `XRT_NET_OK` | 探测完成。是否可用请看 `pProbe->bReady`。 |
| `XRT_NET_ERROR` | 参数错误或探测过程失败。 |

**补充说明**：探测成功不等于 embedder 一定 ready。比如 ONNX runtime 找到了，但模型文件没找到时，函数仍可能完成探测，`bReady` 为 `false`。

**范例代码**：

```c
xllm_memory_builtin_embedder_options options;
xllm_memory_builtin_embedder_probe probe;
xllm_error error;

xllm_error_init(&error);
xllm_memory_builtin_embedder_options_init(&options);
memset(&probe, 0, sizeof(probe));

options.eKind = XLLM_MEMORY_BUILTIN_EMBEDDER_MULTILINGUAL_E5_SMALL_ONNX;
options.bAutoDiscoverAssets = true;

if (xllm_memory_probe_builtin_embedder(&options, &probe, &error) == XRT_NET_OK) {
    if (probe.bReady) {
        /* 可以创建 ONNX E5 embedder。 */
    }
}

xllm_memory_builtin_embedder_probe_reset(&probe);
xllm_error_reset(&error);
```

### xllm_memory_make_builtin_embedder

**功能**：根据 options 创建一个内置 embedder。

**原型**：

```c
XLLM_API int xllm_memory_make_builtin_embedder(
    const xllm_memory_builtin_embedder_options *pOptions,
    xllm_memory_embedder *pEmbedder,
    xllm_error *pError
);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pOptions` | 内置 embedder 配置。 |
| `pEmbedder` | 输出 embedder。 |
| `pError` | 错误对象，可为 `NULL`。 |

**返回值**：

| 返回值 | 含义 |
| --- | --- |
| `XRT_NET_OK` | 创建成功。 |
| `XRT_NET_ERROR` | 配置不完整、资源不可用或内部初始化失败。 |

**补充说明**：创建成功后，`pEmbedder` 持有资源。最终应调用 `xllm_memory_embedder_reset`，或者把它交给 `xllm_memory_create` 后由 memory 生命周期管理。

### xllm_memory_options_init

**功能**：初始化 `xllm_memory_options` 并填入推荐默认值。

**原型**：

```c
XLLM_API void xllm_memory_options_init(xllm_memory_options *pOptions);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pOptions` | 要初始化的 options。可为 `NULL`。 |

**返回值**：无。

**默认值**：

| 字段 | 默认值 |
| --- | --- |
| `eScheme` | `XLLM_MEMORY_SCHEME_AUTO` |
| `bEnableHybridSearch` | `true` |
| `tLexicalWeight` | set, `1.0` |
| `tVectorWeight` | set, `1.0` |
| `uDefaultChunkChars` | `800` |
| `uDefaultChunkOverlapChars` | `120` |
| `uDefaultMaxHits` | `5` |
| `uSqliteBusyTimeoutMs` | `5000` |

**范例代码**：

```c
xllm_memory_options options;
xllm_memory_options_init(&options);
options.sNamespace = "demo";
options.sSqlitePath = "demo-memory.sqlite3";
options.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;
```

### xllm_memory_create

**功能**：创建 memory store。

**原型**：

```c
XLLM_API int xllm_memory_create(
    xllm_runtime *pRuntime,
    const xllm_memory_options *pOptions,
    xllm_memory **ppMemory
);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pRuntime` | xllm runtime。 |
| `pOptions` | memory 配置。可为 `NULL`，表示使用默认 options。 |
| `ppMemory` | 输出 memory 指针。 |

**返回值**：

| 返回值 | 含义 |
| --- | --- |
| `XRT_NET_OK` | 创建成功。 |
| `XRT_NET_ERROR` | runtime 无效、scheme 不可用、数据库初始化失败或 embedder 初始化失败。 |

**补充说明**：

- `pRuntime` 必须比 `xllm_memory` 活得更久。
- 如果你设置了 `sSqlitePath`，memory 内容会保存在 SQLite 文件中。
- 如果选择 `BUILTIN_SPARSE`，即使没有 embedding 模型也可以入库和搜索。
- 如果选择 `CUSTOM`，请在 `pOptions->tEmbedder` 中提供有效回调。

**范例代码**：

```c
xllm_runtime *runtime = NULL;
xllm_memory *memory = NULL;
xllm_runtime_options rt_options;
xllm_memory_options mem_options;

xllm_runtime_options_init(&rt_options);
xllm_runtime_create(&rt_options, &runtime);

xllm_memory_options_init(&mem_options);
mem_options.sNamespace = "tutorial";
mem_options.sSqlitePath = "tutorial-memory.sqlite3";
mem_options.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;

if (xllm_memory_create(runtime, &mem_options, &memory) != XRT_NET_OK) {
    /* 处理错误。 */
}

xllm_memory_destroy(memory);
xllm_runtime_destroy(runtime);
```

### xllm_memory_destroy

**功能**：销毁 memory store 并释放内部资源。

**原型**：

```c
XLLM_API void xllm_memory_destroy(xllm_memory *pMemory);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pMemory` | 要销毁的 memory。可为 `NULL`。 |

**返回值**：无。

**补充说明**：销毁后不要再使用之前从该 memory 得到的结果对象。已经独立拷贝到你自己内存里的字符串不受影响。

### xllm_memory_get_scheme

**功能**：读取当前 memory 实际使用的检索方案。

**原型**：

```c
XLLM_API xllm_memory_scheme xllm_memory_get_scheme(const xllm_memory *pMemory);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pMemory` | memory 对象。 |

**返回值**：当前实际 scheme。

**补充说明**：当创建时使用 `AUTO`，这个函数能告诉你最终落到了 sparse、ONNX E5 还是 custom。

### xllm_memory_get_profile_id

**功能**：读取当前 memory 的 profile id。

**原型**：

```c
XLLM_API const char *xllm_memory_get_profile_id(const xllm_memory *pMemory);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pMemory` | memory 对象。 |

**返回值**：profile id 字符串。返回指针由 memory 持有，不要释放。

**补充说明**：profile id 用来帮助判断一个已有 SQLite memory 文件是否与当前 embedding 维度、检索方案等配置匹配。

### xllm_memory_record_count

**功能**：统计 record 数量。

**原型**：

```c
XLLM_API size_t xllm_memory_record_count(
    const xllm_memory *pMemory,
    xllm_memory_scope eScope
);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pMemory` | memory 对象。 |
| `eScope` | 要统计的范围，常用 `ANY`、`MEMORY`、`KNOWLEDGE`。 |

**返回值**：record 数量。

**补充说明**：这是轻量状态查询，适合在入库后确认写入是否发生。

### xllm_memory_chunk_count

**功能**：统计 chunk 数量。

**原型**：

```c
XLLM_API size_t xllm_memory_chunk_count(
    const xllm_memory *pMemory,
    xllm_memory_scope eScope
);
```

**参数**：

| 参数 | 说明 |
| --- | --- |
| `pMemory` | memory 对象。 |
| `eScope` | 要统计的范围。 |

**返回值**：chunk 数量。

**补充说明**：一个 record 可能产生多个 chunk，所以 chunk 数通常大于或等于 record 数。

## 完整范例：创建一个最小 memory store

```c
#include "xllm.h"
#include "xllm-memory.h"

int main(void)
{
    xllm_runtime *runtime = NULL;
    xllm_memory *memory = NULL;
    xllm_runtime_options rt_options;
    xllm_memory_options mem_options;

    xllm_runtime_options_init(&rt_options);
    if (xllm_runtime_create(&rt_options, &runtime) != XRT_NET_OK) {
        return 1;
    }

    xllm_memory_options_init(&mem_options);
    mem_options.sNamespace = "learn-xllm";
    mem_options.sSqlitePath = "learn-xllm.sqlite3";
    mem_options.eScheme = XLLM_MEMORY_SCHEME_BUILTIN_SPARSE;

    if (xllm_memory_create(runtime, &mem_options, &memory) != XRT_NET_OK) {
        xllm_runtime_destroy(runtime);
        return 1;
    }

    printf("records=%zu chunks=%zu\n",
        xllm_memory_record_count(memory, XLLM_MEMORY_SCOPE_ANY),
        xllm_memory_chunk_count(memory, XLLM_MEMORY_SCOPE_ANY));

    xllm_memory_destroy(memory);
    xllm_runtime_destroy(runtime);
    return 0;
}
```

## 常见错误

### 误把对话记忆写入 KNOWLEDGE

如果你要保存“用户喜欢简洁回答”这类偏好，请使用 `XLLM_MEMORY_SCOPE_MEMORY`。`KNOWLEDGE` 更适合文档、文件和资料。

### 创建 custom scheme 但没有设置 embedder

`XLLM_MEMORY_SCHEME_CUSTOM` 需要有效的 `pfnEmbedText`。如果只是想快速体验，先用 `BUILTIN_SPARSE`。

### 忘记释放 probe 或结果对象

`xllm_memory_builtin_embedder_probe` 的结果要用 `xllm_memory_builtin_embedder_probe_reset` 释放。search、list、debug 等结果对象也有对应 reset API。

## 相关文档

- [Memory Ingest API](api-memory-ingest.md)
- [Memory Search API](api-memory-search.md)
- [Memory Workspace API](api-memory-workspace.md)
- [Diagnostics API](api-diagnostics.md)
- [返回 API 索引](README.md)
