#include "../test.h"



/* 生成固定测试字节，避免格式测试依赖密码模块。 */
static void testSshHostKeyFill(
	unsigned char* pData,
	size_t iSize,
	unsigned char iStart
)
{
	size_t i;

	for ( i = 0u; i < iSize; ++i ) {
		pData[i] = (unsigned char)(iStart + i);
	}
}



/* 验证 Ed25519 key/signature blob 的规范编解码与通用视图。 */
static void testSshHostKeyRoundtrip(void)
{
	unsigned char arrPublic[XSSH_ED25519_PUBLIC_SIZE];
	unsigned char arrSignature[XSSH_ED25519_SIGNATURE_SIZE];
	unsigned char arrKeyBlob[96];
	unsigned char arrSignatureBlob[112];
	xsshwriter KeyWriter;
	xsshwriter SignatureWriter;
	xsshpublickey GenericKey;
	xsshsignature GenericSignature;
	xbytesview Public;
	xbytesview Signature;

	testSshHostKeyFill(arrPublic, sizeof(arrPublic), 0x10u);
	testSshHostKeyFill(arrSignature, sizeof(arrSignature), 0x40u);
	testRequire(xrtSshWriterInit(
		&KeyWriter,
		arrKeyBlob,
		sizeof(arrKeyBlob)
	) && xrtSshWriterInit(
		&SignatureWriter,
		arrSignatureBlob,
		sizeof(arrSignatureBlob)
	) && (xrtSshEd25519PublicKeyWrite(
		&KeyWriter,
		(xbytesview){ arrPublic, sizeof(arrPublic) }
	) == XSSH_OK) && (xrtSshEd25519SignatureWrite(
		&SignatureWriter,
		(xbytesview){ arrSignature, sizeof(arrSignature) }
	) == XSSH_OK), "ssh hostkey blob write failed");

	testRequire((xrtSshPublicKeyRead(
		(xbytesview){ arrKeyBlob, KeyWriter.Size },
		&GenericKey
	) == XSSH_OK) && testSshTextEqual(
		GenericKey.Algorithm,
		XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519)
	) && (GenericKey.Parameters.Size == 4u + sizeof(arrPublic)) &&
		(xrtSshSignatureRead(
			(xbytesview){ arrSignatureBlob, SignatureWriter.Size },
			&GenericSignature
		) == XSSH_OK) && testSshTextEqual(
		GenericSignature.Algorithm,
		XRT_STR_LITERAL(XSSH_HOSTKEY_ED25519)
	), "ssh generic hostkey views failed");

	testRequire((xrtSshEd25519PublicKeyRead(
		(xbytesview){ arrKeyBlob, KeyWriter.Size },
		&Public
	) == XSSH_OK) && testSshBytesEqual(
		Public,
		(xbytesview){ arrPublic, sizeof(arrPublic) }
	) && (xrtSshEd25519SignatureRead(
		(xbytesview){ arrSignatureBlob, SignatureWriter.Size },
		&Signature
	) == XSSH_OK) && testSshBytesEqual(
		Signature,
		(xbytesview){ arrSignature, sizeof(arrSignature) }
	), "ssh ed25519 hostkey views failed");
}



/* 验证截断、尾随、错误算法与错误长度不会发布输出。 */
static void testSshHostKeyRejectsInvalid(void)
{
	unsigned char arrPublic[XSSH_ED25519_PUBLIC_SIZE] = { 0u };
	unsigned char arrBlob[96];
	xsshwriter Writer;
	xbytesview Value;
	xbytesview Keep;
	size_t iBlobSize;

	testRequire(xrtSshWriterInit(&Writer, arrBlob, sizeof(arrBlob)) &&
		(xrtSshEd25519PublicKeyWrite(
			&Writer,
			(xbytesview){ arrPublic, sizeof(arrPublic) }
		) == XSSH_OK), "ssh hostkey invalid setup failed");
	iBlobSize = Writer.Size;
	Keep.Data = (const unsigned char*)(uintptr_t)0x1234u;
	Keep.Size = 77u;
	Value = Keep;
	testRequire((xrtSshEd25519PublicKeyRead(
		(xbytesview){ arrBlob, iBlobSize - 1u },
		&Value
	) == XSSH_NEED_MORE) && (memcmp(&Value, &Keep, sizeof(Value)) == 0),
		"ssh hostkey truncation changed output");

	arrBlob[iBlobSize] = 0u;
	testRequire((xrtSshEd25519PublicKeyRead(
		(xbytesview){ arrBlob, iBlobSize + 1u },
		&Value
	) == XSSH_ERROR_PROTOCOL) && (memcmp(&Value, &Keep, sizeof(Value)) == 0),
		"ssh hostkey trailing data was accepted");

	arrBlob[4] = 'x';
	testRequire((xrtSshEd25519PublicKeyRead(
		(xbytesview){ arrBlob, iBlobSize },
		&Value
	) == XSSH_ERROR_UNSUPPORTED) &&
		(memcmp(&Value, &Keep, sizeof(Value)) == 0),
		"ssh hostkey wrong algorithm was accepted");

	testRequire(xrtSshWriterInit(&Writer, arrBlob, sizeof(arrBlob)) &&
		(xrtSshWriteString(
			&Writer,
			XRT_BYTES_LITERAL(XSSH_HOSTKEY_ED25519)
		) == XSSH_OK) && (xrtSshWriteString(
			&Writer,
			(xbytesview){ arrPublic, sizeof(arrPublic) - 1u }
		) == XSSH_OK) && (xrtSshEd25519PublicKeyRead(
			(xbytesview){ arrBlob, Writer.Size },
			&Value
		) == XSSH_ERROR_PROTOCOL), "ssh hostkey wrong length was accepted");
}



/* 验证容量和输入重叠失败不会提交 writer 状态。 */
static void testSshHostKeyWriteAtomic(void)
{
	unsigned char arrData[128];
	xsshwriter Writer;
	xsshwriter Keep;
	xbytesview Signature;

	memset(arrData, 0x5au, sizeof(arrData));
	Signature.Data = arrData + 32u;
	Signature.Size = XSSH_ED25519_SIGNATURE_SIZE;
	testRequire(xrtSshWriterInit(&Writer, arrData, 16u),
		"ssh hostkey short writer init failed");
	Keep = Writer;
	testRequire((xrtSshEd25519SignatureWrite(
		&Writer,
		Signature
	) == XSSH_ERROR_SPACE) &&
		(memcmp(&Writer, &Keep, sizeof(Writer)) == 0) &&
		(arrData[0] == 0x5au), "ssh hostkey short write was partial");

	testRequire(xrtSshWriterInit(&Writer, arrData, sizeof(arrData)),
		"ssh hostkey overlap writer init failed");
	Signature.Data = arrData + 16u;
	Keep = Writer;
	testRequire((xrtSshEd25519SignatureWrite(
		&Writer,
		Signature
	) == XSSH_ERROR_ARGUMENT) &&
		(memcmp(&Writer, &Keep, sizeof(Writer)) == 0),
		"ssh hostkey overlapping input was accepted");
}



/* 运行主机密钥格式层边界测试。 */
int main(void)
{
	testSshHostKeyRoundtrip();
	testSshHostKeyRejectsInvalid();
	testSshHostKeyWriteAtomic();
	return 0;
}
