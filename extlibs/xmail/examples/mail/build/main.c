#include <stdio.h>
#include <xmail.h>



/* 示例直接把 Builder 片段写到标准输出。 */
static bool writeOutput(xbytesview Data, ptr pUserData)
{
	FILE* pFile = (FILE*)pUserData;

	return fwrite(Data.Data, 1u, Data.Size, pFile) == Data.Size;
}



/* 构建不需要中间整封邮件对象的简单文本消息。 */
int main(void)
{
	const xmailaddress arrTo[] = {
		{ XRT_STR_INIT("Receiver"), XRT_STR_INIT("to@example.com") }
	};
	xmailbuilder Builder;

	if ( !xrtMailBuilderInit(&Builder, writeOutput, stdout) ||
		 !xrtMailBuilderAddressHeader(
			&Builder,
			XRT_STR_LITERAL("To"),
			arrTo,
			1u,
			XMAIL_WORD_BASE64,
			XMAIL_ADDRESS_DEFAULT,
			0
		 ) || !xrtMailBuilderWordHeader(
			&Builder,
			XRT_STR_LITERAL("Subject"),
			XRT_STR_LITERAL("流式邮件"),
			XMAIL_WORD_BASE64,
			0
		 ) || !xrtMailBuilderHeadersEnd(&Builder) ||
		 !xrtMailBuilderBody(&Builder, "hello\r\n", 7u) ||
		 !xrtMailBuilderFinish(&Builder) ) {
		return 1;
	}
	return 0;
}
