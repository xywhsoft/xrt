#include "../test.h"



static const char sKeyBase64[] =
	"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";



/* 解码公共夹具，并可选择改变最后一个公钥字节。 */
static size_t testSshKnownHostDbKey(
	unsigned char* pBlob,
	size_t iCapacity,
	bool bChanged
)
{
	size_t iSize = 0u;

	testRequire(xrtBase64Decode(
		sKeyBase64,
		sizeof(sKeyBase64) - 1u,
		pBlob,
		iCapacity,
		&iSize,
		NULL
	), "ssh known-host db fixture decode failed");
	if ( bChanged ) {
		pBlob[iSize - 1u] ^= 1u;
	}
	return iSize;
}



/* 验证游标跳过空行/注释，并让严格模式返回可定位坏行。 */
static void testSshKnownHostDbCursor(void)
{
	static const char sSource[] =
		"\n# comment\r\ninvalid line\n"
		"host.example ssh-ed25519 "
		"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n";
	xsshknownhostdb Database;
	xsshknownhostdb KeepDatabase;
	xsshknownhostentry Entry;
	xsshknownhostentry Keep;

	testRequire(xrtSshKnownHostDbInit(
		&Database,
		XRT_STR_LITERAL(sSource),
		0u
	) == XSSH_OK, "ssh lenient known-host database init failed");
	testRequire((xrtSshKnownHostDbNext(
		&Database,
		&Entry
	) == XSSH_OK) && Entry.Valid && (Entry.LineNumber == 4u) &&
		testSshTextEqual(
			Entry.KnownHost.Hosts,
			XRT_STR_LITERAL("host.example")
		), "ssh lenient known-host cursor mismatch");
	Keep = Entry;
	testRequire((xrtSshKnownHostDbNext(
		&Database,
		&Entry
	) == XSSH_NEED_MORE) &&
		(memcmp(&Entry, &Keep, sizeof(Entry)) == 0),
		"ssh known-host cursor end changed output");
	testRequire((xrtSshKnownHostDbInit(
		&Database,
		XRT_STR_LITERAL(sSource),
		(uint32)XSSH_KNOWN_HOST_DB_STRICT
	) == XSSH_OK) && (xrtSshKnownHostDbNext(
		&Database,
		&Entry
	) == XSSH_OK) && !Entry.Valid && (Entry.LineNumber == 3u) &&
		testSshTextEqual(Entry.Source, XRT_STR_LITERAL("invalid line")) &&
		(xrtSshKnownHostDbNext(&Database, &Entry) == XSSH_OK) &&
		Entry.Valid && (Entry.LineNumber == 4u),
		"ssh strict known-host cursor mismatch");
	KeepDatabase = Database;
	testRequire((xrtSshKnownHostDbNext(
		&Database,
		(xsshknownhostentry*)sSource
	) == XSSH_ERROR_ARGUMENT) &&
		(memcmp(&Database, &KeepDatabase, sizeof(Database)) == 0) &&
		(xrtSshKnownHostDbInit(
			(xsshknownhostdb*)sSource,
			XRT_STR_LITERAL(sSource),
			0u
		) == XSSH_ERROR_ARGUMENT),
		"ssh known-host cursor overlap was accepted");
}



/* 验证精确、变更、新主机、CA 与撤销优先级。 */
static void testSshKnownHostDbTrust(void)
{
	static const char sMatch[] =
		"host.example ssh-ed25519 "
		"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n";
	static const char sCa[] =
		"@cert-authority *.example ssh-ed25519 "
		"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n";
	static const char sRevoked[] =
		"host.example ssh-ed25519 "
		"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
		"@revoked host.example ssh-ed25519 "
		"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n";
	unsigned char arrKey[64];
	unsigned char arrChanged[64];
	size_t iKeySize = testSshKnownHostDbKey(arrKey, sizeof(arrKey), false);
	size_t iChangedSize = testSshKnownHostDbKey(
		arrChanged,
		sizeof(arrChanged),
		true
	);
	xsshknownhostcheck Check;

	testRequire((xrtSshKnownHostDbCheck(
		XRT_STR_LITERAL(sMatch),
		XRT_STR_LITERAL("HOST.EXAMPLE"),
		22u,
		(xbytesview){ arrKey, iKeySize },
		0u,
		&Check
	) == XSSH_OK) && (Check.Trust == XSSH_KNOWN_HOST_TRUST_MATCH) &&
		(Check.Entry.LineNumber == 1u), "ssh known-host exact trust mismatch");
	testRequire((xrtSshKnownHostDbCheck(
		XRT_STR_LITERAL(sMatch),
		XRT_STR_LITERAL("host.example"),
		22u,
		(xbytesview){ arrChanged, iChangedSize },
		0u,
		&Check
	) == XSSH_OK) && (Check.Trust == XSSH_KNOWN_HOST_TRUST_CHANGED),
		"ssh known-host changed trust mismatch");
	testRequire((xrtSshKnownHostDbCheck(
		XRT_STR_LITERAL(sMatch),
		XRT_STR_LITERAL("new.example"),
		22u,
		(xbytesview){ arrKey, iKeySize },
		0u,
		&Check
	) == XSSH_OK) && (Check.Trust == XSSH_KNOWN_HOST_TRUST_NEW) &&
		(Check.Entry.LineNumber == 0u), "ssh known-host new trust mismatch");
	testRequire((xrtSshKnownHostDbCheck(
		XRT_STR_LITERAL(sCa),
		XRT_STR_LITERAL("server.example"),
		22u,
		(xbytesview){ arrChanged, iChangedSize },
		0u,
		&Check
	) == XSSH_OK) &&
		(Check.Trust == XSSH_KNOWN_HOST_TRUST_CERT_AUTHORITY),
		"ssh known-host CA candidate mismatch");
	testRequire((xrtSshKnownHostDbCheck(
		XRT_STR_LITERAL(sRevoked),
		XRT_STR_LITERAL("host.example"),
		22u,
		(xbytesview){ arrKey, iKeySize },
		0u,
		&Check
	) == XSSH_OK) && (Check.Trust == XSSH_KNOWN_HOST_TRUST_REVOKED) &&
		(Check.Entry.LineNumber == 2u), "ssh revoked key did not win");
}



/* 验证哈希主机、严格坏行、未知 marker 与参数原子性。 */
static void testSshKnownHostDbBoundaries(void)
{
	static const char sHashed[] =
		"|1|ICEiIyQlJicoKSorLC0uLzAxMjM=|xBQE+zEva59fZY5Xy11PuF9jNd8= "
		"ssh-ed25519 "
		"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n";
	static const char sInvalid[] =
		"broken\n"
		"host.example ssh-ed25519 "
		"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n";
	static const char sUnknown[] =
		"@future host.example ssh-ed25519 "
		"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n";
	unsigned char arrKey[64];
	size_t iKeySize = testSshKnownHostDbKey(arrKey, sizeof(arrKey), false);
	xsshknownhostcheck Check;
	xsshknownhostcheck Keep;

	testRequire((xrtSshKnownHostDbCheck(
		XRT_STR_LITERAL(sHashed),
		XRT_STR_LITERAL("hashed.example"),
		22u,
		(xbytesview){ arrKey, iKeySize },
		0u,
		&Check
	) == XSSH_OK) && (Check.Trust == XSSH_KNOWN_HOST_TRUST_MATCH),
		"ssh hashed known-host database mismatch");
	testRequire((xrtSshKnownHostDbCheck(
		XRT_STR_LITERAL(sInvalid),
		XRT_STR_LITERAL("host.example"),
		22u,
		(xbytesview){ arrKey, iKeySize },
		0u,
		&Check
	) == XSSH_OK) && (Check.Trust == XSSH_KNOWN_HOST_TRUST_MATCH) &&
		(xrtSshKnownHostDbCheck(
			XRT_STR_LITERAL(sInvalid),
			XRT_STR_LITERAL("host.example"),
			22u,
			(xbytesview){ arrKey, iKeySize },
			(uint32)XSSH_KNOWN_HOST_DB_STRICT,
			&Check
		) == XSSH_OK) && (Check.Trust == XSSH_KNOWN_HOST_TRUST_INVALID) &&
		(Check.Entry.LineNumber == 1u), "ssh strict invalid line policy mismatch");
	testRequire((xrtSshKnownHostDbCheck(
		XRT_STR_LITERAL(sUnknown),
		XRT_STR_LITERAL("host.example"),
		22u,
		(xbytesview){ arrKey, iKeySize },
		0u,
		&Check
	) == XSSH_OK) && (Check.Trust == XSSH_KNOWN_HOST_TRUST_NEW) &&
		(xrtSshKnownHostDbCheck(
			XRT_STR_LITERAL(sUnknown),
			XRT_STR_LITERAL("host.example"),
			22u,
			(xbytesview){ arrKey, iKeySize },
			(uint32)XSSH_KNOWN_HOST_DB_STRICT,
			&Check
		) == XSSH_OK) && (Check.Trust == XSSH_KNOWN_HOST_TRUST_INVALID),
		"ssh unknown marker policy mismatch");
	Keep = Check;
	testRequire((xrtSshKnownHostDbCheck(
		XRT_STR_LITERAL(sHashed),
		XRT_STR_LITERAL("hashed.example"),
		0u,
		(xbytesview){ arrKey, iKeySize },
		0u,
		&Check
	) == XSSH_ERROR_ARGUMENT) &&
		(memcmp(&Check, &Keep, sizeof(Check)) == 0),
		"ssh invalid known-host check changed output");
	testRequire(xrtSshKnownHostDbCheck(
		XRT_STR_LITERAL(sHashed),
		XRT_STR_LITERAL("hashed.example"),
		22u,
		(xbytesview){ arrKey, iKeySize },
		0u,
		(xsshknownhostcheck*)sHashed
	) == XSSH_ERROR_ARGUMENT, "ssh overlapping known-host result accepted");
}



/* 运行 known_hosts 数据库策略测试。 */
int main(void)
{
	testSshKnownHostDbCursor();
	testSshKnownHostDbTrust();
	testSshKnownHostDbBoundaries();
	return 0;
}
