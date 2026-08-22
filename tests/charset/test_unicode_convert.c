#include "../test.h"



/* 六个转换方向必须在 ASCII、汉字、补充平面和嵌入零上往返。 */
static void testRoundTrips(void)
{
	static const char arrSource[] = {
		'A', 0, (char)0xE4, (char)0xBD, (char)0xA0,
		(char)0xF0, (char)0x9F, (char)0x98, (char)0x80
	};
	static const uint16 arrExpected16[] = {
		0x0041u, 0x0000u, 0x4F60u, 0xD83Du, 0xDE00u
	};
	static const uint32 arrExpected32[] = {
		0x00000041u, 0x00000000u, 0x00004F60u, 0x0001F600u
	};
	xstrview Source = { arrSource, sizeof(arrSource) };
	uint16* pUtf16;
	uint32* pUtf32;
	uint32* pFrom16;
	uint16* pFrom32;
	str sFrom16;
	str sFrom32;
	size_t iSize = 0;

	pUtf16 = xrtUtf8ViewTo16(Source, XUTF_STRICT, &iSize);
	testRequire((pUtf16 != NULL) && (iSize == 5), "UTF-8 to UTF-16 failed");
	testRequire(memcmp(pUtf16, arrExpected16, sizeof(arrExpected16)) == 0,
		"UTF-8 to UTF-16 output is wrong");
	pUtf32 = xrtUtf8ViewTo32(Source, XUTF_STRICT, &iSize);
	testRequire((pUtf32 != NULL) && (iSize == 4), "UTF-8 to UTF-32 failed");
	testRequire(memcmp(pUtf32, arrExpected32, sizeof(arrExpected32)) == 0,
		"UTF-8 to UTF-32 output is wrong");

	/* 其余四个方向复用刚得到的严格有效输入。 */
	sFrom16 = xrtUtf16ViewTo8(xrtUtf16View(pUtf16, 5), XUTF_STRICT, &iSize);
	testRequire((sFrom16 != NULL) && (iSize == sizeof(arrSource)),
		"UTF-16 to UTF-8 failed");
	testRequire(memcmp(sFrom16, arrSource, sizeof(arrSource)) == 0,
		"UTF-16 round trip changed bytes");
	sFrom32 = xrtUtf32ViewTo8(xrtUtf32View(pUtf32, 4), XUTF_STRICT, &iSize);
	testRequire((sFrom32 != NULL) && (iSize == sizeof(arrSource)),
		"UTF-32 to UTF-8 failed");
	pFrom16 = xrtUtf16ViewTo32(xrtUtf16View(pUtf16, 5), XUTF_STRICT, &iSize);
	testRequire((pFrom16 != NULL) && (iSize == 4), "UTF-16 to UTF-32 failed");
	pFrom32 = xrtUtf32ViewTo16(xrtUtf32View(pUtf32, 4), XUTF_STRICT, &iSize);
	testRequire((pFrom32 != NULL) && (iSize == 5), "UTF-32 to UTF-16 failed");

	xrtFree(pFrom32);
	xrtFree(pFrom16);
	xrtFree(sFrom32);
	xrtFree(sFrom16);
	xrtFree(pUtf32);
	xrtFree(pUtf16);
}



/* 计量和容量不足必须返回可恢复的精确进度。 */
static void testBuffers(void)
{
	static const char sSource[] = "A\xF0\x9F\x98\x80";
	uint16 arrOutput[2];
	xutfresult Result;

	Result = xrtUtf8To16Buffer((xstrview){ sSource, 5 }, NULL, 0, XUTF_STRICT);
	testRequire((Result.Status == XUTF_OK) && (Result.Read == 5) &&
		(Result.Written == 3), "UTF buffer measurement failed");
	Result = xrtUtf8To16Buffer((xstrview){ sSource, 5 }, arrOutput, 2, XUTF_STRICT);
	testRequire((Result.Status == XUTF_NO_SPACE) && (Result.Read == 1) &&
		(Result.Written == 1), "UTF buffer partial progress is wrong");
}



/* 替换模式按最大子部件前进，严格模式保留错误位置和结构化错误。 */
static void testInvalidPolicies(void)
{
	static const char arrInvalid[] = { (char)0xE1, (char)0x80, 'A' };
	static const uint16 arrExpected[] = { 0xFFFDu, 0x0041u };
	uint16 arrOutput[4];
	xutfresult Result;
	const xerror* pError;

	xrtClearError();
	Result = xrtUtf8To16Buffer((xstrview){ arrInvalid, sizeof(arrInvalid) },
		arrOutput, 4, XUTF_STRICT);
	testRequire((Result.Status == XUTF_INVALID) && (Result.Error == 0),
		"strict conversion did not stop at invalid input");
	pError = xrtGetError();
	testRequire((pError != NULL) && (xrtErrorKind(pError) == XERR_VALUE) &&
		(strcmp(xrtErrorDomain(pError), "xrt.unicode") == 0),
		"strict conversion did not publish Unicode error");

	Result = xrtUtf8To16Buffer((xstrview){ arrInvalid, sizeof(arrInvalid) },
		arrOutput, 4, XUTF_REPLACE);
	testRequire((Result.Status == XUTF_OK) && (Result.Written == 2),
		"replacement conversion failed");
	testRequire(memcmp(arrOutput, arrExpected, sizeof(arrExpected)) == 0,
		"replacement did not consume maximal subpart");
}



/* 空转换仍返回独立、可释放且完整补零的所有权对象。 */
static void testOwnedEmpty(void)
{
	uint16* pUtf16 = xrtUtf8ViewTo16((xstrview){ NULL, 0 }, XUTF_STRICT, NULL);
	uint32* pUtf32 = xrtUtf8ViewTo32((xstrview){ NULL, 0 }, XUTF_STRICT, NULL);

	testRequire((pUtf16 != NULL) && (pUtf16[0] == 0),
		"owned empty UTF-16 is invalid");
	testRequire((pUtf32 != NULL) && (pUtf32[0] == 0),
		"owned empty UTF-32 is invalid");
	xrtFree(pUtf32);
	xrtFree(pUtf16);
}



/* 验证缓冲区转换和分配便捷层拒绝输入输出别名及单位溢出。 */
static void testConversionAliasing(void)
{
	union {
		uint64 Align;
		unsigned char Bytes[64];
	} Memory;
	unsigned char arrBefore[sizeof(Memory.Bytes)];
	uint32 arrTarget[1] = { UINT32_C(0xA5A5A5A5) };
	xutfresult Result;

	memset(&Memory, 0, sizeof(Memory));
	memcpy(Memory.Bytes, "abc", 3);
	memcpy(arrBefore, Memory.Bytes, sizeof(arrBefore));
	xrtClearError();
	Result = xrtUtf8To16Buffer(
		(xstrview){ (cstr)Memory.Bytes, 3 },
		(uint16*)(void*)Memory.Bytes, 16, XUTF_STRICT
	);
	testRequire((Result.Status == XUTF_INVALID) &&
		(memcmp(Memory.Bytes, arrBefore, sizeof(arrBefore)) == 0),
		"UTF buffer conversion accepted overlapping storage");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"UTF buffer overlap error mismatch");

	xrtClearError();
	testRequire(xrtUtf8ViewTo16(
		(xstrview){ (cstr)Memory.Bytes, 3 }, XUTF_STRICT,
		(size_t*)(void*)Memory.Bytes
	) == NULL, "UTF allocated conversion accepted an aliased size output");
	testRequire(memcmp(Memory.Bytes, arrBefore, sizeof(arrBefore)) == 0,
		"UTF allocated alias failure modified input");

	xrtClearError();
	Result = xrtUtf8To32Buffer(
		XRT_STR_LITERAL("A"), arrTarget, SIZE_MAX, XUTF_STRICT
	);
	testRequire((Result.Status == XUTF_OVERFLOW) &&
		(arrTarget[0] == UINT32_C(0xA5A5A5A5)) &&
		(xrtErrorKind(xrtGetError()) == XERR_RANGE),
		"UTF target-unit overflow contract mismatch");
}



/* 执行 Unicode 六向转换和错误策略测试。 */
int main(void)
{
	testRoundTrips();
	testBuffers();
	testInvalidPolicies();
	testOwnedEmpty();
	testConversionAliasing();
	return 0;
}
