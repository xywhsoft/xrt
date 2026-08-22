#include "../test.h"



/* 构建测试主机公钥和签名 blob。 */
static void testSshHostBasedFixtures(
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
		arrPublicKey[i] = (unsigned char)(0x20u + i);
	}
	for ( i = 0u; i < sizeof(arrSignature); ++i ) {
		arrSignature[i] = (unsigned char)(0x80u + i);
	}
	testRequire(xrtSshWriterInit(&Writer, pKey, iKeyCapacity) &&
		(xrtSshEd25519PublicKeyWrite(
			&Writer,
			(xbytesview){ arrPublicKey, sizeof(arrPublicKey) }
		) == XSSH_OK), "ssh hostbased key fixture failed");
	*pKeySize = Writer.Size;
	testRequire(xrtSshWriterInit(&Writer, pSignature, iSignatureCapacity) &&
		(xrtSshEd25519SignatureWrite(
			&Writer,
			(xbytesview){ arrSignature, sizeof(arrSignature) }
		) == XSSH_OK), "ssh hostbased signature fixture failed");
	*pSignatureSize = Writer.Size;
}



/* 验证 DNS 主机名的标签和总长度边界。 */
static void testSshHostBasedNames(void)
{
	char arrLongLabel[64];

	memset(arrLongLabel, 'a', sizeof(arrLongLabel));
	testRequire(xrtSshAuthHostNameValid(
		XRT_STR_LITERAL("client.example.com")
	) && xrtSshAuthHostNameValid(
		XRT_STR_LITERAL("client.example.com.")
	) && xrtSshAuthHostNameValid(
		XRT_STR_LITERAL("localhost")
	), "ssh hostbased rejected valid host name");
	testRequire(!xrtSshAuthHostNameValid(XRT_STR_LITERAL("-bad.example")) &&
		!xrtSshAuthHostNameValid(XRT_STR_LITERAL("bad-.example")) &&
		!xrtSshAuthHostNameValid(XRT_STR_LITERAL("bad..example")) &&
		!xrtSshAuthHostNameValid(XRT_STR_LITERAL("bad_name.example")) &&
		!xrtSshAuthHostNameValid(
			(xstrview){ arrLongLabel, sizeof(arrLongLabel) }
		), "ssh hostbased accepted invalid host name");
}



/* 验证完整请求和严格借用解析。 */
static void testSshHostBasedRequest(void)
{
	unsigned char arrKey[96];
	unsigned char arrSignature[96];
	unsigned char arrPayload[384];
	size_t iKeySize;
	size_t iSignatureSize;
	xsshwriter Writer;
	xsshauthhostbased HostBased;

	testSshHostBasedFixtures(
		arrKey,
		sizeof(arrKey),
		&iKeySize,
		arrSignature,
		sizeof(arrSignature),
		&iSignatureSize
	);
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthHostBasedWrite(
			&Writer,
			XRT_STR_LITERAL("server-user"),
			XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
			(xbytesview){ arrKey, iKeySize },
			XRT_STR_LITERAL("client.example.com."),
			XRT_STR_LITERAL("client-user"),
			(xbytesview){ arrSignature, iSignatureSize }
		) == XSSH_OK) && (xrtSshAuthHostBasedRead(
			(xbytesview){ arrPayload, Writer.Size },
			&HostBased
		) == XSSH_OK) && testSshTextEqual(
		HostBased.User,
		XRT_STR_LITERAL("server-user")
	) && testSshTextEqual(
		HostBased.HostName,
		XRT_STR_LITERAL("client.example.com.")
	) && testSshTextEqual(
		HostBased.ClientUser,
		XRT_STR_LITERAL("client-user")
	) && testSshBytesEqual(
		HostBased.PublicKey,
		(xbytesview){ arrKey, iKeySize }
	) && testSshBytesEqual(
		HostBased.Signature,
		(xbytesview){ arrSignature, iSignatureSize }
	), "ssh hostbased request mismatch");
}



/* 验证签名原文与完整请求前缀逐字节一致。 */
static void testSshHostBasedSignData(void)
{
	static const unsigned char arrSession[] = { 1u, 2u, 3u, 4u };
	unsigned char arrKey[96];
	unsigned char arrSignature[96];
	unsigned char arrRequest[384];
	unsigned char arrSignData[384];
	size_t iKeySize;
	size_t iSignatureSize;
	size_t iRequestFields;
	xsshwriter Request;
	xsshwriter SignData;

	testSshHostBasedFixtures(
		arrKey,
		sizeof(arrKey),
		&iKeySize,
		arrSignature,
		sizeof(arrSignature),
		&iSignatureSize
	);
	testRequire(xrtSshWriterInit(&Request, arrRequest, sizeof(arrRequest)) &&
		(xrtSshAuthHostBasedWrite(
			&Request,
			XRT_STR_LITERAL("alice"),
			XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
			(xbytesview){ arrKey, iKeySize },
			XRT_STR_LITERAL("client.example"),
			XRT_STR_LITERAL("local-alice"),
			(xbytesview){ arrSignature, iSignatureSize }
		) == XSSH_OK) && xrtSshWriterInit(
			&SignData,
			arrSignData,
			sizeof(arrSignData)
		) && (xrtSshAuthHostBasedSignDataWrite(
			&SignData,
			(xbytesview){ arrSession, sizeof(arrSession) },
			XRT_STR_LITERAL("alice"),
			XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
			(xbytesview){ arrKey, iKeySize },
			XRT_STR_LITERAL("client.example"),
			XRT_STR_LITERAL("local-alice")
		) == XSSH_OK), "ssh hostbased sign-data setup failed");
	iRequestFields = Request.Size - 4u - iSignatureSize;
	testRequire(SignData.Size == (4u + sizeof(arrSession) + iRequestFields) &&
		(arrSignData[3] == sizeof(arrSession)) &&
		(memcmp(
			arrSignData + 4u,
			arrSession,
			sizeof(arrSession)
		) == 0) && (memcmp(
			arrSignData + 4u + sizeof(arrSession),
			arrRequest,
			iRequestFields
		) == 0), "ssh hostbased sign-data layout mismatch");
}



/* 验证截断、尾随、算法、空间和重叠边界。 */
static void testSshHostBasedBoundaries(void)
{
	static const unsigned char arrWrongSignature[] = {
		0u, 0u, 0u, 7u, 's', 's', 'h', '-', 'r', 's', 'a',
		0u, 0u, 0u, 1u, 0u
	};
	unsigned char arrKey[96];
	unsigned char arrSignature[96];
	unsigned char arrPayload[384];
	size_t iKeySize;
	size_t iSignatureSize;
	size_t iRequestSize;
	xsshwriter Writer;
	xsshauthhostbased HostBased;

	testSshHostBasedFixtures(
		arrKey,
		sizeof(arrKey),
		&iKeySize,
		arrSignature,
		sizeof(arrSignature),
		&iSignatureSize
	);
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthHostBasedWrite(
			&Writer,
			XRT_STR_LITERAL("alice"),
			XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
			(xbytesview){ arrKey, iKeySize },
			XRT_STR_LITERAL("client.example"),
			XRT_STR_LITERAL("local-alice"),
			(xbytesview){ arrSignature, iSignatureSize }
		) == XSSH_OK), "ssh hostbased boundary setup failed");
	iRequestSize = Writer.Size;
	testRequire(xrtSshAuthHostBasedRead(
		(xbytesview){ arrPayload, iRequestSize - 1u },
		&HostBased
	) == XSSH_NEED_MORE, "ssh hostbased truncated signature was not incremental");
	arrPayload[iRequestSize] = 0u;
	testRequire(xrtSshAuthHostBasedRead(
		(xbytesview){ arrPayload, iRequestSize + 1u },
		&HostBased
	) == XSSH_ERROR_PROTOCOL, "ssh hostbased accepted trailing data");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthHostBasedWrite(
			&Writer,
			XRT_STR_LITERAL("alice"),
			XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
			(xbytesview){ arrKey, iKeySize },
			XRT_STR_LITERAL("client.example"),
			XRT_STR_LITERAL("local-alice"),
			(xbytesview){
				arrWrongSignature,
				sizeof(arrWrongSignature)
			}
		) == XSSH_ERROR_PROTOCOL) && (Writer.Size == 0u),
		"ssh hostbased accepted mismatched signature algorithm");

	memcpy(arrPayload + 16u, arrSignature, iSignatureSize);
	testRequire((xrtSshAuthHostBasedWrite(
		&Writer,
		XRT_STR_LITERAL("alice"),
		XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
		(xbytesview){ arrKey, iKeySize },
		XRT_STR_LITERAL("bad_name"),
		XRT_STR_LITERAL("local-alice"),
		(xbytesview){ arrSignature, iSignatureSize }
	) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh hostbased accepted invalid host name");

	testRequire((xrtSshAuthHostBasedWrite(
		&Writer,
		XRT_STR_LITERAL("alice"),
		XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
		(xbytesview){ arrKey, iKeySize },
		XRT_STR_LITERAL("client.example"),
		XRT_STR_LITERAL("local-alice"),
		(xbytesview){ arrPayload + 16u, iSignatureSize }
	) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh hostbased accepted overlapping signature");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, 8u) &&
		(xrtSshAuthHostBasedWrite(
			&Writer,
			XRT_STR_LITERAL("alice"),
			XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519),
			(xbytesview){ arrKey, iKeySize },
			XRT_STR_LITERAL("client.example"),
			XRT_STR_LITERAL("local-alice"),
			(xbytesview){ arrSignature, iSignatureSize }
		) == XSSH_ERROR_SPACE) && (Writer.Size == 0u),
		"ssh hostbased short write changed state");
}



/* 运行 hostbased 消息、签名原文与边界测试。 */
int main(void)
{
	testSshHostBasedNames();
	testSshHostBasedRequest();
	testSshHostBasedSignData();
	testSshHostBasedBoundaries();
	return 0;
}
