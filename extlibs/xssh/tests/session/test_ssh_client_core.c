#include "../test.h"



/* 为无网络测试直接提交双方 identification transcript。 */
static void testSshClientCoreVersions(xsshsessiontcp* pSession)
{
	xsshsessioncore* pCore = xrtSshSessionTcpCore(pSession);
	xsshtransportcore* pTransport = xrtSshTransportTcpCore(
		xrtSshSessionTcpTransport(pSession)
	);

	testRequire((pCore != NULL) && (pTransport != NULL),
		"ssh client core version access failed");
	testRequire(xrtSshSessionCoreVersionPrepare(
		pCore,
		pTransport,
		XSSH_TRANSPORT_LOCAL,
		XRT_STR_LITERAL("SSH-2.0-client_core")
	) == XSSH_OK,
		"ssh client local version prepare failed");
	testRequire(xrtSshTransportCoreIdentificationCommit(
		pTransport,
		XSSH_TRANSPORT_LOCAL
	) == XSSH_OK, "ssh client local transport version commit failed");
	testRequire(xrtSshSessionCoreVersionCommit(
		pCore,
		pTransport
	) == XSSH_OK, "ssh client local transcript commit failed");
	testRequire(xrtSshSessionCoreVersionPrepare(
		pCore,
		pTransport,
		XSSH_TRANSPORT_PEER,
		XRT_STR_LITERAL("SSH-2.0-server_core")
	) == XSSH_OK, "ssh client peer version prepare failed");
	testRequire(xrtSshTransportCoreIdentificationCommit(
		pTransport,
		XSSH_TRANSPORT_PEER
	) == XSSH_OK, "ssh client peer transport version commit failed");
	testRequire(xrtSshSessionCoreVersionCommit(
		pCore,
		pTransport
	) == XSSH_OK, "ssh client peer transcript commit failed");
}



/* 验证默认配置、动态 KEXINIT 输出和无隐藏网络所有权。 */
static void testSshClientCoreNext(void)
{
	xsshclientcoreconfig ClientConfig;
	xsshsessiontcpconfig SessionConfig;
	xsshclientnext Next;
	xsshsessionreader Reader;
	xsshclientcore Client;
	xsshsessiontcp Session;
	xsshkexinit KexInit;
	xnetbufpool* pPool;

	testRequire(xrtSshClientCoreConfigInit(&ClientConfig) &&
		(ClientConfig.Kex.Role == XSSH_ROLE_CLIENT) &&
		ClientConfig.Kex.Initial && ClientConfig.ProbeNone &&
		(ClientConfig.OutputInitial ==
		 XSSH_CLIENT_OUTPUT_INITIAL_DEFAULT) &&
		(ClientConfig.OutputLimit ==
		 XSSH_CLIENT_OUTPUT_LIMIT_DEFAULT),
		"ssh client core defaults failed");
	ClientConfig.Version = XRT_STR_LITERAL("SSH-2.0-client_core");
	ClientConfig.User = XRT_STR_LITERAL("alice");
	ClientConfig.OutputInitial = 32u;
	ClientConfig.OutputLimit = 4096u;
	pPool = xrtNetBufPoolCreate(NULL);
	testRequire((pPool != NULL) && xrtSshSessionTcpConfigInit(
		&SessionConfig,
		XSSH_ROLE_CLIENT
	) && xrtSshSessionTcpInit(
		&Session,
		pPool,
		&SessionConfig,
		0u
	) && xrtSshSessionReaderInit(
		&Reader,
		pPool,
		&Session
	) && xrtSshClientCoreInit(
		&Client,
		&ClientConfig
	) && (Client.Output == NULL) && (Client.OutputCapacity == 0u),
		"ssh client core initialization failed");

	testRequire((xrtSshClientCoreNext(
		&Client,
		&Session,
		&Reader,
		0u,
		&Next
	) == XSSH_OK) && (Next.Kind == XSSH_CLIENT_NEXT_IDENTIFICATION) &&
		testSshTextEqual(Next.Text, ClientConfig.Version) &&
		(Next.Data.Size == 0u), "ssh client identification action failed");

	testSshClientCoreVersions(&Session);
	testRequire((xrtSshClientCoreNext(
		&Client,
		&Session,
		&Reader,
		1u,
		&Next
	) == XSSH_OK) && (Next.Kind == XSSH_CLIENT_NEXT_PAYLOAD) &&
		(Next.Data.Data == Client.Output) &&
		(Next.Data.Size > ClientConfig.OutputInitial) &&
		(Client.OutputCapacity > ClientConfig.OutputInitial) &&
		(xrtSshKexInitRead(Next.Data, &KexInit) == XSSH_OK) &&
		xrtSshNameListContains(
			KexInit.KexAlgorithms,
			XRT_STR_LITERAL("curve25519-sha256")
		), "ssh client dynamic KEXINIT action failed");

	xrtSshClientCoreClear(&Client);
	xrtSshSessionReaderClear(&Reader);
	xrtSshSessionTcpClear(&Session);
	xrtNetBufPoolDestroy(pPool);
}



/* 验证 password helper 的方法选择、边界和最终协议编码。 */
static void testSshClientPassword(void)
{
	unsigned char arrPayload[256];
	xsshclientauth Auth;
	xsshauthpassword PasswordMessage;
	xsshwriter Writer;
	xstrview Password = XRT_STR_LITERAL("correct horse battery staple");

	memset(&Auth, 0, sizeof(Auth));
	Auth.User = XRT_STR_LITERAL("alice");
	Auth.Methods = XRT_STR_LITERAL("publickey,password");
	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshClientPasswordAuth(
		NULL,
		&Writer,
		&Auth,
		&Password
	) == XSSH_OK) && (xrtSshAuthPasswordRead(
		(xbytesview){ arrPayload, Writer.Size },
		&PasswordMessage
	) == XSSH_OK) && testSshTextEqual(
		PasswordMessage.User,
		Auth.User
	) && testSshTextEqual(
		PasswordMessage.Password,
		Password
	), "ssh client password helper failed");

	Auth.Methods = XRT_STR_LITERAL("publickey");
	testRequire(xrtSshWriterInit(
		&Writer,
		arrPayload,
		sizeof(arrPayload)
	) && (xrtSshClientPasswordAuth(
		NULL,
		&Writer,
		&Auth,
		&Password
	) == XSSH_ERROR_AUTHENTICATION) &&
		xrtSshWriterInit(&Writer, arrPayload, 1u),
		"ssh client password method filter failed");
	Auth.Methods = (xstrview){ NULL, 0u };
	testRequire(xrtSshClientPasswordAuth(
		NULL,
		&Writer,
		&Auth,
		&Password
	) == XSSH_ERROR_SPACE, "ssh client password space boundary failed");
}



/* 验证无效动态预算不会创建半初始化核心。 */
static void testSshClientCoreBoundaries(void)
{
	xsshclientcoreconfig Config;
	xsshclientcore Client;

	testRequire(xrtSshClientCoreConfigInit(&Config),
		"ssh client boundary defaults failed");
	Config.OutputInitial = 0u;
	testRequire(!xrtSshClientCoreInit(&Client, &Config),
		"ssh client accepted zero output capacity");
	Config.OutputInitial = 128u;
	Config.OutputLimit = 64u;
	testRequire(!xrtSshClientCoreInit(&Client, &Config),
		"ssh client accepted inverted output budget");
	xrtSshClientCoreClear(&Client);
}



/* 运行客户端动作核心回归。 */
int main(void)
{
	testSshClientCoreNext();
	testSshClientPassword();
	testSshClientCoreBoundaries();
	return 0;
}
