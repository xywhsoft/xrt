#include "../test.h"



/* 使用固定 cookie 构建并解析一个 KEXINIT。 */
static void testSshKexInitRoundtrip(void)
{
	unsigned char arrCookie[XSSH_KEX_COOKIE_SIZE];
	unsigned char arrPayload[512];
	xsshkexinitconfig Config;
	xsshkexinit KexInit;
	xsshwriter Writer;
	size_t i;

	for ( i = 0u; i < sizeof(arrCookie); ++i ) {
		arrCookie[i] = (uint8)i;
	}
	testRequire(xrtSshKexInitConfigInit(
		&Config,
		XSSH_ROLE_CLIENT,
		true
	) && xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexInitWrite(
			&Writer,
			(xbytesview){ arrCookie, sizeof(arrCookie) },
			&Config
		) == XSSH_OK) && (arrPayload[0] == XSSH_MSG_KEXINIT) &&
		(xrtSshKexInitRead(
			(xbytesview){ arrPayload, Writer.Size },
			&KexInit
		) == XSSH_OK) &&
		testSshBytesEqual(
			KexInit.Cookie,
			(xbytesview){ arrCookie, sizeof(arrCookie) }
		) && testSshTextEqual(
			KexInit.KexAlgorithms,
			XRT_STR_LITERAL(XSSH_KEX_CLIENT_INITIAL_DEFAULT)
		) && testSshTextEqual(
			KexInit.ServerHostKeyAlgorithms,
			XRT_STR_LITERAL(XSSH_HOSTKEY_DEFAULT)
		) && testSshTextEqual(
			KexInit.EncryptionClientToServer,
			XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT)
		) && testSshTextEqual(
			KexInit.CompressionServerToClient,
			XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT)
		) && (KexInit.LanguagesClientToServer.Size == 0u) &&
		!KexInit.FirstKexPacketFollows,
		"ssh kexinit default roundtrip failed");
}



/* 构建一端的自定义 KEXINIT 并返回借用 payload 的解析结果。 */
static size_t testSshKexInitBuild(
	unsigned char* pOutput,
	size_t iCapacity,
	xstrview Kex,
	xstrview HostKey,
	xstrview Cipher,
	xstrview Mac,
	bool bFollows,
	xsshkexinit* pKexInit
)
{
	unsigned char arrCookie[XSSH_KEX_COOKIE_SIZE] = { 0u };
	xsshkexinitconfig Config;
	xsshwriter Writer;

	if ( !xrtSshKexInitConfigInit(
		&Config,
		XSSH_ROLE_CLIENT,
		false
	) ) {
		return 0u;
	}
	Config.KexAlgorithms = Kex;
	Config.ServerHostKeyAlgorithms = HostKey;
	Config.EncryptionClientToServer = Cipher;
	Config.EncryptionServerToClient = Cipher;
	Config.MacClientToServer = Mac;
	Config.MacServerToClient = Mac;
	Config.FirstKexPacketFollows = bFollows;
	if ( !xrtSshWriterInit(&Writer, pOutput, iCapacity) ||
		(xrtSshKexInitWrite(
			&Writer,
			(xbytesview){ arrCookie, sizeof(arrCookie) },
			&Config
		) != XSSH_OK) || (xrtSshKexInitRead(
			(xbytesview){ pOutput, Writer.Size },
			pKexInit
		) != XSSH_OK) ) {
		return 0u;
	}
	return Writer.Size;
}



/* 验证客户端优先级、AEAD MAC 省略和 guessed packet 规则。 */
static void testSshKexNegotiation(void)
{
	unsigned char arrClient[512];
	unsigned char arrServer[512];
	xsshkexinit Client;
	xsshkexinit Server;
	xsshkexnegotiation Negotiation;
	bool bSkip = false;

	testRequire((testSshKexInitBuild(
		arrClient,
		sizeof(arrClient),
		XRT_STR_LITERAL("curve25519-sha256,test-kex"),
		XRT_STR_LITERAL("ssh-ed25519,rsa-sha2-256"),
		XRT_STR_LITERAL("aes128-gcm@openssh.com,aes256-gcm@openssh.com"),
		XRT_STR_LITERAL("client-only-mac"),
		false,
		&Client
	) != 0u) && (testSshKexInitBuild(
		arrServer,
		sizeof(arrServer),
		XRT_STR_LITERAL("test-kex,curve25519-sha256"),
		XRT_STR_LITERAL("rsa-sha2-256,ssh-ed25519"),
		XRT_STR_LITERAL("aes256-gcm@openssh.com,aes128-gcm@openssh.com"),
		XRT_STR_LITERAL("server-only-mac"),
		true,
		&Server
	) != 0u), "ssh kex negotiation setup failed");
	testRequire((xrtSshKexNegotiate(
		&Client,
		&Server,
		&Negotiation
	) == XSSH_OK) && testSshTextEqual(
		Negotiation.KexAlgorithm,
		XRT_STR_LITERAL("curve25519-sha256")
	) && testSshTextEqual(
		Negotiation.ServerHostKeyAlgorithm,
		XRT_STR_LITERAL("ssh-ed25519")
	) && testSshTextEqual(
		Negotiation.CipherClientToServer,
		XRT_STR_LITERAL("aes128-gcm@openssh.com")
	) && (Negotiation.MacClientToServer.Data == NULL) &&
		(Negotiation.MacClientToServer.Size == 0u) &&
		xrtSshCipherIsAead(Negotiation.CipherClientToServer),
		"ssh kex AEAD negotiation mismatch");
	testRequire((xrtSshKexGuessSkip(
		&Server,
		&Negotiation,
		&bSkip
	) == XSSH_OK) && bSkip,
		"ssh kex wrong guess was not skipped");
	testRequire((xrtSshKexGuessSkip(
		&Client,
		&Negotiation,
		&bSkip
	) == XSSH_OK) && !bSkip,
		"ssh kex disabled guess requested skip");
	Client.FirstKexPacketFollows = true;
	Client.KexAlgorithms = XRT_STR_LITERAL(
		XSSH_KEX_EXT_INFO_CLIENT ",curve25519-sha256,test-kex"
	);
	testRequire((xrtSshKexGuessSkip(
		&Client,
		&Negotiation,
		&bSkip
	) == XSSH_OK) && !bSkip,
		"ssh kex indicator changed guessed algorithm");
}



/* 验证首次 KEX 的角色标记、扩展方向和重协商清单。 */
static void testSshKexFeatures(void)
{
	unsigned char arrCookie[XSSH_KEX_COOKIE_SIZE] = { 0u };
	unsigned char arrClient[512];
	unsigned char arrServer[512];
	xsshkexinitconfig ClientConfig;
	xsshkexinitconfig ServerConfig;
	xsshkexinit Client;
	xsshkexinit Server;
	xsshkexinit BadPeer;
	xsshkexfeatures Features;
	xsshkexfeatures Keep;
	xsshwriter Writer;

	testRequire(xrtSshKexInitConfigInit(
		&ClientConfig,
		XSSH_ROLE_CLIENT,
		true
	) && xrtSshKexInitConfigInit(
		&ServerConfig,
		XSSH_ROLE_SERVER,
		true
	) && xrtSshWriterInit(&Writer, arrClient, sizeof(arrClient)) &&
		(xrtSshKexInitWrite(
			&Writer,
			(xbytesview){ arrCookie, sizeof(arrCookie) },
			&ClientConfig
		) == XSSH_OK) && (xrtSshKexInitRead(
			(xbytesview){ arrClient, Writer.Size },
			&Client
		) == XSSH_OK), "ssh client KEX feature setup failed");
	testRequire(xrtSshWriterInit(&Writer, arrServer, sizeof(arrServer)) &&
		(xrtSshKexInitWrite(
			&Writer,
			(xbytesview){ arrCookie, sizeof(arrCookie) },
			&ServerConfig
		) == XSSH_OK) && (xrtSshKexInitRead(
			(xbytesview){ arrServer, Writer.Size },
			&Server
		) == XSSH_OK), "ssh server KEX feature setup failed");
	testRequire((xrtSshKexFeatures(
		&Client,
		&Server,
		XSSH_ROLE_CLIENT,
		true,
		&Features
	) == XSSH_OK) && Features.AcceptExtInfo && Features.SendExtInfo &&
		Features.Strict, "ssh initial KEX features mismatch");

	Client.KexAlgorithms = XRT_STR_LITERAL(
		XSSH_KEX_DEFAULT "," XSSH_KEX_STRICT_CLIENT
	);
	Server.KexAlgorithms = XRT_STR_LITERAL(
		XSSH_KEX_DEFAULT "," XSSH_KEX_STRICT_SERVER
	);
	testRequire((xrtSshKexFeatures(
		&Client,
		&Server,
		XSSH_ROLE_CLIENT,
		true,
		&Features
	) == XSSH_OK) && Features.Strict,
		"ssh standard strict markers did not match");
	Server.KexAlgorithms = XRT_STR_LITERAL(
		XSSH_KEX_DEFAULT "," XSSH_KEX_STRICT_SERVER_PRE_STANDARD
	);
	testRequire((xrtSshKexFeatures(
		&Client,
		&Server,
		XSSH_ROLE_CLIENT,
		true,
		&Features
	) == XSSH_OK) && !Features.Strict,
		"ssh cross-generation strict markers matched");

	testRequire(xrtSshKexInitConfigInit(
		&ClientConfig,
		XSSH_ROLE_CLIENT,
		false
	) && testSshTextEqual(
		ClientConfig.KexAlgorithms,
		XRT_STR_LITERAL(XSSH_KEX_DEFAULT)
	), "ssh rekey default retained initial indicators");
	Client.KexAlgorithms = ClientConfig.KexAlgorithms;
	Server.KexAlgorithms = XRT_STR_LITERAL(
		XSSH_KEX_DEFAULT "," XSSH_KEX_EXT_INFO_SERVER "," \
		XSSH_KEX_STRICT_SERVER
	);
	testRequire((xrtSshKexFeatures(
		&Client,
		&Server,
		XSSH_ROLE_CLIENT,
		false,
		&Features
	) == XSSH_OK) && !Features.AcceptExtInfo && !Features.SendExtInfo &&
		!Features.Strict, "ssh rekey features were not empty");
	Client.KexAlgorithms = XRT_STR_LITERAL(XSSH_KEX_SERVER_INITIAL_DEFAULT);
	Server.KexAlgorithms = XRT_STR_LITERAL(XSSH_KEX_CLIENT_INITIAL_DEFAULT);
	testRequire((xrtSshKexFeatures(
		&Client,
		&Server,
		XSSH_ROLE_CLIENT,
		false,
		&Features
	) == XSSH_OK) && !Features.AcceptExtInfo && !Features.SendExtInfo &&
		!Features.Strict, "ssh rekey did not ignore initial-only markers");
	Client.KexAlgorithms = ClientConfig.KexAlgorithms;
	Server.KexAlgorithms = XRT_STR_LITERAL(XSSH_KEX_SERVER_INITIAL_DEFAULT);

	BadPeer = Server;
	BadPeer.KexAlgorithms = XRT_STR_LITERAL(XSSH_KEX_CLIENT_INITIAL_DEFAULT);
	memset(&Keep, 0x5a, sizeof(Keep));
	Features = Keep;
	testRequire((xrtSshKexFeatures(
		&Client,
		&BadPeer,
		XSSH_ROLE_CLIENT,
		true,
		&Features
	) == XSSH_ERROR_PROTOCOL) &&
		(memcmp(&Features, &Keep, sizeof(Features)) == 0),
		"ssh peer role indicator changed feature output");

	ClientConfig.Initial = true;
	ClientConfig.KexAlgorithms = XRT_STR_LITERAL(
		XSSH_KEX_DEFAULT "," XSSH_KEX_EXT_INFO_SERVER
	);
	testRequire(xrtSshWriterInit(&Writer, arrClient, sizeof(arrClient)) &&
		(xrtSshKexInitWrite(
			&Writer,
			(xbytesview){ arrCookie, sizeof(arrCookie) },
			&ClientConfig
		) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh local wrong-role indicator was accepted");
	ClientConfig.Initial = false;
	ClientConfig.KexAlgorithms =
		XRT_STR_LITERAL(XSSH_KEX_CLIENT_INITIAL_DEFAULT);
	testRequire(xrtSshKexInitWrite(
		&Writer,
		(xbytesview){ arrCookie, sizeof(arrCookie) },
		&ClientConfig
	) == XSSH_ERROR_ARGUMENT, "ssh rekey accepted initial indicators");
}



/* 验证非 AEAD cipher 仍严格协商双向 MAC。 */
static void testSshKexMacNegotiation(void)
{
	unsigned char arrClient[384];
	unsigned char arrServer[384];
	xsshkexinit Client;
	xsshkexinit Server;
	xsshkexnegotiation Negotiation;

	testRequire((testSshKexInitBuild(
		arrClient,
		sizeof(arrClient),
		XRT_STR_LITERAL("test-kex"),
		XRT_STR_LITERAL("test-host"),
		XRT_STR_LITERAL("aes128-ctr"),
		XRT_STR_LITERAL("hmac-sha2-512,hmac-sha2-256"),
		false,
		&Client
	) != 0u) && (testSshKexInitBuild(
		arrServer,
		sizeof(arrServer),
		XRT_STR_LITERAL("test-kex"),
		XRT_STR_LITERAL("test-host"),
		XRT_STR_LITERAL("aes128-ctr"),
		XRT_STR_LITERAL("hmac-sha2-256,hmac-sha2-512"),
		false,
		&Server
	) != 0u) && (xrtSshKexNegotiate(
		&Client,
		&Server,
		&Negotiation
	) == XSSH_OK) && testSshTextEqual(
		Negotiation.MacClientToServer,
		XRT_STR_LITERAL("hmac-sha2-512")
	) && !xrtSshCipherIsAead(Negotiation.CipherClientToServer),
		"ssh kex MAC negotiation mismatch");
}



/* 验证写入容量和解析错误均不发布部分状态。 */
static void testSshKexInitFailureAtomic(void)
{
	unsigned char arrCookie[XSSH_KEX_COOKIE_SIZE] = { 0u };
	unsigned char arrPayload[512];
	xsshkexinitconfig Config;
	xsshkexinit KexInit;
	xsshkexinit Keep;
	xsshwriter Writer;
	size_t iPayloadSize;

	testRequire(xrtSshKexInitConfigInit(
		&Config,
		XSSH_ROLE_CLIENT,
		true
	), "ssh kexinit config setup failed");
	memset(arrPayload, 0x5a, sizeof(arrPayload));
	testRequire(xrtSshWriterInit(&Writer, arrPayload, 8u) &&
		(xrtSshKexInitWrite(
			&Writer,
			(xbytesview){ arrCookie, sizeof(arrCookie) },
			&Config
		) == XSSH_ERROR_SPACE) && (Writer.Size == 0u) &&
		(arrPayload[0] == 0x5au), "ssh kexinit short write was partial");
	testRequire(xrtSshKexInitWrite(
		&Writer,
		(xbytesview){ arrCookie, sizeof(arrCookie) - 1u },
		&Config
	) == XSSH_ERROR_ARGUMENT, "ssh kexinit accepted short cookie");
	testRequire(xrtSshKexInitWrite(
		&Writer,
		(xbytesview){ arrCookie, sizeof(arrCookie) },
		NULL
	) == XSSH_ERROR_ARGUMENT, "ssh kexinit accepted missing role");
	Config.KexAlgorithms = XRT_STR_LITERAL("bad,,list");
	testRequire(xrtSshKexInitWrite(
		&Writer,
		(xbytesview){ arrCookie, sizeof(arrCookie) },
		&Config
	) == XSSH_ERROR_ARGUMENT, "ssh kexinit accepted invalid list");

	testRequire(xrtSshKexInitConfigInit(
		&Config,
		XSSH_ROLE_CLIENT,
		true
	), "ssh kexinit overlap setup failed");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexInitWrite(
			&Writer,
			(xbytesview){ arrPayload + 32u, sizeof(arrCookie) },
			&Config
		) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh kexinit accepted overlapping cookie");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshKexInitWrite(
			&Writer,
			(xbytesview){ arrCookie, sizeof(arrCookie) },
			&Config
		) == XSSH_OK), "ssh kexinit invalid input setup failed");
	iPayloadSize = Writer.Size;
	memset(&Keep, 0x5a, sizeof(Keep));
	KexInit = Keep;
	arrPayload[0] = 0xffu;
	testRequire((xrtSshKexInitRead(
		(xbytesview){ arrPayload, iPayloadSize },
		&KexInit
	) == XSSH_ERROR_PROTOCOL) &&
		(memcmp(&KexInit, &Keep, sizeof(KexInit)) == 0),
		"ssh kexinit wrong message changed output");
	arrPayload[0] = XSSH_MSG_KEXINIT;
	arrPayload[iPayloadSize - 1u] = 1u;
	testRequire((xrtSshKexInitRead(
		(xbytesview){ arrPayload, iPayloadSize },
		&KexInit
	) == XSSH_ERROR_PROTOCOL) &&
		(memcmp(&KexInit, &Keep, sizeof(KexInit)) == 0),
		"ssh kexinit reserved field was accepted");
	arrPayload[iPayloadSize - 1u] = 0u;
	arrPayload[1u + XSSH_KEX_COOKIE_SIZE + 4u] = ',';
	testRequire(xrtSshKexInitRead(
		(xbytesview){ arrPayload, iPayloadSize },
		&KexInit
	) == XSSH_ERROR_PROTOCOL, "ssh kexinit invalid name-list was accepted");
	testRequire(xrtSshKexInitRead(
		(xbytesview){ arrPayload, 3u },
		&KexInit
	) == XSSH_NEED_MORE, "ssh kexinit truncation result mismatch");
}



/* 验证无共同算法时输出保持不变。 */
static void testSshKexNoMatchAtomic(void)
{
	unsigned char arrClient[384];
	unsigned char arrServer[384];
	xsshkexinit Client;
	xsshkexinit Server;
	xsshkexnegotiation Negotiation;
	xsshkexnegotiation Keep;

	testRequire((testSshKexInitBuild(
		arrClient,
		sizeof(arrClient),
		XRT_STR_LITERAL("client-kex"),
		XRT_STR_LITERAL("host"),
		XRT_STR_LITERAL("cipher"),
		XRT_STR_LITERAL("mac"),
		false,
		&Client
	) != 0u) && (testSshKexInitBuild(
		arrServer,
		sizeof(arrServer),
		XRT_STR_LITERAL("server-kex"),
		XRT_STR_LITERAL("host"),
		XRT_STR_LITERAL("cipher"),
		XRT_STR_LITERAL("mac"),
		false,
		&Server
	) != 0u), "ssh kex no-match setup failed");
	memset(&Keep, 0x5a, sizeof(Keep));
	Negotiation = Keep;
	testRequire((xrtSshKexNegotiate(
		&Client,
		&Server,
		&Negotiation
	) == XSSH_ERROR_UNSUPPORTED) &&
		(memcmp(&Negotiation, &Keep, sizeof(Negotiation)) == 0),
		"ssh kex no-match changed output");
	Client.KexAlgorithms = XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_CLIENT);
	Server.KexAlgorithms = XRT_STR_LITERAL(XSSH_KEX_EXT_INFO_CLIENT);
	Negotiation = Keep;
	testRequire((xrtSshKexNegotiate(
		&Client,
		&Server,
		&Negotiation
	) == XSSH_ERROR_PROTOCOL) &&
		(memcmp(&Negotiation, &Keep, sizeof(Negotiation)) == 0),
		"ssh extension indicator was negotiated as an algorithm");
}



/* 运行 KEXINIT 构建、解析与协商测试。 */
int main(void)
{
	testSshKexInitRoundtrip();
	testSshKexNegotiation();
	testSshKexFeatures();
	testSshKexMacNegotiation();
	testSshKexInitFailureAtomic();
	testSshKexNoMatchAtomic();
	return 0;
}
