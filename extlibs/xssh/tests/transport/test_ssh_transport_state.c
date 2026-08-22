#include "../test.h"

#define TEST_SSH_KEX_ECDH_INIT 30u
#define TEST_SSH_KEX_ECDH_REPLY 31u



/* 构造不拥有 payload 的 KEXINIT/协商测试视图。 */
static void testSshTransportKexViews(
	xsshrole Role,
	xstrview ClientAlgorithms,
	xstrview ServerAlgorithms,
	bool bLocalFollows,
	bool bPeerFollows,
	xsshkexinit* pLocal,
	xsshkexinit* pPeer,
	xsshkexnegotiation* pNegotiation
)
{
	xsshkexinit Client;
	xsshkexinit Server;

	memset(&Client, 0, sizeof(Client));
	memset(&Server, 0, sizeof(Server));
	memset(pNegotiation, 0, sizeof(*pNegotiation));
	Client.KexAlgorithms = ClientAlgorithms;
	Server.KexAlgorithms = ServerAlgorithms;
	Client.ServerHostKeyAlgorithms = XRT_STR_LITERAL("ssh-ed25519");
	Server.ServerHostKeyAlgorithms = XRT_STR_LITERAL("ssh-ed25519");
	Client.EncryptionClientToServer = XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	Client.EncryptionServerToClient = XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	Server.EncryptionClientToServer = XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	Server.EncryptionServerToClient = XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	Client.MacClientToServer = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	Client.MacServerToClient = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	Server.MacClientToServer = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	Server.MacServerToClient = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	Client.CompressionClientToServer =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	Client.CompressionServerToClient =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	Server.CompressionClientToServer =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	Server.CompressionServerToClient =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	Client.FirstKexPacketFollows = Role == XSSH_ROLE_CLIENT ?
		bLocalFollows : bPeerFollows;
	Server.FirstKexPacketFollows = Role == XSSH_ROLE_SERVER ?
		bLocalFollows : bPeerFollows;
	testRequire(xrtSshKexNegotiate(
		&Client,
		&Server,
		pNegotiation
	) == XSSH_OK, "ssh transport test negotiation failed");
	if ( Role == XSSH_ROLE_CLIENT ) {
		*pLocal = Client;
		*pPeer = Server;
	} else {
		*pLocal = Server;
		*pPeer = Client;
	}
}



/* 完成双向 identification。 */
static void testSshTransportIdentify(xsshtransportstate* pState)
{
	testRequire((xrtSshTransportIdentificationCommit(
		pState,
		XSSH_TRANSPORT_LOCAL
	) == XSSH_OK) && (xrtSshTransportIdentificationCommit(
		pState,
		XSSH_TRANSPORT_PEER
	) == XSSH_OK) &&
		(pState->Phase == XSSH_TRANSPORT_KEY_EXCHANGE),
		"ssh transport identification failed");
}



/* 通过通用规则 API 配置当前 ECDH 单次 init/reply。 */
static bool testSshTransportKexRulesEcdh(
	xsshtransportkexrules* pRules,
	xsshrole Role
)
{
	if ( !xrtSshTransportKexRulesInit(pRules) ) {
		return false;
	}
	return Role == XSSH_ROLE_CLIENT ?
		xrtSshTransportKexRuleSet(
			pRules,
			XSSH_TRANSPORT_LOCAL,
			TEST_SSH_KEX_ECDH_INIT,
			1u
		) && xrtSshTransportKexRuleSet(
			pRules,
			XSSH_TRANSPORT_PEER,
			TEST_SSH_KEX_ECDH_REPLY,
			1u
		) : xrtSshTransportKexRuleSet(
			pRules,
			XSSH_TRANSPORT_LOCAL,
			TEST_SSH_KEX_ECDH_REPLY,
			1u
		) && xrtSshTransportKexRuleSet(
			pRules,
			XSSH_TRANSPORT_PEER,
			TEST_SSH_KEX_ECDH_INIT,
			1u
		);
}



/* 配置双方默认 strict KEXINIT 和 ECDH 方法规则。 */
static void testSshTransportConfigureDefault(
	xsshtransportstate* pState,
	xsshrole Role,
	bool bInitial
)
{
	xsshtransportkexrules Rules;
	xsshkexinit Local;
	xsshkexinit Peer;
	xsshkexnegotiation Negotiation;
	xstrview ClientAlgorithms = bInitial ?
		XRT_STR_LITERAL(XSSH_KEX_CLIENT_INITIAL_DEFAULT) :
		XRT_STR_LITERAL(XSSH_KEX_DEFAULT);
	xstrview ServerAlgorithms = bInitial ?
		XRT_STR_LITERAL(XSSH_KEX_SERVER_INITIAL_DEFAULT) :
		XRT_STR_LITERAL(XSSH_KEX_DEFAULT);

	testSshTransportKexViews(
		Role,
		ClientAlgorithms,
		ServerAlgorithms,
		false,
		false,
		&Local,
		&Peer,
		&Negotiation
	);
	testRequire(testSshTransportKexRulesEcdh(&Rules, Role) &&
		(xrtSshTransportKexConfigure(
			pState,
			&Local,
			&Peer,
			&Negotiation,
			&Rules
		) == XSSH_OK), "ssh transport KEX configure failed");
}



/* 完成 client 方向的一代 ECDH 和双向 NEWKEYS。 */
static void testSshTransportFinishClientKex(
	xsshtransportstate* pState,
	uint32* pLocalActions,
	uint32* pPeerActions
)
{
	testRequire((xrtSshTransportMessageCommit(
		pState,
		XSSH_TRANSPORT_LOCAL,
		TEST_SSH_KEX_ECDH_INIT
	) == XSSH_OK) && (xrtSshTransportMessageCommit(
		pState,
		XSSH_TRANSPORT_PEER,
		TEST_SSH_KEX_ECDH_REPLY
	) == XSSH_OK) && (xrtSshTransportNewKeysCommit(
		pState,
		XSSH_TRANSPORT_LOCAL,
		pLocalActions
	) == XSSH_OK) && (xrtSshTransportNewKeysCommit(
		pState,
		XSSH_TRANSPORT_PEER,
		pPeerActions
	) == XSSH_OK), "ssh transport KEX completion failed");
}



/* 验证初始 strict KEX、方向密钥边界和每次 rekey 的序列重置动作。 */
static void testSshTransportStrictLifecycle(void)
{
	xsshtransportstate State;
	uint32 iLocalActions;
	uint32 iPeerActions;

	testRequire(xrtSshTransportStateInit(&State, XSSH_ROLE_CLIENT),
		"ssh transport init failed");
	testSshTransportIdentify(&State);
	testRequire((xrtSshTransportKexInitCommit(
		&State,
		XSSH_TRANSPORT_LOCAL,
		false
	) == XSSH_OK) && (xrtSshTransportKexInitCommit(
		&State,
		XSSH_TRANSPORT_PEER,
		false
	) == XSSH_OK), "ssh transport initial KEXINIT failed");
	testSshTransportConfigureDefault(&State, XSSH_ROLE_CLIENT, true);
	testRequire(State.Strict && State.AcceptExtInfo && State.SendExtInfo &&
		(xrtSshTransportNewKeysCheck(
			&State,
			XSSH_TRANSPORT_LOCAL
		) == XSSH_ERROR_STATE) && (xrtSshTransportMessageCommit(
			&State,
			XSSH_TRANSPORT_PEER,
			TEST_SSH_KEX_ECDH_INIT
		) == XSSH_ERROR_PROTOCOL),
		"ssh transport method quota was not enforced");
	testRequire((xrtSshTransportMessageCommit(
		&State,
		XSSH_TRANSPORT_LOCAL,
		TEST_SSH_KEX_ECDH_INIT
	) == XSSH_OK) && (xrtSshTransportMessageCommit(
		&State,
		XSSH_TRANSPORT_LOCAL,
		TEST_SSH_KEX_ECDH_INIT
	) == XSSH_ERROR_STATE) && (xrtSshTransportMessageCommit(
		&State,
		XSSH_TRANSPORT_PEER,
		TEST_SSH_KEX_ECDH_REPLY
	) == XSSH_OK), "ssh transport exact KEX count failed");
	testRequire((xrtSshTransportNewKeysCommit(
		&State,
		XSSH_TRANSPORT_LOCAL,
		&iLocalActions
	) == XSSH_OK) && (iLocalActions ==
		(XSSH_TRANSPORT_ACTION_ACTIVATE_KEYS |
		 XSSH_TRANSPORT_ACTION_RESET_SEQUENCE)) &&
		xrtSshTransportCanApplication(&State, XSSH_TRANSPORT_LOCAL) &&
		!xrtSshTransportCanApplication(&State, XSSH_TRANSPORT_PEER) &&
		(xrtSshTransportNewKeysCommit(
			&State,
			XSSH_TRANSPORT_PEER,
			&iPeerActions
		) == XSSH_OK) && (iPeerActions ==
		(XSSH_TRANSPORT_ACTION_ACTIVATE_KEYS |
		 XSSH_TRANSPORT_ACTION_RESET_SEQUENCE |
		 XSSH_TRANSPORT_ACTION_KEX_COMPLETE)) &&
		(State.KexCount == 1u) && (State.Phase == XSSH_TRANSPORT_OPEN),
		"ssh transport initial NEWKEYS actions failed");

	testRequire((xrtSshTransportKexInitCommit(
		&State,
		XSSH_TRANSPORT_PEER,
		false
	) == XSSH_OK) && xrtSshTransportKexReplyNeeded(&State) &&
		xrtSshTransportCanApplication(&State, XSSH_TRANSPORT_LOCAL) &&
		(xrtSshTransportMessageCheck(
			&State,
			XSSH_TRANSPORT_PEER,
			94u
		) == XSSH_ERROR_PROTOCOL) && (xrtSshTransportKexInitCommit(
			&State,
			XSSH_TRANSPORT_LOCAL,
			false
		) == XSSH_OK), "ssh transport directional rekey gate failed");
	testSshTransportConfigureDefault(&State, XSSH_ROLE_CLIENT, false);
	testSshTransportFinishClientKex(
		&State,
		&iLocalActions,
		&iPeerActions
	);
	testRequire((State.KexCount == 2u) &&
		((iLocalActions & XSSH_TRANSPORT_ACTION_RESET_SEQUENCE) != 0u) &&
		((iPeerActions & XSSH_TRANSPORT_ACTION_RESET_SEQUENCE) != 0u) &&
		(xrtSshTransportMessageCheck(
			&State,
			XSSH_TRANSPORT_LOCAL,
			TEST_SSH_KEX_ECDH_INIT
		) == XSSH_ERROR_STATE),
		"ssh strict rekey did not reset both sequence directions");
}



/* 验证 strict 协商会追溯拒绝 KEXINIT 前的非 KEX packet。 */
static void testSshTransportStrictFirstPacket(void)
{
	xsshtransportstate State;
	xsshtransportkexrules Rules;
	xsshkexinit Local;
	xsshkexinit Peer;
	xsshkexnegotiation Negotiation;

	testRequire(xrtSshTransportStateInit(&State, XSSH_ROLE_CLIENT),
		"ssh strict first-packet init failed");
	testSshTransportIdentify(&State);
	testRequire((xrtSshTransportMessageCommit(
		&State,
		XSSH_TRANSPORT_PEER,
		XSSH_MSG_IGNORE
	) == XSSH_OK) && (xrtSshTransportKexInitCommit(
		&State,
		XSSH_TRANSPORT_LOCAL,
		false
	) == XSSH_OK) && (xrtSshTransportKexInitCommit(
		&State,
		XSSH_TRANSPORT_PEER,
		false
	) == XSSH_OK) && testSshTransportKexRulesEcdh(
		&Rules,
		XSSH_ROLE_CLIENT
	), "ssh strict first-packet setup failed");
	testSshTransportKexViews(
		XSSH_ROLE_CLIENT,
		XRT_STR_LITERAL(XSSH_KEX_CLIENT_INITIAL_DEFAULT),
		XRT_STR_LITERAL(XSSH_KEX_SERVER_INITIAL_DEFAULT),
		false,
		false,
		&Local,
		&Peer,
		&Negotiation
	);
	testRequire((xrtSshTransportKexConfigure(
		&State,
		&Local,
		&Peer,
		&Negotiation,
		&Rules
	) == XSSH_ERROR_PROTOCOL) && !State.KexConfigured,
		"ssh strict KEX accepted a non-first peer KEXINIT");
}



/* 验证未协商 strict 时保留 RFC 4253 generic 消息兼容路径。 */
static void testSshTransportNonStrictInitial(void)
{
	xsshtransportstate State;
	xsshtransportkexrules Rules;
	xsshkexinit Local;
	xsshkexinit Peer;
	xsshkexnegotiation Negotiation;
	uint32 iLocalActions;
	uint32 iPeerActions;

	testRequire(xrtSshTransportStateInit(&State, XSSH_ROLE_CLIENT),
		"ssh non-strict init failed");
	testSshTransportIdentify(&State);
	testRequire((xrtSshTransportMessageCommit(
		&State,
		XSSH_TRANSPORT_LOCAL,
		XSSH_MSG_IGNORE
	) == XSSH_OK) && (xrtSshTransportMessageCommit(
		&State,
		XSSH_TRANSPORT_PEER,
		XSSH_MSG_DEBUG
	) == XSSH_OK) && (xrtSshTransportKexInitCommit(
		&State,
		XSSH_TRANSPORT_LOCAL,
		false
	) == XSSH_OK) && (xrtSshTransportKexInitCommit(
		&State,
		XSSH_TRANSPORT_PEER,
		false
	) == XSSH_OK) && testSshTransportKexRulesEcdh(
		&Rules,
		XSSH_ROLE_CLIENT
	), "ssh non-strict setup failed");
	testSshTransportKexViews(
		XSSH_ROLE_CLIENT,
		XRT_STR_LITERAL(XSSH_KEX_DEFAULT),
		XRT_STR_LITERAL(XSSH_KEX_DEFAULT),
		false,
		false,
		&Local,
		&Peer,
		&Negotiation
	);
	testRequire((xrtSshTransportKexConfigure(
		&State,
		&Local,
		&Peer,
		&Negotiation,
		&Rules
	) == XSSH_OK) && !State.Strict &&
		(xrtSshTransportMessageCommit(
			&State,
			XSSH_TRANSPORT_LOCAL,
			XSSH_MSG_DEBUG
		) == XSSH_OK), "ssh non-strict generic message was rejected");
	testSshTransportFinishClientKex(
		&State,
		&iLocalActions,
		&iPeerActions
	);
	testRequire(((iLocalActions &
		XSSH_TRANSPORT_ACTION_RESET_SEQUENCE) == 0u) &&
		((iPeerActions & XSSH_TRANSPORT_ACTION_RESET_SEQUENCE) == 0u),
		"ssh non-strict KEX requested sequence reset");
}



/* 验证 strict 初始 KEX 不允许下一次 packet 使 uint32 序列回绕。 */
static void testSshTransportStrictSequenceLimit(void)
{
	xsshtransportstate State;

	testRequire(xrtSshTransportStateInit(&State, XSSH_ROLE_CLIENT),
		"ssh strict sequence init failed");
	testSshTransportIdentify(&State);
	testRequire((xrtSshTransportKexInitCommit(
		&State,
		XSSH_TRANSPORT_LOCAL,
		false
	) == XSSH_OK) && (xrtSshTransportKexInitCommit(
		&State,
		XSSH_TRANSPORT_PEER,
		false
	) == XSSH_OK), "ssh strict sequence KEXINIT failed");
	testSshTransportConfigureDefault(&State, XSSH_ROLE_CLIENT, true);
	State.LocalPackets = UINT32_MAX;
	State.PeerPackets = UINT32_MAX;
	testRequire((xrtSshTransportMessageCheck(
		&State,
		XSSH_TRANSPORT_LOCAL,
		TEST_SSH_KEX_ECDH_INIT
	) == XSSH_ERROR_STATE) && (xrtSshTransportMessageCheck(
		&State,
		XSSH_TRANSPORT_PEER,
		TEST_SSH_KEX_ECDH_REPLY
	) == XSSH_ERROR_PROTOCOL),
		"ssh strict initial sequence wrap was accepted");
}



/* 验证错误 guessed packet 被计入序列但不消费实际方法额度。 */
static void testSshTransportWrongGuess(void)
{
	xsshtransportstate State;
	xsshtransportkexrules Rules;
	xsshkexinit Local;
	xsshkexinit Peer;
	xsshkexnegotiation Negotiation;
	uint32 iActions;

	testRequire(xrtSshTransportStateInit(&State, XSSH_ROLE_SERVER),
		"ssh guessed packet init failed");
	testSshTransportIdentify(&State);
	testRequire((xrtSshTransportKexInitCommit(
		&State,
		XSSH_TRANSPORT_PEER,
		true
	) == XSSH_OK) && (xrtSshTransportMessageCommit(
		&State,
		XSSH_TRANSPORT_PEER,
		TEST_SSH_KEX_ECDH_INIT
	) == XSSH_OK) && (xrtSshTransportKexInitCommit(
		&State,
		XSSH_TRANSPORT_LOCAL,
		false
	) == XSSH_OK) && testSshTransportKexRulesEcdh(
		&Rules,
		XSSH_ROLE_SERVER
	), "ssh guessed packet setup failed");
	testSshTransportKexViews(
		XSSH_ROLE_SERVER,
		XRT_STR_LITERAL(
			"wrong-kex,curve25519-sha256," \
			XSSH_KEX_EXT_INFO_CLIENT "," XSSH_KEX_STRICT_CLIENT
		),
		XRT_STR_LITERAL(
			XSSH_KEX_SERVER_INITIAL_DEFAULT
		),
		false,
		true,
		&Local,
		&Peer,
		&Negotiation
	);
	testRequire((xrtSshTransportKexConfigure(
		&State,
		&Local,
		&Peer,
		&Negotiation,
		&Rules
	) == XSSH_OK) && State.PeerGuessSkip &&
		(State.PeerKexRemaining[0] == 1u) &&
		(xrtSshTransportNewKeysCheck(
			&State,
			XSSH_TRANSPORT_PEER
		) == XSSH_ERROR_PROTOCOL) && (xrtSshTransportMessageCommit(
			&State,
			XSSH_TRANSPORT_PEER,
			TEST_SSH_KEX_ECDH_INIT
		) == XSSH_OK) && (xrtSshTransportMessageCommit(
			&State,
			XSSH_TRANSPORT_LOCAL,
			TEST_SSH_KEX_ECDH_REPLY
		) == XSSH_OK) && (xrtSshTransportNewKeysCommit(
			&State,
			XSSH_TRANSPORT_PEER,
			&iActions
		) == XSSH_OK), "ssh wrong guessed packet consumed real quota");
}



/* 验证 transport 不接受与双方清单不一致的外部协商结果。 */
static void testSshTransportNegotiationValidation(void)
{
	xsshtransportstate State;
	xsshtransportstate Keep;
	xsshtransportkexrules Rules;
	xsshkexinit Local;
	xsshkexinit Peer;
	xsshkexnegotiation Negotiation;

	testRequire(xrtSshTransportStateInit(&State, XSSH_ROLE_CLIENT),
		"ssh negotiation validation init failed");
	testSshTransportIdentify(&State);
	testRequire((xrtSshTransportKexInitCommit(
		&State,
		XSSH_TRANSPORT_LOCAL,
		false
	) == XSSH_OK) && (xrtSshTransportKexInitCommit(
		&State,
		XSSH_TRANSPORT_PEER,
		false
	) == XSSH_OK) && testSshTransportKexRulesEcdh(
		&Rules,
		XSSH_ROLE_CLIENT
	), "ssh negotiation validation setup failed");
	testSshTransportKexViews(
		XSSH_ROLE_CLIENT,
		XRT_STR_LITERAL(XSSH_KEX_CLIENT_INITIAL_DEFAULT),
		XRT_STR_LITERAL(XSSH_KEX_SERVER_INITIAL_DEFAULT),
		false,
		false,
		&Local,
		&Peer,
		&Negotiation
	);
	Negotiation.KexAlgorithm = XRT_STR_LITERAL("unoffered-kex");
	Keep = State;
	testRequire((xrtSshTransportKexConfigure(
		&State,
		&Local,
		&Peer,
		&Negotiation,
		&Rules
	) == XSSH_ERROR_ARGUMENT) &&
		(memcmp(&State, &Keep, sizeof(State)) == 0),
		"ssh transport accepted or published forged negotiation");
	testSshTransportKexViews(
		XSSH_ROLE_CLIENT,
		XRT_STR_LITERAL(XSSH_KEX_CLIENT_INITIAL_DEFAULT),
		XRT_STR_LITERAL(XSSH_KEX_SERVER_INITIAL_DEFAULT),
		false,
		false,
		&Local,
		&Peer,
		&Negotiation
	);
	Negotiation.KexAlgorithm.Data = NULL;
	Negotiation.KexAlgorithm.Size = 1u;
	testRequire((xrtSshTransportKexConfigure(
		&State,
		&Local,
		&Peer,
		&Negotiation,
		&Rules
	) == XSSH_ERROR_ARGUMENT) &&
		(memcmp(&State, &Keep, sizeof(State)) == 0),
		"ssh transport accepted malformed negotiation view");
}



/* 验证首次和 server 认证成功前第二次 EXT_INFO 的严格相邻关系。 */
static void testSshTransportExtInfoOrder(void)
{
	xsshtransportstate State;
	uint32 iLocalActions;
	uint32 iPeerActions;

	testRequire(xrtSshTransportStateInit(&State, XSSH_ROLE_CLIENT),
		"ssh ext-info init failed");
	testSshTransportIdentify(&State);
	testRequire((xrtSshTransportKexInitCommit(
		&State,
		XSSH_TRANSPORT_LOCAL,
		false
	) == XSSH_OK) && (xrtSshTransportKexInitCommit(
		&State,
		XSSH_TRANSPORT_PEER,
		false
	) == XSSH_OK), "ssh ext-info KEXINIT failed");
	testSshTransportConfigureDefault(&State, XSSH_ROLE_CLIENT, true);
	testSshTransportFinishClientKex(
		&State,
		&iLocalActions,
		&iPeerActions
	);
	testRequire((xrtSshTransportMessageCommit(
		&State,
		XSSH_TRANSPORT_LOCAL,
		XSSH_MSG_EXT_INFO
	) == XSSH_OK) && State.LocalFirstExtUsed &&
		(xrtSshTransportMessageCommit(
			&State,
			XSSH_TRANSPORT_LOCAL,
			XSSH_MSG_EXT_INFO
		) == XSSH_ERROR_STATE),
		"ssh client EXT_INFO opportunity was not single-use");
	testRequire((xrtSshTransportMessageCommit(
		&State,
		XSSH_TRANSPORT_PEER,
		XSSH_MSG_SERVICE_ACCEPT
	) == XSSH_OK) && !State.PeerFirstExtOpen &&
		(xrtSshTransportMessageCommit(
			&State,
			XSSH_TRANSPORT_PEER,
			XSSH_MSG_EXT_INFO
		) == XSSH_OK) && State.PeerAuthSuccessPending &&
		(xrtSshTransportMessageCommit(
			&State,
			XSSH_TRANSPORT_PEER,
			52u
		) == XSSH_ERROR_PROTOCOL) && (xrtSshTransportAuthSuccessCommit(
			&State,
			XSSH_TRANSPORT_PEER
		) == XSSH_OK) && !State.PeerAuthSuccessPending &&
		(xrtSshTransportAuthSuccessCommit(
			&State,
			XSSH_TRANSPORT_PEER
		) == XSSH_ERROR_PROTOCOL) &&
		(xrtSshTransportMessageCommit(
			&State,
			XSSH_TRANSPORT_PEER,
			XSSH_MSG_EXT_INFO
		) == XSSH_ERROR_PROTOCOL),
		"ssh server secondary EXT_INFO order failed");
}



/* 验证非法参数、动作输出和关闭状态保持失败原子性。 */
static void testSshTransportBoundary(void)
{
	xsshtransportstate State;
	xsshtransportstate Keep;
	uint32 iActions = UINT32_C(0x5a5a5a5a);

	testRequire(xrtSshTransportStateInit(&State, XSSH_ROLE_SERVER),
		"ssh transport boundary init failed");
	Keep = State;
	testRequire((xrtSshTransportKexInitCommit(
		&State,
		XSSH_TRANSPORT_LOCAL,
		false
	) == XSSH_ERROR_STATE) &&
		(memcmp(&State, &Keep, sizeof(State)) == 0) &&
		(xrtSshTransportNewKeysCommit(
			&State,
			XSSH_TRANSPORT_LOCAL,
			&iActions
		) == XSSH_ERROR_STATE) &&
		(iActions == UINT32_C(0x5a5a5a5a)),
		"ssh transport failure changed state or action output");
	xrtSshTransportClose(&State);
	testRequire((State.Phase == XSSH_TRANSPORT_CLOSED) &&
		(xrtSshTransportMessageCommit(
			&State,
			XSSH_TRANSPORT_LOCAL,
			XSSH_MSG_IGNORE
		) == XSSH_ERROR_STATE), "ssh transport closed state accepted packet");
	xrtSshTransportStateClear(&State);
	testRequire(State.Guard == 0u, "ssh transport clear retained guard");
}



/* 运行 SSH transport 状态、strict-kex、rekey 和 EXT_INFO 边界测试。 */
int main(void)
{
	testSshTransportStrictLifecycle();
	testSshTransportStrictFirstPacket();
	testSshTransportNonStrictInitial();
	testSshTransportStrictSequenceLimit();
	testSshTransportWrongGuess();
	testSshTransportNegotiationValidation();
	testSshTransportExtInfoOrder();
	testSshTransportBoundary();
	return 0;
}
