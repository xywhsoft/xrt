# xwork Policy And Approval Contract

This document fixes the policy and approval boundary used by local tools,
remote workers, replay, and agent scheduling.

## Policy Ownership

- `xwork_policy_options` is copied by value into runtime/control-plane options
  that store it.
- Network allow/deny pattern arrays and the strings inside those arrays are
  borrowed. They must outlive the runtime or control plane that evaluates them.
- Approval evaluation input strings are borrowed for the duration of the call.
  Approval decision strings point to stable internal/default text or input-owned
  override strings, so callers must copy them if they need longer lifetime.

## Approval Modes

- `XWORK_APPROVAL_NEVER` means the operation does not require approval.
- `XWORK_APPROVAL_ALWAYS` means the operation requires approval.
- `XWORK_APPROVAL_ON_DEMAND` means the operation requires approval when it is
  evaluated.
- `XWORK_APPROVAL_DEFAULT` allows read-only work without approval. Side-effecting
  work requires approval for manual/semi-auto autonomy and does not require
  approval for full-auto autonomy unless the caller or profile requests a stricter
  mode.

## Risk Evaluation

- Read-only tools map to low risk.
- Workspace writes map to medium risk.
- Process execution and network access map to high risk.
- External mutations map to critical risk.
- A caller may provide an explicit risk level, scope, and reason override when
  profile or tool context has better information than the side-effect class.
- Auto approval is only reported when approval is required, auto approval was
  requested, and the effective risk is less than or equal to
  `eAutoApproveRiskLimit`.
- The default auto-approve risk limit is permissive for embedding flexibility.
  Strict products should set a lower limit in profile/runtime policy.

## Network Policy

- If no network access is requested, evaluation allows the operation.
- A missing URL/host for requested network access is denied.
- Deny patterns are checked before allow patterns.
- If allow patterns are configured, the host must match one of them.
- If no allow patterns are configured, access is allowed unless
  `bDenyNetworkByDefault` is true.
- Denied network access returns a high-risk decision with a stable reason string.

## Runtime And Orchestrator Boundary

- Tool/orchestrator code should evaluate approval before executing a
  side-effecting host tool or remote task.
- If approval is required and not auto-approved, the run should enter a waiting
  approval state and expose an `xwork_approval_request`.
- `xwork_run_submit_approval()` records the caller decision and lets the run
  continue or fail according to the decision.
- Approval decisions are audit data. They should be persisted with run events or
  snapshots when the embedding needs recovery/audit.
- Multi-agent scheduling does not bypass policy. Each task callback, host tool,
  or remote assignment remains responsible for using the same policy boundary.

## Remote Worker Boundary

- Remote task assignment must pass through policy/approval before dispatch.
- Worker capability allowlists are an additional gate, not a replacement for
  policy.
- Workspace root enforcement and network policy apply before worker execution.
- Destructive command classification should raise the effective risk before
  approval evaluation.
- Remote worker credentials and tenant secrets are caller-owned and are not
  serialized into persistence snapshots.

## Replay Boundary

- Replay strict/audit modes may return recorded host results without invoking the
  real host service.
- Replay record mode with side-effect blocking refuses side-effecting host calls
  before the real service is invoked.
- Replay does not grant approval. A caller that replays a run for audit should
  keep approval records as part of the persisted event/checkpoint stream.
- Divergence reports should be treated as audit failures when they affect a
  side-effecting decision path.

