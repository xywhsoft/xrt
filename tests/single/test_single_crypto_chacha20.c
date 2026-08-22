#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件 ChaCha20 的 RFC 向量首尾。 */
int main(void)
{
	uint8 Key[XRT_CHACHA20_KEY_SIZE];
	uint8 Nonce[XRT_CHACHA20_NONCE_SIZE] = {
		0, 0, 0, 0, 0, 0, 0, 0x4A, 0, 0, 0, 0
	};
	cstr sPlain =
		"Ladies and Gentlemen of the class of '99: If I could offer you only "
		"one tip for the future, sunscreen would be it.";
	uint8 Output[114];

	for ( size_t i = 0; i < sizeof(Key); i++ ) {
		Key[i] = (uint8)i;
	}
	return (!xrtChaCha20(
			Key, Nonce, 1, sPlain, Output, sizeof(Output)
		) || (Output[0] != 0x6Eu) || (Output[113] != 0x4Du)) ? 1 : 0;
}
