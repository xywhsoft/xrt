#ifndef XRT_TEST_TLS_ANCHOR_VERIFY_H
#define XRT_TEST_TLS_ANCHOR_VERIFY_H

#include "../xrt.h"
#include <stdio.h>
#include <string.h>

static uint8* __Test_TLSAnchorDecodeBase64(const char* sBase64, size_t* pLen)
{
	size_t iBase64Len;
	size_t iDerLen;
	uint8* pDer;

	if ( !sBase64 || !pLen ) return NULL;
	iBase64Len = strlen(sBase64);
	if ( iBase64Len == 0u ) return NULL;

	iDerLen = (iBase64Len / 4u) * 3u;
	if ( iBase64Len > 0u && sBase64[iBase64Len - 1u] == '=' ) iDerLen--;
	if ( iBase64Len > 1u && sBase64[iBase64Len - 2u] == '=' ) iDerLen--;

	pDer = (uint8*)xrtBase64Decode((str)sBase64, iBase64Len, NULL);
	if ( !pDer || pDer == (uint8*)xCore.sNull ) return NULL;
	*pLen = iDerLen;
	return pDer;
}

static int Test_TLSAnchorVerify(void)
{
	static const char sLeafB64[] =
		"MIIDZDCCAkygAwIBAgIQUpVe3zOFEYZMXAFiovMLijANBgkqhkiG9w0BAQsFADAnMSUwIwYDVQQDDBxYUlQgU0hBMjU2IFRlc3QgSW50ZXJtZWRpYXRlMB4XDTI2MDMyMjEwMTU0M1oXDTMxMDMyMzEwMTU0M1owHTEbMBkGA1UEAwwSc2hhMS1yb290LnRlc3QueHJ0MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA697KFbxIHKU/LswQ82roCxm8W7EsR8rpLtqaYcr9Ka/WVjo+dXnn91qfXhWMBQLFRKGZRrQZFlOAiUnxzdSXbHiVJ4Rl9Ic4rXy+pX7JNlZ/dZgbrzUeDwcKOHW9UrBPQdB8/eUi9Fz5WskyS6HMulYW1zwC8AOBFiIEOH3QeOFtrWohCV6OHF6v4NT2DTFY84wDfOGfMrbGFgDKc5Dx9aFy1z0SF5jEfX791sgHT5mLtDiCoPpq4MTAcQ8965DYY7Zll34yb9Sjr2zHst6geEG2E3nI6wxve1n31fXCGqAqgbAOAyCPJ08NfCjGRA9MECcQDW9AooAYyKisGi8vCQIDAQABo4GVMIGSMA4GA1UdDwEB/wQEAwIFoDAdBgNVHREEFjAUghJzaGExLXJvb3QudGVzdC54cnQwDAYDVR0TAQH/BAIwADATBgNVHSUEDDAKBggrBgEFBQcDATAfBgNVHSMEGDAWgBQXJ8ePjWx+8ummUXkgLec2WIHLkzAdBgNVHQ4EFgQUOvPQ6THheDH+FndoSB2KnAIwx+kwDQYJKoZIhvcNAQELBQADggEBAGuAvFi41gf5/Nrx8/LD2R10dZpMB7ZMlBLigI3nQLZZDOPerF5UrtUS+Eiil/wZ0GIKNX1gpPTTjZc/yVn6JWtxzXQzKV7VKwvRkgDZUK7wwiEC2cO5jTmRoSwHos9I6wqgcg/jYvaIRgnOcu4TIgCr0OqW/NiZ+opvTxC79suf6vFXGhG76PUzQgz/nYbQT0LgeTGGCeDD1BxLquA15Vcz1lNCjEzVYKw8haUPSBFpNeWg1Wh287JQiavoyvdsDAz4EENh/k/wdB9luX5VzkX8XapmeYyOSo4+QKOckt4W23F0E5oJjNSpaB8W7A6Kic3Ux0yjTUr+iq2HBGStEmc=";
	static const char sIntermediateB64[] =
		"MIIDNDCCAhygAwIBAgIQb4lqcBOzbZpKTPJz76AfvzANBgkqhkiG9w0BAQsFADAdMRswGQYDVQQDDBJYUlQgU0hBMSBUZXN0IFJvb3QwHhcNMjYwMzIyMTAxNTQzWhcNMzEwMzIzMTAxNTQzWjAnMSUwIwYDVQQDDBxYUlQgU0hBMjU2IFRlc3QgSW50ZXJtZWRpYXRlMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAxZbg7XjGgRWMX2tewxbdJYpw7YBXJS7X1ieGRn1E3iQQjr+HfKr0+fSKDOtWNKin/ujbga247yaV6RkcNl6f18D08OfACGw0HAlT08c+BMZTGdfXuAedFVg0XLzkQj5N1hdpddO4mnDI+yjmyscxRXYhvMXZYcAUyxX043rqYujxn9KICZF2sCEi7tPrfxmF4Fhj+xBJ6mLY+dQ9/rPIX35d3kMENw2X8RrCYD0lNzLRIvw6QBTN3yg9ljhC9WQdBCu5bW73ocloIUnvYNnciosQ6iqy+CmKAEwnryhI9+l+OaJXr2aISRy2Y3vFiI7bIdEIkJOKUiUH7FqLKlcTHQIDAQABo2YwZDAOBgNVHQ8BAf8EBAMCAYYwEgYDVR0TAQH/BAgwBgEB/wIBADAfBgNVHSMEGDAWgBS7XWteXpf7m2YB3SNxfdpZlqSG/DAdBgNVHQ4EFgQUFyfHj41sfvLpplF5IC3nNliBy5MwDQYJKoZIhvcNAQELBQADggEBABBsg2d+1CdlwgV9Fq2dWsPfJXQBTmarOhMxY7yXTwvZ6WJspSEayYo+j68yo/YCT71Vuj2zNOyCBK67FEio/Cpg3GmVHaW+/erVZcLMdOzd07C+Ui7TShSZxuiTGr0np9edwGcx8KofwkXo3fCUEyvxIk/zHq8ZvkQ6PbqiQEZOEfbVMGyXAOO9WjKUuE7HkfW84pDiMRUADMMj6kDUGzuZQyrouhIvBUsPV4/Vo//HijiAUIvx3PFkQ4x/xiOX5cQIFkqlZZY0tifA7vGTYm5quPc81iaNmFmzFt7yspghdF/sNlAy+FO6j9+d+05xJIllJ24j8L6t8n2A7RAzxTE=";
	static const char sRootB64[] =
		"MIIDCTCCAfGgAwIBAgIQGrgMKTt7J6NCb9zhbeYyBDANBgkqhkiG9w0BAQUFADAdMRswGQYDVQQDDBJYUlQgU0hBMSBUZXN0IFJvb3QwHhcNMjYwMzIyMTAxNTQzWhcNMzEwMzIzMTAxNTQzWjAdMRswGQYDVQQDDBJYUlQgU0hBMSBUZXN0IFJvb3QwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCr1KFh6FPTP0Wcf4d4KlLIZNHialEmGi3GNlSo92ISuneigVf6p0VSKm5yZT+rufd9U4gGCpYjOZmTsTxg3V1tmVU/ufL6H653UcmgghvWLbzwRNQcwsCn9MZkxrTh1tGs/k2Q9KMtpW4u7xFMwzvj4ERrgQi5NkIdVOu/v6Y3NiDoonAhPqB/kikIG9dTyq/xuLC2oxZimNsoH449RsDL1vw2DPJtgdaaK+krqgZVUaWGqK/CbYMCSyBS+seJeaeiQZQ5wl1PLdfa38UBHZEQfZFnip4kCs9jD7V8HcetFRLP821VwirvHFmztEeYE8482s89ZrAYpe8Wp5aeI2yxAgMBAAGjRTBDMA4GA1UdDwEB/wQEAwIBhjASBgNVHRMBAf8ECDAGAQH/AgEBMB0GA1UdDgQWBBS7XWteXpf7m2YB3SNxfdpZlqSG/DANBgkqhkiG9w0BAQUFAAOCAQEAZn/DtiyRrRdM8L1HtfXmh4WwMf6hEab/lbogO5k/fCEqt+j8+eIOgxBY4UQIizFaZV1ev45oJ0xOoxe3mapOEfAVvz6J/j8xVF5hULHFNNnxmIk10Sf2ExKaRqlVtWkgwfZsqC5HQCq97TSmtEYiaGZdVtG87mteKoYyTMriE5luk6UVHJWbppWLUI/u9oV5hrt4onOEOF0UsHDhhBalg/nBw0wSWuYLWIDtedNIlXahqZ8dkQoQTsZ3sJ1kr0lWGk+LJoGEL6AX14XZgSRcGU+eAh22w/jvIiH/MLi3ebNJlGIO0X+b3GhT5Z/2Afkcj0PKc4D1qNoy8MNPCSAvtg==";
	xtlsctx tCtx;
	struct __xrt_x509_cert tRootCert;
	uint8* pLeaf = NULL;
	uint8* pIntermediate = NULL;
	uint8* pRoot = NULL;
	size_t iLeafLen = 0u;
	size_t iIntermediateLen = 0u;
	size_t iRootLen = 0u;
	uint8* arrChainWithRoot[3];
	size_t arrChainWithRootLen[3];
	uint8* arrChainNoRoot[2];
	size_t arrChainNoRootLen[2];
	bool bStrictRoot = false;
	bool bChainRoot = false;
	bool bWithRoot = false;
	bool bWithoutRoot = false;
	int iRet = 1;

	printf("\n\n------------------------------------\n");
	printf("\n TLS Trust Anchor Test:\n\n");

	xrtInit();
	memset(&tCtx, 0, sizeof(tCtx));
	strncpy(tCtx.sHostname, "sha1-root.test.xrt", sizeof(tCtx.sHostname) - 1u);

	pLeaf = __Test_TLSAnchorDecodeBase64(sLeafB64, &iLeafLen);
	pIntermediate = __Test_TLSAnchorDecodeBase64(sIntermediateB64, &iIntermediateLen);
	pRoot = __Test_TLSAnchorDecodeBase64(sRootB64, &iRootLen);
	if ( !pLeaf || !pIntermediate || !pRoot ) {
		printf("  TLS anchor fixture decode : FAIL\n");
		goto cleanup;
	}

	tCtx.pCaData = pRoot;
	tCtx.iCaDataLen = iRootLen;

	bStrictRoot = __xrt_x509_parse(pRoot, iRootLen, &tRootCert);
	bChainRoot = __xrt_x509_parse_for_chain(pRoot, iRootLen, &tRootCert);

	arrChainWithRoot[0] = pLeaf;
	arrChainWithRoot[1] = pIntermediate;
	arrChainWithRoot[2] = pRoot;
	arrChainWithRootLen[0] = iLeafLen;
	arrChainWithRootLen[1] = iIntermediateLen;
	arrChainWithRootLen[2] = iRootLen;

	arrChainNoRoot[0] = pLeaf;
	arrChainNoRoot[1] = pIntermediate;
	arrChainNoRootLen[0] = iLeafLen;
	arrChainNoRootLen[1] = iIntermediateLen;

	bWithRoot = __xrt_tls_verify_presented_chain(&tCtx, arrChainWithRoot, arrChainWithRootLen, 3u);
	bWithoutRoot = __xrt_tls_verify_presented_chain(&tCtx, arrChainNoRoot, arrChainNoRootLen, 2u);

	printf("  TLS anchor sha1 root strict parse rejected : %s\n", !bStrictRoot ? "PASS" : "FAIL");
	printf("  TLS anchor sha1 root chain parse accepted : %s\n", bChainRoot ? "PASS" : "FAIL");
	printf("  TLS anchor self-signed root presented : %s\n", bWithRoot ? "PASS" : "FAIL");
	printf("  TLS anchor root store termination : %s\n", bWithoutRoot ? "PASS" : "FAIL");

	if ( !bStrictRoot && bChainRoot && bWithRoot && bWithoutRoot ) {
		iRet = 0;
	}

cleanup:
	if ( pLeaf ) xrtFree(pLeaf);
	if ( pIntermediate ) xrtFree(pIntermediate);
	if ( pRoot ) xrtFree(pRoot);
	xrtUnit();
	return iRet;
}

#endif
