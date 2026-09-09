# Profile 选择

> 状态：中文初稿，待审阅。

xwork 内置 `xcode` 和 `xclaw` 两个 profile，用于给 AI IDE 和自主 Agent 提供默认策略。profile 是基线配置，不是产品最终策略。

## 选择建议

| 常量 | Profile | 适合场景 | 默认倾向 |
| --- | --- | --- |
| `XWORK_PROFILE_XCODE` | `xcode` | AI IDE、需要用户审阅的代码修改 | 半自动、低风险自动审批、默认不启用 workspace memory、网络默认拒绝。 |
| `XWORK_PROFILE_XCLAW` | `xclaw` | 命令行自主 Agent、长任务执行 | 更高自主性、可启用 workspace memory、planner boundary 开启、网络仍默认拒绝。 |

## 使用顺序

建议先应用 profile，再由产品覆盖更严格策略：

```text
init options
get builtin profile
apply profile to runtime/workspace/run/orchestrator/xllm options
override product-specific security/model settings
create runtime/workspace/run
```

## 最小示例

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

## 注意事项

- profile 不替代产品级安全策略。
- 如果产品要求更严格审批，应在 apply profile 后覆盖 policy。
- network 默认建议保持 deny-by-default。
- 恢复 run 时，应重新应用兼容 profile，再加载 snapshot。

## 下一步

- [Profiles API](../api/api-profiles.md)
- [AI IDE Agent 范例](../case/ai-ide-agent.md)
- [claw 自主 Agent 范例](../case/claw-autonomous-agent.md)
