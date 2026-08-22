#include <stdio.h>
#include <xssh.h>



/* 使用显式 salt 生成可持久化的 OpenSSH hashed-host token。 */
int main(void)
{
	unsigned char arrSalt[XSSH_KNOWN_HOST_HASH_SIZE] = { 1u };
	char sHash[80];
	size_t iHashSize;
	bool bMatch;

	if ( (xrtSshKnownHostHashWrite(
		XRT_STR_LITERAL("host.example"),
		22u,
		(xbytesview){ arrSalt, sizeof(arrSalt) },
		sHash,
		sizeof(sHash),
		&iHashSize
	) != XSSH_OK) || (xrtSshKnownHostHashMatch(
		(xstrview){ sHash, iHashSize },
		XRT_STR_LITERAL("host.example"),
		22u,
		&bMatch
	) != XSSH_OK) ) {
		return 1;
	}
	printf("hashed-host=%s match=%d\n", sHash, bMatch ? 1 : 0);
	return bMatch ? 0 : 1;
}
