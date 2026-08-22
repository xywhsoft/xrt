#include <stdio.h>

#include <xrt.h>



/* 从系统安全随机源生成 128 位标识并在使用后清理栈缓冲。 */
int main(void)
{
	uint8 arrId[16];

	if ( !xrtSecureRandom(arrId, sizeof(arrId)) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(arrId); i++ ) {
		printf("%02x", (unsigned int)arrId[i]);
	}
	printf("\n");
	xrtSecureZero(arrId, sizeof(arrId));
	return 0;
}
