#include "../test.h"



/* 验证 known_hosts 固定字段、marker 与公钥解码。 */
static void testSshKnownHostLine(void)
{
	static const char sLine[] =
		"@revoked *.example.com,!bad.example.com ssh-ed25519 "
		"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA "
		"retired key";
	unsigned char arrBlob[64];
	xsshknownhostline KnownHost;
	xsshpublickey PublicKey;
	xsshknownhostmatch Match;
	bool bKeyMatch = false;

	testRequire((xrtSshKnownHostLineRead(
		XRT_STR_LITERAL(sLine),
		&KnownHost
	) == XSSH_OK) &&
		(KnownHost.MarkerKind == XSSH_KNOWN_HOST_MARKER_REVOKED) &&
		testSshTextEqual(
			KnownHost.Marker,
			XRT_STR_LITERAL(XSSH_KNOWN_HOST_REVOKED)
		) && testSshTextEqual(
			KnownHost.Hosts,
			XRT_STR_LITERAL("*.example.com,!bad.example.com")
		) && testSshTextEqual(
			KnownHost.Algorithm,
			XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519)
		) && testSshTextEqual(
			KnownHost.Comment,
			XRT_STR_LITERAL("retired key")
		) && !KnownHost.Hashed, "ssh known-host line fields mismatch");
	testRequire((xrtSshKnownHostLineDecode(
		&KnownHost,
		arrBlob,
		sizeof(arrBlob),
		&PublicKey
	) == XSSH_OK) && testSshTextEqual(
		PublicKey.Algorithm,
		XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519)
	) && (xrtSshKnownHostLineKeyMatch(
		&KnownHost,
		(xbytesview){ arrBlob, KnownHost.BlobSize },
		&bKeyMatch
	) == XSSH_OK) && bKeyMatch, "ssh known-host key decode failed");
	testRequire((xrtSshKnownHostLineMatch(
		&KnownHost,
		XRT_STR_LITERAL("GOOD.EXAMPLE.COM"),
		22u,
		&Match
	) == XSSH_OK) && (Match == XSSH_KNOWN_HOST_MATCH) &&
		(xrtSshKnownHostLineMatch(
			&KnownHost,
			XRT_STR_LITERAL("bad.example.com"),
			22u,
			&Match
		) == XSSH_OK) && (Match == XSSH_KNOWN_HOST_NEGATED),
		"ssh known-host line match mismatch");
}



/* 验证大小写、通配符、否定项、端口和 IPv6 虚拟 target。 */
static void testSshKnownHostPatterns(void)
{
	xsshknownhostmatch Match;

	testRequire((xrtSshKnownHostPatternsMatch(
		XRT_STR_LITERAL("host?.example.com,*.internal"),
		XRT_STR_LITERAL("HOST1.EXAMPLE.COM"),
		22u,
		&Match
	) == XSSH_OK) && (Match == XSSH_KNOWN_HOST_MATCH),
		"ssh known-host wildcard mismatch");
	testRequire((xrtSshKnownHostPatternsMatch(
		XRT_STR_LITERAL("[host.example]:2200"),
		XRT_STR_LITERAL("host.example"),
		2200u,
		&Match
	) == XSSH_OK) && (Match == XSSH_KNOWN_HOST_MATCH) &&
		(xrtSshKnownHostPatternsMatch(
			XRT_STR_LITERAL("[2001:db8::1]:2222"),
			XRT_STR_LITERAL("2001:DB8::1"),
			2222u,
			&Match
		) == XSSH_OK) && (Match == XSSH_KNOWN_HOST_MATCH),
		"ssh known-host non-default port mismatch");
	testRequire((xrtSshKnownHostPatternsMatch(
		XRT_STR_LITERAL("example.com"),
		XRT_STR_LITERAL("example.com"),
		2200u,
		&Match
	) == XSSH_OK) && (Match == XSSH_KNOWN_HOST_NO_MATCH),
		"ssh known-host port isolation failed");
}



/* 验证 CA/未知 marker、hashed 分层和坏 pattern-list。 */
static void testSshKnownHostBoundaries(void)
{
	static const char sKey[] =
		"ssh-ed25519 "
		"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	char sLine[256];
	xsshknownhostline KnownHost;
	xsshknownhostline Keep;
	xsshknownhostmatch Match = XSSH_KNOWN_HOST_NEGATED;
	int iWritten;

	iWritten = snprintf(
		sLine,
		sizeof(sLine),
		"@cert-authority *.example.org %s",
		sKey
	);
	testRequire((iWritten > 0) && ((size_t)iWritten < sizeof(sLine)) &&
		(xrtSshKnownHostLineRead(
			(xstrview){ sLine, (size_t)iWritten },
			&KnownHost
		) == XSSH_OK) &&
		(KnownHost.MarkerKind ==
		 XSSH_KNOWN_HOST_MARKER_CERT_AUTHORITY),
		"ssh known-host CA marker mismatch");
	iWritten = snprintf(
		sLine,
		sizeof(sLine),
		"@future example.org %s",
		sKey
	);
	testRequire((xrtSshKnownHostLineRead(
		(xstrview){ sLine, (size_t)iWritten },
		&KnownHost
	) == XSSH_OK) &&
		(KnownHost.MarkerKind == XSSH_KNOWN_HOST_MARKER_UNKNOWN),
		"ssh known-host unknown marker was not preserved");
	iWritten = snprintf(
		sLine,
		sizeof(sLine),
		"|1|AAAAAAAAAAAAAAAAAAAAAAAAAAAA|BBBBBBBBBBBBBBBBBBBBBBBBBBBB %s",
		sKey
	);
	testRequire((xrtSshKnownHostLineRead(
		(xstrview){ sLine, (size_t)iWritten },
		&KnownHost
	) == XSSH_OK) && KnownHost.Hashed &&
		(xrtSshKnownHostLineMatch(
			&KnownHost,
			XRT_STR_LITERAL("example.org"),
			22u,
			&Match
		) == XSSH_ERROR_UNSUPPORTED) &&
		(Match == XSSH_KNOWN_HOST_NEGATED),
		"ssh known-host hash layer boundary mismatch");
	Keep = KnownHost;
	testRequire((xrtSshKnownHostLineRead(
		XRT_STR_LITERAL("one,,two ssh-ed25519 AAAA"),
		&KnownHost
	) == XSSH_ERROR_PROTOCOL) &&
		(memcmp(&KnownHost, &Keep, sizeof(KnownHost)) == 0) &&
		(xrtSshKnownHostLineRead(
			XRT_STR_LITERAL("|1|salt|hash,plain ssh-ed25519 AAAA"),
			&KnownHost
		) == XSSH_ERROR_PROTOCOL), "ssh bad known-host patterns accepted");
	testRequire(xrtSshKnownHostLineRead(
		(xstrview){ sLine, (size_t)iWritten },
		(xsshknownhostline*)sLine
	) == XSSH_ERROR_ARGUMENT, "ssh overlapping known-host output accepted");
}



/* 运行 OpenSSH known_hosts 明文格式测试。 */
int main(void)
{
	testSshKnownHostLine();
	testSshKnownHostPatterns();
	testSshKnownHostBoundaries();
	return 0;
}
