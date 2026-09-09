# Provider Smoke

>Status: First draft in Chinese, awaiting review.

Provider smoke is used to verify that xwork executes the path of a real or stub provider through xllm. By default CI only runs offline stubs; real providers require explicit environment variables.

## Coverage

`tests/xwork_orchestrator_provider_smoke.c` covers:

- OpenAI-compatible, Anthropic-compatible, Ollama-compatible offline stubs.
- provider model-call failure mapping.
- Optional real provider success path.
- Optional expected-error path.
- Main execution path uses `xwork_run_execute_async()`.

## Build

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -Ilib\sqlite tests\xwork_orchestrator_provider_smoke.c lib\sqlite\sqlite3.c -o tests\xwork_orchestrator_provider_smoke.exe -lws2_32 -liphlpapi
```

## Real provider success path

```powershell
$env:XWORK_PROVIDER_SMOKE_ADAPTER="openai_compat"
$env:XWORK_PROVIDER_SMOKE_BASE_URL="https://provider.example/v1"
$env:XWORK_PROVIDER_SMOKE_MODEL="model-id"
$env:XWORK_PROVIDER_SMOKE_API_KEY="..."
tests\xwork_orchestrator_provider_smoke.exe
```

expected:

- run completed successfully.
- The model executes a minimal tool loop.
- orchestrator returns `XWORK_OK`.

## Expected-error path

```powershell
$env:XWORK_PROVIDER_SMOKE_EXPECT_ERROR="1"
tests\xwork_orchestrator_provider_smoke.exe
```

expected:

- provider call failed.
- orchestrator maps to `XWORK_ERROR_EXTERNAL_FAILURE`.
- run into `XWORK_RUN_FAILED`.
- summary contains `xllm_error=<code>` and provider message.

## Commonly used variables

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

## Next step

- [xllm Integration API](../api/api-xllm-integration.md)
- [Orchestrator API](../api/api-orchestrator.md)
- [Internal provider smoke runbook](../../dev/docs/PROVIDER_SMOKE.md)
