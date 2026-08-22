#include "../test.h"



/* 验证默认值与调用方单调时间起点。 */
static void testSshAuthGuardDefaults(void)
{
	xsshauthguardpolicy Policy;
	xsshauthguard Guard;
	xsshauthguarddecision Decision;

	xrtSshAuthGuardPolicyInit(&Policy);
	testRequire((Policy.TimeoutMs == XSSH_AUTH_DEFAULT_TIMEOUT_MS) &&
		(Policy.ByteLimit == XSSH_AUTH_DEFAULT_BYTE_LIMIT) &&
		(Policy.AttemptLimit == XSSH_AUTH_DEFAULT_ATTEMPT_LIMIT) &&
		(Policy.RoundLimit == XSSH_AUTH_DEFAULT_ROUND_LIMIT) &&
		(Policy.MessageLimit == XSSH_AUTH_DEFAULT_MESSAGE_LIMIT),
		"ssh auth guard defaults mismatch");
	testRequire(xrtSshAuthGuardInit(&Guard, NULL, 1000u) &&
		(xrtSshAuthGuardCheck(&Guard, 999u, &Decision) == XSSH_OK) &&
		(Decision == XSSH_AUTH_GUARD_ALLOW),
		"ssh auth guard rejected monotonic clock rollback");
}



/* 验证尝试上限允许恰好达到限制并拒绝下一项。 */
static void testSshAuthGuardAttempts(void)
{
	xsshauthguardpolicy Policy = { 0 };
	xsshauthguard Guard;
	xsshauthguarddecision Decision;

	Policy.AttemptLimit = 2u;
	testRequire(xrtSshAuthGuardInit(&Guard, &Policy, 0u),
		"ssh auth attempt guard init failed");
	testRequire((xrtSshAuthGuardReserve(
		&Guard,
		XSSH_AUTH_EVENT_ATTEMPT,
		10u,
		1u,
		&Decision
	) == XSSH_OK) && (Decision == XSSH_AUTH_GUARD_ALLOW) &&
		(xrtSshAuthGuardReserve(
			&Guard,
			XSSH_AUTH_EVENT_ATTEMPT,
			20u,
			2u,
			&Decision
		) == XSSH_OK) && (Decision == XSSH_AUTH_GUARD_ALLOW),
		"ssh auth guard rejected allowed attempts");
	testRequire((xrtSshAuthGuardReserve(
		&Guard,
		XSSH_AUTH_EVENT_ATTEMPT,
		30u,
		3u,
		&Decision
	) == XSSH_OK) && (Decision == XSSH_AUTH_GUARD_DISCONNECT) &&
		(Guard.Exhaustion == XSSH_AUTH_EXHAUST_ATTEMPTS) &&
		(Guard.Attempts == 3u) && (Guard.Messages == 3u) &&
		(Guard.Bytes == 60u),
		"ssh auth guard attempt exhaustion mismatch");
	testRequire(!xrtSshAuthGuardComplete(&Guard),
		"ssh auth guard completed after exhaustion");
}



/* 验证轮次、消息和字节预算彼此独立。 */
static void testSshAuthGuardResources(void)
{
	xsshauthguardpolicy Policy = { 0 };
	xsshauthguard Guard;
	xsshauthguarddecision Decision;

	Policy.RoundLimit = 1u;
	Policy.MessageLimit = 2u;
	Policy.ByteLimit = 8u;
	testRequire(xrtSshAuthGuardInit(&Guard, &Policy, 0u) &&
		(xrtSshAuthGuardReserve(
			&Guard,
			XSSH_AUTH_EVENT_ROUND,
			3u,
			0u,
			&Decision
		) == XSSH_OK) && (Decision == XSSH_AUTH_GUARD_ALLOW) &&
		(xrtSshAuthGuardReserve(
			&Guard,
			XSSH_AUTH_EVENT_MESSAGE,
			5u,
			0u,
			&Decision
		) == XSSH_OK) && (Decision == XSSH_AUTH_GUARD_ALLOW),
		"ssh auth resource guard rejected exact limits");
	testRequire((xrtSshAuthGuardReserve(
		&Guard,
		XSSH_AUTH_EVENT_MESSAGE,
		0u,
		0u,
		&Decision
	) == XSSH_OK) && (Decision == XSSH_AUTH_GUARD_DISCONNECT) &&
		(Guard.Exhaustion == XSSH_AUTH_EXHAUST_MESSAGES),
		"ssh auth message exhaustion mismatch");

	Policy.MessageLimit = 0u;
	Policy.RoundLimit = 0u;
	Policy.ByteLimit = 8u;
	testRequire(xrtSshAuthGuardInit(&Guard, &Policy, 0u) &&
		(xrtSshAuthGuardReserve(
			&Guard,
			XSSH_AUTH_EVENT_MESSAGE,
			9u,
			0u,
			&Decision
		) == XSSH_OK) && (Decision == XSSH_AUTH_GUARD_DISCONNECT) &&
		(Guard.Exhaustion == XSSH_AUTH_EXHAUST_BYTES),
		"ssh auth byte exhaustion mismatch");
}



/* 验证超时、成功冻结和非法事件的事务语义。 */
static void testSshAuthGuardState(void)
{
	xsshauthguardpolicy Policy = { 0 };
	xsshauthguard Guard;
	xsshauthguard Snapshot;
	xsshauthguarddecision Decision;

	Policy.TimeoutMs = 100u;
	testRequire(xrtSshAuthGuardInit(&Guard, &Policy, 1000u) &&
		(xrtSshAuthGuardCheck(&Guard, 1099u, &Decision) == XSSH_OK) &&
		(Decision == XSSH_AUTH_GUARD_ALLOW) &&
		(xrtSshAuthGuardCheck(&Guard, 1100u, &Decision) == XSSH_OK) &&
		(Decision == XSSH_AUTH_GUARD_DISCONNECT) &&
		(Guard.Exhaustion == XSSH_AUTH_EXHAUST_TIMEOUT),
		"ssh auth timeout boundary mismatch");

	Policy.TimeoutMs = 0u;
	testRequire(xrtSshAuthGuardInit(&Guard, &Policy, 0u) &&
		xrtSshAuthGuardComplete(&Guard) &&
		(xrtSshAuthGuardReserve(
			&Guard,
			XSSH_AUTH_EVENT_ATTEMPT,
			100u,
			UINT64_MAX,
			&Decision
		) == XSSH_OK) && (Decision == XSSH_AUTH_GUARD_IGNORE) &&
		(Guard.Messages == 0u),
		"ssh auth completed guard did not ignore message");

	testRequire(xrtSshAuthGuardInit(&Guard, &Policy, 0u),
		"ssh auth invalid-event setup failed");
	Snapshot = Guard;
	testRequire((xrtSshAuthGuardReserve(
		&Guard,
		(xsshauthevent)99,
		1u,
		0u,
		&Decision
	) == XSSH_ERROR_ARGUMENT) &&
		(memcmp(&Guard, &Snapshot, sizeof(Guard)) == 0),
		"ssh auth invalid event changed state");
}



/* 运行认证资源预算与完成状态测试。 */
int main(void)
{
	testSshAuthGuardDefaults();
	testSshAuthGuardAttempts();
	testSshAuthGuardResources();
	testSshAuthGuardState();
	return 0;
}
