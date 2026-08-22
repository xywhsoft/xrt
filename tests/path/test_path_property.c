#include "../test.h"



/* 推进确定性伪随机状态，避免性质测试依赖随机模块。 */
static uint32 testPathRandom(uint32* pState)
{
	*pState = (*pState * UINT32_C(1664525)) + UINT32_C(1013904223);
	return *pState;
}



/* 在固定测试缓冲中追加一段文本。 */
static void testPathAppend(char* sPath, size_t iCapacity,
	size_t* pSize, cstr sText)
{
	size_t iSize = strlen(sText);

	testRequire(*pSize <= (iCapacity - iSize - 1u),
		"path property generator overflowed");
	memcpy(sPath + *pSize, sText, iSize);
	*pSize += iSize;
	sPath[*pSize] = 0;
}



/* 生成包含根、重复分隔符和点段的短路径。 */
static void testPathGenerate(char* sPath, size_t iCapacity,
	xpathstyle Style, uint32* pState)
{
	static const cstr arrSegment[] = {
		"a", "b", ".", "..", "file.txt", ".hidden", "x..y"
	};
	size_t iSize = 0;
	size_t iCount;

	sPath[0] = 0;
	if ( Style == XPATH_POSIX ) {
		if ( (testPathRandom(pState) & 3u) == 0u ) {
			testPathAppend(sPath, iCapacity, &iSize, "///");
		}
	} else {
		switch ( testPathRandom(pState) % 6u ) {
			case 0:
				testPathAppend(sPath, iCapacity, &iSize, "C:");
				break;
			case 1:
				testPathAppend(sPath, iCapacity, &iSize, "C:\\\\");
				break;
			case 2:
				testPathAppend(sPath, iCapacity, &iSize, "\\");
				break;
			case 3:
				testPathAppend(sPath, iCapacity, &iSize,
					"\\\\server\\share\\");
				break;
			default:
				break;
		}
	}

	iCount = testPathRandom(pState) % 8u;
	for ( size_t i = 0; i < iCount; i++ ) {
		cstr sSegment = arrSegment[
			testPathRandom(pState) %
			(sizeof(arrSegment) / sizeof(arrSegment[0]))];

		if ( (iSize != 0) &&
			 (sPath[iSize - 1] != '/') && (sPath[iSize - 1] != '\\') &&
			 !((Style == XPATH_WINDOWS) && (iSize == 2) &&
			   (sPath[1] == ':')) ) {
			testPathAppend(sPath, iCapacity, &iSize,
				(Style == XPATH_WINDOWS) &&
				((testPathRandom(pState) & 1u) != 0u) ? "\\\\" : "//");
		}
		testPathAppend(sPath, iCapacity, &iSize, sSegment);
	}
}



/* 生成只含普通名称的绝对路径，用于验证相对路径逆关系。 */
static void testPathAbsolute(char* sPath, size_t iCapacity,
	xpathstyle Style, uint32* pState)
{
	static const cstr arrSegment[] = { "a", "b", "c", "d", "file.txt" };
	size_t iSize = 0;
	size_t iCount = 1u + (testPathRandom(pState) % 6u);
	cstr sSeparator = Style == XPATH_WINDOWS ? "\\" : "/";

	sPath[0] = 0;
	testPathAppend(sPath, iCapacity, &iSize,
		Style == XPATH_WINDOWS ? "C:\\" : "/");
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( (iSize != 0) &&
			 (sPath[iSize - 1] != '/') && (sPath[iSize - 1] != '\\') ) {
			testPathAppend(sPath, iCapacity, &iSize, sSeparator);
		}
		testPathAppend(sPath, iCapacity, &iSize, arrSegment[
			testPathRandom(pState) %
			(sizeof(arrSegment) / sizeof(arrSegment[0]))]);
	}
}



/* 清理必须幂等，组件必须始终借用原输入边界。 */
static void testPathCleanProperty(xpathstyle Style, uint32* pState)
{
	char sPath[256];

	for ( size_t i = 0; i < 5000u; i++ ) {
		xpathiter Iterator;
		xpathcomponent Component;
		xstrview Path;
		str sFirst;
		str sSecond;
		size_t iComponents = 0;

		testPathGenerate(sPath, sizeof(sPath), Style, pState);
		Path = xrtStrView(sPath);
		sFirst = xrtPathClean(Path, Style);
		testRequire(sFirst != NULL, "path property clean failed");
		sSecond = xrtPathClean(xrtStrView(sFirst), Style);
		testRequire((sSecond != NULL) && (strcmp(sFirst, sSecond) == 0),
			"path clean is not idempotent");

		testRequire(xrtPathIterInit(&Iterator, Path, Style),
			"path property iterator initialization failed");
		while ( xrtPathNext(&Iterator, &Component) ) {
			testRequire((Component.Text.Size != 0) &&
				(Component.Text.Data >= Path.Data) &&
				(Component.Text.Data + Component.Text.Size <=
				 Path.Data + Path.Size),
				"path iterator returned a view outside the input");
			iComponents++;
			testRequire(iComponents <= (Path.Size + 1u),
				"path iterator did not make progress");
		}
		xrtFree(sFirst);
		xrtFree(sSecond);
	}
}



/* 相对路径与重新拼接必须在同根绝对路径上构成逆关系。 */
static void testPathRelativeProperty(xpathstyle Style, uint32* pState)
{
	char sBase[256];
	char sTarget[256];

	for ( size_t i = 0; i < 3000u; i++ ) {
		xstrview arrParts[2];
		str sRelative;
		str sJoined;
		str sCleanTarget;

		testPathAbsolute(sBase, sizeof(sBase), Style, pState);
		testPathAbsolute(sTarget, sizeof(sTarget), Style, pState);
		sRelative = xrtPathRelative(
			xrtStrView(sBase), xrtStrView(sTarget), Style);
		testRequire(sRelative != NULL,
			"path property relative conversion failed");
		arrParts[0] = xrtStrView(sBase);
		arrParts[1] = xrtStrView(sRelative);
		sJoined = xrtPathBuild(arrParts, 2, Style);
		sCleanTarget = xrtPathClean(xrtStrView(sTarget), Style);
		testRequire((sJoined != NULL) && (sCleanTarget != NULL) &&
			(strcmp(sJoined, sCleanTarget) == 0),
			"path relative round trip failed");
		xrtFree(sRelative);
		xrtFree(sJoined);
		xrtFree(sCleanTarget);
	}
}



/* 未知父目录深度只有在目标退得不少于基目录时才可纯词法表达。 */
static void testPathParentProperty(void)
{
	for ( size_t iBaseParents = 0; iBaseParents <= 5u; iBaseParents++ ) {
		for ( size_t iTargetParents = 0; iTargetParents <= 5u;
			 iTargetParents++ ) {
			char sBase[64] = { 0 };
			char sTarget[64] = { 0 };
			size_t iBaseSize = 0;
			size_t iTargetSize = 0;
			xstrview arrParts[2];
			str sRelative;

			for ( size_t i = 0; i < iBaseParents; i++ ) {
				testPathAppend(sBase, sizeof(sBase), &iBaseSize, "../");
			}
			for ( size_t i = 0; i < iTargetParents; i++ ) {
				testPathAppend(sTarget, sizeof(sTarget), &iTargetSize, "../");
			}
			testPathAppend(sBase, sizeof(sBase), &iBaseSize, "base");
			testPathAppend(sTarget, sizeof(sTarget), &iTargetSize, "target");
			xrtClearError();
			sRelative = xrtPathRelative(
				xrtStrView(sBase), xrtStrView(sTarget), XPATH_POSIX);
			if ( iTargetParents < iBaseParents ) {
				testRequire((sRelative == NULL) &&
					(xrtGetError() != NULL) &&
					(xrtErrorCode(xrtGetError()) == XPATH_ERROR_ROOT),
					"unresolved parent depth was incorrectly expressible");
			} else {
				str sJoined;
				str sCleanTarget;

				testRequire(sRelative != NULL,
					"expressible parent depth was rejected");
				arrParts[0] = xrtStrView(sBase);
				arrParts[1] = xrtStrView(sRelative);
				sJoined = xrtPathBuild(arrParts, 2, XPATH_POSIX);
				sCleanTarget = xrtPathClean(
					xrtStrView(sTarget), XPATH_POSIX);
				testRequire((sJoined != NULL) && (sCleanTarget != NULL) &&
					(strcmp(sJoined, sCleanTarget) == 0),
					"unresolved parent relative round trip failed");
				xrtFree(sJoined);
				xrtFree(sCleanTarget);
			}
			xrtFree(sRelative);
		}
	}
}



/* 执行两套词法风格的确定性性质测试。 */
int main(void)
{
	uint32 iState = UINT32_C(0x7A91C35D);

	testPathCleanProperty(XPATH_POSIX, &iState);
	testPathCleanProperty(XPATH_WINDOWS, &iState);
	testPathRelativeProperty(XPATH_POSIX, &iState);
	testPathRelativeProperty(XPATH_WINDOWS, &iState);
	testPathParentProperty();
	return 0;
}
