# Policy / Approval API

> Status: Chinese function-by-function reference, waiting for manual review.

The Policy / Approval API defines xwork's unified security boundaries before executing files, processes, networks, remote workers, and replay side effects.

## Module positioning

Policy determines "whether it is allowed, whether it requires approval, and whether it can be automatically approved." An Approval request is an audit object exposed to the product UI, CLI, or automated policies after the run is paused. This module does not implement UI, account permissions, worker authentication, or socket access control; these are still the responsibility of the host product.

## This page covers the statement

| Category | Statement |
| --- | --- |
| Structure | `xwork_policy_options`, `xwork_approval_eval_input`, `xwork_approval_decision`,
| Function | `xwork_policy_options_init`, `xwork_approval_eval_input_init`, `xwork_approval_decision_init`,

## Core Strategy

| Field/Object | Description |
| --- | --- |
| `eAutoApproveRiskLimit` | Highest risk level allowed for automatic approval. |
| `psNetworkAllowHostPatterns` | network host allowlist. Both arrays and strings are borrowed. |
| `psNetworkDenyHostPatterns` | Network host denylist, takes precedence over allowlist. |
| `bDenyNetworkByDefault` | None Whether to deny network access by default when allowlist is hit. |
| `xwork_approval_decision` | Allow/require approval/risk/scope/reason after evaluation. |
| |

## Ownership Rules

- Policy options will be copied by value to options such as runtime/control-plane, but allow/deny pattern arrays and strings are borrowed.
- Strings such as tool id, scope, reason, URL, and host in eval input are all borrowed.
- Strings in decision point to static text or input-owned override text; the caller should make a copy if long-term preservation is required.
- Approval request init does not allocate resources; the request obtained from query/load has a deep-copy field and must be reset after use.

---

### xwork_policy_options_init

Initialize policy options.

**Function:**

Used to set the default security policy before creating a runtime, control plane, or independent assessment.

**Function prototype:**

```c
XWORK_API void xwork_policy_options_init(xwork_policy_options *pOptions);
```

**parameter:**

- `pOptions`: Output parameter. Can be `NULL`; write default policy when not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. The allow/deny array field defaults to `NULL`, which is subsequently provided by the caller and maintained in the life cycle.

**Additional Note:**

- It is recommended to init first, then apply the profile, and then cover according to product security requirements.
- The default policy is appropriate for the development baseline and should not replace the final product security policy.

**Example code:**

```c
#include "xwork.h"

int main(void) {
    xwork_policy_options policy;
    xwork_policy_options_init(&policy);
    policy.bDenyNetworkByDefault = true;
    return 0;
}
```

**Related API:**

- `xwork_policy_evaluate_approval`
- `xwork_policy_evaluate_network_access`

---

### xwork_approval_eval_input_init

Initialize approval evaluation input.

**Function:**

Used to describe the approval context of a tool, remote task, or side effect.

**Function prototype:**

```c
XWORK_API void xwork_approval_eval_input_init(xwork_approval_eval_input *pInput);
```

**parameter:**

- `pInput`: Output parameter. Can be `NULL`; writes default autonomy, approval mode, side effect and risk when not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. String fields in the input are borrowed from the caller.

**Additional Note:**

- The risk override field can provide a more accurate description of risk than the default side-effect mapping.

**Example code:**

```c
xwork_approval_eval_input input;
xwork_approval_eval_input_init(&input);
input.eSideEffect = XWORK_SIDE_EFFECT_PROCESS_EXEC;
```

**Related API:**

- `xwork_policy_evaluate_approval`

---

### xwork_approval_decision_init

Initialize approval evaluation results.

**Function:**

Prepare to receive output from `xwork_policy_evaluate_approval`.

**Function prototype:**

```c
XWORK_API void xwork_approval_decision_init(xwork_approval_decision *pDecision);
```

**parameter:**

- `pDecision`: Output parameter. Can be `NULL`; cleared and written to default risk level when not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. The decision string is borrowed.

**Additional Note:**

- decision does not require reset.

**Example code:**

```c
xwork_approval_decision decision;
xwork_approval_decision_init(&decision);
```

**Related API:**

- `xwork_policy_evaluate_approval`

---

### xwork_network_policy_eval_input_init

Initialize network policy evaluation input.

**Function:**

The URL/host context used to describe a network access request.

**Function prototype:**

```c
XWORK_API void xwork_network_policy_eval_input_init(
    xwork_network_policy_eval_input *pInput
);
```

**parameter:**

- `pInput`: Output parameter. Can be `NULL`; cleared if not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. The URL and host strings are borrowed from the caller.

**Additional Note:**

- Assessments are usually allowed to pass when network access is not requested.

**Example code:**

```c
xwork_network_policy_eval_input input;
xwork_network_policy_eval_input_init(&input);
input.bNetworkAccessRequested = true;
input.sHost = "api.example.com";
```

**Related API:**

- `xwork_policy_evaluate_network_access`

---

### xwork_network_policy_decision_init

Initialize network policy evaluation results.

**Function:**

Prepare to receive output from `xwork_policy_evaluate_network_access`.

**Function prototype:**

```c
XWORK_API void xwork_network_policy_decision_init(
    xwork_network_policy_decision *pDecision
);
```

**parameter:**

- `pDecision`: Output parameter. Can be `NULL`; cleared and written to default risk level when not `NULL`.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. The decision string is borrowed.

**Additional Note:**

- decision does not require reset.

**Example code:**

```c
xwork_network_policy_decision decision;
xwork_network_policy_decision_init(&decision);
```

**Related API:**

- `xwork_policy_evaluate_network_access`

---

### xwork_approval_request_init

Initialize approval request.

**Function:**

Prepare to receive approval requests returned by run or persistence.

**Function prototype:**

```c
XWORK_API void xwork_approval_request_init(xwork_approval_request *pRequest);
```

**parameter:**

- `pRequest`: Output parameter. Can be `NULL`; cleared if not `NULL` and set default risk/status.

**Return value:**

none.

**Resource ownership:**

Functions do not allocate resources. The filled request has a deep-copy field, which is used to call `xwork_approval_request_reset`.

**Additional Note:**

- request is an audit object, usually exposed to UI/CLI and then called by the product `xwork_run_submit_approval`.

**Example code:**

```c
xwork_approval_request request;
xwork_approval_request_init(&request);
xwork_approval_request_reset(&request);
```

**Related API:**

- `xwork_run_get_last_approval_request`
- `xwork_approval_request_reset`

---

### xwork_approval_request_reset

Release and reset the approval request.

**Function:**

Release the id, run id, tool id, reason, scope, action summary and other deep-copy fields in the request.

**Function prototype:**

```c
XWORK_API void xwork_approval_request_reset(xwork_approval_request *pRequest);
```

**parameter:**

- `pRequest`: input/output parameters. Can be `NULL`.

**Return value:**

none.

**Resource ownership:**

Releases the copy of the string owned by request.

**Additional Note:**

- After reset, the request returns to the init state.

**Example code:**

```c
xwork_approval_request_reset(&request);
```

**Related API:**

- `xwork_approval_request_init`

---

### xwork_policy_evaluate_approval

Evaluate whether a side effect is allowed or requires approval.

**Function:**

Generate unified approval decisions based on autonomy, approval mode, side effect, risk override and auto-approve limit.

**Function prototype:**

```c
XWORK_API xwork_status xwork_policy_evaluate_approval(
    const xwork_policy_options *pPolicy,
    const xwork_approval_eval_input *pInput,
    xwork_approval_decision *pDecision
);
```

**parameter:**

- `pPolicy`: input parameters. Can be `NULL`; `NULL` uses the default policy.
- `pInput`: input parameters. Must not be `NULL`.
- `pDecision`: Output parameter. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: Evaluation successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: input or decision is empty, or the enumeration value is invalid.

**Resource ownership:**

The decision string is borrowed; the function does not allocate resources that need to be released by the caller.

**Additional Note:**

- `XWORK_APPROVAL_ALWAYS` will force approval.
- `XWORK_APPROVAL_NEVER` will skip approval, but additional gates can still be added to the product layer.
- `XWORK_APPROVAL_DEFAULT` will combine autonomy, side effect and risk threshold judgment.

**Example code:**

```c
xwork_policy_options policy;
xwork_approval_eval_input input;
xwork_approval_decision decision;

xwork_policy_options_init(&policy);
xwork_approval_eval_input_init(&input);
xwork_approval_decision_init(&decision);
input.eAutonomy = XWORK_AUTONOMY_SEMI_AUTO;
input.eSideEffect = XWORK_SIDE_EFFECT_WORKSPACE_WRITE;

(void)xwork_policy_evaluate_approval(&policy, &input, &decision);
```

**Related API:**

- `xwork_run_submit_approval`
- `xwork_policy_evaluate_network_access`

---

### xwork_policy_evaluate_network_access

Evaluate whether a network access is allowed.

**Function:**

Generate network access decisions based on network requested flag, URL/host, denylist, allowlist, and default deny policy.

**Function prototype:**

```c
XWORK_API xwork_status xwork_policy_evaluate_network_access(
    const xwork_policy_options *pPolicy,
    const xwork_network_policy_eval_input *pInput,
    xwork_network_policy_decision *pDecision
);
```

**parameter:**

- `pPolicy`: input parameters. Can be `NULL`; `NULL` uses the default policy.
- `pInput`: input parameters. Must not be `NULL`.
- `pDecision`: Output parameter. Must not be `NULL`.

**Return value:**

- `XWORK_OK`: Evaluation successful.
- `XWORK_ERROR_INVALID_ARGUMENT`: input or decision is empty.

**Resource ownership:**

The decision string is borrowed; the function does not allocate resources that need to be released by the caller.

**Additional Note:**

- deny patterns take precedence over allow patterns.
- After configuring allow patterns, the host must match the allowlist.
- Depends on `bDenyNetworkByDefault` when allow patterns is not configured.

**Example code:**

```c
xwork_network_policy_eval_input input;
xwork_network_policy_decision decision;

xwork_network_policy_eval_input_init(&input);
xwork_network_policy_decision_init(&decision);
input.bNetworkAccessRequested = true;
input.sHost = "api.example.com";

(void)xwork_policy_evaluate_network_access(NULL, &input, &decision);
```

**Related API:**

- `xwork_policy_options_init`
- `xwork_policy_evaluate_approval`

## Error handling

- `XWORK_ERROR_INVALID_ARGUMENT`: Invalid policy/input/decision pointer or invalid enum.
- `XWORK_ERROR_INVALID_STATE`: The current status of run cannot be submitted for approval.
- `XWORK_ERROR_NOT_FOUND`: There are no approval requests to submit.

## Restore boundaries

The approval request can be restored as an audit object by running snapshot/persistence. Policy callbacks, UI states, user sessions, and account permissions are not within the scope of xwork recovery.

## Thread boundaries

The policy evaluate function does not modify global state; concurrency safety depends on whether the caller concurrently modifies the incoming policy/input storage.

## Related documents

- [Tool API](api-tools.md)
- [Run API](api-run.md)
- [Orchestrator API](api-orchestrator.md)
- [Tools, Approval, and Artifacts](../guide/tool-approval-artifact-intro.md)
- [Internal policy contract](../../dev/docs/POLICY_APPROVAL.md)
