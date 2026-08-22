#include "../test.h"



/* 安全条目必须在 Windows 与 POSIX 上得到相同结论。 */
int main(void)
{
	static const char sInvalidUtf8[] = { (char)0xC0, (char)0xAF };
	static const char sEmbeddedNull[] = { 'a', 0, 'b' };
	static const cstr arrDevice[] = {
		"CON", "prn.txt", "AUX.tar.gz", "nul ",
		"COM1.log", "com9", "LPT1.bin", "lpt9",
		"CONIN$", "conout$.txt",
		"COM\xC2\xB9.txt", "LPT\xC2\xB2", "com\xC2\xB3.log"
	};
	static const cstr arrForbidden[] = {
		"bad<name", "bad>name", "bad:name", "bad\"name",
		"bad|name", "bad?name", "bad*name"
	};
	xpathsafesegment Segment;

	/* 正常文件、目录和 Unicode 名称必须保持可用。 */
	testRequire(xrtPathIsSafeEntry(XRT_STR_LITERAL("assets/icons/app.png"), false),
		"valid portable file entry was rejected");
	testRequire(xrtPathIsSafeEntry(XRT_STR_LITERAL("assets/icons/"), true),
		"valid portable directory entry was rejected");
	testRequire(xrtPathIsSafeEntry(XRT_STR_LITERAL("data/\xE6\x96\x87\xE4\xBB\xB6.txt"), false),
		"valid Unicode entry was rejected");
	testRequire(xrtPathIsSafeEntry(XRT_STR_LITERAL("COM0.txt"), false),
		"ordinary name resembling a device was rejected");

	/* 根、空段、点段和平台专用路径语法必须被拒绝。 */
	testRequire(!xrtPathIsSafeEntry(XRT_STR_LITERAL("assets/icons/"), false),
		"directory syntax was accepted as a file entry");
	testRequire(!xrtPathIsSafeEntry(XRT_STR_LITERAL("assets/icons//"), true),
		"double directory suffix was accepted");
	testRequire(!xrtPathIsSafeEntry(XRT_STR_LITERAL("../secret"), false),
		"parent traversal entry was accepted");
	testRequire(!xrtPathIsSafeEntry(XRT_STR_LITERAL("assets/./icon"), false),
		"current-directory path segment was accepted");
	testRequire(!xrtPathIsSafeEntry(XRT_STR_LITERAL("a//b"), false),
		"empty path segment was accepted");
	testRequire(!xrtPathIsSafeEntry(XRT_STR_LITERAL("C:/boot.ini"), false),
		"drive path was accepted");
	testRequire(!xrtPathIsSafeEntry(XRT_STR_LITERAL("a\\b"), false),
		"backslash path was accepted");

	/* Windows 禁用字符和全部公开设备名形式必须统一拒绝。 */
	for ( size_t i = 0; i < sizeof(arrForbidden) / sizeof(arrForbidden[0]); i++ ) {
		testRequire(!xrtPathIsSafeEntry(xrtStrView(arrForbidden[i]), false),
			"Windows forbidden character was accepted");
	}
	for ( size_t i = 0; i < sizeof(arrDevice) / sizeof(arrDevice[0]); i++ ) {
		testRequire(!xrtPathIsSafeEntry(xrtStrView(arrDevice[i]), false),
			"Windows device name was accepted");
	}
	testRequire(!xrtPathIsSafeEntry(XRT_STR_LITERAL("trailing. "), false),
		"trailing space was accepted");

	/* 非法编码和嵌入零字节不能穿过安全入口。 */
	testRequire(!xrtPathIsSafeEntry(
		(xstrview){ sInvalidUtf8, sizeof(sInvalidUtf8) }, false),
		"invalid UTF-8 entry was accepted");
	testRequire(!xrtPathIsSafeEntry(
		(xstrview){ sEmbeddedNull, sizeof(sEmbeddedNull) }, false),
		"embedded null entry was accepted");

	/* 流式段检查必须与完整路径检查共享设备名和非法字节规则。 */
	xrtPathSafeSegmentInit(&Segment);
	for ( size_t i = 0; i < sizeof("file.txt") - 1u; i++ ) {
		testRequire(xrtPathSafeSegmentFeed(
			&Segment,
			(uint8)"file.txt"[i]
		), "valid streamed path segment was rejected");
	}
	testRequire(xrtPathSafeSegmentFinish(&Segment),
		"valid streamed path segment did not finish");

	xrtPathSafeSegmentInit(&Segment);
	for ( size_t i = 0; i < sizeof("CON.txt") - 1u; i++ ) {
		testRequire(xrtPathSafeSegmentFeed(
			&Segment,
			(uint8)"CON.txt"[i]
		), "device path segment failed before completion");
	}
	testRequire(!xrtPathSafeSegmentFinish(&Segment),
		"streamed Windows device name was accepted");

	xrtPathSafeSegmentInit(&Segment);
	testRequire(!xrtPathSafeSegmentFeed(&Segment, (uint8)'?'),
		"streamed forbidden path byte was accepted");
	return 0;
}
