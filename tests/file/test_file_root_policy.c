#include "../test.h"
#include "../../src/internal/xrt_file_root.h"



/* 为内部策略测试生成无需额外分配的隔离目录名。 */
static void testRootPolicyName(char* sBuffer, size_t iCapacity)
{
	int iSize = snprintf(sBuffer, iCapacity,
		".xrt-root-policy-%lld", (long long)xrtNow());

	testRequire((iSize > 0) && ((size_t)iSize < iCapacity),
		"root policy fixture name formatting failed");
}



/* 写入一份用于精确名称和链接策略验证的短文件。 */
static void testRootPolicyWrite(xroot Root, cstr sPath)
{
	xfileoptions Options;
	xfile File;

	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE;
	File = xrtRootFileOpen(Root, sPath, &Options);
	testRequire(File != NULL, "root policy fixture open failed");
	testRequire(xrtWriteFull(File, "policy", 6u, NULL),
		"root policy fixture write failed");
	testRequire(xrtClose(File), "root policy fixture close failed");
}



/* 精确策略必须拒绝大小写替代名，兼容策略继续保持原行为。 */
static void testRootPolicyCase(void)
{
	char sDirectory[96];
	xroot Parent;
	xroot Root;
	xfile File;
	xfileoptions Options;
	xfileinfo Info;
	xdir Dir;

	testRootPolicyName(sDirectory, sizeof(sDirectory));
	Parent = xrtRootOpen(".");
	testRequire(Parent != NULL, "root policy parent open failed");
	if ( !xrtRootRemove(Parent, sDirectory) ) xrtClearError();
	testRequire(xrtRootDirCreate(Parent, sDirectory, 0700u),
		"root policy fixture directory creation failed");
	Root = xrtRootOpenIn(Parent, sDirectory);
	testRequire(Root != NULL, "root policy fixture root open failed");
	testRootPolicyWrite(Root, "Case.txt");

	File = __xrtRootFileOpenPolicy(Root, "Case.txt", NULL,
		XROOT_POLICY_CASE_SENSITIVE);
	testRequire(File != NULL, "exact root policy rejected the exact name");
	testRequire(xrtClose(File), "exact root policy file close failed");
	testRequire(__xrtRootFileOpenPolicy(Root, "case.txt", NULL,
		XROOT_POLICY_CASE_SENSITIVE) == NULL,
		"exact root policy accepted a case-substituted name");
	xrtClearError();
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE;
	/* Case-sensitive filesystems can create a distinct spelling. On a
	 * case-insensitive filesystem, the existing alias must stay protected. */
	if ( xrtRootStat(Root, "case.txt", true, &Info) ) {
		testRequire(__xrtRootFileOpenPolicy(Root, "case.txt", &Options,
			XROOT_POLICY_CASE_SENSITIVE) == NULL,
			"exact root policy opened an existing case alias for creation");
		xrtClearError();
	} else {
		xrtClearError();
		File = __xrtRootFileOpenPolicy(Root, "case.txt", &Options,
			XROOT_POLICY_CASE_SENSITIVE);
		testRequire(File != NULL && xrtClose(File),
			"exact root policy rejected creation of a distinct case-sensitive name");
		testRequire(xrtRootRemove(Root, "case.txt"),
			"distinct case-sensitive file cleanup failed");
	}
	testRequire(__xrtRootStatPolicy(Root, "Case.txt", true, &Info,
		XROOT_POLICY_CASE_SENSITIVE) &&
		(Info.Type == XFILE_TYPE_FILE) && (Info.Size == 6u),
		"exact root policy stat failed");
	Dir = __xrtRootDirOpenPolicy(Root, ".", 0u,
		XROOT_POLICY_CASE_SENSITIVE);
	testRequire((Dir != NULL) && xrtDirClose(Dir),
		"exact root policy directory open failed");
	testRequire(__xrtRootFileOpenPolicy(Root, "Case.txt", NULL,
		0x80000000u) == NULL,
		"root policy accepted unknown policy bits");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"unknown root policy bits reported the wrong error");
	xrtClearError();

	testRequire(xrtRootRemove(Root, "Case.txt"),
		"root policy file cleanup failed");
	testRequire(xrtRootClose(Root), "root policy root close failed");
	testRequire(xrtRootRemove(Parent, sDirectory),
		"root policy directory cleanup failed");
	testRequire(xrtRootClose(Parent), "root policy parent close failed");
}



/* 禁链接策略必须同时拒绝末级链接和中间目录链接。 */
static void testRootPolicyLinks(void)
{
	char sDirectory[96];
	str sFileLink;
	str sDirLink;
	xroot Parent;
	xroot Root;
	xfile File;

	testRootPolicyName(sDirectory, sizeof(sDirectory));
	Parent = xrtRootOpen(".");
	testRequire(Parent != NULL, "link policy parent open failed");
	if ( !xrtRootRemove(Parent, sDirectory) ) xrtClearError();
	testRequire(xrtRootDirCreate(Parent, sDirectory, 0700u),
		"link policy fixture directory creation failed");
	Root = xrtRootOpenIn(Parent, sDirectory);
	testRequire(Root != NULL, "link policy fixture root open failed");
	testRootPolicyWrite(Root, "target.txt");
	testRequire(xrtRootDirCreate(Root, "actual", 0700u),
		"link policy child directory creation failed");
	testRootPolicyWrite(Root, "actual/child.txt");
	sFileLink = xrtPathJoin(sDirectory, "file-link");
	sDirLink = xrtPathJoin(sDirectory, "dir-link");
	testRequire((sFileLink != NULL) && (sDirLink != NULL),
		"link policy fixture path construction failed");

	if ( xrtLinkCreate("target.txt", sFileLink, false) &&
		 xrtLinkCreate("actual", sDirLink, true) ) {
		xfileinfo Info;

		testRequire(!__xrtRootStatPolicy(Root,
			"file-link", false, &Info, 0u),
			"no-link root policy returned final link metadata");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_PERMISSION),
			"link metadata rejection reported the wrong error");
		xrtClearError();
		testRequire(__xrtRootFileOpenPolicy(Root,
			"file-link", NULL, 0u) == NULL,
			"no-link root policy followed a final link");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_PERMISSION),
			"final link rejection reported the wrong error");
		xrtClearError();
		testRequire(__xrtRootFileOpenPolicy(Root,
			"dir-link/child.txt", NULL, 0u) == NULL,
			"no-link root policy followed an intermediate link");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_PERMISSION),
			"intermediate link rejection reported the wrong error");
		xrtClearError();
		File = __xrtRootFileOpenPolicy(Root, "file-link", NULL,
			XROOT_POLICY_FOLLOW_LINKS);
		testRequire(File != NULL,
			"compatible root policy rejected a safe final link");
		testRequire(xrtClose(File),
			"compatible root policy file close failed");
		testRequire(xrtRootRemove(Root, "file-link") &&
			xrtRootRemove(Root, "dir-link"),
			"link policy fixture link cleanup failed");
	} else {
		/* Windows without symbolic-link privilege still covers exact lookup. */
		xrtClearError();
		if ( xrtFileExists(sFileLink) ) {
			testRequire(xrtRootRemove(Root, "file-link"),
				"partial link policy fixture cleanup failed");
		}
	}

	testRequire(xrtRootRemove(Root, "actual/child.txt") &&
		xrtRootRemove(Root, "actual") &&
		xrtRootRemove(Root, "target.txt"),
		"link policy fixture object cleanup failed");
	testRequire(xrtRootClose(Root), "link policy root close failed");
	testRequire(xrtRootRemove(Parent, sDirectory),
		"link policy directory cleanup failed");
	testRequire(xrtRootClose(Parent), "link policy parent close failed");
	xrtFree(sDirLink);
	xrtFree(sFileLink);
}



int main(void)
{
	testRootPolicyCase();
	testRootPolicyLinks();
	return 0;
}
