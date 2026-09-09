# xwork Provider Smoke

This document records how to run and interpret the optional real-provider smoke
for the `xllm` integration path.

## Scope

`tests/xwork_orchestrator_provider_smoke.c` covers:

- Offline local OpenAI-compatible, Anthropic-compatible, and Ollama-compatible
  stub request/response normalization.
- Offline provider model-call failure mapping.
- Optional real-provider execution through `xwork_run_execute_async`.
- Optional expected-error mode for bad credentials, bad model ids, rate-limit
  or controlled upstream failures.

The default CI matrix runs only the offline path. Real provider execution is
opt-in because it depends on credentials, network availability, model behavior,
and provider billing.

## Build

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -Ilib\sqlite tests\xwork_orchestrator_provider_smoke.c lib\sqlite\sqlite3.c -o tests\xwork_orchestrator_provider_smoke.exe -lws2_32 -liphlpapi
```

## Real Provider Success Path

Set at least:

```powershell
$env:XWORK_PROVIDER_SMOKE_ADAPTER="openai_compat"
$env:XWORK_PROVIDER_SMOKE_BASE_URL="https://provider.example/v1"
$env:XWORK_PROVIDER_SMOKE_MODEL="model-id"
$env:XWORK_PROVIDER_SMOKE_API_KEY="..."
tests\xwork_orchestrator_provider_smoke.exe
```

Expected result:

- The run completes successfully.
- The model performs a minimal tool loop.
- The smoke observes request/response/tool-loop traces.
- The orchestrator returns `XWORK_OK`.

## Real Provider Expected-Error Path

Use a deliberately invalid key, model id, or controlled error endpoint and set:

```powershell
$env:XWORK_PROVIDER_SMOKE_EXPECT_ERROR="1"
tests\xwork_orchestrator_provider_smoke.exe
```

Expected result:

- The model call fails through the provider.
- The orchestrator maps the failure to `XWORK_ERROR_EXTERNAL_FAILURE`.
- The run reaches `XWORK_RUN_FAILED`.
- The summary includes `xllm_error=<code>` and the provider message.

## Optional Variables

The smoke forwards these variables when present:

- `XWORK_PROVIDER_SMOKE_PROFILE_ID`
- `XWORK_PROVIDER_SMOKE_XWORK_PROFILE`
- `XWORK_PROVIDER_SMOKE_PROVIDER`
- `XWORK_PROVIDER_SMOKE_OPENAI_ORG_ID`
- `XWORK_PROVIDER_SMOKE_OPENAI_PROJECT_ID`
- `XWORK_PROVIDER_SMOKE_ANTHROPIC_API_VERSION`
- `XWORK_PROVIDER_SMOKE_ANTHROPIC_BETA_HEADERS`
- `XWORK_PROVIDER_SMOKE_DEBUG_MODE`
- `XWORK_PROVIDER_SMOKE_REDACT_MODE`
- `XWORK_PROVIDER_SMOKE_CONNECT_TIMEOUT_MS`
- `XWORK_PROVIDER_SMOKE_READ_TIMEOUT_MS`
- `XWORK_PROVIDER_SMOKE_VERIFY_PEER`
- `XWORK_PROVIDER_SMOKE_PROXY_KIND`
- `XWORK_PROVIDER_SMOKE_PROXY_HOST`
- `XWORK_PROVIDER_SMOKE_PROXY_PORT`
- `XWORK_PROVIDER_SMOKE_PROXY_USER`
- `XWORK_PROVIDER_SMOKE_PROXY_PASS`
- `XWORK_PROVIDER_SMOKE_CA_BUNDLE_PATH`
- `XWORK_PROVIDER_SMOKE_CLIENT_CERT_PATH`
- `XWORK_PROVIDER_SMOKE_CLIENT_KEY_PATH`

## Provider Drift Log Template

Record provider differences in `CHANGELOG.md` or release notes when they affect
behavior. Use this compact format:

```text
provider=<name>
adapter=<adapter>
model=<model-id-or-family>
date=<YYYY-MM-DD>
path=<success|expected-error>
status=<pass|fail|flaky>
observed=<short behavior note>
impact=<none|test-only|runtime-contract|api-contract>
action=<none|doc|test|xllm-update|xwork-update>
```

Behavior-impacting provider drift should be fixed or documented before cutting a
frozen `xwork` source snapshot.
