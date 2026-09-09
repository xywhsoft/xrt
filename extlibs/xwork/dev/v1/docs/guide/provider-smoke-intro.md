# Provider Smoke

> 状态：中文初稿，待审阅。

Provider smoke 用于验证 xwork 通过 xllm 执行真实或 stub provider 的路径。默认 CI 只跑离线 stub；真实 provider 需要显式环境变量。

## 覆盖范围

`tests/xwork_orchestrator_provider_smoke.c` 覆盖：

- OpenAI-compatible、Anthropic-compatible、Ollama-compatible 离线 stub。
- provider model-call failure 映射。
- 可选真实 provider 成功路径。
- 可选 expected-error 路径。
- 主执行路径使用 `xwork_run_execute_async()`。

## 构建

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -Ilib\sqlite tests\xwork_orchestrator_provider_smoke.c lib\sqlite\sqlite3.c -o tests\xwork_orchestrator_provider_smoke.exe -lws2_32 -liphlpapi
```

## 真实 provider 成功路径

```powershell
$env:XWORK_PROVIDER_SMOKE_ADAPTER="openai_compat"
$env:XWORK_PROVIDER_SMOKE_BASE_URL="https://provider.example/v1"
$env:XWORK_PROVIDER_SMOKE_MODEL="model-id"
$env:XWORK_PROVIDER_SMOKE_API_KEY="..."
tests\xwork_orchestrator_provider_smoke.exe
```

预期：

- run 成功完成。
- 模型执行最小 tool loop。
- orchestrator 返回 `XWORK_OK`。

## Expected-error 路径

```powershell
$env:XWORK_PROVIDER_SMOKE_EXPECT_ERROR="1"
tests\xwork_orchestrator_provider_smoke.exe
```

预期：

- provider 调用失败。
- orchestrator 映射为 `XWORK_ERROR_EXTERNAL_FAILURE`。
- run 进入 `XWORK_RUN_FAILED`。
- summary 包含 `xllm_error=<code>` 和 provider message。

## 常用变量

- `XWORK_PROVIDER_SMOKE_PROFILE_ID`
- `XWORK_PROVIDER_SMOKE_XWORK_PROFILE`
- `XWORK_PROVIDER_SMOKE_PROVIDER`
- `XWORK_PROVIDER_SMOKE_OPENAI_ORG_ID`
- `XWORK_PROVIDER_SMOKE_OPENAI_PROJECT_ID`
- `XWORK_PROVIDER_SMOKE_DEBUG_MODE`
- `XWORK_PROVIDER_SMOKE_REDACT_MODE`
- `XWORK_PROVIDER_SMOKE_CONNECT_TIMEOUT_MS`
- `XWORK_PROVIDER_SMOKE_READ_TIMEOUT_MS`
- `XWORK_PROVIDER_SMOKE_VERIFY_PEER`
- `XWORK_PROVIDER_SMOKE_PROXY_KIND`

## 下一步

- [xllm Integration API](../api/api-xllm-integration.md)
- [Orchestrator API](../api/api-orchestrator.md)
- [内部 provider smoke runbook](../../dev/docs/PROVIDER_SMOKE.md)
