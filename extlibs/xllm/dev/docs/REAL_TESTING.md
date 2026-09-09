# xllm 真实环境测试

这份文档用于在拿到真实账号和 API key 后，验证 `xllm` 与真实平台的：

- 连通性
- 鉴权
- 请求组装
- 响应解析
- 流式行为
- 结构化输出
- tool loop
- 多模态输入

当前真实联调围绕两个入口展开：

- 单次 probe: [real_provider_probe.c](./examples/real_provider_probe.c)
- 批量 runner: [run_real_provider_probe_matrix.ps1](./run_real_provider_probe_matrix.ps1)

配套脚本：

- 构建脚本: [build_real_provider_probe_example.bat](./build_real_provider_probe_example.bat)
- Windows 包装脚本: [run_real_provider_probe_matrix.bat](./run_real_provider_probe_matrix.bat)

补充回归：

- 期望断言失败分支: [smoke_real_provider_probe_expectation_failure.c](./examples/smoke_real_provider_probe_expectation_failure.c)
- `tool_choice=none` 分支: [smoke_real_provider_probe_tool_choice_none.c](./examples/smoke_real_provider_probe_tool_choice_none.c)
- OpenAI 额外 header 分支: [smoke_real_provider_probe_openai_headers.c](./examples/smoke_real_provider_probe_openai_headers.c)
- Anthropic 额外 header 分支: [smoke_real_provider_probe_anthropic_headers.c](./examples/smoke_real_provider_probe_anthropic_headers.c)
- `tool_choice=named` 分支: [smoke_real_provider_probe_tool_choice_named.c](./examples/smoke_real_provider_probe_tool_choice_named.c)

## 目标

`real_provider_probe` 不是为了替代所有 smoke。

更准确地说：

- smoke 负责离线回归和协议细节
- real probe 负责真实 key、真实网关、真实模型的联调确认

只要 probe 能在真实环境里成功返回，就说明至少这些链路已经跑通：

- `base_url` 可达
- 鉴权生效
- `xllm` 的请求格式与该 provider 主路径兼容
- `xllm` 能正确解析该 provider 的真实响应

即使最终状态是 `refused` 或 `content_filtered`，也仍然能说明协议主路径工作正常。

## 先构建

```powershell
cmd /c build_real_provider_probe_example.bat
```

产物：

- 可执行文件: [real_provider_probe.exe](./build/real_provider_probe.exe)

## 单次 probe

最少需要这些环境变量：

- `XLLM_REAL_ADAPTER`
- `XLLM_REAL_BASE_URL`
- `XLLM_REAL_MODEL`

如果不想重复设置通用 `XLLM_REAL_*` 变量，`real_provider_probe` 现在也支持直接读取平台别名环境变量。
当前已由本地 smoke 覆盖并验证过的别名入口包括：

- `OPENAI_*`
- `ANTHROPIC_*`
- `GLM_*`
- `MINIMAX_*`
- `KIMI_*`
- `QWEN_*`
- `DOUBAO_*`
- `GEMINI_*`
- `VERTEX_GEMINI_*`

当前也已经有本地 smoke 验证过的多模态 alias-env probe 入口：
- `OPENAI_*`
- `ANTHROPIC_*`
- `GLM_*`
- `MINIMAX_*`
- `KIMI_*`
- `QWEN_*`
- `DOUBAO_*`
- `GEMINI_*`
- `VERTEX_GEMINI_*`

常见示例：

```powershell
$env:XLLM_REAL_ADAPTER = "openai_compat"
$env:XLLM_REAL_BASE_URL = "https://your-openai-compatible-endpoint/v1"
$env:XLLM_REAL_MODEL = "your-model"
$env:XLLM_REAL_API_KEY = "your-key"
$env:XLLM_REAL_AUTH_KIND = "bearer"

.\build\real_provider_probe.exe
```

probe 会输出两类信息：

- 人类可读摘要
  - `status`
  - `finish_reason`
  - `visible_text`
  - `usage`
  - `tool_calls`
  - `events`
  - `duration_ms`
- 机器可读摘要
  - `probe_summary_json: {...}`

如果设置了 `XLLM_REAL_SUMMARY_PATH`，probe 还会把同样的 JSON 写入指定文件。

即使 setup 很早就失败，比如缺少必需环境变量，也会尽量输出 `probe_summary_json`。

## 期望断言

`real_provider_probe` 支持轻量断言，这样真实联调不只是“能不能连通”，还能确认“结果是否符合预期”。

支持这些环境变量：

- `XLLM_REAL_EXPECT_STATUS`
- `XLLM_REAL_EXPECT_TEXT_CONTAINS`
- `XLLM_REAL_EXPECT_JSON_CONTAINS`
- `XLLM_REAL_EXPECT_TOOL_CALLS_MIN`
- `XLLM_REAL_EXPECT_IMAGE_PARTS_MIN`
- `XLLM_REAL_EXPECT_FILE_PARTS_MIN`
- `XLLM_REAL_EXPECT_ARTIFACT_PARTS_MIN`
- `XLLM_REAL_EXPECT_ARTIFACT_BEGIN_MIN`
- `XLLM_REAL_EXPECT_ARTIFACT_CHUNK_MIN`
- `XLLM_REAL_EXPECT_ARTIFACT_READY_MIN`

行为规则：

- 如果请求成功到达 provider，但最终结果不满足这些期望，probe 会非 0 退出
- 失败原因会写进 `probe_summary_json.error_message`
- 当前这类失败统一映射为 `error_code=parse`

如果 provider 返回了图片、文件、音频或视频输出，probe 现在还会在 summary 里记录：

- `text_part_count`
- `image_part_count`
- `file_part_count`
- `audio_part_count`
- `video_part_count`
- `json_part_count`
- `artifact_part_count`
- `event_artifact_begin_count`
- `event_artifact_chunk_count`
- `event_artifact_ready_count`

示例：

```powershell
$env:XLLM_REAL_EXPECT_TEXT_CONTAINS = "pong"
.\build\real_provider_probe.exe
```

如果模型返回文本里没有 `pong`，probe 会失败，并输出类似：

```text
expected visible_text to contain 'pong'
```

## 批量运行

如果想批量跑一组真实场景：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run_real_provider_probe_matrix.ps1
```

或者：

```powershell
cmd /c run_real_provider_probe_matrix.bat
```

现在也可以直接通过命令行参数覆盖这两个高频入口，避免每次都先设环境变量：

- `-OutputDir`
- `-CaseFilter`

示例：

```powershell
.\run_real_provider_probe_matrix.ps1 -OutputDir .\build\real_probe_qwen -CaseFilter text,stream,json
```

runner 会：

- 自动构建 probe
- 逐个运行预设 case
- 为每个 case 生成独立日志
- 为每个 case 生成独立 summary JSON
- 生成文本报告和 JSON 报告

当前内置 case：

- `text`
- `stream`
- `json`
- 可选 `json_schema`
- 可选 `thinking`
- 可选 `tool`
- 可选 `provider_tool`
- 可选 `multimodal`

这些 case 还带了最小断言：

- `text` 和 `stream`
  - 期望 `visible_text` 包含 `pong`
- `json` 和 `json_schema`
  - 期望 `json_output` 包含 `"value":"pong"`
- `thinking`
  - 期望 `visible_text` 包含 `pong`
- `tool`
  - 期望 `visible_text` 包含 `tool-pong`
- `provider_tool`
  - 默认不带固定文本断言，更适合验证 provider-native tool 能否被真实平台接受

## 批量输出

默认输出位置：

- 文本报告: [real_provider_probe_report.txt](./build/real_provider_probe_report.txt)
- JSON 报告: [real_provider_probe_report.json](./build/real_provider_probe_report.json)
- case 日志: `build/real_provider_probe_logs/*.log`
- case summary: `build/real_provider_probe_logs/*.summary.json`

如果在 runner 启动前就缺少必需环境变量，它也会先生成 txt/json 报告，再以非 0 退出。

可以通过 `XLLM_REAL_OUTPUT_DIR` 指定单独的输出目录，适合按平台、账号或批次留档。

## 常用环境变量

### 基础连接

- `XLLM_REAL_ADAPTER`
  - `openai_compat`
  - `anthropic_native`
  - `ollama_native`
- `XLLM_REAL_BASE_URL`
- `XLLM_REAL_MODEL`
- `XLLM_REAL_API_KEY`
- `XLLM_REAL_AUTH_KIND`
  - `none`
  - `bearer`
  - `api_key_header`
- `XLLM_REAL_AUTH_HEADER`
- `XLLM_REAL_AUTH_SCHEME`
- `*_AUTH_KIND / *_AUTH_HEADER / *_AUTH_SCHEME`
  - probe 现在也支持平台别名认证变量，例如 `AZURE_OPENAI_AUTH_KIND`、`OPENAI_AUTH_HEADER`、`VERTEX_GEMINI_AUTH_KIND`
- `XLLM_REAL_OPENAI_ORGANIZATION`
- `XLLM_REAL_OPENAI_PROJECT`
- `XLLM_REAL_ANTHROPIC_VERSION`
- `XLLM_REAL_ANTHROPIC_BETA_HEADERS`
- `XLLM_REAL_DEFAULT_HEADERS`
- `XLLM_REAL_PROFILE_ID`
- `XLLM_REAL_TIMEOUT_MS`
- `XLLM_REAL_PROXY_KIND`
- `XLLM_REAL_PROXY_HOST`
- `XLLM_REAL_PROXY_PORT`
- `XLLM_REAL_PROXY_USER`
- `XLLM_REAL_PROXY_PASS`
- `XLLM_REAL_PROXY_TEST_URL`
- `XLLM_REAL_PROXY_PREFLIGHT_ONLY`
- `XLLM_REAL_PROXY_PREFLIGHT_ONLY=1` 时，runner 只执行代理预检，不要求 `XLLM_REAL_ADAPTER`、`XLLM_REAL_BASE_URL`、`XLLM_REAL_MODEL`

说明：

- `XLLM_REAL_ANTHROPIC_BETA_HEADERS` 使用逗号分隔，例如 `beta-a,beta-b`
- `XLLM_REAL_DEFAULT_HEADERS` 使用分号分隔的 `Name=Value` 列表，例如 `X-Demo=yes;X-Trace=123`

### 请求内容

- `XLLM_REAL_SYSTEM_PROMPT`
- `XLLM_REAL_PROMPT`
- `XLLM_REAL_MAX_OUTPUT_TOKENS`
- `XLLM_REAL_STREAM`
  - `0`
  - `1`
  - `auto`
  - `prefer`
  - `require`

### 结构化输出

- `XLLM_REAL_RESPONSE_FORMAT`
  - `text`
  - `json`
  - `json_schema`
- `XLLM_REAL_BEST_EFFORT_JSON`
- `XLLM_REAL_JSON_SCHEMA`
- `XLLM_REAL_SCHEMA_NAME`

### reasoning / thinking

- `XLLM_REAL_REASONING`
  - `off`
  - `low`
  - `medium`
  - `high`
- `XLLM_REAL_EXPOSE_THINKING`
- `XLLM_REAL_REASONING_BUDGET_TOKENS`

### 高层自动工具循环

- `XLLM_REAL_ENABLE_TOOL`
- `XLLM_REAL_TOOL_KIND`
  - `client`
  - `provider`
- `XLLM_REAL_TOOL_CHOICE`
  - `auto`
  - `none`
  - `required`
  - `named`
- `XLLM_REAL_TOOL_CHOICE_NAME`
- `XLLM_REAL_ALLOW_PARALLEL_TOOL_CALLS`
- `XLLM_REAL_TOOL_REQUIRED`
- `XLLM_REAL_TOOL_ID`
- `XLLM_REAL_TOOL_WIRE_NAME`
- `XLLM_REAL_TOOL_DESCRIPTION`
- `XLLM_REAL_TOOL_SCHEMA`
- `XLLM_REAL_TOOL_RESULT_TEXT`
- `XLLM_REAL_TOOL_RESULT_IMAGE_URL`
- `XLLM_REAL_TOOL_RESULT_IMAGE_FILE_ID`
- `XLLM_REAL_TOOL_RESULT_IMAGE_MIME`
- `XLLM_REAL_TOOL_RESULT_FILE_URL`
- `XLLM_REAL_TOOL_RESULT_FILE_FILE_ID`
- `XLLM_REAL_TOOL_RESULT_FILE_MIME`
- `XLLM_REAL_TOOL_RESULT_FILE_NAME`
- `XLLM_REAL_PROVIDER_TOOL_JSON`

说明：

- `client` 模式会注册本地工具执行器，并使用 `XLLM_REAL_TOOL_SCHEMA`
- `provider` 模式不会注册本地执行器，而是把 `XLLM_REAL_PROVIDER_TOOL_JSON` 直接作为 provider-native tool 透传
- `XLLM_REAL_TOOL_CHOICE=named` 时，建议同时设置 `XLLM_REAL_TOOL_CHOICE_NAME`
- 如果未设置 `XLLM_REAL_TOOL_CHOICE_NAME`，probe 会回退到 `XLLM_REAL_TOOL_WIRE_NAME`
- `XLLM_REAL_TOOL_REQUIRED=1` 仍然保留，作为旧的兼容写法；如果未设置 `XLLM_REAL_TOOL_CHOICE`，它等价于 `required`
- `XLLM_REAL_ALLOW_PARALLEL_TOOL_CALLS=1` 会把高层 tool policy 的并行开关打开
- 如果未设置 `XLLM_REAL_PROVIDER_TOOL_JSON`，probe 会按 adapter 提供最小默认值
  - `anthropic_native`: `web_search_20250305`
  - `ollama_native`: `web_search`

### 多模态输入

图片输入：

- `XLLM_REAL_IMAGE_URL`
- `XLLM_REAL_IMAGE_PATH`
- `XLLM_REAL_IMAGE_FILE_ID`
- `XLLM_REAL_IMAGE_MIME`
- `XLLM_REAL_IMAGE_NAME`
- `XLLM_REAL_MULTIMODAL_MODEL`

文件输入：

- `XLLM_REAL_FILE_URL`
- `XLLM_REAL_FILE_PATH`
- `XLLM_REAL_FILE_FILE_ID`
- `XLLM_REAL_FILE_MIME`
- `XLLM_REAL_FILE_NAME`

### probe 输出

- `XLLM_REAL_SUMMARY_PATH`
- `XLLM_REAL_CASE_NAME`

### batch runner

- `XLLM_REAL_OUTPUT_DIR`
- `XLLM_REAL_CASE_FILTER`
- `XLLM_REAL_RUN_JSON_SCHEMA_CASES`
- `XLLM_REAL_RUN_THINKING_CASES`
- `XLLM_REAL_RUN_TOOL_CASES`
- `XLLM_REAL_RUN_PROVIDER_TOOL_CASES`
- `XLLM_REAL_RUN_MULTIMODAL_CASES`
- If proxy env is configured, the matrix runner performs a lightweight proxy preflight before running cases.

说明：

- `XLLM_REAL_CASE_FILTER` 使用逗号分隔，例如 `text,stream,json`
- 如果 filter 指定了未知 case，runner 会把它记为 `skipped`
- 如果 filter 指定了某个 case，但对应 `XLLM_REAL_RUN_*` 开关没开，runner 也会把它记为 `skipped`
- `multimodal` case 被启用但没有提供任何图片或文件输入时，也会记为 `skipped`

## 推荐测试顺序

建议真实联调按下面的顺序推进：

1. `text`
2. `stream`
3. `json`
4. `thinking`
5. `tool`
6. `provider_tool`
7. `multimodal`

这样更容易定位问题：

- 先确认基础连通性
- 再确认流式
- 再确认结构化输出
- 最后再看工具和多模态

## Unsupported Capability Notes

当前本地 `probe` matrix 还固定了两条与真实联调直接相关的“已知不支持”行为：

- `anthropic_native + json_schema`
  - 未开启 best-effort 时，预期本地返回 `unsupported_capability`
- `ollama_native + tool_choice=required`
  - 预期本地返回 `unsupported_capability`

另外需要注意：

- `ollama_native + json_schema` 当前不会在本地前置拒绝，而是会继续尝试请求上游。
- 因此这条行为目前不作为 unsupported smoke 基线，而是保留为“实现现状”，后续在新设计阶段再统一评估。

## Azure OpenAI 真实联调备注

当前仓库已提供一组 Azure OpenAI 专用示例，位于：

- [azure_openai_stateless.c](./examples/openai/azure_openai/azure_openai_stateless.c)
- [azure_openai_session.c](./examples/openai/azure_openai/azure_openai_session.c)
- [azure_openai_stream.c](./examples/openai/azure_openai/azure_openai_stream.c)
- [azure_openai_json_schema.c](./examples/openai/azure_openai/azure_openai_json_schema.c)
- [azure_openai_tool_loop.c](./examples/openai/azure_openai/azure_openai_tool_loop.c)
- [azure_openai_multimodal.c](./examples/openai/azure_openai/azure_openai_multimodal.c)

已验证通过的真实能力：

- 文本非流式
- `session` 多轮上下文
- 流式输出
- 严格 JSON Schema 输出
- 高层自动工具循环
- 多模态图片输入：
  - `image_url`
  - 小尺寸 inline image bytes

已观察到的真实行为：

- 在 `https://eastus2.api.cognitive.microsoft.com/` 这条 endpoint 上，较大的 inline image base64 请求体可能导致 upstream 在返回任何 HTTP 状态前直接关闭连接
- 该场景下的典型日志表现为：
  - `transport_status=-4`
  - `http_status=0`
  - `error_code=network`
- 这不能直接视为 Azure OpenAI 的公开通用大小上限；当前更合理的结论是：
  - `image_url` 路径稳定可用
  - 小尺寸 inline image bytes 可用
  - 较大 inline image bytes 在当前 endpoint/网关/legacy 路径下存在尺寸敏感行为

因此，当前对 Azure OpenAI 的真实联调建议是：

- 优先使用 `image_url`
- 如需测试 inline image bytes，优先使用小尺寸图片
- 如需继续排查大图行为，建议单独对资源专属 endpoint 或不同网关路径复测

## GLM / Kimi 多模态真实联调补充

基于原生 adapter，已补充验证：

- [glm_multimodal.c](./examples/glm/glm_multimodal.c)
- [kimi_multimodal.c](./examples/kimi/kimi_multimodal.c)

并为两条示例增加了便于排障的环境变量：

- GLM：
  - `GLM_DEBUG=1`
  - `GLM_TIMEOUT_MS`
- Kimi：
  - `KIMI_DEBUG=1`
  - `KIMI_TIMEOUT_MS`
  - `KIMI_IMAGE_FILE_ID`

当前真实结论：

- `image_url` 输入：
  - GLM native 成功
  - Kimi native 成功
- 本地图片 `C:\1.png`（约 67 KB，最终请求体约 89 KB）：
  - GLM native 失败
  - Kimi native 失败
- 两家的失败表现一致：
  - `transport_status=-4`
  - `http_status=0`
  - `error_code=network`

因此当前建议是：

- GLM / Kimi 的真实多模态联调优先走 `image_url`
- 若要测 `inline image bytes`，先用小图
- 对较大 inline 图像的异常关闭连接行为，先视为 provider / 网关侧真实行为差异记录，后续再结合官方端点继续复测

## 建议

- 保留现有 smoke，继续作为离线回归基线
- 对难模拟、模拟价值低的 provider 特性，直接以真实 probe 结果为准
- 最终平台适配是否稳定，以真实 key 联调结果为准

## Additional ASCII Notes

- `probe_qwen_named_tool_with_thinking_unsupported`
  - Adapter: `qwen_native`
  - Trigger: `XLLM_REAL_REASONING=medium` with `XLLM_REAL_TOOL_CHOICE=named`
  - Expected result: `unsupported_capability`
  - Expected message: `qwen native thinking mode does not support forcing a specific tool`
- `probe_gemini_tool_result_image_unsupported`
  - Adapter: `gemini_native`
  - Trigger: tool loop enabled with `GEMINI_TOOL_RESULT_IMAGE_URL`
  - Expected result: `unsupported_input_type`
  - Expected message: `gemini tool result currently supports only text/json parts`
- `probe_vertex_gemini_tool_result_image_unsupported`
  - Adapter: `vertex_gemini_native`
  - Trigger: tool loop enabled with `VERTEX_GEMINI_TOOL_RESULT_IMAGE_URL`
  - Expected result: `unsupported_input_type`
  - Expected message: `gemini tool result currently supports only text/json parts`
