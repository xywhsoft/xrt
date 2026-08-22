#include "../test.h"



/* 构建历史 exchange hash 向量输入。 */
static void testSshKexHashFixture(
	xsshkexhashsha256* pInput,
	uint8* pClient,
	uint8* pServer,
	uint8* pShared
)
{
	static const unsigned char arrClientKex[] = {
		20u, 'c', 'l', 'i', 'e', 'n', 't', '-',
		'k', 'e', 'x', 'i', 'n', 'i', 't'
	};
	static const unsigned char arrServerKex[] = {
		20u, 's', 'e', 'r', 'v', 'e', 'r', '-',
		'k', 'e', 'x', 'i', 'n', 'i', 't'
	};
	size_t i;

	for ( i = 0u; i < 32u; ++i ) {
		pClient[i] = (uint8)(i + 1u);
		pServer[i] = (uint8)(i + 65u);
		pShared[i] = i == 0u ? 0x80u : (uint8)i;
	}
	memset(pInput, 0, sizeof(*pInput));
	pInput->ClientVersion = XRT_BYTES_LITERAL("SSH-2.0-xssh-client");
	pInput->ServerVersion = XRT_BYTES_LITERAL("SSH-2.0-xssh-server");
	pInput->ClientKexInit = (xbytesview){ arrClientKex, sizeof(arrClientKex) };
	pInput->ServerKexInit = (xbytesview){ arrServerKex, sizeof(arrServerKex) };
	pInput->ServerHostKey = XRT_BYTES_LITERAL("server-host-key");
	pInput->ClientEphemeral = (xbytesview){ pClient, 32u };
	pInput->ServerEphemeral = (xbytesview){ pServer, 32u };
	pInput->SharedSecret = (xbytesview){ pShared, 32u };
}



/* 验证 exchange hash 与跨摘要长度密钥扩展向量。 */
static void testSshKexHashVectors(void)
{
	static const unsigned char arrExpectedHash[XSSH_SHA256_SIZE] = {
		0xe8u, 0x4fu, 0x65u, 0xafu, 0x56u, 0x89u, 0xbbu, 0xbcu,
		0x95u, 0xa6u, 0x6cu, 0xceu, 0x16u, 0x49u, 0x48u, 0x27u,
		0xcau, 0x13u, 0x3fu, 0xecu, 0xcfu, 0xe3u, 0xccu, 0x92u,
		0x63u, 0x4eu, 0x3au, 0x3du, 0x41u, 0x11u, 0xa3u, 0x26u
	};
	static const unsigned char arrExpectedKey[40] = {
		0xf5u, 0x30u, 0xf8u, 0x05u, 0xe6u, 0x4du, 0xbeu, 0x83u,
		0xcdu, 0x70u, 0x36u, 0x21u, 0xa4u, 0xc3u, 0xb4u, 0x5bu,
		0x17u, 0x18u, 0x05u, 0x22u, 0x90u, 0x4cu, 0x4au, 0xdau,
		0x49u, 0xf8u, 0xf7u, 0x4cu, 0xcfu, 0x25u, 0xfeu, 0xfcu,
		0xfdu, 0x0cu, 0x44u, 0xdfu, 0x88u, 0x3bu, 0xdcu, 0xa2u
	};
	uint8 arrClient[32];
	uint8 arrServer[32];
	uint8 arrShared[32];
	uint8 arrHash[XSSH_SHA256_SIZE];
	uint8 arrKey[40];
	xsshkexhashsha256 Input;

	testSshKexHashFixture(&Input, arrClient, arrServer, arrShared);
	testRequire((xrtSshKexHashSha256(&Input, arrHash) == XSSH_OK) &&
		(memcmp(arrHash, arrExpectedHash, sizeof(arrHash)) == 0),
		"ssh kex exchange hash vector mismatch");
	testRequire((xrtSshKexDeriveSha256(
		arrKey,
		sizeof(arrKey),
		Input.SharedSecret,
		arrHash,
		arrHash,
		'A'
	) == XSSH_OK) &&
		(memcmp(arrKey, arrExpectedKey, sizeof(arrKey)) == 0),
		"ssh kex key derivation vector mismatch");
}



/* 验证非法共享秘密、标签和输出重叠在写入前失败。 */
static void testSshKexHashEdges(void)
{
	uint8 arrClient[32];
	uint8 arrServer[32];
	uint8 arrShared[64];
	uint8 arrHash[XSSH_SHA256_SIZE] = { 0u };
	uint8 arrOutput[40];
	uint8 arrKeep[40];
	xsshkexhashsha256 Input;

	testSshKexHashFixture(&Input, arrClient, arrServer, arrShared);
	memset(arrOutput, 0x5a, sizeof(arrOutput));
	memcpy(arrKeep, arrOutput, sizeof(arrKeep));
	testRequire((xrtSshKexDeriveSha256(
		arrOutput,
		sizeof(arrOutput),
		Input.SharedSecret,
		arrHash,
		arrHash,
		'G'
	) == XSSH_ERROR_ARGUMENT) &&
		(memcmp(arrOutput, arrKeep, sizeof(arrOutput)) == 0),
		"ssh kex invalid derivation label changed output");
	memset(arrShared, 0, 32u);
	testRequire(xrtSshKexHashSha256(&Input, arrHash) == XSSH_ERROR_ARGUMENT,
		"ssh kex accepted zero shared secret");
	testSshKexHashFixture(&Input, arrClient, arrServer, arrShared);
	testRequire(xrtSshKexDeriveSha256(
		arrShared,
		32u,
		Input.SharedSecret,
		arrHash,
		arrHash,
		'A'
	) == XSSH_ERROR_ARGUMENT, "ssh kex accepted overlapping key output");
}



/* 运行 SHA-256 exchange hash 与派生测试。 */
int main(void)
{
	testSshKexHashVectors();
	testSshKexHashEdges();
	return 0;
}
