#include <stdio.h>
#include <xssh.h>



/* 输出一个 host-key blob 的 OpenSSH SHA-256 指纹。 */
int main(void)
{
	char sFingerprint[64];
	size_t iFingerprintSize;

	if ( xrtSshHostKeyFingerprintSha256(
		XRT_BYTES_LITERAL("host-key-blob"),
		sFingerprint,
		sizeof(sFingerprint),
		&iFingerprintSize
	) != XSSH_OK ) {
		return 1;
	}
	printf("%s (%zu)\n", sFingerprint, iFingerprintSize);
	return 0;
}
