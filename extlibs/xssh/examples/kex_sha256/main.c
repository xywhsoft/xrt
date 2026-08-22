#include <stdio.h>
#include <xssh.h>



/* 计算一个最小 exchange hash 输入。 */
int main(void)
{
	unsigned char arrShared[1] = { 1u };
	unsigned char arrHash[XSSH_SHA256_SIZE];
	xsshkexhashsha256 Input = { 0 };

	Input.SharedSecret.Data = arrShared;
	Input.SharedSecret.Size = sizeof(arrShared);
	if ( xrtSshKexHashSha256(&Input, arrHash) != XSSH_OK ) {
		return 1;
	}
	printf("exchange-hash-prefix=%02x%02x\n", arrHash[0], arrHash[1]);
	return 0;
}
