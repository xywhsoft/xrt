#ifdef XSSH_MODULE_XSSH
	#undef XSSH_MODULE_XSSH
#endif
#define XSSH_MODULE_SSH_PRIVATE_KEY
#define XSSH_IMPLEMENTATION
#include "../../single/xssh.h"



/* 构建最小未知公钥容器，验证私钥格式层不引入 PEM 或密码算法。 */
int main(void)
{
	static const unsigned char arrMagic[] = XSSH_PRIVATE_KEY_MAGIC;
	unsigned char arrPublic[32];
	unsigned char arrBinary[128];
	xsshwriter PublicWriter;
	xsshwriter Writer;
	xsshopensshprivatekey PrivateKey;
	bool bEncrypted = true;

	#if !defined(XSSH_FEATURE_PRIVATE_KEY) || \
		!defined(XSSH_FEATURE_HOSTKEY)
		#error "SSH private-key dependency closure is incomplete"
	#endif
	#if defined(XRT_FEATURE_PEM) || defined(XRT_FEATURE_CRYPTO_ED25519) || \
		defined(XRT_FEATURE_NETWORK)
		#error "SSH private-key unexpectedly enabled PEM, Ed25519 or network"
	#endif

	return xrtSshWriterInit(&PublicWriter, arrPublic, sizeof(arrPublic)) &&
		(xrtSshWriteString(
			&PublicWriter,
			XRT_BYTES_LITERAL("test-key")
		) == XSSH_OK) && xrtSshWriterInit(
			&Writer,
			arrBinary,
			sizeof(arrBinary)
		) && (xrtSshWriteBytes(
			&Writer,
			(xbytesview){ arrMagic, sizeof(arrMagic) }
		) == XSSH_OK) && (xrtSshWriteString(
			&Writer,
			XRT_BYTES_LITERAL("none")
		) == XSSH_OK) && (xrtSshWriteString(
			&Writer,
			XRT_BYTES_LITERAL("none")
		) == XSSH_OK) && (xrtSshWriteString(
			&Writer,
			(xbytesview){ NULL, 0u }
		) == XSSH_OK) && (xrtSshWriteU32(&Writer, 1u) == XSSH_OK) &&
		(xrtSshWriteString(
			&Writer,
			(xbytesview){ arrPublic, PublicWriter.Size }
		) == XSSH_OK) && (xrtSshWriteString(
			&Writer,
			XRT_BYTES_LITERAL("private")
		) == XSSH_OK) && (xrtSshPrivateKeyRead(
			(xbytesview){ arrBinary, Writer.Size },
			&PrivateKey
		) == XSSH_OK) && (xrtSshPrivateKeyIsEncrypted(
			&PrivateKey,
			&bEncrypted
		) == XSSH_OK) && !bEncrypted ? 0 : 1;
}
