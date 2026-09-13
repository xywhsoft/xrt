#define XPOP3_MODULE_POP3
#include <xpop3.h>



/* 验证发布包只安装公共头时仍可解析 POP3 响应。 */
int main(void)
{
	xpop3replyview Reply;

	return xrtPop3ReplyParse(
		XRT_STR_LITERAL("+OK mailbox ready"),
		&Reply
	) ? 0 : 1;
}
