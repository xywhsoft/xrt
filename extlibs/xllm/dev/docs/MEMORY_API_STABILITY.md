# xllm Memory Public API 稳定性审查清单

本文档用于标记 `xllm-memory.h` 当前 public API 的稳定性等级，指导后续 memory v3 继续开发时哪些接口可以冻结，哪些仍允许调整。

状态说明：

- `Stable`: 可作为下游集成主线使用。只允许向后兼容扩展。
- `Provisional`: 当前可用，但字段、默认策略或语义仍可能在 0.x 阶段收敛。
- `Experimental`: 面向内部演进或早期试用，暂不承诺稳定。

## 1. 总体原则

- `xllm_memory_*_options_init()` 是 ABI/API 扩展的主路径。新增字段必须保证 zero-init 或 init 后有安全默认值。
- public struct 只允许在尾部追加字段，不允许重排、删除或改变现有字段含义。
- enum 只允许追加新值，不允许重排旧值；`DEFAULT` / `AUTO` 语义必须保持可解释。
- result/list/search 对象必须继续提供 reset/free 类 API，调用者不应依赖内部分配细节。
- `session` 仍不依赖 `memory`；`memory bridge` 仍是 opt-in convenience layer。
- 默认行为必须保守：不默认写长期 conversation memory，不默认索引敏感文件，不静默混用 profile/schema。

## 2. Stable API

这些接口已经可作为 AI IDE / claw 集成主线使用。

### 2.1 生命周期与配置

- `xllm_memory_options_init`
- `xllm_memory_create`
- `xllm_memory_destroy`
- `xllm_memory_get_scheme`
- `xllm_memory_get_profile_id`
- `xllm_memory_record_count`
- `xllm_memory_chunk_count`

稳定约束：

- `xllm_memory_create` 必须继续通过 options 解析 scheme/profile/storage。
- 同一 namespace/profile mismatch 不允许静默混用。
- `record_count` / `chunk_count` 语义保持按 scope 计数。
- profile migration / rollback 策略见 `docs\MEMORY_PROFILE_MIGRATION.md`。

### 2.2 基础 ingest / search / list

- `xllm_memory_ingest_options_init`
- `xllm_memory_ingest_text`
- `xllm_memory_search_options_init`
- `xllm_memory_search`
- `xllm_memory_list_options_init`
- `xllm_memory_list_records`
- `xllm_memory_list_chunks`
- `xllm_memory_search_result_reset`
- `xllm_memory_record_list_result_reset`
- `xllm_memory_chunk_list_result_reset`

稳定约束：

- metadata/source/scope filters 必须保持向后兼容。
- `xllm_memory_list_records` 和 `xllm_memory_list_chunks` 暴露的 profile/hash/range 字段不应退化。
- search score 允许随 retrieval profile 优化，但“越高越相关”的方向不能改变。

### 2.3 Context apply

- `xllm_memory_context_options_init`
- `xllm_memory_turn_search_apply_options_init`
- `xllm_memory_apply_search_to_request`
- `xllm_memory_apply_search_to_turn`
- `xllm_memory_search_and_apply_to_request`
- `xllm_memory_search_and_apply_to_turn`
- `xllm_memory_search_and_apply_from_turn_to_request`
- `xllm_memory_search_and_apply_from_turn_to_turn`

稳定约束：

- apply API 必须保持不接管 session 生命周期。
- context budget、distinct、min score 语义应保持兼容。
- 后续 citation/source rendering 应通过 options 扩展，而不是破坏现有结果。

### 2.4 删除、清理和生命周期

- `xllm_memory_remove`
- `xllm_memory_remove_by_source_uri`
- `xllm_memory_remove_by_conversation`
- `xllm_memory_remove_by_metadata`
- `xllm_memory_remove_expired`
- `xllm_memory_trim_conversation`
- `xllm_memory_remove_expired_options_init`
- `xllm_memory_remove_by_metadata_options_init`
- `xllm_memory_trim_conversation_options_init`

稳定约束：

- remove 类 API 必须返回 removed count。
- metadata key-only 匹配只代表 key 存在，缺失 key 不得误判为命中。
- conversation trim/expiry/priority 继续作为长期 memory 生命周期基础能力。

### 2.5 Diagnostics

- `xllm_memory_diagnostics_init`
- `xllm_memory_get_diagnostics`

稳定约束：

- diagnostics 只读，不改变 memory 状态。
- 已有 scheme/profile/storage/vector/embedder 字段保持可用。
- 可在尾部追加 DB health、retrieval debug、asset version 等字段。

## 3. Provisional API

这些接口可用，但后续仍可能收敛字段、默认值或策略。

### 3.1 Conversation memory

- `xllm_memory_ingest_turn_response_options_init`
- `xllm_memory_ingest_turn_response`
- `xllm_memory_extraction_policy`

当前约束：

- `DEFAULT` 解析为 `TURN_RESPONSE`。
- `NONE` 必须跳过长期写入并返回成功。
- `TURN_RESPONSE` 写入 typed metadata：`memory_type=conversation.turn_response.v1`、`extraction_policy=turn_response`。

仍可调整：

- 新增 `SUMMARY`、`TASK`、`FACT`、`PREFERENCE` 等 extraction policy。
- 新增 typed metadata 字段。
- 新增宿主确认/审核相关 options。

不允许破坏：

- 不得改成默认保存完整原始对话。
- 不得让 bridge 默认 post-chat ingest。

### 3.2 File / workspace ingest and sync

- `xllm_memory_ingest_file_options_init`
- `xllm_memory_ingest_directory_options_init`
- `xllm_memory_ingest_workspace_options_init`
- `xllm_memory_sync_file_options_init`
- `xllm_memory_sync_files_options_init`
- `xllm_memory_sync_file_events_options_init`
- `xllm_memory_ingest_file`
- `xllm_memory_ingest_directory`
- `xllm_memory_ingest_workspace`
- `xllm_memory_sync_file`
- `xllm_memory_sync_files`
- `xllm_memory_sync_file_events`
- `xllm_memory_sync_workspace`
- `xllm_memory_ingest_directory_result_*`
- `xllm_memory_sync_workspace_result_*`
- `xllm_memory_change_set_*`

当前约束：

- `bSkipUnchanged` 继续基于 content hash 避免同内容 churn。
- source uri、metadata、scope 继续可过滤和可删除。

仍可调整：

- 默认 ignore/sensitive patterns。
- progress callback。
- workspace status/diagnostics。
- language-aware chunking。

### 3.3 Builtin embedder

- `xllm_memory_builtin_embedder_options_init`
- `xllm_memory_builtin_embedder_probe_reset`
- `xllm_memory_probe_builtin_embedder`
- `xllm_memory_make_builtin_embedder`
- `xllm_memory_embedder_init`
- `xllm_memory_embedder_reset`

当前约束：

- E5 asset identity 必须继续进入 diagnostics/SQLite meta。
- sqlite-vec 缺失时 fallback 仍应可用。

仍可调整：

- asset discovery。
- model family/profile。
- runtime loading diagnostics。

## 4. Experimental API

这些接口当前服务于 watcher/worker 和持续索引，仍需要长跑验证。

### 4.1 File event queue

- `xllm_memory_file_event_queue_*`

风险：

- queue compaction、rename/update/delete 顺序和 crash recovery 还需要更强验证。

### 4.2 Watcher bridge / pump / worker

- `xllm_memory_watcher_bridge_*`
- `xllm_memory_watcher_pump_*`
- `xllm_memory_watcher_worker_*`

风险：

- long-run worker、并发 ingest/search、DB reopen、crash/restart 尚未形成稳定生产基线。

稳定前要求：

- 增加长跑 smoke。
- 增加 crash/restart/reopen 验证。
- 增加 busy timeout / WAL / transaction 策略审查。

## 5. API Freeze Policy

0.x 阶段允许继续演进，但按以下规则控制破坏性：

- `Stable` API 不做源级破坏性变更。
- `Provisional` API 可追加字段和值，但变更默认行为前必须更新 spec、README 和 smoke。
- `Experimental` API 可调整，但必须保留迁移说明。
- 所有 public header 变更必须跑 `build.bat singlehead`。
- memory public API 行为变更必须至少跑相关 memory smoke subset。
- release 前必须跑 `build.bat release-gate`。

## 6. 当前未冻结事项

这些能力明确还未冻结：

- conversation `SUMMARY` / `TASK` / `FACT` / `PREFERENCE` extraction policy。
- workspace sensitive file 默认排除策略。
- progress callback 和 workspace index status API。
- retrieval eval / debug dump API。
- DB health check API。
- watcher/worker 长跑和 crash recovery 语义。
- semantic versioning 的正式 1.0 freeze 边界。

## 7. 后续审查清单

新增或修改 memory public API 前检查：

- [ ] 是否能通过现有 options struct 追加字段实现。
- [ ] 是否需要新增 smoke case。
- [ ] 是否会改变默认长期记忆写入策略。
- [ ] 是否会改变 search/list/remove 的 filter 语义。
- [ ] 是否会改变 profile/schema mismatch 处理。
- [ ] 是否需要同步 diagnostics。
- [ ] 是否需要更新 `XLLM_AGENT_INFRA_SPEC.md`。
- [ ] 是否需要更新 README quickstart 或 architecture 文档。
