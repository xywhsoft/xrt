#ifndef XRT_EXAMPLE_CRYPTO_RSA_FIXTURE_H
#define XRT_EXAMPLE_CRYPTO_RSA_FIXTURE_H

#include <string.h>
#include <xrt.h>



/* 固定 1024 位密钥只用于演示 API，不得复制到真实部署。 */
static const char __xrtExampleRsaModulusHex[] =
	"a85c1f279cc3b32743765c4eb7637245808ddea69024913619cbc715e5f5d862"
	"0836236b50af06ebeaa4ceb7f3c312836206422290b1af8926d275c34fb6b0c7b"
	"fcecc7ea230113abc0390b162fc8088313b74977c07d2fb11e111493813db89c3"
	"2236ed32295129c423df6031e06b5f02907c14038fc9c7c1b5cd69e1aa46d9";

static const char __xrtExampleRsaPrivateExponentHex[] =
	"2ebfd5ced412eb6e5da7421e8181d5bb42e5220c323e552c26a1951899905893"
	"70204859863ff3b612a3ccdafb8f1156f62332be125ba4987af539ca101a749f"
	"af3a625dc02272afc1e8d102ce1b4efaf920ad8682e49cfd2a63a7dca606d337"
	"60a6444a582013a1a6965b8b97621fad9953684668e96750c9469b8237e2ab79";

static const char __xrtExampleRsaPrime1Hex[] =
	"dcae97bce244aa4f0267c1be85727b8411cb65e56d9424900fe8885045926423"
	"00f85edd6f7b09c3b504ec60ffe10045915d4e1d9465f2ba1de82e00665dba9f";

static const char __xrtExampleRsaPrime2Hex[] =
	"c34ddff4c78a93fd78456bce716a36385e2f000c72bcb8a988468cbcfe460723"
	"dafbc61a5557a79fc2c333012e08e28943b83c41fcf02901fbd161ba84d30387";

static const char __xrtExampleRsaExponent1Hex[] =
	"c54333d3dd966f7e4cb21e978585fe2ee21124b65eab2bbbb2dcb2ecbe23cb6b"
	"16b4ed6796fd2e8f48426c619098b9bb75e327ae7d365c304e077b075479b815";

static const char __xrtExampleRsaExponent2Hex[] =
	"1e80ff45c014c8e081f475ce1cb0b61f3fb69f8f522c5fbb3ae9a9f9aacd4d3"
	"8306fba954a57127b45742f7733b5778c70e349a614d77dd02d809a7f0357e1e9";

static const char __xrtExampleRsaCoefficientHex[] =
	"191a552c1a823f413345c0fff7a14f68d1a9a9c57a0f34b9eabbf75db2b41e7"
	"4ef44105bb6515affe674e0f598c17f620242aa6230c2a95bf27484bb9fea05fc";

static const char __xrtExampleRsaHashHex[] =
	"59c77e7eb2a002be887e13502f2f3c225488350efba44f0d17789883d5d27a8f";

static const char __xrtExampleRsaSaltHex[] =
	"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";

static const uint8 __xrtExampleRsaExponent[3] = { 1u, 0u, 1u };



typedef struct __xrt_example_rsa_fixture {
	xrsaprivatekey Key;
	uint8 Modulus[128];
	uint8 PrivateExponent[128];
	uint8 Prime1[64];
	uint8 Prime2[64];
	uint8 Exponent1[64];
	uint8 Exponent2[64];
	uint8 Coefficient[64];
	uint8 Hash[XRT_SHA256_SIZE];
	uint8 Salt[XRT_SHA256_SIZE];
} __xrt_example_rsa_fixture;



/* 把固定示例资产中的小写十六进制文本解码为字节。 */
static bool __xrtExampleRsaHex(
	const char* sText,
	uint8* pOutput,
	size_t iOutputSize
)
{
	for ( size_t i = 0; i < iOutputSize; i++ ) {
		char cHigh = sText[i * 2u];
		char cLow = sText[(i * 2u) + 1u];
		uint8 iHigh;
		uint8 iLow;

		/* 示例资产固定使用十进制数字和小写十六进制字母。 */
		if ( (cHigh >= '0') && (cHigh <= '9') ) {
			iHigh = (uint8)(cHigh - '0');
		} else if ( (cHigh >= 'a') && (cHigh <= 'f') ) {
			iHigh = (uint8)(cHigh - 'a' + 10);
		} else {
			return false;
		}
		if ( (cLow >= '0') && (cLow <= '9') ) {
			iLow = (uint8)(cLow - '0');
		} else if ( (cLow >= 'a') && (cLow <= 'f') ) {
			iLow = (uint8)(cLow - 'a' + 10);
		} else {
			return false;
		}
		pOutput[i] = (uint8)((iHigh << 4u) | iLow);
	}
	return sText[iOutputSize * 2u] == '\0';
}



/* 解码完整指数和 CRT 参数，并建立不拥有字节的私钥视图。 */
static bool __xrtExampleRsaInit(__xrt_example_rsa_fixture* pFixture)
{
	memset(pFixture, 0, sizeof(*pFixture));
	if ( !__xrtExampleRsaHex(
			__xrtExampleRsaModulusHex,
			pFixture->Modulus,
			sizeof(pFixture->Modulus)
		) || !__xrtExampleRsaHex(
			__xrtExampleRsaPrivateExponentHex,
			pFixture->PrivateExponent,
			sizeof(pFixture->PrivateExponent)
		) || !__xrtExampleRsaHex(
			__xrtExampleRsaPrime1Hex,
			pFixture->Prime1,
			sizeof(pFixture->Prime1)
		) || !__xrtExampleRsaHex(
			__xrtExampleRsaPrime2Hex,
			pFixture->Prime2,
			sizeof(pFixture->Prime2)
		) || !__xrtExampleRsaHex(
			__xrtExampleRsaExponent1Hex,
			pFixture->Exponent1,
			sizeof(pFixture->Exponent1)
		) || !__xrtExampleRsaHex(
			__xrtExampleRsaExponent2Hex,
			pFixture->Exponent2,
			sizeof(pFixture->Exponent2)
		) || !__xrtExampleRsaHex(
			__xrtExampleRsaCoefficientHex,
			pFixture->Coefficient,
			sizeof(pFixture->Coefficient)
		) || !__xrtExampleRsaHex(
			__xrtExampleRsaHashHex,
			pFixture->Hash,
			sizeof(pFixture->Hash)
		) || !__xrtExampleRsaHex(
			__xrtExampleRsaSaltHex,
			pFixture->Salt,
			sizeof(pFixture->Salt)
		) ) {
		return false;
	}

	/* 视图字段直接指向同一个 fixture 的固定字节区间。 */
	pFixture->Key.Public.Modulus = pFixture->Modulus;
	pFixture->Key.Public.ModulusSize = sizeof(pFixture->Modulus);
	pFixture->Key.Public.Exponent = __xrtExampleRsaExponent;
	pFixture->Key.Public.ExponentSize = sizeof(__xrtExampleRsaExponent);
	pFixture->Key.PrivateExponent = pFixture->PrivateExponent;
	pFixture->Key.PrivateExponentSize = sizeof(pFixture->PrivateExponent);
	pFixture->Key.Prime1 = pFixture->Prime1;
	pFixture->Key.Prime1Size = sizeof(pFixture->Prime1);
	pFixture->Key.Prime2 = pFixture->Prime2;
	pFixture->Key.Prime2Size = sizeof(pFixture->Prime2);
	pFixture->Key.Exponent1 = pFixture->Exponent1;
	pFixture->Key.Exponent1Size = sizeof(pFixture->Exponent1);
	pFixture->Key.Exponent2 = pFixture->Exponent2;
	pFixture->Key.Exponent2Size = sizeof(pFixture->Exponent2);
	pFixture->Key.Coefficient = pFixture->Coefficient;
	pFixture->Key.CoefficientSize = sizeof(pFixture->Coefficient);
	return true;
}

#endif
