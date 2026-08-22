#include "../test.h"



/* 构造一份字段可辨识的 TLS 1.3 恢复配置。 */
static void testTlsResumeConfig(
	xtlsresumeconfig* pConfig,
	uint8* pTicket,
	uint8* pSecret,
	uint8* pProtocol,
	uint8* pPeer
)
{
	xrtTlsResumeConfigInit(pConfig);
	pConfig->Cipher = XTLS_AES_128_GCM_SHA256;
	pConfig->Ticket = (xbytesview) { pTicket, 5u };
	pConfig->Secret = (xbytesview) { pSecret, 32u };
	pConfig->ServerName = XRT_STR_LITERAL("api.example.test");
	pConfig->Protocol = (xbytesview) { pProtocol, 2u };
	pConfig->PeerIdentity = (xbytesview) { pPeer, 4u };
	pConfig->Lifetime = 60u;
	pConfig->AgeAdd = UINT32_C(0x10203040);
	pConfig->MaxEarlyData = 4096u;
	pConfig->IssuedAt = INT64_C(1700000000000000);
}



/* 恢复对象必须深拷贝所有材料并准确发布生命周期元数据。 */
static void testTlsResumeSnapshot(void)
{
	uint8 Ticket[5] = { 1, 2, 3, 4, 5 };
	uint8 Secret[32];
	uint8 Protocol[2] = { 'h', '2' };
	uint8 Peer[4] = { 9, 8, 7, 6 };
	xtlsresumeconfig Config;
	xtlsresumeinfo Info;
	xtlsresume* pResume;

	memset(Secret, 0x5a, sizeof(Secret));
	testTlsResumeConfig(&Config, Ticket, Secret, Protocol, Peer);
	pResume = xrtTlsResumeCreate(&Config);
	testRequire(pResume != NULL, "TLS resume snapshot creation failed");

	memset(Ticket, 0, sizeof(Ticket));
	memset(Secret, 0, sizeof(Secret));
	memset(Protocol, 0, sizeof(Protocol));
	memset(Peer, 0, sizeof(Peer));
	testRequire(xrtTlsResumeInfo(pResume, &Info),
		"TLS resume info query failed");
	testRequire((Info.Version == XTLS_VERSION_13) &&
		(Info.Cipher == XTLS_AES_128_GCM_SHA256) &&
		(Info.Lifetime == 60u) &&
		(Info.AgeAdd == UINT32_C(0x10203040)) &&
		(Info.MaxEarlyData == 4096u) &&
		(Info.IssuedAt == Config.IssuedAt) &&
		(Info.ExpiresAt == Config.IssuedAt + INT64_C(60000000)),
		"TLS resume metadata changed during snapshot creation");
	testRequire((Info.Ticket.Size == 5u) &&
		(Info.Ticket.Data[0] == 1u) &&
		(Info.Ticket.Data[4] == 5u) &&
		(Info.Secret.Size == 32u) &&
		(Info.Secret.Data[0] == 0x5au) &&
		(Info.Protocol.Size == 2u) &&
		(Info.Protocol.Data[0] == 'h') &&
		(Info.PeerIdentity.Size == 4u) &&
		(Info.PeerIdentity.Data[0] == 9u) &&
		(Info.ServerName.Size == 16u) &&
		(memcmp(Info.ServerName.Data, "api.example.test", 16u) == 0),
		"TLS resume borrowed caller-owned storage");

	testRequire(xrtTlsResumeRetain(pResume) == pResume,
		"TLS resume retain changed object identity");
	xrtTlsResumeRelease(pResume);
	testRequire(xrtTlsResumeInfo(pResume, &Info),
		"retained TLS resume did not remain alive");
	xrtTlsResumeRelease(pResume);
	xrtTlsResumeRelease(NULL);
}



/* 有效期使用半开区间，票据年龄按毫秒取整后执行协议规定的模加。 */
static void testTlsResumeAge(void)
{
	uint8 Ticket[5] = { 1, 2, 3, 4, 5 };
	uint8 Secret[32] = { 0 };
	uint8 Protocol[2] = { 'h', '2' };
	uint8 Peer[4] = { 9, 8, 7, 6 };
	xtlsresumeconfig Config;
	xtlsresume* pResume;
	uint32 iAge = 77u;

	testTlsResumeConfig(&Config, Ticket, Secret, Protocol, Peer);
	Config.Lifetime = 1u;
	Config.AgeAdd = UINT32_MAX;
	pResume = xrtTlsResumeCreate(&Config);
	testRequire(pResume != NULL, "TLS resume age setup failed");

	testRequire(!xrtTlsResumeValidAt(pResume, Config.IssuedAt - 1) &&
		xrtTlsResumeValidAt(pResume, Config.IssuedAt) &&
		xrtTlsResumeValidAt(
			pResume, Config.IssuedAt + INT64_C(999999)
		) && !xrtTlsResumeValidAt(
			pResume, Config.IssuedAt + INT64_C(1000000)
		), "TLS resume validity boundary is incorrect");
	testRequire(xrtTlsResumeTicketAge(
		pResume, Config.IssuedAt + 1999, &iAge
	) && (iAge == 0u), "TLS resume ticket age did not wrap modulo 32 bits");
	iAge = 77u;
	testRequire(!xrtTlsResumeTicketAge(
		pResume, Config.IssuedAt + INT64_C(1000000), &iAge
	) && (iAge == 77u), "expired TLS resume changed ticket age output");
	xrtTlsResumeRelease(pResume);
}



/* 配置验证必须拒绝可能产生歧义、越界或错误密钥长度的对象。 */
static void testTlsResumeInvalid(void)
{
	uint8 Ticket[5] = { 1, 2, 3, 4, 5 };
	uint8 Secret[32] = { 0 };
	uint8 Protocol[2] = { 'h', '2' };
	uint8 Peer[4] = { 9, 8, 7, 6 };
	char Name[4] = { 'a', '\0', 'b', 'c' };
	xtlsresumeconfig Config;
	xtlsresumeinfo Info;
	uint32 iAge = 41u;

	testTlsResumeConfig(&Config, Ticket, Secret, Protocol, Peer);
	Config.Version = XTLS_VERSION_12;
	testRequire(xrtTlsResumeCreate(&Config) == NULL,
		"TLS 1.2 resume was accepted before its contract exists");

	testTlsResumeConfig(&Config, Ticket, Secret, Protocol, Peer);
	Config.Cipher = XTLS_ECDHE_RSA_AES_128_GCM_SHA256;
	testRequire(xrtTlsResumeCreate(&Config) == NULL,
		"TLS 1.2 cipher was accepted by TLS 1.3 resume");

	testTlsResumeConfig(&Config, Ticket, Secret, Protocol, Peer);
	Config.Ticket.Size = 0;
	testRequire(xrtTlsResumeCreate(&Config) == NULL,
		"empty TLS resume ticket was accepted");

	testTlsResumeConfig(&Config, Ticket, Secret, Protocol, Peer);
	Config.Secret.Size = 31u;
	testRequire(xrtTlsResumeCreate(&Config) == NULL,
		"wrong TLS resume PSK length was accepted");

	testTlsResumeConfig(&Config, Ticket, Secret, Protocol, Peer);
	Config.Lifetime = XTLS13_TICKET_LIFETIME_MAX + 1u;
	testRequire(xrtTlsResumeCreate(&Config) == NULL,
		"oversized TLS resume lifetime was accepted");

	testTlsResumeConfig(&Config, Ticket, Secret, Protocol, Peer);
	Config.ServerName = (xstrview) { Name, sizeof(Name) };
	testRequire(xrtTlsResumeCreate(&Config) == NULL,
		"null byte in TLS resume server name was accepted");

	testTlsResumeConfig(&Config, Ticket, Secret, Protocol, Peer);
	Config.Protocol = (xbytesview) { Protocol, 256u };
	testRequire(xrtTlsResumeCreate(&Config) == NULL,
		"oversized TLS resume ALPN was accepted");

	testTlsResumeConfig(&Config, Ticket, Secret, Protocol, Peer);
	Config.IssuedAt = INT64_MAX;
	testRequire(xrtTlsResumeCreate(&Config) == NULL,
		"overflowing TLS resume expiration was accepted");

	memset(&Info, 0x6a, sizeof(Info));
	testRequire(!xrtTlsResumeInfo(NULL, &Info) &&
		((const uint8*)&Info)[0] == 0x6au,
		"invalid TLS resume info query changed output");
	testRequire(!xrtTlsResumeTicketAge(NULL, 0, &iAge) && (iAge == 41u),
		"invalid TLS resume age query changed output");
	xrtTlsResumeConfigInit(NULL);
}



/* 执行 TLS 恢复对象的正常、边界和错误路径回归。 */
int main(void)
{
	testTlsResumeSnapshot();
	testTlsResumeAge();
	testTlsResumeInvalid();
	return 0;
}
