#include <xhttp.h>
#include <stdio.h>



/* 展示在客户端完成回调中读取可选 Content-Type。 */
void inspectResponseContentType(const xhttpresponse* pResponse)
{
	xmediatype Type;
	xhttpnext Next = xrtHttpResponseContentType(
		pResponse,
		&Type
	);

	if ( Next == XHTTP_NEXT_ITEM ) {
		printf("type=%.*s/%.*s\n",
			(int)Type.Type.Size,
			Type.Type.Data,
			(int)Type.Subtype.Size,
			Type.Subtype.Data
		);
	}
}



/* 范例入口不创建网络事务。 */
int main(void)
{
	return 0;
}

