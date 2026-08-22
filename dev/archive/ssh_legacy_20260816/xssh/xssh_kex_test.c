#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"
#include "xssh.h"

static int xssh_kex_test_fail(const char* sName, const char* sDetail)
{
	printf("FAIL %s: %s\n", sName, sDetail ? sDetail : "");
	return 1;
}

static bool xssh_kex_slice_eq(xsshslice s, const char* sText)
{
	size_t iLen = strlen(sText);
	return (s.iLen == iLen && memcmp(s.pData, sText, iLen) == 0) ? TRUE : FALSE;
}

static xsshslice xssh_kex_test_slice(const void* pData, size_t iLen)
{
	xsshslice s;
	s.pData = (const uint8*)pData;
	s.iLen = iLen;
	return s;
}

static int xssh_kex_test_default_roundtrip(void)
{
	uint8 arrCookie[XSSH_KEX_COOKIE_SIZE];
	uint8 arrBuf[2048];
	xsshkexinitdesc desc;
	xsshkexinit parsed;
	xsshwriter wr;
	size_t i;

	for ( i = 0u; i < XSSH_KEX_COOKIE_SIZE; ++i ) {
		arrCookie[i] = (uint8)i;
	}
	xsshKexInitDescInit(&desc);
	desc.pCookie = arrCookie;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshKexInitWrite(&wr, &desc) != XSSH_OK ) {
		return xssh_kex_test_fail("kex_default_roundtrip", "write failed");
	}
	if ( wr.iLen == 0u || arrBuf[0] != XSSH_MSG_KEXINIT ) {
		return xssh_kex_test_fail("kex_default_roundtrip", "message id mismatch");
	}
	if ( xsshKexInitRead(arrBuf, wr.iLen, &parsed) != XSSH_OK ) {
		return xssh_kex_test_fail("kex_default_roundtrip", "read failed");
	}
	if ( memcmp(parsed.arrCookie, arrCookie, sizeof(arrCookie)) != 0 ) {
		return xssh_kex_test_fail("kex_default_roundtrip", "cookie mismatch");
	}
	if ( !xssh_kex_slice_eq(parsed.tKexAlgorithms, XSSH_ALG_KEX_DEFAULT) ||
		!xssh_kex_slice_eq(parsed.tServerHostKeyAlgorithms, XSSH_ALG_HOSTKEY_DEFAULT) ||
		!xsshWireNameListContains((const char*)parsed.tServerHostKeyAlgorithms.pData, parsed.tServerHostKeyAlgorithms.iLen,
			"ssh-ed25519", strlen("ssh-ed25519")) ||
		!xsshWireNameListContains((const char*)parsed.tEncryptionClientToServer.pData, parsed.tEncryptionClientToServer.iLen,
			"chacha20-poly1305@openssh.com", strlen("chacha20-poly1305@openssh.com")) ||
		!xsshWireNameListContains((const char*)parsed.tCompressionClientToServer.pData, parsed.tCompressionClientToServer.iLen,
			"none", strlen("none")) ) {
		return xssh_kex_test_fail("kex_default_roundtrip", "default algorithm list mismatch");
	}
	if ( parsed.bFirstKexPacketFollows || parsed.iReserved != 0u ) {
		return xssh_kex_test_fail("kex_default_roundtrip", "flag/reserved mismatch");
	}
	return 0;
}

static int xssh_kex_test_custom_roundtrip(void)
{
	uint8 arrCookie[XSSH_KEX_COOKIE_SIZE];
	uint8 arrBuf[512];
	xsshkexinitdesc desc;
	xsshkexinit parsed;
	xsshwriter wr;

	memset(arrCookie, 0xabu, sizeof(arrCookie));
	xsshKexInitDescInit(&desc);
	desc.pCookie = arrCookie;
	desc.sKexAlgorithms = "curve25519-sha256";
	desc.sServerHostKeyAlgorithms = "ssh-ed25519";
	desc.sEncryptionClientToServer = "aes128-gcm@openssh.com";
	desc.sEncryptionServerToClient = "aes128-gcm@openssh.com";
	desc.sMacClientToServer = "";
	desc.sMacServerToClient = "";
	desc.sCompressionClientToServer = "none";
	desc.sCompressionServerToClient = "none";
	desc.bFirstKexPacketFollows = TRUE;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshKexInitWrite(&wr, &desc) != XSSH_OK ||
		xsshKexInitRead(arrBuf, wr.iLen, &parsed) != XSSH_OK ) {
		return xssh_kex_test_fail("kex_custom_roundtrip", "roundtrip failed");
	}
	if ( !parsed.bFirstKexPacketFollows ||
		!xssh_kex_slice_eq(parsed.tKexAlgorithms, "curve25519-sha256") ||
		!xssh_kex_slice_eq(parsed.tMacClientToServer, "") ) {
		return xssh_kex_test_fail("kex_custom_roundtrip", "parsed values mismatch");
	}
	return 0;
}

static int xssh_kex_test_invalid_message(void)
{
	uint8 arrBuf[2048];
	xsshwriter wr;
	xsshkexinit parsed;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshKexInitWrite(&wr, NULL) != XSSH_OK ) {
		return xssh_kex_test_fail("kex_invalid_message", "setup write failed");
	}
	arrBuf[0] = 99u;
	if ( xsshKexInitRead(arrBuf, wr.iLen, &parsed) != XSSH_ERR_BAD_PACKET ) {
		return xssh_kex_test_fail("kex_invalid_message", "invalid message accepted");
	}
	return 0;
}

static int xssh_kex_test_invalid_name_list(void)
{
	uint8 arrBuf[512];
	xsshkexinitdesc desc;
	xsshwriter wr;

	xsshKexInitDescInit(&desc);
	desc.sKexAlgorithms = "curve25519-sha256,,ecdh-sha2-nistp256";
	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshKexInitWrite(&wr, &desc) != XSSH_ERR_INVALID || wr.iLen != 0u ) {
		return xssh_kex_test_fail("kex_invalid_name_list", "invalid writer list accepted");
	}
	return 0;
}

static int xssh_kex_test_nonzero_reserved_rejected(void)
{
	uint8 arrBuf[2048];
	xsshwriter wr;
	xsshkexinit parsed;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshKexInitWrite(&wr, NULL) != XSSH_OK ) {
		return xssh_kex_test_fail("kex_reserved", "setup write failed");
	}
	arrBuf[wr.iLen - 1u] = 1u;
	if ( xsshKexInitRead(arrBuf, wr.iLen, &parsed) != XSSH_ERR_BAD_PACKET ) {
		return xssh_kex_test_fail("kex_reserved", "nonzero reserved accepted");
	}
	return 0;
}

static int xssh_kex_test_no_space_is_atomic(void)
{
	uint8 arrBuf[32];
	xsshwriter wr;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshKexInitWrite(&wr, NULL) != XSSH_ERR_NO_SPACE || wr.iLen != 0u ) {
		return xssh_kex_test_fail("kex_no_space", "small writer changed state");
	}
	return 0;
}

static int xssh_kex_test_first_match(void)
{
	xsshslice s;

	if ( xsshWireNameListFirstMatch("a,b,c", 5u, "d,b", 3u, &s) != XSSH_OK ||
		!xssh_kex_slice_eq(s, "b") ) {
		return xssh_kex_test_fail("kex_first_match", "expected match not selected");
	}
	if ( xsshWireNameListFirstMatch("rsa-sha2-256,ssh-ed25519", strlen("rsa-sha2-256,ssh-ed25519"),
			"sha2,ssh-rsa", strlen("sha2,ssh-rsa"), &s) != XSSH_ERR_UNSUPPORTED ) {
		return xssh_kex_test_fail("kex_first_match", "partial token matched");
	}
	if ( xsshWireNameListFirstMatch("a,b", 3u, "c,d", 3u, &s) != XSSH_ERR_UNSUPPORTED ) {
		return xssh_kex_test_fail("kex_first_match", "missing match accepted");
	}
	return 0;
}

static int xssh_kex_build_parse(const xsshkexinitdesc* pDesc, uint8* pBuf, size_t iCap, xsshkexinit* pOut)
{
	xsshwriter wr;

	xsshWriterInit(&wr, pBuf, iCap);
	if ( xsshKexInitWrite(&wr, pDesc) != XSSH_OK ) {
		return XSSH_ERR_INVALID;
	}
	return xsshKexInitRead(pBuf, wr.iLen, pOut);
}

static int xssh_kex_test_negotiate_aead_defaults(void)
{
	uint8 arrClientCookie[XSSH_KEX_COOKIE_SIZE];
	uint8 arrServerCookie[XSSH_KEX_COOKIE_SIZE];
	uint8 arrClientBuf[2048];
	uint8 arrServerBuf[2048];
	xsshkexinitdesc clientDesc;
	xsshkexinitdesc serverDesc;
	xsshkexinit clientKex;
	xsshkexinit serverKex;
	xsshkexnegotiation neg;

	memset(arrClientCookie, 0x11, sizeof(arrClientCookie));
	memset(arrServerCookie, 0x22, sizeof(arrServerCookie));
	xsshKexInitDescInit(&clientDesc);
	xsshKexInitDescInit(&serverDesc);
	clientDesc.pCookie = arrClientCookie;
	serverDesc.pCookie = arrServerCookie;
	serverDesc.sKexAlgorithms = "ecdh-sha2-nistp256,curve25519-sha256";
	serverDesc.sServerHostKeyAlgorithms = "rsa-sha2-256,ssh-ed25519";
	serverDesc.sEncryptionClientToServer = "aes256-gcm@openssh.com,aes128-gcm@openssh.com";
	serverDesc.sEncryptionServerToClient = "aes128-gcm@openssh.com";
	serverDesc.sMacClientToServer = "hmac-sha2-256";
	serverDesc.sMacServerToClient = "hmac-sha2-256";
	serverDesc.sCompressionClientToServer = "none";
	serverDesc.sCompressionServerToClient = "none";

	if ( xssh_kex_build_parse(&clientDesc, arrClientBuf, sizeof(arrClientBuf), &clientKex) != XSSH_OK ||
		xssh_kex_build_parse(&serverDesc, arrServerBuf, sizeof(arrServerBuf), &serverKex) != XSSH_OK ||
		xsshKexNegotiate(&clientKex, &serverKex, &neg) != XSSH_OK ) {
		return xssh_kex_test_fail("kex_negotiate_aead", "negotiate failed");
	}
	if ( !xssh_kex_slice_eq(neg.tKexAlgorithm, "curve25519-sha256") ||
		!xssh_kex_slice_eq(neg.tServerHostKeyAlgorithm, "ssh-ed25519") ||
		!xssh_kex_slice_eq(neg.tCipherClientToServer, "aes128-gcm@openssh.com") ||
		!xssh_kex_slice_eq(neg.tCipherServerToClient, "aes128-gcm@openssh.com") ||
		neg.tMacClientToServer.iLen != 0u ||
		neg.tMacServerToClient.iLen != 0u ||
		!xssh_kex_slice_eq(neg.tCompressionClientToServer, "none") ||
		!xssh_kex_slice_eq(neg.tCompressionServerToClient, "none") ) {
		return xssh_kex_test_fail("kex_negotiate_aead", "selected algorithms mismatch");
	}
	return 0;
}

static int xssh_kex_test_negotiate_ctr_with_hmac(void)
{
	uint8 arrClientCookie[XSSH_KEX_COOKIE_SIZE];
	uint8 arrServerCookie[XSSH_KEX_COOKIE_SIZE];
	uint8 arrClientBuf[1024];
	uint8 arrServerBuf[1024];
	xsshkexinitdesc clientDesc;
	xsshkexinitdesc serverDesc;
	xsshkexinit clientKex;
	xsshkexinit serverKex;
	xsshkexnegotiation neg;

	memset(arrClientCookie, 0x33, sizeof(arrClientCookie));
	memset(arrServerCookie, 0x44, sizeof(arrServerCookie));
	xsshKexInitDescInit(&clientDesc);
	xsshKexInitDescInit(&serverDesc);
	clientDesc.pCookie = arrClientCookie;
	serverDesc.pCookie = arrServerCookie;
	clientDesc.sKexAlgorithms = "curve25519-sha256";
	clientDesc.sServerHostKeyAlgorithms = "ssh-ed25519";
	clientDesc.sEncryptionClientToServer = "aes128-ctr";
	clientDesc.sEncryptionServerToClient = "aes128-ctr";
	clientDesc.sMacClientToServer = "hmac-sha2-512,hmac-sha2-256";
	clientDesc.sMacServerToClient = "hmac-sha2-512,hmac-sha2-256";
	clientDesc.sCompressionClientToServer = "none";
	clientDesc.sCompressionServerToClient = "none";
	serverDesc.sKexAlgorithms = "curve25519-sha256";
	serverDesc.sServerHostKeyAlgorithms = "ssh-ed25519";
	serverDesc.sEncryptionClientToServer = "aes128-ctr";
	serverDesc.sEncryptionServerToClient = "aes128-ctr";
	serverDesc.sMacClientToServer = "hmac-sha2-256";
	serverDesc.sMacServerToClient = "hmac-sha2-256";
	serverDesc.sCompressionClientToServer = "none";
	serverDesc.sCompressionServerToClient = "none";

	if ( xssh_kex_build_parse(&clientDesc, arrClientBuf, sizeof(arrClientBuf), &clientKex) != XSSH_OK ||
		xssh_kex_build_parse(&serverDesc, arrServerBuf, sizeof(arrServerBuf), &serverKex) != XSSH_OK ||
		xsshKexNegotiate(&clientKex, &serverKex, &neg) != XSSH_OK ) {
		return xssh_kex_test_fail("kex_negotiate_ctr", "negotiate failed");
	}
	if ( !xssh_kex_slice_eq(neg.tCipherClientToServer, "aes128-ctr") ||
		!xssh_kex_slice_eq(neg.tCipherServerToClient, "aes128-ctr") ||
		!xssh_kex_slice_eq(neg.tMacClientToServer, "hmac-sha2-256") ||
		!xssh_kex_slice_eq(neg.tMacServerToClient, "hmac-sha2-256") ) {
		return xssh_kex_test_fail("kex_negotiate_ctr", "selected algorithms mismatch");
	}
	return 0;
}

static int xssh_kex_test_negotiate_no_match(void)
{
	uint8 arrClientBuf[2048];
	uint8 arrServerBuf[2048];
	xsshkexinitdesc clientDesc;
	xsshkexinitdesc serverDesc;
	xsshkexinit clientKex;
	xsshkexinit serverKex;
	xsshkexnegotiation neg;

	xsshKexInitDescInit(&clientDesc);
	xsshKexInitDescInit(&serverDesc);
	serverDesc.sKexAlgorithms = "diffie-hellman-group1-sha1";
	if ( xssh_kex_build_parse(&clientDesc, arrClientBuf, sizeof(arrClientBuf), &clientKex) != XSSH_OK ||
		xssh_kex_build_parse(&serverDesc, arrServerBuf, sizeof(arrServerBuf), &serverKex) != XSSH_OK ) {
		return xssh_kex_test_fail("kex_negotiate_no_match", "setup failed");
	}
	if ( xsshKexNegotiate(&clientKex, &serverKex, &neg) != XSSH_ERR_UNSUPPORTED ) {
		return xssh_kex_test_fail("kex_negotiate_no_match", "missing kex match accepted");
	}
	return 0;
}

static int xssh_kex_test_first_kex_packet_follows_skip(void)
{
	uint8 arrClientBuf[2048];
	uint8 arrServerBuf[2048];
	xsshkexinitdesc clientDesc;
	xsshkexinitdesc serverDesc;
	xsshkexinit clientKex;
	xsshkexinit serverKex;
	xsshkexnegotiation neg;

	xsshKexInitDescInit(&clientDesc);
	xsshKexInitDescInit(&serverDesc);
	serverDesc.sKexAlgorithms = "ecdh-sha2-nistp256,curve25519-sha256";
	serverDesc.sServerHostKeyAlgorithms = "ssh-ed25519";
	serverDesc.sEncryptionClientToServer = "aes128-gcm@openssh.com";
	serverDesc.sEncryptionServerToClient = "aes128-gcm@openssh.com";
	serverDesc.sMacClientToServer = "";
	serverDesc.sMacServerToClient = "";
	serverDesc.sCompressionClientToServer = "none";
	serverDesc.sCompressionServerToClient = "none";
	serverDesc.bFirstKexPacketFollows = TRUE;
	if ( xssh_kex_build_parse(&clientDesc, arrClientBuf, sizeof(arrClientBuf), &clientKex) != XSSH_OK ||
		xssh_kex_build_parse(&serverDesc, arrServerBuf, sizeof(arrServerBuf), &serverKex) != XSSH_OK ||
		xsshKexNegotiate(&clientKex, &serverKex, &neg) != XSSH_OK ) {
		return xssh_kex_test_fail("kex_first_follows", "wrong-guess setup failed");
	}
	if ( !xsshKexShouldSkipFirstKexFollower(&serverKex, &neg) ) {
		return xssh_kex_test_fail("kex_first_follows", "wrong guess did not request skip");
	}

	serverDesc.sKexAlgorithms = "curve25519-sha256,ecdh-sha2-nistp256";
	if ( xssh_kex_build_parse(&serverDesc, arrServerBuf, sizeof(arrServerBuf), &serverKex) != XSSH_OK ||
		xsshKexNegotiate(&clientKex, &serverKex, &neg) != XSSH_OK ) {
		return xssh_kex_test_fail("kex_first_follows", "correct-guess setup failed");
	}
	if ( xsshKexShouldSkipFirstKexFollower(&serverKex, &neg) ) {
		return xssh_kex_test_fail("kex_first_follows", "correct guess requested skip");
	}
	return 0;
}

static void xssh_kex_make_hash_fixture(xsshkexhashdesc* pDesc, uint8* pClientE, uint8* pServerF, uint8* pSharedK)
{
	static const uint8 arrClientKex[] = { XSSH_MSG_KEXINIT, 'c', 'l', 'i', 'e', 'n', 't', '-', 'k', 'e', 'x', 'i', 'n', 'i', 't' };
	static const uint8 arrServerKex[] = { XSSH_MSG_KEXINIT, 's', 'e', 'r', 'v', 'e', 'r', '-', 'k', 'e', 'x', 'i', 'n', 'i', 't' };
	size_t i;

	for ( i = 0u; i < 32u; ++i ) {
		pClientE[i] = (uint8)(i + 1u);
		pServerF[i] = (uint8)(i + 65u);
		pSharedK[i] = (i == 0u) ? 0x80u : (uint8)i;
	}
	memset(pDesc, 0, sizeof(*pDesc));
	pDesc->tClientVersion = xssh_kex_test_slice("SSH-2.0-xssh-client", strlen("SSH-2.0-xssh-client"));
	pDesc->tServerVersion = xssh_kex_test_slice("SSH-2.0-xssh-server", strlen("SSH-2.0-xssh-server"));
	pDesc->tClientKexInit = xssh_kex_test_slice(arrClientKex, sizeof(arrClientKex));
	pDesc->tServerKexInit = xssh_kex_test_slice(arrServerKex, sizeof(arrServerKex));
	pDesc->tServerHostKey = xssh_kex_test_slice("server-host-key", strlen("server-host-key"));
	pDesc->tClientEphemeral = xssh_kex_test_slice(pClientE, 32u);
	pDesc->tServerEphemeral = xssh_kex_test_slice(pServerF, 32u);
	pDesc->tSharedSecret = xssh_kex_test_slice(pSharedK, 32u);
}

static int xssh_kex_test_exchange_hash_sha256(void)
{
	static const uint8 arrExpectedHash[XSSH_SHA256_SIZE] = {
		0xe8u, 0x4fu, 0x65u, 0xafu, 0x56u, 0x89u, 0xbbu, 0xbcu,
		0x95u, 0xa6u, 0x6cu, 0xceu, 0x16u, 0x49u, 0x48u, 0x27u,
		0xcau, 0x13u, 0x3fu, 0xecu, 0xcfu, 0xe3u, 0xccu, 0x92u,
		0x63u, 0x4eu, 0x3au, 0x3du, 0x41u, 0x11u, 0xa3u, 0x26u
	};
	uint8 arrClientE[32];
	uint8 arrServerF[32];
	uint8 arrSharedK[32];
	uint8 arrInput[512];
	uint8 arrHash[XSSH_SHA256_SIZE];
	xsshkexhashdesc desc;
	xsshwriter wr;

	xssh_kex_make_hash_fixture(&desc, arrClientE, arrServerF, arrSharedK);
	xsshWriterInit(&wr, arrInput, sizeof(arrInput));
	if ( xsshKexHashInputWriteCurve25519(&wr, &desc) != XSSH_OK || wr.iLen != 212u ) {
		return xssh_kex_test_fail("kex_exchange_hash", "hash input write mismatch");
	}
	xrtSHA256(arrInput, wr.iLen, arrHash);
	if ( memcmp(arrHash, arrExpectedHash, sizeof(arrExpectedHash)) != 0 ) {
		return xssh_kex_test_fail("kex_exchange_hash", "manual hash mismatch");
	}
	memset(arrHash, 0, sizeof(arrHash));
	if ( xsshKexHashSHA256Curve25519(&desc, arrHash) != XSSH_OK ||
		memcmp(arrHash, arrExpectedHash, sizeof(arrExpectedHash)) != 0 ) {
		return xssh_kex_test_fail("kex_exchange_hash", "helper hash mismatch");
	}
	return 0;
}

static int xssh_kex_test_exchange_hash_no_space(void)
{
	uint8 arrClientE[32];
	uint8 arrServerF[32];
	uint8 arrSharedK[32];
	uint8 arrInput[16];
	xsshkexhashdesc desc;
	xsshwriter wr;

	xssh_kex_make_hash_fixture(&desc, arrClientE, arrServerF, arrSharedK);
	xsshWriterInit(&wr, arrInput, sizeof(arrInput));
	if ( xsshKexHashInputWriteCurve25519(&wr, &desc) != XSSH_ERR_NO_SPACE || wr.iLen != 0u ) {
		return xssh_kex_test_fail("kex_exchange_hash_no_space", "small writer changed state");
	}
	return 0;
}

static int xssh_kex_test_key_derivation_sha256(void)
{
	static const uint8 arrExpected[6][40] = {
		{ 0xf5u, 0x30u, 0xf8u, 0x05u, 0xe6u, 0x4du, 0xbeu, 0x83u, 0xcdu, 0x70u, 0x36u, 0x21u, 0xa4u, 0xc3u, 0xb4u, 0x5bu, 0x17u, 0x18u, 0x05u, 0x22u, 0x90u, 0x4cu, 0x4au, 0xdau, 0x49u, 0xf8u, 0xf7u, 0x4cu, 0xcfu, 0x25u, 0xfeu, 0xfcu, 0xfdu, 0x0cu, 0x44u, 0xdfu, 0x88u, 0x3bu, 0xdcu, 0xa2u },
		{ 0xa6u, 0xf0u, 0x73u, 0x3du, 0x71u, 0x7cu, 0xa7u, 0xc6u, 0xc8u, 0x9du, 0xe1u, 0x67u, 0xd7u, 0xf3u, 0xdau, 0x8cu, 0xb1u, 0x07u, 0x14u, 0x62u, 0xccu, 0xc7u, 0x72u, 0x75u, 0x65u, 0x92u, 0x70u, 0x6du, 0xc3u, 0x4du, 0x16u, 0x86u, 0x9cu, 0xfau, 0xcau, 0xbdu, 0xceu, 0x3eu, 0xdcu, 0x3eu },
		{ 0x34u, 0xc8u, 0x43u, 0x19u, 0x37u, 0x84u, 0xaeu, 0xfbu, 0xdfu, 0x57u, 0x52u, 0x17u, 0xebu, 0xd3u, 0xdbu, 0x44u, 0x84u, 0xa7u, 0x68u, 0x61u, 0x9eu, 0x2cu, 0xdbu, 0x4fu, 0x7au, 0xd1u, 0x67u, 0x1au, 0x8cu, 0xa2u, 0x34u, 0x20u, 0x35u, 0x4bu, 0x11u, 0xa2u, 0xe7u, 0x01u, 0xceu, 0xc3u },
		{ 0x6au, 0x3eu, 0x01u, 0x75u, 0x51u, 0x40u, 0x17u, 0x66u, 0xfdu, 0x68u, 0x36u, 0xaau, 0x60u, 0xb9u, 0x77u, 0x5cu, 0xc7u, 0xbau, 0xe8u, 0x9du, 0x65u, 0x28u, 0x22u, 0x3fu, 0x9cu, 0x14u, 0x58u, 0x8bu, 0xc9u, 0xb8u, 0x0fu, 0x81u, 0x06u, 0x2fu, 0xafu, 0x22u, 0x0eu, 0x2du, 0xf2u, 0x0fu },
		{ 0x64u, 0x0au, 0x77u, 0xb9u, 0x40u, 0x9du, 0xa6u, 0xcbu, 0x1au, 0xccu, 0xcbu, 0x5au, 0x97u, 0x35u, 0xe3u, 0x00u, 0x31u, 0xa0u, 0x7cu, 0x14u, 0x07u, 0xcdu, 0x55u, 0xf4u, 0x10u, 0x50u, 0x29u, 0x5au, 0x70u, 0x4cu, 0xd2u, 0xcfu, 0xd5u, 0x1bu, 0xfau, 0x40u, 0x0cu, 0x24u, 0xa5u, 0x6du },
		{ 0x7au, 0x38u, 0xf2u, 0xc2u, 0x34u, 0xf6u, 0xc8u, 0x8bu, 0x2cu, 0x6bu, 0x19u, 0xd3u, 0xabu, 0xc0u, 0x6bu, 0x04u, 0x05u, 0x79u, 0x0eu, 0xb7u, 0x64u, 0x9fu, 0x0cu, 0xc5u, 0x93u, 0xa7u, 0xc6u, 0x5eu, 0xd1u, 0x5eu, 0xc2u, 0x28u, 0x76u, 0x67u, 0x43u, 0xb1u, 0x10u, 0xd1u, 0x9fu, 0x4du }
	};
	static const uint8 arrExpectedHash[XSSH_SHA256_SIZE] = {
		0xe8u, 0x4fu, 0x65u, 0xafu, 0x56u, 0x89u, 0xbbu, 0xbcu,
		0x95u, 0xa6u, 0x6cu, 0xceu, 0x16u, 0x49u, 0x48u, 0x27u,
		0xcau, 0x13u, 0x3fu, 0xecu, 0xcfu, 0xe3u, 0xccu, 0x92u,
		0x63u, 0x4eu, 0x3au, 0x3du, 0x41u, 0x11u, 0xa3u, 0x26u
	};
	uint8 arrClientE[32];
	uint8 arrServerF[32];
	uint8 arrSharedK[32];
	uint8 arrOut[40];
	xsshkexhashdesc desc;
	size_t i;

	xssh_kex_make_hash_fixture(&desc, arrClientE, arrServerF, arrSharedK);
	for ( i = 0u; i < 6u; ++i ) {
		memset(arrOut, 0, sizeof(arrOut));
		if ( xsshKexDeriveKeySHA256(arrOut, sizeof(arrOut), desc.tSharedSecret, arrExpectedHash, arrExpectedHash, (uint8)('A' + i)) != XSSH_OK ||
			memcmp(arrOut, arrExpected[i], sizeof(arrOut)) != 0 ) {
			return xssh_kex_test_fail("kex_key_derivation", "derived key mismatch");
		}
	}
	if ( xsshKexDeriveKeySHA256(arrOut, sizeof(arrOut), desc.tSharedSecret, arrExpectedHash, arrExpectedHash, (uint8)'G') != XSSH_ERR_INVALID ) {
		return xssh_kex_test_fail("kex_key_derivation", "invalid letter accepted");
	}
	return 0;
}

static int xssh_kex_test_curve25519_shared_secret_vector(void)
{
	static const uint8 arrAlicePriv[32] = {
		0x77u, 0x07u, 0x6du, 0x0au, 0x73u, 0x18u, 0xa5u, 0x7du,
		0x3cu, 0x16u, 0xc1u, 0x72u, 0x51u, 0xb2u, 0x66u, 0x45u,
		0xdfu, 0x4cu, 0x2fu, 0x87u, 0xebu, 0xc0u, 0x99u, 0x2au,
		0xb1u, 0x77u, 0xfbu, 0xa5u, 0x1du, 0xb9u, 0x2cu, 0x2au
	};
	static const uint8 arrBobPub[32] = {
		0xdeu, 0x9eu, 0xdbu, 0x7du, 0x7bu, 0x7du, 0xc1u, 0xb4u,
		0xd3u, 0x5bu, 0x61u, 0xc2u, 0xecu, 0xe4u, 0x35u, 0x37u,
		0x3fu, 0x83u, 0x43u, 0xc8u, 0x5bu, 0x78u, 0x67u, 0x4du,
		0xadu, 0xfcu, 0x7eu, 0x14u, 0x6fu, 0x88u, 0x2bu, 0x4fu
	};
	static const uint8 arrExpected[32] = {
		0x4au, 0x5du, 0x9du, 0x5bu, 0xa4u, 0xceu, 0x2du, 0xe1u,
		0x72u, 0x8eu, 0x3bu, 0xf4u, 0x80u, 0x35u, 0x0fu, 0x25u,
		0xe0u, 0x7eu, 0x21u, 0xc9u, 0x47u, 0xd1u, 0x9eu, 0x33u,
		0x76u, 0xf0u, 0x9bu, 0x3cu, 0x1eu, 0x16u, 0x17u, 0x42u
	};
	uint8 arrShared[32];

	if ( xsshKexCurve25519SharedSecret(arrShared, arrAlicePriv, arrBobPub) != XSSH_OK ||
		memcmp(arrShared, arrExpected, sizeof(arrExpected)) != 0 ) {
		return xssh_kex_test_fail("curve25519_shared_secret", "RFC 7748 shared secret mismatch");
	}
	return 0;
}

static int xssh_kex_test_curve25519_all_zero_rejected(void)
{
	uint8 arrPriv[32] = { 0 };
	uint8 arrPeer[32] = { 0 };
	uint8 arrShared[32];

	arrPriv[0] = 1u;
	if ( xsshKexCurve25519SharedSecret(arrShared, arrPriv, arrPeer) != XSSH_ERR_BAD_PACKET ) {
		return xssh_kex_test_fail("curve25519_all_zero", "all-zero shared secret accepted");
	}
	if ( !xsshMemIsAllZero(arrShared, sizeof(arrShared)) ) {
		return xssh_kex_test_fail("curve25519_all_zero", "shared secret not cleared");
	}
	return 0;
}

static int xssh_kex_test_ecdh_init_roundtrip(void)
{
	uint8 arrPub[32];
	uint8 arrBuf[128];
	xsshwriter wr;
	xsshkexecdhinit init;
	size_t i;

	for ( i = 0u; i < sizeof(arrPub); ++i ) {
		arrPub[i] = (uint8)(0xa0u + i);
	}
	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshKexECDHInitWrite(&wr, arrPub, sizeof(arrPub)) != XSSH_OK ||
		wr.iLen != 37u ||
		arrBuf[0] != XSSH_MSG_KEX_ECDH_INIT ) {
		return xssh_kex_test_fail("ecdh_init_roundtrip", "write mismatch");
	}
	if ( xsshKexECDHInitRead(arrBuf, wr.iLen, &init) != XSSH_OK ||
		init.tClientPublic.iLen != sizeof(arrPub) ||
		memcmp(init.tClientPublic.pData, arrPub, sizeof(arrPub)) != 0 ) {
		return xssh_kex_test_fail("ecdh_init_roundtrip", "read mismatch");
	}
	arrBuf[0] = 99u;
	if ( xsshKexECDHInitRead(arrBuf, wr.iLen, &init) != XSSH_ERR_BAD_PACKET ) {
		return xssh_kex_test_fail("ecdh_init_roundtrip", "bad message accepted");
	}
	return 0;
}

static int xssh_kex_test_ecdh_reply_roundtrip(void)
{
	static const uint8 arrHostKey[] = { 'h', 'o', 's', 't', 'k', 'e', 'y' };
	static const uint8 arrServerPub[] = { 1u, 2u, 3u, 4u };
	static const uint8 arrSig[] = { 's', 'i', 'g' };
	uint8 arrBuf[128];
	xsshwriter wr;
	xsshkexecdhreply reply;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshKexECDHReplyWrite(&wr,
			xssh_kex_test_slice(arrHostKey, sizeof(arrHostKey)),
			xssh_kex_test_slice(arrServerPub, sizeof(arrServerPub)),
			xssh_kex_test_slice(arrSig, sizeof(arrSig))) != XSSH_OK ||
		arrBuf[0] != XSSH_MSG_KEX_ECDH_REPLY ) {
		return xssh_kex_test_fail("ecdh_reply_roundtrip", "write mismatch");
	}
	if ( xsshKexECDHReplyRead(arrBuf, wr.iLen, &reply) != XSSH_OK ||
		reply.tServerHostKey.iLen != sizeof(arrHostKey) ||
		reply.tServerPublic.iLen != sizeof(arrServerPub) ||
		reply.tSignature.iLen != sizeof(arrSig) ||
		memcmp(reply.tServerPublic.pData, arrServerPub, sizeof(arrServerPub)) != 0 ) {
		return xssh_kex_test_fail("ecdh_reply_roundtrip", "read mismatch");
	}
	arrBuf[0] = 99u;
	if ( xsshKexECDHReplyRead(arrBuf, wr.iLen, &reply) != XSSH_ERR_BAD_PACKET ) {
		return xssh_kex_test_fail("ecdh_reply_roundtrip", "bad message accepted");
	}
	return 0;
}

static int xssh_kex_test_ed25519_hostkey_verify(void)
{
	static const uint8 arrHash[XSSH_SHA256_SIZE] = {
		0xe8u, 0x4fu, 0x65u, 0xafu, 0x56u, 0x89u, 0xbbu, 0xbcu,
		0x95u, 0xa6u, 0x6cu, 0xceu, 0x16u, 0x49u, 0x48u, 0x27u,
		0xcau, 0x13u, 0x3fu, 0xecu, 0xcfu, 0xe3u, 0xccu, 0x92u,
		0x63u, 0x4eu, 0x3au, 0x3du, 0x41u, 0x11u, 0xa3u, 0x26u
	};
	uint8 arrSeed[32];
	uint8 arrPub[32];
	uint8 arrSig[64];
	uint8 arrKeyBlob[128];
	uint8 arrSigBlob[128];
	xsshwriter wrKey;
	xsshwriter wrSig;
	xsshpublickey key;
	xsshsignature sig;
	size_t i;

	for ( i = 0u; i < sizeof(arrSeed); ++i ) {
		arrSeed[i] = (uint8)(0x10u + i);
	}
	xrtEd25519PublicKey(arrPub, arrSeed);
	if ( !xrtEd25519Sign(arrSig, arrHash, sizeof(arrHash), arrSeed) ) {
		return xssh_kex_test_fail("ed25519_hostkey", "sign failed");
	}

	xsshWriterInit(&wrKey, arrKeyBlob, sizeof(arrKeyBlob));
	xsshWriterInit(&wrSig, arrSigBlob, sizeof(arrSigBlob));
	if ( xsshPublicKeyWriteEd25519(&wrKey, arrPub) != XSSH_OK ||
		xsshSignatureWrite(&wrSig, "ssh-ed25519", arrSig, sizeof(arrSig)) != XSSH_OK ) {
		return xssh_kex_test_fail("ed25519_hostkey", "blob write failed");
	}
	if ( xsshPublicKeyReadEd25519(arrKeyBlob, wrKey.iLen, &key) != XSSH_OK ||
		xsshSignatureRead(arrSigBlob, wrSig.iLen, &sig) != XSSH_OK ||
		xsshHostKeyVerifyEd25519(&key, &sig, arrHash, sizeof(arrHash)) != XSSH_OK ) {
		return xssh_kex_test_fail("ed25519_hostkey", "verify failed");
	}

	arrSigBlob[wrSig.iLen - 1u] ^= 0x01u;
	if ( xsshSignatureRead(arrSigBlob, wrSig.iLen, &sig) != XSSH_OK ||
		xsshHostKeyVerifyEd25519(&key, &sig, arrHash, sizeof(arrHash)) != XSSH_ERR_BAD_PACKET ) {
		return xssh_kex_test_fail("ed25519_hostkey", "bad signature accepted");
	}
	return 0;
}

static int xssh_kex_test_ed25519_hostkey_rejects_bad_blob(void)
{
	uint8 arrBadKey[64];
	uint8 arrPub[31];
	xsshwriter wr;
	xsshpublickey key;

	memset(arrPub, 1, sizeof(arrPub));
	xsshWriterInit(&wr, arrBadKey, sizeof(arrBadKey));
	if ( xsshWireWriteString(&wr, "ssh-ed25519", strlen("ssh-ed25519")) != XSSH_OK ||
		xsshWireWriteString(&wr, arrPub, sizeof(arrPub)) != XSSH_OK ) {
		return xssh_kex_test_fail("ed25519_bad_blob", "setup failed");
	}
	if ( xsshPublicKeyReadEd25519(arrBadKey, wr.iLen, &key) != XSSH_ERR_BAD_PACKET ) {
		return xssh_kex_test_fail("ed25519_bad_blob", "bad key length accepted");
	}
	arrBadKey[7] = 'r';
	if ( xsshPublicKeyReadEd25519(arrBadKey, wr.iLen, &key) != XSSH_ERR_UNSUPPORTED ) {
		return xssh_kex_test_fail("ed25519_bad_blob", "bad key algorithm accepted");
	}
	return 0;
}

int main(void)
{
	if ( xssh_kex_test_default_roundtrip() ) return 1;
	if ( xssh_kex_test_custom_roundtrip() ) return 1;
	if ( xssh_kex_test_invalid_message() ) return 1;
	if ( xssh_kex_test_invalid_name_list() ) return 1;
	if ( xssh_kex_test_nonzero_reserved_rejected() ) return 1;
	if ( xssh_kex_test_no_space_is_atomic() ) return 1;
	if ( xssh_kex_test_first_match() ) return 1;
	if ( xssh_kex_test_negotiate_aead_defaults() ) return 1;
	if ( xssh_kex_test_negotiate_ctr_with_hmac() ) return 1;
	if ( xssh_kex_test_negotiate_no_match() ) return 1;
	if ( xssh_kex_test_first_kex_packet_follows_skip() ) return 1;
	if ( xssh_kex_test_exchange_hash_sha256() ) return 1;
	if ( xssh_kex_test_exchange_hash_no_space() ) return 1;
	if ( xssh_kex_test_key_derivation_sha256() ) return 1;
	if ( xssh_kex_test_curve25519_shared_secret_vector() ) return 1;
	if ( xssh_kex_test_curve25519_all_zero_rejected() ) return 1;
	if ( xssh_kex_test_ecdh_init_roundtrip() ) return 1;
	if ( xssh_kex_test_ecdh_reply_roundtrip() ) return 1;
	if ( xssh_kex_test_ed25519_hostkey_verify() ) return 1;
	if ( xssh_kex_test_ed25519_hostkey_rejects_bad_blob() ) return 1;

	printf("xssh_kex_test: PASS\n");
	return 0;
}
