#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"
#include "xssh.h"

static int xssh_known_test_fail(const char* sName, const char* sDetail)
{
	printf("FAIL %s: %s\n", sName, sDetail ? sDetail : "");
	return 1;
}

static bool xssh_known_slice_eq(xsshslice s, const char* sText)
{
	size_t iLen = strlen(sText);
	return (s.iLen == iLen && memcmp(s.pData, sText, iLen) == 0) ? TRUE : FALSE;
}

static int xssh_known_make_ed25519_line(char* sLine, size_t iLineCap, uint8* pPub, uint8* pBlob, size_t* pBlobLen, const char* sPrefix)
{
	uint8 arrSeed[32];
	xsshwriter wr;
	str sB64;
	size_t i;
	int iWritten;

	for ( i = 0u; i < sizeof(arrSeed); ++i ) {
		arrSeed[i] = (uint8)(0x50u + i);
	}
	xrtEd25519PublicKey(pPub, arrSeed);
	xsshWriterInit(&wr, pBlob, 128u);
	if ( xsshPublicKeyWriteEd25519(&wr, pPub) != XSSH_OK ) {
		return xssh_known_test_fail("make_line", "key blob write failed");
	}
	*pBlobLen = wr.iLen;
	sB64 = xrtBase64Encode(pBlob, wr.iLen, NULL);
	if ( sB64 == NULL ) {
		return xssh_known_test_fail("make_line", "base64 encode failed");
	}
	iWritten = snprintf(sLine, iLineCap, "%sssh-ed25519 %s generated-comment", sPrefix ? sPrefix : "", sB64);
	xrtFree(sB64);
	if ( iWritten <= 0 || (size_t)iWritten >= iLineCap ) {
		return xssh_known_test_fail("make_line", "line buffer too small");
	}
	return 0;
}

static int xssh_known_make_hashed_host(char* sOut, size_t iOutCap, const char* sHost, uint16 iPort)
{
	uint8 arrSalt[XSSH_SHA1_SIZE];
	uint8 arrHash[XSSH_SHA1_SIZE];
	char sHostPattern[256];
	str sSaltB64;
	str sHashB64;
	size_t i;
	int iWritten;

	if ( sOut == NULL || iOutCap == 0u || sHost == NULL ) {
		return xssh_known_test_fail("make_hashed_host", "invalid args");
	}
	for ( i = 0u; i < sizeof(arrSalt); ++i ) {
		arrSalt[i] = (uint8)(0x20u + i);
	}
	if ( iPort == XSSH_DEFAULT_PORT ) {
		iWritten = snprintf(sHostPattern, sizeof(sHostPattern), "%s", sHost);
	} else {
		iWritten = snprintf(sHostPattern, sizeof(sHostPattern), "[%s]:%u", sHost, (unsigned)iPort);
	}
	if ( iWritten <= 0 || (size_t)iWritten >= sizeof(sHostPattern) ||
		xsshCryptoHMACSHA1(arrSalt, sizeof(arrSalt), (const uint8*)sHostPattern, strlen(sHostPattern), arrHash) != XSSH_OK ) {
		return xssh_known_test_fail("make_hashed_host", "hash failed");
	}
	sSaltB64 = xrtBase64Encode(arrSalt, sizeof(arrSalt), NULL);
	sHashB64 = xrtBase64Encode(arrHash, sizeof(arrHash), NULL);
	if ( sSaltB64 == NULL || sHashB64 == NULL ) {
		if ( sSaltB64 != NULL ) xrtFree(sSaltB64);
		if ( sHashB64 != NULL ) xrtFree(sHashB64);
		return xssh_known_test_fail("make_hashed_host", "base64 failed");
	}
	iWritten = snprintf(sOut, iOutCap, "|1|%s|%s", sSaltB64, sHashB64);
	xrtFree(sSaltB64);
	xrtFree(sHashB64);
	return (iWritten > 0 && (size_t)iWritten < iOutCap) ? 0 : xssh_known_test_fail("make_hashed_host", "buffer too small");
}

static size_t xssh_known_count_text(const char* sText, const char* sNeedle)
{
	size_t iCount = 0u;
	size_t iNeedleLen;
	const char* p;

	if ( sText == NULL || sNeedle == NULL || sNeedle[0] == '\0' ) {
		return 0u;
	}
	iNeedleLen = strlen(sNeedle);
	p = sText;
	while ( (p = strstr(p, sNeedle)) != NULL ) {
		iCount++;
		p += iNeedleLen;
	}
	return iCount;
}

typedef struct {
	xssh_session_t tSession;
	xsshslice tBlob;
	int iRet;
} xssh_known_accept_new_thread_ctx;

static uint32 xssh_known_accept_new_thread(ptr pArg)
{
	xssh_known_accept_new_thread_ctx* pCtx = (xssh_known_accept_new_thread_ctx*)pArg;

	if ( pCtx == NULL ) {
		return 1u;
	}
	pCtx->iRet = xsshTransportCheckHostKey(&pCtx->tSession, pCtx->tBlob);
	return (pCtx->iRet == XSSH_OK) ? 0u : 2u;
}

static int xssh_known_test_base64_decode(void)
{
	uint8 arrOut[8];
	size_t iOut = 0u;

	if ( xsshBase64DecodeTo("TWFu", 4u, arrOut, sizeof(arrOut), &iOut) != XSSH_OK ||
		iOut != 3u ||
		memcmp(arrOut, "Man", 3u) != 0 ) {
		return xssh_known_test_fail("base64", "Man mismatch");
	}
	if ( xsshBase64DecodeTo("TWE=", 4u, arrOut, sizeof(arrOut), &iOut) != XSSH_OK ||
		iOut != 2u ||
		memcmp(arrOut, "Ma", 2u) != 0 ) {
		return xssh_known_test_fail("base64", "Ma mismatch");
	}
	if ( xsshBase64DecodeTo("TQ==", 4u, arrOut, sizeof(arrOut), &iOut) != XSSH_OK ||
		iOut != 1u ||
		arrOut[0] != 'M' ) {
		return xssh_known_test_fail("base64", "M mismatch");
	}
	if ( xsshBase64DecodeTo("TQ", 2u, arrOut, sizeof(arrOut), &iOut) != XSSH_ERR_INVALID ||
		xsshBase64DecodeTo("T@==", 4u, arrOut, sizeof(arrOut), &iOut) != XSSH_ERR_INVALID ||
		xsshBase64DecodeTo("TWFu", 4u, arrOut, 2u, &iOut) != XSSH_ERR_NO_SPACE ) {
		return xssh_known_test_fail("base64", "invalid handling mismatch");
	}
	return 0;
}

static int xssh_known_test_public_key_line(void)
{
	char sLine[512];
	uint8 arrPub[32];
	uint8 arrBlob[128];
	uint8 arrDecoded[128];
	size_t iBlobLen = 0u;
	xsshopensshpublickeyline line;

	if ( xssh_known_make_ed25519_line(sLine, sizeof(sLine), arrPub, arrBlob, &iBlobLen, "") ) {
		return 1;
	}
	if ( xsshOpenSshPublicKeyLineRead(sLine, strlen(sLine), arrDecoded, sizeof(arrDecoded), &line) != XSSH_OK ||
		!xssh_known_slice_eq(line.tAlgorithm, "ssh-ed25519") ||
		!xssh_known_slice_eq(line.tComment, "generated-comment") ||
		line.tBlob.iLen != iBlobLen ||
		memcmp(line.tBlob.pData, arrBlob, iBlobLen) != 0 ||
		line.tKey.tKeyData.iLen != 32u ||
		memcmp(line.tKey.tKeyData.pData, arrPub, 32u) != 0 ) {
		return xssh_known_test_fail("public_key_line", "plain line mismatch");
	}

	if ( xssh_known_make_ed25519_line(sLine, sizeof(sLine), arrPub, arrBlob, &iBlobLen, "no-pty ") ) {
		return 1;
	}
	if ( xsshOpenSshPublicKeyLineRead(sLine, strlen(sLine), arrDecoded, sizeof(arrDecoded), &line) != XSSH_OK ||
		!xssh_known_slice_eq(line.tComment, "generated-comment") ) {
		return xssh_known_test_fail("public_key_line", "option-prefixed line mismatch");
	}

	if ( xssh_known_make_ed25519_line(sLine, sizeof(sLine), arrPub, arrBlob, &iBlobLen, "command=\"echo hello\",no-pty ") ) {
		return 1;
	}
	if ( xsshOpenSshPublicKeyLineRead(sLine, strlen(sLine), arrDecoded, sizeof(arrDecoded), &line) != XSSH_OK ||
		!xssh_known_slice_eq(line.tAlgorithm, "ssh-ed25519") ||
		!xssh_known_slice_eq(line.tComment, "generated-comment") ||
		line.tBlob.iLen != iBlobLen ) {
		return xssh_known_test_fail("public_key_line", "quoted option line mismatch");
	}
	return 0;
}

static int xssh_known_test_known_hosts_line(void)
{
	char sKeyLine[512];
	char sKnownLine[768];
	char sMultiLine[1024];
	char sHashHost[256];
	uint8 arrPub[32];
	uint8 arrBlob[128];
	uint8 arrDecoded[128];
	size_t iBlobLen = 0u;
	const char* sAlgPos;
	const char* sOtherTypeLine = "example.com rsa-sha2-256 not-base64 ignored-comment";
	xsshknownhostline line;

	if ( xssh_known_make_ed25519_line(sKeyLine, sizeof(sKeyLine), arrPub, arrBlob, &iBlobLen, "") ) {
		return 1;
	}
	sAlgPos = strstr(sKeyLine, "ssh-ed25519");
	if ( sAlgPos == NULL ) {
		return xssh_known_test_fail("known_hosts", "generated key line missing algorithm");
	}
	snprintf(sKnownLine, sizeof(sKnownLine), "example.com,[example.org]:2222 %s", sAlgPos);
	if ( xsshKnownHostLineRead(sKnownLine, strlen(sKnownLine), arrDecoded, sizeof(arrDecoded), &line) != XSSH_OK ||
		!xsshKnownHostMatches(line.tHosts, "example.com", 22u) ||
		!xsshKnownHostMatches(line.tHosts, "example.org", 2222u) ||
		xsshKnownHostMatches(line.tHosts, "example.org", 22u) ||
		line.tKey.tKeyData.iLen != 32u ||
		memcmp(line.tKey.tKeyData.pData, arrPub, 32u) != 0 ) {
		return xssh_known_test_fail("known_hosts", "known_hosts parse/match mismatch");
	}

	if ( xsshKnownHostLineReadHeader(sOtherTypeLine, strlen(sOtherTypeLine), &line) != XSSH_OK ||
		!xsshKnownHostMatches(line.tHosts, "example.com", 22u) ||
		!xssh_known_slice_eq(line.tAlgorithm, "rsa-sha2-256") ||
		!xssh_known_slice_eq(line.tBase64, "not-base64") ||
		!xssh_known_slice_eq(line.tComment, "ignored-comment") ) {
		return xssh_known_test_fail("known_hosts", "multi key type header mismatch");
	}
	snprintf(sMultiLine, sizeof(sMultiLine), "%s\n%s", sOtherTypeLine, sKnownLine);
	if ( xsshKnownHostsCheckText(sMultiLine, strlen(sMultiLine), "example.com", 22u,
			(xsshslice){ arrBlob, iBlobLen }, XSSH_HOSTKEY_STRICT, NULL, NULL, 0u) != XSSH_OK ) {
		return xssh_known_test_fail("known_hosts", "multi key type strict match failed");
	}
	if ( xsshKnownHostsCheckText(sOtherTypeLine, strlen(sOtherTypeLine), "example.com", 22u,
			(xsshslice){ arrBlob, iBlobLen }, XSSH_HOSTKEY_STRICT, NULL, NULL, 0u) != XSSH_ERR_HOSTKEY_UNKNOWN ) {
		return xssh_known_test_fail("known_hosts", "different key type should not mismatch");
	}

	if ( xssh_known_make_hashed_host(sHashHost, sizeof(sHashHost), "hashed.example", 22u) ) {
		return 1;
	}
	snprintf(sKnownLine, sizeof(sKnownLine), "%s %s", sHashHost, sAlgPos);
	if ( xsshKnownHostLineRead(sKnownLine, strlen(sKnownLine), arrDecoded, sizeof(arrDecoded), &line) != XSSH_OK ||
		!xsshKnownHostMatches(line.tHosts, "hashed.example", 22u) ||
		xsshKnownHostMatches(line.tHosts, "other.example", 22u) ) {
		return xssh_known_test_fail("known_hosts", "hashed default host mismatch");
	}

	if ( xssh_known_make_hashed_host(sHashHost, sizeof(sHashHost), "hashed.example", 2200u) ) {
		return 1;
	}
	snprintf(sKnownLine, sizeof(sKnownLine), "%s %s", sHashHost, sAlgPos);
	if ( xsshKnownHostLineRead(sKnownLine, strlen(sKnownLine), arrDecoded, sizeof(arrDecoded), &line) != XSSH_OK ||
		!xsshKnownHostMatches(line.tHosts, "hashed.example", 2200u) ||
		xsshKnownHostMatches(line.tHosts, "hashed.example", 22u) ) {
		return xssh_known_test_fail("known_hosts", "hashed port host mismatch");
	}
	return 0;
}

static int xssh_known_test_hostkey_policy(void)
{
	char sKnownLine[768];
	char sBadLine[768];
	char sCandidate[768];
	char sOtherTypeLine[128];
	char sRaceLine[8200];
	char sLockPath[512];
	uint8 arrPub[32];
	uint8 arrBlob[128];
	uint8 arrBadBlob[128];
	uint8 arrDecoded[128];
	size_t iBlobLen = 0u;
	xsshslice blob;
	xsshknownhostline candidate;
	xssh_session_t session;
	xssh_known_accept_new_thread_ctx tRaceCtx;
	xthread pRaceThread = NULL;
	bool bAcceptedNew = FALSE;
	str sBadB64;
	char* sWrittenText = NULL;
	size_t iWrittenLen = 0u;
	int iWritten;

	if ( xssh_known_make_ed25519_line(sKnownLine, sizeof(sKnownLine), arrPub, arrBlob, &iBlobLen, "example.com ") ) {
		return 1;
	}
	blob.pData = arrBlob;
	blob.iLen = iBlobLen;
	if ( xsshKnownHostsCheckText(sKnownLine, strlen(sKnownLine), "example.com", 22u, blob,
			XSSH_HOSTKEY_STRICT, &bAcceptedNew, sCandidate, sizeof(sCandidate)) != XSSH_OK ||
		bAcceptedNew ) {
		return xssh_known_test_fail("known_policy", "strict match failed");
	}
	if ( xsshKnownHostsCheckText(sKnownLine, strlen(sKnownLine), "unknown.example", 22u, blob,
			XSSH_HOSTKEY_STRICT, &bAcceptedNew, sCandidate, sizeof(sCandidate)) != XSSH_ERR_HOSTKEY_UNKNOWN ) {
		return xssh_known_test_fail("known_policy", "strict unknown accepted");
	}
	snprintf(sOtherTypeLine, sizeof(sOtherTypeLine), "example.com rsa-sha2-256 not-base64 ignored-comment");
	memset(sCandidate, 0, sizeof(sCandidate));
	if ( xsshKnownHostsCheckText(sOtherTypeLine, strlen(sOtherTypeLine), "example.com", 22u, blob,
			XSSH_HOSTKEY_ACCEPT_NEW, &bAcceptedNew, sCandidate, sizeof(sCandidate)) != XSSH_OK ||
		!bAcceptedNew ||
		strstr(sCandidate, "example.com ssh-ed25519 ") == NULL ) {
		return xssh_known_test_fail("known_policy", "accept-new same host different key type failed");
	}
	memset(sCandidate, 0, sizeof(sCandidate));
	if ( xsshKnownHostsCheckText(NULL, 0u, "new.example", 2222u, blob,
			XSSH_HOSTKEY_ACCEPT_NEW, &bAcceptedNew, sCandidate, sizeof(sCandidate)) != XSSH_OK ||
		!bAcceptedNew ||
		xsshKnownHostLineRead(sCandidate, strlen(sCandidate), arrDecoded, sizeof(arrDecoded), &candidate) != XSSH_OK ||
		!xsshKnownHostMatches(candidate.tHosts, "new.example", 2222u) ||
		candidate.tBlob.iLen != iBlobLen ||
		memcmp(candidate.tBlob.pData, arrBlob, iBlobLen) != 0 ) {
		return xssh_known_test_fail("known_policy", "accept-new candidate mismatch");
	}
	(void)xrtDirDelete((str)"xssh_known_hosts_accept_new_tmp");
	memset(&session, 0, sizeof(session));
	xsshConfigInit(&session.tConfig);
	session.tConfig.sHost = "created.example";
	session.tConfig.iPort = 22u;
	session.tConfig.iHostKeyPolicy = XSSH_HOSTKEY_ACCEPT_NEW;
	session.tConfig.sKnownHostsPath = "xssh_known_hosts_accept_new_tmp\\nested\\known_hosts";
	if ( xsshTransportCheckHostKey(&session, blob) != XSSH_OK ) {
		(void)xrtDirDelete((str)"xssh_known_hosts_accept_new_tmp");
		return xssh_known_test_fail("known_policy", "accept-new nested path failed");
	}
	if ( !session.tResult.bHostKeyAcceptedNew || session.tResult.bHostKeyInsecure ) {
		(void)xrtDirDelete((str)"xssh_known_hosts_accept_new_tmp");
		return xssh_known_test_fail("known_policy", "accept-new result flags mismatch");
	}
	sWrittenText = (char*)xrtFileGetAll((str)session.tConfig.sKnownHostsPath, &iWrittenLen);
	if ( sWrittenText == NULL || iWrittenLen == 0u || strstr(sWrittenText, "created.example ssh-ed25519 ") == NULL ) {
		if ( sWrittenText != NULL ) xrtFree(sWrittenText);
		(void)xrtDirDelete((str)"xssh_known_hosts_accept_new_tmp");
		return xssh_known_test_fail("known_policy", "accept-new nested file mismatch");
	}
	xrtFree(sWrittenText);
	(void)xrtDirDelete((str)"xssh_known_hosts_accept_new_tmp");

	(void)xrtDirDelete((str)"xssh_known_hosts_accept_new_race_tmp");
	if ( xsshKnownHostsCreateParentDir("xssh_known_hosts_accept_new_race_tmp\\nested\\known_hosts") != XSSH_OK ||
		xsshKnownHostsAcquireAppendLock("xssh_known_hosts_accept_new_race_tmp\\nested\\known_hosts", sLockPath, sizeof(sLockPath)) != XSSH_OK ) {
		(void)xrtDirDelete((str)"xssh_known_hosts_accept_new_race_tmp");
		return xssh_known_test_fail("known_policy", "accept-new race lock setup failed");
	}
	memset(&tRaceCtx, 0, sizeof(tRaceCtx));
	xsshConfigInit(&tRaceCtx.tSession.tConfig);
	tRaceCtx.tSession.tConfig.sHost = "race.example";
	tRaceCtx.tSession.tConfig.iPort = 22u;
	tRaceCtx.tSession.tConfig.iHostKeyPolicy = XSSH_HOSTKEY_ACCEPT_NEW;
	tRaceCtx.tSession.tConfig.sKnownHostsPath = "xssh_known_hosts_accept_new_race_tmp\\nested\\known_hosts";
	tRaceCtx.tBlob = blob;
	pRaceThread = xrtThreadCreate((ptr)xssh_known_accept_new_thread, &tRaceCtx, 0u);
	if ( pRaceThread == NULL ) {
		xsshKnownHostsReleaseAppendLock(sLockPath);
		(void)xrtDirDelete((str)"xssh_known_hosts_accept_new_race_tmp");
		return xssh_known_test_fail("known_policy", "accept-new race thread failed");
	}
	xrtSleep(50u);
	if ( xsshKnownHostMakeLine(sCandidate, sizeof(sCandidate), "race.example", 22u, blob) != XSSH_OK ) {
		xsshKnownHostsReleaseAppendLock(sLockPath);
		xrtThreadWait(pRaceThread);
		xrtThreadDestroy(pRaceThread);
		(void)xrtDirDelete((str)"xssh_known_hosts_accept_new_race_tmp");
		return xssh_known_test_fail("known_policy", "accept-new race line build failed");
	}
	iWritten = snprintf(sRaceLine, sizeof(sRaceLine), "%s\n", sCandidate);
	if ( iWritten <= 0 || (size_t)iWritten >= sizeof(sRaceLine) ||
		xrtFileAppend((str)"xssh_known_hosts_accept_new_race_tmp\\nested\\known_hosts", (str)sRaceLine, (size_t)iWritten, XRT_CP_UTF8) != iWritten ) {
		xsshKnownHostsReleaseAppendLock(sLockPath);
		xrtThreadWait(pRaceThread);
		xrtThreadDestroy(pRaceThread);
		(void)xrtDirDelete((str)"xssh_known_hosts_accept_new_race_tmp");
		return xssh_known_test_fail("known_policy", "accept-new race manual append failed");
	}
	xsshKnownHostsReleaseAppendLock(sLockPath);
	xrtThreadWait(pRaceThread);
	xrtThreadDestroy(pRaceThread);
	if ( tRaceCtx.iRet != XSSH_OK || tRaceCtx.tSession.tResult.bHostKeyAcceptedNew ) {
		(void)xrtDirDelete((str)"xssh_known_hosts_accept_new_race_tmp");
		return xssh_known_test_fail("known_policy", "accept-new race duplicate append not suppressed");
	}
	sWrittenText = (char*)xrtFileGetAll((str)"xssh_known_hosts_accept_new_race_tmp\\nested\\known_hosts", &iWrittenLen);
	(void)iWrittenLen;
	if ( sWrittenText == NULL || xssh_known_count_text(sWrittenText, "race.example ssh-ed25519 ") != 1u ) {
		if ( sWrittenText != NULL ) xrtFree(sWrittenText);
		(void)xrtDirDelete((str)"xssh_known_hosts_accept_new_race_tmp");
		return xssh_known_test_fail("known_policy", "accept-new race file contains duplicate or missing line");
	}
	xrtFree(sWrittenText);
	(void)xrtDirDelete((str)"xssh_known_hosts_accept_new_race_tmp");

	memcpy(arrBadBlob, arrBlob, iBlobLen);
	arrBadBlob[iBlobLen - 1u] ^= 0x55u;
	sBadB64 = xrtBase64Encode(arrBadBlob, iBlobLen, NULL);
	if ( sBadB64 == NULL ) {
		return xssh_known_test_fail("known_policy", "bad base64 encode failed");
	}
	iWritten = snprintf(sBadLine, sizeof(sBadLine), "example.com ssh-ed25519 %s", sBadB64);
	xrtFree(sBadB64);
	if ( iWritten <= 0 || (size_t)iWritten >= sizeof(sBadLine) ) {
		return xssh_known_test_fail("known_policy", "bad line buffer too small");
	}
	if ( xsshKnownHostsCheckText(sBadLine, strlen(sBadLine), "example.com", 22u, blob,
			XSSH_HOSTKEY_STRICT, &bAcceptedNew, sCandidate, sizeof(sCandidate)) != XSSH_ERR_HOSTKEY_MISMATCH ) {
		return xssh_known_test_fail("known_policy", "mismatch not rejected");
	}
	if ( xsshKnownHostsCheckText(sBadLine, strlen(sBadLine), "example.com", 22u, blob,
			XSSH_HOSTKEY_INSECURE, &bAcceptedNew, NULL, 0u) != XSSH_OK ) {
		return xssh_known_test_fail("known_policy", "insecure explicit policy rejected");
	}
	memset(&session, 0, sizeof(session));
	xsshConfigInit(&session.tConfig);
	session.tConfig.sHost = "example.com";
	session.tConfig.iPort = 22u;
	session.tConfig.iHostKeyPolicy = XSSH_HOSTKEY_INSECURE;
	if ( xsshTransportCheckHostKey(&session, blob) != XSSH_OK ||
		!session.tResult.bHostKeyInsecure ||
		session.tResult.bHostKeyAcceptedNew ||
		strlen(session.tResult.sHostKeyFingerprint) == 0u ) {
		return xssh_known_test_fail("known_policy", "insecure result flags mismatch");
	}
	return 0;
}

static int xssh_known_test_fingerprint(void)
{
	char sKeyLine[512];
	char sFingerprint[128];
	uint8 arrMd5[XSSH_MD5_SIZE];
	static const uint8 arrMd5Abc[XSSH_MD5_SIZE] = {
		0x90u,0x01u,0x50u,0x98u,0x3cu,0xd2u,0x4fu,0xb0u,
		0xd6u,0x96u,0x3fu,0x7du,0x28u,0xe1u,0x7fu,0x72u
	};
	uint8 arrPub[32];
	uint8 arrBlob[128];
	size_t iBlobLen = 0u;
	size_t iLen;

	if ( xsshCryptoMD5((const uint8*)"abc", 3u, arrMd5) != XSSH_OK ||
		memcmp(arrMd5, arrMd5Abc, sizeof(arrMd5Abc)) != 0 ) {
		return xssh_known_test_fail("fingerprint", "md5 vector mismatch");
	}
	if ( xssh_known_make_ed25519_line(sKeyLine, sizeof(sKeyLine), arrPub, arrBlob, &iBlobLen, "") ) {
		return 1;
	}
	if ( xsshHostKeyFingerprintSHA256(sFingerprint, sizeof(sFingerprint), (xsshslice){ arrBlob, iBlobLen }) != XSSH_OK ) {
		return xssh_known_test_fail("fingerprint", "fingerprint build failed");
	}
	iLen = strlen(sFingerprint);
	if ( iLen <= strlen("SHA256:") ||
		memcmp(sFingerprint, "SHA256:", strlen("SHA256:")) != 0 ||
		strchr(sFingerprint, '=') != NULL ) {
		return xssh_known_test_fail("fingerprint", "fingerprint format mismatch");
	}
	memset(sFingerprint, 0, sizeof(sFingerprint));
	if ( xsshHostKeyFingerprintMD5(sFingerprint, sizeof(sFingerprint), (xsshslice){ arrBlob, iBlobLen }) != XSSH_OK ) {
		return xssh_known_test_fail("fingerprint", "md5 fingerprint build failed");
	}
	iLen = strlen(sFingerprint);
	if ( iLen != 51u ||
		memcmp(sFingerprint, "MD5:", strlen("MD5:")) != 0 ||
		sFingerprint[6] != ':' ||
		sFingerprint[9] != ':' ||
		strchr(sFingerprint, '=') != NULL ) {
		return xssh_known_test_fail("fingerprint", "md5 fingerprint format mismatch");
	}
	return 0;
}

static int xssh_known_test_errors(void)
{
	uint8 arrDecoded[8];
	xsshopensshpublickeyline pubLine;
	xsshknownhostline knownLine;

	if ( xsshOpenSshPublicKeyLineRead("# comment", strlen("# comment"), arrDecoded, sizeof(arrDecoded), &pubLine) != XSSH_ERR_INVALID ) {
		return xssh_known_test_fail("known_errors", "comment accepted as key");
	}
	if ( xsshKnownHostLineRead("host ssh-rsa AAAA", strlen("host ssh-rsa AAAA"), arrDecoded, sizeof(arrDecoded), &knownLine) != XSSH_ERR_UNSUPPORTED ) {
		return xssh_known_test_fail("known_errors", "unsupported known_host algorithm accepted");
	}
	return 0;
}

int main(void)
{
	if ( xssh_known_test_base64_decode() ) return 1;
	if ( xssh_known_test_public_key_line() ) return 1;
	if ( xssh_known_test_known_hosts_line() ) return 1;
	if ( xssh_known_test_hostkey_policy() ) return 1;
	if ( xssh_known_test_fingerprint() ) return 1;
	if ( xssh_known_test_errors() ) return 1;

	printf("xssh_known_hosts_test: PASS\n");
	return 0;
}
