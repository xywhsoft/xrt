# Profile selection

>Status: First draft in Chinese, awaiting review.

xwork has two built-in profiles, `xcode` and `xclaw`, which are used to provide default policies for AI IDE and autonomous Agents. profile is a baseline configuration, not the final product strategy.

## Select suggestions

| Constants | Profile | Suitable for scenarios | Default tendency |
| --- | --- | --- |
| `XWORK_PROFILE_XCODE` | |
| `XWORK_PROFILE_XCLAW` | |

## Order of use

It is recommended to apply the profile first, and then have the product cover the stricter policy:

```text
init options
get builtin profile
apply profile to runtime/workspace/run/orchestrator/xllm options
override product-specific security/model settings
create runtime/workspace/run
```

## Minimal example

```c
xwork_profile tProfile;
xwork_runtime_options tRuntime;
xwork_run_options tRun;

xwork_profile_init(&tProfile);
xwork_runtime_options_init(&tRuntime);
xwork_run_options_init(&tRun);

if (xwork_profile_get_builtin(XWORK_PROFILE_XCLAW, &tProfile) != XWORK_OK) {
    return 1;
}

xwork_profile_apply_runtime_options(&tProfile, &tRuntime);
xwork_profile_apply_run_options(&tProfile, &tRun);
```

## Notes

- profile does not replace product-level security policy.
- If the product requires stricter approval, the policy should be overridden after apply profile.
- network default recommendation is to keep deny-by-default.
- When resuming run, the compatible profile should be reapplied and the snapshot should be loaded again.

## Next step

- [Profiles API](../api/api-profiles.md)
- [AI IDE Agent Example](../case/ai-ide-agent.md)
- [claw Autonomous Agent Example](../case/claw-autonomous-agent.md)
