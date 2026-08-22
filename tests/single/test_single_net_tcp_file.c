#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头构建必须公开并链接 TCP 文件发送入口。 */
int main(void)
{
	xnetresult (*pSendFile)(xnetstream*, xfile, uint64, size_t) =
		xrtNetStreamSendFile;

	return pSendFile != NULL ? 0 : 1;
}
