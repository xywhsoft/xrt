#include <xmail.h>



/* 展示 IMAP COMPRESS 的显式配置；真实会话在认证后调用协商函数。 */
int main(void)
{
	ximapcompressconfig Compress;

	xrtImapCompressConfigInit(&Compress);
	if ( !xrtImapCompressConfigValid(&Compress) ) {
		return 1;
	}

	/*
		认证后的客户端可直接执行：
		xrtImapClientCompress(pClient, &Compress, iDeadline, pCancel);
	*/
	return 0;
}
