#include "../test.h"



/* 构建确定性的 Ed25519 公钥文本夹具。 */
static void testSshKeyTextFixture(
	unsigned char* pBlob,
	size_t iBlobCapacity,
	size_t* pBlobSize,
	char* sBase64,
	size_t iBase64Capacity,
	size_t* pBase64Size
)
{
	unsigned char arrPublic[XSSH_ED25519_PUBLIC_SIZE];
	xsshwriter Writer;
	size_t i;

	for ( i = 0u; i < sizeof(arrPublic); ++i ) {
		arrPublic[i] = (unsigned char)(0x20u + i);
	}
	testRequire(xrtSshWriterInit(&Writer, pBlob, iBlobCapacity) &&
		(xrtSshEd25519PublicKeyWrite(
			&Writer,
			(xbytesview){ arrPublic, sizeof(arrPublic) }
		) == XSSH_OK) && xrtBase64Encode(
			pBlob,
			Writer.Size,
			sBase64,
			iBase64Capacity,
			pBase64Size,
			NULL
		), "ssh key text fixture failed");
	*pBlobSize = Writer.Size;
}



/* 验证普通行、quoted options、注释和 Ed25519 blob 解码。 */
static void testSshKeyTextRead(void)
{
	unsigned char arrExpected[96];
	unsigned char arrDecoded[96];
	char sBase64[160];
	char sLine[384];
	size_t iExpectedSize;
	size_t iBase64Size;
	xsshopensshkeyline KeyLine;
	xsshpublickey PublicKey;
	xbytesview Ed25519;
	bool bMatch = false;
	int iWritten;

	testSshKeyTextFixture(
		arrExpected,
		sizeof(arrExpected),
		&iExpectedSize,
		sBase64,
		sizeof(sBase64),
		&iBase64Size
	);
	iWritten = snprintf(
		sLine,
		sizeof(sLine),
		"  command=\"echo hello\",no-pty ssh-ed25519 %s generated key  \r\n",
		sBase64
	);
	testRequire((iWritten > 0) && ((size_t)iWritten < sizeof(sLine)) &&
		(xrtSshPublicKeyLineRead(
			(xstrview){ sLine, (size_t)iWritten },
			&KeyLine
		) == XSSH_OK) && testSshTextEqual(
			KeyLine.Options,
			XRT_STR_LITERAL("command=\"echo hello\",no-pty")
		) && testSshTextEqual(
			KeyLine.Algorithm,
			XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519)
		) && (KeyLine.Base64.Size == iBase64Size) &&
		testSshTextEqual(
			KeyLine.Comment,
			XRT_STR_LITERAL("generated key")
		) && (KeyLine.BlobSize == iExpectedSize),
		"ssh key text fields mismatch");
	testRequire((xrtSshPublicKeyLineDecode(
		&KeyLine,
		arrDecoded,
		sizeof(arrDecoded),
		&PublicKey
	) == XSSH_OK) && testSshTextEqual(
		PublicKey.Algorithm,
		XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519)
	) && (memcmp(arrDecoded, arrExpected, iExpectedSize) == 0) &&
		(xrtSshEd25519PublicKeyRead(
			(xbytesview){ arrDecoded, KeyLine.BlobSize },
			&Ed25519
		) == XSSH_OK) &&
		(Ed25519.Size == XSSH_ED25519_PUBLIC_SIZE) &&
		(xrtSshPublicKeyLineMatch(
			&KeyLine,
			(xbytesview){ arrExpected, iExpectedSize },
			&bMatch
		) == XSSH_OK) && bMatch,
		"ssh key text decode mismatch");
	arrExpected[iExpectedSize - 1u] ^= 1u;
	testRequire((xrtSshPublicKeyLineMatch(
		&KeyLine,
		(xbytesview){ arrExpected, iExpectedSize },
		&bMatch
	) == XSSH_OK) && !bMatch, "ssh key text mismatch was accepted");
}



/* 验证算法识别不依赖固定白名单，且无 options/comment 时返回空视图。 */
static void testSshKeyTextExtensibleAlgorithm(void)
{
	static const char sAlgorithm[] = "vendor-key@example.com";
	unsigned char arrBlob[96];
	unsigned char arrDecoded[96];
	char sBase64[160];
	char sLine[320];
	xsshwriter Writer;
	xsshopensshkeyline KeyLine;
	xsshpublickey PublicKey;
	size_t iBase64Size;
	int iWritten;

	testRequire(xrtSshWriterInit(&Writer, arrBlob, sizeof(arrBlob)) &&
		(xrtSshWriteString(
			&Writer,
			XRT_BYTES_LITERAL("vendor-key@example.com")
		) == XSSH_OK) && (xrtSshWriteBytes(
			&Writer,
			XRT_BYTES_LITERAL("vendor-parameters")
		) == XSSH_OK) && xrtBase64Encode(
			arrBlob,
			Writer.Size,
			sBase64,
			sizeof(sBase64),
			&iBase64Size,
			NULL
		), "ssh extension key fixture failed");
	iWritten = snprintf(
		sLine,
		sizeof(sLine),
		"restrict %s %s",
		sAlgorithm,
		sBase64
	);
	testRequire((iWritten > 0) && ((size_t)iWritten < sizeof(sLine)) &&
		(xrtSshPublicKeyLineRead(
			(xstrview){ sLine, (size_t)iWritten },
			&KeyLine
		) == XSSH_OK) && testSshTextEqual(
			KeyLine.Options,
			XRT_STR_LITERAL("restrict")
		) && testSshTextEqual(
			KeyLine.Algorithm,
			XRT_STR_LITERAL("vendor-key@example.com")
		) && (KeyLine.Comment.Size == 0u) &&
		(xrtSshPublicKeyLineDecode(
			&KeyLine,
			arrDecoded,
			sizeof(arrDecoded),
			&PublicKey
		) == XSSH_OK) && testSshTextEqual(
			PublicKey.Algorithm,
			XRT_STR_LITERAL("vendor-key@example.com")
		), "ssh extension key line mismatch");
}



/* 覆盖 Base64 尾组只有一或两个原始字节的零分配比较。 */
static void testSshKeyTextMatchTails(void)
{
	static const unsigned char arrParameters[] = { 0x11u, 0x22u };
	unsigned char arrBlob[16];
	char sBase64[32];
	char sLine[48];
	xsshwriter Writer;
	xsshopensshkeyline KeyLine;
	size_t iBase64Size;
	bool bMatch;
	int iWritten;

	testRequire(xrtSshWriterInit(&Writer, arrBlob, sizeof(arrBlob)) &&
		(xrtSshWriteString(&Writer, XRT_BYTES_LITERAL("a")) == XSSH_OK) &&
		xrtBase64Encode(
			arrBlob,
			Writer.Size,
			sBase64,
			sizeof(sBase64),
			&iBase64Size,
			NULL
		), "ssh two-byte tail fixture failed");
	iWritten = snprintf(sLine, sizeof(sLine), "a %s", sBase64);
	testRequire((iWritten > 0) && ((size_t)iWritten < sizeof(sLine)) &&
		(xrtSshPublicKeyLineRead(
			(xstrview){ sLine, (size_t)iWritten },
			&KeyLine
		) == XSSH_OK) && (xrtSshPublicKeyLineMatch(
			&KeyLine,
			(xbytesview){ arrBlob, Writer.Size },
			&bMatch
		) == XSSH_OK) && bMatch, "ssh two-byte Base64 tail mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrBlob, sizeof(arrBlob)) &&
		(xrtSshWriteString(&Writer, XRT_BYTES_LITERAL("a")) == XSSH_OK) &&
		(xrtSshWriteBytes(
			&Writer,
			(xbytesview){ arrParameters, sizeof(arrParameters) }
		) == XSSH_OK) && xrtBase64Encode(
			arrBlob,
			Writer.Size,
			sBase64,
			sizeof(sBase64),
			&iBase64Size,
			NULL
		), "ssh one-byte tail fixture failed");
	iWritten = snprintf(sLine, sizeof(sLine), "a %s", sBase64);
	testRequire((iWritten > 0) && ((size_t)iWritten < sizeof(sLine)) &&
		(xrtSshPublicKeyLineRead(
			(xstrview){ sLine, (size_t)iWritten },
			&KeyLine
		) == XSSH_OK) && (xrtSshPublicKeyLineMatch(
			&KeyLine,
			(xbytesview){ arrBlob, Writer.Size },
			&bMatch
		) == XSSH_OK) && bMatch, "ssh one-byte Base64 tail mismatch");
}



/* 验证格式、容量、算法一致性和内存重叠边界。 */
static void testSshKeyTextRejectsInvalid(void)
{
	unsigned char arrBlob[96];
	unsigned char arrDecoded[96];
	char sBase64[160];
	char sLine[320];
	size_t iBlobSize;
	size_t iBase64Size;
	xsshopensshkeyline KeyLine;
	xsshpublickey PublicKey;
	xsshpublickey Keep;
	int iWritten;

	testSshKeyTextFixture(
		arrBlob,
		sizeof(arrBlob),
		&iBlobSize,
		sBase64,
		sizeof(sBase64),
		&iBase64Size
	);
	(void)iBase64Size;
	testRequire((xrtSshPublicKeyLineRead(
		XRT_STR_LITERAL("# ignored"),
		&KeyLine
	) == XSSH_ERROR_PROTOCOL) && (xrtSshPublicKeyLineRead(
		XRT_STR_LITERAL("command=\"unterminated ssh-ed25519 AAAA"),
		&KeyLine
	) == XSSH_ERROR_PROTOCOL) && (xrtSshPublicKeyLineRead(
		XRT_STR_LITERAL("ssh-ed25519 not_base64"),
		&KeyLine
	) == XSSH_ERROR_UNSUPPORTED), "ssh invalid key line accepted");

	iWritten = snprintf(
		sLine,
		sizeof(sLine),
		"ssh-rsa %s mismatch",
		sBase64
	);
	testRequire((iWritten > 0) && ((size_t)iWritten < sizeof(sLine)) &&
		(xrtSshPublicKeyLineRead(
			(xstrview){ sLine, (size_t)iWritten },
			&KeyLine
		) == XSSH_ERROR_PROTOCOL),
		"ssh text/blob algorithm mismatch was not rejected while parsing");
	iWritten = snprintf(
		sLine,
		sizeof(sLine),
		"ssh-ed25519 %s mismatch",
		sBase64
	);
	testRequire((iWritten > 0) && ((size_t)iWritten < sizeof(sLine)) &&
		(xrtSshPublicKeyLineRead(
			(xstrview){ sLine, (size_t)iWritten },
			&KeyLine
		) == XSSH_OK), "ssh valid key line parse failed");
	Keep.Algorithm.Data = (const char*)(uintptr_t)0x1234u;
	Keep.Algorithm.Size = 7u;
	Keep.Parameters.Data = (const unsigned char*)(uintptr_t)0x5678u;
	Keep.Parameters.Size = 9u;
	PublicKey = Keep;
	memset(arrDecoded, 0x5au, sizeof(arrDecoded));
	testRequire((xrtSshPublicKeyLineDecode(
		&KeyLine,
		arrDecoded,
		iBlobSize - 1u,
		&PublicKey
	) == XSSH_ERROR_SPACE) &&
		(memcmp(&PublicKey, &Keep, sizeof(PublicKey)) == 0) &&
		(arrDecoded[0] == 0x5au), "ssh short key decode changed outputs");
	KeyLine.Algorithm = XRT_STR_LITERAL("ssh-rsa");
	testRequire((xrtSshPublicKeyLineDecode(
		&KeyLine,
		arrDecoded,
		sizeof(arrDecoded),
		&PublicKey
	) == XSSH_ERROR_PROTOCOL) &&
		(memcmp(&PublicKey, &Keep, sizeof(PublicKey)) == 0),
		"ssh text/blob algorithm mismatch accepted");
	KeyLine.Algorithm = XRT_STR_LITERAL("ssh-ed25519");
	testRequire(xrtSshPublicKeyLineDecode(
		&KeyLine,
		(void*)KeyLine.Base64.Data,
		KeyLine.BlobSize,
		&PublicKey
	) == XSSH_ERROR_ARGUMENT, "ssh overlapping key decode accepted");
	testRequire(xrtSshPublicKeyLineRead(
		(xstrview){ sLine, (size_t)iWritten },
		(xsshopensshkeyline*)sLine
	) == XSSH_ERROR_ARGUMENT, "ssh overlapping key line output accepted");
	testRequire(xrtSshPublicKeyLineMatch(
		&KeyLine,
		(xbytesview){ arrBlob, iBlobSize },
		(bool*)KeyLine.Base64.Data
	) == XSSH_ERROR_ARGUMENT, "ssh overlapping key match output accepted");
}



/* 运行 OpenSSH 公钥文本格式测试。 */
int main(void)
{
	testSshKeyTextRead();
	testSshKeyTextExtensibleAlgorithm();
	testSshKeyTextMatchTails();
	testSshKeyTextRejectsInvalid();
	return 0;
}
