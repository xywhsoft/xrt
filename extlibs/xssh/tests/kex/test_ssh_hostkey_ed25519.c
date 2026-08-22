#include "../test.h"



/* 验证 RFC 8032 空消息向量经 SSH blob 封装后仍能严格认证。 */
static void testSshEd25519VerifyVector(void)
{
	static const unsigned char arrPublic[XSSH_ED25519_PUBLIC_SIZE] = {
		0xd7u, 0x5au, 0x98u, 0x01u, 0x82u, 0xb1u, 0x0au, 0xb7u,
		0xd5u, 0x4bu, 0xfeu, 0xd3u, 0xc9u, 0x64u, 0x07u, 0x3au,
		0x0eu, 0xe1u, 0x72u, 0xf3u, 0xdau, 0xa6u, 0x23u, 0x25u,
		0xafu, 0x02u, 0x1au, 0x68u, 0xf7u, 0x07u, 0x51u, 0x1au
	};
	static const unsigned char arrSignature[XSSH_ED25519_SIGNATURE_SIZE] = {
		0xe5u, 0x56u, 0x43u, 0x00u, 0xc3u, 0x60u, 0xacu, 0x72u,
		0x90u, 0x86u, 0xe2u, 0xccu, 0x80u, 0x6eu, 0x82u, 0x8au,
		0x84u, 0x87u, 0x7fu, 0x1eu, 0xb8u, 0xe5u, 0xd9u, 0x74u,
		0xd8u, 0x73u, 0xe0u, 0x65u, 0x22u, 0x49u, 0x01u, 0x55u,
		0x5fu, 0xb8u, 0x82u, 0x15u, 0x90u, 0xa3u, 0x3bu, 0xacu,
		0xc6u, 0x1eu, 0x39u, 0x70u, 0x1cu, 0xf9u, 0xb4u, 0x6bu,
		0xd2u, 0x5bu, 0xf5u, 0xf0u, 0x59u, 0x5bu, 0xbeu, 0x24u,
		0x65u, 0x51u, 0x41u, 0x43u, 0x8eu, 0x7au, 0x10u, 0x0bu
	};
	unsigned char arrKeyBlob[96];
	unsigned char arrSignatureBlob[112];
	xsshwriter KeyWriter;
	xsshwriter SignatureWriter;

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
	) == XSSH_OK), "ssh ed25519 vector setup failed");

	testRequire(xrtSshEd25519HostKeyVerify(
		(xbytesview){ arrKeyBlob, KeyWriter.Size },
		(xbytesview){ arrSignatureBlob, SignatureWriter.Size },
		(xbytesview){ NULL, 0u }
	) == XSSH_OK, "ssh ed25519 vector verification failed");

	arrSignatureBlob[SignatureWriter.Size - 1u] ^= 0x01u;
	testRequire(xrtSshEd25519HostKeyVerify(
		(xbytesview){ arrKeyBlob, KeyWriter.Size },
		(xbytesview){ arrSignatureBlob, SignatureWriter.Size },
		(xbytesview){ NULL, 0u }
	) == XSSH_ERROR_AUTHENTICATION,
		"ssh ed25519 bad signature was not an authentication error");
}



/* 运行 Ed25519 主机密钥认证测试。 */
int main(void)
{
	testSshEd25519VerifyVector();
	return 0;
}
