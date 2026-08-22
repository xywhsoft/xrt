#include "../test.h"



/* 构建测试公钥和签名 blob。 */
static void testSshPublicKeyFixtures(
	unsigned char* pKey,
	size_t iKeyCapacity,
	size_t* pKeySize,
	unsigned char* pSignature,
	size_t iSignatureCapacity,
	size_t* pSignatureSize
)
{
	unsigned char arrPublicKey[XSSH_ED25519_PUBLIC_SIZE];
	unsigned char arrSignature[XSSH_ED25519_SIGNATURE_SIZE];
	xsshwriter Writer;
	size_t i;

	for ( i = 0u; i < sizeof(arrPublicKey); ++i ) {
		arrPublicKey[i] = (unsigned char)i;
	}
	for ( i = 0u; i < sizeof(arrSignature); ++i ) {
		arrSignature[i] = (unsigned char)(0x80u + i);
	}
	testRequire(xrtSshWriterInit(&Writer, pKey, iKeyCapacity) &&
		(xrtSshEd25519PublicKeyWrite(
			&Writer,
			(xbytesview){ arrPublicKey, sizeof(arrPublicKey) }
		) == XSSH_OK), "ssh auth publickey fixture failed");
	*pKeySize = Writer.Size;
	testRequire(xrtSshWriterInit(&Writer, pSignature, iSignatureCapacity) &&
		(xrtSshEd25519SignatureWrite(
			&Writer,
			(xbytesview){ arrSignature, sizeof(arrSignature) }
		) == XSSH_OK), "ssh auth signature fixture failed");
	*pSignatureSize = Writer.Size;
}



/* 验证 probe、PK_OK 和带签名请求。 */
static void testSshPublicKeyRequests(void)
{
	unsigned char arrKey[96];
	unsigned char arrSignature[96];
	unsigned char arrPayload[384];
	size_t iKeySize;
	size_t iSignatureSize;
	xsshwriter Writer;
	xsshauthpublickey Request;
	xsshauthpublickeyok Accepted;

	testSshPublicKeyFixtures(
		arrKey,
		sizeof(arrKey),
		&iKeySize,
		arrSignature,
		sizeof(arrSignature),
		&iSignatureSize
	);
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthPublicKeyWrite(
			&Writer,
			XRT_STR_LITERAL("alice"),
			XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
			(xbytesview){ arrKey, iKeySize }
		) == XSSH_OK) && (xrtSshAuthPublicKeyRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Request
		) == XSSH_OK) && !Request.HasSignature && testSshTextEqual(
			Request.User,
			XRT_STR_LITERAL("alice")
		) && testSshBytesEqual(
			Request.PublicKey,
			(xbytesview){ arrKey, iKeySize }
		), "ssh publickey probe mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthPublicKeyOkWrite(
			&Writer,
			XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
			(xbytesview){ arrKey, iKeySize }
		) == XSSH_OK) && (xrtSshAuthPublicKeyOkRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Accepted
		) == XSSH_OK) && testSshTextEqual(
			Accepted.Algorithm,
			XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519)
		), "ssh publickey ok mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthPublicKeySignedWrite(
			&Writer,
			XRT_STR_LITERAL("alice"),
			XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
			(xbytesview){ arrKey, iKeySize },
			(xbytesview){ arrSignature, iSignatureSize }
		) == XSSH_OK) && (xrtSshAuthPublicKeyRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Request
		) == XSSH_OK) && Request.HasSignature && testSshBytesEqual(
			Request.Signature,
			(xbytesview){ arrSignature, iSignatureSize }
		), "ssh signed publickey request mismatch");
}



/* 验证 RFC 4252 签名原文的精确布局。 */
static void testSshPublicKeySignData(void)
{
	static const unsigned char arrExpected[] = {
		0x00u, 0x00u, 0x00u, 0x03u, 0xaau, 0xbbu, 0xccu, 0x32u,
		0x00u, 0x00u, 0x00u, 0x04u, 0x64u, 0x61u, 0x76u, 0x65u,
		0x00u, 0x00u, 0x00u, 0x0eu, 0x73u, 0x73u, 0x68u, 0x2du,
		0x63u, 0x6fu, 0x6eu, 0x6eu, 0x65u, 0x63u, 0x74u, 0x69u,
		0x6fu, 0x6eu, 0x00u, 0x00u, 0x00u, 0x09u, 0x70u, 0x75u,
		0x62u, 0x6cu, 0x69u, 0x63u, 0x6bu, 0x65u, 0x79u, 0x01u,
		0x00u, 0x00u, 0x00u, 0x0bu, 0x73u, 0x73u, 0x68u, 0x2du,
		0x65u, 0x64u, 0x32u, 0x35u, 0x35u, 0x31u, 0x39u, 0x00u,
		0x00u, 0x00u, 0x12u, 0x00u, 0x00u, 0x00u, 0x0bu, 0x73u,
		0x73u, 0x68u, 0x2du, 0x65u, 0x64u, 0x32u, 0x35u, 0x35u,
		0x31u, 0x39u, 0x01u, 0x02u, 0x03u
	};
	static const unsigned char arrSession[] = { 0xaau, 0xbbu, 0xccu };
	static const unsigned char arrKey[] = {
		0u, 0u, 0u, 11u, 's', 's', 'h', '-', 'e', 'd', '2', '5', '5', '1', '9',
		1u, 2u, 3u
	};
	unsigned char arrPayload[256];
	xsshwriter Writer;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthPublicKeySignDataWrite(
			&Writer,
			(xbytesview){ arrSession, sizeof(arrSession) },
			XRT_STR_LITERAL("dave"),
			XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
			(xbytesview){ arrKey, sizeof(arrKey) }
		) == XSSH_OK), "ssh publickey sign data write failed");
	testRequire(Writer.Size == sizeof(arrExpected),
		"ssh publickey sign data size mismatch");
	testRequire(memcmp(arrPayload, arrExpected, sizeof(arrExpected)) == 0,
		"ssh publickey sign data bytes mismatch");
}



/* 验证算法不匹配、重叠和空间边界。 */
static void testSshPublicKeyBoundaries(void)
{
	unsigned char arrKey[96];
	unsigned char arrSignature[96];
	unsigned char arrPayload[256];
	size_t iKeySize;
	size_t iSignatureSize;
	xsshwriter Writer;
	xsshwriter SignatureWriter;

	testSshPublicKeyFixtures(
		arrKey,
		sizeof(arrKey),
		&iKeySize,
		arrSignature,
		sizeof(arrSignature),
		&iSignatureSize
	);
	testRequire(xrtSshWriterInit(
		&SignatureWriter,
		arrSignature,
		sizeof(arrSignature)
	) && (xrtSshSignatureWrite(
		&SignatureWriter,
		XRT_STR_LITERAL("ssh-rsa"),
		XRT_BYTES_LITERAL("sig")
	) == XSSH_OK), "ssh mismatched signature fixture failed");
	iSignatureSize = SignatureWriter.Size;
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthPublicKeySignedWrite(
			&Writer,
			XRT_STR_LITERAL("alice"),
			XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
			(xbytesview){ arrKey, iKeySize },
			(xbytesview){ arrSignature, iSignatureSize }
		) == XSSH_ERROR_PROTOCOL) && (Writer.Size == 0u),
		"ssh publickey accepted mismatched signature algorithm");

	memcpy(arrPayload + 4u, arrKey, iKeySize);
	testRequire((xrtSshAuthPublicKeyWrite(
		&Writer,
		XRT_STR_LITERAL("alice"),
		XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
		(xbytesview){ arrPayload + 4u, iKeySize }
	) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh publickey accepted overlapping key");
	Writer.Capacity = 16u;
	testRequire((xrtSshAuthPublicKeyWrite(
		&Writer,
		XRT_STR_LITERAL("alice"),
		XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
		(xbytesview){ arrKey, iKeySize }
	) == XSSH_ERROR_SPACE) && (Writer.Size == 0u),
		"ssh publickey short write changed state");
}



/* 运行 publickey 方法消息与边界测试。 */
int main(void)
{
	testSshPublicKeyRequests();
	testSshPublicKeySignData();
	testSshPublicKeyBoundaries();
	return 0;
}
