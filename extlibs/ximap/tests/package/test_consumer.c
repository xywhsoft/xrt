#define XIMAP_MODULE_IMAP
#include <ximap.h>



/* 验证发布包只安装公共头时仍可解析 IMAP 响应。 */
int main(void)
{
	ximapresponseview Response;

	return xrtImapResponseParse(
		XRT_STR_LITERAL("* 23 FETCH (BODY[] {4096}"),
		&Response
	) ? 0 : 1;
}
