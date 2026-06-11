#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"
#include "xssh.h"

static int xssh_auth_test_fail(const char* sName, const char* sDetail)
{
	printf("FAIL %s: %s\n", sName, sDetail ? sDetail : "");
	return 1;
}

static bool xssh_auth_slice_eq(xsshslice s, const char* sText)
{
	size_t iLen = strlen(sText);
	return (s.iLen == iLen && memcmp(s.pData, sText, iLen) == 0) ? TRUE : FALSE;
}

static int xssh_auth_expect_mem(const char* sName, const void* pA, const void* pB, size_t iLen)
{
	if ( memcmp(pA, pB, iLen) != 0 ) {
		return xssh_auth_test_fail(sName, "memory mismatch");
	}
	return 0;
}

static int xssh_auth_test_none_request(void)
{
	uint8 arrBuf[128];
	xsshwriter wr;
	xsshauthrequestmsg req;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshAuthWriteNoneRequest(&wr, "alice") != XSSH_OK ||
		xsshAuthReadRequest(arrBuf, wr.iLen, &req) != XSSH_OK ||
		!xssh_auth_slice_eq(req.tUserName, "alice") ||
		!xssh_auth_slice_eq(req.tServiceName, "ssh-connection") ||
		!xssh_auth_slice_eq(req.tMethodName, "none") ) {
		return xssh_auth_test_fail("auth_none", "roundtrip mismatch");
	}
	return 0;
}

static int xssh_auth_test_password_request(void)
{
	uint8 arrBuf[128];
	xsshwriter wr;
	xsshauthrequestmsg req;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshAuthWritePasswordRequest(&wr, "bob", "secret") != XSSH_OK ||
		xsshAuthReadRequest(arrBuf, wr.iLen, &req) != XSSH_OK ||
		req.bPasswordChange ||
		!xssh_auth_slice_eq(req.tMethodName, "password") ||
		!xssh_auth_slice_eq(req.tPassword, "secret") ) {
		return xssh_auth_test_fail("auth_password", "roundtrip mismatch");
	}
	return 0;
}

static int xssh_auth_test_keyboard_interactive_request(void)
{
	uint8 arrBuf[512];
	xsshwriter wr;
	xsshauthrequestmsg req;
	xsshauthkbdintrequestmsg infoReq;
	xsshauthkbdintresponsemsg infoResp;
	const char* psSinglePrompts[] = { "Password: " };
	const bool arrSingleEcho[] = { FALSE };
	const char* psSingleResponses[] = { "secret" };
	const char* psMultiPrompts[] = { "Password: ", "OTP: " };
	const bool arrMultiEcho[] = { FALSE, FALSE };
	const char* psMultiResponses[] = { "secret", "123456" };

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshAuthWriteKeyboardInteractiveRequest(&wr, "diana", "pam") != XSSH_OK ||
		xsshAuthReadRequest(arrBuf, wr.iLen, &req) != XSSH_OK ||
		!xssh_auth_slice_eq(req.tUserName, "diana") ||
		!xssh_auth_slice_eq(req.tServiceName, "ssh-connection") ||
		!xssh_auth_slice_eq(req.tMethodName, "keyboard-interactive") ||
		!xssh_auth_slice_eq(req.tKbdIntLanguageTag, "") ||
		!xssh_auth_slice_eq(req.tKbdIntSubmethods, "pam") ) {
		return xssh_auth_test_fail("auth_kbdint_request", "request roundtrip mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshAuthWriteKeyboardInteractiveInfoRequest(&wr, "login", "password required",
			psSinglePrompts, arrSingleEcho, 1u) != XSSH_OK ||
		xsshAuthReadKeyboardInteractiveInfoRequest(arrBuf, wr.iLen, &infoReq) != XSSH_OK ||
		!xssh_auth_slice_eq(infoReq.tName, "login") ||
		!xssh_auth_slice_eq(infoReq.tInstruction, "password required") ||
		!xssh_auth_slice_eq(infoReq.tLanguageTag, "") ||
		infoReq.iPromptCount != 1u ||
		!xssh_auth_slice_eq(infoReq.arrPrompts[0], "Password: ") ||
		infoReq.arrEcho[0] ) {
		return xssh_auth_test_fail("auth_kbdint_request", "single prompt mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshAuthWriteKeyboardInteractiveInfoResponse(&wr, psSingleResponses, 1u) != XSSH_OK ||
		xsshAuthReadKeyboardInteractiveInfoResponse(arrBuf, wr.iLen, &infoResp) != XSSH_OK ||
		infoResp.iResponseCount != 1u ||
		!xssh_auth_slice_eq(infoResp.arrResponses[0], "secret") ) {
		return xssh_auth_test_fail("auth_kbdint_request", "single response mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshAuthWriteKeyboardInteractiveInfoRequest(&wr, "login", "two factors",
			psMultiPrompts, arrMultiEcho, 2u) != XSSH_OK ||
		xsshAuthReadKeyboardInteractiveInfoRequest(arrBuf, wr.iLen, &infoReq) != XSSH_OK ||
		infoReq.iPromptCount != 2u ||
		!xssh_auth_slice_eq(infoReq.arrPrompts[0], "Password: ") ||
		!xssh_auth_slice_eq(infoReq.arrPrompts[1], "OTP: ") ) {
		return xssh_auth_test_fail("auth_kbdint_request", "multi prompt mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshAuthWriteKeyboardInteractiveInfoResponse(&wr, psMultiResponses, 2u) != XSSH_OK ||
		xsshAuthReadKeyboardInteractiveInfoResponse(arrBuf, wr.iLen, &infoResp) != XSSH_OK ||
		infoResp.iResponseCount != 2u ||
		!xssh_auth_slice_eq(infoResp.arrResponses[0], "secret") ||
		!xssh_auth_slice_eq(infoResp.arrResponses[1], "123456") ) {
		return xssh_auth_test_fail("auth_kbdint_request", "multi response mismatch");
	}
	return 0;
}

static int xssh_auth_test_publickey_request(void)
{
	uint8 arrKeyBlob[] = { 'k', 'e', 'y' };
	uint8 arrSigBlob[] = { 's', 'i', 'g' };
	uint8 arrBuf[128];
	xsshwriter wr;
	xsshauthrequestmsg req;
	xsshslice keyBlob;
	xsshslice sigBlob;
	xsshslice emptySig;

	keyBlob.pData = arrKeyBlob;
	keyBlob.iLen = sizeof(arrKeyBlob);
	sigBlob.pData = arrSigBlob;
	sigBlob.iLen = sizeof(arrSigBlob);
	emptySig.pData = NULL;
	emptySig.iLen = 0u;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshAuthWritePublicKeyRequest(&wr, "carol", FALSE, "ssh-ed25519",
			keyBlob,
			emptySig) != XSSH_OK ||
		xsshAuthReadRequest(arrBuf, wr.iLen, &req) != XSSH_OK ||
		req.bPublicKeyHasSignature ||
		!xssh_auth_slice_eq(req.tMethodName, "publickey") ||
		!xssh_auth_slice_eq(req.tPublicKeyAlgorithm, "ssh-ed25519") ||
		req.tPublicKeyBlob.iLen != sizeof(arrKeyBlob) ) {
		return xssh_auth_test_fail("auth_publickey", "probe mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshAuthWritePublicKeyRequest(&wr, "carol", TRUE, "ssh-ed25519",
			keyBlob,
			sigBlob) != XSSH_OK ||
		xsshAuthReadRequest(arrBuf, wr.iLen, &req) != XSSH_OK ||
		!req.bPublicKeyHasSignature ||
		req.tSignature.iLen != sizeof(arrSigBlob) ||
		memcmp(req.tSignature.pData, arrSigBlob, sizeof(arrSigBlob)) != 0 ) {
		return xssh_auth_test_fail("auth_publickey", "signed mismatch");
	}
	return 0;
}

static int xssh_auth_test_publickey_sign_data_layout(void)
{
	static const uint8 arrExpected[] = {
		0x00u, 0x00u, 0x00u, 0x03u, 0xaau, 0xbbu, 0xccu, 0x32u,
		0x00u, 0x00u, 0x00u, 0x04u, 0x64u, 0x61u, 0x76u, 0x65u,
		0x00u, 0x00u, 0x00u, 0x0eu, 0x73u, 0x73u, 0x68u, 0x2du,
		0x63u, 0x6fu, 0x6eu, 0x6eu, 0x65u, 0x63u, 0x74u, 0x69u,
		0x6fu, 0x6eu, 0x00u, 0x00u, 0x00u, 0x09u, 0x70u, 0x75u,
		0x62u, 0x6cu, 0x69u, 0x63u, 0x6bu, 0x65u, 0x79u, 0x01u,
		0x00u, 0x00u, 0x00u, 0x0bu, 0x73u, 0x73u, 0x68u, 0x2du,
		0x65u, 0x64u, 0x32u, 0x35u, 0x35u, 0x31u, 0x39u, 0x00u,
		0x00u, 0x00u, 0x03u, 0x01u, 0x02u, 0x03u
	};
	uint8 arrSession[] = { 0xaau, 0xbbu, 0xccu };
	uint8 arrKeyBlob[] = { 1u, 2u, 3u };
	uint8 arrBuf[128];
	xsshwriter wr;
	xsshslice sessionId;
	xsshslice keyBlob;

	sessionId.pData = arrSession;
	sessionId.iLen = sizeof(arrSession);
	keyBlob.pData = arrKeyBlob;
	keyBlob.iLen = sizeof(arrKeyBlob);

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshAuthWritePublicKeySignData(&wr, sessionId, "dave", "ssh-ed25519", keyBlob) != XSSH_OK ) {
		return xssh_auth_test_fail("auth_publickey_sign_data_layout", "write failed");
	}
	if ( wr.iLen != sizeof(arrExpected) ) {
		return xssh_auth_test_fail("auth_publickey_sign_data_layout", "length mismatch");
	}
	return xssh_auth_expect_mem("auth_publickey_sign_data_layout", arrBuf, arrExpected, sizeof(arrExpected));
}

static int xssh_auth_test_publickey_sign_data_ed25519(void)
{
	uint8 arrSeed[32];
	uint8 arrPub[32];
	uint8 arrSession[32];
	uint8 arrKeyBlobBuf[128];
	uint8 arrSignData[256];
	uint8 arrSig[64];
	uint8 arrSigBlobBuf[128];
	uint8 arrReqBuf[512];
	xsshwriter keyWr;
	xsshwriter signWr;
	xsshwriter sigWr;
	xsshwriter reqWr;
	xsshslice sessionId;
	xsshslice keyBlob;
	xsshslice sigBlob;
	xsshauthrequestmsg req;
	xsshpublickey key;
	xsshsignature sig;
	size_t i;

	for ( i = 0u; i < sizeof(arrSeed); ++i ) {
		arrSeed[i] = (uint8)i;
		arrSession[i] = (uint8)(0xa0u + i);
	}
	xrtEd25519PublicKey(arrPub, arrSeed);

	xsshWriterInit(&keyWr, arrKeyBlobBuf, sizeof(arrKeyBlobBuf));
	if ( xsshPublicKeyWriteEd25519(&keyWr, arrPub) != XSSH_OK ) {
		return xssh_auth_test_fail("auth_publickey_sign_data_ed25519", "key blob write failed");
	}
	sessionId.pData = arrSession;
	sessionId.iLen = sizeof(arrSession);
	keyBlob.pData = arrKeyBlobBuf;
	keyBlob.iLen = keyWr.iLen;

	xsshWriterInit(&signWr, arrSignData, sizeof(arrSignData));
	if ( xsshAuthWritePublicKeySignData(&signWr, sessionId, "erin", "ssh-ed25519", keyBlob) != XSSH_OK ||
		!xrtEd25519Sign(arrSig, arrSignData, signWr.iLen, arrSeed) ) {
		return xssh_auth_test_fail("auth_publickey_sign_data_ed25519", "sign failed");
	}
	xsshWriterInit(&sigWr, arrSigBlobBuf, sizeof(arrSigBlobBuf));
	if ( xsshSignatureWrite(&sigWr, "ssh-ed25519", arrSig, sizeof(arrSig)) != XSSH_OK ) {
		return xssh_auth_test_fail("auth_publickey_sign_data_ed25519", "signature blob write failed");
	}
	sigBlob.pData = arrSigBlobBuf;
	sigBlob.iLen = sigWr.iLen;

	xsshWriterInit(&reqWr, arrReqBuf, sizeof(arrReqBuf));
	if ( xsshAuthWritePublicKeyRequest(&reqWr, "erin", TRUE, "ssh-ed25519", keyBlob, sigBlob) != XSSH_OK ||
		xsshAuthReadRequest(arrReqBuf, reqWr.iLen, &req) != XSSH_OK ||
		!req.bPublicKeyHasSignature ||
		!xssh_auth_slice_eq(req.tUserName, "erin") ) {
		return xssh_auth_test_fail("auth_publickey_sign_data_ed25519", "request roundtrip failed");
	}
	if ( xsshPublicKeyReadEd25519(req.tPublicKeyBlob.pData, req.tPublicKeyBlob.iLen, &key) != XSSH_OK ||
		xsshSignatureRead(req.tSignature.pData, req.tSignature.iLen, &sig) != XSSH_OK ||
		xsshHostKeyVerifyEd25519(&key, &sig, arrSignData, signWr.iLen) != XSSH_OK ) {
		return xssh_auth_test_fail("auth_publickey_sign_data_ed25519", "signature verify failed");
	}
	xsshSecureZero(arrSeed, sizeof(arrSeed));
	xsshSecureZero(arrSig, sizeof(arrSig));
	xsshSecureZero(arrSignData, sizeof(arrSignData));
	return 0;
}

static int xssh_auth_test_ed25519_rfc8032_vector(void)
{
	static const uint8 arrSeed[32] = {
		0x9du, 0x61u, 0xb1u, 0x9du, 0xefu, 0xfdu, 0x5au, 0x60u,
		0xbau, 0x84u, 0x4au, 0xf4u, 0x92u, 0xecu, 0x2cu, 0xc4u,
		0x44u, 0x49u, 0xc5u, 0x69u, 0x7bu, 0x32u, 0x69u, 0x19u,
		0x70u, 0x3bu, 0xacu, 0x03u, 0x1cu, 0xaeu, 0x7fu, 0x60u
	};
	static const uint8 arrExpectedPub[32] = {
		0xd7u, 0x5au, 0x98u, 0x01u, 0x82u, 0xb1u, 0x0au, 0xb7u,
		0xd5u, 0x4bu, 0xfeu, 0xd3u, 0xc9u, 0x64u, 0x07u, 0x3au,
		0x0eu, 0xe1u, 0x72u, 0xf3u, 0xdau, 0xa6u, 0x23u, 0x25u,
		0xafu, 0x02u, 0x1au, 0x68u, 0xf7u, 0x07u, 0x51u, 0x1au
	};
	static const uint8 arrExpectedSig[64] = {
		0xe5u, 0x56u, 0x43u, 0x00u, 0xc3u, 0x60u, 0xacu, 0x72u,
		0x90u, 0x86u, 0xe2u, 0xccu, 0x80u, 0x6eu, 0x82u, 0x8au,
		0x84u, 0x87u, 0x7fu, 0x1eu, 0xb8u, 0xe5u, 0xd9u, 0x74u,
		0xd8u, 0x73u, 0xe0u, 0x65u, 0x22u, 0x49u, 0x01u, 0x55u,
		0x5fu, 0xb8u, 0x82u, 0x15u, 0x90u, 0xa3u, 0x3bu, 0xacu,
		0xc6u, 0x1eu, 0x39u, 0x70u, 0x1cu, 0xf9u, 0xb4u, 0x6bu,
		0xd2u, 0x5bu, 0xf5u, 0xf0u, 0x59u, 0x5bu, 0xbeu, 0x24u,
		0x65u, 0x51u, 0x41u, 0x43u, 0x8eu, 0x7au, 0x10u, 0x0bu
	};
	const uint8 arrEmpty[1] = { 0u };
	uint8 arrPub[32];
	uint8 arrSig[64];
	uint8 arrKeyBlob[128];
	uint8 arrSigBlob[128];
	xsshwriter keyWr;
	xsshwriter sigWr;
	xsshpublickey key;
	xsshsignature sig;

	xrtEd25519PublicKey(arrPub, arrSeed);
	if ( xssh_auth_expect_mem("auth_ed25519_rfc8032", arrPub, arrExpectedPub, sizeof(arrExpectedPub)) ) {
		return 1;
	}
	if ( !xrtEd25519Sign(arrSig, arrEmpty, 0u, arrSeed) ||
		xssh_auth_expect_mem("auth_ed25519_rfc8032", arrSig, arrExpectedSig, sizeof(arrExpectedSig)) ) {
		xsshSecureZero(arrSig, sizeof(arrSig));
		return xssh_auth_test_fail("auth_ed25519_rfc8032", "signature vector mismatch");
	}
	xsshWriterInit(&keyWr, arrKeyBlob, sizeof(arrKeyBlob));
	xsshWriterInit(&sigWr, arrSigBlob, sizeof(arrSigBlob));
	if ( xsshPublicKeyWriteEd25519(&keyWr, arrPub) != XSSH_OK ||
		xsshSignatureWrite(&sigWr, "ssh-ed25519", arrSig, sizeof(arrSig)) != XSSH_OK ||
		xsshPublicKeyReadEd25519(arrKeyBlob, keyWr.iLen, &key) != XSSH_OK ||
		xsshSignatureRead(arrSigBlob, sigWr.iLen, &sig) != XSSH_OK ||
		xsshHostKeyVerifyEd25519(&key, &sig, arrEmpty, 0u) != XSSH_OK ) {
		xsshSecureZero(arrSig, sizeof(arrSig));
		return xssh_auth_test_fail("auth_ed25519_rfc8032", "signature verify failed");
	}
	arrSigBlob[sigWr.iLen - 1u] ^= 0x01u;
	if ( xsshSignatureRead(arrSigBlob, sigWr.iLen, &sig) != XSSH_OK ||
		xsshHostKeyVerifyEd25519(&key, &sig, arrEmpty, 0u) != XSSH_ERR_BAD_PACKET ) {
		xsshSecureZero(arrSig, sizeof(arrSig));
		return xssh_auth_test_fail("auth_ed25519_rfc8032", "tampered signature accepted");
	}
	xsshSecureZero(arrSig, sizeof(arrSig));
	return 0;
}

static int xssh_auth_make_openssh_ed25519_private_key(char* sOut, size_t iOutCap,
	const uint8 arrSeed[32], uint8* pKeyBlob, size_t* pKeyBlobLen)
{
	static const uint8 arrMagic[] = {
		'o','p','e','n','s','s','h','-','k','e','y','-','v','1','\0'
	};
	uint8 arrPub[32];
	uint8 arrPrivateRaw[64];
	uint8 arrPrivateList[512];
	uint8 arrBin[1024];
	xsshwriter keyWr;
	xsshwriter privWr;
	xsshwriter wr;
	str sB64;
	size_t iPad;
	int iWritten;

	if ( sOut == NULL || iOutCap == 0u || arrSeed == NULL || pKeyBlob == NULL || pKeyBlobLen == NULL ) {
		return XSSH_ERR_INVALID;
	}
	xrtEd25519PublicKey(arrPub, arrSeed);
	xsshWriterInit(&keyWr, pKeyBlob, 128u);
	if ( xsshPublicKeyWriteEd25519(&keyWr, arrPub) != XSSH_OK ) {
		return XSSH_ERR_NO_SPACE;
	}
	*pKeyBlobLen = keyWr.iLen;
	memcpy(arrPrivateRaw, arrSeed, 32u);
	memcpy(arrPrivateRaw + 32u, arrPub, 32u);

	xsshWriterInit(&privWr, arrPrivateList, sizeof(arrPrivateList));
	if ( xsshWireWriteU32(&privWr, 0x12345678u) != XSSH_OK ||
		xsshWireWriteU32(&privWr, 0x12345678u) != XSSH_OK ||
		xsshWireWriteString(&privWr, "ssh-ed25519", strlen("ssh-ed25519")) != XSSH_OK ||
		xsshWireWriteString(&privWr, arrPub, 32u) != XSSH_OK ||
		xsshWireWriteString(&privWr, arrPrivateRaw, sizeof(arrPrivateRaw)) != XSSH_OK ||
		xsshWireWriteString(&privWr, "xssh-test", strlen("xssh-test")) != XSSH_OK ) {
		xsshSecureZero(arrPrivateRaw, sizeof(arrPrivateRaw));
		return XSSH_ERR_NO_SPACE;
	}
	for ( iPad = 1u; (privWr.iLen % 8u) != 0u; ++iPad ) {
		if ( xsshWireWriteU8(&privWr, (uint8)iPad) != XSSH_OK ) {
			xsshSecureZero(arrPrivateRaw, sizeof(arrPrivateRaw));
			return XSSH_ERR_NO_SPACE;
		}
	}

	xsshWriterInit(&wr, arrBin, sizeof(arrBin));
	if ( xsshWireReserve(&wr, sizeof(arrMagic)) != XSSH_OK ) {
		xsshSecureZero(arrPrivateRaw, sizeof(arrPrivateRaw));
		return XSSH_ERR_NO_SPACE;
	}
	memcpy(wr.pData + wr.iLen, arrMagic, sizeof(arrMagic));
	wr.iLen += sizeof(arrMagic);
	if ( xsshWireWriteString(&wr, "none", strlen("none")) != XSSH_OK ||
		xsshWireWriteString(&wr, "none", strlen("none")) != XSSH_OK ||
		xsshWireWriteString(&wr, "", 0u) != XSSH_OK ||
		xsshWireWriteU32(&wr, 1u) != XSSH_OK ||
		xsshWireWriteString(&wr, pKeyBlob, *pKeyBlobLen) != XSSH_OK ||
		xsshWireWriteString(&wr, arrPrivateList, privWr.iLen) != XSSH_OK ) {
		xsshSecureZero(arrPrivateRaw, sizeof(arrPrivateRaw));
		xsshSecureZero(arrPrivateList, sizeof(arrPrivateList));
		return XSSH_ERR_NO_SPACE;
	}
	sB64 = xrtBase64Encode(arrBin, wr.iLen, NULL);
	if ( sB64 == NULL ) {
		xsshSecureZero(arrPrivateRaw, sizeof(arrPrivateRaw));
		xsshSecureZero(arrPrivateList, sizeof(arrPrivateList));
		return XSSH_ERR_NO_SPACE;
	}
	iWritten = snprintf(sOut, iOutCap,
		"-----BEGIN OPENSSH PRIVATE KEY-----\n%s\n-----END OPENSSH PRIVATE KEY-----\n",
		(const char*)sB64);
	xrtFree(sB64);
	xsshSecureZero(arrPrivateRaw, sizeof(arrPrivateRaw));
	xsshSecureZero(arrPrivateList, sizeof(arrPrivateList));
	xsshSecureZero(arrBin, sizeof(arrBin));
	return (iWritten > 0 && (size_t)iWritten < iOutCap) ? XSSH_OK : XSSH_ERR_NO_SPACE;
}

static int xssh_auth_test_openssh_private_key_ed25519(void)
{
	uint8 arrSeed[32];
	uint8 arrKeyBlob[128];
	uint8 arrSignData[] = { 'x', 's', 's', 'h' };
	uint8 arrSig[64];
	char sPem[2048];
	size_t iKeyBlobLen = 0u;
	size_t i;
	xsshprivateidentity id;
	xsshpublickey key;
	xsshsignature sig;
	uint8 arrSigBlob[128];
	xsshwriter sigWr;

	for ( i = 0u; i < sizeof(arrSeed); ++i ) {
		arrSeed[i] = (uint8)(0x70u + i);
	}
	if ( xssh_auth_make_openssh_ed25519_private_key(sPem, sizeof(sPem), arrSeed, arrKeyBlob, &iKeyBlobLen) != XSSH_OK ||
		xsshOpenSshPrivateKeyV1Read(sPem, strlen(sPem), &id) != XSSH_OK ||
		!id.bHasEd25519 ||
		strcmp(id.sAlgorithm, "ssh-ed25519") != 0 ||
		id.iPublicKeyBlobLen != iKeyBlobLen ||
		memcmp(id.arrPublicKeyBlob, arrKeyBlob, iKeyBlobLen) != 0 ) {
		return xssh_auth_test_fail("auth_openssh_private_key", "parse mismatch");
	}
	if ( !xrtEd25519Sign(arrSig, arrSignData, sizeof(arrSignData), id.arrEd25519Seed) ) {
		xsshSecureZero(&id, sizeof(id));
		return xssh_auth_test_fail("auth_openssh_private_key", "sign failed");
	}
	xsshWriterInit(&sigWr, arrSigBlob, sizeof(arrSigBlob));
	if ( xsshSignatureWrite(&sigWr, "ssh-ed25519", arrSig, sizeof(arrSig)) != XSSH_OK ||
		xsshPublicKeyReadEd25519(id.arrPublicKeyBlob, id.iPublicKeyBlobLen, &key) != XSSH_OK ||
		xsshSignatureRead(arrSigBlob, sigWr.iLen, &sig) != XSSH_OK ||
		xsshHostKeyVerifyEd25519(&key, &sig, arrSignData, sizeof(arrSignData)) != XSSH_OK ) {
		xsshSecureZero(&id, sizeof(id));
		return xssh_auth_test_fail("auth_openssh_private_key", "verify failed");
	}
	xsshSecureZero(&id, sizeof(id));
	return 0;
}

static int xssh_auth_test_openssh_private_key_file(void)
{
	uint8 arrSeed[32];
	uint8 arrKeyBlob[128];
	char sPem[2048];
	size_t iKeyBlobLen = 0u;
	size_t i;
	FILE* f;
	xsshauth auth;
	xsshprivateidentity id;
	const char* sPath = "xssh_ed25519_key.tmp";

	for ( i = 0u; i < sizeof(arrSeed); ++i ) {
		arrSeed[i] = (uint8)(0x20u + i);
	}
	if ( xssh_auth_make_openssh_ed25519_private_key(sPem, sizeof(sPem), arrSeed, arrKeyBlob, &iKeyBlobLen) != XSSH_OK ) {
		return xssh_auth_test_fail("auth_openssh_private_key_file", "pem build failed");
	}
	f = fopen(sPath, "wb");
	if ( f == NULL ) {
		return xssh_auth_test_fail("auth_openssh_private_key_file", "file open failed");
	}
	if ( fwrite(sPem, 1u, strlen(sPem), f) != strlen(sPem) ) {
		fclose(f);
		remove(sPath);
		return xssh_auth_test_fail("auth_openssh_private_key_file", "file write failed");
	}
	fclose(f);
	xsshAuthInit(&auth);
	auth.sPrivateKeyPath = sPath;
	if ( xsshAuthLoadPrivateIdentity(&auth, &id) != XSSH_OK ||
		id.iPublicKeyBlobLen != iKeyBlobLen ||
		memcmp(id.arrPublicKeyBlob, arrKeyBlob, iKeyBlobLen) != 0 ) {
		remove(sPath);
		return xssh_auth_test_fail("auth_openssh_private_key_file", "file parse mismatch");
	}
	xsshSecureZero(&id, sizeof(id));
	remove(sPath);
	return 0;
}

static int xssh_auth_test_failure_success_banner(void)
{
	uint8 arrBuf[128];
	xsshwriter wr;
	xsshauthfailuremsg fail;
	xsshauthbannermsg banner;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshAuthWriteFailure(&wr, "publickey,password", TRUE) != XSSH_OK ||
		xsshAuthReadFailure(arrBuf, wr.iLen, &fail) != XSSH_OK ||
		!fail.bPartialSuccess ||
		!xssh_auth_slice_eq(fail.tMethods, "publickey,password") ) {
		return xssh_auth_test_fail("auth_failure", "failure mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshAuthWriteSuccess(&wr) != XSSH_OK ||
		xsshAuthReadSuccess(arrBuf, wr.iLen) != XSSH_OK ) {
		return xssh_auth_test_fail("auth_success", "success mismatch");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshAuthWriteBanner(&wr, "hello", "en") != XSSH_OK ||
		xsshAuthReadBanner(arrBuf, wr.iLen, &banner) != XSSH_OK ||
		!xssh_auth_slice_eq(banner.tMessage, "hello") ||
		!xssh_auth_slice_eq(banner.tLanguageTag, "en") ) {
		return xssh_auth_test_fail("auth_banner", "banner mismatch");
	}
	return 0;
}

static int xssh_auth_test_publickey_ok(void)
{
	uint8 arrKeyBlob[] = { 1u, 2u, 3u, 4u };
	uint8 arrBuf[128];
	xsshwriter wr;
	xsshauthpkokmsg ok;
	xsshslice keyBlob;

	keyBlob.pData = arrKeyBlob;
	keyBlob.iLen = sizeof(arrKeyBlob);

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshAuthWritePublicKeyOk(&wr, "ssh-ed25519", keyBlob) != XSSH_OK ||
		xsshAuthReadPublicKeyOk(arrBuf, wr.iLen, &ok) != XSSH_OK ||
		!xssh_auth_slice_eq(ok.tPublicKeyAlgorithm, "ssh-ed25519") ||
		ok.tPublicKeyBlob.iLen != sizeof(arrKeyBlob) ) {
		return xssh_auth_test_fail("auth_pk_ok", "roundtrip mismatch");
	}
	return 0;
}

static int xssh_auth_test_errors(void)
{
	uint8 arrBuf[128];
	xsshwriter wr;
	xsshauthrequestmsg req;
	xsshauthfailuremsg fail;
	const char* psTooMany[XSSH_AUTH_KBDINT_PROMPT_MAX + 1u];
	bool arrEcho[XSSH_AUTH_KBDINT_PROMPT_MAX + 1u];
	size_t i;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshAuthWriteFailure(&wr, "password,,publickey", FALSE) != XSSH_ERR_INVALID || wr.iLen != 0u ) {
		return xssh_auth_test_fail("auth_errors", "invalid method list accepted");
	}
	if ( xsshAuthWritePasswordRequest(&wr, "bob", NULL) != XSSH_ERR_INVALID ) {
		return xssh_auth_test_fail("auth_errors", "null password accepted");
	}
	for ( i = 0u; i < XSSH_AUTH_KBDINT_PROMPT_MAX + 1u; ++i ) {
		psTooMany[i] = "Prompt: ";
		arrEcho[i] = FALSE;
	}
	if ( xsshAuthWriteKeyboardInteractiveInfoRequest(&wr, "login", "",
			psTooMany, arrEcho, XSSH_AUTH_KBDINT_PROMPT_MAX + 1u) != XSSH_ERR_INVALID ) {
		return xssh_auth_test_fail("auth_errors", "too many kbdint prompts accepted");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshAuthWriteNoneRequest(&wr, "alice") != XSSH_OK ) {
		return xssh_auth_test_fail("auth_errors", "setup failed");
	}
	arrBuf[0] = XSSH_MSG_USERAUTH_SUCCESS;
	if ( xsshAuthReadRequest(arrBuf, wr.iLen, &req) != XSSH_ERR_BAD_PACKET ) {
		return xssh_auth_test_fail("auth_errors", "bad request id accepted");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshAuthWriteFailure(&wr, "password", FALSE) != XSSH_OK ) {
		return xssh_auth_test_fail("auth_errors", "failure setup failed");
	}
	arrBuf[wr.iLen++] = 0u;
	if ( xsshAuthReadFailure(arrBuf, wr.iLen, &fail) != XSSH_ERR_BAD_PACKET ) {
		return xssh_auth_test_fail("auth_errors", "trailing failure accepted");
	}
	return 0;
}

int main(void)
{
	if ( xssh_auth_test_none_request() ) return 1;
	if ( xssh_auth_test_password_request() ) return 1;
	if ( xssh_auth_test_keyboard_interactive_request() ) return 1;
	if ( xssh_auth_test_publickey_request() ) return 1;
	if ( xssh_auth_test_publickey_sign_data_layout() ) return 1;
	if ( xssh_auth_test_publickey_sign_data_ed25519() ) return 1;
	if ( xssh_auth_test_ed25519_rfc8032_vector() ) return 1;
	if ( xssh_auth_test_openssh_private_key_ed25519() ) return 1;
	if ( xssh_auth_test_openssh_private_key_file() ) return 1;
	if ( xssh_auth_test_failure_success_banner() ) return 1;
	if ( xssh_auth_test_publickey_ok() ) return 1;
	if ( xssh_auth_test_errors() ) return 1;

	printf("xssh_auth_test: PASS\n");
	return 0;
}
