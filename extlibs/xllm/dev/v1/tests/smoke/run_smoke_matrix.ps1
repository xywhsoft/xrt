param(
    [string]$OutputDir,
    [string]$Filter,
    [string]$StartAt,
    [switch]$ListCases
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptRoot "..\..")).Path
$smokeScriptRoot = Join-Path $scriptRoot "scripts"
Set-Location $repoRoot

$outputDir = $OutputDir
if ([string]::IsNullOrWhiteSpace($outputDir)) {
    $outputDir = $env:XLLM_SMOKE_OUTPUT_DIR
}
if ([string]::IsNullOrWhiteSpace($outputDir)) {
    $outputDir = Join-Path $repoRoot "build\smoke_runs"
} elseif (-not [System.IO.Path]::IsPathRooted($outputDir)) {
    $outputDir = Join-Path $repoRoot $outputDir
}

$filterRaw = $Filter
if ([string]::IsNullOrWhiteSpace($filterRaw)) {
    $filterRaw = $env:XLLM_SMOKE_FILTER
}
$filters = @()
if (-not [string]::IsNullOrWhiteSpace($filterRaw)) {
    $filters = $filterRaw.Split(",") | ForEach-Object { $_.Trim().ToLowerInvariant() } | Where-Object { $_ -ne "" }
}

$startAtRaw = $StartAt
if ([string]::IsNullOrWhiteSpace($startAtRaw)) {
    $startAtRaw = $env:XLLM_SMOKE_START_AT
}
$startAt = $null
if (-not [string]::IsNullOrWhiteSpace($startAtRaw)) {
    $startAt = $startAtRaw.Trim().ToLowerInvariant()
}

$cases = @(
    @{ Name = "public_error_ex"; Group = "core"; Script = "build_smoke_public_error_ex_example.bat" },
    @{ Name = "log_event_taxonomy"; Group = "core"; Script = "build_smoke_log_event_taxonomy_example.bat" },
    @{ Name = "resource_cleanup"; Group = "core"; Script = "build_smoke_resource_cleanup_example.bat" },
    @{ Name = "auto_tool_loop"; Group = "core"; Script = "build_smoke_auto_tool_loop_example.bat" },
    @{ Name = "session_summary"; Group = "session"; Script = "build_smoke_session_summary_example.bat" },
    @{ Name = "session_tool_compact_regression"; Group = "session"; Script = "build_smoke_session_tool_compact_regression_example.bat" },
    @{ Name = "session_state_options"; Group = "session"; Script = "build_smoke_session_state_options_example.bat" },
    @{ Name = "openai_basic"; Group = "openai"; Script = "build_smoke_openai_compat_example.bat" },
    @{ Name = "openai_stream_retry"; Group = "openai"; Script = "build_smoke_openai_compat_stream_retry_example.bat" },
    @{ Name = "openai_session_stream_output_artifacts"; Group = "openai"; Script = "build_smoke_openai_compat_session_stream_output_artifacts_example.bat" },
    @{ Name = "openai_proxy_http"; Group = "proxy"; Script = "build_smoke_openai_compat_proxy_http_connect_example.bat" },
    @{ Name = "openai_proxy_socks5"; Group = "proxy"; Script = "build_smoke_openai_compat_proxy_socks5_example.bat" },
    @{ Name = "anthropic_basic"; Group = "anthropic"; Script = "build_smoke_anthropic_native_example.bat" },
    @{ Name = "anthropic_stream_tool_loop"; Group = "anthropic"; Script = "build_smoke_anthropic_native_stream_tool_loop_example.bat" },
    @{ Name = "anthropic_session_thinking"; Group = "anthropic"; Script = "build_smoke_anthropic_native_session_thinking_continuity_example.bat" },
    @{ Name = "anthropic_stream_output_artifacts"; Group = "anthropic"; Script = "build_smoke_anthropic_native_stream_output_artifacts_example.bat" },
    @{ Name = "ollama_basic"; Group = "ollama"; Script = "build_smoke_ollama_native_example.bat" },
    @{ Name = "ollama_stream_tool_loop"; Group = "ollama"; Script = "build_smoke_ollama_native_stream_tool_loop_example.bat" },
    @{ Name = "ollama_session_stream_output_artifacts"; Group = "ollama"; Script = "build_smoke_ollama_native_session_stream_output_artifacts_example.bat" },
    @{ Name = "glm_examples_build"; Group = "examples"; Script = "examples\\glm\\build.bat" },
    @{ Name = "minimax_examples_build"; Group = "examples"; Script = "examples\\minimax\\build.bat" },
    @{ Name = "kimi_examples_build"; Group = "examples"; Script = "examples\\kimi\\build.bat" },
    @{ Name = "qwen_examples_build"; Group = "examples"; Script = "examples\\qwen\\build.bat" },
    @{ Name = "doubao_examples_build"; Group = "examples"; Script = "examples\\doubao\\build.bat" },
    @{ Name = "gemini_examples_build"; Group = "examples"; Script = "examples\\gemini\\build.bat" },
    @{ Name = "memory_examples_build"; Group = "examples"; Script = "examples\\memory\\build.bat" },
    @{ Name = "memory_ingest_directory"; Group = "examples"; Script = "build_smoke_memory_ingest_directory_example.bat" },
    @{ Name = "memory_ingest_workspace"; Group = "examples"; Script = "build_smoke_memory_ingest_workspace_example.bat" },
    @{ Name = "memory_ingest_workspace_ignore_patterns"; Group = "examples"; Script = "build_smoke_memory_ingest_workspace_ignore_patterns_example.bat" },
    @{ Name = "memory_ingest_progress"; Group = "examples"; Script = "build_smoke_memory_ingest_progress_example.bat" },
    @{ Name = "memory_workspace_gitignore"; Group = "examples"; Script = "build_smoke_memory_workspace_gitignore_example.bat" },
    @{ Name = "memory_workspace_nested_gitignore"; Group = "examples"; Script = "build_smoke_memory_workspace_nested_gitignore_example.bat" },
    @{ Name = "memory_sync_file"; Group = "examples"; Script = "build_smoke_memory_sync_file_example.bat" },
    @{ Name = "memory_sync_file_threaded"; Group = "examples"; Script = "build_smoke_memory_sync_file_threaded_example.bat" },
    @{ Name = "memory_sync_files"; Group = "examples"; Script = "build_smoke_memory_sync_files_example.bat" },
    @{ Name = "memory_sync_file_events"; Group = "examples"; Script = "build_smoke_memory_sync_file_events_example.bat" },
    @{ Name = "memory_file_event_queue"; Group = "examples"; Script = "build_smoke_memory_file_event_queue_example.bat" },
    @{ Name = "memory_watcher_compact_pending"; Group = "examples"; Script = "build_smoke_memory_watcher_compact_pending_example.bat" },
    @{ Name = "memory_watcher_bridge"; Group = "examples"; Script = "build_smoke_memory_watcher_bridge_example.bat" },
    @{ Name = "memory_watcher_pump"; Group = "examples"; Script = "build_smoke_memory_watcher_pump_example.bat" },
    @{ Name = "memory_watcher_worker_state"; Group = "examples"; Script = "build_smoke_memory_watcher_worker_state_example.bat" },
    @{ Name = "memory_watcher_worker"; Group = "examples"; Script = "build_smoke_memory_watcher_worker_example.bat" },
    @{ Name = "memory_watcher_worker_run_ready"; Group = "examples"; Script = "build_smoke_memory_watcher_worker_run_ready_example.bat" },
    @{ Name = "memory_watcher_worker_run_loop"; Group = "examples"; Script = "build_smoke_memory_watcher_worker_run_loop_example.bat" },
    @{ Name = "memory_sync_workspace"; Group = "examples"; Script = "build_smoke_memory_sync_workspace_example.bat" },
    @{ Name = "memory_change_set"; Group = "examples"; Script = "build_smoke_memory_change_set_example.bat" },
    @{ Name = "memory_list_filters"; Group = "examples"; Script = "build_smoke_memory_list_filters_example.bat" },
    @{ Name = "memory_exact_source_uri"; Group = "examples"; Script = "build_smoke_memory_exact_source_uri_example.bat" },
    @{ Name = "memory_remove_by_source_uri"; Group = "examples"; Script = "build_smoke_memory_remove_by_source_uri_example.bat" },
    @{ Name = "memory_remove_by_conversation"; Group = "examples"; Script = "build_smoke_memory_remove_by_conversation_example.bat" },
    @{ Name = "memory_remove_by_metadata"; Group = "examples"; Script = "build_smoke_memory_remove_by_metadata_example.bat" },
    @{ Name = "memory_long_run"; Group = "examples"; Script = "build_smoke_memory_long_run_example.bat" },
    @{ Name = "memory_error_consistency"; Group = "examples"; Script = "build_smoke_memory_error_consistency_example.bat" },
    @{ Name = "memory_metadata_filters"; Group = "examples"; Script = "build_smoke_memory_metadata_filters_example.bat" },
    @{ Name = "memory_workspace_defaults"; Group = "examples"; Script = "build_smoke_memory_workspace_defaults_example.bat" },
    @{ Name = "memory_workspace_sensitive_defaults"; Group = "examples"; Script = "build_smoke_memory_workspace_sensitive_defaults_example.bat" },
    @{ Name = "memory_workspace_status"; Group = "examples"; Script = "build_smoke_memory_workspace_status_example.bat" },
    @{ Name = "memory_workspace_incremental"; Group = "examples"; Script = "build_smoke_memory_workspace_incremental_example.bat" },
    @{ Name = "memory_workspace_long_run"; Group = "examples"; Script = "build_smoke_memory_workspace_long_run_example.bat" },
    @{ Name = "memory_watcher_reopen"; Group = "examples"; Script = "build_smoke_memory_watcher_reopen_example.bat" },
    @{ Name = "sqlite_vec_probe"; Group = "examples"; Script = "build_smoke_sqlite_vec_probe_example.bat" },
    @{ Name = "memory_sqlite_policy"; Group = "examples"; Script = "build_smoke_memory_sqlite_policy_example.bat" },
    @{ Name = "memory_crash_consistency"; Group = "examples"; Script = "build_smoke_memory_crash_consistency_example.bat" },
    @{ Name = "memory_health_check"; Group = "examples"; Script = "build_smoke_memory_health_check_example.bat" },
    @{ Name = "memory_retrieval_debug"; Group = "examples"; Script = "build_smoke_memory_retrieval_debug_example.bat" },
    @{ Name = "memory_builtin_sparse"; Group = "examples"; Script = "build_smoke_memory_builtin_sparse_example.bat" },
    @{ Name = "memory_builtin_e5"; Group = "examples"; Script = "build_smoke_memory_builtin_e5_example.bat" },
    @{ Name = "memory_hybrid_rrf"; Group = "examples"; Script = "build_smoke_memory_hybrid_rrf_example.bat" },
    @{ Name = "memory_search_apply_turn"; Group = "examples"; Script = "build_smoke_memory_search_apply_turn_example.bat" },
    @{ Name = "memory_search_apply_from_turn"; Group = "examples"; Script = "build_smoke_memory_search_apply_from_turn_example.bat" },
    @{ Name = "memory_chat_bridge"; Group = "examples"; Script = "build_smoke_memory_chat_bridge_example.bat" },
    @{ Name = "memory_extraction_policy"; Group = "examples"; Script = "build_smoke_memory_extraction_policy_example.bat" },
    @{ Name = "memory_search_apply_budget"; Group = "examples"; Script = "build_smoke_memory_search_apply_budget_example.bat" },
    @{ Name = "memory_search_apply_distinct"; Group = "examples"; Script = "build_smoke_memory_search_apply_distinct_example.bat" },
    @{ Name = "memory_search_apply_min_score"; Group = "examples"; Script = "build_smoke_memory_search_apply_min_score_example.bat" },
    @{ Name = "agent_loop_example"; Group = "examples"; Script = "build_smoke_agent_loop_example.bat" },
    @{ Name = "ai_ide_memory_example"; Group = "examples"; Script = "build_smoke_ai_ide_memory_example.bat" },
    @{ Name = "conversation_memory_example"; Group = "examples"; Script = "build_smoke_conversation_memory_example.bat" },
    @{ Name = "task_memory_example"; Group = "examples"; Script = "build_smoke_task_memory_example.bat" },
    @{ Name = "memory_ingest_turn_response"; Group = "examples"; Script = "build_smoke_memory_ingest_turn_response_example.bat" },
    @{ Name = "memory_ingest_summary"; Group = "examples"; Script = "build_smoke_memory_ingest_summary_example.bat" },
    @{ Name = "memory_ingest_turn_response_stable"; Group = "examples"; Script = "build_smoke_memory_ingest_turn_response_stable_example.bat" },
    @{ Name = "memory_task_type"; Group = "examples"; Script = "build_smoke_memory_task_type_example.bat" },
    @{ Name = "memory_fact_preference_type"; Group = "examples"; Script = "build_smoke_memory_fact_preference_type_example.bat" },
    @{ Name = "memory_compact_conversation"; Group = "examples"; Script = "build_smoke_memory_compact_conversation_example.bat" },
    @{ Name = "memory_concurrent_ingest_search"; Group = "examples"; Script = "build_smoke_memory_concurrent_ingest_search_example.bat" },
    @{ Name = "memory_conversation_filters"; Group = "examples"; Script = "build_smoke_memory_conversation_filters_example.bat" },
    @{ Name = "memory_remove_expired"; Group = "examples"; Script = "build_smoke_memory_remove_expired_example.bat" },
    @{ Name = "memory_priority_search"; Group = "examples"; Script = "build_smoke_memory_priority_search_example.bat" },
    @{ Name = "memory_recency_search"; Group = "examples"; Script = "build_smoke_memory_recency_search_example.bat" },
    @{ Name = "memory_list_recency_sort"; Group = "examples"; Script = "build_smoke_memory_list_recency_sort_example.bat" },
    @{ Name = "memory_trim_conversation"; Group = "examples"; Script = "build_smoke_memory_trim_conversation_example.bat" },
    @{ Name = "memory_trim_conversation_priority"; Group = "examples"; Script = "build_smoke_memory_trim_conversation_priority_example.bat" },
    @{ Name = "memory_trim_conversation_budget"; Group = "examples"; Script = "build_smoke_memory_trim_conversation_budget_example.bat" },
    @{ Name = "memory_search_skip_expired"; Group = "examples"; Script = "build_smoke_memory_search_skip_expired_example.bat" },
    @{ Name = "memory_list_skip_expired"; Group = "examples"; Script = "build_smoke_memory_list_skip_expired_example.bat" },
    @{ Name = "memory_scheme_mode"; Group = "examples"; Script = "build_smoke_memory_scheme_mode_example.bat" },
    @{ Name = "memory_scheme_mode_custom"; Group = "examples"; Script = "build_smoke_memory_scheme_mode_custom_example.bat" },
    @{ Name = "memory_diagnostics"; Group = "examples"; Script = "build_smoke_memory_diagnostics_example.bat" },
    @{ Name = "azure_openai_examples_build"; Group = "examples"; Script = "build_smoke_azure_openai_examples_example.bat" },
    @{ Name = "probe_proxy_preflight"; Group = "probe"; Script = "build_smoke_real_provider_probe_matrix_proxy_preflight_only_example.bat" },
    @{ Name = "probe_openai_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_openai_alias_envs_example.bat" },
    @{ Name = "probe_openai_api_key_header_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_openai_api_key_header_alias_envs_example.bat" },
    @{ Name = "probe_azure_openai_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_azure_openai_alias_envs_example.bat" },
    @{ Name = "probe_azure_openai_auth_none_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_azure_openai_auth_none_alias_envs_example.bat" },
    @{ Name = "probe_openai_tool_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_openai_tool_alias_envs_example.bat" },
    @{ Name = "probe_azure_openai_tool_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_azure_openai_tool_alias_envs_example.bat" },
    @{ Name = "probe_openai_tool_result_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_openai_tool_result_alias_envs_example.bat" },
    @{ Name = "probe_azure_openai_tool_result_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_azure_openai_tool_result_alias_envs_example.bat" },
    @{ Name = "probe_openai_multimodal_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_openai_multimodal_alias_envs_example.bat" },
    @{ Name = "probe_azure_openai_multimodal_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_azure_openai_multimodal_alias_envs_example.bat" },
    @{ Name = "probe_azure_openai_image_inline_bytes"; Group = "probe"; Script = "build_smoke_real_provider_probe_azure_openai_image_inline_bytes_example.bat" },
    @{ Name = "probe_azure_openai_file_url"; Group = "probe"; Script = "build_smoke_real_provider_probe_azure_openai_file_url_example.bat" },
    @{ Name = "probe_azure_openai_file_inline_bytes"; Group = "probe"; Script = "build_smoke_real_provider_probe_azure_openai_file_inline_bytes_example.bat" },
    @{ Name = "probe_azure_openai_file_file_id"; Group = "probe"; Script = "build_smoke_real_provider_probe_azure_openai_file_file_id_example.bat" },
    @{ Name = "probe_openai_file_file_id"; Group = "probe"; Script = "build_smoke_real_provider_probe_openai_file_file_id_example.bat" },
    @{ Name = "probe_openai_file_inline_bytes"; Group = "probe"; Script = "build_smoke_real_provider_probe_openai_file_inline_bytes_example.bat" },
    @{ Name = "probe_openai_file_url"; Group = "probe"; Script = "build_smoke_real_provider_probe_openai_file_url_example.bat" },
    @{ Name = "probe_openai_image_file_id"; Group = "probe"; Script = "build_smoke_real_provider_probe_openai_image_file_id_example.bat" },
    @{ Name = "probe_azure_openai_image_file_id"; Group = "probe"; Script = "build_smoke_real_provider_probe_azure_openai_image_file_id_example.bat" },
    @{ Name = "probe_anthropic_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_anthropic_alias_envs_example.bat" },
    @{ Name = "probe_anthropic_multimodal_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_anthropic_multimodal_alias_envs_example.bat" },
    @{ Name = "probe_anthropic_tool_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_anthropic_tool_alias_envs_example.bat" },
    @{ Name = "probe_anthropic_tool_result_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_anthropic_tool_result_alias_envs_example.bat" },
    @{ Name = "probe_anthropic_json_schema_unsupported"; Group = "probe"; Script = "build_smoke_real_provider_probe_anthropic_json_schema_unsupported_example.bat" },
    @{ Name = "probe_anthropic_image_file_id"; Group = "probe"; Script = "build_smoke_real_provider_probe_anthropic_image_file_id_example.bat" },
    @{ Name = "probe_anthropic_document_file_id"; Group = "probe"; Script = "build_smoke_real_provider_probe_anthropic_document_file_id_example.bat" },
    @{ Name = "probe_ollama_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_ollama_alias_envs_example.bat" },
    @{ Name = "probe_ollama_tool_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_ollama_tool_alias_envs_example.bat" },
    @{ Name = "probe_ollama_tool_choice_required_unsupported"; Group = "probe"; Script = "build_smoke_real_provider_probe_ollama_tool_choice_required_unsupported_example.bat" },
    @{ Name = "probe_ollama_multimodal_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_ollama_multimodal_alias_envs_example.bat" },
    @{ Name = "probe_ollama_file_alias_envs_unsupported"; Group = "probe"; Script = "build_smoke_real_provider_probe_ollama_file_alias_envs_unsupported_example.bat" },
    @{ Name = "probe_ollama_image_file_id_alias_envs_unsupported"; Group = "probe"; Script = "build_smoke_real_provider_probe_ollama_image_file_id_alias_envs_unsupported_example.bat" },
    @{ Name = "probe_glm_tool_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_glm_tool_alias_envs_example.bat" },
    @{ Name = "probe_glm_multimodal_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_glm_multimodal_alias_envs_example.bat" },
    @{ Name = "probe_glm_file_alias_envs_unsupported"; Group = "probe"; Script = "build_smoke_real_provider_probe_glm_file_alias_envs_unsupported_example.bat" },
    @{ Name = "probe_glm_image_file_id_alias_envs_unsupported"; Group = "probe"; Script = "build_smoke_real_provider_probe_glm_image_file_id_alias_envs_unsupported_example.bat" },
    @{ Name = "probe_minimax_tool_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_minimax_tool_alias_envs_example.bat" },
    @{ Name = "probe_minimax_multimodal_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_minimax_multimodal_alias_envs_example.bat" },
    @{ Name = "probe_minimax_file_alias_envs_unsupported"; Group = "probe"; Script = "build_smoke_real_provider_probe_minimax_file_alias_envs_unsupported_example.bat" },
    @{ Name = "probe_minimax_image_file_id_alias_envs_unsupported"; Group = "probe"; Script = "build_smoke_real_provider_probe_minimax_image_file_id_alias_envs_unsupported_example.bat" },
    @{ Name = "probe_kimi_tool_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_kimi_tool_alias_envs_example.bat" },
    @{ Name = "probe_kimi_multimodal_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_kimi_multimodal_alias_envs_example.bat" },
    @{ Name = "probe_kimi_file_alias_envs_unsupported"; Group = "probe"; Script = "build_smoke_real_provider_probe_kimi_file_alias_envs_unsupported_example.bat" },
    @{ Name = "probe_kimi_image_file_id_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_kimi_image_file_id_alias_envs_example.bat" },
    @{ Name = "probe_qwen_tool_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_qwen_tool_alias_envs_example.bat" },
    @{ Name = "probe_qwen_tool_result_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_qwen_tool_result_alias_envs_example.bat" },
    @{ Name = "probe_qwen_named_tool_with_thinking_unsupported"; Group = "probe"; Script = "build_smoke_real_provider_probe_qwen_named_tool_with_thinking_unsupported_example.bat" },
    @{ Name = "probe_qwen_multimodal_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_qwen_multimodal_alias_envs_example.bat" },
    @{ Name = "probe_qwen_file_alias_envs_unsupported"; Group = "probe"; Script = "build_smoke_real_provider_probe_qwen_file_alias_envs_unsupported_example.bat" },
    @{ Name = "probe_qwen_image_file_id_alias_envs_unsupported"; Group = "probe"; Script = "build_smoke_real_provider_probe_qwen_image_file_id_alias_envs_unsupported_example.bat" },
    @{ Name = "probe_doubao_tool_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_doubao_tool_alias_envs_example.bat" },
    @{ Name = "probe_doubao_tool_result_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_doubao_tool_result_alias_envs_example.bat" },
    @{ Name = "probe_doubao_multimodal_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_doubao_multimodal_alias_envs_example.bat" },
    @{ Name = "probe_doubao_file_alias_envs_unsupported"; Group = "probe"; Script = "build_smoke_real_provider_probe_doubao_file_alias_envs_unsupported_example.bat" },
    @{ Name = "probe_doubao_image_file_id_alias_envs_unsupported"; Group = "probe"; Script = "build_smoke_real_provider_probe_doubao_image_file_id_alias_envs_unsupported_example.bat" },
    @{ Name = "probe_gemini_tool_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_gemini_tool_alias_envs_example.bat" },
    @{ Name = "probe_gemini_tool_result_image_unsupported"; Group = "probe"; Script = "build_smoke_real_provider_probe_gemini_tool_result_image_unsupported_example.bat" },
    @{ Name = "probe_gemini_multimodal_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_gemini_multimodal_alias_envs_example.bat" },
    @{ Name = "probe_gemini_file_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_gemini_file_alias_envs_example.bat" },
    @{ Name = "probe_gemini_image_file_id_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_gemini_image_file_id_alias_envs_example.bat" },
    @{ Name = "probe_vertex_gemini_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_vertex_gemini_alias_envs_example.bat" },
    @{ Name = "probe_vertex_gemini_tool_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_vertex_gemini_tool_alias_envs_example.bat" },
    @{ Name = "probe_vertex_gemini_tool_result_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_vertex_gemini_tool_result_alias_envs_example.bat" },
    @{ Name = "probe_vertex_gemini_tool_result_image_unsupported"; Group = "probe"; Script = "build_smoke_real_provider_probe_vertex_gemini_tool_result_image_unsupported_example.bat" },
    @{ Name = "probe_vertex_gemini_multimodal_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_vertex_gemini_multimodal_alias_envs_example.bat" },
    @{ Name = "probe_vertex_gemini_file_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_vertex_gemini_file_alias_envs_example.bat" },
    @{ Name = "probe_vertex_gemini_image_file_id_alias_envs"; Group = "probe"; Script = "build_smoke_real_provider_probe_vertex_gemini_image_file_id_alias_envs_example.bat" }
)

function Test-CaseSelected {
    param(
        [hashtable]$Case,
        [string[]]$SelectedFilters
    )

    if ($SelectedFilters.Count -eq 0) {
        return $true
    }

    $name = $Case.Name.ToLowerInvariant()
    $group = $Case.Group.ToLowerInvariant()
    foreach ($item in $SelectedFilters) {
        if ($item -eq $name -or $item -eq $group) {
            return $true
        }
    }
    return $false
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
$logDir = Join-Path $outputDir "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$selectedCases = @($cases | Where-Object { Test-CaseSelected $_ $filters })

if (-not [string]::IsNullOrWhiteSpace($startAt)) {
    $startIndex = -1
    for ($i = 0; $i -lt $selectedCases.Count; $i += 1) {
        if ($selectedCases[$i].Name.ToLowerInvariant() -eq $startAt) {
            $startIndex = $i
            break
        }
    }

    if ($startIndex -lt 0) {
        $report = [ordered]@{
            success = $false
            output_dir = $outputDir
            case_filter = $filterRaw
            start_at = $startAtRaw
            case_count = 0
            failed_count = 0
            executed_case_count = 0
            generated_at = (Get-Date).ToString("s")
            error = "start_at case not found in selected smoke cases"
            results = @()
        }
        $jsonPath = Join-Path $outputDir "smoke_matrix_report.json"
        $txtPath = Join-Path $outputDir "smoke_matrix_report.txt"
        $report | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 $jsonPath
        "start_at case not found in selected smoke cases" | Set-Content -Encoding UTF8 $txtPath
        Write-Error ("start_at case not found: {0}" -f $startAtRaw)
    }

    $selectedCases = @($selectedCases[$startIndex..($selectedCases.Count - 1)])
}

if ($ListCases) {
    foreach ($case in $selectedCases) {
        $scriptPath = $case.Script
        if ($scriptPath -like "build_smoke_*.bat") {
            $scriptPath = Join-Path "tests\smoke\scripts" $scriptPath
        }
        Write-Host ("{0}`t{1}`t{2}" -f $case.Group, $case.Name, $scriptPath)
    }
    exit 0
}

$results = New-Object System.Collections.Generic.List[object]
$failed = 0
$executed = 0
$startTime = Get-Date

if ($selectedCases.Count -eq 0) {
    $report = [ordered]@{
        success = $false
        output_dir = $outputDir
        case_filter = $filterRaw
        start_at = $startAtRaw
        case_count = 0
        failed_count = 0
        executed_case_count = 0
        generated_at = (Get-Date).ToString("s")
        error = "no smoke cases selected"
        results = @()
    }
    $jsonPath = Join-Path $outputDir "smoke_matrix_report.json"
    $txtPath = Join-Path $outputDir "smoke_matrix_report.txt"
    $report | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 $jsonPath
    "no smoke cases selected" | Set-Content -Encoding UTF8 $txtPath
    Write-Error "no smoke cases selected"
}

foreach ($case in $selectedCases) {
    $executed += 1
    $logPath = Join-Path $logDir ($case.Name + ".log")
    $caseStart = Get-Date
    $scriptPath = $case.Script
    if ($scriptPath -like "build_smoke_*.bat") {
        $scriptPath = Join-Path $smokeScriptRoot $scriptPath
    } elseif (-not [System.IO.Path]::IsPathRooted($scriptPath)) {
        $scriptPath = Join-Path $repoRoot $scriptPath
    }
    $cmdLine = ('"{0}" > "{1}" 2>&1' -f $scriptPath, $logPath)
    & cmd.exe /c $cmdLine
    $exitCode = $LASTEXITCODE
    $duration = [int][Math]::Round(((Get-Date) - $caseStart).TotalMilliseconds)
    $status = if ($exitCode -eq 0) { "passed" } else { "failed" }
    if ($exitCode -ne 0) {
        $failed += 1
    }
    $results.Add([ordered]@{
        name = $case.Name
        group = $case.Group
        script = $scriptPath
        status = $status
        rc = $exitCode
        duration_ms = $duration
        log = $logPath
    }) | Out-Null
    Write-Host ("[{0}] {1} ({2} ms)" -f $status.ToUpperInvariant(), $case.Name, $duration)
}

$report = [ordered]@{
    success = ($failed -eq 0)
    output_dir = $outputDir
    case_filter = $filterRaw
    start_at = $startAtRaw
    case_count = $selectedCases.Count
    failed_count = $failed
    executed_case_count = $executed
    generated_at = (Get-Date).ToString("s")
    total_duration_ms = [int][Math]::Round(((Get-Date) - $startTime).TotalMilliseconds)
    results = $results
}

$jsonPath = Join-Path $outputDir "smoke_matrix_report.json"
$txtPath = Join-Path $outputDir "smoke_matrix_report.txt"
$report | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 $jsonPath

$txt = New-Object System.Text.StringBuilder
[void]$txt.AppendLine(("success: {0}" -f $report.success))
[void]$txt.AppendLine(("output_dir: {0}" -f $report.output_dir))
[void]$txt.AppendLine(("case_filter: {0}" -f $report.case_filter))
[void]$txt.AppendLine(("start_at: {0}" -f $report.start_at))
[void]$txt.AppendLine(("case_count: {0}" -f $report.case_count))
[void]$txt.AppendLine(("failed_count: {0}" -f $report.failed_count))
[void]$txt.AppendLine(("executed_case_count: {0}" -f $report.executed_case_count))
[void]$txt.AppendLine(("generated_at: {0}" -f $report.generated_at))
[void]$txt.AppendLine(("total_duration_ms: {0}" -f $report.total_duration_ms))
[void]$txt.AppendLine("")
foreach ($item in $results) {
    [void]$txt.AppendLine(("{0} [{1}] rc={2} duration_ms={3} script={4}" -f $item.name, $item.status, $item.rc, $item.duration_ms, $item.script))
}
$txt.ToString() | Set-Content -Encoding UTF8 $txtPath

Write-Host ""
Write-Host ("report json: {0}" -f $jsonPath)
Write-Host ("report txt:  {0}" -f $txtPath)

if ($failed -ne 0) {
    exit 1
}

exit 0
