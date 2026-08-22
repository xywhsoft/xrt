#include "../test.h"



/* 生成可重复的伪随机测试数据。 */
static uint32 testNext(uint32* pState)
{
	uint32 iValue = *pState;

	iValue ^= iValue << 13u;
	iValue ^= iValue >> 17u;
	iValue ^= iValue << 5u;
	*pState = iValue;
	return iValue;
}



/* 使用独立的直接算法生成标准或 URL-safe Base64 参考文本。 */
static size_t testEncode(const uint8* pData, size_t iSize,
	char* sOutput, bool bUrl, bool bPadding)
{
	static const char sStandard[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	static const char sUrl[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	const char* sAlphabet = bUrl ? sUrl : sStandard;
	size_t iRead = 0;
	size_t iWrite = 0;

	while ( iRead < iSize ) {
		size_t iRemain = iSize - iRead;
		uint32 iValue = (uint32)pData[iRead] << 16u;

		if ( iRemain >= 2u ) {
			iValue |= (uint32)pData[iRead + 1u] << 8u;
		}
		if ( iRemain >= 3u ) {
			iValue |= (uint32)pData[iRead + 2u];
		}
		sOutput[iWrite++] = sAlphabet[(iValue >> 18u) & 0x3Fu];
		sOutput[iWrite++] = sAlphabet[(iValue >> 12u) & 0x3Fu];
		if ( iRemain >= 2u ) {
			sOutput[iWrite++] = sAlphabet[(iValue >> 6u) & 0x3Fu];
		} else if ( bPadding ) {
			sOutput[iWrite++] = '=';
		}
		if ( iRemain >= 3u ) {
			sOutput[iWrite++] = sAlphabet[iValue & 0x3Fu];
		} else if ( bPadding ) {
			sOutput[iWrite++] = '=';
		}
		iRead += iRemain >= 3u ? 3u : iRemain;
	}
	sOutput[iWrite] = 0;
	return iWrite;
}



/* 随机交叉验证四种内置模式的编码文本和完整往返。 */
int main(void)
{
	uint8 arrInput[257];
	uint8 arrOutput[257];
	char arrEncoded[349];
	char arrExpected[349];
	uint32 iState = UINT32_C(0xB64C0DEC);

	for ( size_t iRound = 0; iRound < 50000u; iRound++ ) {
		size_t iInputSize = testNext(&iState) % (sizeof(arrInput) + 1u);

		for ( size_t i = 0; i < iInputSize; i++ ) {
			arrInput[i] = (uint8)testNext(&iState);
		}
		for ( uint32 iMode = 0; iMode < 4u; iMode++ ) {
			xbase64config Config;
			bool bUrl = (iMode & 1u) != 0;
			bool bPadding = (iMode & 2u) == 0;
			size_t iExpectedSize;
			size_t iEncodedSize = SIZE_MAX;
			size_t iOutputSize = SIZE_MAX;

			memset(&Config, 0, sizeof(Config));
			if ( bUrl ) {
				Config.Flags |= (uint32)XBASE64_URL;
			}
			if ( !bPadding ) {
				Config.Flags |= (uint32)XBASE64_NO_PADDING;
			}
			iExpectedSize = testEncode(arrInput, iInputSize,
				arrExpected, bUrl, bPadding);
			testRequire(xrtBase64Encode(arrInput, iInputSize,
				arrEncoded, sizeof(arrEncoded), &iEncodedSize, &Config),
				"random Base64 encode failed");
			testRequire((iEncodedSize == iExpectedSize) &&
				(memcmp(arrEncoded, arrExpected, iExpectedSize + 1u) == 0),
				"random Base64 reference mismatch");
			testRequire(xrtBase64Decode(arrEncoded, iEncodedSize,
				arrOutput, sizeof(arrOutput), &iOutputSize, &Config),
				"random Base64 decode failed");
			testRequire((iOutputSize == iInputSize) &&
				(memcmp(arrOutput, arrInput, iInputSize) == 0),
				"random Base64 round-trip mismatch");
		}
	}
	printf("[PASS] codec-base64-property\n");
	return 0;
}
