#include <stdio.h>
#include <xrt.h>



/* 生成密钥，并复算公钥检查密钥对一致性。 */
int main(void)
{
	uint8 Seed[XRT_ED25519_SEED_SIZE];
	uint8 Public[XRT_ED25519_PUBLIC_SIZE];
	uint8 Derived[XRT_ED25519_PUBLIC_SIZE];
	bool bEqual;

	if ( !xrtEd25519KeyPair(Seed, Public) ||
		 !xrtEd25519Public(Seed, Derived) ) {
		return 1;
	}
	bEqual = xrtConstTimeEqual(Public, Derived, sizeof(Public));
	xrtSecureZero(Seed, sizeof(Seed));
	printf("consistent: %s\n", bEqual ? "yes" : "no");
	return bEqual ? 0 : 1;
}
