#include <xhttp.h>
#include <stdio.h>



/* 展示在客户端完成回调中逐条读取独立 Set-Cookie。 */
bool inspectResponseSetCookies(const xhttpresponse* pResponse)
{
	xsetcookie Cookie;
	xhttpnext Next;
	size_t iHeader = 0;

	while ( (Next = xrtHttpResponseSetCookieNext(
		pResponse,
		&iHeader,
		&Cookie
	)) == XHTTP_NEXT_ITEM ) {
		printf("cookie=%.*s\n",
			(int)Cookie.Name.Size,
			Cookie.Name.Data
		);
	}
	return Next == XHTTP_NEXT_END;
}



/* 范例入口不创建网络事务。 */
int main(void)
{
	return 0;
}

