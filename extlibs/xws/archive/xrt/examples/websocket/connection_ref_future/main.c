#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 最后一个异步发送离开后释放应用持有的不可变消息块。 */
static void releaseMessage(ptr pContext, cbytes pData, size_t iSize)
{
	(void)pContext;
	(void)iSize;
	xrtFree((ptr)pData);
}



/* 从任意业务线程把一条所有权消息提交给 Connection Worker。 */
static xfuture* sendOwnedText(xwsconn* pConnection, xstrview Text)
{
	str sCopy;
	xnetref Ref;

	if ( Text.Size > (SIZE_MAX - 1u) ) {
		return NULL;
	}
	sCopy = (str)xrtMalloc(Text.Size + 1u);
	if ( sCopy == NULL ) {
		return NULL;
	}
	if ( Text.Size != 0 ) {
		memcpy(sCopy, Text.Data, Text.Size);
	}
	sCopy[Text.Size] = '\0';
	Ref = (xnetref) {
		(cbytes)sCopy,
		Text.Size,
		releaseMessage,
		NULL
	};
	if ( Text.Size == 0 ) {
		xrtFree(sCopy);
		return xrtWsConnTextAsync(pConnection, Text);
	}
	{
		xfuture* pFuture = xrtWsConnTextRefAsync(
			pConnection,
			&Ref
		);

		if ( pFuture == NULL ) {
			xrtFree(sCopy);
		}
		return pFuture;
	}
}



/* 示例函数由已经建立 WebSocket 连接的业务代码调用。 */
int main(void)
{
	(void)sendOwnedText;
	puts("WebSocket reference Future example is ready");
	return 0;
}
