#include "../test.h"



/* 比较并释放路径模块返回的拥有字符串。 */
static void testPathText(str sActual, cstr sExpected, cstr sMessage)
{
	testRequire(sActual != NULL, sMessage);
	testRequire(strcmp(sActual, sExpected) == 0, sMessage);
	xrtFree(sActual);
}



/* 路径分解必须明确覆盖两套平台根和现代扩展名语义。 */
static void testPathParse(void)
{
	xpathparts Parts;
	xpathparts Saved;
	char arrInvalid[] = { 'a', 0, 'b' };

	testRequire(xrtPathParse(XRT_STR_LITERAL("/home/user/archive.tar.gz/"),
		XPATH_POSIX, &Parts), "POSIX path parse failed");
	testRequire((Parts.RootKind == XPATH_ROOT_POSIX) &&
		((Parts.Flags & XPATH_FLAG_ABSOLUTE) != 0) &&
		((Parts.Flags & XPATH_FLAG_TRAILING_SEPARATOR) != 0),
		"POSIX root flags are incorrect");
	testRequire(xrtStrEqual(Parts.Parent, XRT_STR_LITERAL("/home/user")) &&
		xrtStrEqual(Parts.Name, XRT_STR_LITERAL("archive.tar.gz")) &&
		xrtStrEqual(Parts.Stem, XRT_STR_LITERAL("archive.tar")) &&
		xrtStrEqual(Parts.Ext, XRT_STR_LITERAL(".gz")),
		"POSIX path parts are incorrect");

	testRequire(xrtPathParse(XRT_STR_LITERAL(".gitignore"),
		XPATH_POSIX, &Parts), "hidden file parse failed");
	testRequire(xrtStrEqual(Parts.Stem, XRT_STR_LITERAL(".gitignore")) &&
		xrtStrEmpty(Parts.Ext), "hidden file was treated as an extension");

	testRequire(xrtPathParse(XRT_STR_LITERAL("C:\\work\\file.txt"),
		XPATH_WINDOWS, &Parts), "Windows drive path parse failed");
	testRequire((Parts.RootKind == XPATH_ROOT_DRIVE) &&
		((Parts.Flags & XPATH_FLAG_ABSOLUTE) != 0) &&
		xrtStrEqual(Parts.Root, XRT_STR_LITERAL("C:\\")),
		"Windows drive root is incorrect");

	testRequire(xrtPathParse(XRT_STR_LITERAL("C:work\\file.txt"),
		XPATH_WINDOWS, &Parts), "Windows drive-relative parse failed");
	testRequire((Parts.RootKind == XPATH_ROOT_DRIVE_RELATIVE) &&
		((Parts.Flags & XPATH_FLAG_ROOTED) != 0) &&
		((Parts.Flags & XPATH_FLAG_ABSOLUTE) == 0),
		"drive-relative path was treated as absolute");

	testRequire(xrtPathParse(XRT_STR_LITERAL("\\work\\file.txt"),
		XPATH_WINDOWS, &Parts), "Windows root-relative parse failed");
	testRequire((Parts.RootKind == XPATH_ROOT_WINDOWS) &&
		((Parts.Flags & XPATH_FLAG_ROOTED) != 0) &&
		((Parts.Flags & XPATH_FLAG_ABSOLUTE) == 0),
		"root-relative path was treated as absolute");

	testRequire(xrtPathParse(XRT_STR_LITERAL("\\\\server\\share\\dir\\file"),
		XPATH_WINDOWS, &Parts), "UNC path parse failed");
	testRequire((Parts.RootKind == XPATH_ROOT_UNC) &&
		((Parts.Flags & XPATH_FLAG_ABSOLUTE) != 0) &&
		xrtStrEqual(Parts.Root, XRT_STR_LITERAL("\\\\server\\share\\")),
		"UNC root is incomplete");

	memset(&Parts, 0xA5, sizeof(Parts));
	Saved = Parts;
	testRequire(!xrtPathParse((xstrview){ arrInvalid, sizeof(arrInvalid) },
		XPATH_POSIX, &Parts), "embedded null path was accepted");
	testRequire(memcmp(&Parts, &Saved, sizeof(Parts)) == 0,
		"failed path parse modified the output");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XPATH_ERROR_FORMAT),
		"embedded null path reported the wrong error");
}



/* 组件迭代器必须零分配地保留根、点段和普通名称边界。 */
static void testPathIter(void)
{
	xpathiter Iterator;
	xpathiter Saved;
	xpathcomponent Component;
	char arrInvalid[] = { 'a', 0, 'b' };

	testRequire(xrtPathIterInit(&Iterator,
		XRT_STR_LITERAL("C:\\alpha\\.\\..\\beta"), XPATH_WINDOWS),
		"path iterator initialization failed");
	testRequire(xrtPathNext(&Iterator, &Component) &&
		(Component.Kind == XPATH_COMPONENT_ROOT) &&
		xrtStrEqual(Component.Text, XRT_STR_LITERAL("C:\\")),
		"path iterator drive root mismatch");
	testRequire(xrtPathNext(&Iterator, &Component) &&
		(Component.Kind == XPATH_COMPONENT_NORMAL) &&
		xrtStrEqual(Component.Text, XRT_STR_LITERAL("alpha")),
		"path iterator first name mismatch");
	testRequire(xrtPathNext(&Iterator, &Component) &&
		(Component.Kind == XPATH_COMPONENT_CURRENT),
		"path iterator current component mismatch");
	testRequire(xrtPathNext(&Iterator, &Component) &&
		(Component.Kind == XPATH_COMPONENT_PARENT),
		"path iterator parent component mismatch");
	testRequire(xrtPathNext(&Iterator, &Component) &&
		(Component.Kind == XPATH_COMPONENT_NORMAL) &&
		xrtStrEqual(Component.Text, XRT_STR_LITERAL("beta")),
		"path iterator final name mismatch");
	testRequire(!xrtPathNext(&Iterator, &Component) &&
		xrtStrEmpty(Component.Text),
		"path iterator did not stop cleanly");

	memset(&Iterator, 0xA5, sizeof(Iterator));
	Saved = Iterator;
	testRequire(!xrtPathIterInit(&Iterator,
		(xstrview){ arrInvalid, sizeof(arrInvalid) }, XPATH_POSIX),
		"path iterator accepted an embedded null");
	testRequire(memcmp(&Iterator, &Saved, sizeof(Iterator)) == 0,
		"failed path iterator initialization modified the output");
	xrtClearError();
	memset(&Component, 0xA5, sizeof(Component));
	testRequire(!xrtPathNext(&Iterator, &Component) &&
		xrtStrEmpty(Component.Text),
		"invalid path iterator state was accepted");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"invalid path iterator reported the wrong error");
}



/* 常用拥有字符串 Helper 必须保持简短且没有空字符串哨兵所有权。 */
static void testPathParts(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		cstr sRenamed = "alpha\\next.bin";
	#else
		cstr sRenamed = "alpha/next.bin";
	#endif

	testPathText(xrtPathName("alpha/beta.tar.gz"), "beta.tar.gz",
		"path name mismatch");
	testPathText(xrtPathStem("alpha/beta.tar.gz"), "beta.tar",
		"path stem mismatch");
	testPathText(xrtPathExt("alpha/beta.tar.gz"), ".gz",
		"path extension mismatch");
	testPathText(xrtPathParent("alpha/beta.tar.gz"), "alpha",
		"path parent mismatch");
	testPathText(xrtPathExt(".gitignore"), "",
		"hidden file extension mismatch");
	testPathText(xrtPathParent("file.txt"), "",
		"parentless path did not return an owned empty string");
	#if defined(_WIN32) || defined(_WIN64)
		testRequire(xrtPathIsRoot("C:\\") && !xrtPathIsRoot("C:") &&
			!xrtPathIsRoot("C:\\work"), "Windows root predicate is incorrect");
	#else
		testRequire(xrtPathIsRoot("/") && !xrtPathIsRoot("/work") &&
			!xrtPathIsRoot("."), "POSIX root predicate is incorrect");
	#endif

	testPathText(xrtPathWithExt("alpha/archive.tar.gz", "zip"),
		"alpha/archive.tar.zip", "extension replacement failed");
	testPathText(xrtPathWithExt("alpha/.gitignore", ".txt"),
		"alpha/.gitignore.txt", "hidden file extension append failed");
	testPathText(xrtPathWithExt("alpha/file.txt", ""),
		"alpha/file", "extension removal failed");
	testPathText(xrtPathWithName("alpha/file.txt", "next.bin"),
		sRenamed, "name replacement failed");
}



/* 本地路径判断必须允许可归约路径并拒绝逃逸、卷和设备名称。 */
static void testPathLocal(void)
{
	testRequire(xrtPathIsLocal(XRT_STR_LITERAL("assets/a/../icon.png"),
		XPATH_POSIX), "local POSIX path was rejected");
	testRequire(xrtPathIsLocal(XRT_STR_LITERAL("."), XPATH_POSIX),
		"current directory was not local");
	testRequire(!xrtPathIsLocal(XRT_STR_LITERAL("../secret"), XPATH_POSIX),
		"leading parent path was local");
	testRequire(!xrtPathIsLocal(XRT_STR_LITERAL("a/../../secret"), XPATH_POSIX),
		"nested parent escape was local");
	testRequire(!xrtPathIsLocal(XRT_STR_LITERAL("/absolute"), XPATH_POSIX),
		"absolute POSIX path was local");
	testRequire(xrtPathIsLocal(XRT_STR_LITERAL("C:name"), XPATH_POSIX),
		"POSIX colon name was treated as a Windows volume");

	testRequire(xrtPathIsLocal(XRT_STR_LITERAL("assets\\a\\..\\icon.png"),
		XPATH_WINDOWS), "local Windows path was rejected");
	testRequire(!xrtPathIsLocal(XRT_STR_LITERAL("C:name"), XPATH_WINDOWS),
		"drive-relative path was local");
	testRequire(!xrtPathIsLocal(XRT_STR_LITERAL("\\root"), XPATH_WINDOWS),
		"root-relative Windows path was local");
	testRequire(!xrtPathIsLocal(XRT_STR_LITERAL("assets\\NUL.txt"),
		XPATH_WINDOWS), "Windows device name was local");
	testRequire(!xrtPathIsLocal(XRT_STR_LITERAL("assets\\file:stream"),
		XPATH_WINDOWS), "Windows alternate data stream was local");
}



/* 清理必须覆盖点段、越根、UNC 和设备命名空间。 */
static void testPathClean(void)
{
	testPathText(xrtPathClean(
		XRT_STR_LITERAL("//alpha///beta/./gamma/../delta/.."), XPATH_POSIX),
		"/alpha/beta", "POSIX path clean failed");
	testPathText(xrtPathClean(XRT_STR_LITERAL("../../a/../b"), XPATH_POSIX),
		"../../b", "relative double-dot clean failed");
	testPathText(xrtPathClean(XRT_STR_LITERAL("/../../a"), XPATH_POSIX),
		"/a", "absolute path escaped its root");
	testPathText(xrtPathClean(
		XRT_STR_LITERAL("C:/alpha//beta/../file.txt"), XPATH_WINDOWS),
		"C:\\alpha\\file.txt", "Windows drive clean failed");
	testPathText(xrtPathClean(
		XRT_STR_LITERAL("\\\\server\\share\\a\\..\\b"), XPATH_WINDOWS),
		"\\\\server\\share\\b", "UNC clean failed");
	testPathText(xrtPathClean(
		XRT_STR_LITERAL("\\\\?\\C:\\alpha\\..\\file"), XPATH_WINDOWS),
		"\\\\?\\C:\\alpha\\..\\file",
		"Windows device path was normalized destructively");
	testPathText(xrtPathClean((xstrview){ NULL, 0 }, XPATH_POSIX),
		".", "empty path did not clean to current directory");
	testPathText(xrtPathClean(XRT_STR_LITERAL("C:..\\a"), XPATH_WINDOWS),
		"C:..\\a", "drive-relative parent path was corrupted");
}



/* 拼接必须移除旧版 4094 字节限制并明确带根项的替换语义。 */
static void testPathBuild(void)
{
	char* sLong = (char*)xrtMalloc(5001);
	str sResult;
	xstrview arrParts[3];

	testRequire(sLong != NULL, "long path setup allocation failed");
	memset(sLong, 'a', 5000);
	sLong[5000] = 0;
	sResult = xrtPathJoin(sLong, "b");
	testRequire((sResult != NULL) && (strlen(sResult) == 5002),
		"path join retained the old fixed length limit");
	xrtFree(sResult);
	xrtFree(sLong);

	arrParts[0] = XRT_STR_LITERAL("alpha");
	arrParts[1] = XRT_STR_LITERAL("beta");
	arrParts[2] = XRT_STR_LITERAL("../gamma");
	testPathText(xrtPathBuild(arrParts, 3, XPATH_POSIX),
		"alpha/gamma", "multi-part path build failed");

	arrParts[0] = XRT_STR_LITERAL("discarded");
	arrParts[1] = XRT_STR_LITERAL("/root");
	arrParts[2] = XRT_STR_LITERAL("child");
	testPathText(xrtPathBuild(arrParts, 3, XPATH_POSIX),
		"/root/child", "rooted path did not replace earlier parts");
	testPathText(xrtPathBuild(NULL, 0, XPATH_POSIX),
		".", "empty path build did not return current directory");

	arrParts[0] = XRT_STR_LITERAL("C:");
	arrParts[1] = XRT_STR_LITERAL("child");
	testPathText(xrtPathBuild(arrParts, 2, XPATH_WINDOWS),
		"C:child", "drive-relative prefix became absolute");
	arrParts[0] = XRT_STR_LITERAL("C:\\base\\old");
	arrParts[1] = XRT_STR_LITERAL("\\root\\child");
	testPathText(xrtPathBuild(arrParts, 2, XPATH_WINDOWS),
		"C:\\root\\child", "root-relative path lost its drive prefix");
	arrParts[0] = XRT_STR_LITERAL("\\\\server\\share\\old");
	arrParts[1] = XRT_STR_LITERAL("\\root\\child");
	testPathText(xrtPathBuild(arrParts, 2, XPATH_WINDOWS),
		"\\\\server\\share\\root\\child",
		"root-relative path lost its UNC volume");
}



/* 相对路径必须按目录语义工作，并拒绝不可表达的跨根结果。 */
static void testPathRelative(void)
{
	str sResult;

	testPathText(xrtPathRelative(XRT_STR_LITERAL("/a/b"),
		XRT_STR_LITERAL("/a/c/d"), XPATH_POSIX),
		"../c/d", "POSIX relative path failed");
	testPathText(xrtPathRelative(XRT_STR_LITERAL("."),
		XRT_STR_LITERAL("a"), XPATH_POSIX),
		"a", "current-directory relative path failed");
	testPathText(xrtPathRelative(XRT_STR_LITERAL("same/path"),
		XRT_STR_LITERAL("same/path"), XPATH_POSIX),
		".", "equal path relative result failed");
	testPathText(xrtPathRelative(XRT_STR_LITERAL("/"),
		XRT_STR_LITERAL("/a/b"), XPATH_POSIX),
		"a/b", "root-relative result failed");
	testPathText(xrtPathRelative(XRT_STR_LITERAL("C:\\Work\\Source"),
		XRT_STR_LITERAL("c:\\work\\source\\Child"), XPATH_WINDOWS),
		"Child", "Windows relative path compared segments case-sensitively");

	xrtClearError();
	sResult = xrtPathRelative(XRT_STR_LITERAL("C:\\a"),
		XRT_STR_LITERAL("D:\\b"), XPATH_WINDOWS);
	testRequire(sResult == NULL, "different Windows roots were accepted");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XPATH_ERROR_ROOT),
		"different roots reported the wrong error");

	testPathText(xrtPathRelative(XRT_STR_LITERAL("../a"),
		XRT_STR_LITERAL("../../b"), XPATH_POSIX),
		"../../b", "expressible unresolved-parent relative path failed");
	xrtClearError();
	sResult = xrtPathRelative(XRT_STR_LITERAL("../../a"),
		XRT_STR_LITERAL("../b"), XPATH_POSIX);
	testRequire(sResult == NULL,
		"unresolved-parent base produced an invalid relative path");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XPATH_ERROR_ROOT),
		"unresolved-parent relative failure reported the wrong error");

	xrtClearError();
	sResult = xrtPathRelative(XRT_STR_LITERAL("\\\\?\\C:\\a"),
		XRT_STR_LITERAL("\\\\?\\C:\\b"), XPATH_WINDOWS);
	testRequire(sResult == NULL,
		"device namespace paths produced an ordinary relative path");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
		"device namespace relative failure reported the wrong error");
}



/* 执行路径词法契约测试。 */
int main(void)
{
	testPathParse();
	testPathIter();
	testPathParts();
	testPathLocal();
	testPathClean();
	testPathBuild();
	testPathRelative();
	return 0;
}
