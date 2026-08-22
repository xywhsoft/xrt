#include "../test.h"



/* 在系统临时目录构造模板文件测试路径。 */
static str testTemplateFilePath(void)
{
	str sDirectory = xrtPathTemp();
	str sPath;

	testRequire(sDirectory != NULL, "template file temporary directory failed");
	sPath = xrtPathJoin(sDirectory, "xrt-template-file-test.tpl");
	xrtFree(sDirectory);
	testRequire(sPath != NULL, "template file path allocation failed");
	return sPath;
}



/* 验证默认文件入口读取源码、释放临时字节并返回独立模板。 */
static void testTemplateFileDefault(cstr sPath)
{
	xtemplate* pTemplate;
	xvalue* pData = xrtValueObject();
	str sOutput;

	testRequire(
		xrtFileWriteAll(
			sPath,
			(xbytesview){ (cbytes)"Hello {$name}", 13u }
		),
		"template file fixture write failed"
	);
	pTemplate = xrtTemplateCompileFile(sPath);
	testRequire(
		(pTemplate != NULL) && (pData != NULL),
		"template file default compile failed"
	);
	testRequire(
		xrtValueObjectSetNew(
			pData,
			XRT_STR_LITERAL("name"),
			xrtValueString(XRT_STR_LITERAL("Alice"))
		),
		"template file data setup failed"
	);
	sOutput = xrtTemplateRender(pTemplate, pData, NULL);
	testRequire(
		(sOutput != NULL) && (strcmp(sOutput, "Hello Alice") == 0),
		"template file render mismatch"
	);
	xrtFree(sOutput);
	xrtValueRelease(pData);
	xrtTemplateRelease(pTemplate);
}



/* 验证文件读取沿用编译源码上限，并保留文件层结构化错误。 */
static void testTemplateFileLimit(cstr sPath)
{
	xtemplateconfig Config;

	xrtTemplateConfigInit(&Config);
	Config.MaxSourceBytes = 4u;
	xrtClearError();
	testRequire(
		xrtTemplateCompileFileConfig(sPath, &Config) == NULL,
		"template file source limit was ignored"
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorDomain(xrtGetError()) != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.file") == 0) &&
		(xrtErrorCode(xrtGetError()) == XFILE_ERROR_LIMIT),
		"template file source limit error mismatch"
	);
}



/* 验证读取错误和模板语法错误不被便捷层改写。 */
static void testTemplateFileErrors(cstr sPath)
{
	xtemplate* pTemplate;

	testRequire(
		xrtFileWriteAll(
			sPath,
			(xbytesview){ (cbytes)"{$name", 6u }
		),
		"invalid template file fixture write failed"
	);
	xrtClearError();
	pTemplate = xrtTemplateCompileFile(sPath);
	testRequire(pTemplate == NULL, "invalid template file was accepted");
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorDomain(xrtGetError()) != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.template") == 0) &&
		(xrtErrorCode(xrtGetError()) == XTEMPLATE_ERROR_SYNTAX),
		"template file syntax error mismatch"
	);

	testRequire(xrtFileDelete(sPath), "template file fixture delete failed");
	xrtClearError();
	testRequire(
		xrtTemplateCompileFile(sPath) == NULL,
		"missing template file was accepted"
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorDomain(xrtGetError()) != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.file") == 0),
		"missing template file error was rewritten"
	);
}



/* 运行模板文件便捷层的读取、限制、错误和生命周期测试。 */
int main(void)
{
	str sPath = testTemplateFilePath();

	(void)xrtFileDelete(sPath);
	xrtClearError();
	testTemplateFileDefault(sPath);
	testTemplateFileLimit(sPath);
	testTemplateFileErrors(sPath);
	xrtFree(sPath);
	printf("[PASS] template file\n");
	return 0;
}
