# Model profiles and context governance

## Model profiles

`xllm_model_profile` is a non-secret contract for one model. It declares the
provider dialect, capabilities, window accounting mode, context/input/output
limits, and recommended output/summary reserves. Endpoint URLs and API keys
remain in `xllm_client_config`.

Built-in profile snapshots currently include `glm-5`, `glm-5.1`, and
`glm-5.2`. The GLM-5.2 coding profile uses a 1,000,000-token shared context
window and a 131,072-token output ceiling. GLM-5/5.1 use 204,800 and 131,072.
These values are data, not hidden constants: a host can inspect the profile or
provide a validated custom profile.

When `xllm_client_config.pModelProfile` is set, client creation and every model
request fail before transport if the selected model, requested output limit, or
required tool/reasoning/stream capability is outside the profile contract.
Legacy clients without a profile remain supported but do not receive this
governance.

## Compaction quality gate

Session compaction is a transaction with a deterministic quality gate. A
candidate summary must:

- fit `uSummaryMinTokens..uSummaryMaxTokens` (the minimum increases for large
  source prefixes);
- contain all headings selected by `uCompactionRequiredSections`;
- preserve the eight default sections: objective, constraints, architecture
  and decisions, completed work, repository state, verification evidence, open
  issues and risks, and exact next actions.

`xllmCompactionEvaluateSummary()` exposes the source/summary token estimates,
present and missing section masks, and the final decision. Commit calls the
same evaluator, so a host cannot accidentally checkpoint a summary that it
would reject during preview. The policy is persisted with the session.

The evaluator is intentionally structural and deterministic. The workflow
layer owns model retries and richer task-specific checks; the session layer
owns the invariant that only a policy-compliant summary can advance the durable
compaction checkpoint.
