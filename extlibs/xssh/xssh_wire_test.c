#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"
#include "xssh.h"

static int xssh_test_fail(const char* sName, const char* sDetail)
{
	printf("FAIL %s: %s\n", sName, sDetail ? sDetail : "");
	return 1;
}

static int xssh_test_expect_mem(const char* sName, const void* pA, const void* pB, size_t iLen)
{
	if ( memcmp(pA, pB, iLen) != 0 ) {
		return xssh_test_fail(sName, "memory mismatch");
	}
	return 0;
}

static int xssh_test_basic_api(void)
{
	xsshconfig cfg;
	xsshresult ret;
	xsshauth auth;
	xsshpty pty;

	xsshConfigInit(&cfg);
	xsshResultInit(&ret);
	xsshAuthInit(&auth);
	xsshPtyInit(&pty);

	if ( cfg.iPort != 22u || cfg.iHostKeyPolicy != XSSH_HOSTKEY_STRICT ) {
		return xssh_test_fail("basic_api", "bad config defaults");
	}
	if ( ret.iStage != XSSH_STAGE_NONE || auth.sPassword != NULL ) {
		return xssh_test_fail("basic_api", "bad zero init");
	}
	if ( strcmp(pty.sTerm, "xterm-256color") != 0 || pty.iCols != 80u || pty.iRows != 24u ) {
		return xssh_test_fail("basic_api", "bad pty defaults");
	}
	return 0;
}

static int xssh_test_wire_numbers(void)
{
	uint8 arrBuf[32];
	const uint8 arrExpected[] = {
		0x12u,
		0x01u,
		0x89u, 0xabu, 0xcdu, 0xefu,
		0x01u, 0x23u, 0x45u, 0x67u, 0x89u, 0xabu, 0xcdu, 0xefu
	};
	xsshwriter wr;
	xsshreader rd;
	uint8 iU8 = 0u;
	bool bValue = FALSE;
	uint32 iU32 = 0u;
	uint64 iU64 = 0u;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshWireWriteU8(&wr, 0x12u) != XSSH_OK ||
		xsshWireWriteBool(&wr, TRUE) != XSSH_OK ||
		xsshWireWriteU32(&wr, 0x89abcdefu) != XSSH_OK ||
		xsshWireWriteU64(&wr, 0x0123456789abcdefull) != XSSH_OK ) {
		return xssh_test_fail("wire_numbers", "write failed");
	}
	if ( wr.iLen != sizeof(arrExpected) ) {
		return xssh_test_fail("wire_numbers", "bad encoded length");
	}
	if ( xssh_test_expect_mem("wire_numbers", arrBuf, arrExpected, sizeof(arrExpected)) ) {
		return 1;
	}

	xsshReaderInit(&rd, arrBuf, wr.iLen);
	if ( xsshWireReadU8(&rd, &iU8) != XSSH_OK ||
		xsshWireReadBool(&rd, &bValue) != XSSH_OK ||
		xsshWireReadU32(&rd, &iU32) != XSSH_OK ||
		xsshWireReadU64(&rd, &iU64) != XSSH_OK ) {
		return xssh_test_fail("wire_numbers", "read failed");
	}
	if ( iU8 != 0x12u || !bValue || iU32 != 0x89abcdefu || iU64 != 0x0123456789abcdefull ) {
		return xssh_test_fail("wire_numbers", "decoded value mismatch");
	}
	return 0;
}

static int xssh_test_wire_partial_is_atomic(void)
{
	uint8 arrU64Half[] = { 0x01u, 0x02u, 0x03u, 0x04u, 0x05u };
	uint8 arrBuf[4];
	xsshreader rd;
	xsshwriter wr;
	uint64 iValue = 0u;

	xsshReaderInit(&rd, arrU64Half, sizeof(arrU64Half));
	if ( xsshWireReadU64(&rd, &iValue) != XSSH_WIRE_NEED_MORE || rd.iPos != 0u ) {
		return xssh_test_fail("wire_partial", "u64 partial read changed reader position");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshWireWriteU64(&wr, 0x0102030405060708ull) != XSSH_ERR_NO_SPACE || wr.iLen != 0u ) {
		return xssh_test_fail("wire_partial", "u64 partial write changed writer length");
	}
	return 0;
}

static int xssh_test_wire_string(void)
{
	const uint8 arrValue[] = { 'a', 'b', 'c', 0u, 'd' };
	const uint8 arrExpected[] = { 0u, 0u, 0u, 5u, 'a', 'b', 'c', 0u, 'd' };
	uint8 arrBuf[32];
	xsshwriter wr;
	xsshreader rd;
	xsshslice s;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshWireWriteString(&wr, arrValue, sizeof(arrValue)) != XSSH_OK ) {
		return xssh_test_fail("wire_string", "write failed");
	}
	if ( wr.iLen != sizeof(arrExpected) ) {
		return xssh_test_fail("wire_string", "bad encoded length");
	}
	if ( xssh_test_expect_mem("wire_string", arrBuf, arrExpected, sizeof(arrExpected)) ) {
		return 1;
	}

	xsshReaderInit(&rd, arrBuf, wr.iLen - 1u);
	if ( xsshWireReadString(&rd, &s) != XSSH_WIRE_NEED_MORE || rd.iPos != 0u ) {
		return xssh_test_fail("wire_string", "partial string did not roll back");
	}

	xsshReaderInit(&rd, arrBuf, wr.iLen);
	if ( xsshWireReadString(&rd, &s) != XSSH_OK || s.iLen != sizeof(arrValue) ) {
		return xssh_test_fail("wire_string", "read failed");
	}
	if ( xssh_test_expect_mem("wire_string", s.pData, arrValue, sizeof(arrValue)) ) {
		return 1;
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshWireWriteString(&wr, "", ((size_t)0xffffffffu) + 1u) != XSSH_ERR_OVERFLOW || wr.iLen != 0u ) {
		return xssh_test_fail("wire_string", "oversized string write was not rejected atomically");
	}
	return 0;
}

static int xssh_test_name_list(void)
{
	const char* sList = "curve25519-sha256,ecdh-sha2-nistp256,ssh-ed25519";
	uint8 arrBuf[128];
	xsshwriter wr;
	xsshreader rd;
	xsshslice s;

	if ( !xsshWireNameListIsValid("", 0u) ||
		!xsshWireNameListIsValid(sList, strlen(sList)) ||
		xsshWireNameListIsValid("a,,b", 4u) ||
		xsshWireNameListIsValid(",a", 2u) ||
		xsshWireNameListIsValid("a,", 2u) ) {
		return xssh_test_fail("name_list", "validation mismatch");
	}
	if ( !xsshWireNameListHasDuplicate("a,b,a", 5u) ||
		xsshWireNameListHasDuplicate("a,b,c", 5u) ) {
		return xssh_test_fail("name_list", "duplicate detection mismatch");
	}
	if ( !xsshWireNameListContains(sList, strlen(sList), "ecdh-sha2-nistp256", strlen("ecdh-sha2-nistp256")) ||
		xsshWireNameListContains(sList, strlen(sList), "sha2", strlen("sha2")) ) {
		return xssh_test_fail("name_list", "contains mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshWireWriteNameList(&wr, sList) != XSSH_OK ) {
		return xssh_test_fail("name_list", "write failed");
	}
	xsshReaderInit(&rd, arrBuf, wr.iLen);
	if ( xsshWireReadString(&rd, &s) != XSSH_OK || s.iLen != strlen(sList) ||
		memcmp(s.pData, sList, s.iLen) != 0 ) {
		return xssh_test_fail("name_list", "roundtrip mismatch");
	}
	return 0;
}

static int xssh_test_mpint_positive(void)
{
	const uint8 arrHighBit[] = { 0x80u };
	const uint8 arrExpectedHighBit[] = { 0u, 0u, 0u, 2u, 0u, 0x80u };
	const uint8 arrZeroExpected[] = { 0u, 0u, 0u, 0u };
	const uint8 arrBadNonCanonical[] = { 0u, 0u, 0u, 2u, 0u, 0x7fu };
	const uint8 arrBadNegative[] = { 0u, 0u, 0u, 1u, 0x80u };
	uint8 arrBuf[32];
	xsshwriter wr;
	xsshreader rd;
	xsshslice s;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshWireWriteMpintPositive(&wr, NULL, 0u) != XSSH_OK ||
		wr.iLen != sizeof(arrZeroExpected) ||
		memcmp(arrBuf, arrZeroExpected, sizeof(arrZeroExpected)) != 0 ) {
		return xssh_test_fail("mpint", "zero encoding mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshWireWriteMpintPositive(&wr, arrHighBit, sizeof(arrHighBit)) != XSSH_OK ||
		wr.iLen != sizeof(arrExpectedHighBit) ||
		memcmp(arrBuf, arrExpectedHighBit, sizeof(arrExpectedHighBit)) != 0 ) {
		return xssh_test_fail("mpint", "high-bit positive encoding mismatch");
	}
	xsshReaderInit(&rd, arrBuf, wr.iLen);
	if ( xsshWireReadMpintPositive(&rd, &s) != XSSH_OK ||
		s.iLen != 2u || s.pData[0] != 0u || s.pData[1] != 0x80u ) {
		return xssh_test_fail("mpint", "high-bit positive read mismatch");
	}

	xsshReaderInit(&rd, arrBadNonCanonical, sizeof(arrBadNonCanonical));
	if ( xsshWireReadMpintPositive(&rd, &s) != XSSH_ERR_INVALID ) {
		return xssh_test_fail("mpint", "non-canonical mpint accepted");
	}
	xsshReaderInit(&rd, arrBadNegative, sizeof(arrBadNegative));
	if ( xsshWireReadMpintPositive(&rd, &s) != XSSH_ERR_INVALID ) {
		return xssh_test_fail("mpint", "negative mpint accepted as positive");
	}
	return 0;
}

static int xssh_test_mpint_signed(void)
{
	const uint8 arrOne[] = { 0x01u };
	const uint8 arr129[] = { 0x81u };
	const uint8 arrExpectedMinusOne[] = { 0u, 0u, 0u, 1u, 0xffu };
	const uint8 arrExpectedMinus129[] = { 0u, 0u, 0u, 2u, 0xffu, 0x7fu };
	const uint8 arrPositive128[] = { 0u, 0u, 0u, 2u, 0u, 0x80u };
	const uint8 arrBadNegativeExtend[] = { 0u, 0u, 0u, 2u, 0xffu, 0x80u };
	uint8 arrBuf[32];
	xsshwriter wr;
	xsshreader rd;
	xsshslice s;
	bool bNegative = FALSE;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshWireWriteMpintSigned(&wr, arrOne, sizeof(arrOne), TRUE) != XSSH_OK ||
		wr.iLen != sizeof(arrExpectedMinusOne) ||
		memcmp(arrBuf, arrExpectedMinusOne, sizeof(arrExpectedMinusOne)) != 0 ) {
		return xssh_test_fail("mpint_signed", "minus one encoding mismatch");
	}
	xsshReaderInit(&rd, arrBuf, wr.iLen);
	if ( xsshWireReadMpintSigned(&rd, &s, &bNegative) != XSSH_OK ||
		!bNegative ||
		s.iLen != 1u ||
		s.pData[0] != 0xffu ) {
		return xssh_test_fail("mpint_signed", "minus one read mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshWireWriteMpintSigned(&wr, arr129, sizeof(arr129), TRUE) != XSSH_OK ||
		wr.iLen != sizeof(arrExpectedMinus129) ||
		memcmp(arrBuf, arrExpectedMinus129, sizeof(arrExpectedMinus129)) != 0 ) {
		return xssh_test_fail("mpint_signed", "minus 129 encoding mismatch");
	}
	xsshReaderInit(&rd, arrBuf, wr.iLen);
	if ( xsshWireReadMpintSigned(&rd, &s, &bNegative) != XSSH_OK ||
		!bNegative ||
		s.iLen != 2u ||
		s.pData[0] != 0xffu ||
		s.pData[1] != 0x7fu ) {
		return xssh_test_fail("mpint_signed", "minus 129 read mismatch");
	}

	xsshReaderInit(&rd, arrPositive128, sizeof(arrPositive128));
	if ( xsshWireReadMpintSigned(&rd, &s, &bNegative) != XSSH_OK ||
		bNegative ||
		s.iLen != 2u ||
		s.pData[0] != 0u ||
		s.pData[1] != 0x80u ) {
		return xssh_test_fail("mpint_signed", "positive signed read mismatch");
	}

	xsshReaderInit(&rd, arrBadNegativeExtend, sizeof(arrBadNegativeExtend));
	if ( xsshWireReadMpintSigned(&rd, &s, &bNegative) != XSSH_ERR_INVALID ) {
		return xssh_test_fail("mpint_signed", "non-canonical negative accepted");
	}
	return 0;
}

static int xssh_test_banner(void)
{
	const char* sWithPrelude = "comment line\r\nSSH-2.0-xssh-test\r\nignored";
	const char* sLfOnly = "SSH-1.99-compat\n";
	const char* sInvalid = "SSH-1.5-old\r\n";
	xsshslice s;
	size_t iConsumed = 0u;
	int iRet;

	iRet = xsshWireReadBanner(sWithPrelude, strlen(sWithPrelude), &s, &iConsumed);
	if ( iRet != XSSH_OK || s.iLen != strlen("SSH-2.0-xssh-test") ||
		memcmp(s.pData, "SSH-2.0-xssh-test", s.iLen) != 0 ||
		iConsumed != strlen("comment line\r\nSSH-2.0-xssh-test\r\n") ) {
		return xssh_test_fail("banner", "prelude banner parse mismatch");
	}

	iRet = xsshWireReadBanner(sLfOnly, strlen(sLfOnly), &s, &iConsumed);
	if ( iRet != XSSH_OK || s.iLen != strlen("SSH-1.99-compat") ) {
		return xssh_test_fail("banner", "lf banner parse mismatch");
	}

	if ( xsshWireReadBanner("SSH-2.0-partial", strlen("SSH-2.0-partial"), &s, &iConsumed) != XSSH_WIRE_NEED_MORE ) {
		return xssh_test_fail("banner", "partial banner mismatch");
	}
	if ( xsshWireReadBanner(sInvalid, strlen(sInvalid), &s, &iConsumed) != XSSH_ERR_INVALID ) {
		return xssh_test_fail("banner", "invalid version accepted");
	}
	return 0;
}

int main(void)
{
	if ( xssh_test_basic_api() ) return 1;
	if ( xssh_test_wire_numbers() ) return 1;
	if ( xssh_test_wire_partial_is_atomic() ) return 1;
	if ( xssh_test_wire_string() ) return 1;
	if ( xssh_test_name_list() ) return 1;
	if ( xssh_test_mpint_positive() ) return 1;
	if ( xssh_test_mpint_signed() ) return 1;
	if ( xssh_test_banner() ) return 1;

	printf("xssh_wire_test: PASS\n");
	return 0;
}
