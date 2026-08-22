#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头发布包含客户端响应 Digest 一次选择入口。 */
int main(void)
{
	xhttpdigestchallenge Challenge;
	xhttpdigestchoice Choice;
	size_t iSize;

	return xrtHttpResponseDigestChallengeChoose(
		NULL,
		NULL,
		NULL,
		0,
		&iSize,
		&Challenge,
		&Choice
	) == XHTTP_NEXT_ERROR ? 0 : 1;
}
