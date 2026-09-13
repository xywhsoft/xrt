#define XSMTP_MODULE_SMTP
#include <xsmtp.h>



/* 验证发布包只安装公共头时仍可解析 SMTP 响应行。 */
int main(void)
{
	xsmtpreplyline Reply;

	return xrtSmtpReplyLineParse(
		XRT_STR_LITERAL("250-SIZE 10485760"),
		&Reply
	) ? 0 : 1;
}
