#include "../test.h"



/* 验证默认策略对应字节、时间、包和块安全阈值。 */
static void testSshRekeyDefaults(void)
{
	xsshrekeypolicy Policy;
	xsshrekeystate State;
	xsshrekeydecision Decision = XSSH_REKEY_REQUIRED;

	xrtSshRekeyPolicyInit(&Policy);
	testRequire((Policy.ByteLimit == XSSH_REKEY_DEFAULT_BYTE_LIMIT) &&
		(Policy.SendPacketLimit == XSSH_REKEY_DEFAULT_SEND_PACKET_LIMIT) &&
		(Policy.ReceivePacketLimit ==
			XSSH_REKEY_DEFAULT_RECEIVE_PACKET_LIMIT) &&
		(Policy.BlockLimit == XSSH_REKEY_DEFAULT_BLOCK_LIMIT) &&
		(Policy.TimeLimitMs == XSSH_REKEY_DEFAULT_TIME_LIMIT_MS) &&
		(Policy.HardPacketLimit == XSSH_REKEY_HARD_PACKET_LIMIT),
		"ssh rekey default policy mismatch");
	testRequire(xrtSshRekeyInit(&State, NULL, 100u) &&
		(xrtSshRekeyCheck(&State, 100u, &Decision) == XSSH_OK) &&
		(Decision == XSSH_REKEY_NONE), "ssh rekey default init failed");
	testRequire((xrtSshRekeyCheck(
		&State,
		100u + XSSH_REKEY_DEFAULT_TIME_LIMIT_MS,
		&Decision
	) == XSSH_OK) && (Decision == XSSH_REKEY_RECOMMENDED),
		"ssh rekey time threshold failed");
}



/* 验证双向软阈值、主动请求和新密钥重置。 */
static void testSshRekeySoftLimits(void)
{
	xsshrekeypolicy Policy;
	xsshrekeystate State;
	xsshrekeydecision Decision;

	xrtSshRekeyPolicyInit(&Policy);
	Policy.ByteLimit = 100u;
	Policy.SendPacketLimit = 3u;
	Policy.ReceivePacketLimit = 2u;
	Policy.BlockLimit = 10u;
	Policy.TimeLimitMs = 1000u;
	Policy.HardPacketLimit = 8u;
	testRequire(xrtSshRekeyInit(&State, &Policy, 10u),
		"ssh rekey custom init failed");
	testRequire((xrtSshRekeyReserveSend(
		&State,
		40u,
		3u,
		20u,
		&Decision
	) == XSSH_OK) && (Decision == XSSH_REKEY_NONE) &&
		(State.Sent.Packets == 1u) && (State.Sent.Bytes == 40u),
		"ssh rekey first send failed");
	testRequire((xrtSshRekeyReserveReceive(
		&State,
		60u,
		4u,
		30u,
		&Decision
	) == XSSH_OK) && (Decision == XSSH_REKEY_NONE),
		"ssh rekey first receive failed");
	testRequire((xrtSshRekeyReserveReceive(
		&State,
		1u,
		1u,
		40u,
		&Decision
	) == XSSH_OK) && (Decision == XSSH_REKEY_RECOMMENDED),
		"ssh rekey receive packet threshold failed");
	testRequire(xrtSshRekeyRequest(&State) &&
		(xrtSshRekeyCheck(&State, 40u, &Decision) == XSSH_OK) &&
		(Decision == XSSH_REKEY_RECOMMENDED),
		"ssh rekey manual request failed");
	testRequire(xrtSshRekeyReset(&State, 500u) &&
		(State.Sent.Packets == 0u) && (State.Received.Packets == 0u) &&
		!State.Requested && (State.Policy.ByteLimit == 100u) &&
		(xrtSshRekeyCheck(&State, 500u, &Decision) == XSSH_OK) &&
		(Decision == XSSH_REKEY_NONE), "ssh rekey reset failed");
}



/* 验证两个 NEWKEYS 边界分别开始新代且不会互相清除计数。 */
static void testSshRekeyDirectionalReset(void)
{
	xsshrekeypolicy Policy;
	xsshrekeystate State;
	xsshrekeydecision Decision;

	memset(&Policy, 0, sizeof(Policy));
	Policy.TimeLimitMs = 100u;
	Policy.HardPacketLimit = 10u;
	testRequire(xrtSshRekeyInit(&State, &Policy, 10u) &&
		(xrtSshRekeyReserveSend(
			&State,
			20u,
			2u,
			20u,
			&Decision
		) == XSSH_OK) && (xrtSshRekeyReserveReceive(
			&State,
			30u,
			3u,
			30u,
			&Decision
		) == XSSH_OK) && xrtSshRekeyRequest(&State),
		"ssh rekey directional setup failed");
	testRequire(xrtSshRekeyResetSend(&State, 50u) &&
		(State.Sent.Packets == 0u) &&
		(State.Received.Packets == 1u) &&
		(State.SendStartedMs == 50u) &&
		(State.ReceiveStartedMs == 10u) && State.Requested &&
		(xrtSshRekeyReserveSend(
			&State,
			40u,
			4u,
			55u,
			&Decision
		) == XSSH_OK) && (State.Sent.Packets == 1u),
		"ssh rekey send reset damaged receive generation");
	testRequire(xrtSshRekeyResetReceive(&State, 60u) &&
		(State.Sent.Packets == 1u) &&
		(State.Received.Packets == 0u) &&
		(State.SendStartedMs == 50u) &&
		(State.ReceiveStartedMs == 60u) && State.Requested &&
		xrtSshRekeyComplete(&State) && !State.Requested,
		"ssh rekey receive reset damaged send generation");
	testRequire((xrtSshRekeyCheck(
		&State,
		149u,
		&Decision
	) == XSSH_OK) && (Decision == XSSH_REKEY_NONE) &&
		(xrtSshRekeyCheck(
			&State,
			150u,
			&Decision
		) == XSSH_OK) && (Decision == XSSH_REKEY_RECOMMENDED),
		"ssh rekey directional time generation failed");
}



/* 验证允许完整使用硬额度，但不会登记第一个越界包。 */
static void testSshRekeyHardLimit(void)
{
	xsshrekeypolicy Policy;
	xsshrekeystate State;
	xsshrekeystate Keep;
	xsshrekeydecision Decision;
	size_t i;

	memset(&Policy, 0, sizeof(Policy));
	Policy.HardPacketLimit = 3u;
	testRequire(xrtSshRekeyInit(&State, &Policy, 0u),
		"ssh rekey hard init failed");
	for ( i = 0u; i < 3u; ++i ) {
		testRequire((xrtSshRekeyReserveSend(
			&State,
			1u,
			0u,
			0u,
			&Decision
		) == XSSH_OK) && ((i < 2u) ?
			(Decision == XSSH_REKEY_NONE) :
			(Decision == XSSH_REKEY_RECOMMENDED)),
			"ssh rekey hard budget reservation failed");
	}
	Keep = State;
	testRequire((xrtSshRekeyReserveSend(
		&State,
		1u,
		0u,
		0u,
		&Decision
	) == XSSH_OK) && (Decision == XSSH_REKEY_REQUIRED) &&
		(memcmp(&State, &Keep, sizeof(State)) == 0),
		"ssh rekey hard limit changed state");
}



/* 验证饱和计数和非法策略不会回绕或发布输出。 */
static void testSshRekeyBoundary(void)
{
	xsshrekeypolicy Policy;
	xsshrekeystate State;
	xsshrekeystate Keep;
	xsshrekeydecision Decision;
	xsshrekeydecision KeepDecision = XSSH_REKEY_REQUIRED;

	memset(&Policy, 0, sizeof(Policy));
	Policy.ByteLimit = UINT64_MAX;
	Policy.BlockLimit = UINT64_MAX;
	Policy.HardPacketLimit = 4u;
	testRequire(xrtSshRekeyInit(&State, &Policy, 0u) &&
		(xrtSshRekeyReserveSend(
			&State,
			UINT64_MAX,
			UINT64_MAX,
			0u,
			&Decision
		) == XSSH_OK) && (Decision == XSSH_REKEY_RECOMMENDED) &&
		(State.Sent.Bytes == UINT64_MAX) &&
		(State.Sent.Blocks == UINT64_MAX),
		"ssh rekey saturating counter failed");
	testRequire((xrtSshRekeyReserveSend(
		&State,
		1u,
		1u,
		0u,
		&Decision
	) == XSSH_OK) && (State.Sent.Bytes == UINT64_MAX) &&
		(State.Sent.Blocks == UINT64_MAX),
		"ssh rekey saturating counter wrapped");

	Policy.HardPacketLimit = XSSH_REKEY_HARD_PACKET_LIMIT + 1u;
	Keep = State;
	Decision = KeepDecision;
	testRequire(!xrtSshRekeyInit(&State, &Policy, 0u) &&
		(memcmp(&State, &Keep, sizeof(State)) == 0),
		"ssh rekey invalid policy changed state");
	testRequire((xrtSshRekeyCheck(NULL, 0u, &Decision) ==
		XSSH_ERROR_ARGUMENT) && (Decision == KeepDecision),
		"ssh rekey invalid check changed output");
	testRequire(xrtSshRekeyCheck(
		&State,
		0u,
		(xsshrekeydecision*)&State.Sent
	) == XSSH_ERROR_ARGUMENT, "ssh rekey accepted overlapping output");
}



/* 运行 SSH rekey 策略与计数边界测试。 */
int main(void)
{
	testSshRekeyDefaults();
	testSshRekeySoftLimits();
	testSshRekeyDirectionalReset();
	testSshRekeyHardLimit();
	testSshRekeyBoundary();
	return 0;
}
