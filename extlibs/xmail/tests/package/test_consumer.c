#define XMAIL_MODULE_XMAIL
#include <xmail.h>



/* 验证发布包只安装公共头时仍可使用邮件内容底座。 */
int main(void)
{
	char arrOutput[32];
	size_t iSize;

	return xrtMailHeaderWrite(
		XRT_STR_LITERAL("Subject"),
		XRT_STR_LITERAL("hello"),
		0,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) ? 0 : 1;
}
