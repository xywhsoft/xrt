/*
 * XRT Example - Password Generator
 * XRT 范例 - 密码生成器
 *
 * Description / 说明:
 *   EN: Demonstrates generating a password and checking character coverage.
 *   CN: 演示密码生成以及字符类别覆盖检查。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/tools_password_gen.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/tools_password_gen -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"


static bool HasAny(str sText, str sPool)
{
	while ( *sText ) {
		if ( strchr(sPool, *sText) != NULL ) {
			return TRUE;
		}
		sText++;
	}
	return FALSE;
}



int main()
{
	str sTemplate = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789!@#$%^&*";
	str sPassword = NULL;

	xrtInit();

	sPassword = xrtRandStr(sTemplate, strlen(sTemplate), 16);
	printf("password = %s\n", sPassword);
	printf("has_upper = %s\n", HasAny(sPassword, "ABCDEFGHIJKLMNOPQRSTUVWXYZ") ? "TRUE" : "FALSE");
	printf("has_lower = %s\n", HasAny(sPassword, "abcdefghijklmnopqrstuvwxyz") ? "TRUE" : "FALSE");
	printf("has_digit = %s\n", HasAny(sPassword, "0123456789") ? "TRUE" : "FALSE");
	printf("has_symbol = %s\n", HasAny(sPassword, "!@#$%^&*") ? "TRUE" : "FALSE");

	xrtFree(sPassword);
	xrtUnit();
	return 0;
}
