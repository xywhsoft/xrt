#include <stdio.h>

#include <xrt.h>



/* 演示对不可信哈希表键执行带密钥的分块哈希。 */
int main(void)
{
	xsipkey Key = xrtSipKey(UINT64_C(0x0123456789ABCDEF),
		UINT64_C(0xFEDCBA9876543210));
	xsiphash State;
	uint64 iHash;

	xrtSipHashInit(&State, Key);
	if ( !xrtSipHashUpdate(&State, "request:", 8) ||
		 !xrtSipHashUpdate(&State, "user-input", 10) ) {
		return 1;
	}
	iHash = xrtSipHashFinal(&State);
	printf("%016llX\n", (unsigned long long)iHash);
	return 0;
}
