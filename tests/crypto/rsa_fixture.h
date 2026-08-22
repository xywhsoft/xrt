#ifndef XRT_TEST_CRYPTO_RSA_FIXTURE_H
#define XRT_TEST_CRYPTO_RSA_FIXTURE_H

#include <xrt/crypto.h>
#include <xrt/error.h>

#include <string.h>



static const char __xrtTestRsaModulusHex[] =
	"a85c1f279cc3b32743765c4eb7637245808ddea69024913619cbc715e5f5d862"
	"0836236b50af06ebeaa4ceb7f3c312836206422290b1af8926d275c34fb6b0c7b"
	"fcecc7ea230113abc0390b162fc8088313b74977c07d2fb11e111493813db89c3"
	"2236ed32295129c423df6031e06b5f02907c14038fc9c7c1b5cd69e1aa46d9";

static const char __xrtTestRsaHashHex[] =
	"59c77e7eb2a002be887e13502f2f3c225488350efba44f0d17789883d5d27a8f";

static const char __xrtTestRsaSha224HashHex[] =
	"000102030405060708090a0b0c0d0e0f101112131415161718191a1b";

static const char __xrtTestRsaSaltHex[] =
	"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";

static const char __xrtTestRsaRawHex[] =
	"5bdb6dd1210b9cd58cc73db2209e3d03b9720c0f65d1b9fad4cad15321b08856"
	"361d0ec0de879420b15f41543e099b2bb0d6118c2dde550985464c538e12ea35"
	"764b4952f89d62d218a90df194cb69b5e2b10a9d01a151cea52bcf233400c0fc"
	"5c6c61a0b5154b04dda9889c24a0e28b2a2bab0ccb44c5aa27ff765cf6f66e46";

static const char __xrtTestRsaPssHex[] =
	"1baa0cc63e7036eb41420aa0da33bbf7d74cc892e9505216ccd2502de624ab7c"
	"d63fe3762037b5518bde573911c8e18397eed8a3613c36ac87659e58a7d5b3af"
	"0510150683236aa2b1f3593856a28434b0249502602cdf5e2591cf80870230e393"
	"36b18fa96fab5e633e39b1dd5b8ff61d8dd255636c6ec1cdcf94f2cd7977af";

static const char __xrtTestRsaPssSha224Hex[] =
	"516e82de63809896811bc25e348c5b554874aabc1faa2a9d582ea176d70f3936"
	"a4dd65eb11e0b9f0729d4c0349c268cbae2c1a4080a3ecfb137d4593959012e"
	"01bdccabfa5afe4a66a9ef3edb7a22463937421098e19f86c31f40b40a91efb"
	"e78a975a5e27fda186ea2a1ac34a0f5614487606db290a4d04da4abd83f006b"
	"60f";

static const char __xrtTestRsaPkcs1Hex[] =
	"5f6e811c69408bc1c49bcec0740822238fc9d39b0a72c3617066696e587dd6f8"
	"20826f8b429fc92e24a92664e5fd2c743b6cc2421f273db3de80f80d4ed2ff8c"
	"bcda52454f1c15d6bbe5efd8b0fe590eb591a2569653e28067dbe38db2417ff8a"
	"104ac375fef941b37bd54840ac56d5fd5b55270c3097eeb336360a426cf70b0";

static const char __xrtTestRsaPkcs1Sha224Hex[] =
	"10ef5c20388041e58408c12dab37e2d20f7e0592d8a8e523be7a37fdf41d782"
	"1a08a5b97881d9b936e4f2e89270ce0276f620f66027bb229b0052b972cfe53"
	"c0e61b9f867d929306d6c81a359d6f0f7625778f641bced76f621b15d186b5"
	"b2b1d3cc17d447d7340065bc91b6cfd69eb2dc16c22db0af30bd35d636c52e"
	"70e111";

#if defined(XRT_FEATURE_CRYPTO_RSA_PRIVATE)

static const char __xrtTestRsaPrivateExponentHex[] =
	"2ebfd5ced412eb6e5da7421e8181d5bb42e5220c323e552c26a1951899905893"
	"70204859863ff3b612a3ccdafb8f1156f62332be125ba4987af539ca101a749f"
	"af3a625dc02272afc1e8d102ce1b4efaf920ad8682e49cfd2a63a7dca606d337"
	"60a6444a582013a1a6965b8b97621fad9953684668e96750c9469b8237e2ab79";

static const char __xrtTestRsaPrime1Hex[] =
	"dcae97bce244aa4f0267c1be85727b8411cb65e56d9424900fe8885045926423"
	"00f85edd6f7b09c3b504ec60ffe10045915d4e1d9465f2ba1de82e00665dba9f";

static const char __xrtTestRsaPrime2Hex[] =
	"c34ddff4c78a93fd78456bce716a36385e2f000c72bcb8a988468cbcfe460723"
	"dafbc61a5557a79fc2c333012e08e28943b83c41fcf02901fbd161ba84d30387";

static const char __xrtTestRsaExponent1Hex[] =
	"c54333d3dd966f7e4cb21e978585fe2ee21124b65eab2bbbb2dcb2ecbe23cb6b"
	"16b4ed6796fd2e8f48426c619098b9bb75e327ae7d365c304e077b075479b815";

static const char __xrtTestRsaExponent2Hex[] =
	"1e80ff45c014c8e081f475ce1cb0b61f3fb69f8f522c5fbb3ae9a9f9aacd4d3"
	"8306fba954a57127b45742f7733b5778c70e349a614d77dd02d809a7f0357e1e9";

static const char __xrtTestRsaCoefficientHex[] =
	"191a552c1a823f413345c0fff7a14f68d1a9a9c57a0f34b9eabbf75db2b41e7"
	"4ef44105bb6515affe674e0f598c17f620242aa6230c2a95bf27484bb9fea05fc";

#endif



static const uint8 __xrtTestRsaPublicExponent[3] = { 1, 0, 1 };



/* 把固定测试向量中的小写十六进制文本解码为字节。 */
static bool __xrtTestRsaHex(
	const char* sText,
	uint8* pOutput,
	size_t iOutputSize
)
{
	for ( size_t i = 0; i < iOutputSize; i++ ) {
		uint8 iHigh;
		uint8 iLow;
		char cHigh = sText[i * 2u];
		char cLow = sText[(i * 2u) + 1u];

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



/* 初始化固定 RSA 公钥和摘要测试资产。 */
static bool __xrtTestRsaFixture(
	xrsapublickey* pKey,
	uint8 pModulus[128],
	uint8 pHash[32]
)
{
	if ( !__xrtTestRsaHex(__xrtTestRsaModulusHex, pModulus, 128u) ||
		 !__xrtTestRsaHex(__xrtTestRsaHashHex, pHash, 32u) ) {
		return false;
	}
	pKey->Modulus = pModulus;
	pKey->ModulusSize = 128u;
	pKey->Exponent = __xrtTestRsaPublicExponent;
	pKey->ExponentSize = sizeof(__xrtTestRsaPublicExponent);
	return true;
}



#if defined(XRT_FEATURE_CRYPTO_RSA_PRIVATE)

typedef struct __xrt_test_rsa_private_fixture {
	xrsaprivatekey Key;
	uint8 Modulus[128];
	uint8 PrivateExponent[128];
	uint8 Prime1[64];
	uint8 Prime2[64];
	uint8 Exponent1[64];
	uint8 Exponent2[64];
	uint8 Coefficient[64];
	uint8 Hash[32];
	uint8 Salt[32];
} __xrt_test_rsa_private_fixture;



/* 解码完整指数与 CRT 五参数，并建立不拥有字节的私钥视图。 */
static inline bool __xrtTestRsaPrivateFixture(
	__xrt_test_rsa_private_fixture* pFixture
)
{
	memset(pFixture, 0, sizeof(*pFixture));
	if ( !__xrtTestRsaFixture(
		&pFixture->Key.Public,
		pFixture->Modulus,
		pFixture->Hash
	) ||
		 !__xrtTestRsaHex(
			__xrtTestRsaPrivateExponentHex,
			pFixture->PrivateExponent,
			sizeof(pFixture->PrivateExponent)
		 ) ||
		 !__xrtTestRsaHex(
			__xrtTestRsaPrime1Hex,
			pFixture->Prime1,
			sizeof(pFixture->Prime1)
		 ) ||
		 !__xrtTestRsaHex(
			__xrtTestRsaPrime2Hex,
			pFixture->Prime2,
			sizeof(pFixture->Prime2)
		 ) ||
		 !__xrtTestRsaHex(
			__xrtTestRsaExponent1Hex,
			pFixture->Exponent1,
			sizeof(pFixture->Exponent1)
		 ) ||
		 !__xrtTestRsaHex(
			__xrtTestRsaExponent2Hex,
			pFixture->Exponent2,
			sizeof(pFixture->Exponent2)
		 ) ||
		 !__xrtTestRsaHex(
			__xrtTestRsaCoefficientHex,
			pFixture->Coefficient,
			sizeof(pFixture->Coefficient)
		 ) ||
		 !__xrtTestRsaHex(
			__xrtTestRsaSaltHex,
			pFixture->Salt,
			sizeof(pFixture->Salt)
		 ) ) {
		return false;
	}

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

#endif
