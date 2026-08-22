#include "../test.h"



#define TEST_ENV_NAME "XRT_TEST_ENVIRONMENT_7F4C2B19"
#define TEST_ENV_UTF8_NAME "XRT_TEST_ENVIRONMENT_中文"



/* 当前错误必须属于环境变量稳定域和预期代码。 */
static void testEnvironmentError(int32 iCode, cstr sMessage)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, sMessage);
	testRequire(strcmp(xrtErrorDomain(pError), "xrt.environment") == 0,
		"environment error domain mismatch");
	testRequire(xrtErrorCode(pError) == iCode,
		"environment error code mismatch");
	xrtClearError();
}



/* 缺失变量与空值必须是两个可区分的成功状态。 */
static void testEnvironmentMissingAndEmpty(void)
{
	str sValue = (str)(uintptr_t)1u;

	testRequire(xrtEnvRemove(TEST_ENV_NAME),
		"environment cleanup failed");
	testRequire(xrtEnvLookup(TEST_ENV_NAME, &sValue),
		"missing environment lookup failed");
	testRequire(sValue == NULL,
		"missing environment variable returned a value");
	testRequire(xrtEnvSet(TEST_ENV_NAME, ""),
		"empty environment value set failed");
	testRequire(xrtEnvLookup(TEST_ENV_NAME, &sValue),
		"empty environment value lookup failed");
	testRequire((sValue != NULL) && (sValue[0] == '\0'),
		"empty environment value became missing");
	xrtFree(sValue);
}



/* UTF-8 名称、值、覆盖和便捷读取必须跨平台一致。 */
static void testEnvironmentUtf8AndOverwrite(void)
{
	str sValue;

	testRequire(xrtEnvRemove(TEST_ENV_UTF8_NAME),
		"UTF-8 environment cleanup failed");
	testRequire(xrtEnvSet(TEST_ENV_UTF8_NAME, "第一版"),
		"UTF-8 environment set failed");
	sValue = xrtEnvGet(TEST_ENV_UTF8_NAME);
	testRequire((sValue != NULL) && (strcmp(sValue, "第一版") == 0),
		"UTF-8 environment get mismatch");
	xrtFree(sValue);

	testRequire(xrtEnvSet(TEST_ENV_UTF8_NAME, "第二版"),
		"environment overwrite failed");
	sValue = NULL;
	testRequire(xrtEnvLookup(TEST_ENV_UTF8_NAME, &sValue),
		"overwritten environment lookup failed");
	testRequire((sValue != NULL) && (strcmp(sValue, "第二版") == 0),
		"environment overwrite mismatch");
	xrtFree(sValue);
	testRequire(xrtEnvRemove(TEST_ENV_UTF8_NAME),
		"UTF-8 environment removal failed");
	testRequire(xrtEnvRemove(TEST_ENV_UTF8_NAME),
		"environment removal is not idempotent");
}



/* 名称、值和输出参数错误必须被稳定拒绝。 */
static void testEnvironmentInvalid(void)
{
	char sInvalid[] = { (char)0xC3, (char)0x28, '\0' };
	str sValue = (str)(uintptr_t)1u;

	testRequire(!xrtEnvLookup(NULL, &sValue),
		"null environment name was accepted");
	testRequire(sValue == NULL,
		"failed environment lookup did not clear output");
	testEnvironmentError(XENV_ERROR_NAME,
		"null environment name did not set an error");
	testRequire(!xrtEnvSet("", "value"),
		"empty environment name was accepted");
	testEnvironmentError(XENV_ERROR_NAME,
		"empty environment name did not set an error");
	testRequire(!xrtEnvSet("BAD=NAME", "value"),
		"environment name containing '=' was accepted");
	testEnvironmentError(XENV_ERROR_NAME,
		"invalid environment name did not set an error");
	testRequire(!xrtEnvSet(TEST_ENV_NAME, sInvalid),
		"invalid UTF-8 environment value was accepted");
	testEnvironmentError(XENV_ERROR_VALUE,
		"invalid environment value did not set an error");
	testRequire(!xrtEnvLookup(TEST_ENV_NAME, NULL),
		"null environment output was accepted");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"null environment output error mismatch");
	xrtClearError();
}



/* 执行环境变量读取、写入、删除和边界契约。 */
int main(void)
{
	testEnvironmentMissingAndEmpty();
	testEnvironmentUtf8AndOverwrite();
	testEnvironmentInvalid();
	testRequire(xrtEnvRemove(TEST_ENV_NAME),
		"environment final cleanup failed");
	return 0;
}
