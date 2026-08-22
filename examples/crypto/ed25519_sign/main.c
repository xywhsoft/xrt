#include <stdio.h>
#include <xrt.h>



/* 使用可复用展开密钥签署一条消息。 */
int main(void)
{
	static const char Message[] = "xrt ed25519";
	uint8 Seed[XRT_ED25519_SEED_SIZE] = { 0 };
	uint8 Signature[XRT_ED25519_SIGNATURE_SIZE];
	xed25519key Key;
	bool bSigned;

	if ( !xrtEd25519KeyInit(&Key, Seed) ) {
		return 1;
	}
	bSigned = xrtEd25519SignKey(
		&Key, Message, sizeof(Message) - 1u, Signature
	);
	xrtEd25519KeyClear(&Key);
	printf("signed: %s\n", bSigned ? "yes" : "no");
	return bSigned ? 0 : 1;
}
