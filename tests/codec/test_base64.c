#include "../test.h"



typedef struct testbase64vector {
	cstr Plain;
	cstr Encoded;
} testbase64vector;



/* 验证 RFC 4648 标准向量、查询模式和分配型便捷路径。 */
static void testBase64Vectors(void)
{
	static const testbase64vector Vectors[] = {
		{ "", "" },
		{ "f", "Zg==" },
		{ "fo", "Zm8=" },
		{ "foo", "Zm9v" },
		{ "foob", "Zm9vYg==" },
		{ "fooba", "Zm9vYmE=" },
		{ "foobar", "Zm9vYmFy" }
	};
	char Encoded[32];
	uint8 Decoded[16];

	for ( size_t i = 0; i < sizeof(Vectors) / sizeof(Vectors[0]); i++ ) {
		size_t iPlainSize = strlen(Vectors[i].Plain);
		size_t iEncodedSize = strlen(Vectors[i].Encoded);
		size_t iSize = SIZE_MAX;
		str sAllocated;
		bytes pAllocated;

		testRequire(xrtBase64Encode(
			Vectors[i].Plain, iPlainSize, NULL, 0, &iSize, NULL
		) && (iSize == iEncodedSize), "Base64 encode query mismatch");
		memset(Encoded, 0xA5, sizeof(Encoded));
		testRequire(xrtBase64Encode(
			Vectors[i].Plain, iPlainSize, Encoded, sizeof(Encoded), &iSize, NULL
		) && (iSize == iEncodedSize) &&
			(strcmp(Encoded, Vectors[i].Encoded) == 0),
			"Base64 RFC encode vector mismatch");

		iSize = SIZE_MAX;
		testRequire(xrtBase64Decode(
			Vectors[i].Encoded, iEncodedSize, NULL, 0, &iSize, NULL
		) && (iSize == iPlainSize), "Base64 decode query mismatch");
		memset(Decoded, 0xA5, sizeof(Decoded));
		testRequire(xrtBase64Decode(
			Vectors[i].Encoded, iEncodedSize, Decoded, sizeof(Decoded), &iSize, NULL
		) && (iSize == iPlainSize) &&
			(memcmp(Decoded, Vectors[i].Plain, iPlainSize) == 0),
			"Base64 RFC decode vector mismatch");

		sAllocated = xrtBase64EncodeNew(Vectors[i].Plain, iPlainSize, NULL);
		testRequire((sAllocated != NULL) &&
			(strcmp(sAllocated, Vectors[i].Encoded) == 0),
			"Base64 allocated encode mismatch");
		xrtFree(sAllocated);
		pAllocated = xrtBase64DecodeNew(
			Vectors[i].Encoded, iEncodedSize, &iSize, NULL
		);
		testRequire((pAllocated != NULL) && (iSize == iPlainSize) &&
			(memcmp(pAllocated, Vectors[i].Plain, iPlainSize) == 0) &&
			(pAllocated[iSize] == 0), "Base64 allocated decode mismatch");
		xrtFree(pAllocated);
	}
}



/* 验证 URL-safe、无填充、自定义字母表和 PEM 空白路径。 */
static void testBase64Modes(void)
{
	static const uint8 Binary[] = { 0xFB, 0xFF, 0xEF };
	static const char CustomAlphabet[] =
		"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
	xbase64config Config;
	char Text[32];
	uint8 Output[16];
	size_t iSize;

	memset(&Config, 0, sizeof(Config));
	testRequire(xrtBase64Encode(
		Binary, sizeof(Binary), Text, sizeof(Text), &iSize, &Config
	) && (strcmp(Text, "+//v") == 0), "Base64 standard alphabet mismatch");

	Config.Flags = (uint32)XBASE64_URL;
	testRequire(xrtBase64Encode(
		Binary, sizeof(Binary), Text, sizeof(Text), &iSize, &Config
	) && (strcmp(Text, "-__v") == 0), "Base64 URL alphabet mismatch");
	testRequire(xrtBase64Decode(
		Text, iSize, Output, sizeof(Output), &iSize, &Config
	) && (iSize == sizeof(Binary)) &&
		(memcmp(Output, Binary, sizeof(Binary)) == 0),
		"Base64 URL round trip mismatch");

	Config.Flags = (uint32)XBASE64_NO_PADDING;
	testRequire(xrtBase64Encode(
		"f", 1, Text, sizeof(Text), &iSize, &Config
	) && (strcmp(Text, "Zg") == 0), "Base64 raw encode mismatch");
	testRequire(xrtBase64Decode(
		Text, iSize, Output, sizeof(Output), &iSize, &Config
	) && (iSize == 1) && (Output[0] == (uint8)'f'),
		"Base64 raw decode mismatch");

	Config.Flags = (uint32)XBASE64_OPTIONAL_PADDING;
	testRequire(xrtBase64Decode(
		"Zg", 2, Output, sizeof(Output), &iSize, &Config
	) && (iSize == 1) && (Output[0] == (uint8)'f'),
		"Base64 optional mode rejected missing padding");
	testRequire(xrtBase64Decode(
		"Zg=", 3, Output, sizeof(Output), &iSize, &Config
	) && (iSize == 1) && (Output[0] == (uint8)'f'),
		"Base64 optional mode rejected partial padding");
	testRequire(xrtBase64Decode(
		"Zg==", 4, Output, sizeof(Output), &iSize, &Config
	) && (iSize == 1) && (Output[0] == (uint8)'f'),
		"Base64 optional mode rejected full padding");

	memset(&Config, 0, sizeof(Config));
	Config.Alphabet = CustomAlphabet;
	testRequire(xrtBase64Encode(
		Binary, sizeof(Binary), Text, sizeof(Text), &iSize, &Config
	) && (strcmp(Text, "-__l") == 0), "Base64 custom alphabet mismatch");
	testRequire(xrtBase64Decode(
		Text, iSize, Output, sizeof(Output), &iSize, &Config
	) && (iSize == sizeof(Binary)) &&
		(memcmp(Output, Binary, sizeof(Binary)) == 0),
		"Base64 custom alphabet round trip mismatch");

	memset(&Config, 0, sizeof(Config));
	Config.Flags = (uint32)XBASE64_IGNORE_SPACE;
	testRequire(xrtBase64Decode(
		" \vZm\t9v\f\r\n", sizeof(" \vZm\t9v\f\r\n") - 1u,
		Output, sizeof(Output), &iSize, &Config
	) && (iSize == 3) && (memcmp(Output, "foo", 3) == 0),
		"Base64 whitespace decode mismatch");
}



/* 验证编码和解码都支持最常用的同址原地操作。 */
static void testBase64InPlace(void)
{
	uint8 Buffer[32];
	size_t iSize;

	memcpy(Buffer, "foobar", 6);
	testRequire(xrtBase64Encode(
		Buffer, 6, (char*)Buffer, sizeof(Buffer), &iSize, NULL
	) && (iSize == 8) && (strcmp((char*)Buffer, "Zm9vYmFy") == 0),
		"Base64 in-place encode failed");
	testRequire(xrtBase64Decode(
		(char*)Buffer, iSize, Buffer, sizeof(Buffer), &iSize, NULL
	) && (iSize == 6) && (memcmp(Buffer, "foobar", 6) == 0),
		"Base64 in-place decode failed");
}



/* 验证旧实现曾接受的非规范填充、残余位和字符位置全部被拒绝。 */
static void testBase64RejectsMalformed(void)
{
	static const cstr Invalid[] = {
		"A===", "=m9v", "Zm=v", "Zm9v=", "Zm9v*===",
		"Zh==", "Zm9=", "Zg", "Zm9v\n"
	};
	uint8 Output[8];
	uint8 Before[8];

	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	for ( size_t i = 0; i < sizeof(Invalid) / sizeof(Invalid[0]); i++ ) {
		size_t iSize = 123;

		xrtClearError();
		testRequire(!xrtBase64Decode(
			Invalid[i], strlen(Invalid[i]), Output, sizeof(Output), &iSize, NULL
		) && (iSize == 123) && (memcmp(Output, Before, sizeof(Output)) == 0) &&
			(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL) &&
			(xrtErrorCode(xrtGetError()) == XCODEC_ERROR_BASE64_FORMAT),
			"Base64 malformed input contract mismatch");
	}

	{
		xbase64config Config = { NULL, (uint32)XBASE64_NO_PADDING };
		size_t iSize = 123;

		testRequire(!xrtBase64Decode(
			"Zg==", 4, Output, sizeof(Output), &iSize, &Config
		) && (iSize == 123), "Base64 raw mode accepted padding");
		testRequire(!xrtBase64Decode(
			"A", 1, Output, sizeof(Output), &iSize, &Config
		) && (iSize == 123), "Base64 raw mode accepted one-symbol tail");
	}
}



/* 验证配置、容量、重叠和长度溢出失败都不发布半个结果。 */
static void testBase64FailureAtomicity(void)
{
	static const char DuplicateAlphabet[] =
		"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	xbase64config Config;
	uint8 Buffer[32];
	uint8 Before[32];
	size_t iSize;

	memset(Buffer, 0xA5, sizeof(Buffer));
	memcpy(Before, Buffer, sizeof(Buffer));
	iSize = 99;
	testRequire(!xrtBase64Encode(
		"f", 1, (char*)Buffer, 4, &iSize, NULL
	) && (iSize == 4) && (memcmp(Buffer, Before, sizeof(Buffer)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"Base64 encode capacity failure was not atomic");

	iSize = 99;
	testRequire(!xrtBase64Decode(
		"Zm9v", 4, Buffer, 2, &iSize, NULL
	) && (iSize == 3) && (memcmp(Buffer, Before, sizeof(Buffer)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"Base64 decode capacity failure was not atomic");

	memset(&Config, 0, sizeof(Config));
	Config.Alphabet = DuplicateAlphabet;
	iSize = 99;
	testRequire(!xrtBase64Encode(
		"f", 1, (char*)Buffer, sizeof(Buffer), &iSize, &Config
	) && (iSize == 99) &&
		(xrtErrorCode(xrtGetError()) == XCODEC_ERROR_BASE64_CONFIG),
		"Base64 accepted a duplicate custom alphabet");

	Config.Alphabet = "short";
	testRequire(!xrtBase64Decode(
		"Zg==", 4, Buffer, sizeof(Buffer), &iSize, &Config
	), "Base64 accepted a short custom alphabet");

	Config.Alphabet = NULL;
	Config.Flags = UINT32_C(0x80000000);
	testRequire(!xrtBase64Encode(
		"f", 1, (char*)Buffer, sizeof(Buffer), &iSize, &Config
	), "Base64 accepted unknown configuration flags");

	Config.Flags = (uint32)XBASE64_IGNORE_SPACE;
	testRequire(!xrtBase64Encode(
		"f", 1, (char*)Buffer, sizeof(Buffer), &iSize, &Config
	), "Base64 encoder accepted a decode-only flag");

	Config.Flags = (uint32)XBASE64_NO_PADDING |
		(uint32)XBASE64_OPTIONAL_PADDING;
	testRequire(!xrtBase64Decode(
		"Zg", 2, Buffer, sizeof(Buffer), &iSize, &Config
	), "Base64 accepted conflicting padding modes");

	iSize = 99;
	testRequire(!xrtBase64Encode(
		(const void*)(uintptr_t)1u, SIZE_MAX - 1u,
		NULL, 0, &iSize, NULL
	) && (iSize == 99) && (xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"Base64 encoded-size overflow contract mismatch");

	iSize = 99;
	testRequire(!xrtBase64Encode(
		Buffer, 4, (char*)Buffer + 1, sizeof(Buffer) - 1u, &iSize, NULL
	) && (iSize == 99) && (memcmp(Buffer, Before, sizeof(Buffer)) == 0),
		"Base64 accepted a partially overlapping encode");
}




/* 验证输入、输出、配置和长度字段在地址回绕时不被读取。 */
static void testBase64WrappingRanges(void)
{
	const void* pWrapping =
		(const void*)(uintptr_t)(UINTPTR_MAX - 1u);
	size_t* pWrappingSize =
		(size_t*)(uintptr_t)(UINTPTR_MAX - 1u);
	const xbase64config* pWrappingConfig =
		(const xbase64config*)(uintptr_t)(UINTPTR_MAX - 1u);
	xbase64config Config = { (cstr)pWrapping, 0 };
	char Text[8];
	size_t iSize = 77u;

	testRequire(
		!xrtBase64Encode(pWrapping, 2u, NULL, 0, &iSize, NULL) &&
		(iSize == 77u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"Base64 accepted a wrapping encode input"
	);
	testRequire(
		!xrtBase64Encode("x", 1u, (char*)pWrapping, 2u, &iSize, NULL) &&
		(iSize == 77u),
		"Base64 accepted a wrapping encode output"
	);
	testRequire(
		!xrtBase64Encode("x", 1u, Text, sizeof(Text), pWrappingSize, NULL),
		"Base64 accepted a wrapping encode size output"
	);
	testRequire(
		!xrtBase64Decode(pWrapping, 2u, NULL, 0, &iSize, NULL) &&
		(iSize == 77u),
		"Base64 accepted a wrapping decode input"
	);
	testRequire(
		!xrtBase64Decode("AA==", 4u, (ptr)pWrapping, 2u, &iSize, NULL) &&
		(iSize == 77u),
		"Base64 accepted a wrapping decode output"
	);
	testRequire(
		!xrtBase64Decode(
			"AA==", 4u, Text, sizeof(Text), pWrappingSize, NULL
		),
		"Base64 accepted a wrapping decode size output"
	);
	testRequire(
		!xrtBase64Encode("x", 1u, NULL, 0, &iSize, pWrappingConfig) &&
		(iSize == 77u),
		"Base64 accepted a wrapping configuration"
	);
	testRequire(
		!xrtBase64Encode("x", 1u, NULL, 0, &iSize, &Config) &&
		(iSize == 77u),
		"Base64 accepted a wrapping custom alphabet"
	);
	testRequire(
		xrtBase64DecodeNew(pWrapping, 2u, &iSize, NULL) == NULL,
		"Base64 DecodeNew accepted a wrapping input"
	);
	testRequire(
		xrtBase64DecodeNew("AA==", 4u, pWrappingSize, NULL) == NULL,
		"Base64 DecodeNew accepted a wrapping size output"
	);
}



/* 验证短缓冲区之后的独立长度字段仍可接收精确需求。 */
static void testBase64AdjacentSize(void)
{
	typedef struct testbase64output {
		uint8 Buffer[4];
		size_t Size;
	} testbase64output;
	testbase64output Output;
	uint8 Before[sizeof(Output.Buffer)];
	static const char Encoded[] = "AAECAwQFBgcICQoL";
	static const uint8 Data[] = {
		0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u
	};

	memset(&Output, 0xA5, sizeof(Output));
	memcpy(Before, Output.Buffer, sizeof(Before));
	Output.Size = 77u;
	testRequire(
		!xrtBase64Encode(
			Data, sizeof(Data), (char*)Output.Buffer,
			sizeof(Output.Buffer), &Output.Size, NULL
		) && (Output.Size == 16u) &&
		(memcmp(Output.Buffer, Before, sizeof(Before)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"Base64 short encode rejected an adjacent size field"
	);

	memset(&Output, 0xA5, sizeof(Output));
	memcpy(Before, Output.Buffer, sizeof(Before));
	Output.Size = 77u;
	testRequire(
		!xrtBase64Decode(
			Encoded, sizeof(Encoded) - 1u, Output.Buffer,
			sizeof(Output.Buffer), &Output.Size, NULL
		) && (Output.Size == sizeof(Data)) &&
		(memcmp(Output.Buffer, Before, sizeof(Before)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"Base64 short decode rejected an adjacent size field"
	);
}



/* 执行 Base64 标准向量、模式、原地路径和严格边界测试。 */
int main(void)
{
	testBase64Vectors();
	testBase64Modes();
	testBase64InPlace();
	testBase64RejectsMalformed();
	testBase64FailureAtomicity();
	testBase64WrappingRanges();
	testBase64AdjacentSize();
	printf("[PASS] codec_base64\n");
	return 0;
}
