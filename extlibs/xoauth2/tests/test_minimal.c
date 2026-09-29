/* 验证 xoauth2-xrt.h 的最小模块闭包可独立构建。 */
#define XRT_IMPLEMENTATION
#include "../xoauth2.c"

int main(void)
{
	xoauth2client client = {0};
	char* url;
	xoauth2UseGithub(&client, "client-id", "client-secret", "https://app/callback");
	url = xoauth2BeginLogin(&client);
	if(url == NULL)
	{
		xoauth2ClientUnit(&client);
		return 1;
	}
	xrtFree(url);
	xoauth2ClientUnit(&client);
	return 0;
}
