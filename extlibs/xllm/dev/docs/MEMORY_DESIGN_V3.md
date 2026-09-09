# xllm Memory v3 落地设计

## 1. 目标

本文档用于把 `xllm memory` 从当前已经可用的 v2 实现，推进到更清晰、可维护、可扩展的 v3 设计。

v3 的目标不是推翻现有能力，而是把已经跑通的能力固化成稳定边界：

- 保留当前 `xllm-memory.h` 的应用层 API 主线。
- 把当前单体实现里的 `chunk`、`embed`、`retrieval`、`storage`、`context apply` 责任拆清楚。
- 固化 `memory_profile`、数据版本和索引版本，避免不同检索方案的数据静默混用。
- 让 `builtin_sparse`、`onnx_e5`、`custom` 三条路线在同一个 memory API 下可裁剪、可验证、可演进。
- 为后续 hybrid retrieval、质量评估、session 协同和生产化发布建立明确阶段。

## 2. 当前实现基线

当前实现已经具备这些能力：

- `xllm_memory_create()` 通过 `xllm_memory_options` 创建 memory 实例。
- 支持 `XLLM_MEMORY_SCHEME_MODE` 编译期裁剪。
- 支持 `builtin_sparse`、`onnx_e5`、`custom` 的 scheme/profile 解析。
- 支持 `sqlite3` 持久化 records/chunks/metadata。
- 支持 `onnx_e5` builtin embedder probe/create。
- 支持 E5 persisted embedding reload。
- 支持 `sqlite-vec` vec0 加速候选召回；缺失时可走内存 cosine fallback。
- 支持 workspace/file ingest/sync/watch queue/worker。
- 支持 conversation memory ingest、conversation/turn filters、priority、expiry、recency、trim。
- 支持 search result 转 `xllm_request` / `xllm_turn` context block。
- 支持 memory/session bridge，但 `session` 仍不硬依赖 `memory`。

当前主要问题：

- `xllm_memory.c` 仍承担过多职责，模块边界不够硬。
- SQLite schema 缺少显式 schema version、profile version、chunk/retrieval/embed profile 元数据列。
- `builtin_sparse` 目前更像文本匹配 baseline，还没有正式 `lindex` 模块和 BM25/postings 边界。
- `onnx_e5` 的 embed profile 还没有完整写入持久化元数据。
- hybrid 已有开关和权重，但 fusion 公式和候选来源契约尚未稳定。
- 当前 API 结果能用，但缺少 profile/diagnostic 查询接口，不利于宿主做生产排障。

## 3. v3 总体架构

对外仍保持三层应用边界：

- `core`: provider/runtime/request/response/tool/error。
- `session`: 短期上下文、摘要、压缩、pending/commit。
- `memory`: 长期记忆、知识入库、索引、召回、上下文注入。

对内把 `memory` 拆成六个机制层：

- `chunk`: 文本结构切分、chunk metadata。
- `embed`: embedding runtime/profile/cache。
- `lindex`: lexical postings / BM25 / phrase / metadata boost。
- `vindex`: vector index / vec0 / flat fallback / future HNSW。
- `store`: SQLite schema、migration、record/chunk/vector metadata 持久化。
- `pipeline`: ingest/search/apply 的业务编排。

默认依赖方向：

- `memory pipeline` 可以依赖 `chunk/embed/lindex/vindex/store`。
- `chunk/embed/lindex/vindex/store` 不反向依赖 `pipeline`。
- `session` 不依赖 `memory`。
- `memory-bridge` 依赖 `session + memory`，作为宿主便利层。

## 4. Profile 设计

v3 固化一个原则：落库数据必须记录 concrete profile，不保存模糊 `auto`。

最小 profile 层次：

- `memory_profile_id`: 例如 `builtin_sparse.v1`、`onnx_e5.v1`、`custom.vendor.v3`。
- `chunk_profile_id`: 例如 `rule_chunker.v1`。
- `retrieval_profile_id`: 例如 `sparse_bm25.v1`、`e5_vec0_cosine.v1`、`hybrid_rrf.v1`。
- `embed_profile_id`: 例如 `multilingual-e5-small.onnx.v1`，无 embedding 时为空。
- `index_profile_id`: 例如 `sqlite_text_scan.v1`、`sqlite_vec0_flat.v1`、`inmem_cosine.v1`。

默认解析规则：

- `eScheme != AUTO` 时，scheme 是硬约束。
- `sMemoryProfileId` 能识别时，必须与 scheme 匹配。
- 未指定 profile 时，按编译期 scheme mode 解析为默认 concrete profile。
- `ALL` 模式下，无 embedder 默认 `builtin_sparse.v1`；有 custom embedder 默认 `custom.v1`。
- `onnx_e5.v1` 必须记录 embed profile、runtime/model/tokenizer 标识。

## 5. Storage Schema v3

现有 SQLite schema 保留，但需要增加版本化字段。

新增/固化表：

- `xllm_memory_meta`: 存储 `schema_version`、`created_by_version`、默认 namespace 设置。
- `xllm_memory_record`: 记录 record 基本信息、scope、source、metadata。
- `xllm_memory_chunk`: 记录 chunk 文本、chunk id、chunk index、embedding blob。
- `xllm_memory_chunk_meta`: 记录 profile/version、hash、byte range、section path、邻接 chunk。
- `xllm_memory_lexical_term`: v3 sparse postings 表。
- `xllm_memory_vector_meta`: 记录 vector rowid 与 embed/index profile。

v3 最低必须落库的 profile 字段：

- `memory_profile_id`
- `chunk_profile_id`
- `retrieval_profile_id`
- `embed_profile_id`
- `index_profile_id`
- `profile_version`

迁移规则：

- 旧库没有这些字段时，按创建时当前 memory profile 补默认值。
- 旧库里的 embedding 只允许在 dim 与 embed profile 匹配时复用。
- 如果 profile 不匹配，搜索时必须跳过或返回明确错误，不能静默混用。

## 6. Chunk v3

当前 `xllm__chunk_text()` 可作为 v3 `rule_chunker.v1` 的起点。

v3 chunk 模块最小内部接口：

```c
typedef struct {
    const char *sText;
    uint32 uTargetChars;
    uint32 uMaxChars;
    uint32 uOverlapChars;
    xvalue tVendorExtra;
} xllm__chunk_options;

typedef struct {
    char *sChunkId;
    char *sText;
    uint32 uChunkIndex;
    uint64 uByteStart;
    uint64 uByteEnd;
    char *sSectionPath;
    char *sContentHash;
    xvalue tMetadata;
} xllm__chunk_result_item;
```

v3 先不要求复杂 parser，但要补齐：

- stable chunk id。
- content hash。
- byte/char range。
- prev/next chunk id。
- chunk profile id。

## 7. Builtin Sparse v3

`builtin_sparse.v1` 定位为无外部依赖 baseline。

v3 不再只依赖简单文本包含/粗略打分，而是引入正式 `lindex`：

- term normalization。
- char bigram/trigram。
- postings 写入 SQLite。
- BM25-like score。
- phrase boost。
- metadata boost。
- priority/recency/expiry 融入最终排序。

第一阶段可接受的实现：

- postings 存 SQLite。
- term 粒度先用 ASCII word + CJK char bigram。
- BM25 参数固定为 profile 常量。
- 不公开 lindex API，仅作为 memory 内部机制。

## 8. ONNX E5 v3

`onnx_e5.v1` 是高质量语义检索主线。

v3 必须明确：

- `embed_profile_id = multilingual-e5-small.onnx.v1`
- query/document task prefix。
- max token/char policy。
- pooling mode。
- normalize mode。
- vector dim。
- model/runtime/tokenizer asset identity。

当前 `sqlite-vec` 和 in-memory cosine fallback 都保留：

- 有 `sqlite-vec.dll` 且 vec0 表可创建时，走 vec0 candidate retrieval。
- 没有 sqlite-vec 时，从 SQLite embedding blob reload 后走 in-memory cosine。
- 两条路径的 search result 分数语义必须统一成“越高越相关”。

## 9. Hybrid v3

hybrid 不再只是 `bEnableHybridSearch` 和权重开关，而是一套 retrieval profile。

推荐 v3 profile：

- `hybrid_rrf.v1`: 使用 reciprocal rank fusion。
- `hybrid_weighted.v1`: 使用归一化后加权分数。

默认落地顺序：

1. 先实现 sparse 和 vector 分别返回候选。
2. 用 RRF 做第一版 fusion，避免不同 score 空间强行线性合并。
3. 后续再暴露 weighted fusion。

v3 默认不把 hybrid 作为 `onnx_e5.v1` 的强制行为；它应是显式 profile 或显式 option。

## 10. Conversation Memory v3

当前 conversation lifecycle 已经较完整，v3 要把语义边界写清楚。

conversation memory 写入类型：

- `fact`: 用户稳定事实。
- `preference`: 用户偏好。
- `task`: 未完成/已完成任务状态。
- `tool_state`: 工具或环境状态。
- `summary`: 压缩后的长期摘要。
- `raw_turn`: 仅在宿主明确要求时保存。

默认策略：

- 不默认把完整对话原文作为长期 memory。
- `ingest_turn_response` 默认使用 `XLLM_MEMORY_EXTRACTION_POLICY_TURN_RESPONSE` 生成可检索 conversation record；宿主可显式设置 `XLLM_MEMORY_EXTRACTION_POLICY_NONE` 跳过长期写入。
- stable identity 用于幂等更新同一 turn。
- priority、expires_at、updated_at 是排序和生命周期的一等字段。

v3 新增设计点：

- `xllm_memory_extraction_policy` 先固化 `DEFAULT` / `NONE` / `TURN_RESPONSE`，后续再扩展 summary/fact/preference/task。
- bridge 默认只做 search-before-chat；ingest-after-chat 由宿主显式开启，且继续服从 `tIngest.eExtractionPolicy`。
- session 不自动写 memory；bridge 只作为 opt-in 便利层。
- conversation record metadata 会写入 `memory_type=conversation.turn_response.v1`、`extraction_policy=turn_response`，并保留 `conversation_kind=turn_response` 兼容字段。
- `xllm_memory_remove_by_metadata` 用于按 typed metadata 批量清理或回滚某类 memory；`sMetadataValue` 为空时按 key 存在匹配。

## 11. Diagnostics v3

生产使用需要可诊断。

建议新增只读 API：

```c
XLLM_API int xllm_memory_get_diagnostics(
    const xllm_memory *pMemory,
    xllm_memory_diagnostics *pDiagnostics,
    xllm_error *pError
);
```

诊断信息至少包含：

- resolved scheme。
- memory profile。
- sqlite path。
- namespace。
- record/chunk count。
- vector table ready。
- sqlite-vec loaded。
- embedding dim。
- runtime/model/tokenizer readiness。
- last migration version。

## 12. 迁移阶段

### Phase A: 文档与边界固化

交付：

- 本 v3 文档。
- 当前实现能力与 v3 目标差距清单。
- profile/schema/version 决策冻结。

验收：

- 不改功能行为。
- release gate 通过。

### Phase B: Schema/Profile 元数据

交付：

- SQLite schema version。
- record/chunk profile fields。
- 旧库 migration。
- profile mismatch 检测。
- public list API exposes record/chunk profile metadata。

验收：

- 新旧 SQLite 库均可打开。
- 新写入记录携带 concrete profile。
- `xllm_memory_list_records` / `xllm_memory_list_chunks` 可读回 concrete profile。
- profile 不匹配不会静默参与召回。

### Phase C: Chunk 模块升级

交付：

- chunk result 携带 hash/range/adjacency。
- chunk profile 写入 SQLite。
- workspace/file ingest/sync 使用 content hash 避免同内容 churn。
- SQLite chunk metadata migration。

验收：

- `list_chunks` 可看到 content hash、byte range、previous/next chunk id。
- repeated ingest/sync 对未变文件不制造无意义 churn；即使文件 mtime 被 touch，只要内容 hash 未变也会跳过。
- list_chunks 可看到必要 chunk metadata。

### Phase D: Builtin Sparse v1

交付：

- SQLite postings 表。
- BM25-like scoring。
- phrase/metadata boost。
- sparse-only smoke。

验收：

- 无 ONNX / 无 sqlite-vec 环境仍有可用检索。已完成：builtin sparse 直接使用 SQLite postings + in-memory BM25-like scoring，不依赖 ONNX/sqlite-vec。
- 工程词、路径、文件名、错误码类查询稳定命中。已完成：exact-token BM25-like scoring、phrase boost、title/source/path metadata boost 已接入，并由 `memory_builtin_sparse` smoke 覆盖。

### Phase E: E5 Profile 固化

交付：

- embed profile 元数据写入。已完成：builtin E5 embedder 会写入 `embed_profile_id`、model/repo/runtime/model/tokenizer/prefix/pooling/normalize/token-limit/dimensions 元数据，并同步到 SQLite meta。
- vec0 / in-memory fallback 分数语义统一。进行中：两条路径均继续使用 cosine 语义；RRF/统一归一化留到 Phase F hybrid。
- asset identity 进入 diagnostics。已完成：`xllm_memory_get_diagnostics` 暴露 E5 builtin kind、profile、asset paths、model identity、query/document prefix、pooling、normalize、max tokens、dimensions。

验收：

- E5 ingest/reload/search 闭环继续通过。已通过 `memory_builtin_e5`。
- 缺 sqlite-vec 时 fallback 继续通过。已有 smoke 保留 in-memory cosine fallback。
- 有 sqlite-vec 时 vec0 path 继续通过。已通过本地 `sqlite-vec candidate retrieval` 路径。

### Phase F: Hybrid v1

交付：

- sparse/vector 双候选。已完成：hybrid 搜索同时计算 lexical BM25-like score 与 vector cosine/vec0 candidate score。
- RRF fusion。已完成：hybrid 使用 scaled RRF，并加入小幅 lexical tie-breaker，避免明显 lexical 精确命中被向量近邻打平时压掉。
- hybrid profile。已完成：ONNX E5 默认 retrieval profile 为 `onnx_e5.hybrid_rrf.v1`，custom hybrid 为 `custom.hybrid_rrf.v1`。
- retrieval diagnostics。已完成：`xllm_memory_hit` 暴露 lexical/vector/RRF score、lexical/vector rank，以及 retrieval/embed/index profile。

验收：

- lexical-only、vector-only、hybrid 三种 profile 可区分。已完成：record/chunk/hit profile 字段能区分 sparse BM25、E5 hybrid RRF 与 custom hybrid RRF。
- hybrid 不降低明显 lexical 精确命中。已由 `memory_hybrid_rrf` 与 search/apply 回归验证。

### Phase G: Bridge 与 Extraction Policy

交付：

- memory extraction policy。已完成：新增 `xllm_memory_extraction_policy`，`DEFAULT` 解析为 `TURN_RESPONSE`，显式 `NONE` 会成功返回但不创建长期 memory record。
- bridge opt-in defaults 固化。已完成：`xllm_memory_chat_bridge_options_init` 默认 `bSearchBeforeChat=true`、`bIngestAfterChat=false`；开启 post-chat ingest 时使用 `tIngest.eExtractionPolicy`。
- conversation memory 类型化。已完成：`turn_response` 写入 `memory_type=conversation.turn_response.v1`、`extraction_policy=turn_response`，同时保留 `conversation_kind=turn_response`。

验收：

- search-before-chat 可独立开启。已有 `memory_chat_bridge` 覆盖。
- ingest-after-chat 不会默认污染长期 memory。已由 `memory_extraction_policy` 覆盖默认 bridge options 和 `NONE` 跳过写入。
- typed conversation metadata 可用于检索、枚举和 chunk 审计。已由 `memory_extraction_policy` 覆盖 `memory_type` / `extraction_policy` 的 search/list/chunk metadata filter。
- typed conversation metadata 可用于批量删除。已由 `memory_remove_by_metadata` 覆盖普通 metadata、scope 限制、key-only 删除和 `memory_type=conversation.turn_response.v1` 删除。
- conversation trim/expiry/priority 继续通过。需要持续回归。

## 13. v3 首批开发任务

优先级从高到低：

1. 增加 `MEMORY_DESIGN_V3.md` 并把 README/closeout 指向 v3。
2. 增加 memory diagnostics API 草案和 smoke。已完成：`xllm_memory_get_diagnostics` 提供 resolved scheme/profile、sqlite/vector/embedder 状态和 record/chunk 计数。
3. 增加 SQLite `schema_version` / profile metadata migration。已完成：namespace 级 `schema_version=1`、`memory_profile_id`、`memory_scheme` 会写入 `xllm_memory_meta`，同一 namespace 用不同 profile 打开会失败；record/chunk 级 profile 字段会落库并通过 list API 暴露。
4. 扩展 chunk metadata，写入 content hash 与 adjacency。已完成：chunker 输出 byte range，SQLite/list API 已暴露 content hash、start/end byte、previous/next chunk id；file/workspace ingest 与 sync 会写入 record 内容 hash，并在 `bSkipUnchanged` 下跳过同内容重建。
5. 落地 builtin sparse postings + BM25 baseline。已完成：SQLite `xllm_memory_sparse_posting` 会随 chunk 持久化 token/term_count/token_count；搜索使用 BM25-like exact-token score，并补充 phrase 与 metadata boost。
6. 固化 E5 embed profile 元数据。已完成：diagnostics 与 SQLite meta 都暴露/持久化 builtin E5 asset identity；`memory_builtin_e5` 验证 diagnostics 与 DB meta。
7. 做 hybrid RRF profile。已完成：搜索使用 RRF 融合 lexical/vector rank，hit 级 retrieval diagnostics 已暴露，并由 `memory_hybrid_rrf` 覆盖。
8. 固化 bridge/extraction policy 生产默认。已完成：默认不自动 post-chat ingest，显式 `NONE` 可跳过长期写入，conversation metadata 类型化，并由 `memory_extraction_policy` 覆盖。

## 14. 不做事项

v3 第一轮不做：

- 把 `chunk/embed/lindex/vindex` 公开成独立一级对外库。
- 默认自动把 session 每轮对话写入长期 memory。
- 在 profile 不匹配时自动跨方案 fallback。
- 引入远程向量数据库作为默认依赖。
- 追求一次性完成 HNSW。

## 15. 结论

v3 的核心判断是：当前 memory 已经不是“从零开始”，而是需要把已有可用实现收敛成稳定产品边界。

先做 profile/schema/diagnostics/chunk metadata，是为了让后续 sparse、E5、hybrid 的质量升级有可验证基础。否则继续堆检索功能会让数据兼容、排障和生产部署变得不可控。
