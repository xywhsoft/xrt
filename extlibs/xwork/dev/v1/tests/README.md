# xwork smoke tests

This directory contains the default offline smoke coverage for `xwork`.

## Test groups

- `standalone`: compiles the aggregate `xwork.c` translation unit.
- `core`: runs runtime/workspace/tool/run lifecycle smoke.
- `host`: runs direct local host filesystem/process invoke smoke.
- `orchestrator`: runs the main offline runtime/orchestrator/host/persistence smoke.
- `persistence`: runs focused file persistence snapshot/event/artifact/run-index smoke.
- `profile`: runs focused built-in profile defaults and profile apply/bootstrap coverage.
- `provider-offline`: runs provider bootstrap, local HTTP stub, and offline provider failure smoke.
- `stress`: runs focused terminal long-output and many-run/artifact persistence query stress coverage.
- `multi-agent`: runs in-process agent pool/task graph fan-out/fan-in scheduling, handoff request/result refs and events, handoff snapshot persistence/recovery, agent result/aggregate report artifacts, snapshot import, agent pool/task graph snapshot persistence, and recovery smoke.
- `remote-worker`: runs control plane/worker registration, HTTP decoded-message transport marker, task approval gate, network policy denial, capability allowlist denial, secret redaction, heartbeat, assignment, remote `process.exec`, remote `process.exec` timeout, destructive-command denial, remote terminal start/list/stop, remote filesystem write/read/apply_patch, workspace-root path denial, artifact/diagnostics summary refs on task results, artifact upload message/blob-ref/chunk-payload query and recovery, stdout/stderr output chunk recovery, cancel, stale-lease, control-plane snapshot persistence, and recovery smoke.
- `replay`: runs deterministic replay cassette API, typed filesystem snapshot/ref API, normalized JSON hash compatibility, replay event log schema, model stream event helper, terminal interaction event, event sequence replay, hash, checkpoint seek, strict success, audit divergence, divergence report artifact, side-effect blocking, cancel, manifest/result, entry query, file persistence list/load/recover, and `.replay` future-version rejection smoke.
- `examples`: compiles and runs the product-facing AI IDE, claw, multi-agent claw, remote worker, and replay examples.
- `provider-real`: optional real-provider smoke, gated by environment variables and not required for default CI.

The main orchestrator smoke is still a large aggregate test for end-to-end
runtime behavior. Core, host, persistence, profile, provider, and stress coverage also
have independent focused binaries.

## Default Windows/MSYS2 commands

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -c xwork.c
gcc -std=c11 -Wall -Wextra -pedantic -Ilib\sqlite tests\xwork_core_smoke.c lib\sqlite\sqlite3.c -o tests\xwork_core_smoke.exe -lws2_32 -liphlpapi
gcc -std=c11 -Wall -Wextra -pedantic -Ilib\sqlite tests\xwork_host_smoke.c lib\sqlite\sqlite3.c -o tests\xwork_host_smoke.exe -lws2_32 -liphlpapi
gcc -std=c11 -Wall -Wextra -pedantic -Ilib\sqlite tests\xwork_orchestrator_smoke.c lib\sqlite\sqlite3.c -o tests\xwork_orchestrator_smoke.exe -lws2_32 -liphlpapi
gcc -std=c11 -Wall -Wextra -pedantic -Ilib\sqlite tests\xwork_persistence_smoke.c lib\sqlite\sqlite3.c -o tests\xwork_persistence_smoke.exe -lws2_32 -liphlpapi
gcc -std=c11 -Wall -Wextra -pedantic -Ilib\sqlite tests\xwork_profile_smoke.c lib\sqlite\sqlite3.c -o tests\xwork_profile_smoke.exe -lws2_32 -liphlpapi
gcc -std=c11 -Wall -Wextra -pedantic -Ilib\sqlite tests\xwork_orchestrator_provider_smoke.c lib\sqlite\sqlite3.c -o tests\xwork_orchestrator_provider_smoke.exe -lws2_32 -liphlpapi
gcc -std=c11 -Wall -Wextra -pedantic -Ilib\sqlite tests\xwork_stress_smoke.c lib\sqlite\sqlite3.c -o tests\xwork_stress_smoke.exe -lws2_32 -liphlpapi
gcc -std=c11 -Wall -Wextra -pedantic -Ilib\sqlite tests\xwork_multi_agent_smoke.c lib\sqlite\sqlite3.c -o tests\xwork_multi_agent_smoke.exe -lws2_32 -liphlpapi
gcc -std=c11 -Wall -Wextra -pedantic -Ilib\sqlite tests\xwork_remote_worker_smoke.c lib\sqlite\sqlite3.c -o tests\xwork_remote_worker_smoke.exe -lws2_32 -liphlpapi
gcc -std=c11 -Wall -Wextra -pedantic -Ilib\sqlite tests\xwork_replay_smoke.c lib\sqlite\sqlite3.c -o tests\xwork_replay_smoke.exe -lws2_32 -liphlpapi
tests\xwork_core_smoke.exe
tests\xwork_host_smoke.exe
tests\xwork_orchestrator_smoke.exe
tests\xwork_persistence_smoke.exe
tests\xwork_profile_smoke.exe
tests\xwork_orchestrator_provider_smoke.exe
tests\xwork_stress_smoke.exe
tests\xwork_multi_agent_smoke.exe
tests\xwork_remote_worker_smoke.exe
tests\xwork_replay_smoke.exe
```

## Example commands

```powershell
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\ai_ide_agent.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\ai_ide_agent.exe -lws2_32 -liphlpapi
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\claw_autonomous_agent.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\claw_autonomous_agent.exe -lws2_32 -liphlpapi
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\multi_agent_claw.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\multi_agent_claw.exe -lws2_32 -liphlpapi
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\remote_worker_agent.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\remote_worker_agent.exe -lws2_32 -liphlpapi
gcc -std=c11 -Wall -Wextra -pedantic -I. -Ilib\sqlite examples\replay_agent_run.c examples\xwork_example_runtime.c lib\sqlite\sqlite3.c -o examples\replay_agent_run.exe -lws2_32 -liphlpapi
examples\ai_ide_agent.exe
examples\claw_autonomous_agent.exe
examples\multi_agent_claw.exe
examples\remote_worker_agent.exe
examples\replay_agent_run.exe
```

## Optional real provider smoke

`tests/xwork_orchestrator_provider_smoke.exe` runs offline by default. To run it
against a real provider, set at least:

```powershell
$env:XWORK_PROVIDER_SMOKE_ADAPTER="openai_compat"
$env:XWORK_PROVIDER_SMOKE_BASE_URL="https://provider.example/v1"
$env:XWORK_PROVIDER_SMOKE_MODEL="model-id"
$env:XWORK_PROVIDER_SMOKE_API_KEY="..."
tests\xwork_orchestrator_provider_smoke.exe
```

Optional provider variables:

- `XWORK_PROVIDER_SMOKE_EXPECT_ERROR`: set to `1` when the configured real
  provider endpoint is expected to fail, for example with a bad key/model or a
  controlled error endpoint. The smoke then requires `XWORK_ERROR_EXTERNAL_FAILURE`
  and a failed run instead of a completed tool-loop.
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
