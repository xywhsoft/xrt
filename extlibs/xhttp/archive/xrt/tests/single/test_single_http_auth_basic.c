#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <string.h>



/* 单头发布必须保留 Basic 编码与解码。 */
int main(void)
{
	char Value[64];
	char Decoded[32];
	char Realm[16];
	xhttpbasicauth Basic;
	xhttpbasicchallenge Challenge;
	size_t iValue;
	size_t iDecoded;

	if ( !xrtHttpBasicWrite(
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("password"),
		Value,
		sizeof(Value),
		&iValue
	) || !xrtHttpBasicRead(
		(xstrview){ Value, iValue },
		Decoded,
		sizeof(Decoded),
		&iDecoded,
		&Basic
	) || (Basic.User.Size != 4u) ||
		(memcmp(Basic.User.Data, "user", 4u) != 0) ||
		!xrtHttpBasicChallengeRead(
			XRT_STR_LITERAL("Basic realm=api, charset=UTF-8"),
			Realm,
			sizeof(Realm),
			&iDecoded,
			&Challenge
		) || !Challenge.Utf8 || (Challenge.Realm.Size != 3u) ) {
		return 1;
	}
	return 0;
}
