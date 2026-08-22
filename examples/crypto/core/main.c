#include <stdio.h>
#include <xrt.h>



/* 展示摘要元数据、常量时间比较和敏感数据清理。 */
int main(void)
{
	uint8 Left[XRT_SHA256_SIZE] = { 1u, 2u, 3u };
	uint8 Right[XRT_SHA256_SIZE] = { 1u, 2u, 3u };
	bool bEqual;

	/* 算法元数据不要求把对应摘要实现编入程序。 */
	if ( xrtCryptoHashSize(XCRYPTO_HASH_SHA256) != sizeof(Left) ) {
		return 1;
	}

	/* 密钥、标签和摘要等固定长度数据应使用常量时间比较。 */
	bEqual = xrtConstTimeEqual(Left, Right, sizeof(Left));
	xrtSecureZero(Left, sizeof(Left));
	xrtSecureZero(Right, sizeof(Right));

	printf("equal: %s\n", bEqual ? "yes" : "no");
	return bEqual ? 0 : 1;
}
