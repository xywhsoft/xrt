#ifndef XSSH_PRIVATE_KEY_FIXTURE_H
#define XSSH_PRIVATE_KEY_FIXTURE_H

#include "../test.h"



typedef struct testsshprivatekeyfixture {
	unsigned char Binary[512];
	size_t BinarySize;
	char Pem[1024];
	size_t PemSize;
	unsigned char Seed[32];
	unsigned char Public[XSSH_ED25519_PUBLIC_SIZE];
	unsigned char PublicKeyBlob[64];
	size_t PublicKeyBlobSize;
} testsshprivatekeyfixture;



/* 构建确定性的未加密单 Ed25519 openssh-key-v1 与 PEM 夹具。 */
static inline void testSshPrivateKeyFixture(
	testsshprivatekeyfixture* pFixture
)
{
	static const unsigned char arrMagic[] = XSSH_PRIVATE_KEY_MAGIC;
	static const unsigned char arrComment[] = "fixture@example";
	unsigned char arrPrivateRaw[32u + XSSH_ED25519_PUBLIC_SIZE];
	unsigned char arrPrivateList[256];
	xsshwriter PublicWriter;
	xsshwriter PrivateWriter;
	xsshwriter Writer;
	uint8 iPadding = 1u;
	size_t iPemSize;
	size_t i;

	memset(pFixture, 0, sizeof(*pFixture));
	for ( i = 0u; i < sizeof(pFixture->Seed); ++i ) {
		pFixture->Seed[i] = (unsigned char)(0x40u + i);
	}
	#if defined(XRT_FEATURE_CRYPTO_ED25519)
		testRequire(xrtEd25519Public(
			pFixture->Seed,
			pFixture->Public
		), "ssh private fixture public derivation failed");
	#else
		for ( i = 0u; i < sizeof(pFixture->Public); ++i ) {
			pFixture->Public[i] = (unsigned char)(0x80u + i);
		}
	#endif
	testRequire(xrtSshWriterInit(
		&PublicWriter,
		pFixture->PublicKeyBlob,
		sizeof(pFixture->PublicKeyBlob)
	) && (xrtSshEd25519PublicKeyWrite(
		&PublicWriter,
		(xbytesview){ pFixture->Public, sizeof(pFixture->Public) }
	) == XSSH_OK), "ssh private fixture public blob failed");
	pFixture->PublicKeyBlobSize = PublicWriter.Size;
	memcpy(arrPrivateRaw, pFixture->Seed, sizeof(pFixture->Seed));
	memcpy(
		arrPrivateRaw + sizeof(pFixture->Seed),
		pFixture->Public,
		sizeof(pFixture->Public)
	);
	testRequire(xrtSshWriterInit(
		&PrivateWriter,
		arrPrivateList,
		sizeof(arrPrivateList)
	) && (xrtSshWriteU32(&PrivateWriter, UINT32_C(0x12345678)) == XSSH_OK) &&
		(xrtSshWriteU32(&PrivateWriter, UINT32_C(0x12345678)) == XSSH_OK) &&
		(xrtSshWriteString(
			&PrivateWriter,
			XRT_BYTES_LITERAL(XSSH_HOSTKEY_ED25519)
		) == XSSH_OK) && (xrtSshWriteString(
			&PrivateWriter,
			(xbytesview){ pFixture->Public, sizeof(pFixture->Public) }
		) == XSSH_OK) && (xrtSshWriteString(
			&PrivateWriter,
			(xbytesview){ arrPrivateRaw, sizeof(arrPrivateRaw) }
		) == XSSH_OK) && (xrtSshWriteString(
			&PrivateWriter,
			(xbytesview){ arrComment, sizeof(arrComment) - 1u }
		) == XSSH_OK), "ssh private fixture list failed");
	while ( (PrivateWriter.Size % 8u) != 0u ) {
		testRequire(xrtSshWriteByte(
			&PrivateWriter,
			iPadding++
		) == XSSH_OK, "ssh private fixture padding failed");
	}
	testRequire(xrtSshWriterInit(
		&Writer,
		pFixture->Binary,
		sizeof(pFixture->Binary)
	) && (xrtSshWriteBytes(
		&Writer,
		(xbytesview){ arrMagic, sizeof(arrMagic) }
	) == XSSH_OK) && (xrtSshWriteString(
		&Writer,
		XRT_BYTES_LITERAL(XSSH_PRIVATE_KEY_NONE)
	) == XSSH_OK) && (xrtSshWriteString(
		&Writer,
		XRT_BYTES_LITERAL(XSSH_PRIVATE_KEY_NONE)
	) == XSSH_OK) && (xrtSshWriteString(
		&Writer,
		(xbytesview){ NULL, 0u }
	) == XSSH_OK) && (xrtSshWriteU32(&Writer, 1u) == XSSH_OK) &&
		(xrtSshWriteString(
			&Writer,
			(xbytesview){
				pFixture->PublicKeyBlob,
				pFixture->PublicKeyBlobSize
			}
		) == XSSH_OK) && (xrtSshWriteString(
			&Writer,
			(xbytesview){ arrPrivateList, PrivateWriter.Size }
		) == XSSH_OK), "ssh private fixture container failed");
	pFixture->BinarySize = Writer.Size;
	#if defined(XRT_FEATURE_PEM)
		testRequire(xrtPemEncode(
			"OPENSSH PRIVATE KEY",
			pFixture->Binary,
			pFixture->BinarySize,
			NULL,
			0u,
			&iPemSize
		) && (iPemSize < sizeof(pFixture->Pem)) && xrtPemEncode(
			"OPENSSH PRIVATE KEY",
			pFixture->Binary,
			pFixture->BinarySize,
			pFixture->Pem,
			sizeof(pFixture->Pem),
			&iPemSize
		), "ssh private fixture PEM failed");
		pFixture->PemSize = iPemSize;
	#else
		(void)iPemSize;
	#endif
	xrtSecureZero(arrPrivateRaw, sizeof(arrPrivateRaw));
	xrtSecureZero(arrPrivateList, sizeof(arrPrivateList));
}

#endif
